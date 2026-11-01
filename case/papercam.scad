// PaperCam enclosure — parametric, for a Bambu A1.
//
//   part = "shell"  the front: bezel, walls, and every opening
//   part = "back"   cover with the board's locating pins, M2 into heat-set inserts
//   part = "both"   side by side for inspection
//   part = "coupon_shell" / "coupon_back" / "coupon"
//                   the top band only, and the piece of cover that meets it:
//                   lens hole, ribs, USB-C slot and pins, for about an hour of
//                   printing instead of three. Print these before the shell.
//
// PRINT test_fit = true FIRST — a thin skeleton with only the panel window,
// lens hole and button hole. Minutes of filament, and it settles the one thing
// no datasheet states: where the active area sits inside the glass.
//
// ---------------------------------------------------------------------------
// DESIGN NOTE: the lens hole is the datum
//
// The camera was first positioned by hand — held centred in the bore and
// taped down. That proved too imprecise, so the board is now located by two
// pins on the back cover through its middle mounting holes, which press it
// forward and put the lens on the hole every time the cover goes on.
//
// The hole does NOT move to suit the pins. It stays where the camera is
// already fitted, and the pins and the USB-C slot are placed from the LENS by
// measurement — lens to holes, lens to port — never chained through board
// edges, where each step adds error. The hole is also oversized (7mm for a
// 3.2mm lens) so that whatever error remains is absorbed by the hole rather
// than showing up as a lens half-hidden behind the bezel.
//
// The tolerance chain: hole slop in the cover's screws, pin slop in the
// board's 2.5mm holes, and print error — about +-0.4mm in all, against the
// +-1.9mm the hole forgives.
//
// CONFIDENCE
//   panel outline / active area   T075A04 datasheet, trustworthy
//   board 41.3 x 25.4             measured; an unsourced spec says 41 x 22
//   board hole pitch / diameter   measured: 20.5mm centres, 2.5mm
//   lens-relative board numbers   FIT-CHECKED, not measured: the coupon print
//                                 (2026-11-01) seated the pins in the holes,
//                                 centred the lens in its hole, and took the
//                                 USB-C plug. Good to within the slop; the
//                                 true values are still unknown.
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
// So the bottom edge has three short hooks instead of a ledge. Tuck the
// panel's bottom edge under them, then lay the top edge down into the pocket.
//
// They are RIGID, whatever earlier versions of this comment claimed. The arm
// lies in the solid bezel band below the cavity, which fuses it along its
// length; only the 2.5mm lip over the glass is free, and that is a hook, not a
// spring. Rigid is what the tuck-and-lay fitting wants anyway.
//
// The arm only has to root the lip in that band. At 9mm it was longer than
// the 8.5mm the band has, and poked 0.5mm out through the bottom face as three
// 14mm slivers — invisible in preview, obvious in the first real render.
finger_count  = 3;
finger_w      = 14;      // along the panel's length
finger_reach  = 2.5;     // how far it overlaps the glass edge
finger_t      = 1.2;     // lip thickness
finger_arm    = 6;       // root in the bezel band; must stay inside the wall
finger_ramp   = 1.6;     // lead-in on the lip's entry face

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
cam_lens_d    = 3.2;              // lens barrel
// Oversized on purpose: the lens can sit up to this far off centre and still
// see out unobstructed. Was 0.5, which needed the camera placed to half a
// millimetre by hand.
cam_hole_slop = 1.9;
cam_hole_d    = cam_lens_d + 2*cam_hole_slop;   // 7.0
cam_hole_csk  = 2.4;              // countersink growth at the front face (45 deg)
btn_d         = 8.2;              // M8 panel mount

// --- USB-C charging slot -------------------------------------------------------
// Through the top wall, straight at the XIAO's port, which faces the top wall
// and lines up with the lens across the board's width.
//
// Sized for the cable's moulded PLASTIC body, not the metal plug. The wall is
// 5.9mm thick here (see the button note), and the metal tip is only ~6.6mm
// long, so with a metal-sized hole the plastic would stop at the outer face
// and the tip would barely reach the port. A body-sized slot lets the plastic
// go right into the wall.
usb_plug_w  = 13.5;   // (E) cable's plastic body, across — measure yours
usb_plug_t  = 7.5;    // (E) and through its thickness
usb_clear   = 0.4;    // a side
usb_depth   = 10;     // (D) fit-checked: lens front face -> port centreline, front to back
usb_x_off   = 0;      // port centre vs lens centre across the board, + toward the button

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

