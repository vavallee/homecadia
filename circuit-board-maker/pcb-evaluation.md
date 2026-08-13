# Custom PCB evaluation — sensor-01

Should the hand-wired sensor-01 build be replaced by a custom PCB?

**Conclusion: not for three units.** Build a through-hole harness board that
fixes three of the six pain points for ~CAD 40, hand-wire the three units, and
measure the assembled device before letting any power argument justify a board
spin. Reasoning below.

Written 2026-08-13, against the build state in [docs/bom.md](../docs/bom.md),
[docs/pinmap.md](../docs/pinmap.md), and
[docs/power-budget.md](../docs/power-budget.md).

## Provenance markers

Every number below carries its source, because several of them are the
difference between a good and a bad decision:

| Marker | Meaning |
|---|---|
| **[DS]** | Read directly from the linked datasheet in preparing this doc |
| **[SEARCH]** | From a search-result summary of a datasheet, not read first-hand |
| **[UNVERIFIED]** | Recall or estimate. Treat as a starting point, not a fact |

Every URL in this document was checked and returns HTTP 200 as of 2026-08-13.

---

## Three premises that were wrong

### 1. "Quiescent current is the number that decides the design." It isn't.

[docs/power-budget.md](../docs/power-budget.md) models a sleep floor of
15–40 µA, divider bleed of 2.1 µA, and Thread ICD poll contribution of
10–30 µA. That is **27–72 µA against a 300 µA budget** — 230 µA of headroom.

A MAX17048 fuel gauge draws 3 µA, or 1% of budget. An MCP73831 charger leaks
under 2 µA, or 0.7%. Ten such parts could be added before it matters.

The real risk is the two terms that have never been measured: the C6's actual
sleep floor with esp-matter LIT ICD running, and the ePaper refresh charge.
**Neither is affected by any PCB decision.** [docs/bringup.md](../docs/bringup.md)
already tracks both; the measured column is empty.

The quiescent-current analysis below is real, but it is arithmetic on a rounding
error. Measure first.

### 2. Pain point 5 (SHT40 mounting) — a PCB makes this worse

[docs/bom.md](../docs/bom.md) already records the constraint: *"SHT40 sits in
the case airflow path, external to MCU heat — self-heating skews readings."*
Putting the sensor on the main board seats it beside the C6 module, the LDO, and
a 20 V booster inductor. That is the opposite of the requirement.

Correct fix: a case revision with a vented sensor pocket, or a ~10 × 10 mm
satellite board on a 4-wire cable. Both are enclosure work. Neither needs a
main-board redesign.

### 3. Pain point 4 (driver-board JST doesn't charge) is already solved

Solved by soldering the cell to the XIAO's BAT pads. It costs nothing at
runtime. It is a documentation problem, not a hardware one, and it is not a PCB
justification.

### Scorecard on the six pain points

| # | Pain point | Verdict |
|---|---|---|
| 1 | Battery divider has no home in the enclosure | **Real, cheap to fix** |
| 2 | No fuel gauge, battery % is an ADC guess | Marginal — divider + OCV LUT is adequate for hourly reporting |
| 3 | USB-C pigtail has no CC resistors, A-to-C only | **Real, 20-cent fix, no PCB needed** |
| 4 | Driver-board JST doesn't charge | Already solved |
| 5 | SHT40 hangs on a pigtail | Not a PCB problem — enclosure work |
| 6 | ~12 wires per unit × 3 | **Real, moderately fixed by a board** |

---

## The thing that shapes every option: where the e-ink driver lives

