# Arena files from Printables

Download the 220 mm files from Dave Lack's model (free account needed) and drop them here:

https://www.printables.com/model/338758-golden-1-center-light-the-beam

- `G1C-Beam 220mm` **base**
- `G1C-Beam 220mm` **lid, no hole** (we cut our own 12.4 mm hole for the beam tube in Bambu Studio
  with a negative-part cylinder). The stock light-hole lid has a 14 × 34 mm slot for the author's
  light bar, not a round hole, so it is not a substitute.

These STLs are not committed to this repo (they're the author's work, licensed on Printables).

Name them `base_220.stl` and `lid_220_nohole.stl` here (the holed lid as `lid_220_hole.stl` if
you grabbed it too).

## Measure before printing the plinth

`python3 tools/measure_arena.py` from the repo root slices the base and writes
`models/arena_outline.scad`, which the plinth outline and its top-plate recess are cut to. What it found on the 220 mm file:

- footprint 205.8 × 208.2 mm, D-shaped: flat wall on one side, prow opposite
- solid floor about 1.5 mm thick, with an octagonal well ~165 mm across in the middle (the socket
  goes in its centre) and a solid deck around it
- a wire channel from the well to an opening in the flat wall 55–76 mm right of centre, 3–10 mm
  above the floor
- the lid seats on the inner box at 49.5 mm

Then `make` in `../` to export the STLs, and print `plinth_test.stl` first.
