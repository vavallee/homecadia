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
   Two ways to physically carry it: a small perfboard offcut (tidier, needs
   somewhere in the cavity to live — the case has no mount for it), or the
   three parts soldered inline and heat-shrunk. Both are in the BOM; decide at
   test fit.
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
7. Seat XIAO on driver board — **USB-C end pointing away from the FPC
   connector**. The board's RST/5V silkscreen marks the XIAO's USB end, at the
   opposite end of the board from the ribbon. Seated 180° out, nothing works
   and nothing is obviously wrong to look at.
8. Connect the 24-pin FPC — see the orientation rule below — then fit into
   case, M2 self-tappers.

### FPC orientation: go by insertion force, not by which way the copper faces

**The correct orientation slides in with light finger pressure. The reversed
one needs force.** That is the reliable test; "contacts down" is ambiguous
depending on how you are holding the board, and getting it wrong is not a
no-op. Reversing the ribbon mirrors the pin order — tab *n* meets panel pin
25−*n* — which lands the panel's VDDIO on RST and VCI on BUSY. The MCU is then
driving a supply rail: pulling RST low shorts it, and the board browns out.
Two panels died during bring-up (2026-08-18/22).

Procedure: flip the black latch up, slide the ribbon in until the **stiffener
is inside the housing** (not merely "as far as it will go" — a ribbon stopped
short still latches and makes intermittent contact), press the latch closed.
Insert and remove only with the power off.

On this batch the working orientation puts the copper contacts facing **up**,
away from the driver board — observed on the bench, not vendor-confirmed, so
use the insertion-force rule as the check.

**Handle the bare flex as an ESD-sensitive part.** One panel failed after a
successful refresh with nothing electrically suspicious in between; ESD from
handling is the leading guess but is unproven. Ground yourself before touching
the ribbon, and hold the panel by the glass edges.

## The driver board carries every pin this build needs

**Confirmed from the board's silkscreen 2026-08-17/18.** The V2 driver board
is a straight carrier for the XIAO: two 7-pin columns matching the XIAO's 14
pins one-for-one. It renames only the six display signals and passes the rest
through under their own names:

| Driver board label | XIAO pin | Used for |
|---|---|---|
| RST / CS / BUSY / DC | D0 / D1 / D2 / D3 | ePaper control |
| MOSI / SCK | D10 / D8 | ePaper SPI |
| 5V / GND / 3V3 | same | power |
| **D4 / D5** | D4 / D5 | **SHT40 I2C** |
| **D6** | D6 | **LED** |
| **D7 / D9** | D7 / D9 | **encoder A / B** |

So once female headers are fitted, the SHT40, LED and encoder all solder to
the **driver board's** through-holes rather than to the XIAO — easier joints
on bigger pads. Only three connections still need the XIAO's underside pads:
encoder push switch (MTCK/GPIO6), battery divider node (MTDI/GPIO5), and
BAT+/BAT−.

**Settle before soldering:** which face the XIAO seats on. Dry-fit both ways
and pick the one that leaves the USB port accessible and clears the FPC
connector. Unverified from photos; desoldering 14 pins is the job to avoid.

## ⚠️ The driver board ships with bare holes

**Found on hardware 2026-08-17.** The Seeed ePaper Driver Board V2 arrives
with **unpopulated through-holes** — no female headers fitted. Step 7 above
was written assuming the XIAO could just seat on it; it can't until 2×7
female header strips are soldered in.

This cost most of a bench session. A jumper wire pushed into a bare plated
hole makes contact only by luck, and the resulting intermittent connections
imitate real faults convincingly: BUSY reading as a floating line (panel
appears dead), the SHT40 dropping off the I2C bus between consecutive
resets, and two USB brownouts severe enough that Windows reported
`Set Address Failed` / `Device Descriptor Request Failed`.

- Solder the headers **before** any further bring-up. Required for the final
  build regardless.
- **Confirm which face the XIAO seats on before soldering** — desoldering 14
  pins is the worst job in this build.
- Preferred bench topology afterwards is the final-build one: XIAO seated
  directly in the driver board (no jumpers at all), SHT40 on the board's
  D4/D5 breakout holes.

## HW-VERIFY

- Confirm BAT+/BAT− pad markings on this XIAO revision before soldering.
- ~~Confirm FPC contact orientation for this panel batch.~~ **Closed
  2026-08-22** — resolved by the insertion-force rule above; panel refreshed
  full and partial on driver board #1.
