/* SSD1680 e-paper driver (mono, 128 sources x up to 296 gates), esp-idf SPI.
 *
 * Buffer format is panel-native: gate-major rows, sources/8 bytes per row,
 * MSB = source 0, bit set = WHITE (SSD1680 RAM convention). For the 2.9"
 * 296x128 panel: 296 rows x 16 bytes = 4736 bytes. Rotation to landscape is
 * the caller's job (see monogfx + display module).
 *
 * Every refresh powers the panel up from deep sleep and puts it back to deep
 * sleep after (deep sleep mode 1, ~1µA class). Partial refresh is a
 * differential update: the driver keeps a copy of the previous frame and
 * writes it to the "old image" RAM so only changed pixels flip.
 *
 * Command values from the SSD1680 datasheet, cross-checked against
 * Adafruit_EPD (MIT). Update-sequence values 0xF7 (mode 1, full) and 0xFF
 * (mode 2, differential) are datasheet display-update options.
 */
#pragma once

#include <stdint.h>
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_host_device_t host; /* e.g. SPI2_HOST */
    int sck_gpio;
    int mosi_gpio;
    int cs_gpio;
    int dc_gpio;
    int rst_gpio;
    int busy_gpio;
    uint16_t gates;   /* panel long axis, e.g. 296 */
    uint16_t sources; /* panel short axis, multiple of 8, e.g. 128 */
} ssd1680_config_t;

typedef struct ssd1680 *ssd1680_handle_t;

esp_err_t ssd1680_init(const ssd1680_config_t *cfg, ssd1680_handle_t *out);

/* Full refresh: complete waveform, clears ghosting, ~2s, visible flash. */
esp_err_t ssd1680_refresh_full(ssd1680_handle_t h, const uint8_t *frame);

/* Partial (differential) refresh: fast, no flash, accumulates ghosting.
 * Falls back to full refresh if no previous frame is known. */
esp_err_t ssd1680_refresh_partial(ssd1680_handle_t h, const uint8_t *frame);

#ifdef __cplusplus
}
#endif
