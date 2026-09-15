// Plinth for the Golden 1 Center "Light the Beam" model.
//
// A low stand that follows the arena's footprint (offset outward by `margin`), with a sloped
// console on the front that holds the ESP32-2432S028R display board. The console face leans
// back 20° so the screen points up at someone at a desk, and nothing above blocks the view.
//
// Three printed parts, each printable without supports:
//   ring   the walls and the console, printed standing up (open top and bottom)
//   plate  the flat top: arena recess on top, wire slot, a rebate so it drops into the ring
//   lid    the bottom lid, with a tongue under the console
//
//   openscad -o exports/plinth.stl       -D part=\"ring\"  plinth.scad
//   openscad -o exports/plinth_top.stl   -D part=\"plate\" plinth.scad
//   openscad -o exports/plinth_lid.stl   -D part=\"lid\"   plinth.scad
//   openscad -o exports/plinth_test.stl  -D part=\"test\"  plinth.scad   <- console only, ~40 min
//
// The arena footprint comes from arena_outline.scad, written by tools/measure_arena.py from
// the downloaded 220 mm STL. Re-run that script if you print a different size.

include <arena_outline.scad>

part = "ring";            // "ring" | "plate" | "lid" | "test"

// ---- arena (from the measured STL) --------------------------------------------
arena_rot      = 90;      // rotate the measured outline so its flat edge faces the back (+Y)
arena_clear    = 0.8;     // gap around the arena in the recess (RDP chords + elephant's foot)
recess_depth   = 1.2;     // how deep the arena sits into the top plate
// rotate(90) maps (x, y) -> (-y, x): the measured wall opening becomes this point on the back
wire_exit      = [-arena_wire_exit[1], arena_wire_exit[0]];

// ---- plinth ------------------------------------------------------------------
margin         = 10;      // plinth outline = arena outline offset by this
plinth_h       = 56;      // overall height including the top plate
wall           = 3;
top_t          = 4;       // top plate thickness
rebate_h       = 3;       // the plate drops this far into the ring
lid_t          = 2.4;
lid_clear      = 0.3;
ring_h         = plinth_h - top_t + rebate_h;   // ring stands this tall; plate top ends at plinth_h

// ---- CYD board (ESP32-2432S028R) ----------------------------------------------
board_w        = 86.5;    // long axis (left-right in the console)
board_h        = 50.0;    // short axis (up the face)
board_stack    = 12;      // PCB + display module + connectors, pocket depth
usb_plug       = 16;      // room left of the board for the micro-USB plug body (cable attached before sliding in)
screen_w       = 62;      // window opening (active area is 57 x 43; +2.5 mm each side)
screen_h       = 47;
screen_ofs_x   = 7;       // active-area centre vs board centre along the long axis: MEASURE on the real board
screen_ofs_y   = 0;
pocket_clear   = 0.4;

// ---- console -----------------------------------------------------------------
tilt           = 20;      // face lean-back, degrees (0 = vertical)
pocket_w       = board_w + usb_plug + pocket_clear;       // board + plug, side by side
console_w      = pocket_w + 2 * wall;                     // ~108 mm
console_out    = plinth_h * tan(tilt);                    // bottom edge stands this far in front (~20 mm)
console_in     = 24;                                      // how far the console body reaches into the ring
front_y        = -arena_outline_size[0] / 2 - margin;     // y of the front-most point of the plinth outline (the prow)

// ---- cable exit --------------------------------------------------------------
usb_jack_d     = 0;       // 0 = plain cable slot; 18 = hole for a panel-mount USB-C jack
cable_slot_w   = 12;
cable_slot_h   = 8;

$fn = 96;
eps = 0.01;

// ---- outline helpers ---------------------------------------------------------
module footprint(r) { offset(r = r) rotate(arena_rot) polygon(arena_outline); }
module outline_solid(r, h) { linear_extrude(h) footprint(r); }

// ---- console -----------------------------------------------------------------
// Frame on the console face: origin at the face's bottom edge centre. Local +Z runs up the
// face; local +Y points INTO the console (the inward normal, tilted down by `tilt`); the
// window is cut toward local -Y, the pocket lives at local +Y.
module face_frame() {
    translate([0, front_y - console_out, 0]) rotate([-tilt, 0, 0]) children();
}
face_len = plinth_h / cos(tilt);
win_c    = face_len / 2;                          // window centre up the face
board_cx = usb_plug / 2;                          // board sits right of centre; plug room on the left

// Solid wedge on the front: face from (y = front_y - console_out, z = 0) to (y = front_y, z = plinth_h).
module console_solid() {
    rotate([90, 0, 90]) linear_extrude(height = console_w, center = true)
        polygon([[front_y + console_in, 0], [front_y - console_out, 0], [front_y, plinth_h], [front_y + console_in, plinth_h]]);
}

