#pragma once

// Pin map for sensor-01 (XIAO ESP32-C6 + Seeed ePaper Driver Board V2).
// Mirrors docs/pinmap.md — change both together.
// HW-VERIFY: all values unverified on hardware; tracked in docs/bringup.md.

// e-paper, fixed by driver board routing (Seeed wiki, ePaper Driver Board V2)
#define EPD_PIN_RST        0    // D0 — also the A0 pad; battery ADC must not use it
#define EPD_PIN_CS         1    // D1
#define EPD_PIN_BUSY       2    // D2
#define EPD_PIN_DC         21   // D3
#define EPD_PIN_SCK        19   // D8
#define EPD_PIN_MOSI       18   // D10

// SHT40 on I2C (Grove cable soldered to D4/D5)
#define SHT40_I2C_SDA      22   // D4
#define SHT40_I2C_SCL      23   // D5
#define SHT40_I2C_ADDR     0x44

// EC11 encoder
#define ENC_PIN_A          17   // D7 — no deep-sleep wake (not an LP pad)
#define ENC_PIN_B          20   // D9
#define ENC_PIN_SW         6    // MTCK underside pad; LP GPIO, deep-sleep wake source

// LED (commissioning state + low battery only)
#define LED_PIN            16   // D6

// Battery divider 2x1M + 100nF on MTDI underside pad (NOT A0 — see docs/pinmap.md;
// schematic-verified: no divider is populated on the board). GPIO5 is in Seeed's
// "avoid" list (strapping/JTAG) — see the strapping caveat in docs/pinmap.md.
#define VBAT_ADC_GPIO      5    // MTDI, ADC1_CH5
#define VBAT_ADC_SETTLE_MS 5    // high-impedance source: delay after ADC config before read

// XIAO ESP32-C6 board-internal pins (schematic XIAO-ESP32-C6_v1.0_SCH_PDF_24028,
// sheet 4/5). Not on the header. The FM8625H RF switch is powered through
// P-FET Q3 (gate = GPIO3, 10k pull-up => OFF at reset): firmware MUST drive
// GPIO3 low or the radios have no antenna. GPIO14 = VCTL: low selects the
// onboard ceramic antenna (ANT1), high the U.FL (ANT2).
#define RF_SWITCH_POWER_GPIO 3   // drive low to power the RF switch
#define RF_ANT_SELECT_GPIO   14  // low = ceramic antenna
#define ONBOARD_LED_GPIO     15  // yellow LED via 1.5k to 3V3, ACTIVE LOW; C6 strapping
                                 // pin — leave high-Z (LED off), never hold low into sleep

// Measurement / reporting policy (defaults; user-configurable from milestone 4)
#define SENSOR_POLL_INTERVAL_S 120
#define REPORT_DELTA_TEMP_C    0.2f
#define REPORT_DELTA_RH_PCT    1.0f
#define FORCE_REPORT_EVERY_N_POLLS 10  // report even without delta every N polls (staleness guard)

#define LOW_BATTERY_PCT 10             // below this: LED pulse + display warning

// Display refresh policy (every refresh costs battery; see docs/power-budget.md)
#define DISPLAY_FULL_REFRESH_EVERY_N 10  // full refresh every N-th refresh to clear ghosting
#define DISPLAY_FLIP_LONG_AXIS  1        // verified on hardware: upright with the FPC at the bottom
#define DISPLAY_FLIP_SHORT_AXIS 0        // verified on hardware
#define DISPLAY_IDLE_TIMEOUT_S 30
#define FACTORY_RESET_HOLD_S   10
