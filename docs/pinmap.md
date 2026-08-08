# Pin map — sensor-01

XIAO ESP32-C6 on Seeed ePaper Driver Board for XIAO V2. This is the single
source of truth; `firmware/sensor-01/main/app_config.h` mirrors it and must be
changed together with this file.

Status: **derived from the Seeed wiki and veltoc, not yet verified on
hardware.** Every row is open in [bringup.md](bringup.md) until checked.

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
