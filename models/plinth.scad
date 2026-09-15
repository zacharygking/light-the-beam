// Plinth for the Golden 1 Center "Light the Beam" model.
//
// A low drum the 220 mm arena sits on. Holds the ESP32-2432S028R display board behind a
// tilted window on the front, the WAGO lever nuts, and a cable exit on the back.
//
// Print upside down (flat top on the bed) in one piece; the bottom lid prints separately.
//   openscad -o exports/plinth.stl -D part=\"body\" plinth.scad
//   openscad -o exports/plinth_lid.stl -D part=\"lid\" plinth.scad
//   openscad -o exports/plinth_test.stl -D part=\"test\" plinth.scad   <- front slice, ~40 min
//
// MEASURE FIRST: arena_d is the arena's footprint at its floor. Set it from the
// downloaded 220 mm STL (Bambu Studio > Measure) before printing the body.

part = "body";            // "body" | "lid" | "test"

// ---- arena (measure these) ---------------------------------------------------
arena_d        = 220;     // footprint diameter of the arena base at the floor
arena_wire_a   = 180;     // angle (deg) where the arena's wire hole is; 0 = front, 180 = back
recess_depth   = 2;       // how deep the arena sits into the top plate

// ---- plinth ------------------------------------------------------------------
plinth_d       = arena_d + 10;
plinth_h       = 50;
wall           = 3;
top_t          = 3;       // top plate thickness
lid_t          = 2.4;
lid_clear      = 0.3;

// ---- CYD board (ESP32-2432S028R) ----------------------------------------------
board_w        = 86.5;    // long axis
board_h        = 50.0;
board_t        = 1.6;     // PCB
board_stack    = 12;      // PCB + display module + connectors, pocket depth
screen_w       = 60;      // window opening (active area is 57 x 43; +1.5 mm each side)
screen_h       = 46;
screen_ofs_x   = 1.5;     // active-area centre vs board centre along the long axis, tune after test print
screen_ofs_y   = 0;
tilt           = 20;      // window tilt back, degrees
pocket_clear   = 0.4;

// ---- cable exit --------------------------------------------------------------
usb_jack_d     = 0;       // 0 = plain cable slot; 18 = hole for a panel-mount USB-C jack
cable_slot_w   = 12;
cable_slot_h   = 8;

$fn = 180;
eps = 0.01;

module drum() {
    difference() {
        cylinder(d = plinth_d, h = plinth_h);
        // hollow, open at the bottom
        translate([0, 0, -eps]) cylinder(d = plinth_d - 2 * wall, h = plinth_h - top_t + eps);
        // arena locating recess in the top plate
        translate([0, 0, plinth_h - recess_depth]) cylinder(d = arena_d + 0.6, h = recess_depth + eps);
        // wire pass-through under the arena's wire hole
        rotate([0, 0, arena_wire_a]) translate([arena_d / 2 - 12, 0, plinth_h - top_t - eps])
            hull() { cylinder(d = 8, h = top_t + 1); translate([-10, 0, 0]) cylinder(d = 8, h = top_t + 1); }
        // lid rebate so the lid sits flush
        translate([0, 0, -eps]) cylinder(d = plinth_d - 2 * wall + 2 * 1.2, h = lid_t + eps);
    }
}

// The window and the cradle share one tilted frame on the front (-Y) of the drum.
// Origin of the frame: centre of the screen opening, on the outer surface.
win_z = plinth_h / 2 - 2;

module tilted_frame() {
    translate([0, -plinth_d / 2, win_z]) rotate([-tilt, 0, 0]) children();
}

module cradle_block() {
    // solid block that the pocket is cut from, fused into the drum wall
    tilted_frame() translate([-(board_w + 8) / 2, -6, -(board_h + 8) / 2])
        cube([board_w + 8, board_stack + 10, board_h + 8]);
}

module window_cut() {
    tilted_frame() translate([-screen_w / 2 + screen_ofs_x, -20, -screen_h / 2 + screen_ofs_y])
        cube([screen_w, 20 + 2.2, screen_h]);
}

module pocket_cut() {
    // the board slides up into this from below; the front lip holds the glass 2.2 mm back
    tilted_frame() translate([-(board_w + pocket_clear) / 2, 2.2, -(board_h + pocket_clear) / 2])
        cube([board_w + pocket_clear, board_stack, board_h + pocket_clear + 40]);   // open downward
    // connector relief along the bottom edge (USB, JST pigtails)
    tilted_frame() translate([-(board_w + 12) / 2, 2.2 + board_stack - 0.5, -board_h / 2 - 40])
        cube([board_w + 12, 12, 40 + 6]);
}

module cable_exit() {
    // back (+Y)
    if (usb_jack_d > 0)
        translate([0, plinth_d / 2, plinth_h / 2 - 4]) rotate([90, 0, 0]) cylinder(d = usb_jack_d, h = 2 * wall + 2, center = true);
    else
        translate([-cable_slot_w / 2, plinth_d / 2 - wall - 1, -eps]) cube([cable_slot_w, wall + 2, cable_slot_h]);
}

module vents() {
    for (a = [30, 60, 120, 150]) rotate([0, 0, a])
        translate([plinth_d / 2 - wall - 1, -1.5, 6]) cube([wall + 2, 3, 26]);
    for (a = [-30, -60, -120, -150]) rotate([0, 0, a])
        translate([plinth_d / 2 - wall - 1, -1.5, 6]) cube([wall + 2, 3, 26]);
}

module body() {
    difference() {
        union() {
            drum();
            intersection() {   // keep the cradle inside the drum's outer skin
                cradle_block();
                cylinder(d = plinth_d - 0.2, h = plinth_h);
            }
        }
        window_cut();
        pocket_cut();
        cable_exit();
        vents();
        // WAGO pocket floor marker: a shallow tray on the inside of the top plate at the back
        rotate([0, 0, arena_wire_a]) translate([arena_d / 2 - 60, -12, plinth_h - top_t - 1.2 + eps]) cube([40, 24, 1.2]);
    }
}

module lid() {
    d = plinth_d - 2 * wall + 2 * 1.2 - 2 * lid_clear;
    difference() {
        union() {
            cylinder(d = d, h = lid_t);
            // friction ring
            translate([0, 0, lid_t - eps]) difference() {
                cylinder(d = d - 2.4, h = 3);
                translate([0, 0, -eps]) cylinder(d = d - 2.4 - 3.2, h = 3 + 2 * eps);
            }
            // bump that presses the board up against the window lip
            translate([-30, -plinth_d / 2 + wall + 4, lid_t - eps]) cube([60, 14, 8]);
        }
        // finger hole
        translate([0, 40, -eps]) cylinder(d = 14, h = lid_t + 1);
    }
}

// Front slice for a fit test: just the window + cradle region, 60 mm wide.
module test_slice() {
    intersection() {
        body();
        translate([-32, -plinth_d / 2 - 1, -eps]) cube([64, 40, plinth_h + 1]);
    }
}

if (part == "body") body();
else if (part == "lid") lid();
else if (part == "test") test_slice();
