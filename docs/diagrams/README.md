# Bench diagrams

Self-contained HTML pages used during sensor-01 bring-up. Open any of them
directly in a browser — no build step, no external assets, no network access.
Each follows the viewer's light/dark theme.

| File | What it shows |
|---|---|
| [bench-rig.html](bench-rig.html) | Breadboard wiring for the second bench step: XIAO + Grove SHT40 + ePaper driver board + panel, with the full jumper table |
| [battery-divider.html](battery-divider.html) | Step 3: the LiPo on the XIAO's underside pads and the 2×1M + 100nF ADC divider — schematic, connection table, and the polarity warning |
| [internals.html](internals.html) | How the parts sit inside the enclosure — plan view and side elevation, dimensioned from the STLs |
| [bringup-ladder.html](bringup-ladder.html) | The no-solder connectivity procedure: one thing added per rung, using only internal pull resistors and the ADC |

## These are dated snapshots, not current state

They record what was believed at the moment they were drawn. Bench work since
has contradicted parts of them, and the contradictions are the useful bit:

- **`bench-rig.html` draws the driver board with female sockets fitted.** It
  ships with bare through-holes and no headers — see the warning in
  [../assembly.md](../assembly.md). The drawing was made before that was
  known, and the phantom faults that followed are what prompted the ladder.
- **`bench-rig.html` says step 1 is verified.** It was, then it wasn't — the
  SHT40 dropped off the I2C bus repeatedly. Root cause was a breadboard with
  dead spring clips, not the sensor or the wiring. A replacement board brought
  it straight back (`0x44` answering, readings flowing).
- **The panel has still never drawn.** Every "what success looks like" item in
  `bench-rig.html` remains unobserved.
- **`battery-divider.html` is drawn ahead of the work**, 2026-08-24. Values and
  pins come from `app_config.h` and `assembly.md`, but nothing in it has been
  built or measured yet.
- **`internals.html` marks its own soft spots.** The driver board and ePaper
  module outlines are sketched rather than measured; everything tagged
  *geometry* came out of the STL files and is trustworthy.

Bring-up rows these feed are tracked in [../bringup.md](../bringup.md).
