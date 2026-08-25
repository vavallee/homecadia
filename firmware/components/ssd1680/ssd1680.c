#include "ssd1680.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
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
#define ACTIVATE_RISE_TIMEOUT_MS 200 /* BUSY goes high within a few ms of a real update */
/* Seconds to park MOSI low at init so a meter can read the net. 0 = off.
 * Bench-only: it stalls display bring-up for this long on every boot. */
#ifndef SSD1680_HOLD_MOSI_LOW_S
#define SSD1680_HOLD_MOSI_LOW_S 0
#endif

#define PROBE_RISE_TIMEOUT_MS 50     /* BUSY goes high within a few ms of a real reset */

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

/* Wait out a display update started by MASTER_ACTIVATE.
 *
 * busy_wait() alone is not enough here: it returns ESP_OK the moment it sees
 * BUSY low, which is also the state of a panel that never received the command
 * stream at all, so a disconnected display and a successful update look
 * identical. A real update raises BUSY first -- about 1.8s for a full refresh
 * and 0.5s for a partial one -- so require the rise before waiting for the fall. */
static esp_err_t busy_wait_update(struct ssd1680 *h)
{
    for (int waited = 0; gpio_get_level(h->cfg.busy_gpio) == 0; waited += 5) {
        if (waited >= ACTIVATE_RISE_TIMEOUT_MS) {
            ESP_LOGE(TAG, "BUSY never rose within %dms of MASTER_ACTIVATE: the panel "
                          "did not accept the command stream", ACTIVATE_RISE_TIMEOUT_MS);
            return ESP_ERR_INVALID_RESPONSE;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return busy_wait(h);
}

/* Is the BUSY line connected to anything, electrically?
 *
 * The C6's internal pulls are weak (~45k). A pin nothing is driving follows the
 * pull; a pin an active driver holds does not. So reading it once with the
 * pull-up and once with the pull-down separates "BUSY is not wired through"
 * from "BUSY is wired and the panel is holding it low" -- which the reset probe
 * on its own cannot tell apart, since both read low.
 *
 * Leaves the pin as a plain input, the state the rest of the driver expects. */
static const char *line_state(int gpio)
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
        return "floating";
    }
    if (!up && !down) {
        return "held LOW";
    }
    if (up && down) {
        return "held HIGH";
    }
    return "inconsistent";
}

/* Log the electrical state of every panel signal before anything drives them.
 *
 * Reported without a verdict on purpose. An earlier version flagged each pin
 * against an expected state and was wrong more often than right: a known-good
 * bare driver board reads SCK and MOSI "held HIGH" from its own pull-ups, and
 * BUSY reads "floating" whenever no panel is attached. Both were flagged as
 * faults and neither was one.
 *
 * These values are context for the drive test below, which is the check that
 * actually distinguishes a fault. Note the pull-ups here are weak (~45k), so
 * "held" only means something is winning against 45k -- not that it can hold
 * the line against a driver. */
static void scan_pins(const ssd1680_config_t *cfg)
{
    const struct {
        const char *name;
        int gpio;
    } pins[] = {
        {"RST ", cfg->rst_gpio},  {"CS  ", cfg->cs_gpio},   {"DC  ", cfg->dc_gpio},
        {"SCK ", cfg->sck_gpio},  {"MOSI", cfg->mosi_gpio}, {"BUSY", cfg->busy_gpio},
    };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        ESP_LOGI(TAG, "  %s GPIO%-2d %s", pins[i].name, pins[i].gpio, line_state(pins[i].gpio));
    }
}

/* Drive each panel signal and read back what the line actually did.
 *
 * The passive scan above cannot separate a fault from the driver board's own
 * pull resistors -- without a schematic, "held LOW" at rest is not evidence of
 * anything. This is unambiguous: a line the C6 drives high must read high. One
 * that does not is shorted, and a shorted RST holds the panel in reset, which
 * looks exactly like a panel that ignores every command.
 *
 * INPUT_OUTPUT mode so the input buffer stays enabled while the pin is driven.
 * These are all panel inputs, so toggling them is safe; BUSY is excluded
 * because it is the panel's output and driving it would fight the panel. */
static void drive_test(const ssd1680_config_t *cfg)
{
    const struct {
        const char *name;
        int gpio;
    } pins[] = {
        {"RST ", cfg->rst_gpio},
        {"CS  ", cfg->cs_gpio},
        {"DC  ", cfg->dc_gpio},
        {"SCK ", cfg->sck_gpio},
        {"MOSI", cfg->mosi_gpio},
    };

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << pins[i].gpio,
            .mode = GPIO_MODE_INPUT_OUTPUT,
        };
        gpio_config(&io);

        gpio_set_level(pins[i].gpio, 1);
        esp_rom_delay_us(500);
        int hi = gpio_get_level(pins[i].gpio);

        gpio_set_level(pins[i].gpio, 0);
        esp_rom_delay_us(500);
        int lo = gpio_get_level(pins[i].gpio);

        gpio_set_level(pins[i].gpio, 1);

        const char *verdict;
        if (hi == 1 && lo == 0) {
            verdict = "follows the driver - OK";
        } else if (hi == 0) {
            verdict = "STUCK LOW while driven high - shorted to GND";
        } else {
            verdict = "STUCK HIGH while driven low - shorted to 3V3";
        }
        if (hi == 1 && lo == 0) {
            ESP_LOGI(TAG, "  %s GPIO%-2d drive hi=%d lo=%d  %s", pins[i].name, pins[i].gpio, hi,
                     lo, verdict);
        } else {
            ESP_LOGE(TAG, "  %s GPIO%-2d drive hi=%d lo=%d  %s", pins[i].name, pins[i].gpio, hi,
                     lo, verdict);
        }
    }

