#include "ui.h"

#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "sdkconfig.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "openthread/thread.h"

#include "app_config.h"
#include "bench_selftest.h"
#include "battery.h"
#include "display.h"
#include "ec11.h"
#include "esp_sleep.h"
#include "sensor_loop.h"
#include "settings.h"

static const char *TAG = "ui";

enum ui_view_t { VIEW_READINGS = 0, VIEW_DIAG, VIEW_SETTINGS, VIEW_COUNT };
enum ui_mode_t { MODE_NAV, MODE_MENU, MODE_EDIT };

static ui_view_t s_view = VIEW_READINGS;
static ui_mode_t s_mode = MODE_NAV;
static display_settings_view_t s_menu;
static esp_timer_handle_t s_idle_timer;

static const uint16_t k_poll_choices[] = {60, 120, 300, 600};
static const int k_poll_choice_count = sizeof(k_poll_choices) / sizeof(k_poll_choices[0]);

static void render_current(void)
{
    switch (s_view) {
    case VIEW_READINGS: {
        sensor_readings_t r = sensor_loop_get_readings();
        display_show_readings(&r);
        break;
    }
    case VIEW_DIAG: {
        display_diag_t d = {};
        sensor_readings_t r = sensor_loop_get_readings();
        d.battery_pct = r.battery_pct;
        d.battery_mv = r.battery_mv;
        d.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        d.fw_version = esp_app_get_description()->version;
        /* HW-VERIFY: OT API access alongside the CHIP-run OpenThread mainloop */
        if (esp_openthread_lock_acquire(pdMS_TO_TICKS(100))) {
            otInstance *inst = esp_openthread_get_instance();
            int8_t rssi;
            if (inst && otThreadGetParentAverageRssi(inst, &rssi) == OT_ERROR_NONE) {
                d.rssi_dbm = rssi;
                d.rssi_valid = true;
            }
            esp_openthread_lock_release();
        }
        display_show_diagnostics(&d);
        break;
    }
    case VIEW_SETTINGS:
        s_menu.highlight_menu = s_mode != MODE_NAV;
        s_menu.editing = s_mode == MODE_EDIT;
        display_show_settings(&s_menu);
        break;
    default:
        break;
    }
}

static void reset_idle_timer(void)
{
    esp_timer_stop(s_idle_timer);
    esp_timer_start_once(s_idle_timer, (uint64_t)DISPLAY_IDLE_TIMEOUT_S * 1000000ULL);
}

static void idle_cb(void *arg)
{
#if CONFIG_HOMECADIA_BENCH_SELFTEST
    ESP_LOGW(TAG, "idle_cb fired");
#endif
    s_mode = MODE_NAV;
    if (s_view != VIEW_READINGS) {
        s_view = VIEW_READINGS;
        render_current();
    }
    /* nothing else: panel is already asleep after its last refresh */
}

static void adjust_edited_value(int dir)
{
    if (s_menu.selected == 0) { /* poll interval */
        int idx = 0;
        for (int i = 0; i < k_poll_choice_count; i++) {
            if (k_poll_choices[i] == s_menu.values.poll_interval_s) {
                idx = i;
            }
        }
        idx += dir;
        if (idx < 0) {
            idx = 0;
        } else if (idx >= k_poll_choice_count) {
            idx = k_poll_choice_count - 1;
        }
        s_menu.values.poll_interval_s = k_poll_choices[idx];
    } else { /* units */
        s_menu.values.use_fahrenheit = !s_menu.values.use_fahrenheit;
    }
}

