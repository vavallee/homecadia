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

// Battery divider 2x1M + 100nF on MTDI underside pad (NOT A0 — see docs/pinmap.md)
#define VBAT_ADC_GPIO      5    // MTDI, ADC1_CH5
#define VBAT_ADC_SETTLE_MS 5    // high-impedance source: delay after ADC config before read

// Measurement / reporting policy (defaults; user-configurable from milestone 4)
#define SENSOR_POLL_INTERVAL_S 120
#define REPORT_DELTA_TEMP_C    0.2f
#define REPORT_DELTA_RH_PCT    1.0f
#define DISPLAY_IDLE_TIMEOUT_S 30
#define FACTORY_RESET_HOLD_S   10
