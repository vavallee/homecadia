#include "ui.h"

#include "button_gpio.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_timer.h"
#include "iot_button.h"
#include "openthread/thread.h"

#include "app_config.h"
#include "bench_selftest.h"
#include "battery.h"
#include "display.h"
#include "ec11.h"
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

static void on_push(void *button_handle, void *usr_data)
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

static void on_factory_reset(void *button_handle, void *usr_data)
{
    ESP_LOGW(TAG, "Encoder held %ds: factory reset", FACTORY_RESET_HOLD_S);
    esp_matter::factory_reset(); /* wipes fabrics + NVS, reboots into commissioning */
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

    button_config_t btn_cfg = {};
    button_gpio_config_t gpio_cfg = {
        .gpio_num = ENC_PIN_SW,
        .active_level = 0,
        .enable_power_save = true,
    };
    button_handle_t btn;
    err = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "button init failed: %s", esp_err_to_name(err));
        return err;
    }
    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, nullptr, on_push, nullptr);

    button_event_args_t reset_args = {};
    reset_args.long_press.press_time = FACTORY_RESET_HOLD_S * 1000;
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, &reset_args, on_factory_reset, nullptr);

    bench_encoder_monitor_start(); /* no-op outside the bench profile */
    return ESP_OK;
}
