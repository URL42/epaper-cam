#include "tone.h"
#include <math.h>
#include <stddef.h>

static uint8_t clamp8(float v)
{
    if (v <= 0.0f)   return 0;
    if (v >= 255.0f) return 255;
    return (uint8_t)(v + 0.5f);
}

void tone_build_lut(const tone_params *p, uint8_t lut[256])
{
    const float lo   = (float)p->black;
    const float hi   = (float)p->white;
    const float span = (hi > lo) ? (hi - lo) : 1.0f;

    for (int i = 0; i < 256; i++) {
        /* 1. Levels: rescale the useful range to 0..1. */
        float x = ((float)i - lo) / span;
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;

        /* 2. Gamma. */
        if (p->gamma > 0.0f && p->gamma != 1.0f) {
            x = powf(x, 1.0f / p->gamma);
        }

        /*
         * 3. S-curve:  x^a / (x^a + (1-x)^a)
         *
         * Chosen over the usual "scale around 0.5" contrast because it cannot
         * clip. It maps 0->0, 0.5->0.5, 1->1 for every a, and simply steepens
         * the middle as a rises. A linear contrast boost strong enough to
         * separate these midtones would flatten the highlights to solid white
         * and lose the picture frames entirely.
         */
        if (p->contrast > 0.0f && p->contrast != 1.0f) {
            const float a  = p->contrast;
            const float xa = powf(x, a);
            const float ya = powf(1.0f - x, a);
            const float d  = xa + ya;
            x = (d > 0.0f) ? (xa / d) : x;
        }

        lut[i] = clamp8(x * 255.0f);
    }
}

/* Separable box blur, horizontal then vertical. Two passes of a box are a
 * crude Gaussian, which is plenty for an unsharp mask — the mask only needs
 * a rough local average to subtract. */
static void box_blur(const uint8_t *src, uint8_t *dst, uint8_t *tmp,
                     int w, int h, int r)
{
    if (r < 1) r = 1;

    for (int y = 0; y < h; y++) {
        const uint8_t *s = src + (size_t)y * (size_t)w;
        uint8_t       *d = tmp + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++) {
            int sum = 0, n = 0;
            const int x0 = (x - r < 0) ? 0 : x - r;
            const int x1 = (x + r >= w) ? w - 1 : x + r;
            for (int i = x0; i <= x1; i++) { sum += s[i]; n++; }
            d[x] = (uint8_t)(sum / n);
        }
    }

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            int sum = 0, n = 0;
            const int y0 = (y - r < 0) ? 0 : y - r;
            const int y1 = (y + r >= h) ? h - 1 : y + r;
            for (int i = y0; i <= y1; i++) { sum += tmp[(size_t)i * (size_t)w + (size_t)x]; n++; }
            dst[(size_t)y * (size_t)w + (size_t)x] = (uint8_t)(sum / n);
        }
    }
}

void tone_apply(uint8_t *img, int w, int h, const tone_params *p,
                const uint8_t lut[256], uint8_t *scratch_a, uint8_t *scratch_b)
{
    const size_t n = (size_t)w * (size_t)h;

    for (size_t i = 0; i < n; i++) {
        img[i] = lut[img[i]];
    }

    if (p->sharp_amt <= 0.0f || !scratch_a || !scratch_b) {
        return;
    }

    /*
     * Unsharp mask, applied last — immediately before quantisation, where the
     * local separation it creates is what survives into the 1-bit output.
     *
     * This matters more than any global curve for dithered photos. A tone
     * curve can only decide that "this grey becomes 60% dots"; the unsharp
     * mask brightens one side of every edge and darkens the other, which is
     * what makes an edge read as an edge rather than as a change in dot
     * density.
     */
    box_blur(img, scratch_a, scratch_b, w, h, p->sharp_rad);

    for (size_t i = 0; i < n; i++) {
        const float detail = (float)img[i] - (float)scratch_a[i];
        img[i] = clamp8((float)img[i] + p->sharp_amt * detail);
    }
}