module window_cut() {
    face_frame() translate([board_cx + screen_ofs_x - screen_w / 2, -30, win_c - screen_h / 2 + screen_ofs_y])
        cube([screen_w, 30 + 2.2, screen_h]);
}

module pocket_cut() {
    // board + plug slide up into this from the open bottom; the 2.2 mm front lip holds the glass
    face_frame() translate([-pocket_w / 2, 2.2, -30])
        cube([pocket_w, board_stack, 30 + win_c + board_h / 2 + pocket_clear]);
    // connector relief behind the board (JST pigtails plug in from the back), inside the side walls
    face_frame() translate([-(console_w - 2 * wall) / 2, 2.2 + board_stack - 0.5, -30])
        cube([console_w - 2 * wall, 12, 30 + 16]);
}

// Hollow the console behind the connector relief, parallel to the face, inside the side walls,
// in front of a 3 mm back wall, below the top. Also opens the ring wall behind the console.
module console_hollow() {
    intersection() {
        face_frame() translate([-(console_w - 2 * wall) / 2, 2.2 + board_stack + 11.5, -1]) cube([console_w - 2 * wall, 80, 120]);
        translate([-console_w / 2, front_y - console_out - 1, -eps]) cube([console_w, console_out + console_in - wall + 1, ring_h - rebate_h]);
    }
}

module cable_exit() {
    back_y = arena_outline_size[0] / 2 + margin;   // flat back of the plinth outline
    if (usb_jack_d > 0)
        translate([0, back_y, plinth_h / 2 - 4]) rotate([90, 0, 0]) cylinder(d = usb_jack_d, h = 2 * wall + 2, center = true);
    else
        translate([-cable_slot_w / 2, back_y - wall - 1, -eps]) cube([cable_slot_w, wall + 2, cable_slot_h]);
}

module vents() {
    // two slots through each flank, well away from the console and the cable exit
    for (y = [-10, 30]) translate([-200, y - 1.5, 8]) cube([400, 3, 24]);
}

// ---- parts -------------------------------------------------------------------
module ring() {
    difference() {
        union() {
            outline_solid(margin, ring_h);
            console_solid();
        }
        // hollow, open top and bottom
        translate([0, 0, -eps]) outline_solid(margin - wall, ring_h + 2 * eps);
        // rebate at the top for the plate: thin the wall to half for the top rebate_h
        translate([0, 0, ring_h - rebate_h]) outline_solid(margin - wall / 2, rebate_h + eps);
        // lid rebate at the bottom
        translate([0, 0, -eps]) outline_solid(margin - wall / 2, lid_t + eps);
        window_cut();
        pocket_cut();
        console_hollow();
        cable_exit();
        vents();
    }
}

module plate() {
    difference() {
        union() {
            outline_solid(margin, top_t);                             // 0 .. top_t (printed flat)
            translate([0, 0, -rebate_h]) outline_solid(margin - wall / 2 - lid_clear, rebate_h + eps);   // drops into the ring
        }
        // arena locating recess on top
        translate([0, 0, top_t - recess_depth]) outline_solid(arena_clear, recess_depth + eps);
        // wire slot under the arena's wall opening, straddling the recess edge
        translate([wire_exit[0], wire_exit[1] + 1, -rebate_h - eps])
            hull() { for (dx = [-9, 9]) translate([dx, 0, 0]) cylinder(d = 6, h = rebate_h + top_t + 1); }
        // console top is part of the ring; notch the plate so it clears the console's back block
        translate([-console_w / 2 - eps, front_y - console_out - 1, -rebate_h - eps]) cube([console_w + 2 * eps, console_out + 1 + wall, rebate_h + eps]);
    }
}

module lid() {
    difference() {
        union() {
            translate([0, 0, 0]) outline_solid(margin - wall / 2 - lid_clear, lid_t);
            // friction ring
            translate([0, 0, lid_t - eps]) difference() {
                outline_solid(margin - wall - lid_clear, 3);
                outline_solid(margin - wall - lid_clear - 1.6, 3 + 2 * eps);
            }
            // tongue under the console: carries the board's bottom edge; starts behind the front lip
            translate([-(console_w - 2 * wall) / 2 + 0.5, front_y - console_out + 3, 0]) cube([console_w - 2 * wall - 1, console_out + console_in - 4, lid_t]);
        }
        // finger hole at the back
        translate([0, 40, -eps]) cylinder(d = 14, h = lid_t + 1);
        // notch for the USB cable coming down out of the console
        translate([-pocket_w / 2 - 2, front_y - console_out + 2, -eps]) cube([usb_plug + 2, 14, lid_t + 1]);
    }
}

// Console only, for a fit test: ~40 min print.
module test_slice() {
    intersection() {
        ring();
        translate([-console_w / 2 - 2, front_y - console_out - 2, -eps]) cube([console_w + 4, console_out + console_in + 6, plinth_h + 1]);
    }
}

if (part == "ring") ring();
else if (part == "plate") plate();
else if (part == "lid") lid();
else if (part == "test") test_slice();