// --- top-band test coupon -------------------------------------------------------
// A slice of the shell around the camera, and the matching slice of cover.
// Everything new in this revision lives here — pins, slot, bigger lens hole —
// so an hour of printing answers what a full shell would.
//
// The slices are cut from the real parts, not modelled separately, so the
// pins, holes and slot are exactly where the real ones are. Two things are
// added: walls along the cut edges, and a rim on the cover's inside face that
// drops inside those walls. The full cover is located by its corner screws,
// none of which fall in this slice; the rim does that job instead, to the same
// sort of accuracy, which is what makes the coupon a fair test of the pins.
coupon_half_w    = 30;     // either side of the lens: takes in the ribs and gussets
coupon_rim_t     = 1.6;
coupon_rim_h     = 2.0;
coupon_rim_clear = 0.25;   // a side

// --- fasteners --------------------------------------------------------------
insert_d = 3.2;                   // brass M2 heat-set
insert_z = 4.0;
screw_d  = 2.4;
screw_head_d = 4.0;               // M2 countersunk head is 3.8; 90 degree seat
boss_d   = 6.5;

// --- board locating pins --------------------------------------------------------
// Two pins standing off the back cover's inside face, into the driver board's
// middle mounting holes. The shoulder at the base of each pin bears on the
// board and pushes it forward, seating the lens against the inside of the
// front face; the pin itself fixes it sideways and up-down.
//
// On the COVER, not the shell, because the middle holes sit behind the glass
// where the shell has nothing to build a post from.
//
// Every number with a letter is measured FROM THE LENS (see the design note).
board_pins       = true;
pin_pitch        = 20.5;   // measured: middle pair centres
board_hole_d     = 2.5;    // measured
lens_to_pins     = 15;     // (A) fit-checked: lens centre -> middle-hole centres, along the board
lens_to_pins_x   = 0;      // (B) hole pair's midpoint vs lens, across the board, + toward the button
board_stack      = 19;     // (C) fit-checked: lens front face -> back face of the driver board
lens_to_board_top = 7;     // (D) fit-checked: lens centre -> board's top (USB-C) edge
board_len        = 41.3;   // measured

pin_d            = board_hole_d - 0.3;   // slip fit; printed holes run small, pins run fat
pin_len          = 2.4;    // past the shoulder: through a 1.6mm PCB and a little beyond
pin_tip          = 0.8;    // taper, so the pins find the holes blind as the cover closes
shoulder_d       = 4.5;    // bears on the hole's pad ring, clear of nearby parts
pin_preload      = 0.2;    // shoulder reaches this far past the board's back face

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

// Centred between the top of the panel pocket and `inner_top_y`, the line of
// the top wall's nominal inner face.
//
// That line is NOT the real inside face. A strip of the old top ledge, 3.5mm
// deep, still runs along the top wall behind the bezel, so the cavity actually
// stops at `top_face_y`, 141.3 — and the camera is ~1.75mm above the true centre
// of its band. It stays there: the camera is already fitted to this hole, and
// the hole is the datum everything else is measured from.
pocket_top_y   = bezel_bottom + panel_h + panel_clear_h;
inner_top_y    = outer_h - wall;
top_face_y     = outer_h - wall - panel_ledge;   // the real inside face, 141.3
cam_band_h     = inner_top_y - pocket_top_y;

cam_cx = outer_w/2 + cam_offset_x;
cam_cy = (pocket_top_y + inner_top_y) / 2;

// Lens-relative features, in shell coordinates. "Toward the button" is +x.
pin_cx   = cam_cx + lens_to_pins_x;
pin_y    = cam_cy - lens_to_pins;
pin_xs   = [pin_cx - pin_pitch/2, pin_cx + pin_pitch/2];
board_back_z = bezel_t + board_stack;            // lens front sits on the bezel's inside
shoulder_len = outer_z - board_back_z + pin_preload;   // from the cover's inside face

usb_x    = cam_cx + usb_x_off;
usb_z    = bezel_t + usb_depth;
usb_w    = usb_plug_w + 2*usb_clear;
usb_t    = usb_plug_t + 2*usb_clear;

// The coupon reaches down past the board's bottom edge, so the whole board
// sits in it and the pins can be judged against the real thing.
board_bottom_y = cam_cy + lens_to_board_top - board_len;
coupon_x0 = cam_cx - coupon_half_w;
coupon_x1 = cam_cx + coupon_half_w;
coupon_y0 = min(board_bottom_y, pin_y - shoulder_d/2) - 4 - wall;

// The ribs fill the top band and stop dead at the pocket. Anything longer
// reaches into the panel's insertion path and blocks it.
cam_wall_len = inner_top_y - pocket_top_y;

