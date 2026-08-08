# Pin map — sensor-01

XIAO ESP32-C6 on Seeed ePaper Driver Board for XIAO V2. This is the single
source of truth; `firmware/sensor-01/main/app_config.h` mirrors it and must be
changed together with this file.

Status: header usage derived from the Seeed wiki and veltoc; board-internal
facts below are **verified against the official schematic**
([XIAO-ESP32-C6_v1.0_SCH_PDF_24028.pdf](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/XIAO-ESP32-C6_v1.0_SCH_PDF_24028.pdf),
sheet numbers cited). Rows still open on hardware are in
[bringup.md](bringup.md).

## Schematic-verified board facts

- **No battery divider is populated on the board.** The only 200k on sheet
  3/5 is the charger's current-set resistor (`ICharge = 24000/200K = 120mA`
  on SGM40567's IREF); A0 routes straight from the header to GPIO0. The
  Seeed wiki's `analogReadMilliVolts(A0)` battery snippet is shared XIAO
  boilerplate and does not apply — any divider is user-added.
- **Underside pads exist for GPIO4–7**: TP4=MTMS/GPIO4, TP5=MTDI/GPIO5,
  TP6=MTCK/GPIO6, TP7=MTDO/GPIO7, plus BOOT(GPIO9), EN, 3V3, GND (sheet 3/5).
- **RF switch must be enabled by firmware** (sheet 4/5): FM8625H powered via
  P-FET Q3 whose gate (GPIO3) has a 10k pull-up — the switch is OFF at
  reset. Drive GPIO3 low to power it; GPIO14 (VCTL) low selects the onboard
  ceramic antenna (ANT1 KH5220-A36), high the U.FL (ANT2). Neither pin is on
  the header. Handled in `app_main.cpp:board_rf_switch_init()`.
- **Onboard yellow LED on GPIO15, active low** (R25 1.5k to 3V3, sheet 4/5).
  GPIO15 is a C6 strapping pin — leave it high-Z (LED off) and never hold it
  low into sleep.
- Sheet 4/5 carries Seeed's own note: **"Avoid using GPIO4, 5, 8, 9, 15"**
  (C6 strapping/JTAG set). Also: MTCK doubles as LP_I2C_SDA, MTDO as
  LP_I2C_SCL.
- **No UART bridge**: USB D+/D− wire directly to the C6's native USB
  (sheet 3/5). Console is USB Serial/JTAG
  (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`); it drops in deep sleep — normal.

### Strapping caveat on the battery-sense pin

GPIO5 (MTDI) is in Seeed's avoid list. A permanently-connected 2×1M divider
holds it at ~Vbat/2 ≈ 1.9V, inside the undefined logic band at strap
sampling. veltoc ships exactly this and reports working boots, but the exact
GPIO5 strap function on the C6 is **unverified** (check the ESP32-C6
datasheet §strapping before final assembly). Mitigations if it bites:
high-side-gate the divider (node rests at GND through the lower 1M at
reset), or move battery sense to A0/A1/A2 — possible only if the display
wiring frees one (the Seeed ePaper driver board takes all three).

| Function | XIAO pin | GPIO | Fixed by | Notes |
|---|---|---|---|---|
| ePaper RST | D0 | 0 | driver board routing | shares pad with A0 — see conflict note |
| ePaper CS | D1 | 1 | driver board routing | |
| ePaper BUSY | D2 | 2 | driver board routing | |
| ePaper DC | D3 | 21 | driver board routing | |
| SHT40 SDA | D4 | 22 | our wiring | Grove cable, I2C addr 0x44 |
| SHT40 SCL | D5 | 23 | our wiring | |
| LED | D6 | 16 | our wiring (veltoc) | |
| Encoder A | D7 | 17 | our wiring (veltoc) | no deep-sleep wake (not an LP pad) |
| ePaper SCK | D8 | 19 | driver board routing | |
| Encoder B | D9 | 20 | our wiring (veltoc) | |
| ePaper MOSI | D10 | 18 | driver board routing | |
| Encoder push | MTCK pad (underside) | 6 | our wiring (veltoc) | LP GPIO → deep-sleep wake source |
| Battery divider | **MTDI pad (underside)** | **5** | our wiring (veltoc) | ADC1_CH5; 2×1M + 100nF |
| Battery + / − | battery pads (underside) | — | XIAO charge IC | ⚠️ polarity check first — [assembly.md](assembly.md) |

## Resolved conflict: battery ADC is NOT on A0

On the XIAO ESP32-C6, **A0 and D0 are the same pin (GPIO0)**, and the ePaper
driver board uses D0 as the panel reset line. TinyENV reads battery on A0, but
TinyENV has no display. veltoc (same driver board as us) moved battery sense to
**A5 = MTDI = GPIO5**, an underside pad with ADC capability (ADC1_CH5). We do
the same. Sharing GPIO0 between panel RST and the divider was rejected: while
the pin is driven as RST the ADC would read the drive level, and leaving it
floating at the divider's mid-rail voltage (~1.9V) puts the panel's reset input
in an undefined logic region while the panel is supposed to be sleeping.

## HW-VERIFY (tracked in bringup.md)

- D-pin → GPIO numbers against the official XIAO ESP32-C6 pinout diagram.
- ePaper pin usage against the assembled driver board (wiki table above is
  V2-specific; probe BUSY/RST if the display misbehaves).
- GPIO6 (MTCK) wakes from deep sleep with the encoder switch wiring.
- ADC reading on GPIO5 with the 2×1M divider: settling behavior and calibration.
