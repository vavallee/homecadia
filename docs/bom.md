# Bill of materials — sensor-01

Quantities are per unit; build run is 3 units. Fill in prices as paid.

| Part | Detail | Qty | Price paid |
|---|---|---|---|
| Seeed XIAO ESP32-C6 | MCU, Thread radio, LiPo charge IC on underside pads | 1 | |
| Seeed ePaper Driver Board for XIAO V2 | SKU 114993558; 24-pin FPC, SPI; battery JST + switch (JST **not** used — see note) | 1 | |
| Seeed 2.9" mono ePaper 296×128 | SKU 104990853; SSD1680-class; partial refresh + panel deep sleep | 1 | |
| Grove SHT40 | SKU 101021032; temp/RH, I2C addr 0x44; Grove cable cut and soldered | 1 | |
| EEMB 2000mAh 3.7V LiPo | ⚠️ polarity may be reversed vs Seeed convention — see [assembly.md](assembly.md) | 1 | |
| Bourns PEC11R-4220F-S0024 | EC11 rotary encoder, 20mm flatted D-shaft, push switch, 24 detents | 1 | |
| 3mm LED | commissioning + low-battery indication only | 1 | |
| Resistor 1MΩ | battery divider (2× 1M, not 100k — see [power-budget.md](power-budget.md)) | 2 | |
| Capacitor 100nF | battery divider ADC hold cap | 1 | |
| USB-C female panel-mount pigtail | 2-wire; charging works via USB **A-to-C cable only** (no CC resistors) | 1 | |
| M2 self-tapping screws | case assembly | ~4 | |

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
  100nF hold cap and a settling delay in firmware.
