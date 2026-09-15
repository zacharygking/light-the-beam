// Beam diffuser tube. Print in WHITE PLA in vase (spiralize) mode, 0.4 nozzle, ZERO bottom
// layers (the foot must stay open for the spine and wires) and a 5 mm brim for adhesion.
// The slicer turns this solid into a single-wall tube; the wall diffuses the LED strip inside.
//
//   openscad -o exports/beam_tube.stl beam_tube.scad
//
// The strip spine (strip_holder.scad) slides in from the bottom; the foot flange seats in the
// socket glued to the arena floor. The tip is a 30-degree cone ending in a 3 mm opening: vase
// mode cannot close a dome, and a cone this shallow prints cleanly.

tube_d   = 12;     // outer diameter; the lid hole should be tube_d + 0.4
tube_h   = 246;    // straight section; the cone tip adds 8 -> 254 total, under the P1S 256 mm Z
foot_d   = 16;
foot_h   = 3;

$fn = 96;

union() {
    cylinder(d = foot_d, h = foot_h);
    translate([0, 0, foot_h - 0.01]) cylinder(d1 = foot_d, d2 = tube_d, h = 2);
    translate([0, 0, foot_h + 2 - 0.02]) cylinder(d = tube_d, h = tube_h - foot_h - 2);
    // cone tip, 3 mm opening at the top
    translate([0, 0, tube_h - 0.01]) cylinder(d1 = tube_d, d2 = 3, h = 8);
}
