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

#ifdef __cplusplus
}
#endif

#endif /* PAPERCAM_DITHER_H */
