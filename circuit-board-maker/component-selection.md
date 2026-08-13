# Component selection — board-mount equivalents

Companion to [pcb-evaluation.md](pcb-evaluation.md). That document decides
*whether* to build a board. This one decides *which parts* go on it, if one gets
built.

Stock and pricing pulled from LCSC via the jlcsearch community API on
2026-08-13. Prices are USD at the low-quantity break. Stock figures move; treat
them as "was plentiful / was thin", not as commitments.

**HS** = hand-solderable at through-hole/JST/perfboard skill level.
**M** = machine assembly only.

---

## Board-mounting is a downgrade for three of these parts

Going board-mount is not uniformly an improvement:

| | Part | Verdict |
|---|---|---|
| ✅ | Divider resistors, hold cap | Clear win — flying components get a footprint |
| ✅ | Battery connector | Clear win — strain goes into the board |
| ✅ | Charger, LDO, fuel gauge | Clear win — they don't exist as discrete parts today |
| ➖ | EC11 encoder | **Already correct.** Nothing to change |
| ⚠️ | 3mm LED | Ambiguous — board-mounting forces case alignment |
| ⚠️ | USB-C | Costs a case revision |
| ❌ | SHT40 | **Actively worse** on the main board |

### The EC11 is already a PCB-mount part

Bourns PEC11R-4220F-S0024 has through-hole PC pins and two mounting posts,
designed to solder to a board and take shaft force through the posts. It is the
one part in [docs/bom.md](../docs/bom.md) that was already selected correctly
for a board. No change.

### The LED gets worse

On flying leads it goes wherever the case has a hole. On a board it is fixed in
X/Y, and the case aperture must match board placement within roughly a
millimetre — or a light pipe gets added. For a single status LED, flying leads
to a panel-mounted 3 mm part is the more forgiving design. **Keep it as-is.**

### The SHT40 must not go on the main board

[docs/bom.md](../docs/bom.md) already records why: *"SHT40 sits in the case
airflow path, external to MCU heat — self-heating skews readings."*

The board-mount version of this part is a **satellite board** — a ~10 × 10 mm
PCB carrying the sensor and its decoupling cap, on a 4-wire cable to a JST-SH on
the main board. That is board-mounting done right. Placing it beside the LDO and
a booster inductor is board-mounting done wrong.

---

## The display: the board-mount answer is "don't"

The instinct is to put the FPC connector and the SSD1680 booster on the board.
The parts exist and are cheap:

| Function | LCSC | Part | Package | Stock | Unit |
|---|---|---|---|---|---|
| 24-pin FPC | [C2856805](https://www.lcsc.com/product-detail/C2856805.html) | FPC-05F-24PH20 | 0.5 mm horiz. | 93,037 | $0.110 |
| 24-pin FPC (Hirose) | [C324726](https://www.lcsc.com/product-detail/C324726.html) | FH34SRJ-24S-0.5SH(50) | 0.5 mm horiz. | 53,948 | $0.276 |
| Inductor 47 µH | [C167766](https://www.lcsc.com/product-detail/C167766.html) | FNR3015S470MT | 3×3 mm | 46,881 | $0.042 |
| Schottky ×3 | [C82046](https://www.lcsc.com/product-detail/C82046.html) | MBR0530T1G | SOD-123 | 78,840 | $0.063 |
| NMOS | [C179399](https://www.lcsc.com/product-detail/C179399.html) | NX3008NBK,215 | SOT-23 | 61,392 | $0.053 |

That NX3008NBK is **the exact part Solomon Systech names** in Table 13-1 of the
[SSD1680 datasheet](https://www.crystalfontz.com/controllers/uploaded/SSD1680.pdf),
and it is in stock at five cents. The whole booster BOM is under a dollar.

**Build it anyway and you have taken on the hardest part of this project to save
about six dollars.** It means laying out a ±20 V switching node beside a Thread
radio, on a first PCB, with two footprint traps:

1. **Top-contact vs bottom-contact FPC.** All the connectors above are
   "horizontal mount," which says nothing about which face the contacts are on.
   Order the wrong one and the panel flex will not mate, and there is no rework
   path on a 0.5 mm ZIF. **Unresolvable until the Seeed panel is physically in
   hand** — measure it when it lands.
2. **The 25 V capacitors.** C2–C7 are 1 µF at **25 V**, and C8 must hold
   *effective* capacitance >0.25 µF at 18 V DC bias. Solomon Systech spells that
   out because Class II ceramics lose most of their capacitance under bias.
   Substitute a 1 µF 6.3 V 0402 because the value matched, and the booster
   quietly underperforms.

**Better move: keep the Seeed driver board and mount *it*.** It already carries
the booster, the FPC, and an ETA9740 charger, and Seeed's product description
lists an **"IO Break out"** header alongside the XIAO socket. Give the harness
board a mating header footprint and the display becomes a module. Cost: a few
millimetres of stack height in an 18.6 mm cavity. Benefit: never touch ±20 V or
0.5 mm pitch.

> **Open question:** the exact pinout of the driver board's IO breakout header is
> not documented in anything found so far. Probe it when the board arrives; the
> harness board design in [hb-01/](hb-01/) treats it as a labelled 2.54 mm field
> rather than a precise mating connector because of this.

---

## Hand-solderable parts

| Job | LCSC | Part | Package | Stock | Unit | |
|---|---|---|---|---|---|---|
| Charger | [C424093](https://www.lcsc.com/product-detail/C424093.html) | MCP73831T-2ACI/OT | SOT-23-5 | 2,727 | $0.87 | HS |
| **LDO 3.3 V, 25 nA Iq** | [C5142805](https://www.lcsc.com/product-detail/C5142805.html) | **TPS7A0233DBVR** | **SOT-23-5** | **433** | $0.76 | HS |
| USB-C, full data | [C2765186](https://www.lcsc.com/product-detail/C2765186.html) | TYPE-C 16PIN 2MD(073) | SMD | 1,171,811 | $0.074 | HS |
| ESD array | [C3040626](https://www.lcsc.com/product-detail/C3040626.html) | SP0503BAHTG | SOT-143 | 59,167 | $0.096 | HS |
| Battery connector | [C131337](https://www.lcsc.com/product-detail/C131337.html) | B2B-PH-K-S(LF)(SN) | TH, 2 mm | 298,406 | $0.035 | HS |
| Encoder | — | PEC11R-4220F-S0024 (owned) | TH | — | — | HS |

### The TPS7A0233 in SOT-23-5 is the find here

The 25 nA Iq part exists in a package solderable with an iron
([TI datasheet](https://www.ti.com/lit/ds/symlink/tps7a02.pdf)). Two caveats:

- **Stock is only 433**, versus 1,409 for the machine-only X2-SON-4 variant
  ([C2860046](https://www.lcsc.com/product-detail/C2860046.html), $0.54). Buy
  spares now if this is wanted.
- **Re-check the 200 mA rating** against 802.15.4 TX peak plus e-ink refresh
  drawn concurrently. That is the one spec that could bite.

### Use the 16-pin USB-C, not the 6-pin

LCSC has [C456012](https://www.lcsc.com/product-detail/C456012.html) "TYPE-C 6P"
at 527,859 stock and $0.039, and it is much easier to hand-solder. **Don't.**
Power-only means no D+/D−, which costs native USB — and
[docs/build.md](../docs/build.md) flashes over exactly that, with the console on
USB Serial/JTAG. Six-pin USB-C means flashing over the underside pads for the
life of the device.

### Board-mount USB-C costs a case revision

The Ø12.8 mm round side-wall hole exists because the pigtail is panel-mount. A
board connector needs a rectangular cutout aligned to board position, and the
board's X/Y placement becomes mechanically load-bearing — every insertion puts
force into the board and its mounts rather than into the case wall. The
panel-mount pigtail's strain relief is a real advantage being given up.

**But the pigtail cannot be fixed in place.** On a 2-wire panel-mount USB-C
part, CC1 and CC2 terminate inside the moulded housing and are not brought out
on the two wires, so there is nothing external to attach a 5.1 kΩ resistor to.
Fixing C-to-C means either swapping the pigtail for one that has the resistors
built in, or putting the receptacle on this board and accepting the case
revision. See the correction in
[pcb-evaluation.md](pcb-evaluation.md#on-pain-point-3--the-fix-is-a-connector-swap-not-two-resistors).

**Measure before buying.** Some 2-wire pigtails do include internal 5.1 kΩ
resistors. Probe CC1→GND and CC2→GND on the connector face; 5.1 kΩ on both means
there is no problem to solve.

Note that on the Option 0 board the receptacle can only carry **power**. The
XIAO does not expose D+/D− on its header, so USB data still comes from the
XIAO's own connector — VBUS from this board feeds the XIAO's 5V pin and its
onboard charger. Flashing and console remain on the XIAO's connector either way.

---

## Machine-assembly-only parts

| Job | LCSC | Part | Package | Stock | Unit | |
|---|---|---|---|---|---|---|
| MCU module | [C5736265](https://www.lcsc.com/product-detail/C5736265.html) | ESP32-C6-MINI-1-N4 | SMD-53P | 3,344 | $4.22 | M |
| Fuel gauge | [C2682616](https://www.lcsc.com/product-detail/C2682616.html) | MAX17048G+T10 | DFN-8-EP 2×2 | 4,403 | $2.32 | M |
| Temp/humidity | [C2909890](https://www.lcsc.com/product-detail/C2909890.html) | SHT40-AD1B-R2 | DFN-4-EP 1.5×1.5 | 20,692 | $2.03 | M |

Also stocked: [C2848306](https://www.lcsc.com/product-detail/C2848306.html)
SHT40-AD1B-R3 (16,103, $1.84 — cheaper tape variant of the same die) and
[C20627095](https://www.lcsc.com/product-detail/C20627095.html)
ESP32-C6-MINI-1U-H4 (787, $4.55 — the 13.2 × 12.5 mm no-antenna U.FL variant).

**There is no hand-solderable fuel gauge.** MAX17048 is DFN-8 2×2 mm; the WLP-8
alternative ([C7497002](https://www.lcsc.com/product-detail/C7497002.html)) is
0.9 × 1.8 mm with 18 in stock. Fixing pain point 2 means committing to machine
assembly. Given that a fuel gauge saves 3 µA of nothing and improves a
percentage reported hourly, this is the weakest item on the pain-point list to
cross that line for.

---

## Passives — one caveat that is new

1 MΩ 1% and 100 nF X7R in 0805 are trivially available and hand-solderable.

**At 1 MΩ, board surface leakage stops being negligible.** Unwashed flux residue
across the divider node can shift the ADC reading measurably. Clean thoroughly
under that node, or accept a calibration offset. This is a *new* failure mode —
it does not exist on the current flying-component divider because there is no
board surface to leak across.

---

## What this does not change

**A keyed JST does not solve the polarity hazard.** [docs/bom.md](../docs/bom.md)
warns that EEMB cells are *"frequently reversed vs the Seeed/Adafruit
convention"* — that is the cell's crimp, not the connector. A polarized board
connector will happily accept a backwards-wired cell and route it to the
charger. **Keep the multimeter check in [docs/assembly.md](../docs/assembly.md).**
Board-mounting arguably makes this *worse*, by removing the moment where bare
wires are in hand and polarity is already on the mind.

**None of this moves the power budget.** Summing the hand-solderable set:
MCP73831 <2 µA (UVLO), TPS7A0233 0.025 µA, divider 2.1 µA — roughly 4 µA against
a 300 µA budget. The conclusion in [pcb-evaluation.md](pcb-evaluation.md) stands:
measure the assembled unit before component selection is allowed to argue from
power.

**Sourcing:** these are LCSC prices, and for a 3-unit build they are irrelevant
next to freight. [docs/bom.md](../docs/bom.md) already shows the encoder carrying
CAD 15.00 shipping + 3.89 HST on a CAD 12.81 order. Consolidate into one LCSC
order or don't bother — the parts here total under USD 10 for three units.

---

## Unverified — check before ordering

- **Saturation current on the 47 µH inductors.** Table 13-1 needs Io = 500 mA
  max. Only inductance and package were confirmed; Isat and Irms were not.
- **FPC contact side** (top vs bottom). Needs the physical panel in hand.
- **MCP73831 suffix decoding.** Confirm the chosen suffix gives 4.20 V
  regulation and the status-output style wanted.
- **Live stock.** All figures are a 2026-08-13 snapshot.
