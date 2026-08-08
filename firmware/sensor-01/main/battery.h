/* Battery voltage via 2x1M divider on VBAT_ADC_GPIO, percent via LiPo OCV LUT. */
#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t battery_init(void);

/* Battery terminal voltage in mV (divider-corrected, multisampled). */
esp_err_t battery_read_mv(uint32_t *out_mv);

/* 0..100 from open-circuit-voltage lookup. Load during the reading skews
 * this optimistic/pessimistic by a few percent — acceptable for a display
 * battery gauge. HW-VERIFY: calibrate LUT endpoints against multimeter. */
uint8_t battery_percent_from_mv(uint32_t mv);
