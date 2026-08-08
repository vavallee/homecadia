# Wall-mount case (veltoc remix, rev 1)

Remix of [veltoc](https://github.com/danking6/veltoc) 3D models (MIT,
© 2026 Dan King). Only `case-back-wallmount.stl` is modified; the other three
files print as-is from upstream.

![case preview](preview.png)

## Files (`stl/`)

| File | Status | Notes |
|---|---|---|
| case-back-wallmount.stl | remixed | wall-mount rails, side USB-C, rear port plugged |
| case-front.stl | upstream | EC11 mount (Ø6.8 shaft hole, Ø9.8/Ø14 recess), 3mm LED hole, display aperture |
| insert-tray.stl | upstream | |
| encoder-knob.stl | upstream | fits 20mm flatted D-shaft |

## Changes in case-back-wallmount.stl

- Rear Ø12.8mm panel-mount USB-C hole **plugged** (was center-rear — would
  face the wall).
- New Ø12.8mm USB-C hole through the **left side wall** (y=25, z=10) for the
  female USB-C panel pigtail; cable reaches the XIAO's USB-C internally.
- Two standoff rails (11×44×3mm) on the rear face: 3mm air gap so the rear
  vents breathe against a wall, and flat landings for Command Small strips
  (one per rail).
- Keyhole slots in each rail (85mm apart, center height y=30): Ø9.5 entry,
  4.3mm slot rising 8mm, head pocket behind a 2mm retention face. Fits #6
  pan-head screws (head ≤ Ø8.5×2.4mm). Level the two screws; the slot rises
  up, so the case drops onto them.
- Orientation on the wall: slatted side **down** (air inlet), rear vent band
  at the top (outlet).

## Print notes

- 3 units → print each file ×3. PETG or PLA, ~0.2mm layers.
- Back prints rear-face down (rails on bed); keyhole head-pockets bridge
  ~8.8mm — fine with standard supports/bridging.
- Tolerances derived from published dims, **not test-fitted**. Print ONE unit
  first and test-fit the panel, driver board, encoder, USB pigtail and a #6
  screw head before committing all three.

## Parts this design needs beyond the sensor BOM

- EC11-style rotary encoder (upstream links Amazon B08728K3YB; this build
  uses the Bourns PEC11R — see [docs/bom.md](../../docs/bom.md))
- Female USB-C panel-mount pigtail (upstream links Amazon B0D1MW6YPL; bezel
  must fit Ø12.8)
- 3mm LED
- Battery divider resistors: upstream specifies 2×100k, **this build uses
  2×1MΩ + 100nF** — 100k bleeds ~21µA continuously, ~9% of the power budget
  ([docs/bom.md](../../docs/bom.md)).
- Upstream firmware drives a BME280; this build's esp-matter firmware uses
  an SHT40.
