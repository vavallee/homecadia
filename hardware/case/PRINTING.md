# Printing the rev 5 back

One part: **`stl/case-back-wallmount.stl`**. The front, tray and knob from the
earlier print are reused; only the back changed.

## The part

| | |
|---|---|
| Size | 116.3 × 59.3 × 31.0 mm (fits any bed) |
| Plastic | about 27 cm³ solid, so roughly 30–35 g of PLA |
| Material | **PLA**, black or dark grey. PETG also works |
| Copies | 1 for now (a test fit). Two more later if it fits |

## Orientation

Print it **as the file comes in**: the flat back with the two rails on the bed,
the open side facing up. Do not rotate it. The slicer drops it to the bed on
the rails.

## Settings (any slicer: PrusaSlicer, Orca, Bambu Studio, Cura)

| Setting | Value | Why |
|---|---|---|
| Nozzle | 0.4 mm | |
| Layer height | 0.2 mm | |
| Walls / perimeters | **3** | the shell walls are 1.44 mm: three 0.45 mm lines plus the slicer's gap fill make them solid |
| Top / bottom layers | 5 / 5 | |
| Infill | 20 %, any pattern | the walls are thinner than the infill would matter |
| Supports | **build plate only** | the keyhole pockets inside the two rails need it; nothing inside the box does |
| Brim | 5 mm | the part is 116 mm long; a brim stops the corners lifting |
| Speed | the slicer's normal PLA profile | |

The slicer may warn about **33 non-manifold edges** or offer to repair the
mesh. Accept the repair. The same 33 edges are in the original model, and the
part has been printed from it before.

## Checks after printing

1. **Height:** 31.0 mm from the bed side of the rails to the top rim.
2. **Inside depth:** about 26.6 mm from the inside floor to the rim.
3. **USB-C hole:** the round hole in the short side wall, 12.8 mm across. The
   USB-C panel connector should push through and its nut thread on.
4. **Front fit:** the existing front should sit on the rim the same way it sat
   on the old back. Nothing about the rim changed.
5. **Keyhole slots:** clear any support out of the two slots in the rails; a
   #6 pan-head screw head should drop into the round end and slide up.

## What changed from the last print

The old back was 23 mm tall and too shallow: the 2000 mAh cell and the
board stack did not fit together. This one is the same box raised 8 mm, cut
above the USB-C hole and joined with a straight band, so everything else
(holes, rails, rim, corner screw posts) is where it was.
