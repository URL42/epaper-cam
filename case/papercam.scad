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

// Clearance around the glass, separate from the general `clear`, and separate
// per axis because only the long one was tight. 0.4mm was a press fit, 1.5mm
// still needed persuading along the length. 2.5mm a side there now.
//
// Both stay hidden behind the 3.5mm bezel overlap, so loosening costs nothing
// visible.
panel_clear_w       = 2.5;   // along the 170.2mm length
panel_clear_h       = 1.5;

// --- panel entry ------------------------------------------------------------
// The ledge that supports the glass also traps it: with 3.5mm of shelf each
// side the gap between ledges is 163.2mm, and the panel is 170.2mm. It cannot
// pass through from the back at all.
//
// So the bottom edge loses its fixed ledge and gains flexible fingers instead.
// Push the panel in from behind, the fingers deflect back, the panel seats
// against the bezel, the fingers spring over its rear face and hold it. The
// top and both sides keep their solid ledge, so three edges still support the
// glass properly.
//
// The fingers are ramped on the entry side so the panel deflects them smoothly
// rather than needing to be forced. Thin in Z on purpose: that is the
// direction they must bend, and a printed cantilever bending across its layers
// is stronger than one bending along them.
//
// If they snap — PLA is brittle and this is a real risk — set finger_count = 0
// and hold the bottom edge with tape. Nothing else depends on them.
finger_count  = 3;
finger_w      = 14;      // along the panel's length
finger_reach  = 2.5;     // how far it overlaps the glass edge
finger_t      = 1.2;     // deflecting thickness
finger_arm    = 9;       // cantilever length; longer bends more easily
finger_ramp   = 1.6;     // lead-in so it is not a press fit

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

// No USB opening. The board sits in the top band with its camera against the
// front face, which puts the USB-C port somewhere no cable can reach through a
// side wall — there is no line from any exterior face to the connector.
//
// Charging therefore means five screws and lifting the board out. Acceptable
// at a 2000mAh cell and weeks between charges; if it becomes tiresome the fix
// is a short USB-C pigtail glued into a hole in the bottom wall, or charging
// through the driver board's BAT connector instead.

// The top wall is not `wall` thick where the button goes: it is wall +
// panel_ledge = 5.9mm, because the ledge that used to run round the panel is
// still present along the top edge. A panel-mount switch has a short thread
// and cannot reach through that, so the inside is counterbored back to leave
// roughly half. 14mm across clears the nut and a socket to tighten it.
btn_wall_t   = 2.9;
btn_relief_d = 14;

// --- camera channel ----------------------------------------------------------
// Two ribs hanging off the inside of the top wall, forming a slot the board
// drops into. Taping to a board's edges is far easier than reaching in to
// stick its face to something, and it locates the camera over the lens hole
// without needing to know where the mounting holes are.
//
// Width is the board plus a little, so it is a guide rather than a press fit —
// the tape does the holding, these just stop it wandering.
cam_wall_gap = 26.0;   // 25.4mm board + slip
cam_wall_t   = 2.0;
//
// cam_wall_len is DERIVED, not chosen — see the derived section. At 25 the ribs
// hung 2.9mm past the top of the panel pocket, straight into the path the
// glass has to travel on its way in, and it would have jammed. They now stop
// exactly at the pocket edge.
//
// 20, not the 22 asked for: the ribs stand on the shelf at z=4.4 and the case
// interior ends at 24.6, so 20.2mm is all there is. 22 would poke through the
// back cover. It reaches the board anyway — with the camera against the front
// face the board's back sits near z=21.5, and 20mm of rib takes us to 24.4.
cam_wall_h   = 20;     // how far back from the panel plane

// Triangular gussets bracing each rib sideways, which is the direction a tall
// thin rib actually fails in. Only in the top band: below the panel's top edge
// there is no shelf under them to stand on.
gusset_base  = 6;
gusset_h     = 11;
gusset_t     = 2;

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
outer_w = panel_w + 2*panel_clear_w + 2*wall;

// NO SIDE LEDGES. The cavity opening is deliberately a hair WIDER than the
// glass so the panel drops straight in from the back.
//
// The ledge was 3.5mm a side, leaving a 163.2mm gap for a 170.2mm panel — it
// could not be fitted at any angle, only threaded in edgewise, and the pocket
// is too shallow to tilt a 170mm sheet of glass. Trying to slide it in through
// a slot was solving the wrong problem: the fix is to stop obstructing it.
//
// What holds the panel now: the bezel lip in front, the fingers along the
// bottom edge behind, and tape. Three things, none of which are in the way
// while you are fitting it.
panel_entry_gap = 0.75;                                 // opening minus panel
cav_x0 = (outer_w - panel_w)/2 - panel_entry_gap/2;
cav_x1 = outer_w - cav_x0;
outer_h = panel_h + bezel_top + bezel_bottom;
outer_z = bezel_t + panel_t + inner_depth;

// Back to centre. The board now sits behind the glass with the camera folded
// forward beside the ribbon, rather than being pushed aside to clear it.
cam_offset_x = 0;

// Vertically CENTRED IN THE BAND between the top of the panel pocket and the
// inner face of the top wall, rather than at half the nominal bezel. Those are
// not the same: the old figure put the well's top edge 0.4mm PAST the inner
// wall, which is what made it read as crowded against it.
pocket_top_y   = bezel_bottom + panel_h + panel_clear_h;
inner_top_y    = outer_h - wall;
cam_band_h     = inner_top_y - pocket_top_y;

