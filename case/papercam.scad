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

// How far the glass sinks into the shell, and how much ledge it lands on.
//
// The ledge width matches the panel's own side border, so the shelf sits
// entirely under the glass margin and never intrudes on the active area.
// Without it there is no rebate at all: the main cavity is inset only by
// `wall`, which is LESS than the panel's inset, so a pocket cut to panel size
// removes nothing the cavity had not already removed.
panel_rebate_depth  = panel_t + 0.8;
panel_ledge         = 3.5;

// Clearance around the glass, separate from the general `clear`. 0.4mm made it
// a fight to seat; 1.5mm a side lets it drop in and still hides behind a
// 3.5mm bezel overlap. Grows the outer shell by 2.2mm, which is free.
panel_clear         = 1.5;

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

// --- hanging ----------------------------------------------------------------
// Two keyholes near the top of the back cover. Two rather than one because a
// 178mm frame on a single hook pivots; two fix the orientation, which also
// means it cannot be hung with the camera at the bottom.
//
// The slot rises ABOVE the round hole, which is the direction that works: the
// frame drops under gravity, so the screw travels UP relative to the plate and
// is captured by the narrow part with its head trapped behind.
//
// Each gets a pad on the INSIDE — 2.4mm of PLA around a keyhole is thin for
// hanging a frame, and thickening it there costs nothing visible.
keyhole_pitch  = 100;      // screw spacing to mark on the wall
keyhole_d      = 9.0;      // clears a #6 / 4mm screw head
keyhole_slot_w = 4.5;      // shank, not head
keyhole_rise   = 9;        // how far it drops onto the screw
keyhole_pad_d  = 18;
keyhole_pad_z  = 2.5;

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
outer_w = panel_w + 2*panel_clear + 2*wall;
outer_h = panel_h + bezel_top + bezel_bottom;
outer_z = bezel_t + panel_t + inner_depth;

// Back to centre. The board now sits behind the glass with the camera folded
// forward beside the ribbon, rather than being pushed aside to clear it.
cam_offset_x = 0;

// Vertically CENTRED IN THE BAND between the top of the panel pocket and the
// inner face of the top wall, rather than at half the nominal bezel. Those are
// not the same: the old figure put the well's top edge 0.4mm PAST the inner
// wall, which is what made it read as crowded against it.
pocket_top_y   = bezel_bottom + panel_h + panel_clear;
inner_top_y    = outer_h - wall;
cam_band_h     = inner_top_y - pocket_top_y;

cam_cx = outer_w/2 + cam_offset_x;
cam_cy = (pocket_top_y + inner_top_y) / 2;

// 16mm, not 22. The well only has to clear the 6.35mm module plus tape, and at
// 22 it did not fit the band at all.
cam_well_w = 16;
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
module panel_pocket() {
    // The +0.1 matters. Without it the pocket's back face lands exactly on the
    // deep cavity's front face, and two subtracted volumes sharing a coplanar
    // surface make OpenCSG render a ghost membrane across the window — it
    // looks precisely like something covering the hole. Always overlap
    // subtracted solids rather than butting them.
    translate([(outer_w - panel_w)/2 - panel_clear, bezel_bottom - panel_clear, bezel_t])
        cube([panel_w + 2*panel_clear, panel_h + 2*panel_clear,
              panel_rebate_depth + 0.1]);
}

// A local relief in the top band so the camera can sit against the inside of
// the front face. The panel pocket stops at the glass, and the stepped cavity
// behind starts too far back for the module to reach.
module camera_well() {
    translate([cam_cx - cam_well_w/2, cam_cy - cam_well_w/2, bezel_t])
        cube([cam_well_w, cam_well_w, panel_rebate_depth + 0.1]);
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
        rotate([90,0,0]) cylinder(d = btn_d, h = wall + panel_ledge + 2);
}

// Right wall, roughly mid-height. Deliberately oversized — the board goes
// wherever it fits. Cuts through the ledge as well as the wall, or it would
// open into 3.5mm of solid plastic.
module usb_slot() {
    translate([outer_w - wall - panel_ledge - 1, outer_h/2 - usb_w/2, bezel_t + 4])
        cube([wall + panel_ledge + 2, usb_w, usb_h]);
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
                // Behind the panel: stepped in, leaving the ledge.
                translate([wall + panel_ledge, wall + panel_ledge,
                           bezel_t + panel_rebate_depth])
                    cube([outer_w - 2*(wall + panel_ledge),
                          outer_h - 2*(wall + panel_ledge), outer_z]);
                // The panel's own pocket, full depth of the rebate.
                panel_pocket();
                // The camera has to reach the front face, and it lives above
                // the panel where the pocket does not go.
                camera_well();
            }
            bosses();
            if (use_board_posts) board_posts();
        }
        window();
        camera_hole();
        button_hole();
        usb_slot();
        bosses(bore = true);
        if (use_board_posts) board_posts(bore = true);
    }
}

// Round hole with a narrower slot rising from it, plus the pad it sits in.
module keyhole_cut(cx, cy) {
    translate([cx, cy, -1]) cylinder(d = keyhole_d, h = back_t + keyhole_pad_z + 2);
    translate([cx - keyhole_slot_w/2, cy, -1])
        cube([keyhole_slot_w, keyhole_rise, back_t + keyhole_pad_z + 2]);
    translate([cx, cy + keyhole_rise, -1])
        cylinder(d = keyhole_slot_w, h = back_t + keyhole_pad_z + 2);
}

keyhole_y  = outer_h - 30;
keyhole_xs = [outer_w/2 - keyhole_pitch/2, outer_w/2 + keyhole_pitch/2];

module back() {
    difference() {
        union() {
            rrect(outer_w, outer_h, 3, back_t);
            // Pads inside, spanning the whole keyhole so the slot end is
            // reinforced too, not just the round hole.
            for (kx = keyhole_xs) {
                translate([kx, keyhole_y, back_t])
                    cylinder(d = keyhole_pad_d, h = keyhole_pad_z);
                translate([kx, keyhole_y + keyhole_rise, back_t])
                    cylinder(d = keyhole_pad_d, h = keyhole_pad_z);
                translate([kx - keyhole_pad_d/2, keyhole_y, back_t])
                    cube([keyhole_pad_d, keyhole_rise, keyhole_pad_z]);
            }
        }
        for (kx = keyhole_xs) keyhole_cut(kx, keyhole_y);
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
echo(str("camera at ", cam_cx, ", ", cam_cy, "  band ", cam_band_h,
         "mm tall, well ", cam_well_w, "mm -> ", (cam_band_h - cam_well_w)/2,
         "mm margin each side"));
echo(str("WALL SCREWS: ", keyhole_pitch, "mm apart, level, ",
         outer_h - keyhole_y - keyhole_rise, "mm below the top of the frame"));
