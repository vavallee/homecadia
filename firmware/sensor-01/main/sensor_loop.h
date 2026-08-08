/* Periodic measurement + delta-based Matter attribute reporting. */
#pragma once

#include <stdint.h>
#include "esp_err.h"

/* Endpoint IDs for the clusters this loop feeds. */
typedef struct {
    uint16_t temperature_endpoint_id;
    uint16_t humidity_endpoint_id;
    uint16_t power_source_endpoint_id;
} sensor_loop_endpoints_t;

/* Starts the poll timer (SENSOR_POLL_INTERVAL_S) and takes an immediate
 * first reading. Call after esp_matter::start(). */
esp_err_t sensor_loop_start(const sensor_loop_endpoints_t *endpoints);

/* Last readings, for the display (milestone 3). */
typedef struct {
    float temp_c;
    float rh;
    uint32_t battery_mv;
    uint8_t battery_pct;
    bool valid;
} sensor_readings_t;

sensor_readings_t sensor_loop_get_readings(void);
