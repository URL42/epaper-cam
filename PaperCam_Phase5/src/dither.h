/*
 * PaperCam — Floyd-Steinberg dither, SVGA grayscale to packed 1bpp.
 *
 * Pure C11. No Arduino headers, no allocation, no I/O — so it builds both
 * natively for testing and unchanged inside the sketch.
 */

#ifndef PAPERCAM_DITHER_H
#define PAPERCAM_DITHER_H

#include <stdint.h>
#include <stddef.h>

/* The .ino is compiled as C++; this file is compiled as C. Without this the
 * C++ side would look for a mangled symbol name and fail to link. */
#ifdef __cplusplus
extern "C" {
#endif

#define DITHER_SRC_W 800
#define DITHER_SRC_H 600            /* SVGA, as the OV2640 delivers it     */

#define DITHER_OUT_W 800
#define DITHER_OUT_H 480            /* the panel                            */

/* 4:3 source into a 5:3 panel. Drop 60 rows top and bottom; every photo is
 * landscape and centre-cropped. */
#define DITHER_CROP_TOP ((DITHER_SRC_H - DITHER_OUT_H) / 2)

#define DITHER_OUT_ROW_BYTES (DITHER_OUT_W / 8)                      /*    100 */
#define DITHER_OUT_BYTES (DITHER_OUT_ROW_BYTES * DITHER_OUT_H)       /* 48,000 */

/* Error-diffusion scratch: two rows, each with one slot of slack at each end
 * so the x-1 and x+1 writes at the row edges need no bounds test. */
#define DITHER_SCRATCH_LEN (2 * (DITHER_OUT_W + 2))                  /*  1,604 */

/*
 * src      — DITHER_SRC_W * DITHER_SRC_H bytes of 8-bit luma.
 * out      — DITHER_OUT_BYTES, MSB-first, bit 7 leftmost. 1 = black,
 *            confirmed on the panel in Phase 1.
 * scratch  — DITHER_SCRATCH_LEN int16_t, contents irrelevant on entry.
 *
 * The caller owns every buffer. This function allocates nothing, so it cannot
 * fail and has nothing to free — the memory decisions stay where the memory
 * budget is understood.
 */
void dither_fs(const uint8_t *src, uint8_t *out, int16_t *scratch);

/* ---------------------------------------------------------------------------
 * 4-level greyscale.
 *
 * The panel datasheet calls this a B/W display and stores a B/W waveform in
 * OTP, but Seeed GFX overrides it with custom LUTs (LUT_*_GRAY in
 * UC8179_Defines.h) and gets four real levels out of it. Confirmed on the
 * bench: four visibly distinct bands.
 *
 * Output matches the layout initGrayMode(GRAY_LEVEL4) gives its sprite: 4 bits
 * per pixel, two pixels per byte, high nibble = even x, values 0..3 where
 * 0 is black and 3 is white. 800x480 = 192,000 bytes, and dither_fs_gray4 can
 * write straight into the sprite via getPointer().
 *
 * Four levels is not a small change for error diffusion. With two levels every
 * pixel is wrong by up to 127 and the dither has to spread that error over a
 * wide area, which is what produces the visible stipple. With four, worst-case
 * error drops to ~42, so the diffusion stays local and the texture largely
 * disappears.
 * ------------------------------------------------------------------------ */

#define DITHER_GRAY4_ROW_BYTES (DITHER_OUT_W / 2)                     /*    400 */
#define DITHER_GRAY4_BYTES (DITHER_GRAY4_ROW_BYTES * DITHER_OUT_H)    /* 192,000 */

void dither_fs_gray4(const uint8_t *src, uint8_t *out, int16_t *scratch);

#ifdef __cplusplus
}
#endif

#endif /* PAPERCAM_DITHER_H */
