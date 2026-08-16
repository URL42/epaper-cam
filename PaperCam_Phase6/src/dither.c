#include "dither.h"
#include <string.h>

/*
 * Floyd-Steinberg with a serpentine scan.
 *
 * Left-to-right rows push error as:
 *
 *          X    7/16
 *   3/16  5/16  1/16
 *
 * Right-to-left rows mirror it. Alternating direction each row is what stops
 * the error from marching consistently one way down the image, which is what
 * produces the diagonal "worm" texture a plain raster scan gives you. It costs
 * one branch per row.
 */

void dither_fs(const uint8_t *src, uint8_t *out, int16_t *scratch)
{
    /* Skip straight to the first kept row. Dithering the 120 cropped rows and
     * throwing them away would be 25% more work for output nobody sees. */
    const uint8_t *s = src + (size_t)DITHER_CROP_TOP * DITHER_SRC_W;

    memset(out, 0, DITHER_OUT_BYTES);
    memset(scratch, 0, DITHER_SCRATCH_LEN * sizeof(int16_t));

    /* Offset by one so index -1 and index DITHER_OUT_W are both legal. */
    int16_t *err_cur  = scratch + 1;
    int16_t *err_next = scratch + (DITHER_OUT_W + 2) + 1;

    for (int y = 0; y < DITHER_OUT_H; y++) {
        const uint8_t *srow = s   + (size_t)y * DITHER_SRC_W;
        uint8_t       *orow = out + (size_t)y * DITHER_OUT_ROW_BYTES;

        const int ltr  = ((y & 1) == 0);
        const int step = ltr ? 1 : -1;
        int       x    = ltr ? 0 : DITHER_OUT_W - 1;

        for (int i = 0; i < DITHER_OUT_W; i++, x += step) {
            /*
             * int32_t, not uint8_t. Accumulated error routinely pushes this
             * outside 0..255, and in an unsigned byte that wraps — 255 + 3
             * becomes 2, a white pixel turning black. That wraparound is the
             * single most common way this algorithm is written wrong, and it
             * shows up as salt-and-pepper speckle in flat bright areas.
             */
            int32_t old = (int32_t)srow[x] + (int32_t)err_cur[x];

            /* Clamping before thresholding bounds the error we propagate to
             * +/-127, which keeps the int16_t scratch far from overflow and
             * stops a blown-out region from ringing into its neighbours. */
            if (old < 0)   old = 0;
            if (old > 255) old = 255;

            const int     black = (old < 128);
            const int32_t newv  = black ? 0 : 255;
            const int32_t e     = old - newv;

            if (black) {
                orow[x >> 3] |= (uint8_t)(0x80u >> (x & 7));
            }

            const int fwd = x + step;   /* next pixel this row */
            const int bwd = x - step;   /* previous pixel, one row down */

            err_cur[fwd]  += (int16_t)((e * 7) / 16);
            err_next[bwd] += (int16_t)((e * 3) / 16);
            err_next[x]   += (int16_t)((e * 5) / 16);
            err_next[fwd] += (int16_t)((e * 1) / 16);
        }

        /* Next row's accumulated error becomes this row's input. Swapping
         * pointers rather than copying 1.6KB per row. */
        int16_t *tmp = err_cur;
        err_cur  = err_next;
        err_next = tmp;

        memset(err_next - 1, 0, (DITHER_OUT_W + 2) * sizeof(int16_t));
    }
}

/* ---------------------------------------------------------------------------
 * 4-level variant. Same serpentine Floyd-Steinberg; only the quantiser and the
 * output packing differ.
 * ------------------------------------------------------------------------ */

void dither_fs_gray4(const uint8_t *src, uint8_t *out, int16_t *scratch)
{
    const uint8_t *s = src + (size_t)DITHER_CROP_TOP * DITHER_SRC_W;

    memset(out, 0, DITHER_GRAY4_BYTES);
    memset(scratch, 0, DITHER_SCRATCH_LEN * sizeof(int16_t));

    int16_t *err_cur  = scratch + 1;
    int16_t *err_next = scratch + (DITHER_OUT_W + 2) + 1;

    for (int y = 0; y < DITHER_OUT_H; y++) {
        const uint8_t *srow = s   + (size_t)y * DITHER_SRC_W;
        uint8_t       *orow = out + (size_t)y * DITHER_GRAY4_ROW_BYTES;

        const int ltr  = ((y & 1) == 0);
        const int step = ltr ? 1 : -1;
        int       x    = ltr ? 0 : DITHER_OUT_W - 1;

        for (int i = 0; i < DITHER_OUT_W; i++, x += step) {
            int32_t old = (int32_t)srow[x] + (int32_t)err_cur[x];
            if (old < 0)   old = 0;
            if (old > 255) old = 255;

            /*
             * Nearest of the four *measured* levels, so compare against the
             * midpoints between them rather than dividing by a constant. The
             * levels are not evenly spaced — see DITHER_GRAY4_L* in dither.h.
             */
            static const int16_t LEVEL[4] = {
                DITHER_GRAY4_L0, DITHER_GRAY4_L1,
                DITHER_GRAY4_L2, DITHER_GRAY4_L3
            };
            int32_t level;
            if      (old < (DITHER_GRAY4_L0 + DITHER_GRAY4_L1) / 2) level = 0;
            else if (old < (DITHER_GRAY4_L1 + DITHER_GRAY4_L2) / 2) level = 1;
            else if (old < (DITHER_GRAY4_L2 + DITHER_GRAY4_L3) / 2) level = 2;
            else                                                    level = 3;

            /* The error is against what the panel will really show, which is
             * the whole point of measuring. */
            const int32_t e = old - LEVEL[level];

            /* High nibble is the even-x pixel, matching the sprite layout
             * initGrayMode(GRAY_LEVEL4) sets up. */
            if ((x & 1) == 0) {
                orow[x >> 1] |= (uint8_t)(level << 4);
            } else {
                orow[x >> 1] |= (uint8_t)level;
            }

            const int fwd = x + step;
            const int bwd = x - step;

            err_cur[fwd]  += (int16_t)((e * 7) / 16);
            err_next[bwd] += (int16_t)((e * 3) / 16);
            err_next[x]   += (int16_t)((e * 5) / 16);
            err_next[fwd] += (int16_t)((e * 1) / 16);
        }

        int16_t *tmp = err_cur;
        err_cur  = err_next;
        err_next = tmp;
        memset(err_next - 1, 0, (DITHER_OUT_W + 2) * sizeof(int16_t));
    }
}
