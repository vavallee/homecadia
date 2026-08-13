# homecadia sensor-01 harness board — netlist

Board `hb-01`, 48 x 40 mm, 2 layer, 1.6 mm FR4.
Generated 2026-08-13. **Never opened in KiCad — see the warning in README.md.**

## Components

| Ref | Value | Footprint | LCSC | Note |
|---|---|---|---|---|
| J1 | USB-C 16P | `Connector_USB:USB_C_Receptacle_XKB_U262-16XN-4BVC11` | [C2765186](https://www.lcsc.com/product-detail/C2765186.html) | power only; D+/D- to test pads |
| R1 | 5.1k | `Resistor_SMD:R_0805_2012Metric` | — | CC1 pulldown |
| R2 | 5.1k | `Resistor_SMD:R_0805_2012Metric` | — | CC2 pulldown |
| D1 | SP0503BAHTG | `Package_TO_SOT_SMD:SOT-143` | [C3040626](https://www.lcsc.com/product-detail/C3040626.html) | ESD array on VBUS/D+/D- |
| R3 | 1M 1% | `Resistor_SMD:R_0805_2012Metric` | — | divider high side |
| R4 | 1M 1% | `Resistor_SMD:R_0805_2012Metric` | — | divider low side |
| C1 | 100nF | `Capacitor_SMD:C_0805_2012Metric` | — | ADC hold cap |
| R5 | 1k | `Resistor_SMD:R_0805_2012Metric` | — | LED series |
| D2 | LED 3mm | `LED_THT:LED_D3.0mm` | — | or 2-pin header to panel LED |
| SW1 | PEC11R-4220F-S0024 | `Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm` | — | VERIFY footprint against Bourns PEC11R drawing |
| J2 | XIAO field 2x7 | `Connector_PinHeader_2.54mm:PinHeader_2x07_P2.54mm_Vertical` | — | labelled field to XIAO / driver-board breakout |
| J3 | BAT JST-PH | `Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical` | [C131337](https://www.lcsc.com/product-detail/C131337.html) | polarity is NOT guaranteed by the key |
| J4 | SHT40 JST-SH | `Connector_JST:JST_SH_BM04B-SRSS-TB_1x04-1MP_P1.00mm_Vertical` | — | to sb-01 satellite |
| J5 | 5V out | `Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical` | — | VBUS/GND to XIAO 5V pin |
| TP1 | VBAT | `TestPoint:TestPoint_Pad_D1.5mm` | — |  |
| TP2 | VDIV | `TestPoint:TestPoint_Pad_D1.5mm` | — |  |
| TP3 | GND | `TestPoint:TestPoint_Pad_D1.5mm` | — |  |
| TP4 | 3V3 | `TestPoint:TestPoint_Pad_D1.5mm` | — |  |
| H1 | M2 | `MountingHole:MountingHole_2.2mm_M2` | — |  |
| H2 | M2 | `MountingHole:MountingHole_2.2mm_M2` | — |  |
| H3 | M2 | `MountingHole:MountingHole_2.2mm_M2` | — |  |
| H4 | M2 | `MountingHole:MountingHole_2.2mm_M2` | — |  |

## Nets

| Net | Nodes |
|---|---|
| `+3V3` | J2.12, J4.1, TP4.1 |
| `CC1` | J1.A5, R1.1 |
| `CC2` | J1.B5, R2.1 |
| `ENC_A` | J2.8, SW1.A |
| `ENC_B` | J2.10, SW1.B |
| `ENC_SW` | J2.14, SW1.S1 |
| `GND` | J1.A1, J1.B1, J1.A12, J1.B12, R1.2, R2.2, D1.2, R4.2, C1.2, D2.1, SW1.C, SW1.S2, J2.1, J3.2, J4.4, J5.2, TP3.1 |
| `LED_A` | J2.7, R5.1 |
| `LED_K` | R5.2, D2.2 |
| `SCL` | J2.6, J4.2 |
| `SDA` | J2.5, J4.3 |
| `VBAT` | J3.1, R3.1, TP1.1 |
| `VBUS` | J1.A4, J1.B4, J1.A9, J1.B9, D1.3, J5.1 |
| `VDIV` | R3.2, R4.1, C1.1, J2.13, TP2.1 |
