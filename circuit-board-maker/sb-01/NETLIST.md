# homecadia SHT40 satellite — netlist

Board `sb-01`, 14 x 16 mm, 2 layer, 1.6 mm FR4.
Generated 2026-08-13. **Never opened in KiCad — see the warning in README.md.**

## Components

| Ref | Value | Footprint | LCSC | Note |
|---|---|---|---|---|
| U1 | SHT40-AD1B-R2 | `Sensor_Humidity:Sensirion_DFN-4_1.5x1.5mm_P0.8mm_SHT4x_NoCentralPad` | [C2909890](https://www.lcsc.com/product-detail/C2909890.html) | DFN-4 no central pad; needs reflow or hot air |
| C1 | 100nF | `Capacitor_SMD:C_0805_2012Metric` | — | decoupling |
| J1 | JST-SH 4P | `Connector_JST:JST_SH_BM04B-SRSS-TB_1x04-1MP_P1.00mm_Vertical` | — | to hb-01 J4 |
| H1 | M2 | `MountingHole:MountingHole_2.2mm_M2` | — |  |

## Nets

| Net | Nodes |
|---|---|
| `+3V3` | U1.1, C1.1, J1.1 |
| `GND` | U1.3, C1.2, J1.4 |
| `SCL` | U1.2, J1.2 |
| `SDA` | U1.4, J1.3 |
