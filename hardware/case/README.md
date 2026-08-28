# Wall-mount case (veltoc remix, rev 4)

Remix of [veltoc](https://github.com/danking6/veltoc) 3D models (MIT,
© 2026 Dan King). `case-back-wallmount.stl` and `case-front.stl` are both
modified; `insert-tray.stl` and `encoder-knob.stl` print as-is from upstream.

> **rev 2 — lengthened 7 mm for the 2000 mAh cell.** Upstream is built around a
> 1400 mAh cell (EEMB 112945, 45 mm long). Our BOM specifies a 2000 mAh EEMB
> LP103454, whose finished size is **56 × 34.5 × 10.6 mm** — 11 mm longer. The
> rev 1 interior measured 99.64 mm and had to hold the 48.5 mm `insert-tray`
> plus the cell: 104.5 mm of parts in a 99.6 mm box.
>
> Both shells were cut at **x = 24.73 mm** — a band verified prismatic (identical
> cross-section either side) and clear of the display aperture (29.5–97.5),
> encoder, LED, vents and tray posts — and everything right of the cut moved
> +7 mm. Interior is now **106.64 mm**, leaving **2.1 mm clearance**. The display
> aperture is unchanged at 68.00 mm wide; it simply sits 7 mm further along.

![assembled render](render-assembled.png)

Assembled view, rev 4. Rendered directly from the STL geometry in this
directory — real part shapes and real relative positions, so the proportions,
the wordmark placement and the aperture framing are accurate. The wordmark is
composited at its true size and position, representing the laser engraving.
Surface finish and screen contents are illustrative. `preview.png` is the older
rev 1 image and no longer matches the models.

**Colours (PLA, matched to EurekaTec stock):** teal front, light grey dial,
black or dark grey back (PLA availability to confirm), tray any colour since it
is never seen. EurekaTec's ABS stock is black / dark grey / red only, which is
part of why the build moved to PLA.

## Files (`stl/`)

| File | Status | Outer size (mm) | Notes |
|---|---|---|---|
| case-back-wallmount.stl | remixed + rev 2 stretch | 116.28 × 59.28 × 23.00 | wall-mount rails, side USB-C, rear port plugged |
| case-front.stl | rev 2 stretch | 110.60 × 53.60 × 8.20 | EC11 mount (Ø6.8 shaft hole, Ø9.8/Ø14 recess), 3mm LED hole, display aperture; flat face, wordmark laser-engraved after printing |
| insert-tray.stl | rev 2 thickened | 48.50 × 45.50 × 6.60 | XIAO mount: plate thickened 1.0 → **1.6mm**, four Ø2.4mm pegs on a 17.0 × 21.0mm pattern. **Not** a battery cradle — the cell sits loose beside it |
| encoder-knob.stl | upstream | 13.50 × 13.50 × 9.50 | fits 20mm flatted D-shaft |

Interior of the assembled back: **106.64 × ~47.9 × ~18.6 mm**, one open cavity
with four corner bosses. No battery compartment and no sensor mount — both are
open questions for the test fit.

## Changes in case-back-wallmount.stl

- Rear Ø12.8mm panel-mount USB-C hole **plugged** (was center-rear — would
  face the wall).
- New Ø12.8mm USB-C hole through the **left side wall** (y=25, z=10) for the
  female USB-C panel pigtail; cable reaches the XIAO's USB-C internally.
- Two standoff rails (11×44×3mm) on the rear face: 3mm air gap so the rear
  vents breathe against a wall, and flat landings for Command Small strips
  (one per rail).
- Keyhole slots in each rail (**92mm apart after the rev 2 stretch** — was 85;
  measured centre-to-centre at x=7.50 and x=99.50, center height y=30): Ø9.5 entry,
  4.3mm slot rising 8mm, head pocket behind a 2mm retention face. Fits #6
  pan-head screws (head ≤ Ø8.5×2.4mm). Level the two screws; the slot rises
  up, so the case drops onto them.
- Orientation on the wall: slatted side **down** (air inlet), rear vent band
  at the top (outlet).

