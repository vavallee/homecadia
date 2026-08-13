# Bill of materials — sensor-01

Quantities are per unit; build run is 3 units. Prices in CAD; blank = not yet
known, fill as paid. Network-side hardware (ZBT-2 border router, Voice PE) is
in [infrastructure.md](infrastructure.md); hardware checks that gate assembly
are in [bringup.md](bringup.md).

## Ordered 2026-08-08

| Part | Spec / SKU | Qty per unit | Vendor | Order status | Price paid (CAD) |
|---|---|---|---|---|---|
| Seeed XIAO ESP32-C6 | MCU, Thread radio, LiPo charge IC on underside pads | 1 | — | in hand (pre-existing) | |
| Seeed ePaper Driver Board for XIAO V2 | SKU 114993558; 24-pin FPC, SPI; battery JST + power switch (JST **not** used — see notes) | 1 | Seeed | ordered 2026-08-08 | |
| Seeed 2.9" mono ePaper 296×128 | SKU 104990853; SSD1680-class; partial refresh + panel deep sleep | 1 | Seeed | ordered 2026-08-08 | |
| Grove SHT40 | SKU 101021032; temp/RH, I2C addr 0x44; Grove cable cut and soldered | 1 | Seeed | ordered 2026-08-08 | |
| JST pigtails | SKU 321050009 | 1 set | Seeed | ordered 2026-08-08 | |
| EEMB 2000mAh 3.7V LiPo | LP103454; finished size **56 × 34.5 × 10.6mm** — needs the rev 2 case ([hardware/case](../hardware/case/README.md)); ⚠️ polarity — see warnings below | 1 | Amazon.ca | ordered 2026-08-08 | |

Per-line prices unknown; order totals: Seeed ~$125–155, batteries ~$50–60.

## Not yet ordered

| Part | Spec / SKU | Qty per unit | Vendor | Order status | Price paid (CAD) |
|---|---|---|---|---|---|
| Bourns PEC11R-4220F-S0024 | EC11 rotary encoder, 20mm flatted D-shaft, push switch, 24 detents | 1 | Digi-Key CA | not ordered | |
| 3mm LED | commissioning + low-battery indication only | 1 | Digi-Key CA | not ordered | |
| Resistor 1MΩ | battery divider (2× 1M, not 100k — see notes) | 2 | Digi-Key CA | not ordered | |
| Capacitor 100nF | divider ADC hold cap; **through-hole** — 0805 SMD is not usable for a perfboard hand-build | 1 | Digi-Key CA | not ordered | |
| USB-C female panel-mount pigtail | 2-wire chassis type; bezel must fit Ø12.8 hole; charging via USB **A-to-C only** (no CC resistors, C-to-C won't handshake) | 1 | Amazon.ca | not ordered | |
| M2 self-tapping screw kit | case assembly, ~4/unit | 1 kit total | Amazon.ca | not ordered | |
| Enclosure set | FDM PETG, veltoc wall-mount remix ([hardware/case](../hardware/case/README.md)); **order ONE test set first** | 1 set | JLC3DP | not ordered | |
| #6 pan-head screws + drywall anchors | keyhole wall mounts (head ≤ Ø8.5×2.4mm) | 2 | local | not ordered | |
| Command Small strips | rail landings on case back | 2 | local | not ordered | |
| Perfboard | divider carrier build | 1 | TBD | not ordered | |
| Hookup wire | 26–28AWG silicone stranded | — | TBD | not ordered | |
| Heat shrink | assorted; fallback if the divider carrier doesn't work out | — | TBD | not ordered | |

Digi-Key line (encoders + LEDs + resistors together): ~$21 landed for the run.

## ⚠️ Assembly warnings (repeated in [assembly.md](assembly.md))

- **EEMB LiPo polarity is frequently reversed** vs the Seeed/Adafruit
  convention. Multimeter-check every cell before connecting anything.
  Reversed polarity kills the XIAO.
- Battery solders to the **XIAO's underside battery pads** (its charge IC),
  NOT the ePaper driver board's JST — that path does not charge.
- **SHT40 sits in the case airflow path, external to MCU heat** —
  self-heating skews readings.

## Notes

- **Battery JST on the ePaper driver board is unused.** The battery solders to
  the XIAO's underside battery pads so the XIAO's charge IC manages it. The
  Seeed wiki claims the V2 driver board has its own charging IC, but veltoc's
  build (same board) found the JST path does not charge in this stack. Battery
  goes to the XIAO pads; the driver board's power switch is therefore also
  bypassed.
- **100k vs 1M divider:** a 100k+100k divider across a 4.2V battery bleeds
  ~21µA continuously — about 9% of the ≤300µA average power budget. 1M+1M
  bleeds ~2.1µA. The tradeoff is a high-impedance ADC source, handled with the
  100nF hold cap and a settling delay in firmware. See
  [power-budget.md](power-budget.md); divider lands on GPIO5/MTDI, not A0
  ([pinmap.md](pinmap.md)).
