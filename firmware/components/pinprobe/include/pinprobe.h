/* Electrical checks on a GPIO, for bench harness vetting.
 *
 * Both calls MUST run before any driver owns the pin. A pin claimed by a
 * peripheral (I2C, SPI, an interrupt handler) reports the peripheral, not the
 * wire: the sht40 diagnostic first ran after i2c_new_master_bus() and returned
 * the same reading for every wiring change because it was measuring the
 * controller's own pull-ups and stuck-bus state. See field-notes.md section 16.
 *
 * Both leave the pin as a plain input with no pulls. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PINPROBE_FLOATING,  /* follows the internal pulls: nothing external drives it */
    PINPROBE_HELD_LOW,  /* stays low against the ~45k pull-up */
    PINPROBE_HELD_HIGH, /* stays high against the ~45k pull-down */
    PINPROBE_INCONSISTENT,
} pinprobe_line_t;

/* Read the pin with the internal pull-up, then the pull-down. "held" means
 * something external wins against ~45k -- not that it can hold the line
 * against a driver. */
pinprobe_line_t pinprobe_line_state(int gpio);
const char *pinprobe_line_name(pinprobe_line_t s);

typedef struct {
    int hi; /* level read back while driving high */
    int lo; /* level read back while driving low */
} pinprobe_drive_t;

/* Drive the pin high then low in INPUT_OUTPUT mode and read back each time.
 * A healthy output reads {1,0}. {0,x} is shorted to ground, {1,1} is held
 * high by something stronger than the C6. Only for pins that are outputs in
 * this design; driving a peripheral's output fights it. */
pinprobe_drive_t pinprobe_drive_test(int gpio);
static inline bool pinprobe_drive_ok(pinprobe_drive_t d) { return d.hi == 1 && d.lo == 0; }
const char *pinprobe_drive_verdict(pinprobe_drive_t d);

#ifdef __cplusplus
}
#endif
