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

### Strapping on the battery-sense pin — resolved, not a risk

GPIO5 (MTDI) is in Seeed's avoid list, and the permanently-connected 2×1M
divider holds it at ~Vbat/2 ≈ 1.9V — inside the undefined logic band
(V<sub>IL</sub> ≲ 0.8V, V<sub>IH</sub> ≳ 2.3V) while straps are sampled. So the
strap latches unpredictably.

**It does not matter.** Per the [ESP32-C6
datasheet](https://documentation.espressif.com/esp32-c6_datasheet_en.html)
Table 3-4, MTMS (GPIO4) and MTDI (GPIO5) strap **the SDIO sampling and driving
clock edge** — nothing else. Both float by default (no internal pulls at
reset), with a 3ms hold requirement after CHIP_PU rises (Table 3-2).
**sensor-01 uses no SDIO**: display is SPI, sensor is I2C, console is native
USB. Whichever way the strap lands, no code path reads the setting. veltoc's
working boots are explained, not lucky.

Consequence: **no gating MOSFET is needed**, and the divider's 2.1µA bleed
stays as budgeted (0.7% of the 300µA target — see
[power-budget.md](power-budget.md)).

Mitigations, retained only in case a future homecadia device on this pin does
use SDIO: high-side-gate the divider with a P-FET (node then rests at GND
through the lower 1M at reset, giving a clean logic 0 and near-zero bleed), or
move battery sense to A0/A1/A2 — possible only if the display wiring frees one
(the Seeed ePaper driver board takes all three).

One real cost remains: MTDI is JTAG TDI, so external JTAG through the underside
pads is unusable while the divider is attached. No practical loss — console and
debug both run over the C6's native USB Serial/JTAG.

| Function | XIAO pin | GPIO | Fixed by | Notes |
|---|---|---|---|---|
| ePaper RST | D0 | 0 | driver board routing | shares pad with A0 — see conflict note |
| ePaper CS | D1 | 1 | driver board routing | |
| ePaper BUSY | D2 | 2 | driver board routing | |
| ePaper DC | D3 | 21 | driver board routing | |
| SHT40 SDA | D4 | 22 | our wiring | Grove cable, I2C addr 0x44 |
| SHT40 SCL | D5 | 23 | our wiring | |
| LED | D7 | 17 | our wiring | swapped with encoder A 2026-08-25; see bringup.md Encoder & LED |
| Encoder A | D9 | 20 | our wiring | no deep-sleep wake (not an LP pad). A/B chosen so clockwise = view advances (bench 2026-08-26) |
| ePaper SCK | D8 | 19 | driver board routing | |
| Encoder B | D6 | 16 | our wiring | was D7, which reads 0V once SPI is up (field-notes s16); D6 neighbours only SCL |
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

## Driver board schematic

Seeed publishes the ePaper Driver Board for XIAO schematic at
<https://files.seeedstudio.com/wiki/xiao_075inch_epaper_panel/ePaper_Driver_Board.pdf>
(rev 1.0, 2024-12-09, one sheet). The wiki page for the board is
<https://wiki.seeedstudio.com/xiao_eink_expansion_board_v2/>. Note the older
"ePaper Breakout Board" page (`XIAO-eInk-Expansion-Board`) documents a
*different* board with BUSY on D5 — not this one.
