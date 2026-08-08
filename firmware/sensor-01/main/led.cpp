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

static void set_led(bool on)
{
    s_on = on;
    gpio_set_level((gpio_num_t)LED_PIN, on ? 1 : 0);
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
    gpio_set_level((gpio_num_t)LED_PIN, 0);

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
