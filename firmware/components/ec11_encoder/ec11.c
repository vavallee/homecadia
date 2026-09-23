#include "ec11.h"

#include "driver/gpio.h"
#include "esp_bit_defs.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "ec11";

#define NOTIFY_ACTIVITY BIT0 /* ISR: a pin changed */
#define NOTIFY_TIMEOUT  BIT1 /* timer: awake window over */

static struct {
    int gpio_a;
    int gpio_b;
    ec11_cb_t cb;
    void *cb_arg;
    QueueHandle_t queue;
    TaskHandle_t task;
    volatile uint8_t prev_state;
    volatile int8_t accum;
    uint8_t rest_state;
    /* light-sleep wake (ec11_enable_light_sleep_wake) */
    int wake_gpio;
    uint32_t awake_ms;
    esp_pm_lock_handle_t awake_lock;
    esp_timer_handle_t awake_timer;
    bool awake_held;
} s = {.wake_gpio = -1};

/* Index: prev_state<<2 | new_state. Valid Gray transitions are ±1, everything
 * else (bounce, skipped state) contributes 0. */
static const int8_t k_transition[16] = {
    0, -1, +1, 0,
    +1, 0, 0, -1,
    -1, 0, 0, +1,
    0, +1, -1, 0,
};

static void IRAM_ATTR isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    uint8_t state = (gpio_get_level(s.gpio_a) << 1) | gpio_get_level(s.gpio_b);
    int8_t delta = k_transition[(s.prev_state << 2) | state];
    s.prev_state = state;
    s.accum += delta;
    /* A detent is complete when the contacts are back at rest. Emitting on the
     * sign of the accumulated steps, rather than on exactly 4, keeps a detent
     * whose first transitions were missed -- by a light-sleep wake, or a slow
     * turn -- and bounce at rest still sums to 0. */
    if (state == s.rest_state && s.accum != 0) {
        int dir = s.accum > 0 ? 1 : -1;
        s.accum = 0;
        xQueueSendFromISR(s.queue, &dir, &woken);
    }
    xTaskNotifyFromISR(s.task, NOTIFY_ACTIVITY, eSetBits, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

/* Arm the wake pin for the level it is not at, so the next change wakes. */
static void arm_wake(void)
{
    esp_deepsleep_gpio_wake_up_mode_t mode = gpio_get_level(s.wake_gpio)
            ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH;
    esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(BIT64(s.wake_gpio), mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wake arm GPIO%d: %s", s.wake_gpio, esp_err_to_name(err));
    }
}

static void awake_timeout_cb(void *arg)
{
    xTaskNotify(s.task, NOTIFY_TIMEOUT, eSetBits);
}

/* Blocks until notified; never polls, so it costs nothing in light sleep.
 * Owns the awake lock: activity (re)starts the window, the timer ends it,
 * and activity wins if both arrive together. */
static void ec11_task(void *arg)
{
    int dir;
    uint32_t bits;
    for (;;) {
        xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);
        if (s.wake_gpio >= 0) {
            if (bits & NOTIFY_ACTIVITY) {
                if (!s.awake_held) {
                    s.awake_held = true;
                    esp_pm_lock_acquire(s.awake_lock);
                }
                esp_timer_stop(s.awake_timer);
                esp_timer_start_once(s.awake_timer, (uint64_t)s.awake_ms * 1000);
            } else if ((bits & NOTIFY_TIMEOUT) && s.awake_held) {
                arm_wake();
                s.awake_held = false;
                esp_pm_lock_release(s.awake_lock);
            }
        }
        while (xQueueReceive(s.queue, &dir, 0) == pdTRUE) {
            s.cb(dir, s.cb_arg);
        }
    }
}

esp_err_t ec11_init(int gpio_a, int gpio_b, ec11_cb_t cb, void *arg)
{
    s.gpio_a = gpio_a;
    s.gpio_b = gpio_b;
    s.cb = cb;
    s.cb_arg = arg;

    s.queue = xQueueCreate(8, sizeof(int));
    if (!s.queue) {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << gpio_a) | (1ULL << gpio_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }

    /* The knob is at a detent at boot: that pin state is "rest". */
    s.prev_state = (gpio_get_level(gpio_a) << 1) | gpio_get_level(gpio_b);
    s.rest_state = s.prev_state;

    if (xTaskCreate(ec11_task, "ec11", 2048, NULL, 4, &s.task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    /* may already be installed by another component; both outcomes are fine */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    gpio_isr_handler_add(gpio_a, isr, NULL);
    gpio_isr_handler_add(gpio_b, isr, NULL);
    return ESP_OK;
}

esp_err_t ec11_enable_light_sleep_wake(int wake_gpio, uint32_t awake_ms)
{
    if (wake_gpio != s.gpio_a && wake_gpio != s.gpio_b) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "ec11", &s.awake_lock);
    if (err != ESP_OK) {
        return err;
    }
    const esp_timer_create_args_t targs = {
        .callback = awake_timeout_cb,
        .name = "ec11_awake",
    };
    err = esp_timer_create(&targs, &s.awake_timer);
    if (err != ESP_OK) {
        return err;
    }
    s.awake_ms = awake_ms;
    s.wake_gpio = wake_gpio;
    arm_wake();
    return ESP_OK;
}
