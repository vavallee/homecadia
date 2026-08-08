/* EC11 rotary encoder: interrupt-driven quadrature decode, one callback per
 * detent. The 4-state Gray-code transition table rejects contact bounce
 * without timers, so the encoder draws nothing while idle. The push switch
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

#ifdef __cplusplus
}
#endif
