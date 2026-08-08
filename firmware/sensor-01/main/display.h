/* E-ink display: rendering + refresh policy. Owns a task; all refreshes are
 * queued so callers (esp_timer task, Matter event thread) never block on the
 * ~2s panel busy time. */
#pragma once

#include "esp_err.h"
#include "sensor_loop.h"

esp_err_t display_init(void);

/* View 1: big temperature + RH, battery on the bottom line.
 * Uses partial refresh; every DISPLAY_FULL_REFRESH_EVERY_N-th refresh is a
 * full one to clear ghosting. */
void display_show_readings(const sensor_readings_t *r);

/* First-boot / decommissioned screen: commissioning QR + manual pairing code. */
void display_show_commissioning(const char *qr_payload, const char *manual_code);
