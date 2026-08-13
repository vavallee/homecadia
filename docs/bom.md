# Bill of materials — sensor-01

Quantities are per unit; build run is 3 units. Prices in CAD; blank = not yet
known, fill as paid. Network-side hardware (ZBT-2 border router, Voice PE) is
in [infrastructure.md](infrastructure.md); hardware checks that gate assembly
are in [bringup.md](bringup.md).

## Ordered and confirmed (reconciled against vendor order pages, 2026-08-13)

| Part | Spec / SKU | Qty per unit | Vendor | Order status | Price paid |
|---|---|---|---|---|---|
| Seeed XIAO ESP32-C6 | MCU, Thread radio, LiPo charge IC on underside pads | 1 | — | **3 in hand, confirmed 2026-08-13** | — |
| Seeed ePaper Driver Board for XIAO V2 | SKU 114993558; 24-pin FPC, SPI; battery JST + power switch (JST **not** used — see notes) | 1 | Seeed | shipped 3/3; at port of destination 2026-08-13 | US$5.90 ea / 17.70 |
| Seeed 2.9" mono ePaper 296×128 | SKU 104990853; SSD1680-class; partial refresh + panel deep sleep | 1 | Seeed | shipped 3/3 | US$9.95 ea / 29.85 |
| Grove SHT40 | SKU 101021032; ±0.2 °C, ±1.8 %RH typ (max ±3.5 %); I2C 0x44; Grove cable cut and soldered | 1 | Seeed | shipped 3/3 | US$5.50 ea / 16.50 |
| JST 2-pin power connector | SKU 321050009 | 1 | Seeed | shipped 20/20 | US$0.05 ea / 1.00 |
| EEMB 2000mAh 3.7V LiPo | LP103454; finished size **56 × 34.5 × 10.6mm** — drove the rev 2 case stretch ([hardware/case](../hardware/case/README.md)); JST 2.0mm lead, cut it off; ⚠️ polarity | 1 | Amazon.ca | **delivered 2026-08-12**, qty 3 | |
| Perfboard | Chanzon 34pcs double-sided FR4, 5 sizes | 1 | Amazon.ca | arriving 19–28 Aug | 22.79 (kit) |
| Capacitor 100nF | Chanzon 50pcs, 104M disc ceramic, 1000V, through-hole. Voltage rating is overkill but harmless; ±20% is fine for an ADC hold cap | 1 | Amazon.ca | **delivered 2026-08-11** | 29.05 (kit, with resistors) |
| Resistor 1MΩ | ALLECIN 1/4W 1% metal film, 25 values 1Ω–1MΩ — 1MΩ is the top value | 2 | Amazon.ca | **delivered 2026-08-11** | (same order as above) |
| USB-C female panel-mount pigtail | Gebildet 10pcs, 2-pin 24AWG, 3A, waterproof. **M11×2.3 nut, needs a Ø12mm hole — case hole is Ø12.8mm, so it fits** with ~0.8mm slop. Charging via USB **A-to-C only** (no CC resistors) | 1 | Amazon.ca | arriving 2026-08-13 | 14.99 (10pcs) |
| 3mm LED | Chanzon 60pcs assortment, diffused, 3V 20mA; needs a series resistor from the kit above | 1 | Amazon.ca | arriving 19–28 Aug | 9.11 (kit) |
| M2 self-tapping screw kit | 800pcs stainless, cross-drive pan head | ~4 | Amazon.ca | **delivered 2026-08-12** | 23.44 (kit) |
| Bourns PEC11R-4220F-S0024 | EC11 rotary encoder, 20mm flatted D-shaft, push switch, 24 detents; DK part PEC11R-4220F-S0024-ND | 1 | Digi-Key CA | ordered qty 3, **3 in stock, 0 backordered**; DDP Timberlea NS | 4.27 ea / 12.81 (+15.00 ship +3.89 HST = **31.70**) |

Seeed line total **US$65.05** (4 lines above, shipping not shown on the order
page). Amazon.ca lines visible above total **CAD 99.38**, plus the battery
order (price not captured). The earlier CAD 125–155 estimate for Seeed was high.

## Still to order

| Part | Spec / SKU | Qty per unit | Vendor | Order status | Price paid (CAD) |
|---|---|---|---|---|---|
| Enclosure set | **MJF Nylon PA12**, 4 parts, 30.12 cm³/set — FDM is ruled out by JLC3DP's 30×30×10mm minimum ([hardware/case](../hardware/case/README.md)). **Order ONE test set first** | 1 set | JLC3DP | quoting | |
| #6 pan-head screws + drywall anchors | keyhole wall mounts, **92mm centres** (rev 2), head ≤ Ø8.5×2.4mm | 2 | local | not ordered | |
| Command Small strips | rail landings on case back | 2 | local | not ordered | |
| Hookup wire | 26–28AWG silicone stranded | — | local / Amazon.ca | not ordered | |
| Heat shrink | assorted; fallback if the perfboard divider carrier doesn't work out | — | local / Amazon.ca | not ordered | |

The LEDs and resistors were bought from Amazon.ca rather than Digi-Key, so the
encoder is the sole Digi-Key line and carries the whole $15 freight + HST.

**Every electronic part in this build is now ordered or in hand.** What is left
is the enclosure order and four local consumables.

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
