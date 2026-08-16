// PaperCam enclosure — parametric, for a Bambu A1.
//
//   part = "back"  -> the shell that holds everything
//   part = "front" -> the bezel
//   part = "both"  -> laid out side by side for inspection
//
// FIRST PRINT SHOULD BE test_fit = true. That renders a thin frame carrying
// only the panel recess, the camera hole and the button hole — about fifteen
// minutes of filament instead of several hours — so the measurements can be
// wrong cheaply. Several of them probably are.
//
// CONFIDENCE IN THE NUMBERS
//
//   Panel outline and active area   datasheet, trustworthy
//   Battery 10 x 34 x 50            LP103450 part number, trustworthy
//   Driver board 41 x 25.4          measured. One online spec says 41 x 22,
//                                   with no source. Clearance covers both.
//   Board stack 19mm                measured, and the single number that most
//                                   affects how thick this ends up
//   Camera 6.35 sq, lens 3.2 dia    measured
//   Panel active-area offset        UNKNOWN — assumed centred horizontally,
//                                   see panel_active_y_offset below
//   Board mounting holes            UNKNOWN — using a captive pocket instead
//
// ---------------------------------------------------------------------------

part      = "both";
test_fit  = false;
$fn       = 48;

// --- panel, from the T075A04 datasheet -------------------------------------
panel_w   = 170.2;
panel_h   = 111.2;
panel_t   = 1.1;
active_w  = 163.2;
active_h  = 97.92;

// The datasheet gives both outlines but not where the active area sits inside
// the glass. Horizontally the margins are equal (3.5mm). Vertically there is
// 13.28mm to distribute and the FPC edge almost certainly takes more of it.
// Assumed centred until measured; positive moves the window down.
panel_active_y_offset = 0;

// --- what goes behind -------------------------------------------------------
board_w   = 41.3;    // measured 1-5/8"
board_h   = 25.4;    // measured 1"
board_z   = 19.0;    // PCB back to the top of the XIAO — sets the depth
batt_w    = 50.0;    // LP103450
batt_h    = 34.0;
batt_z    = 10.0;

cam_w     = 6.35;    // module PCB, measured 1/4" square
cam_lens_d    = 3.2;
cam_lens_proud = 3.2;

// --- bezel -----------------------------------------------------------------
// Wider at the top because the camera lives up there, looking out the front
// like a webcam above a monitor.
bezel_side   = 10;
bezel_bottom = 10;
bezel_top    = 26;
bezel_t      = 2.5;

wall     = 2.4;      // 3 perimeters at 0.4mm, prints solid without infill
clear    = 0.4;      // slip fit; tighten to 0.25 if your A1 runs loose

// --- derived ----------------------------------------------------------------
inner_w  = panel_w + 2*clear;
inner_h  = panel_h + bezel_top - bezel_side + 2*clear;
cavity_z = board_z + clear;

outer_w  = inner_w + 2*wall;
outer_h  = panel_h + bezel_top + bezel_bottom;
outer_z  = bezel_t + panel_t + cavity_z + wall;

// Camera sits centred, in the band above the panel.
cam_cx   = outer_w / 2;
cam_cy   = outer_h - bezel_top/2;

// Button on the TOP EDGE, pointing up. M8 panel mount; 8.2 clears the thread.
btn_hole_d = 8.2;
btn_x      = outer_w - bezel_side - 12;

// ---------------------------------------------------------------------------

module rrect(w, h, r, z) {
    linear_extrude(z)
        offset(r=r) offset(r=-r) square([w, h]);
}

