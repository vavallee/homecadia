/* SHT40 temperature/humidity sensor, I2C, single-shot reads.
 * Sensirion SHT40 on Grove board, default address 0x44.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sht40 *sht40_handle_t;

/* Creates the I2C master bus on the given pins and probes the sensor.
 * Returns ESP_ERR_NOT_FOUND if the sensor doesn't ACK its address. */
esp_err_t sht40_init(int sda_gpio, int scl_gpio, uint8_t i2c_addr, sht40_handle_t *out);

/* Single-shot high-precision measurement (~9ms). Values:
 * temp_c in °C, rh in %RH (clamped to 0..100).
 * Returns ESP_ERR_INVALID_CRC on checksum failure. */
esp_err_t sht40_read(sht40_handle_t h, float *temp_c, float *rh);

#ifdef __cplusplus
}
#endif
