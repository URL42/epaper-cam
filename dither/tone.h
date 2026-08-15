/*
 * PaperCam — tone mapping ahead of the dither.
 *
 * A 1-bit display renders midtones as 50% dot patterns, which read as grey
 * mush no matter how good the source is. The job here is to push tones apart
 * so the dither produces shapes instead of noise.
 *
 * Pure C11, same rules as dither.c: no allocation, no I/O, caller owns every
 * buffer, so this compiles unchanged inside the sketch.
 */

#ifndef PAPERCAM_TONE_H
#define PAPERCAM_TONE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t black;      /* input level mapped to 0   */
    uint8_t white;      /* input level mapped to 255 */
    float   gamma;      /* >1 lifts midtones, <1 crushes them; 1 = off */
    float   contrast;   /* S-curve strength; 1 = off, 2 = strong       */
    float   sharp_amt;  /* unsharp amount; 0 = off, 0.5-1.5 typical    */
    int     sharp_rad;  /* unsharp box radius in pixels; 2-8 typical   */
} tone_params;

/*
 * Settled by sweeping real captures through dither/tune against three very
 * different frames: a flat mid-tone gallery wall, a backlit portrait, and a
 * high-key desk shot. Black coverage lands at 43-44% on all three.
 *
 * The counter-intuitive part is how little global contrast is wanted. The
 * first attempt used contrast 1.5-2.0 and it crushed the backlit portrait to
 * a silhouette. The unsharp mask is what actually makes a dithered image
 * read, because it creates separation locally at every edge; the global curve
 * then only has to keep the result from looking dark, which the gamma lift
 * does. Contrast above ~1.3 buys punch by throwing away tonal range.
 */
#define TONE_DEFAULTS { 8, 255, 1.35f, 1.20f, 0.90f, 3 }

/*
 * All the global tone maths collapses into one 256-entry table, so the
 * per-pixel cost on device is a single array lookup rather than two pow()
 * calls. Build once, apply to every frame.
 */
void tone_build_lut(const tone_params *p, uint8_t lut[256]);

/*
 * Applies `lut` in place, then optionally an unsharp mask.
 *
 * Both scratch buffers must be w*h bytes and are only touched when
 * p->sharp_amt > 0; pass NULL to skip sharpening entirely.
 */
void tone_apply(uint8_t *img, int w, int h, const tone_params *p,
                const uint8_t lut[256], uint8_t *scratch_a, uint8_t *scratch_b);

#ifdef __cplusplus
}
#endif

#endif /* PAPERCAM_TONE_H */
