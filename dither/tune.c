/*
 * PaperCam — tone-curve playground (host only).
 *
 * Loads a real captured frame, applies a tone curve, dithers it, and writes
 * the packed 1bpp result as PBM — exactly the bytes the panel would receive.
 *
 *   ./tune ../frames/gallery_wall.pgm out.pbm --contrast 1.8 --sharp 0.8
 *
 * The point is iteration speed: this runs in milliseconds against real
 * photographs, versus reflash-shoot-wait-squint at roughly three attempts a
 * minute on the bench.
 */

#include "dither.h"
#include "tone.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Next integer in a PGM header or ASCII body, skipping whitespace and
 * #-comments. Leaves the stream one byte past the digits, which for binary
 * PGM is exactly the single whitespace separator before the pixel data. */
static int pgm_next_int(FILE *f, int *out)
{
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == EOF) return 0;
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n') { }
            continue;
        }
        if (isspace(c)) continue;
        break;
    }
    if (!isdigit(c)) return 0;

    int v = 0;
    do {
        v = v * 10 + (c - '0');
        c = fgetc(f);
    } while (c != EOF && isdigit(c));

    *out = v;
    return 1;
}

/* Accepts both PGM flavours: P5 (binary) and P2 (ASCII). Some image tools
 * silently rewrite one as the other on save, and the difference is not worth
 * a failed load. */
static uint8_t *load_pgm(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }

    const int c1 = fgetc(f);
    const int c2 = fgetc(f);
    if (c1 != 'P' || (c2 != '2' && c2 != '5')) {
        fprintf(stderr, "%s: not a PGM (magic %c%c)\n", path, c1, c2);
        fclose(f);
        return NULL;
    }
    const int ascii = (c2 == '2');

    int maxval = 0;
    if (!pgm_next_int(f, w) || !pgm_next_int(f, h) || !pgm_next_int(f, &maxval)) {
        fprintf(stderr, "%s: malformed header\n", path);
        fclose(f);
        return NULL;
    }
    if (maxval != 255) {
        fprintf(stderr, "%s: maxval %d, expected 255\n", path, maxval);
        fclose(f);
        return NULL;
    }

    const size_t n   = (size_t)*w * (size_t)*h;
    uint8_t     *buf = malloc(n);
    if (!buf) { fclose(f); return NULL; }

    if (ascii) {
        for (size_t i = 0; i < n; i++) {
            int v = 0;
            if (!pgm_next_int(f, &v)) {
                fprintf(stderr, "%s: ran out of pixels at %zu of %zu\n", path, i, n);
                free(buf);
                fclose(f);
                return NULL;
            }
            buf[i] = (uint8_t)v;
        }
    } else if (fread(buf, 1, n, f) != n) {
        fprintf(stderr, "%s: short read\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return buf;
}

static void write_pbm(const char *path, const uint8_t *packed)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P4\n%d %d\n", DITHER_OUT_W, DITHER_OUT_H);
    fwrite(packed, 1, DITHER_OUT_BYTES, f);
    fclose(f);
}

/* Also dump the post-tone greyscale, so a bad result can be traced to the
 * curve rather than blamed on the ditherer. */
static void write_pgm(const char *path, const uint8_t *gray, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(gray, 1, (size_t)w * (size_t)h, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
            "usage: %s <in.pgm> <out.pbm> [options]\n"
            "  --black N     input level mapped to 0      (default 0)\n"
            "  --white N     input level mapped to 255    (default 255)\n"
            "  --gamma F     >1 lifts midtones            (default 1.0)\n"
            "  --contrast F  S-curve strength, 1 = off    (default 1.0)\n"
            "  --sharp F     unsharp amount, 0 = off      (default 0.0)\n"
            "  --radius N    unsharp radius in pixels     (default 3)\n"
            "  --dump-gray P also write the toned greyscale to P\n",
            argv[0]);
        return 2;
    }

    const char *in_path   = argv[1];
    const char *out_path  = argv[2];
    const char *gray_path = NULL;

    tone_params p = { 0, 255, 1.0f, 1.0f, 0.0f, 3 };

    for (int i = 3; i < argc; i++) {
        const int last = (i + 1 >= argc);
        if      (!strcmp(argv[i], "--black")    && !last) p.black     = (uint8_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--white")    && !last) p.white     = (uint8_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gamma")    && !last) p.gamma     = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--contrast") && !last) p.contrast  = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--sharp")    && !last) p.sharp_amt = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--radius")   && !last) p.sharp_rad = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump-gray")&& !last) gray_path   = argv[++i];
        else { fprintf(stderr, "unknown option: %s\n", argv[i]); return 2; }
    }

    int w = 0, h = 0;
    uint8_t *img = load_pgm(in_path, &w, &h);
    if (!img) return 1;

    if (w != DITHER_SRC_W || h != DITHER_SRC_H) {
        fprintf(stderr, "%s is %dx%d, expected %dx%d\n",
                in_path, w, h, DITHER_SRC_W, DITHER_SRC_H);
        free(img);
        return 1;
    }

    uint8_t *sa      = malloc((size_t)w * (size_t)h);
    uint8_t *sb      = malloc((size_t)w * (size_t)h);
    uint8_t *packed  = malloc(DITHER_OUT_BYTES);
    int16_t *scratch = malloc(DITHER_SCRATCH_LEN * sizeof(int16_t));
    if (!sa || !sb || !packed || !scratch) { fprintf(stderr, "oom\n"); return 1; }

    uint8_t lut[256];
    tone_build_lut(&p, lut);
    tone_apply(img, w, h, &p, lut, sa, sb);

    if (gray_path) write_pgm(gray_path, img, w, h);

    dither_fs(img, packed, scratch);
    write_pbm(out_path, packed);

    /* Black coverage is a useful one-number sanity check: a curve that drives
     * this far from ~40-55% has usually crushed the image one way or another. */
    static const uint8_t nib[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
    uint32_t black = 0;
    for (size_t i = 0; i < DITHER_OUT_BYTES; i++)
        black += nib[packed[i] & 0xF] + nib[packed[i] >> 4];

    printf("%-28s black %5.1f%%  (black=%u white=%u gamma=%.2f "
           "contrast=%.2f sharp=%.2f r=%d)\n",
           out_path, 100.0 * black / (DITHER_OUT_W * DITHER_OUT_H),
           p.black, p.white, (double)p.gamma, (double)p.contrast,
           (double)p.sharp_amt, p.sharp_rad);

    free(img); free(sa); free(sb); free(packed); free(scratch);
    return 0;
}
