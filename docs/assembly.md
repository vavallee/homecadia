# Assembly — sensor-01

Draft; finalized in milestone 6 with photos. Wiring targets are defined in
[pinmap.md](pinmap.md).

## ⚠️ Battery polarity — read before touching the battery

**Check polarity with a multimeter before connecting anything.** The EEMB
LiPo's connector may be wired **reversed** relative to the Seeed convention.
Reversed polarity destroys the XIAO's charge circuit instantly.

1. Multimeter on DC volts, red probe to the battery lead you believe is +.
2. Expect **+3.5 to +4.2V**. A negative reading means the leads are swapped —
   re-pin the connector or swap the wires at the solder joint.
3. The battery solders to the **XIAO's underside battery pads** (marked BAT+ /
   BAT−), not to the ePaper driver board's JST. The JST path does not charge
   in this stack (veltoc, confirmed in their build; the wiki's charging-IC
   claim did not hold).

## Order of operations (draft)

1. Solder the battery divider: 1MΩ from BAT+ to the **MTDI underside pad**
   (GPIO5), 1MΩ from that pad to GND, 100nF from the pad to GND. Not A0 —
   A0 is the ePaper reset line ([pinmap.md](pinmap.md)).
2. Solder encoder: A → D7, B → D9, common → GND; push switch → MTCK underside
   pad (GPIO6) and GND.
3. Solder LED (with series resistor) → D6.
4. Cut Grove cable, solder SHT40: SDA → D4, SCL → D5, VCC → 3V3, GND → GND.
   Position the sensor **in the case airflow path, away from MCU heat** —
   self-heating skews readings.
5. USB-C panel pigtail → XIAO USB pads or port (mechanical detail TBD with the
   case). **Charging only works with a USB A-to-C cable** — the 2-wire pigtail
   has no CC resistors, so C-to-C supplies won't enable VBUS.
6. Multimeter polarity check (above), then solder battery to XIAO pads.
7. Seat XIAO on driver board, connect 24-pin FPC (contacts down, lock the
   latch), fit into case, M2 self-tappers.

## HW-VERIFY

- Confirm BAT+/BAT− pad markings on this XIAO revision before soldering.
- Confirm FPC contact orientation for this panel batch.