## rev 4 — wordmark is laser-engraved, not printed

**The rev 3 embossed wordmark was reverted.** Raised lettering is incompatible
with the only good print orientation for this part. The outer face is flat and
the interior bosses rise 8.2mm, so the panel must print outer-face-down: face on
the bed, bosses building upward, no supports. Add 0.6mm of raised lettering to
that face and only **85.6 mm² of letter tops touch the bed** — the entire panel
then starts 0.6mm up over air and needs support across the whole cosmetic
surface. Flipping it over is worse: the plate ends up 8.2mm in the air on boss
pillars. EurekaTec caught this in quoting; the geometry confirms it.

`case-front.stl` is therefore back to flat (2,600 triangles, 110.60 × 53.60 ×
8.20mm, 6.16 cm³, watertight) and the wordmark is applied **after printing, by
laser engraving** — which also reads crisper at 5mm cap height than extruded
plastic ever would. This requires PLA; ABS lasers poorly.

Artwork lives in [`artwork/`](artwork/):

| File | Purpose |
|---|---|
| `wordmark-homecadia.svg` | **vector, send this one.** Glyphs outlined (no live text, no font needed), 36.72 × 5.70mm, 1 user unit = 1mm, filled black = material to remove |
| `wordmark-placement.svg` | vector placement drawing: panel outline, datum lines, wordmark in position. Reference geometry is stroked grey; only the filled wordmark is engraved |
| `wordmark-homecadia-1200dpi.png` | the original raster, black-on-white at 1200 dpi. Superseded by the SVG; kept because it is what the 2026-08-13 quote was based on |
| `wordmark-placement.png` | original dimensioned drawing, datum = lower-left corner of the outer face |

The SVGs are generated from `Ubuntu-B.ttf` at 5.00mm cap height with 0.098mm of
letter-spacing, chosen so the ink box matches the 36.72mm width the raster
artwork and the quote were built around. Two independent renderers (fontTools
and PIL) put untracked Ubuntu Bold at 35.94mm, so the spacing in the original
PNG was deliberate and is reproduced rather than discarded.

**The height is 5.70mm, not the 5.64mm quoted above.** 5.70mm is the true ink
height of `homecadia` in Ubuntu Bold at a 5.00mm cap height — the ascenders of
`h` and `d` rise above the cap line, and there are no descenders. The 5.64
figure appears to be a measurement artifact; the 0.06mm difference is far below
engraving tolerance and does not move the placement datum, which is the
lower-left corner.

Placement: 54.09mm from the left edge, 3.07mm up from the bottom edge, 36.72 ×
5.64mm, Ubuntu Bold, 5.00mm cap height, engraved 0.3–0.5mm deep. That sits in
the solid strip between the aperture's bottom edge and the panel edge, clear of
the vents, encoder and LED hole.

## Wall thickness / DFM

Measured off the meshes, so these are the numbers to answer a vendor DFM flag with:

| Feature | Thickness | Verdict |
|---|---|---|
| `case-back-wallmount` shell walls | **1.44mm** | 3–4 perimeters at a 0.4mm nozzle — normal enclosure wall |
| `case-front` shell walls | **~1.3mm** | same |
| `insert-tray` plate | **1.60mm** (rev 2; was 1.00) | thickened specifically to clear the flag |
| `insert-tray` pegs | Ø2.4mm shank, Ø4.0mm base, 5mm tall | pins, not walls; printable in PLA |

JLC3DP's FDM guidance is a 1.2mm minimum with 2mm ideal, so the shells sit
between the two and trigger "thin walls detected". **Accept it** — a 1.4mm wall
is sound in PLA or ABS, and thickening the shells would mean a true surface
offset in CAD, not the cut-and-shift edits used here.

Sub-millimetre readings do appear in an automated scan (down to 0.13mm) at the
corner bosses and the mating lip. Those are **tangency slivers** where a
cylindrical boss meets a filleted wall, not standalone walls; slicers merge
them into the adjacent perimeter. Verified by cross-section at z=10.

## Material and vendor (current decision)

