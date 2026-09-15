# Arena files from Printables

Download the 220 mm files from Dave Lack's model (free account needed) and drop them here:

https://www.printables.com/model/338758-golden-1-center-light-the-beam

- `G1C-Beam 220mm` **base**
- `G1C-Beam 220mm` **lid, no hole** (we cut our own 12.4 mm hole for the beam tube in Bambu Studio
  with a negative-part cylinder; or measure the stock light-hole lid, it may already fit)

These STLs are not committed to this repo (they're the author's work, licensed on Printables).

## Measure before printing the plinth

In Bambu Studio, open the base and use the Measure tool to get:

1. Footprint diameter at the floor → `arena_d` in `../plinth.scad`
2. Angle of the wire hole around the base (front = 0°) → `arena_wire_a`
3. Whether the floor is open or solid (if solid, the beam socket's wire hole needs a matching
   hole drilled or cut in the floor)

Then `make` in `../` to export the STLs, and print `plinth_test.stl` first.
