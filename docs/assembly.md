# Assembly — sensor-01

The repeatable build procedure. It is written from four XIAOs: #1 died to a
source-meter injection, #2 and #3 lost underside pads, #4 is in service as
node 26 (2026-09-28). Every gate below exists because skipping it cost a board
or a day. Wiring targets are defined in [pinmap.md](pinmap.md); photos come in
milestone 6.

**Soldering is where every fault in this build was introduced, and none of
them was visible.** The procedure is therefore: solder the smallest possible
set of joints, prove each one by measurement while it is still reworkable,
then fix it in place and never touch it again.

## Rules

1. **Check the meter before the board.** Touch the probes together on beep,
   then read a known voltage (the XIAO's 5V pin on USB reads ~5 V). A probe
   lead loose in its jack reads 0 V and "no beep" on everything; it produced a
   full session of wrong conclusions about good wiring
   ([field-notes.md](field-notes.md) §25).
2. **Four underside joints, no more:** BAT+, BAT−, MTMS, MTCK. Everything else
   goes on header pins.
3. **The silkscreen labels sit beside the pads, and 3V3 is next to MTCK.**
   Which pad a wire is on is decided by the tests, not by looking.
4. **Test before glue.** Hot glue is the strain relief the pads need, and it is
   one-way: taking it off to rework XIAO #3 took the pads with it.
5. **A wire is a lever.** Use 30 AWG silicone or wire-wrap wire on the pads.
   Stiff hookup wire tore pads on two boards with no heat involved.
6. **Electronics flux only** (no-clean pen, e.g. Kester 951). Plumbing flux
   conducts ([field-notes.md](field-notes.md) §17).
7. **USB and a battery source never feed BAT+ together.** Lift the PPK2 VOUT
   lead or unplug the cell before USB goes in.
8. **One change, then one measurement.**

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
4. A protected cell that reads 0 V is usually latched, not dead: probe the
   metal contact (not the housing) and test by whether it accepts charge.
   Causes and the test order are in [battery-runbook.md](battery-runbook.md).

## Wiring, as built on node 26

| Signal | XIAO connection | GPIO | Joint |
|---|---|---|---|
| Battery + | BAT+ pad | — | underside |
| Battery − | BAT− pad | — | underside |
| Battery divider node | MTMS pad | 4 | underside |
| Encoder A | MTCK pad | 6 | underside |
| Encoder B | D6 | 16 | header |
| Encoder common (middle leg) | GND | — | header |
| Push switch | D9 and GND | 20 | header |
| LED + 330 Ω | D7 | 17 | header |
| SHT40 SDA / SCL | D4 / D5 | 22 / 23 | header |
| ePaper RST, CS, BUSY, DC, SCK, MOSI | D0, D1, D2, D3, D8, D10 | 0, 1, 2, 21, 19, 18 | driver board |

Battery divider: 1 MΩ from BAT+ to the node, 1 MΩ from the node to GND, 100 nF
from the node to GND, MTMS wire to the node
([diagrams/battery-divider.html](diagrams/battery-divider.html)). Not A0: A0 is
the ePaper reset line. Encoder A is on MTCK because only LP GPIOs 0–7 can wake
light sleep and the header's three (D0–D2) belong to the display. The push
switch cannot wake the chip: turn first, then press within 2 s.

## Stage 0 — bare XIAO (5 min, no soldering)

Proves the board before anything is attached, so a later fault cannot be the
XIAO as delivered.

1. Plug the bare XIAO into USB. Windows must list a port with
   `VID:PID=303A:1001`.
2. Flash the bench image (all four segments, offsets in
   [build.md](build.md)).
3. Unplug and replug USB (a serial-line reset leaves the console silent), then
   capture 30 s. **Pass:** the harness prints
   `coupling scan: no pin follows any other` and all six outputs
   `follows the driver`.

## Stage 1 — solder the four underside wires

- Leaded solder 300–320 °C, lead-free 340–350 °C (general guidance, not
  measured on this board). Pads lift from time on the pad and from pull on the
  wire more than from temperature.
- Pre-tin the wire. Tin the pad with one touch. Join with a touch of about 1 s.
- Trim stray strands at the BAT+ and BAT− joints; on XIAO #4 strands fanned
  toward the neighbouring pad.
- Lay the board flat and do not move the wires until Stage 2 is done.

## Stage 2 — unpowered tests (5 min, before glue)

USB out, no battery, meter on beep, board flat. Rule 1 first. "Wire" means the
free end of the wire, so the joint is inside the measurement.

| # | Probe A | Probe B | Pass | A fail means |
|---|---|---|---|---|
| 1 | MTCK wire | 3V3, 5V, GND header pins in turn | no beep ×3 | wire is on a power or ground pad |
| 2 | MTMS wire | 3V3, 5V, GND header pins in turn | no beep ×3 | same |
| 3 | BAT+ wire | BAT− wire | no beep | strand or solder bridge across the battery |
| 4 | BAT− wire | GND header pin | beep | BAT− joint open |
| 5 | MTCK wire | GND pin, **B button held**, then **R button held** | no beep ×2 | wire is on BOOT or EN |
| 6 | MTMS wire | GND pin, B held, then R held | no beep ×2 | same |
| 7 | MTCK wire | MTMS wire | no beep | bridge between the two signal pads |

Tests 1–4 exclude the pads that cause damage. Tests 5–6 exclude the two pads
that cannot be fixed in firmware: the XIAO's buttons short BOOT and EN to
ground, which makes those pads identifiable without power. After all seven,
the only mistakes left are mix-ups among MTMS, MTDI, MTCK and MTDO, and those
are a pin number in `app_config.h`.

**All pass: glue.** Hot glue over the four joints and again over the wires a
centimetre away, so a pull lands on glue. Any fail: rework now, while it costs
one joint and not a pad.

## Stage 3 — powered tests on USB, bench image (10 min)

Attach the header-side parts (encoder B and common, push switch, LED, SHT40,
display) and join the encoder A lead to the MTCK wire. No battery.

| # | Action | Pass, from the console | Proves |
|---|---|---|---|
| 1 | boot, wait 10 s | `sensor_loop: reported … bat NN% (~4000–4100mV)` | MTMS wire is on GPIO4 and BAT+ is wired: with no cell the divider reads the charger output |
| 2 | same boot | `harness: … device at 0x44`, six outputs `follows the driver` | sensor and display wiring |
| 3 | turn 3 detents clockwise, 3 back | three `on_rotate dir=1`, three `dir=-1`, `encoder raw` lines changing on both A and B | MTCK wire is on GPIO6; encoder B and common |
| 4 | press 3 times | `on_push` per press | switch wiring |
| 5 | through 3 and 4 | the COM port stays present | no contact shorts a rail |

Start the capture before acting and read it after. A port that vanishes when
the knob turns is a rail being shorted through the encoder: stop and return to
Stage 2.

## Stage 4 — battery-side tests, shipping image (15 min)

1. Flash the shipping image. Unplug USB.
2. PPK2 in Source Meter mode at 3700 mV, VOUT to BAT+, GND to GND, then enable
   output.
3. **Unpaired, expect about 28 mA average** with a 20–40 mA baseline while the
   15-minute commissioning window is open (measured 2026-09-28), then
   **~20 µA** about 30 s after it closes: the unit deep-sleeps until a detent
   starts a new window (field-notes.md §27). Do not leave an unpaired unit on
   a cell with the window open.
4. Pair it ([commissioning.md](commissioning.md)). Check the border router is
   up first.
5. Wait 3 minutes, knob untouched. **Pass:** a 10 s window averages on the
   order of 100 µA with a flat baseline (91 µA measured on node 26), and the
   controller shows a battery voltage within 1 % of the source.
6. Turn one detent. **Pass:** the screen changes view and the trace shows one
   burst of about 2 s at ~40 mA, then the flat baseline again.

Reference values: [power-budget.md](power-budget.md).

## Stage 5 — driver board, display and case

1. Solder 2×7 female headers to the driver board (it ships with bare holes,
   see below). Wash, dry, and run one bench boot: the coupling scan must pass
   before anything else is attached.
2. Cut the Grove cable and solder the SHT40: SDA → D4, SCL → D5, VCC → 3V3,
   GND → GND. Position the sensor **in the case airflow path, away from MCU
   heat**; self-heating skews readings.
3. Encoder B, push switch and LED solder to the driver board's D6, D9 and D7
   through-holes. The encoder needs soldered leads: its blades seat in neither
   breadboard springs nor Dupont sockets.
4. USB-C panel pigtail → XIAO USB port (mechanical detail pending the case
   redesign). **Charging only works with a USB A-to-C cable**: the 2-wire
   pigtail has no CC resistors, so C-to-C supplies won't enable VBUS.
5. Multimeter polarity check (above), then the battery to the BAT wires.
6. Seat the XIAO on the driver board, **USB-C end pointing away from the FPC
   connector**. The board's RST/5V silkscreen marks the XIAO's USB end, at the
   opposite end of the board from the ribbon. Seated 180° out, nothing works
   and nothing is obviously wrong to look at.
7. Connect the 24-pin FPC (orientation rule below), repeat Stage 3 tests 1–5,
   then fit into the case.

**Case rev 5** ([hardware/case](../hardware/case/README.md)) is built for this
stack, floor upward: the cell flat on the floor, the tray on the cell, the
XIAO in its sockets on the driver board, the panel against the front. Two
things differ from the breadboard build:

- **Trim the pins under the driver board to ~1.5 mm** once the encoder, LED,
  switch and SHT40 are soldered to it. As built for the breadboard they stand
  9.0 mm proud, and rev 5's depth assumes they are gone; after trimming the
  board no longer plugs into a breadboard.
- The cell's leads (~29 mm) and the USB-C panel connector's long leads coil
  in the free bay at one end. Foam tape holds the cell to the floor and the
  tray to the cell (the case has no cradle).

Rev 5 has not been printed; the first set is the test fit
([bringup.md](bringup.md), Case & mechanical).

## Symptom → cause, from this build

| Symptom | Cause found |
|---|---|
| 0 V on every pin, no beep on known-good wires | meter probe lead loose in its jack |
| Board drops off USB the moment the knob turns; encoder A never changes in the log | encoder wire on the 3V3 pad beside MTCK; each detent shorts the rail |
| Self-test reads encoder A `held HIGH` | consistent with the wire on 3V3; not recorded on a correct MTCK wire, so not a verdict |
| Press never registers, rotation fine | wire loose in the D9 row |
| One encoder line dead, which one changes between runs | bare encoder blades in a breadboard |
| A pin at 0 V under power that reads open unpowered | conductive flux residue |
| Battery reads the same whatever the source voltage | the source setting did not apply (meter the source first; retro.md T2), or the divider top on the 3V3 rail, not BAT+ |
| Cell reads 0 V | protection latched, or the probe is on the housing |
| Console silent after a flash, port present | board left in download mode or wedged by the reset pulse: replug USB |
| 28 mA average on battery | unit is unpaired with its window open |
| Paired floor well above ~57 µA, polls above ~0.5 mC, 1 ms wakes with no radio | a non-wake pin with a level interrupt isolated in sleep (field-notes.md §28); bisect the firmware on the same board |
| Healthy cell, unit resets continuously on it, LED flickers faintly | resistance in the battery path: [battery-runbook.md](battery-runbook.md) B3 |

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

**The driver board expects the panel's copper contacts facing UP**, away from
the board. Confirmed on the bench twice: 2026-08-22 (first successful refresh)
and 2026-08-25 (recovery from a misseated ribbon). Get this wrong and the
symptoms do not say "wrong orientation" — see [field-notes.md](field-notes.md)
section 15, where a misseated ribbon put a panel supply rail onto MOSI and
presented as a dead panel and then as a firmware bug.

Still use the insertion-force rule as the working check: it holds whichever way
you happen to be holding the board, and "up" does not.

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
| **D6** | D6 | **encoder B** |
| **D7** | D7 | **LED** |
| **D9** | D9 | **encoder push switch** |

So once female headers are fitted, the SHT40, LED, encoder B and push switch
all solder to the **driver board's** through-holes rather than to the XIAO —
easier joints on bigger pads. Four connections need the XIAO's underside pads:
encoder A (MTCK/GPIO6), battery divider node (MTMS/GPIO4), and BAT+/BAT−.

**Settle before soldering:** which face the XIAO seats on. Dry-fit both ways
and pick the one that leaves the USB port accessible and clears the FPC
connector. Unverified from photos; desoldering 14 pins is the job to avoid.

## ⚠️ The driver board ships with bare holes

**Found on hardware 2026-08-17.** The Seeed ePaper Driver Board V2 arrives
with **unpopulated through-holes** — no female headers fitted. The XIAO
cannot seat on it until 2×7 female header strips are soldered in.

This cost most of a bench session. A jumper wire pushed into a bare plated
hole makes contact only by luck, and the resulting intermittent connections
imitate real faults convincingly: BUSY reading as a floating line (panel
appears dead), the SHT40 dropping off the I2C bus between consecutive
resets, and two USB brownouts severe enough that Windows reported
`Set Address Failed` / `Device Descriptor Request Failed`.

- Solder the headers **before** any further bring-up. Required for the final
  build regardless.
- **Electronics flux only** — rosin-core wire or a no-clean (J-STD-004) pen.
  Plumbing paste flux (ASTM B-813, e.g. AIM Nitro) left ionic residue on
  driver boards #1 and #2 that conducted kΩ between adjacent pins under bias
  and cost days ([field-notes.md](field-notes.md) section 17).
- **After soldering: two-stage clean, dry, scan.** IPA flood drained off the
  edge, then hot soapy water + distilled rinse, dry hard (bores hold water).
  Then one bench-profile boot: the harness scan must print
  `coupling scan: no pin follows any other` before anything else is attached.
- **Confirm which face the XIAO seats on before soldering** — desoldering 14
  pins is the worst job in this build.
- Preferred bench topology afterwards is the final-build one: XIAO seated
  directly in the driver board (no jumpers at all), SHT40 on the board's
  D4/D5 breakout holes.

## HW-VERIFY

- ~~Confirm BAT+/BAT− pad markings on this XIAO revision before soldering.~~
  **Closed 2026-09-28** — node 26 charges through them and reports the source
  voltage within 0.5 %.
- ~~Confirm FPC contact orientation for this panel batch.~~ **Closed
  2026-08-22** — resolved by the insertion-force rule above; panel refreshed
  full and partial on driver board #1.
