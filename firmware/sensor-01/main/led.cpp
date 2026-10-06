#include "led.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define BLINK_MS      500
#define PULSE_MS      100
#define PULSE_GAP_MS  10000

static bool s_commissioning;
static bool s_low_battery;
static bool s_on;
static esp_timer_handle_t s_timer;

static void schedule(uint32_t ms)
{
    esp_timer_stop(s_timer);
    esp_timer_start_once(s_timer, (uint64_t)ms * 1000);
}

/* The chip light-sleeps inside the 100 ms pulse, and with
 * CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP the GPIO's power domain goes
 * down with it: the LED lit only while the chip was awake, two ~2.5 ms flashes
 * 100 ms apart (2026-10-05, capture ppk2-20261006T015052; gpio_sleep_sel_dis()
 * alone did not help). The pad hold keeps the level through that power-down
 * (gpio_hold_en(), driver/gpio.h, IDF v5.5.5), so each change re-latches it. */
static void set_led(bool on)
{
    s_on = on;
    gpio_num_t pin = (gpio_num_t)LED_PIN;
    gpio_set_level(pin, on ? 1 : 0);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT); /* the hold note: configure before releasing it */
    gpio_hold_dis(pin);
    gpio_hold_en(pin);
}

static void tick(void *arg)
{
    if (s_commissioning) {
        set_led(!s_on);
        schedule(BLINK_MS);
    } else if (s_low_battery) {
        set_led(!s_on);
        schedule(s_on ? PULSE_MS : PULSE_GAP_MS);
    } else {
        set_led(false);
    }
}

static void apply(void)
{
    esp_timer_stop(s_timer);
    set_led(false);
    if (s_commissioning || s_low_battery) {
        schedule(10);
    }
}

esp_err_t led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }
    set_led(false);

    const esp_timer_create_args_t targs = {
        .callback = tick,
        .name = "led",
    };
    return esp_timer_create(&targs, &s_timer);
}

void led_set_commissioning(bool active)
{
    if (s_commissioning == active) {
        return;
    }
    s_commissioning = active;
    apply();
}

void led_set_low_battery(bool active)
{
    if (s_low_battery == active) {
        return;
    }
    s_low_battery = active;
    apply();
}
