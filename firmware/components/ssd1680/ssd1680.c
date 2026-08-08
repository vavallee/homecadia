#include "ssd1680.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ssd1680";

/* Commands (SSD1680 datasheet) */
#define CMD_DRIVER_CONTROL 0x01
#define CMD_DEEP_SLEEP     0x10
#define CMD_DATA_MODE      0x11
#define CMD_SW_RESET       0x12
#define CMD_TEMP_CONTROL   0x18
#define CMD_MASTER_ACTIVATE 0x20
#define CMD_DISP_CTRL2     0x22
#define CMD_WRITE_RAM_BW   0x24
#define CMD_WRITE_RAM_OLD  0x26 /* "RED" RAM doubles as previous image in mode-2 updates */
#define CMD_WRITE_BORDER   0x3C
#define CMD_SET_RAMXPOS    0x44
#define CMD_SET_RAMYPOS    0x45
#define CMD_SET_RAMXCOUNT  0x4E
#define CMD_SET_RAMYCOUNT  0x4F

#define UPDATE_FULL    0xF7 /* display mode 1, load OTP LUT + temp */
#define UPDATE_PARTIAL 0xFF /* display mode 2, differential */

#define BORDER_FULL    0x05
#define BORDER_PARTIAL 0x80 /* VCOM border: no border flicker on partial */

#define BUSY_TIMEOUT_MS 6000

struct ssd1680 {
    ssd1680_config_t cfg;
    spi_device_handle_t spi;
    size_t frame_size;
    uint8_t *prev; /* previous displayed frame, NULL until first refresh */
};

static esp_err_t cmd(struct ssd1680 *h, uint8_t c)
{
    gpio_set_level(h->cfg.dc_gpio, 0);
    spi_transaction_t t = {.length = 8, .tx_buffer = &c};
    return spi_device_polling_transmit(h->spi, &t);
}

static esp_err_t data(struct ssd1680 *h, const uint8_t *buf, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    gpio_set_level(h->cfg.dc_gpio, 1);
    spi_transaction_t t = {.length = len * 8, .tx_buffer = buf};
    return spi_device_polling_transmit(h->spi, &t);
}

static esp_err_t cmd1(struct ssd1680 *h, uint8_t c, uint8_t v)
{
    esp_err_t err = cmd(h, c);
    return err != ESP_OK ? err : data(h, &v, 1);
}

