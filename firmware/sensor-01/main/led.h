/* Status LED, deliberately minimal (LEDs cost battery):
 * commissioning window open -> 500ms blink; low battery -> 100ms pulse
 * every 10s; otherwise off. Commissioning overrides low battery. */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t led_init(void);
void led_set_commissioning(bool active);
void led_set_low_battery(bool active);