// The bezel: a frame with a window for the panel and a hole for the lens.
module front() {
    difference() {
        rrect(outer_w, outer_h, 3, bezel_t);

        // Panel window, sized to the ACTIVE area rather than the glass, so the
        // border and the FPC tail stay hidden behind the bezel.
        translate([(outer_w - active_w)/2,
                   bezel_bottom + (panel_h - active_h)/2 - panel_active_y_offset,
                   -1])
            cube([active_w, active_h, bezel_t + 2]);

        // Lens. Countersunk from the front so the bezel does not vignette a
        // wide-angle lens sitting 2.5mm behind the outer face.
        translate([cam_cx, cam_cy, -1])
            cylinder(d = cam_lens_d + 1.2, h = bezel_t + 2);
        translate([cam_cx, cam_cy, bezel_t - 1.2])
            cylinder(d1 = cam_lens_d + 1.2, d2 = cam_lens_d + 4, h = 1.21);
    }
}

// The shell: panel recess at the front, everything else behind it.
module back() {
    difference() {
        union() {
            rrect(outer_w, outer_h, 3, outer_z);
        }

        // Main cavity.
        translate([wall, wall, bezel_t])
            cube([outer_w - 2*wall, outer_h - 2*wall, outer_z]);

        // Shallow recess so the panel sits flush and cannot slide. Only
        // panel_t + 0.3 deep — the panel is 1.1mm of glass and wants support
        // across its whole back, not a deep pocket to rattle in.
        translate([(outer_w - panel_w)/2 - clear,
                   bezel_bottom - clear,
                   bezel_t])
            cube([panel_w + 2*clear, panel_h + 2*clear, panel_t + 0.3]);

        // Lens hole through the front face of the shell too.
        translate([cam_cx, cam_cy, -1])
            cylinder(d = cam_lens_d + 1.5, h = bezel_t + 2);

        // Button, through the top wall, pointing up.
        translate([btn_x, outer_h + 1, bezel_t + cavity_z/2])
            rotate([90, 0, 0]) cylinder(d = btn_hole_d, h = wall + 2);

        // USB-C on the right wall. Generous: the exact position depends on how
        // the board lands in its pocket, and a slot is easier to file than a
        // hole is to move.
        translate([outer_w - wall - 1, board_pocket_y() + 4, bezel_t + 3])
            cube([wall + 2, 12, 8]);
    }

    // Board pocket: a captive well rather than screw posts, because the
    // mounting hole positions are unmeasured. Add posts here once known.
    translate([0, 0, 0]) board_pocket();

    // Battery pocket, with a slot so a strap or tape can hold it.
    translate([wall + 6, wall + 6, bezel_t + panel_t]) {
        difference() {
            cube([batt_w + 2*wall + 2*clear, batt_h + 2*wall + 2*clear, batt_z/2]);
            translate([wall, wall, -1])
                cube([batt_w + 2*clear, batt_h + 2*clear, batt_z]);
        }
    }
}

function board_pocket_y() = outer_h - bezel_top - board_h - 8;

module board_pocket() {
    translate([outer_w/2 - (board_w + 2*wall + 2*clear)/2,
               board_pocket_y(),
               bezel_t + panel_t]) {
        difference() {
            cube([board_w + 2*wall + 2*clear,
                  board_h + 2*wall + 2*clear, 5]);
            translate([wall, wall, -1])
                cube([board_w + 2*clear, board_h + 2*clear, 7]);
        }
    }
}

// A skeleton carrying only the features that must line up with real hardware.
module test_frame() {
    difference() {
        rrect(outer_w, outer_h, 3, bezel_t);
        translate([(outer_w - active_w)/2,
                   bezel_bottom + (panel_h - active_h)/2 - panel_active_y_offset,
                   -1])
            cube([active_w, active_h, bezel_t + 2]);
        translate([cam_cx, cam_cy, -1])
            cylinder(d = cam_lens_d + 1.2, h = bezel_t + 2);
        translate([btn_x, cam_cy - 14, -1])
            cylinder(d = btn_hole_d, h = bezel_t + 2);
    }
}

// ---------------------------------------------------------------------------

if (test_fit)            test_frame();
else if (part == "front") front();
else if (part == "back")  back();
else {
    front();
    translate([outer_w + 10, 0, 0]) back();
}

echo(str("outer: ", outer_w, " x ", outer_h, " x ", outer_z, " mm"));
echo(str("cavity depth behind panel: ", cavity_z, " mm"));