The **SSD1680 is not a part you place.** §14 of the
[SSD1680 datasheet Rev 0.14](https://www.crystalfontz.com/controllers/uploaded/SSD1680.pdf)
gives *die tray dimensions* for SSD1680Z / SSD1680Z8 — it ships as bare die,
chip-on-glass bonded to the panel's own flex. The panel already has it.

What the Seeed driver board provides is the **booster network** that makes the
on-chip charge pump work. From §13, Table 13-1 **[DS]**:

| Part | Value | Datasheet requirement |
|---|---|---|
| L1 | **47 µH** | CDRH2D18 / LDNP-470NC, Io = 500 mA max |
| Q1 | NMOS | Si1304BDL / NX3008NBK; BVdss ≥ 30 V, Vgs(th) 0.9 V typ / 1.3 V max, Rds(on) ≤ 2.1 Ω @ Vgs = 2.5 V |
| D1–D3 | Schottky | MBR0530; Vr ≥ 30 V, Io ≥ 500 mA, Vf ≤ 430 mV |
| R1 | 2.2 Ω | 0402–0805, 1%, ≥ 0.05 W |
| C0–C1 | 1 µF | X5R/X7R, 6 V or 25 V |
| C2–C7 | 1 µF | 0402/0603/0805, X5R/X7R, **25 V** |
| C8 | 0.47–1 µF | 0603/0805, X7R, 25 V; effective C > 0.25 µF @ 18 V DC bias |
| U1 | — | 24-pin, **0.5 mm pitch ZIF socket** |

§6.3 and §1.2 **[DS]**: VGH is 10–20 V, VGL = −VGH, **max 40 Vp-p** gate swing.
The absolute-max table gives VGH operating 19.5 / 20 / 20.5 V.

Table 13-1 also carries Solomon Systech's own caveat: *"The recommended component
value and reference part is subject to change depending on panel loading.
Customer is required to review if the selected component value and part is
suitable for their application."* — i.e. these values are a starting point for
a panel that is not necessarily ours.

**Consequence: any option that drops the Seeed driver board — the carrier board
*or* the integrated board — requires laying out a 47 µH switching booster
generating ±20 V, with a hard-switching node, seated within a few millimetres of
a 0.5 mm-pitch flex connector, on the same board as a 2.4 GHz radio.**

That is the difficulty of this project. It is not the ESP32 part, and it is
identical in both options. Any framing where the carrier board is "the easy one"
is wrong unless it also keeps the driver board — which is what Option 0 does.

---

## Option 0 — Passive harness board (recommended)

Keeps the XIAO and the Seeed driver board exactly as they are.

### Block diagram

```
[USB-C panel pigtail] -> 2-pin -> CC1/CC2 5.1k to GND -> VBUS/GND -> XIAO USB pads
[LiPo] -> JST-PH 2.0, polarity-keyed -> XIAO BAT pads
       \-> 1M/1M divider + 100nF -> header pin -> GPIO5
[EC11 encoder] -> through-hole footprint ON this board -> 3 header pins
[3mm LED] -> through-hole + series R -> 1 header pin
[SHT40] -> 4-pin JST-SH -> satellite, still on a cable
[2x7 female headers] <- XIAO plugs in
```

Fixes **1** (divider gets a home with mounting holes), **3**, **6** (encoder,
LED and divider wiring become zero flying wires; the 8 SPI wires stay, since the
driver board stays).

### On pain point 3 — do this today, regardless

The pigtail has no CC resistors, so C-to-C silently does nothing. **Two 5.1 kΩ
resistors from CC1 and CC2 to GND at the device end is the entire fix.** It can
be soldered into the pigtail with heatshrink right now. Putting it on a board is
tidier, not necessary. This is a 20-cent fix that does not need a PCB project.

### Parts

2× 5.1 kΩ 1%, 2× 1 MΩ 1% (owned), 1× 100 nF (owned), 1× LED series R (owned),
JST-PH and JST-SH connectors, 2×7 female headers. Optionally a PESD5V0 or
SP0503 ESD array on the USB lines — on this stack the C6's USB pins run straight
to the connector.

### Quiescent current

Divider 2.1 µA. Everything else passive. **Delta vs today: 0 µA.**

### Pin map and firmware

Unchanged. `firmware/sensor-01/main/app_config.h` untouched. Zero firmware
changes.

### Assembly

100% hand-solderable at through-hole/JST skill level. 0805 optional.

### Cost

JLCPCB 2-layer, 5 pcs, small board: roughly **US$2–7 for boards plus US$15–25
shipping to Canada [UNVERIFIED — no live quote pulled]**. Parts mostly owned.
**NRE: one evening in KiCad.** Call it **CAD 30–50 and 2–3 weeks** of shipping.

### Most likely first-timer mistake

**Mirrored header pinout.** The board gets laid out looking at top copper, then
a XIAO whose pinout was memorised from the bottom gets plugged in, and D0–D10
come out reversed. Print the board 1:1 on paper and physically sit the XIAO on
it before ordering. KiCad's ERC cannot catch this.

---

## Option A — Carrier PCB (XIAO socketed, FPC + booster on-board)

### Block diagram

```
[XIAO ESP32-C6] on 2x7 headers
   |- SPI 6 -> SSD1680 booster (L1/Q1/R1/D1-D3/C0-C8) -> 24-pin 0.5mm ZIF -> panel
   |- I2C 2 -> MAX17048 fuel gauge (0x36) + JST-SH -> SHT40 satellite
   |- 3 -> EC11 through-hole footprint
   |- 1 -> LED + R
   \- ADC -> 1M/1M + 100nF
[USB-C receptacle 16p] -> 5.1k x2 CC -> ESD array -> XIAO USB pads / VBUS
[LiPo JST] -> XIAO BAT pads (XIAO's charger stays in play)
```

Fixes **1, 2, 3, 6** — all of pain point 6, since the 8 SPI wires become traces.
Does not fix **5**. **4** was already fixed.

### Parts that matter

- **Booster:** Table 13-1 above, verbatim. All 0402/0603 plus a 47 µH shielded
  inductor and an SOT-23 NMOS.
- **24-pin 0.5 mm ZIF.** Hirose FH12-24S-0.5SH or equivalent. **Top-contact vs
  bottom-contact is a real and unforgiving choice** — get it wrong and the flex
  will not mate, and there is no rework. Buy from two sources; unmarked
  substitutions are common.
- **Fuel gauge:** MAX17048, [LCSC C2682616](https://www.lcsc.com/product-detail/C2682616.html).
  3 µA hibernate, <5 µA active **[SEARCH — from Analog Devices' own product
  title "3 µA 1-Cell/2-Cell Fuel Gauge"; the electrical table was not read, and
  the analog.com PDF could not be fetched from here to verify]**. TDFN ~2×2 mm:
  hand-solderable with hot air, **not** with an iron. Alternative LC709203F,
  15 µA typ **[SEARCH]** — five times the draw, still 5% of budget.
- **USB-C:** 16-pin receptacle, 2× 5.1 kΩ CC to GND. No PD, no e-marker.
- **ESD:** SP0503BAHT or PESD5V0X1BT on D+/D−.
- **Charger and LDO:** the XIAO's are kept. That is the point of this option.
- **No boost needed.** C6 runs 3.0–3.6 V **[DS]**, LiPo is 3.0–4.2 V — the
  XIAO's LDO already covers it.

### Quiescent current, deep sleep

| Item | Iq | Provenance |
|---|---|---|
| XIAO ESP32-C6 whole-board sleep floor | 15 µA best case; 100–300 µA reported with peripherals left on | **[SEARCH]** — [one PPK2 measurement at 15 µA](https://forum.seeedstudio.com/t/15ua-with-xiao-esp32c6-with-ppk2-while-sleeping-basic-example/276412), [comparison thread](https://forum.seeedstudio.com/t/comparison-of-sleep-currents-for-xiao-esp32c6-s3-and-c3/276444) |
| Battery divider | 2.1 µA | 4.2 V / 2 MΩ, our calc |
| MAX17048 hibernate | 3 µA | **[SEARCH]** |
| SSD1680 booster at rest | ~0 (Q1 off, diodes reverse-biased) | **[UNVERIFIED]** — inferred from topology |
| ESD array leakage | <1 µA | **[UNVERIFIED]** |
| **Added by this board** | **~3–4 µA** | |

**~1% of budget. The 15–300 µA uncertainty in the XIAO itself dwarfs everything
the board does.** That is an argument for measuring, not for building.

### Pin map — survives, and improves

Dropping the driver board means the ePaper pins are no longer fixed by Seeed's
routing, so they can be re-mapped. In `app_config.h`:

- `EPD_PIN_RST` moves off GPIO0 → **frees A0**.
- `VBAT_ADC_GPIO` moves 5 → 0 (A0 / ADC1_CH0). **This deletes the entire
  strapping-pin caveat in [docs/pinmap.md](../docs/pinmap.md) lines 37–65** and
  restores external JTAG on the underside pads.
- `RF_SWITCH_POWER_GPIO`, `RF_ANT_SELECT_GPIO`, `ONBOARD_LED_GPIO`:
  **unchanged — the XIAO is still there.** `board_rf_switch_init()` stays.
- Drivers (`sht40`, `ssd1680`, `monogfx`, `ec11_encoder`) take pins as
  parameters and are unaffected.
- **New:** MAX17048 driver on the existing I2C bus at 0x36, plus a change to the
  Matter PowerSource attribute source.

Change surface: one header, one doc, one new I2C driver.

### Assembly

Split job, which is awkward. The 0402 booster caps and the 0.5 mm ZIF need hot
air and flux experience. So this board wants PCBA for the SMD side — and PCBA at
qty 5 with ~15 unique parts means paying setup plus feeder fees to place about
US$4 of components.

### Cost

**[UNVERIFIED — no live quote]** PCB ~US$5; PCBA setup ~US$8 economic plus ~US$3
per unique extended part; parts ~US$8/unit. Realistically **US$80–150 for a
batch of 5**, plus shipping and possible customs. **NRE: 20–35 hours** for a
first board. Assume at least one respin. Get a real quote at
[jlcpcb.com/pcb-assembly](https://jlcpcb.com/pcb-assembly) once a BOM exists.

### Antenna

XIAO's ceramic antenna, unchanged. But the carrier is now a copper plane sitting
next to it, which it was not before. Notch the board away under and around the
XIAO's antenna end.

### Most likely first-timer mistake

**Scattering the booster network.** The schematic does not force placement, so
L1, Q1, D1–D3 and C2–C7 land wherever they fit. The result is a long, high-dV/dt
loop generating ±20 V centimetres from a Thread radio. Symptoms: weak display
contrast or inconsistent refresh, *and* quietly degraded Thread link margin —
and the two will not be connected. Keep the whole booster loop within ~10 mm of
the ZIF, on one side, over a solid ground pour.

---

## Option B — Integrated board (ESP32-C6-MINI-1 soldered down)

### Block diagram

```
[USB-C 16p] -> CC 5.1k x2 -> ESD -> VBUS
   |-> MCP73831 charger (PROG R sets Ichg) -> [LiPo + JST + protection]
   \-> D+/D- -> ESP32-C6-MINI-1 native USB
[VBAT] -> TPS7A02 LDO 3.3V -> VDD rail
   |-> ESP32-C6-MINI-1 (13.2 x 16.6 x 2.4mm, integrated PCB antenna)
   |-> SSD1680 booster network -> 24-pin 0.5mm ZIF -> panel
   |-> MAX17048 (senses VBAT directly)
   \-> SHT40 satellite via JST-SH
[EC11 through-hole] [LED + R] [BOOT + EN buttons] [UART/JTAG test points]
```

Fixes **1, 2, 3, 6**. Not **5**.

### Parts that matter

- **ESP32-C6-MINI-1-N4.** From the
  [ESP32-C6-MINI-1 / -1U datasheet](https://documentation.espressif.com/esp32-c6-mini-1_mini-1u_datasheet_en.html)
  **[DS]**: 13.20 × 16.60 × 2.40 mm, 22 GPIO, 3.0–3.6 V, **deep sleep 7 µA typ
  @ 25 °C with RTC timer and LP memory on.** N-suffix −40…85 °C, H-suffix
  −40…105 °C. The `-1U` variant (13.2 × 12.5 mm) drops the antenna for U.FL.
- **Charger: MCP73831.** From the
  [Microchip MCP73831/2 datasheet DS20001984H](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP73831-Family-Data-Sheet-DS20001984H.pdf)
  **[DS]** — §4.3: *"During any UVLO condition, the battery reverse discharge
  current is less than 2 µA."* The electrical table gives Output Reverse Leakage
  I(DISCHARGE) typ 0.15 µA / max 2 µA with PROG floating. Note two adjacent
  figures that are **not** our case: −5.5 to −15 µA in Charge Complete (input
  present), and a 6 µA typ VBAT source current during battery detection.
  SOT-23-5, hand-solderable. [TP4057](https://www.lcsc.com/product-detail/C12044.html)
  is the LCSC-cheap equivalent at similar leakage **[SEARCH]**.
- **LDO: TPS7A02.** From the [TI datasheet](https://www.ti.com/lit/ds/symlink/tps7a02.pdf)
  **[DS]**: 25 nA Iq, 200 mA, 0.8–5.0 V in 50 mV steps. **Check the 200 mA
  headroom** against 802.15.4 TX peak plus e-ink refresh peak drawn
  concurrently — this is the one spec that could bite. Fallback MCP1700 at
  1.6 µA / 250 mA **[UNVERIFIED]**.
- **Cell protection:** check whether the EEMB LP103454 already carries a PCM.
  Most EEMB cells with leads do. If it does, do not add a second — that stacks
  two ~3 µA parts and two series FETs for nothing. Verify with a multimeter and
  a look at the tab end.
- Booster network, ZIF, fuel gauge, USB-C, ESD: as Option A.

### Quiescent current

| Item | Iq | Provenance |
|---|---|---|
| ESP32-C6-MINI-1 deep sleep | **7 µA** typ | **[DS]** Espressif module datasheet |
| TPS7A02 | **0.025 µA** | **[DS]** TI datasheet |
| MCP73831, no input (UVLO) | **<2 µA** max, 0.15 µA typ | **[DS]** Microchip DS20001984H |
| MAX17048 hibernate | 3 µA | **[SEARCH]** |
| Battery divider (keep as cross-check) | 2.1 µA | our calc |
| Cell PCM, if present | ~3 µA | **[UNVERIFIED]** |
| **Hardware floor** | **~15 µA** | |

Plus the Thread ICD radio duty cycle, which is firmware and identical in every
option.

**~15 µA is a genuinely good number and probably beats the XIAO — but "probably"
is doing real work there, because the XIAO's floor on this stack has never been
measured.** If the XIAO measures 20 µA, Option B saves 5 µA: 1.7% of budget,
about two weeks of an eight-month runtime. If it measures 250 µA, Option B is
transformative. **Which one is true is unknowable today, and the meter costs
less than the board spin.**

### Pin map — what a custom board escapes and what it inherits

**Escaped** (Seeed-added, gone with the XIAO):

- **RF switch.** The C6-MINI-1 has a fixed PCB antenna, no FM8625H, no Q3, no
  VCTL. **Delete `RF_SWITCH_POWER_GPIO`, `RF_ANT_SELECT_GPIO`, and
  `board_rf_switch_init()` in `app_main.cpp`.** This also removes a boot-time
  failure mode where a firmware bug leaves the radio with no antenna.
- **Onboard LED on GPIO15.** Delete `ONBOARD_LED_GPIO`.
- **A0/D0 collision.** Gone — pins become ours to assign.
- **No fuel gauge.** Add MAX17048.

**Inherited** (ESP32-C6 silicon, not Seeed's doing):

- **GPIO4 (MTMS) and GPIO5 (MTDI) still strap the SDIO sampling/driving clock
  edge.** [docs/pinmap.md](../docs/pinmap.md) already cites
  [ESP32-C6 datasheet](https://documentation.espressif.com/esp32-c6_datasheet_en.html)
  Table 3-4 for this. Chip-level. A custom board escapes nothing here — it just
  would not put the divider there, because A0 would be free.
- **GPIO8 and GPIO9 still strap boot mode.** Chip-level.
- **GPIO15 is still a C6 strapping pin** — [docs/pinmap.md](../docs/pinmap.md)
  line 29 says so. What is escaped is Seeed's *LED* on it, not the strap.
- Therefore Seeed's *"avoid GPIO4, 5, 8, 9, 15"* note (sheet 4/5 of the
  [XIAO ESP32-C6 schematic](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/XIAO-ESP32-C6_v1.0_SCH_PDF_24028.pdf))
  is **mostly the C6's own strapping set, not Seeed advice.** It stays binding.

Firmware change surface: `app_config.h` fully rewritten, `board_rf_switch_init()`
deleted, MAX17048 driver added, `docs/pinmap.md` rewritten. The four drivers
worth keeping — `sht40`, `ssd1680`, `monogfx`, `ec11_encoder` — are all
parameterised by pin and **survive untouched.** That is the payoff for having
written them properly.

### Assembly

**Machine only.** The module is castellated + bottom-pad LGA, the MAX17048 is
TDFN, the ZIF is 0.5 mm pitch, the booster caps are 0402. Nothing here is
compatible with through-hole/JST/perfboard skills. Hand-solder only the EC11,
the LED, and the JST connectors.

### Cost

**[UNVERIFIED — no live quote]** Boards + PCBA setup + ~25 unique parts, several
of them extended-library feeder charges, at qty 5: budget **US$150–250 for the
batch**, plus shipping and customs to Canada. **NRE: 40–80 hours** for a first
board, and plan on **two spins** — the booster and the RF section are exactly
what does not work first time. Calendar: **8–14 weeks**, given two rounds of
2–3 week shipping.

**For three devices: roughly CAD 400–700 and two to three months, to replace
CAD 0 of hardware already owned.**

### Antenna

From [Espressif's ESP32-C6 PCB layout guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c6/pcb-layout-design.html)
**[DS]**:

- Place the module's PCB antenna **outside the base board outline**, feed point
  close to the board edge.
- **Cut the base board away on both sides of the antenna and below it.**
- Ground copper and dense ground vias near the antenna on the base board.
- **≥15 mm clearance in all directions inside the housing.**

**That last one is not achievable in this enclosure.** The cavity is
106.64 × ~47.9 × 18.6 mm and contains a 56 × 34.5 × 10.6 mm LiPo — a conductive
slab occupying a third of the floor — plus the panel's conductive backplane on
one face. A corner-mounted antenna cannot get 15 mm clear in all directions.

Two honest qualifications: it is a recommendation, and violating it costs link
margin rather than function — range degrades, the network still joins. And
**the existing XIAO build already violates it in exactly the same way, unmeasured.**
So this is not a reason to prefer one option. It is a reason to run a Thread
RSSI / link-margin check on the hand-wired unit before committing any layout.

**Do not use a PCB trace antenna.** It needs controlled 50 Ω impedance, a tuned
CLC matching network, and a VNA to verify. On a first board it will silently
cost 6–10 dB with no way to know. The module's pre-certified antenna is correct,
and it keeps the "no certification needed for a personal build" status simple.

### Most likely first-timer mistake

**Shipping a board with no bring-up path.** The module is soldered down, so if
the boot straps are wrong, or the USB differential pair is swapped, or EN has no
RC, the result is a brick with no way in. Non-negotiable on any first board:
**BOOT button on GPIO9, EN button with a 10 kΩ/1 µF RC, exposed TXD0/RXD0 test
points, and a ground test point large enough for a scope probe.** Add them even
though native USB "should" work. The one time it does not, they are the
difference between a fix and a respin.

---

## Recommendation

**Do not build a full custom PCB for three units.**

Option B costs 40–80 hours of design, two to three months of calendar, and
CAD 400–700, to save roughly 8 hours of soldering across three units and to add
a fuel gauge that is not needed. It does not earn its cost. If the goal is three
working sensors, hand-wire them.

If the goal is *learning PCB design*, that is legitimate — but it should be
stated as the goal, because the cost/benefit does not close on labour.

### Order of work

1. **Fix the CC resistors now.** Two 5.1 kΩ to GND in the pigtail, heatshrink.
   Pain point 3, gone, for 20 cents. Do not wait for a board.
2. **Hand-wire unit 1** when the parts land. Already committed — hardware is in
   transit and firmware is written.
3. **Design Option 0** in parallel. One evening, ~CAD 40. A good first KiCad
   project: 15 parts, no RF, no switching supply, nothing that cannot be
   reworked. Learn the tool where a mistake costs a jumper wire.
4. **Measure.** PPK2 or µCurrent between cell and XIAO. Fill in the empty
   measured column in [docs/power-budget.md](../docs/power-budget.md). Also
   check Thread link margin from where the unit will actually hang on the wall.
5. **Then decide.** If the measured floor is near 15 µA, Option B buys nothing
   and this is closed. If it is 200 µA+, there is now a real reason to build a
   board, real numbers to design against, and a case revision wanted anyway for
   the SHT40 pocket.

### If overruled, build B, not A

Option A pays the full difficulty of the ±20 V booster and the 0.5 mm ZIF — the
two hard parts — while keeping the XIAO's unknown sleep floor, its regulator,
and its RF switch. It takes all the risk and leaves the main benefit on the
table. If that booster is getting laid out anyway, take the 7 µA module and
delete `board_rf_switch_init()` while you are there.

### Lead times

No 20-week parts were identified in any option. **But no live stock was
checked.** Verify before committing:

- ESP32-C6-MINI-1-N4
- MAX17048 ([LCSC C2682616](https://www.lcsc.com/product-detail/C2682616.html))
- 24-pin 0.5 mm ZIF — buy from two sources, top/bottom-contact substitutions
  are common and unmarked
- 47 µH shielded inductor rated ≥500 mA in a CDRH2D18-class footprint
  (e.g. [Würth WE-LQS](https://www.we-online.com/en/components/products/WE-LQS) family)

---

## Tooling

`kicad-happy@kicad-happy` v2.1.0 is installed at user scope — 952 stars, MIT,
last pushed 2026-08-01. Eleven skills: `kicad` (parses `.kicad_sch`, `.kicad_pcb`
and Gerbers as S-expressions, KiCad 5–10, no KiCad install required), `emc`
(44 rule IDs), `spice`, `datasheets`, `bom`, and supplier/fab skills for DigiKey,
Mouser, LCSC, element14, JLCPCB and PCBWay. Vetted before install: no hooks, no
install scripts, 78k lines of Python across 95 scripts, `subprocess` confined to
invoking ngspice/pdftotext and `urllib` to SPICE model fetches. Cost ~3.2k tokens
always-on.

Alternatives considered and rejected:

| Repo | Stars | Why not |
|---|---|---|
| [nickkraakman/skidl-skills](https://github.com/nickkraakman/skidl-skills) | 15 | The only one that *generates* rather than reviews, via SKiDL → KiCad netlist. Requires KiCad, netlistsvg and a SKiDL virtualenv installed locally — none present on this machine |
| [SPREsxm/claude-pcb-designer](https://github.com/SPREsxm/claude-pcb-designer) | 3 | Prompt-and-checklist only, no file manipulation |
| [obelisk-complex/claude-skills](https://github.com/obelisk-complex/claude-skills/blob/main/pcb-engineer.md) | 0 | Same — prompt-only |

---

## Sources

All verified HTTP 200 on 2026-08-13.

**Datasheets read for this document:**

- [SSD1680 datasheet Rev 0.14, Solomon Systech](https://www.crystalfontz.com/controllers/uploaded/SSD1680.pdf) — §6.3 booster, §13 application circuit, §14 die tray
- [ESP32-C6-MINI-1 / -1U datasheet, Espressif](https://documentation.espressif.com/esp32-c6-mini-1_mini-1u_datasheet_en.html)
- [ESP32-C6 series datasheet, Espressif](https://documentation.espressif.com/esp32-c6_datasheet_en.html) — Table 3-4 strapping pins
- [ESP32-C6 PCB layout design guidelines, Espressif](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c6/pcb-layout-design.html)
- [TPS7A02 datasheet, Texas Instruments](https://www.ti.com/lit/ds/symlink/tps7a02.pdf)
- [MCP73831/2 datasheet DS20001984H, Microchip](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP73831-Family-Data-Sheet-DS20001984H.pdf)
- [XIAO ESP32-C6 v1.0 schematic, Seeed](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/XIAO-ESP32-C6_v1.0_SCH_PDF_24028.pdf)

**Referenced but not read first-hand:**

- [SHT4x datasheet, Sensirion](https://sensirion.com/resource/datasheet/sht4x)
- [MAX17048 at LCSC (C2682616)](https://www.lcsc.com/product-detail/C2682616.html) — the analog.com PDF would not fetch; the 3 µA figure remains **[SEARCH]**
- [TP4057 at LCSC (C12044)](https://www.lcsc.com/product-detail/C12044.html)

**Community measurements:**

- [15 µA on XIAO ESP32C6 with PPK2, Seeed forum](https://forum.seeedstudio.com/t/15ua-with-xiao-esp32c6-with-ppk2-while-sleeping-basic-example/276412)
- [Comparison of sleep currents for XIAO C6/S3/C3, Seeed forum](https://forum.seeedstudio.com/t/comparison-of-sleep-currents-for-xiao-esp32c6-s3-and-c3/276444)
- [XIAO ESP32-C6 getting started wiki, Seeed](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) (GPL-3.0 content — do not copy into this repo)

**Fabrication:**

- [JLCPCB PCB assembly](https://jlcpcb.com/pcb-assembly) — get a real quote once a BOM exists
