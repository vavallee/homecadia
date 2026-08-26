#include "sht40.h"
#include "esp_log.h"
#include "pinprobe.h"

#include <stdlib.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sht40";

#define SHT40_CMD_MEASURE_HIGH_PRECISION 0xFD
#define SHT40_MEASURE_DURATION_MS        10 /* datasheet: 8.3ms max, high precision */

struct sht40 {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
};

/* Sensirion CRC-8: poly 0x31, init 0xFF, over 2 data bytes. */
static uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
        }
    }
    return crc;
}


/* Sweep every 7-bit address: a device answering somewhere unexpected is a
 * wiring swap or a wrong address constant, not a dead sensor. */
static void scan_addresses(i2c_master_bus_handle_t bus)
{
    int found = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) {
            ESP_LOGE(TAG, "  device answering at 0x%02X", a);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "  no device answered on any address 0x08-0x77");
    }
}

esp_err_t sht40_init(int sda_gpio, int scl_gpio, uint8_t i2c_addr, sht40_handle_t *out)
{
    struct sht40 *h = calloc(1, sizeof(*h));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }

    /* Sample the lines first: once the bus exists, the peripheral owns these
     * pins and any reading describes the controller, not the wiring. */
    pinprobe_line_t sda_state = pinprobe_line_state(sda_gpio);
    pinprobe_line_t scl_state = pinprobe_line_state(scl_gpio);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1, /* auto-select */
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, /* Grove board has pullups; harmless belt-and-suspenders */
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &h->bus);
    if (err != ESP_OK) {
        free(h);
        return err;
    }

    /* The line probe above pulled SDA low with SCL high -- a START -- so a
     * present sensor may now be mid-transaction. Recover before asking. */
    i2c_master_bus_reset(h->bus);

    err = i2c_master_probe(h->bus, i2c_addr, 50);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no SHT40 at 0x%02X on SDA=GPIO%d SCL=GPIO%d; bus diagnosis:",
                 i2c_addr, sda_gpio, scl_gpio);
        ESP_LOGE(TAG, "  SDA GPIO%-2d %s", sda_gpio, pinprobe_line_name(sda_state));
        ESP_LOGE(TAG, "  SCL GPIO%-2d %s (held LOW = no clock possible)", scl_gpio,
                 pinprobe_line_name(scl_state));
        scan_addresses(h->bus);
        i2c_del_master_bus(h->bus);
        free(h);
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(h->bus, &dev_cfg, &h->dev);
    if (err != ESP_OK) {
        i2c_del_master_bus(h->bus);
        free(h);
        return err;
    }

    *out = h;
    return ESP_OK;
}

esp_err_t sht40_read(sht40_handle_t h, float *temp_c, float *rh)
{
    uint8_t cmd = SHT40_CMD_MEASURE_HIGH_PRECISION;
    esp_err_t err = i2c_master_transmit(h->dev, &cmd, 1, 50);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(SHT40_MEASURE_DURATION_MS));

    uint8_t buf[6];
    err = i2c_master_receive(h->dev, buf, sizeof(buf), 50);
    if (err != ESP_OK) {
        return err;
    }
    if (crc8(&buf[0], 2) != buf[2] || crc8(&buf[3], 2) != buf[5]) {
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_t = (buf[0] << 8) | buf[1];
    uint16_t raw_rh = (buf[3] << 8) | buf[4];

    *temp_c = -45.0f + 175.0f * raw_t / 65535.0f;
    float rh_val = -6.0f + 125.0f * raw_rh / 65535.0f;
    /* datasheet: computed RH can slightly exceed the physical 0..100 range */
    if (rh_val < 0.0f) {
        rh_val = 0.0f;
    } else if (rh_val > 100.0f) {
        rh_val = 100.0f;
    }
    *rh = rh_val;
    return ESP_OK;
}
