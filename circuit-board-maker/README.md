# circuit-board-maker

Investigation into replacing the hand-wired sensor-01 build with a custom PCB,
and the board designs that came out of it.

> **Status: paper design. Nothing here has been fabricated, and the KiCad files
> have never been opened in KiCad.** See [What is and isn't
> verified](#what-is-and-isnt-verified) before acting on any of it.

## Conclusion

**Don't build a full custom PCB for three units.** Build `hb-01`, hand-wire the
three units, and measure the assembled device before letting any power argument
justify a board spin. Full reasoning in [pcb-evaluation.md](pcb-evaluation.md).

## Documents

| Doc | Contents |
|---|---|
| [pcb-evaluation.md](pcb-evaluation.md) | Three options costed, quiescent-current budgets, antenna clearance, firmware change surface, and why a PCB doesn't earn its cost at qty 3 |
| [component-selection.md](component-selection.md) | Board-mount part equivalents with live LCSC stock, and the three parts where board-mounting is a downgrade |

## Boards

| Board | Size | Purpose | Assembly |
|---|---|---|---|
| [hb-01/](hb-01/) | 48 × 40 mm | Harness board — divider, USB-C + CC resistors, ESD, encoder, LED, JSTs. Keeps the XIAO and the Seeed driver board | **Hand-solderable** |
| [sb-01/](sb-01/) | 12 × 12 mm | SHT40 satellite, keeps the sensor out of MCU heat | Needs reflow or hot air |

![hb-01 plan view](img/hb-01-plan.svg)

![sb-01 plan view](img/sb-01-plan.svg)

![Option 0 interconnect](img/interconnect.svg)

## Files per board

```
hb-01/
  hb-01.kicad_pro    project settings, net classes
  hb-01.kicad_pcb    board outline, mounting holes, silkscreen. No footprints
  hb-01.net          KiCad netlist - import this in Pcbnew to pull in footprints
  NETLIST.md         authoritative connection list + BOM with LCSC codes
```

### Intended workflow

There is no `.kicad_sch`. Schematic capture needs KiCad to resolve and place
symbols, and shipping a schematic that fails to open is worse than shipping
none. The netlist is the design; the schematic is a drawing of it.

1. Open `hb-01.kicad_pro` in KiCad, then open the PCB editor.
2. **File → Import → Netlist**, select `hb-01.net`. Footprints land in a pile
   with the ratsnest connected.
3. Drag them to roughly the placement in the plan view above, then route.
4. If you want a schematic for documentation, draw it from `NETLIST.md`.

## What is and isn't verified

**Verified:**

- Both `.kicad_pcb` files parse and round-trip through `kiutils` — the
  S-expressions are structurally sound.
- Both `.kicad_pro` files are valid JSON.
- Every LCSC part number in `NETLIST.md` and
  [component-selection.md](component-selection.md) was in stock on 2026-08-13.
- All external URLs in both documents resolve.

**Not verified — check these first:**

- **Nothing has been opened in KiCad.** No `kicad-cli` on the machine that
  generated these.
- **Every footprint library name is from memory.** `Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm`
  in particular is an Alps footprint standing in for a Bourns PEC11R — check it
  against the Bourns mechanical drawing before ordering.
- **No DRC, no ERC, no routing.** Placement in the plan views is indicative.
- **Board dimensions are not fitted to the case.** 48 × 40 mm was chosen to sit
  beside the LP103454 in the 106.64 × 47.9 mm cavity, but was not checked
  against the STLs in [hardware/case](../hardware/case/).
- **The Seeed driver board's IO breakout pinout is undocumented.** `J2` is a
  labelled 2.54 mm field, not a verified mating connector. Probe the driver
  board when it arrives.
- **USB-C on `hb-01` carries power only.** The XIAO does not expose D+/D− on its
  header, so flashing and console stay on the XIAO's own connector.

## Regenerating

```
python3 gen.py    # netlists, .kicad_pcb, .kicad_pro, NETLIST.md
python3 svg.py    # img/*.svg
```

Both write directly into this directory. Edit the spec at the top of `gen.py`
rather than editing the generated files by hand — until the design moves into
KiCad proper, at which point KiCad owns them and these scripts should be
retired.
