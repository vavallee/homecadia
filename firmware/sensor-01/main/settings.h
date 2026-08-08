/* User settings, persisted in NVS. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t poll_interval_s; /* sensor poll cadence */
    bool use_fahrenheit;      /* display only; Matter always reports 0.01°C */
} app_settings_t;

esp_err_t settings_init(void);
app_settings_t settings_get(void);
esp_err_t settings_save(const app_settings_t *s);