// 16mm, not 22. The well only has to clear the 6.35mm module plus tape, and at
// 22 it did not fit the band at all.
cam_well_w = 16;
btn_x  = outer_w - bezel_side - 12;

// Five, not six. A boss at top-centre landed exactly on the camera, which is
// also at top-centre — so that one is gone and the top keeps only its corners.
//
// The bottom row sits lower than the top row, partly sunk into the bottom
// wall. At the same inset as the top they reached y=9.9 — into the panel
// pocket, 0.1mm from the glass edge — so the bottom clearance was really
// 0.1mm at three points while the parameter claimed 1.5. They now stop short
// of the pocket.
bx0 = wall + boss_d/2 + 1;
bx1 = outer_w - wall - boss_d/2 - 1;
by0 = min(wall + boss_d/2 + 1, (bezel_bottom - panel_clear_h) - boss_d/2 - 0.1);
by1 = outer_h - wall - boss_d/2 - 1;
screw_pos = [[bx0, by0], [outer_w/2, by0], [bx1, by0], [bx0, by1], [bx1, by1]];

// --- checks -----------------------------------------------------------------
// Each of these is a mistake this design has already made once, or nearly.
ytip_ = bezel_bottom - panel_clear_h;
assert(ytip_ - finger_arm > 0,
       "finger arm pokes out through the bottom face");
assert(by0 + boss_d/2 < ytip_,
       "bottom bosses reach into the panel pocket");
assert(by0 - insert_d/2 >= 1.2,
       "too little wall outside the bottom inserts");
assert(shoulder_len >= 0.5,
       "board stack too deep: the board would hit the back cover");
// The ribs reach almost to the cover (z 24.4 of 24.6), so a shoulder that
// lands on one would stop the cover closing. Fine if the pins are below the
// ribs' lower end, or between them.
assert(pin_y + shoulder_d/2 < inner_top_y - cam_wall_len
       || max([for (x = pin_xs) abs(x - cam_cx)]) + shoulder_d/2 <= cam_wall_gap/2,
       "a pin shoulder lands on a camera rib — check lens_to_pins / lens_to_pins_x");
assert(cam_cy + lens_to_board_top <= top_face_y,
       "board's top edge would be inside the top wall — check lens_to_board_top");
assert(usb_z - usb_t/2 >= bezel_t/2,
       "USB slot would notch the front face");
assert(usb_z + usb_t/2 <= outer_z - 1,
       "USB slot would break out of the back");
assert(abs(usb_x - cam_cx) + usb_w/2 <= cam_wall_gap/2,
       "USB slot would cut into a camera rib");

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

// Countersunk from the FRONT so the bezel cannot vignette a wide lens sitting
// 2.5mm behind the outer face. z=0 is the front face.
//
// It used to be cut from bezel_t downward, widening toward the INSIDE — next
// to the lens, where it does nothing for the field of view. Same mistake as
// the back cover's screw seats: a cone on the face nobody looks through.
// 45 degrees, so it prints face-down without support.
module camera_hole() {
    translate([cam_cx, cam_cy, -1]) cylinder(d = cam_hole_d, h = bezel_t + 2);
    translate([cam_cx, cam_cy, -0.01])
        cylinder(d1 = cam_hole_d + cam_hole_csk, d2 = cam_hole_d, h = cam_hole_csk/2);
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

// Stadium-shaped, through the whole top wall into the cavity.
module usb_slot() {
    translate([usb_x, outer_h + 1, usb_z]) rotate([90, 0, 0])
        linear_extrude(outer_h + 1 - top_face_y + 1)
            hull() for (s = [-1, 1]) translate([s * (usb_w - usb_t)/2, 0]) circle(d = usb_t);
}

// Built in COVER coordinates. The cover is flipped about the vertical axis to
// fit, so a feature at shell x lands at cover x = outer_w - x. Everything so
// far happened to be symmetric and never needed this; the pins need not be.
module cover_pins() {
    for (x = pin_xs)
        translate([outer_w - x, pin_y, back_t - 0.01]) {
            cylinder(d = shoulder_d, h = shoulder_len + 0.01);
            translate([0, 0, shoulder_len]) {
                cylinder(d = pin_d, h = pin_len - pin_tip + 0.01);
                translate([0, 0, pin_len - pin_tip])
                    cylinder(d1 = pin_d, d2 = pin_d - 0.8, h = pin_tip);
            }
        }
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
        }
        window();
        camera_hole();
        button_hole();
        usb_slot();
        bosses(bore = true);
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
            if (board_pins) cover_pins();
        }
        for (kx = keyhole_xs) keyhole_cut(kx, keyhole_y);
        // Countersunk on the OUTSIDE face, z=0 — the face that hangs against
        // the wall. It used to be cut on the inside face (the one with the
        // pads), which left plain holes outside and the screw heads proud.
        // Screw positions are symmetric across the flip, so x needs no mirror.
        for (pos = screw_pos)
            translate([pos[0], pos[1], 0]) {
                translate([0, 0, -1]) cylinder(d = screw_d, h = back_t + 2);
                translate([0, 0, -0.01])
                    cylinder(d1 = screw_head_d, d2 = screw_d, h = (screw_head_d - screw_d)/2);
            }
    }
}

