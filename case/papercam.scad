// PaperCam enclosure — parametric, for a Bambu A1.
//
//   part = "shell"  the front: bezel, walls, and every opening
//   part = "back"   flat cover, M2 into heat-set inserts
//   part = "both"   side by side for inspection
//
// PRINT test_fit = true FIRST — a thin skeleton with only the panel window,
// lens hole and button hole. Minutes of filament, and it settles the one thing
// no datasheet states: where the active area sits inside the glass.
//
// ---------------------------------------------------------------------------
// DESIGN NOTE: why there is almost nothing inside
//
// Earlier versions grew board posts, a battery pocket and a camera retainer.
// All of that has gone. Printed mounts earn their keep in production; for one
// unit each is just another measurement that can be wrong, and every wrong one
// costs a three-hour print.
//
// Everything inside is held by foam tape and Velcro. The important consequence
// is that THE CAMERA ALIGNS TO THE HOLE, not the hole to the camera: hold the
// module against the inside of the front face, centred in the bore, stick it
// down. That single inversion removes every hard constraint this design was
// fighting — ribbon length, where the camera sits on the board, whether the
// board lives in the top band or behind the panel. All of it becomes a choice
// made with the parts in your hands.
//
// Board posts survive as an option, off by default, from measured holes:
// 20.5mm centres both pairs, 2.25mm and 18mm up from the bottom edge, 2.5mm
// diameter. Turn them on if the tape ever annoys you.
//
// CONFIDENCE
//   panel outline / active area   T075A04 datasheet, trustworthy
//   board 41.3 x 25.4             measured; an unsourced spec says 41 x 22
//   board hole positions          measured
//   panel borders                 MEASURED: 3mm plain edges, 10.28mm FPC edge.
//                                 The active area is not centred in the glass.
// ---------------------------------------------------------------------------

part     = "both";
test_fit = false;
$fn      = 48;

// --- panel (datasheet) ------------------------------------------------------
panel_w  = 170.2;  panel_h = 111.2;  panel_t = 1.1;
active_w = 163.2;  active_h = 97.92;

// MEASURED, and the active area is NOT centred in the glass. The datasheet
// gives both outlines and never relates them; 13.28mm of vertical border turns
// out to be 3mm on the plain long edge and 10.28mm on the FPC edge. Assuming
// it centred put the window 3.6mm too high.
//
// Ribbon at the TOP in landscape, so the fat border is the top one.
panel_border_side   = (panel_w - active_w) / 2;        // 3.5, equal per datasheet
panel_border_bottom = 3;                               // measured
panel_border_top    = panel_h - active_h - panel_border_bottom;   // 10.28

// How far the glass sinks into the shell. Deeper than the glass itself so
// there is a definite lip to locate against and room for tape behind it.
panel_rebate_depth  = panel_t + 0.8;

// --- shell ------------------------------------------------------------------
bezel_side   = 10;
bezel_bottom = 10;
bezel_top    = 26;                // camera lives in this band
bezel_t      = 2.5;
back_t       = 2.4;
wall         = 2.4;
clear        = 0.4;

// Deep enough for the tallest thing inside — the board stack at 19mm — plus a
// little air. Nothing is mounted to a fixed height, so this is the only depth
// number that matters.
inner_depth  = 21;

// --- openings ---------------------------------------------------------------
cam_lens_d = 3.2;                 // lens barrel
cam_hole_d = cam_lens_d + 1.0;    // generous: you position the camera to this
btn_d      = 8.2;                 // M8 panel mount
usb_w      = 16;                  // slot, not a hole — easy to file, hard to move
usb_h      = 9;

// --- fasteners --------------------------------------------------------------
insert_d = 3.2;                   // brass M2 heat-set
insert_z = 4.0;
screw_d  = 2.4;
boss_d   = 6.5;

// --- optional board posts (off) ---------------------------------------------
use_board_posts = false;
bp_pitch      = 20.5;             // measured, both pairs
bp_bottom_y   = 2.25;             // hole centre from board's bottom edge
bp_middle_y   = 18;
bp_hole_d     = 2.5;
board_org     = [60, 40];         // where the board's bottom-left corner sits
post_h        = 4;

// --- derived ----------------------------------------------------------------
outer_w = panel_w + 2*clear + 2*wall;
outer_h = panel_h + bezel_top + bezel_bottom;
outer_z = bezel_t + panel_t + inner_depth;

cam_cx = outer_w/2;
cam_cy = outer_h - bezel_top/2;
btn_x  = outer_w - bezel_side - 12;

// Five, not six. A boss at top-centre landed exactly on the camera, which is
// also at top-centre — so that one is gone and the top keeps only its corners.
bx0 = wall + boss_d/2 + 1;
bx1 = outer_w - wall - boss_d/2 - 1;
by0 = wall + boss_d/2 + 1;
by1 = outer_h - wall - boss_d/2 - 1;
screw_pos = [[bx0, by0], [outer_w/2, by0], [bx1, by0], [bx0, by1], [bx1, by1]];

