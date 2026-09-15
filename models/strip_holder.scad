// Two small parts for the beam:
//   spine  - a thin bar the WS2812B strip sticks to, slid up inside the tube (strip faces
//            sideways so light bounces around the wall instead of shining straight up)
//   socket - a puck glued to the arena floor; the tube's foot drops in, wires exit the slot
//
//   openscad -o exports/beam_spine.stl  -D part=\"spine\"  strip_holder.scad
//   openscad -o exports/beam_socket.stl -D part=\"socket\" strip_holder.scad

part = "spine";

tube_id   = 12 - 2 * 0.45;   // inner diameter of the vase-mode tube (one 0.45 mm wall)
strip_w   = 10;              // WS2812B 60/m strip is 10 mm wide
led_count = 14;              // 14 x 16.67 = 233 mm of strip; 15 would not fit inside the 250 mm tube
led_pitch = 16.67;           // 60 LEDs per metre
spine_t   = 1.6;
spine_h   = 245;             // must be shorter than the tube interior (250 - foot 3 - tip): strip is 233 + wire notch

foot_d    = 16;
socket_d  = 30;
socket_h  = 8;

$fn = 96;

module spine() {
    w = min(strip_w, tube_id - 0.6);
    difference() {
        union() {
            // bar
            translate([-w / 2, -spine_t / 2, 0]) cube([w, spine_t, spine_h]);
            // feet: 13 mm wide, wider than the 11 mm bore, so they seat in the tube's 16->12 mm cone
            translate([-13 / 2, -spine_t / 2 - 1.5, 0]) cube([13, spine_t + 3, 4]);
        }
        // wire notch at the bottom
        translate([-2, -3, -0.01]) cube([4, 6, 6]);
    }
}

module socket() {
    difference() {
        cylinder(d = socket_d, h = socket_h);
        translate([0, 0, 2]) cylinder(d = foot_d + 0.4, h = socket_h);
        // wire slot out the side and down through the floor
        translate([0, -3, 2]) cube([socket_d, 6, socket_h]);
        translate([0, 0, -0.01]) cylinder(d = 8, h = 3);
    }
}

if (part == "spine") spine();
else socket();
