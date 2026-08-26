/* Boot-time harness scan: probe every pin in app_config.h before any driver
 * touches it and log what the wiring actually does. Compiled in only with
 * CONFIG_HOMECADIA_BENCH_SELFTEST (sdkconfig.bench). */
#pragma once

#include "sdkconfig.h"

#if CONFIG_HOMECADIA_BENCH_SELFTEST
void bench_selftest(void);
/* Re-read the I2C and encoder lines at a named point in the init sequence, to
 * find which step changes them. ONE-SHOT BISECT TOOL, not left wired in:
 * pinprobe reconfigures the pins, which detaches the I2C driver and disables
 * the encoder's edge interrupt and pull-ups. Calling it after ui_init() or
 * sensor_loop_start() silently breaks both (2026-08-25: it cost a round of
 * "the encoder produces no events"). Insert calls temporarily, then remove. */
void bench_probe_i2c(const char *when);
/* Poll the encoder A/B pins and log every level change. Call after ec11_init
 * so the pull-ups are on. Shows which line actually toggles when the knob
 * turns, independent of the ISR's quadrature filtering. */
void bench_encoder_monitor_start(void);
#else
static inline void bench_selftest(void) {}
static inline void bench_probe_i2c(const char *) {}
static inline void bench_encoder_monitor_start(void) {}
#endif
