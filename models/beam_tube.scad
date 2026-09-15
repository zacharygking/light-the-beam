// Beam diffuser tube. Print in WHITE PLA in vase (spiralize) mode, 0.4 nozzle, 2 bottom layers.
// The slicer turns this solid into a single-wall tube; the wall diffuses the LED strip inside.
//
//   openscad -o exports/beam_tube.stl beam_tube.scad
//
// The strip spine (strip_holder.scad) slides in from the bottom; the foot flange seats in the
// socket glued to the arena floor.

tube_d   = 12;     // outer diameter; the lid hole should be tube_d + 0.4
tube_h   = 250;    // height above the socket; P1S max is 256
foot_d   = 16;
foot_h   = 3;

$fn = 96;

union() {
    cylinder(d = foot_d, h = foot_h);
    translate([0, 0, foot_h - 0.01]) cylinder(d1 = foot_d, d2 = tube_d, h = 2);
    translate([0, 0, foot_h + 2 - 0.02]) cylinder(d = tube_d, h = tube_h - foot_h - 2);
    // rounded tip
    translate([0, 0, tube_h - 0.01]) scale([1, 1, 0.5]) sphere(d = tube_d);
}
