// Plinth for the Golden 1 Center "Light the Beam" model.
//
// A low drum the 220 mm arena sits on, with a small sloped console on the front that holds
// the ESP32-2432S028R display board. The console face leans back 20° so the screen points up
// at someone sitting at a desk, and nothing above it blocks the view. Inside the drum: the
// WAGO lever nuts and the cable exit at the back.
//
// Print upside down (flat top on the bed) in one piece; the bottom lid prints separately.
//   openscad -o exports/plinth.stl -D part=\"body\" plinth.scad
//   openscad -o exports/plinth_lid.stl -D part=\"lid\" plinth.scad
//   openscad -o exports/plinth_test.stl -D part=\"test\" plinth.scad   <- console only, ~40 min
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
board_w        = 86.5;    // long axis (left-right)
board_h        = 50.0;    // short axis (up-down in the console)
board_stack    = 12;      // PCB + display module + connectors, pocket depth
screen_w       = 60;      // window opening (active area is 57 x 43; +1.5 mm each side)
screen_h       = 46;
screen_ofs_x   = 1.5;     // active-area centre vs board centre along the long axis, tune after test print
screen_ofs_y   = 0;
pocket_clear   = 0.4;

// ---- console -----------------------------------------------------------------
tilt           = 20;      // face lean-back, degrees (0 = vertical)
console_w      = board_w + 10;               // 96.5 mm wide
console_out    = plinth_h * tan(tilt);       // how far the bottom edge stands in front of the drum (~18 mm)
console_in     = 24;                         // how far the console body reaches into the drum

// ---- cable exit --------------------------------------------------------------
usb_jack_d     = 0;       // 0 = plain cable slot; 18 = hole for a panel-mount USB-C jack
cable_slot_w   = 12;
cable_slot_h   = 8;

$fn = 180;
eps = 0.01;
R = plinth_d / 2;

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

// Solid console: a wedge on the front (-Y) of the drum. Its face runs from (y = -R - console_out)
// at the floor to (y = -R) at the top, i.e. leaning back by `tilt`. Extruded along X.
module console_solid() {
    rotate([90, 0, 90]) linear_extrude(height = console_w, center = true)
        polygon([[-R + console_in, 0], [-R - console_out, 0], [-R, plinth_h], [-R + console_in, plinth_h]]);
}

// Frame on the console face: origin at the face's bottom edge centre, local +Z runs up the
// face, local +Y is the outward normal (tilted up by `tilt`).
module face_frame() {
    translate([0, -R - console_out, 0]) rotate([-tilt, 0, 0]) children();
}
face_len = plinth_h / cos(tilt);     // length of the sloped face
win_c    = face_len / 2 + 1;         // window centre up the face

module window_cut() {
    face_frame() translate([-screen_w / 2 + screen_ofs_x, -20, win_c - screen_h / 2 + screen_ofs_y])
        cube([screen_w, 20 + 2.2, screen_h]);
}

module pocket_cut() {
    // the board slides up into this from the open bottom; the 2.2 mm front lip holds the glass
    face_frame() translate([-(board_w + pocket_clear) / 2, 2.2, -30])
        cube([board_w + pocket_clear, board_stack, 30 + win_c + board_h / 2 + pocket_clear]);
    // connector relief behind the board's bottom edge (USB, JST pigtails)
    face_frame() translate([-(board_w + 12) / 2, 2.2 + board_stack - 0.5, -30])
        cube([board_w + 12, 12, 30 + 14]);
    // keep the console hollow behind the pocket so it doesn't eat filament
    translate([-console_w / 2 + wall, -R - console_out + 2, -eps]) cube([console_w - 2 * wall, console_out + console_in - 2 * wall, plinth_h - top_t]);
}

module cable_exit() {
    if (usb_jack_d > 0)
        translate([0, R, plinth_h / 2 - 4]) rotate([90, 0, 0]) cylinder(d = usb_jack_d, h = 2 * wall + 2, center = true);
    else
        translate([-cable_slot_w / 2, R - wall - 1, -eps]) cube([cable_slot_w, wall + 2, cable_slot_h]);
}

module vents() {
    for (a = [60, 120, 240, 300]) rotate([0, 0, a])
        translate([R - wall - 1, -1.5, 6]) cube([wall + 2, 3, 26]);
}

module body() {
    difference() {
        union() { drum(); console_solid(); }
        window_cut();
        pocket_cut();
        cable_exit();
        vents();
        // the console must not close off the drum interior: open the drum wall behind it
        translate([-(board_w + 12) / 2, -R - 1, -eps]) cube([board_w + 12, wall + 2, plinth_h - top_t]);
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
            // tongue that reaches forward under the console, with a bump that presses the board up
            translate([-(board_w - 10) / 2, -R - console_out + 1, 0]) cube([board_w - 10, console_out + console_in - 2, lid_t]);
            translate([-30, -R - console_out + 4, lid_t - eps]) cube([60, 10, 6]);
        }
        // finger hole
        translate([0, 40, -eps]) cylinder(d = 14, h = lid_t + 1);
    }
}

// Console only, for a fit test: ~40 min print.
module test_slice() {
    intersection() {
        body();
        translate([-console_w / 2 - 2, -R - console_out - 2, -eps]) cube([console_w + 4, console_out + console_in + 6, plinth_h + 1]);
    }
}

if (part == "body") body();
else if (part == "lid") lid();
else if (part == "test") test_slice();
