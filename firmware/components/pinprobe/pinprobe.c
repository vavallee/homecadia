#include "pinprobe.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"

pinprobe_line_t pinprobe_line_state(int gpio)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    esp_rom_delay_us(2000);
    int up = gpio_get_level(gpio);

    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io);
    esp_rom_delay_us(2000);
    int down = gpio_get_level(gpio);

    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io);

    if (up && !down) {
        return PINPROBE_FLOATING;
    }
    if (!up && !down) {
        return PINPROBE_HELD_LOW;
    }
    if (up && down) {
        return PINPROBE_HELD_HIGH;
    }
    return PINPROBE_INCONSISTENT;
}

const char *pinprobe_line_name(pinprobe_line_t s)
{
    switch (s) {
    case PINPROBE_FLOATING:
        return "floating";
    case PINPROBE_HELD_LOW:
        return "held LOW";
    case PINPROBE_HELD_HIGH:
        return "held HIGH";
    default:
        return "inconsistent";
    }
}

pinprobe_drive_t pinprobe_drive_test(int gpio)
{
    /* INPUT_OUTPUT keeps the input buffer enabled while the pin is driven. */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    gpio_config(&io);

    pinprobe_drive_t r;
    gpio_set_level(gpio, 1);
    esp_rom_delay_us(500);
    r.hi = gpio_get_level(gpio);

    gpio_set_level(gpio, 0);
    esp_rom_delay_us(500);
    r.lo = gpio_get_level(gpio);

    io.mode = GPIO_MODE_INPUT;
    gpio_config(&io);
    return r;
}

const char *pinprobe_drive_verdict(pinprobe_drive_t d)
{
    if (d.hi == 1 && d.lo == 0) {
        return "follows the driver - OK";
    }
    if (d.hi == 0) {
        return "STUCK LOW while driven high - shorted to GND";
    }
    return "STUCK HIGH while driven low - held up by something stronger than the C6";
}
