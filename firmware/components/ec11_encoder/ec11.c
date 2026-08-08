#include "ec11.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static struct {
    int gpio_a;
    int gpio_b;
    ec11_cb_t cb;
    void *cb_arg;
    QueueHandle_t queue;
    volatile uint8_t prev_state;
    volatile int8_t accum;
} s;

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
    uint8_t state = (gpio_get_level(s.gpio_a) << 1) | gpio_get_level(s.gpio_b);
    int8_t delta = k_transition[(s.prev_state << 2) | state];
    s.prev_state = state;
    if (delta == 0) {
        return;
    }
    s.accum += delta;
    /* one mechanical detent = 4 quadrature steps */
    if (s.accum >= 4 || s.accum <= -4) {
        int dir = s.accum > 0 ? 1 : -1;
        s.accum = 0;
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(s.queue, &dir, &woken);
        if (woken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void ec11_task(void *arg)
{
    int dir;
    for (;;) {
        if (xQueueReceive(s.queue, &dir, portMAX_DELAY) == pdTRUE) {
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

    s.prev_state = (gpio_get_level(gpio_a) << 1) | gpio_get_level(gpio_b);

    /* may already be installed by another component; both outcomes are fine */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    gpio_isr_handler_add(gpio_a, isr, NULL);
    gpio_isr_handler_add(gpio_b, isr, NULL);

    if (xTaskCreate(ec11_task, "ec11", 2048, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
