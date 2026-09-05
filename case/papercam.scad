// PaperCam enclosure — parametric, for a Bambu A1.
//
//   part = "front"    the deep shell: bezel, walls, and everything mounts to it
//   part = "back"     the flat cover, M2 screws into heat-set inserts
//   part = "retainer" small cap that traps the camera against the front face
//   part = "all"      laid out side by side for inspection
//
// PRINT test_fit = true FIRST. A thin skeleton carrying only the three
// features that must line up with real hardware — panel window, lens hole,
// button hole. Minutes of filament instead of hours, and it checks the one
// thing no datasheet answers (see panel_active_y_offset).
//
// CONFIDENCE IN THE INPUTS
//   panel outline / active area   T075A04 datasheet, trustworthy
//   battery 10 x 34 x 50          LP103450 part number, trustworthy
//   board 41.3 x 25.4             measured. One unsourced spec says 41 x 22.
//   board stack 19mm              measured; sets the whole depth
//   camera 6.35 sq, lens 3.2      measured
//   cam_body_t                    ESTIMATED (6.35 total - 3.2 lens). Measure.
//   cam_ribbon_len                UNKNOWN. Sets how high the board sits.
//   panel_active_y_offset         UNKNOWN. Datasheet gives both outlines but
//                                 never says where the active area sits in
//                                 the glass. Assumed centred.
//   board hole positions          UNKNOWN. Posts sit at the corners of the
//                                 measured footprint; nudge once seen.
// ---------------------------------------------------------------------------

part     = "all";
test_fit = false;
$fn      = 48;

// --- panel (datasheet) ------------------------------------------------------
panel_w  = 170.2;  panel_h = 111.2;  panel_t = 1.1;
active_w = 163.2;  active_h = 97.92;
panel_active_y_offset = 0;   // +ve moves the window down

// --- internals --------------------------------------------------------------
board_w  = 25.4;   // MOUNTED VERTICALLY: 25.4 across, 41.3 up
board_h  = 41.3;
board_z  = 19.0;   // PCB face to top of XIAO — dominates the depth
board_pcb_standoff = 3;      // posts lift the PCB off the panel back

batt_w   = 34.0;   // also vertical: 34 across, 50 up
batt_h   = 50.0;
batt_z   = 10.0;
batt_cord_gap = 10;          // notch at the pocket's top right for the lead

cam_w        = 6.35;         // module PCB, square
cam_body_t   = 3.15;         // PCB + sensor body, NO lens.  <-- measure this
cam_lens_d   = 3.2;
cam_ribbon_len = 25;         // <-- measure this; board top is placed from it

// --- shell ------------------------------------------------------------------
bezel_side = 10;  bezel_bottom = 10;  bezel_top = 26;
bezel_t    = 2.5;            // front face thickness
back_t     = 2.4;            // cover thickness
wall       = 2.4;
clear      = 0.4;

// M2 heat-set inserts: 3.2mm bore is the usual for a brass M2 insert.
insert_d   = 3.2;
insert_z   = 4.0;
screw_d    = 2.4;            // clearance in the back cover
boss_d     = 6.0;

// --- derived ----------------------------------------------------------------
outer_w  = panel_w + 2*clear + 2*wall;
outer_h  = panel_h + bezel_top + bezel_bottom;
cavity_z = board_pcb_standoff + board_z + clear;
outer_z  = bezel_t + panel_t + cavity_z;      // front shell depth, back sits on top

panel_x  = (outer_w - panel_w)/2 - clear;
panel_y  = bezel_bottom - clear;
panel_back_z = bezel_t + panel_t;             // inner face of the glass

cam_cx = outer_w/2;
cam_cy = outer_h - bezel_top/2;

// Board hangs from just under the top bezel so the ribbon can reach the lens.
// Drop it lower and the ribbon has to stretch; raise it and it fouls the
// camera pocket. Driven directly by cam_ribbon_len.
board_top_y = min(outer_h - bezel_top - 3,
                  cam_cy - cam_ribbon_len + 8);
board_y     = board_top_y - board_h;
board_x     = cam_cx - board_w/2;             // centred, so the ribbon runs straight up

// Battery to the LEFT of the board looking at the front.
batt_x = board_x - 6 - batt_w;
batt_y = board_top_y - batt_h;

btn_d = 8.2;                                   // M8 panel mount
btn_x = outer_w - bezel_side - 12;

// Back-cover screw positions.
sx = [wall + boss_d/2 + 1, outer_w - wall - boss_d/2 - 1];
sy = [wall + boss_d/2 + 1, outer_h/2, outer_h - wall - boss_d/2 - 1];

// ---------------------------------------------------------------------------

module rrect(w, h, r, z) { linear_extrude(z) offset(r=r) offset(r=-r) square([w,h]); }

module cavity() {
    translate([wall, wall, bezel_t])
        cube([outer_w - 2*wall, outer_h - 2*wall, outer_z]);
}

// The window shows the ACTIVE area; the rebate accepts the whole glass, so
// the border and the FPC tail hide behind the bezel.
module window() {
    translate([(outer_w - active_w)/2,
               bezel_bottom + (panel_h - active_h)/2 - panel_active_y_offset, -1])
        cube([active_w, active_h, bezel_t + 2]);
}

