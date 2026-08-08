#include "display.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "qrcode.h"

#include "app_config.h"
#include "monogfx.h"
#include "ssd1680.h"

static const char *TAG = "display";

#define LAND_W 296
#define LAND_H 128
#define FRAME_BYTES (LAND_W * LAND_H / 8)

typedef struct {
    enum { MSG_READINGS, MSG_COMMISSIONING, MSG_DIAG, MSG_SETTINGS } type;
    sensor_readings_t readings;
    char qr[128];
    char manual[24];
    display_diag_t diag;
    char fw[32];
    display_settings_view_t settings;
} display_msg_t;

static ssd1680_handle_t s_panel;
static monogfx_t s_gfx;
static uint8_t s_land[FRAME_BYTES];
static uint8_t s_native[FRAME_BYTES];
static QueueHandle_t s_queue;
static unsigned s_partials_since_full;

/* Landscape (296x128, bit=black) -> native (296 gate rows x 16 source bytes,
 * bit=white). HW-VERIFY: flip flags until the image is upright in the case. */
static void convert_to_native(void)
{
    memset(s_native, 0xFF, sizeof(s_native));
    for (int y = 0; y < LAND_H; y++) {
        for (int x = 0; x < LAND_W; x++) {
            if (!(s_land[(y * LAND_W + x) / 8] & (0x80 >> (x & 7)))) {
                continue; /* white */
            }
            int g = DISPLAY_FLIP_LONG_AXIS ? (LAND_W - 1 - x) : x;
            int s = DISPLAY_FLIP_SHORT_AXIS ? (LAND_H - 1 - y) : y;
            s_native[g * (LAND_H / 8) + s / 8] &= ~(0x80 >> (s & 7));
        }
    }
}

static void push_frame(void)
{
    convert_to_native();
    bool full = s_partials_since_full >= DISPLAY_FULL_REFRESH_EVERY_N;
    esp_err_t err = full ? ssd1680_refresh_full(s_panel, s_native)
                         : ssd1680_refresh_partial(s_panel, s_native);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "refresh failed: %s", esp_err_to_name(err));
        return;
    }
    s_partials_since_full = full ? 0 : s_partials_since_full + 1;
}

/* Draws value with seven-segment digits ('.', '-' supported), returns end x. */
static int draw_seg_string(const char *s, int x, int y, int w, int h, int thick)
{
    for (; *s; s++) {
        if (*s == '.') {
            monogfx_fill_rect(&s_gfx, x, y + h - thick, thick, thick, true);
            x += thick + thick / 2 + 2;
        } else {
            x += monogfx_draw_seg(&s_gfx, x, y, w, h, thick, *s) + 6;
        }
    }
    return x;
}

static void render_readings(const sensor_readings_t *r)
{
    monogfx_clear(&s_gfx);

    app_settings_t cfg = settings_get();
    float temp = cfg.use_fahrenheit ? r->temp_c * 9.0f / 5.0f + 32.0f : r->temp_c;

    char buf[16];
    /* temperature, big, left */
    snprintf(buf, sizeof(buf), "%.1f", temp);
    int end = draw_seg_string(buf, 10, 18, 34, 64, 8);
    monogfx_draw_text(&s_gfx, end + 2, 18, cfg.use_fahrenheit ? "F" : "C", 3);

    /* humidity, right */
    snprintf(buf, sizeof(buf), "%.0f", r->rh);
    int hx = 214;
    hx = draw_seg_string(buf, hx, 26, 22, 44, 6);
    monogfx_draw_text(&s_gfx, hx + 2, 26, "%", 2);
    monogfx_draw_text(&s_gfx, 214, 78, "RH", 2);

    monogfx_fill_rect(&s_gfx, 200, 14, 2, 84, true); /* divider */

    /* bottom bar */
    monogfx_fill_rect(&s_gfx, 0, 104, LAND_W, 1, true);
    snprintf(buf, sizeof(buf), "BAT %u%%", r->battery_pct);
    monogfx_draw_text(&s_gfx, 10, 110, buf, 2);
    snprintf(buf, sizeof(buf), "%.2fV", r->battery_mv / 1000.0f);
    monogfx_draw_text(&s_gfx, 120, 110, buf, 2);
}

/* espressif/qrcode component hands the finished QR to a callback with no
 * user argument; single display instance makes a static target acceptable. */
static void qr_to_gfx(esp_qrcode_handle_t qrcode)
{
    int size = esp_qrcode_get_size(qrcode);
    int scale = 112 / size;
    if (scale < 1) {
        scale = 1;
    }
    int offset = (LAND_H - size * scale) / 2;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                monogfx_fill_rect(&s_gfx, 8 + x * scale, offset + y * scale, scale, scale, true);
            }
        }
    }
}

static void render_commissioning(const char *qr_payload, const char *manual_code)
{
    monogfx_clear(&s_gfx);

    esp_qrcode_config_t qr_cfg = ESP_QRCODE_CONFIG_DEFAULT();
    qr_cfg.display_func = qr_to_gfx;
    if (esp_qrcode_generate(&qr_cfg, qr_payload) != ESP_OK) {
        ESP_LOGW(TAG, "QR generation failed");
    }

    monogfx_draw_text(&s_gfx, 140, 16, "homecadia", 2);
    monogfx_draw_text(&s_gfx, 140, 40, "Add via Matter:", 1);
    monogfx_draw_text(&s_gfx, 140, 56, "scan QR, or code:", 1);
    monogfx_draw_text(&s_gfx, 140, 76, manual_code, 2);
}