**FDM PLA, all four parts, at [EurekaTec.ca](https://eurekatec.ca)** — Canadian,
quotes by email, no online quoting system. Quote requested 2026-08-13; a second
request went to Azata.ca the same day.

ABS was the original choice, for heat margin around the LiPo. Two things changed
it: EurekaTec stocks ABS only in black, dark grey and red, and ABS does not
laser engrave cleanly — and the wordmark has to be engraved (see rev 4 above).
PLA gets both the intended colours and the engraving.

The heat argument turned out to be over-cautious. This is an indoor wall sensor
that must already be sited away from sun, radiators and exterior doors or its
readings are meaningless ([bringup.md](../../docs/bringup.md) placement notes),
so the conditions that would soften PLA are ones the device cannot be used in.
PETG remains the ideal middle ground — asked, answer pending.

**One material for all four parts.** Mixing PLA and ABS across two shells that
mate over 116mm introduces a shrinkage mismatch on a fit that already has only
2.1mm of battery clearance.

Avoid SLA resin: brittle, UV-degrading, and it would shear the Ø2.4mm tray pegs
and the M2 self-tapper bosses. If resin is ever forced, use a toughened grade
(8228 or 9000HE), never 9600.

### Vendor evaluation: JLC3DP — rejected for FDM, viable as MJF fallback

Recorded because the constraints are non-obvious and cost a quoting round.

**JLC3DP does not offer PETG** (their FDM lineup is ABS, ASA, PA12-CF, TPU,
PEBA, PEEK, ABS-ESD, PLA-P), and more decisively their FDM process enforces a
**30 × 30 × 10 mm minimum bounding box**, which this design cannot meet:

| Part | Size (mm) | Volume | FDM 30×30×10 | MJF 5×5×5 |
|---|---|---|---|---|
| case-back-wallmount | 116.28 × 59.28 × 23.00 | 19.86 cm³ | passes | passes |
| case-front | 110.60 × 53.60 × **8.20** | 6.16 cm³ | **rejected** (vendor-confirmed) | passes |
| insert-tray | 48.50 × 45.50 × **6.60** | 3.09 cm³ | **rejected** | passes |
| encoder-knob | 13.50 × 13.50 × **9.50** | 1.01 cm³ | **rejected** (vendor-confirmed) | passes |
| | | **30.12 cm³/set, 90.36 cm³ for 3** | | |

Splitting across processes was rejected: FDM holds ±0.3mm per 100mm and SLA
~±0.1mm, and the front and back must mate over a 116mm span with aligned corner
screws. Mixed shrinkage on mating parts trades a size rule for a fit problem.

If JLC3DP is ever needed, **MJF Nylon PA12** is the process that works there:
its 5×5×5mm minimum accepts every part, 0.8mm minimum wall clears our 1.30mm
thinnest, and 0.8mm minimum feature clears the Ø2.4mm tray pegs. PA12 is also a
genuinely good enclosure material — tough, no supports, no scars in the display
aperture. It costs more than FDM, which is why EurekaTec won on price.

The knob prints on any of these routes; a commodity aluminium 6mm D-shaft knob
remains a fine substitute if the printed bore proves too tight to ream.

## Print notes

- 3 units → print each file ×3. PLA, ~0.2mm layers.
- **case-front prints outer-face-down**, no supports — see rev 4 above for why
  this only works with a flat face.
- Back prints rear-face down (rails on bed); keyhole head-pockets bridge
  ~8.8mm — fine with standard supports/bridging.
- Tolerances derived from published dims, **not test-fitted**. Print ONE unit
  first and test-fit the panel, driver board, encoder, USB pigtail and a #6
  screw head before committing all three.
- `case-back-wallmount.stl` carries **33 non-manifold edges** — present in the
  upstream mesh, not introduced by the rev 2 stretch (verified against git
  HEAD). Slicers and print services auto-repair this class of defect; no action
  needed unless one complains.
- `preview.png` shows the rev 1 geometry and is superseded by
  `render-assembled.png`; replace both with a photo once a unit is built.

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