static esp_err_t busy_wait(struct ssd1680 *h)
{
    for (int waited = 0; waited < BUSY_TIMEOUT_MS; waited += 10) {
        if (gpio_get_level(h->cfg.busy_gpio) == 0) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGE(TAG, "BUSY stuck high >%dms", BUSY_TIMEOUT_MS);
    return ESP_ERR_TIMEOUT;
}

/* Wake from deep sleep and (re)initialize registers. */
static esp_err_t power_up(struct ssd1680 *h)
{
    gpio_set_level(h->cfg.rst_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(h->cfg.rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_err_t err = busy_wait(h);
    if (err != ESP_OK) {
        return err;
    }

    cmd(h, CMD_SW_RESET);
    vTaskDelay(pdMS_TO_TICKS(20));
    err = busy_wait(h);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t g = h->cfg.gates;
    uint8_t drv[3] = {(uint8_t)((g - 1) & 0xFF), (uint8_t)((g - 1) >> 8), 0x00};
    cmd(h, CMD_DRIVER_CONTROL);
    data(h, drv, 3);

    cmd1(h, CMD_DATA_MODE, 0x03); /* x and y increment */

    uint8_t xpos[2] = {0x00, (uint8_t)(h->cfg.sources / 8 - 1)};
    cmd(h, CMD_SET_RAMXPOS);
    data(h, xpos, 2);

    uint8_t ypos[4] = {0x00, 0x00, (uint8_t)((g - 1) & 0xFF), (uint8_t)((g - 1) >> 8)};
    cmd(h, CMD_SET_RAMYPOS);
    data(h, ypos, 4);

    cmd1(h, CMD_TEMP_CONTROL, 0x80); /* internal temperature sensor */
    return ESP_OK;
}

static void set_ram_counter(struct ssd1680 *h)
{
    cmd1(h, CMD_SET_RAMXCOUNT, 0x00);
    uint8_t y[2] = {0x00, 0x00};
    cmd(h, CMD_SET_RAMYCOUNT);
    data(h, y, 2);
}

static esp_err_t deep_sleep(struct ssd1680 *h)
{
    esp_err_t err = cmd1(h, CMD_DEEP_SLEEP, 0x01); /* mode 1: RAM retained */
    vTaskDelay(pdMS_TO_TICKS(10));
    return err;
}

static esp_err_t refresh(struct ssd1680 *h, const uint8_t *frame, bool partial)
{
    /* HW-VERIFY: partial-refresh quality (ghosting, border) on the Seeed
     * 2.9" panel; tune BORDER_/UPDATE_ values if artifacts show. */
    if (partial && !h->prev) {
        partial = false;
    }

    esp_err_t err = power_up(h);
    if (err != ESP_OK) {
        return err;
    }

    cmd1(h, CMD_WRITE_BORDER, partial ? BORDER_PARTIAL : BORDER_FULL);

    if (partial) {
        /* previous image for the differential */
        set_ram_counter(h);
        cmd(h, CMD_WRITE_RAM_OLD);
        data(h, h->prev, h->frame_size);
    }

    set_ram_counter(h);
    cmd(h, CMD_WRITE_RAM_BW);
    data(h, frame, h->frame_size);

    cmd1(h, CMD_DISP_CTRL2, partial ? UPDATE_PARTIAL : UPDATE_FULL);
    cmd(h, CMD_MASTER_ACTIVATE);
    err = busy_wait(h);

    if (err == ESP_OK) {
        if (!h->prev) {
            h->prev = malloc(h->frame_size);
        }
        if (h->prev) {
            memcpy(h->prev, frame, h->frame_size);
        }
    }

    deep_sleep(h);
    return err;
}

esp_err_t ssd1680_refresh_full(ssd1680_handle_t h, const uint8_t *frame)
{
    return refresh(h, frame, false);
}

esp_err_t ssd1680_refresh_partial(ssd1680_handle_t h, const uint8_t *frame)
{
    return refresh(h, frame, true);
}

esp_err_t ssd1680_init(const ssd1680_config_t *cfg, ssd1680_handle_t *out)
{
    struct ssd1680 *h = calloc(1, sizeof(*h));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    h->cfg = *cfg;
    h->frame_size = (size_t)cfg->gates * cfg->sources / 8;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->dc_gpio) | (1ULL << cfg->rst_gpio),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        goto fail;
    }
    gpio_set_level(cfg->rst_gpio, 1);

    io.pin_bit_mask = 1ULL << cfg->busy_gpio;
    io.mode = GPIO_MODE_INPUT;
    err = gpio_config(&io);
    if (err != ESP_OK) {
        goto fail;
    }

    spi_bus_config_t bus = {
        .mosi_io_num = cfg->mosi_gpio,
        .miso_io_num = -1,
        .sclk_io_num = cfg->sck_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)h->frame_size + 8,
    };
    err = spi_bus_initialize(cfg->host, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        goto fail;
    }

    spi_device_interface_config_t dev = {
        .mode = 0,
        .clock_speed_hz = 10 * 1000 * 1000, /* SSD1680 write max 20MHz */
        .spics_io_num = cfg->cs_gpio,
        .queue_size = 2,
    };
    err = spi_bus_add_device(cfg->host, &dev, &h->spi);
    if (err != ESP_OK) {
        spi_bus_free(cfg->host);
        goto fail;
    }

    *out = h;
    return ESP_OK;

fail:
    free(h);
    return err;
}