#if SSD1680_HOLD_MOSI_LOW_S
    /* Bench diagnostic: park MOSI driven low so a meter can read the actual
     * voltage on the net.
     *
     * drive_test() reads a LOGIC level, so a net held at an intermediate
     * voltage -- contention with a resistive source -- reads high and is
     * reported as "stuck". That is indistinguishable from a hard short by
     * firmware alone. The voltage is not: ~3.3V means something is driving
     * against us, ~1-2V means a resistive path, ~0V means the readback was
     * misleading and there is no fault here at all. */
    gpio_config_t hold = {
        .pin_bit_mask = 1ULL << cfg->mosi_gpio,
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    gpio_config(&hold);
    gpio_set_level(cfg->mosi_gpio, 0);
    for (int left = SSD1680_HOLD_MOSI_LOW_S; left > 0; left--) {
        ESP_LOGW(TAG, "holding MOSI (GPIO%d) LOW for measurement: %ds left (reads %d)",
                 (int)cfg->mosi_gpio, left, gpio_get_level(cfg->mosi_gpio));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGW(TAG, "MOSI hold done; continuing normal init");
#endif
}

/* Observe what BUSY does when RST is released, before any SPI traffic.
 *
 * An earlier version of this required BUSY to RISE after reset and reported a
 * missing panel when it did not. That was wrong: this panel releases BUSY LOW
 * coming out of reset, so a healthy display was reported as absent. Do not
 * infer presence or absence from a level here -- only report the transition,
 * which is what actually carries information: a line that moves when RST moves
 * is connected to something that is powered and reacting. */
static void probe_panel(struct ssd1680 *h)
{
    gpio_set_level(h->cfg.rst_gpio, 0);
    esp_rom_delay_us(10000);
    int in_reset = gpio_get_level(h->cfg.busy_gpio);

    gpio_set_level(h->cfg.rst_gpio, 1);
    esp_rom_delay_us(10000);
    int released = gpio_get_level(h->cfg.busy_gpio);

    if (in_reset != released) {
        ESP_LOGI(TAG, "panel reacts to RST: BUSY %d -> %d across reset release",
                 in_reset, released);
    } else {
        ESP_LOGW(TAG, "BUSY (GPIO%d) did not move across a reset pulse on RST (GPIO%d); "
                      "stayed %d. Inconclusive on its own -- see the SW_RESET check below.",
                 (int)h->cfg.busy_gpio, (int)h->cfg.rst_gpio, in_reset);
    }
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

    /* SW_RESET is a command, so BUSY rising here proves the panel is receiving
     * the command stream -- SPI, DC and CS all working. busy_wait() alone
     * cannot show this: it accepts BUSY-low, which is also a panel that heard
     * nothing. Diagnostic only; the sequence continues either way. */
    cmd(h, CMD_SW_RESET);
    int64_t t0 = esp_timer_get_time();
    bool rose = false;
    while (esp_timer_get_time() - t0 < PROBE_RISE_TIMEOUT_MS * 1000) {
        if (gpio_get_level(h->cfg.busy_gpio) != 0) {
            rose = true;
            break;
        }
        esp_rom_delay_us(200);
    }
    if (rose) {
        ESP_LOGI(TAG, "command path OK: BUSY rose %lldus after SW_RESET",
                 esp_timer_get_time() - t0);
    } else {
        ESP_LOGE(TAG, "command path FAULT: BUSY never rose within %dms of SW_RESET. The panel "
                      "is powered but is not receiving commands -- check DC (GPIO%d), CS "
                      "(GPIO%d), SCK (GPIO%d) and MOSI (GPIO%d).",
                 PROBE_RISE_TIMEOUT_MS, (int)h->cfg.dc_gpio, (int)h->cfg.cs_gpio,
                 (int)h->cfg.sck_gpio, (int)h->cfg.mosi_gpio);
    }
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
    err = busy_wait_update(h);

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

    ESP_LOGI(TAG, "panel signal scan (before anything is driven):");
    scan_pins(cfg);
    ESP_LOGI(TAG, "panel signal drive test:");
    drive_test(cfg);

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

    /* Before SPI exists, so a pass here rules the connector out and points any
     * remaining fault at the command lines. */
    probe_panel(h);

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
