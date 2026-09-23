/* EC11 rotary encoder: interrupt-driven quadrature decode, one callback per
 * detent. The 4-state Gray-code transition table rejects contact bounce
 * without timers, and a detent is emitted when the contacts return to rest,
 * so the encoder draws nothing while idle. The push switch
 * is NOT handled here (use espressif/button). */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* dir is +1 (clockwise) or -1. Runs in a dedicated low-priority task. */
typedef void (*ec11_cb_t)(int dir, void *arg);

/* Pins are configured input + internal pullup; encoder common goes to GND.
 * HW-VERIFY: clockwise direction — swap A/B if inverted. */
esp_err_t ec11_init(int gpio_a, int gpio_b, ec11_cb_t cb, void *arg);

/* Let rotation wake the chip from light sleep, including with
 * CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP, where only LP GPIOs can
 * wake it (ESP-IDF sleep_modes docs, esp_deep_sleep_enable_gpio_wakeup).
 * wake_gpio must be gpio_a or gpio_b and an LP GPIO (GPIO0-7 on the C6).
 * The pin is armed for the level it is not at; after any activity the chip
 * is kept out of light sleep for awake_ms so the other pin, which cannot
 * wake it, is decoded too. Needs CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_
 * RESISTORS=n, or arming for HIGH swaps the pin's pull-up for a pull-down. */
esp_err_t ec11_enable_light_sleep_wake(int wake_gpio, uint32_t awake_ms);

#ifdef __cplusplus
}
#endif