// ---------------------------------------------------------------------------

module rrect(w, h, r, z) { linear_extrude(z) offset(r=r) offset(r=-r) square([w,h]); }

// Placed from the glass's own borders, not from its centre.
module window() {
    translate([(outer_w - panel_w)/2 + panel_border_side,
               bezel_bottom + panel_border_bottom, -1])
        cube([active_w, active_h, bezel_t + 2]);
}

// Takes the whole glass, so the border and FPC tail hide behind the bezel.
// Slightly deeper than the glass so it drops in against a definite lip with
// room for tape behind, rather than sitting proud.
module panel_rebate() {
    translate([(outer_w - panel_w)/2 - clear, bezel_bottom - clear, bezel_t])
        cube([panel_w + 2*clear, panel_h + 2*clear, panel_rebate_depth]);
}

// Countersunk from the front so the bezel cannot vignette a wide lens sitting
// 2.5mm behind the outer face.
module camera_hole() {
    translate([cam_cx, cam_cy, -1]) cylinder(d = cam_hole_d, h = bezel_t + 2);
    translate([cam_cx, cam_cy, bezel_t - 1.2])
        cylinder(d1 = cam_hole_d, d2 = cam_hole_d + 4, h = 1.21);
}

module button_hole() {
    translate([btn_x, outer_h + 1, bezel_t + inner_depth/2])
        rotate([90,0,0]) cylinder(d = btn_d, h = wall + 2);
}

// Right wall, roughly mid-height. Deliberately oversized — the board goes
// wherever it fits, so this has to accept a range of positions.
module usb_slot() {
    translate([outer_w - wall - 1, outer_h/2 - usb_w/2, bezel_t + 4])
        cube([wall + 2, usb_w, usb_h]);
}

module bosses(bore = false) {
    for (pos = screw_pos)
        translate([pos[0], pos[1], bezel_t])
            if (bore) translate([0,0,outer_z - bezel_t - insert_z])
                          cylinder(d = insert_d, h = insert_z + 1);
            else      cylinder(d = boss_d, h = outer_z - bezel_t);
}

module board_posts(bore = false) {
    for (dx = [0, bp_pitch]) for (dy = [bp_bottom_y, bp_middle_y])
        translate([board_org[0] + dx, board_org[1] + dy, bezel_t + panel_t])
            if (bore) cylinder(d = bp_hole_d - 0.3, h = post_h + 1);   // self-tapping
            else      cylinder(d = 5, h = post_h);
}

// ---------------------------------------------------------------------------

module shell() {
    difference() {
        union() {
            difference() {
                rrect(outer_w, outer_h, 3, outer_z);
                translate([wall, wall, bezel_t])
                    cube([outer_w - 2*wall, outer_h - 2*wall, outer_z]);
            }
            bosses();
            if (use_board_posts) board_posts();
        }
        window();
        panel_rebate();
        camera_hole();
        button_hole();
        usb_slot();
        bosses(bore = true);
        if (use_board_posts) board_posts(bore = true);
    }
}

module back() {
    difference() {
        rrect(outer_w, outer_h, 3, back_t);
        for (pos = screw_pos)
            translate([pos[0], pos[1], -1]) {
                cylinder(d = screw_d, h = back_t + 2);
                translate([0,0,back_t]) cylinder(d1 = screw_d, d2 = screw_d + 2.4, h = 1.3);
            }
    }
}

module test_frame() {
    difference() {
        rrect(outer_w, outer_h, 3, bezel_t);
        window();
        translate([cam_cx, cam_cy, -1]) cylinder(d = cam_hole_d, h = bezel_t + 2);
        translate([btn_x, cam_cy - 13, -1]) cylinder(d = btn_d, h = bezel_t + 2);
        // the rebate outline, scribed shallow, so the test frame shows where
        // the glass will sit relative to the window
        translate([(outer_w - panel_w)/2 - clear, bezel_bottom - clear, bezel_t - 0.6])
            difference() {
                cube([panel_w + 2*clear, panel_h + 2*clear, 1]);
                translate([1.5, 1.5, -1]) cube([panel_w + 2*clear - 3, panel_h + 2*clear - 3, 3]);
            }
    }
}

// ---------------------------------------------------------------------------

if (test_fit)              test_frame();
else if (part == "shell")  shell();
else if (part == "back")   back();
else { shell(); translate([outer_w + 10, 0, 0]) back(); }

echo(str("outer ", outer_w, " x ", outer_h, " x ", outer_z + back_t, " mm"));
echo(str("inner depth ", inner_depth, " mm; screws: ", len(screw_pos)));
echo(str("panel borders  side ", panel_border_side, "  bottom ",
         panel_border_bottom, "  top ", panel_border_top));