module panel_rebate() {
    translate([panel_x, panel_y, bezel_t])
        cube([panel_w + 2*clear, panel_h + 2*clear, panel_t]);
}

// Stepped bore: lens through the face, body pocket behind it. The module drops
// in from inside and seats against the step, so the lens position is set by
// the front face rather than by where the board happens to land.
module camera_bore() {
    translate([cam_cx, cam_cy, -1]) cylinder(d = cam_lens_d + 0.8, h = bezel_t + 2);
    translate([cam_cx - (cam_w + 2*clear)/2, cam_cy - (cam_w + 2*clear)/2, bezel_t])
        cube([cam_w + 2*clear, cam_w + 2*clear, cam_body_t + clear]);
}

module button_hole() {
    translate([btn_x, outer_h + 1, bezel_t + cavity_z/2])
        rotate([90,0,0]) cylinder(d = btn_d, h = wall + 2);
}

module usb_slot() {
    translate([outer_w - wall - 1, board_y + 6, panel_back_z + board_pcb_standoff + 2])
        cube([wall + 2, 14, 9]);
}

// --- things that stand up inside the shell ---------------------------------

module board_posts(bore = false) {
    inset = 3;
    for (px = [board_x + inset, board_x + board_w - inset])
        for (py = [board_y + inset, board_y + board_h - inset])
            translate([px, py, panel_back_z])
                if (bore) translate([0,0,board_pcb_standoff - insert_z])
                              cylinder(d = insert_d, h = insert_z + 1);
                else      cylinder(d = 5, h = board_pcb_standoff);
}

// Open-topped pocket. Velcro holds the cell; this only stops it sliding.
module battery_pocket() {
    translate([batt_x - wall, batt_y - wall, panel_back_z])
        difference() {
            cube([batt_w + 2*wall + 2*clear, batt_h + 2*wall + 2*clear, batt_z]);
            translate([wall, wall, -1]) cube([batt_w + 2*clear, batt_h + 2*clear, batt_z + 2]);
        }
}

module battery_cord_gap() {
    translate([batt_x + batt_w - batt_cord_gap + clear, batt_y + batt_h - 1, panel_back_z + 2])
        cube([batt_cord_gap + wall + 2, wall + 3, batt_z]);
}

// Two bosses flanking the camera pocket; the retainer bar screws to them.
module camera_bosses(bore = false) {
    for (dx = [-1, 1])
        translate([cam_cx + dx * (cam_w/2 + 4), cam_cy, bezel_t])
            if (bore) translate([0,0,cam_body_t + clear])
                          cylinder(d = insert_d, h = insert_z + 1);
            else      cylinder(d = boss_d, h = cam_body_t + clear + insert_z);
}

module corner_bosses(bore = false) {
    for (x = sx) for (y = sy)
        translate([x, y, bezel_t])
            if (bore) translate([0,0,outer_z - bezel_t - insert_z])
                          cylinder(d = insert_d, h = insert_z + 1);
            else      cylinder(d = boss_d, h = outer_z - bezel_t);
}

// ---------------------------------------------------------------------------

module front() {
    difference() {
        union() {
            difference() { rrect(outer_w, outer_h, 3, outer_z); cavity(); }
            board_posts();
            battery_pocket();
            camera_bosses();
            corner_bosses();
        }
        window();
        panel_rebate();
        camera_bore();
        button_hole();
        usb_slot();
        battery_cord_gap();
        board_posts(bore = true);
        camera_bosses(bore = true);
        corner_bosses(bore = true);
    }
}

module back() {
    difference() {
        rrect(outer_w, outer_h, 3, back_t);
        for (x = sx) for (y = sy)
            translate([x, y, -1]) {
                cylinder(d = screw_d, h = back_t + 2);
                // countersink so the cover sits flat on a shelf
                translate([0,0,back_t - 1.2 + 1])
                    cylinder(d1 = screw_d, d2 = screw_d + 2.2, h = 1.21);
            }
        // no camera hole here — front only
    }
}

module retainer() {
    l = cam_w + 2*4 + boss_d;
    difference() {
        translate([-l/2, -boss_d/2, 0]) rrect(l, boss_d, 1.5, 2.4);
        for (dx = [-1,1]) translate([dx*(cam_w/2 + 4), 0, -1]) cylinder(d = screw_d, h = 5);
    }
}

module test_frame() {
    difference() {
        rrect(outer_w, outer_h, 3, bezel_t);
        window();
        translate([cam_cx, cam_cy, -1]) cylinder(d = cam_lens_d + 0.8, h = bezel_t + 2);
        translate([btn_x, cam_cy - 14, -1]) cylinder(d = btn_d, h = bezel_t + 2);
    }
}

// ---------------------------------------------------------------------------

if (test_fit)                 test_frame();
else if (part == "front")     front();
else if (part == "back")      back();
else if (part == "retainer")  retainer();
else {
    front();
    translate([outer_w + 10, 0, 0]) back();
    translate([outer_w + 10, outer_h + 15, 0]) retainer();
}

echo(str("outer ", outer_w, " x ", outer_h, " x ", outer_z + back_t, " mm"));
echo(str("board at y ", board_y, "..", board_top_y, "  battery x ", batt_x));