static void render_diag(const display_diag_t *d, const char *fw)
{
    monogfx_clear(&s_gfx);
    monogfx_draw_text(&s_gfx, 10, 8, "DIAGNOSTICS", 2);
    monogfx_fill_rect(&s_gfx, 0, 26, LAND_W, 1, true);

    char buf[40];
    if (d->rssi_valid) {
        snprintf(buf, sizeof(buf), "Thread RSSI %ddBm", d->rssi_dbm);
    } else {
        snprintf(buf, sizeof(buf), "Thread: no parent");
    }
    monogfx_draw_text(&s_gfx, 10, 34, buf, 2);

    snprintf(buf, sizeof(buf), "Bat %u%% %.2fV", d->battery_pct, d->battery_mv / 1000.0f);
    monogfx_draw_text(&s_gfx, 10, 56, buf, 2);

    uint32_t days = d->uptime_s / 86400;
    snprintf(buf, sizeof(buf), "Up %lud %02lu:%02lu", (unsigned long)days,
             (unsigned long)(d->uptime_s % 86400) / 3600, (unsigned long)(d->uptime_s % 3600) / 60);
    monogfx_draw_text(&s_gfx, 10, 78, buf, 2);

    snprintf(buf, sizeof(buf), "FW %s", fw);
    monogfx_draw_text(&s_gfx, 10, 104, buf, 1);
}

static void render_settings(const display_settings_view_t *v)
{
    monogfx_clear(&s_gfx);
    monogfx_draw_text(&s_gfx, 10, 8, "SETTINGS", 2);
    monogfx_fill_rect(&s_gfx, 0, 26, LAND_W, 1, true);

    char buf[32];
    const int rows_y[DISPLAY_SETTINGS_ITEMS] = {40, 66};

    snprintf(buf, sizeof(buf), "Poll     %s%us%s", v->editing && v->selected == 0 ? "[" : " ",
             v->values.poll_interval_s, v->editing && v->selected == 0 ? "]" : " ");
    monogfx_draw_text(&s_gfx, 30, rows_y[0], buf, 2);

    snprintf(buf, sizeof(buf), "Units    %s%s%s", v->editing && v->selected == 1 ? "[" : " ",
             v->values.use_fahrenheit ? "F" : "C", v->editing && v->selected == 1 ? "]" : " ");
    monogfx_draw_text(&s_gfx, 30, rows_y[1], buf, 2);

    if (v->highlight_menu) {
        monogfx_draw_text(&s_gfx, 10, rows_y[v->selected], ">", 2);
    }

    monogfx_draw_text(&s_gfx, 10, 108, v->highlight_menu ? "push: edit/confirm  turn: select" : "push to change settings", 1);
}

static void display_task(void *arg)
{
    display_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (msg.type) {
        case display_msg_t::MSG_READINGS:
            render_readings(&msg.readings);
            break;
        case display_msg_t::MSG_DIAG:
            render_diag(&msg.diag, msg.fw);
            break;
        case display_msg_t::MSG_SETTINGS:
            render_settings(&msg.settings);
            break;
        case display_msg_t::MSG_COMMISSIONING:
            render_commissioning(msg.qr, msg.manual);
            s_partials_since_full = DISPLAY_FULL_REFRESH_EVERY_N; /* force full: big change */
            break;
        }
        push_frame();
    }
}

esp_err_t display_init(void)
{
    ssd1680_config_t cfg = {
        .host = SPI2_HOST,
        .sck_gpio = EPD_PIN_SCK,
        .mosi_gpio = EPD_PIN_MOSI,
        .cs_gpio = EPD_PIN_CS,
        .dc_gpio = EPD_PIN_DC,
        .rst_gpio = EPD_PIN_RST,
        .busy_gpio = EPD_PIN_BUSY,
        .gates = LAND_W,
        .sources = LAND_H,
    };
    esp_err_t err = ssd1680_init(&cfg, &s_panel);
    if (err != ESP_OK) {
        return err;
    }
    monogfx_init(&s_gfx, LAND_W, LAND_H, s_land);

    s_queue = xQueueCreate(4, sizeof(display_msg_t));
    if (!s_queue) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(display_task, "display", 4096, nullptr, 3, nullptr) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void display_show_readings(const sensor_readings_t *r)
{
    if (!s_queue) {
        return;
    }
    display_msg_t msg = {};
    msg.type = display_msg_t::MSG_READINGS;
    msg.readings = *r;
    xQueueSend(s_queue, &msg, 0); /* drop if queue full: next report re-renders */
}

void display_show_commissioning(const char *qr_payload, const char *manual_code)
{
    if (!s_queue) {
        return;
    }
    display_msg_t msg = {};
    msg.type = display_msg_t::MSG_COMMISSIONING;
    strlcpy(msg.qr, qr_payload, sizeof(msg.qr));
    strlcpy(msg.manual, manual_code, sizeof(msg.manual));
    xQueueSend(s_queue, &msg, 0);
}

void display_show_diagnostics(const display_diag_t *d)
{
    if (!s_queue) {
        return;
    }
    display_msg_t msg = {};
    msg.type = display_msg_t::MSG_DIAG;
    msg.diag = *d;
    strlcpy(msg.fw, d->fw_version ? d->fw_version : "?", sizeof(msg.fw));
    xQueueSend(s_queue, &msg, 0);
}

void display_show_settings(const display_settings_view_t *v)
{
    if (!s_queue) {
        return;
    }
    display_msg_t msg = {};
    msg.type = display_msg_t::MSG_SETTINGS;
    msg.settings = *v;
    xQueueSend(s_queue, &msg, 0);
}
