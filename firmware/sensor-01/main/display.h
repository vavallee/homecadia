/* E-ink display: rendering + refresh policy. Owns a task; all refreshes are
 * queued so callers (esp_timer task, Matter event thread) never block on the
 * ~2s panel busy time. */
#pragma once

#include "esp_err.h"
#include "sensor_loop.h"
#include "settings.h"

typedef struct {
    bool rssi_valid;
    int8_t rssi_dbm;
    uint32_t uptime_s;
    uint8_t battery_pct;
    uint32_t battery_mv;
    const char *fw_version; /* copied on enqueue */
} display_diag_t;

#define DISPLAY_SETTINGS_ITEMS 2 /* 0: poll interval, 1: units */

typedef struct {
    app_settings_t values;
    uint8_t selected;
    bool highlight_menu; /* selection marker visible (MENU/EDIT modes) */
    bool editing;        /* selected value being edited */
} display_settings_view_t;

esp_err_t display_init(void);

/* View 1: big temperature + RH, battery on the bottom line.
 * Uses partial refresh; every DISPLAY_FULL_REFRESH_EVERY_N-th refresh is a
 * full one to clear ghosting. */
void display_show_readings(const sensor_readings_t *r);

/* First-boot / decommissioned screen: commissioning QR + manual pairing code.
 * Latches: until display_commissioning_done() the readings view renders these
 * codes instead, so periodic sensor reports cannot paint over an unpaired
 * device's only way of being paired. */
void display_show_commissioning(const char *qr_payload, const char *manual_code);

/* Releases the display_show_commissioning() latch (call when a fabric exists). */
void display_commissioning_done(void);

/* View 2: Thread RSSI, uptime, battery, firmware version. */
void display_show_diagnostics(const display_diag_t *d);

/* View 3: settings menu. */
void display_show_settings(const display_settings_view_t *v);