// Inside of the coupon's walls, in SHELL x, left open past the top face.
module coupon_opening()  translate([coupon_x0 + wall, coupon_y0 + wall])
                             square([2*coupon_half_w - 2*wall, top_face_y - coupon_y0]);

module coupon_shell() {
    intersection() {
        shell();
        translate([coupon_x0, coupon_y0, -1])
            cube([2*coupon_half_w, outer_h - coupon_y0 + 1, outer_z + 2]);
    }
    // Walls along the three cut edges. They run 1mm into the top wall rather
    // than stopping at its face, so the join has real overlap, not a seam.
    linear_extrude(outer_z)
        difference() {
            translate([coupon_x0, coupon_y0]) square([2*coupon_half_w, top_face_y - coupon_y0 + 1]);
            coupon_opening();
        }
}

module coupon_back() {
    // The real cover, cut to the same slice. The cover is mirrored in x when
    // fitted, so the slice is too.
    intersection() {
        back();
        translate([outer_w - coupon_x1, coupon_y0, -1])
            cube([2*coupon_half_w, outer_h - coupon_y0 + 1, 50]);
    }
    // Locating rim, standing in for the corner screws. Gapped where the
    // camera ribs come up to within 0.2mm of the cover, or it would land on them.
    rib_span = cam_wall_gap/2 + cam_wall_t + 1;
    translate([0, 0, back_t - 0.01])
        linear_extrude(coupon_rim_h + 0.01)
            translate([outer_w, 0]) mirror([1, 0])       // shell x -> cover x
                difference() {
                    offset(delta = -coupon_rim_clear) coupon_opening_closed();
                    offset(delta = -coupon_rim_clear - coupon_rim_t) coupon_opening_closed();
                    translate([cam_cx - rib_span, inner_top_y - cam_wall_len - 1])
                        square([2*rib_span, cam_wall_len + 10]);
                }
}

// The opening, closed at the top by the top wall's real inside face.
module coupon_opening_closed()
    translate([coupon_x0 + wall, coupon_y0 + wall])
        square([2*coupon_half_w - 2*wall, top_face_y - coupon_y0 - wall]);

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

if (test_fit)                     test_frame();
else if (part == "shell")         shell();
else if (part == "back")          back();
else if (part == "coupon_shell")  coupon_shell();
else if (part == "coupon_back")   coupon_back();
else if (part == "coupon") {
    coupon_shell();
    translate([2*coupon_half_w + 10, 0, 0]) coupon_back();
}
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
echo(str("panel tucks under ", finger_count, " rigid hooks on the bottom edge; ",
         "bottom bosses end ", ytip_ - (by0 + boss_d/2), "mm short of the pocket"));
echo(str("lens hole ", cam_hole_d, "mm for a ", cam_lens_d, "mm lens -> +-",
         cam_hole_slop, "mm of placement error forgiven"));
echo(str("pins at x ", pin_xs[0], " / ", pin_xs[1], ", y ", pin_y,
         "; shoulder ", shoulder_len, "mm off the cover, presses the board ",
         pin_preload, "mm"));
echo(str("USB-C slot ", usb_w, " x ", usb_t, "mm at x ", usb_x, ", z ", usb_z,
         " through ", outer_h - top_face_y, "mm of top wall; board edge ",
         top_face_y - (cam_cy + lens_to_board_top), "mm short of the wall"));
echo(str("inner depth ", inner_depth, " mm; screws: ", len(screw_pos)));
echo(str("panel borders  side ", panel_border_side, "  bottom ",
         panel_border_bottom, "  top ", panel_border_top));
echo(str("camera at ", cam_cx, ", ", cam_cy, "  band ", cam_band_h,
         "mm tall, well ", cam_well_w, "mm -> ", (cam_band_h - cam_well_w)/2,
         "mm margin each side"));
echo(str("WALL SCREWS: ", keyhole_pitch, "mm apart, level, ",
         outer_h - keyhole_y - keyhole_rise, "mm below the top of the frame"));