static void on_rotate(int dir, void *arg)
{
#if CONFIG_HOMECADIA_BENCH_SELFTEST
    ESP_LOGW(TAG, "on_rotate dir=%d (A=%d B=%d)", dir, gpio_get_level((gpio_num_t)ENC_PIN_A),
             gpio_get_level((gpio_num_t)ENC_PIN_B));
#endif
    reset_idle_timer();
    switch (s_mode) {
    case MODE_NAV:
        s_view = (ui_view_t)((s_view + VIEW_COUNT + dir) % VIEW_COUNT);
        if (s_view == VIEW_SETTINGS) {
            s_menu.values = settings_get();
            s_menu.selected = 0;
        }
        break;
    case MODE_MENU:
        s_menu.selected = (s_menu.selected + DISPLAY_SETTINGS_ITEMS + dir) % DISPLAY_SETTINGS_ITEMS;
        break;
    case MODE_EDIT:
        adjust_edited_value(dir);
        break;
    }
    render_current();
}

static void on_push(void)
{
#if CONFIG_HOMECADIA_BENCH_SELFTEST
    ESP_LOGW(TAG, "on_push (SW=%d)", gpio_get_level((gpio_num_t)ENC_PIN_SW));
#endif
    reset_idle_timer();
    if (s_view != VIEW_SETTINGS) {
        /* wake/keep-alive only; in M5 this is also the deep-sleep wake pin */
        return;
    }
    switch (s_mode) {
    case MODE_NAV:
        s_mode = MODE_MENU;
        break;
    case MODE_MENU:
        s_mode = MODE_EDIT;
        break;
    case MODE_EDIT: {
        s_mode = MODE_MENU;
        app_settings_t prev = settings_get();
        settings_save(&s_menu.values);
        if (prev.poll_interval_s != s_menu.values.poll_interval_s) {
            sensor_loop_set_poll_interval(s_menu.values.poll_interval_s);
        }
        break;
    }
    }
    render_current();
}

static void on_factory_reset(void)
{
    ESP_LOGW(TAG, "Encoder held %ds: factory reset", FACTORY_RESET_HOLD_S);
    esp_matter::factory_reset(); /* wipes fabrics + NVS, reboots into commissioning */
}

/* Push switch. On an LP GPIO a press wakes the chip from light sleep; on an
 * HP GPIO (D9, the current layout) it registers only while awake.
 * Not espressif/button: under CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
 * its power-save mode drives ext1 wake and gpio_hold_en on the pin itself
 * (button_gpio.c), and without power save it polls every 5 ms forever.
 *
 * Level interrupts, not edges: a press that wakes the chip happened while it
 * slept, so its edge is gone, but the level is still there when it wakes.
 * The ISR disables the pin's interrupt; the task debounces (re-read after
 * 20 ms), acts, and re-arms for the opposite level. A no-light-sleep lock is
 * held while the switch is down, and a one-shot timer does the factory-reset
 * hold. */
static TaskHandle_t s_sw_task;
#if CONFIG_PM_ENABLE
static esp_pm_lock_handle_t s_sw_lock;
#endif
static esp_timer_handle_t s_sw_hold_timer;

static void IRAM_ATTR sw_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    gpio_ll_set_intr_type(&GPIO, ENC_PIN_SW, GPIO_INTR_DISABLE); /* register write; IRAM-safe */
    vTaskNotifyGiveFromISR(s_sw_task, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void sw_hold_cb(void *arg)
{
    if (gpio_get_level((gpio_num_t)ENC_PIN_SW) == 0) {
        on_factory_reset();
    }
}

static void sw_task(void *arg)
{
    bool pressed = false;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(20));
        bool now = gpio_get_level((gpio_num_t)ENC_PIN_SW) == 0;
        if (now != pressed) {
            pressed = now;
            if (pressed) {
#if CONFIG_PM_ENABLE
                esp_pm_lock_acquire(s_sw_lock);
#endif
                esp_timer_start_once(s_sw_hold_timer, (uint64_t)FACTORY_RESET_HOLD_S * 1000000ULL);
            } else {
                bool reset_fired = esp_timer_stop(s_sw_hold_timer) != ESP_OK;
#if CONFIG_PM_ENABLE
                esp_pm_lock_release(s_sw_lock);
#endif
                if (!reset_fired) {
                    on_push();
                }
            }
        }
        gpio_set_intr_type((gpio_num_t)ENC_PIN_SW, pressed ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
    }
}