cam_cx = outer_w/2 + cam_offset_x;
cam_cy = (pocket_top_y + inner_top_y) / 2;

// The ribs fill the top band and stop dead at the pocket. Anything longer
// reaches into the panel's insertion path and blocks it.
cam_wall_len = inner_top_y - pocket_top_y;

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
    translate([(outer_w - panel_w)/2 - panel_clear_w,
               bezel_bottom - panel_clear_h, bezel_t])
        cube([panel_w + 2*panel_clear_w, panel_h + 2*panel_clear_h,
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
    bz = bezel_t + inner_depth/2;
    // through-hole for the thread
    translate([btn_x, outer_h + 1, bz])
        rotate([90,0,0]) cylinder(d = btn_d, h = wall + panel_ledge + 2);
    // counterbore from the inside, leaving btn_wall_t of material outside
    translate([btn_x, outer_h - btn_wall_t, bz])
        rotate([90,0,0]) cylinder(d = btn_relief_d, h = 10);
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
                // Cavity runs right down past the panel's bottom edge, so
                // there is no ledge there and the glass can be pushed in.
                translate([cav_x0, wall,
                           bezel_t + panel_rebate_depth])
                    cube([cav_x1 - cav_x0,
                          outer_h - wall - (wall + panel_ledge), outer_z]);
                // The panel's own pocket, full depth of the rebate.
                panel_pocket();
                // The camera has to reach the front face, and it lives above
                // the panel where the pocket does not go.
                camera_well();
            }
            bosses();
            camera_walls();
            if (finger_count > 0) panel_fingers();
            if (use_board_posts) board_posts();
        }
        window();
        camera_hole();
        button_hole();
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

// A slot for the board, open at the back so it drops in and can be taped at
// the sides. Rooted in the top wall; runs down past the panel's top edge.
module camera_walls() {
    z0     = bezel_t + panel_rebate_depth;
    left   = cam_cx - cam_wall_gap/2 - cam_wall_t;   // outer face of the left rib
    right  = cam_cx + cam_wall_gap/2 + cam_wall_t;   // outer face of the right rib

    for (x0 = [left, cam_cx + cam_wall_gap/2])
        translate([x0, inner_top_y - cam_wall_len, z0])
            cube([cam_wall_t, cam_wall_len, cam_wall_h]);

    // Gussets on the OUTER faces, so nothing intrudes into the board slot.
    // Two per rib, both inside the top band where there is shelf beneath.
    for (gy = [inner_top_y - 4, inner_top_y - 15]) {
        translate([left - gusset_base, gy, z0]) rotate([90, 0, 0])
            linear_extrude(gusset_t)
                polygon([[0,0], [gusset_base,0], [gusset_base,gusset_h]]);
        translate([right, gy, z0]) rotate([90, 0, 0])
            linear_extrude(gusset_t)
                polygon([[0,0], [gusset_base,0], [0,gusset_h]]);
    }
}

// Flexible retention along the bottom of the pocket, in place of a ledge.
module panel_fingers() {
    fz   = bezel_t + panel_t + 0.2;                 // just behind the glass
    ytip = bezel_bottom - panel_clear_h;            // the pocket's bottom edge
    for (i = [0 : finger_count - 1]) {
        cx = cav_x0 + (cav_x1 - cav_x0) * (i + 0.5) / finger_count;
        translate([cx - finger_w/2, ytip, fz]) {
            // arm, rooted in the wall below the pocket and reaching up into it
            translate([0, -finger_arm, 0]) cube([finger_w, finger_arm + finger_reach, finger_t]);
            // ramp on the entry face so the panel pushes it aside
            translate([0, finger_reach, finger_t])
                rotate([0, 90, 0])
                    linear_extrude(finger_w)
                        polygon([[0,0], [0,-finger_reach], [-finger_ramp,-finger_reach]]);
        }
    }
}

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
echo(str("glass gap: ", panel_clear_w, "mm a side on length, ",
         panel_clear_h, "mm on height"));
echo(str("entry opening ", cav_x1 - cav_x0, "mm for a ", panel_w,
         "mm panel -> lays straight in"));
echo(str("button wall ", wall + panel_ledge, "mm counterbored to ", btn_wall_t,
         "mm over ", btn_relief_d, "mm"));
echo(str("camera channel ", cam_wall_gap, "mm wide, ", cam_wall_len,
         "mm down from the top wall, ", cam_wall_h, "mm deep"));
echo(str("ribs span y ", inner_top_y - cam_wall_len, " to ", inner_top_y,
         "; pocket top at ", pocket_top_y, " -> clear"));
echo(str("rib top at z ", bezel_t + panel_rebate_depth + cam_wall_h,
         "; interior ends at ", outer_z));
echo(str("panel enters past ", finger_count, " fingers on the bottom edge; ",
         "top and sides keep their ledge"));
echo(str("inner depth ", inner_depth, " mm; screws: ", len(screw_pos)));
echo(str("panel borders  side ", panel_border_side, "  bottom ",
         panel_border_bottom, "  top ", panel_border_top));
echo(str("camera at ", cam_cx, ", ", cam_cy, "  band ", cam_band_h,
         "mm tall, well ", cam_well_w, "mm -> ", (cam_band_h - cam_well_w)/2,
         "mm margin each side"));
echo(str("WALL SCREWS: ", keyhole_pitch, "mm apart, level, ",
         outer_h - keyhole_y - keyhole_rise, "mm below the top of the frame"));