static esp_err_t push_switch_init(void)
{
    esp_err_t err;
#if CONFIG_PM_ENABLE /* no PM (bench profile): no light sleep, so no lock needed */
    err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "push_sw", &s_sw_lock);
    if (err != ESP_OK) {
        return err;
    }
#endif
    const esp_timer_create_args_t targs = {
        .callback = sw_hold_cb,
        .name = "sw_hold",
    };
    err = esp_timer_create(&targs, &s_sw_hold_timer);
    if (err != ESP_OK) {
        return err;
    }
    if (xTaskCreate(sw_task, "push_sw", 3072, nullptr, 4, &s_sw_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << ENC_PIN_SW;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.intr_type = GPIO_INTR_LOW_LEVEL;
    err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }
    /* ec11_init installed the ISR service already */
    err = gpio_isr_handler_add((gpio_num_t)ENC_PIN_SW, sw_isr, nullptr);
    if (err != ESP_OK) {
        return err;
    }
#if CONFIG_PM_ENABLE
    /* LP wake on press, only if the switch is on an LP GPIO (0-7). On an HP
     * pin a press registers only while the chip is already awake. While the
     * switch is down the lock keeps the chip awake, so LOW never needs
     * re-arming. */
    if (esp_sleep_is_valid_wakeup_gpio((gpio_num_t)ENC_PIN_SW)) {
        err = esp_deep_sleep_enable_gpio_wakeup(BIT64(ENC_PIN_SW), ESP_GPIO_WAKEUP_GPIO_LOW);
    }
#endif
    return err;
}

esp_err_t ui_init(void)
{
    const esp_timer_create_args_t targs = {
        .callback = idle_cb,
        .name = "ui_idle",
    };
    esp_err_t err = esp_timer_create(&targs, &s_idle_timer);
    if (err != ESP_OK) {
        return err;
    }

    err = ec11_init(ENC_PIN_A, ENC_PIN_B, on_rotate, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "encoder init failed: %s", esp_err_to_name(err));
        return err;
    }
#if CONFIG_PM_ENABLE && CONFIG_HOMECADIA_DIAG_DIAL_WAKE_DELAY_S > 0
    {
        /* Floor A/B (main/Kconfig.projbuild): no dial wake source until the timer fires. */
        const esp_timer_create_args_t wargs = {
            .callback = [](void *) {
                esp_err_t e = ec11_enable_light_sleep_wake(ENC_PIN_A, ENC_AWAKE_MS);
                ESP_LOGW(TAG, "diag: dial wake armed at %llds: %s",
                         (long long)(esp_timer_get_time() / 1000000), esp_err_to_name(e));
            },
            .name = "diag_dial_wake",
        };
        esp_timer_handle_t t = nullptr;
        err = esp_timer_create(&wargs, &t);
        if (err == ESP_OK) {
            err = esp_timer_start_once(t, (uint64_t)CONFIG_HOMECADIA_DIAG_DIAL_WAKE_DELAY_S * 1000000ULL);
        }
        if (err != ESP_OK) {
            return err;
        }
        ESP_LOGW(TAG, "diag: dial wake deferred %ds", CONFIG_HOMECADIA_DIAG_DIAL_WAKE_DELAY_S);
    }
#elif CONFIG_PM_ENABLE
    err = ec11_enable_light_sleep_wake(ENC_PIN_A, ENC_AWAKE_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "encoder wake failed: %s", esp_err_to_name(err));
        return err;
    }
#endif

    err = push_switch_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "push switch init failed: %s", esp_err_to_name(err));
        return err;
    }

    bench_encoder_monitor_start(); /* no-op outside the bench profile */
    return ESP_OK;
}
