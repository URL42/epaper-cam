/*
 * Native harness for dither_fs. Builds and runs on the Mac in under a second:
 *
 *     make test
 *
 * Checks properties that must hold, then writes the output as PBM so the
 * result can actually be looked at. Error-diffusion bugs are visual — banding,
 * worms, directional streaking — and a passing assertion does not prove the
 * image looks right.
 */

#include "dither.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run    = 0;
static int tests_failed = 0;

static void check(const char *name, int pass, const char *detail)
{
    tests_run++;
    if (pass) {
        printf("  PASS  %s\n", name);
    } else {
        tests_failed++;
        printf("  FAIL  %s\n        %s\n", name, detail);
    }
}

/* ------------------------------------------------------------------ */

static uint32_t count_black(const uint8_t *packed)
{
    static const uint8_t nibble[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
    uint32_t n = 0;
    for (size_t i = 0; i < DITHER_OUT_BYTES; i++) {
        n += nibble[packed[i] & 0x0F] + nibble[packed[i] >> 4];
    }
    return n;
}

/* Black pixels within a horizontal band of columns, all rows. */
static uint32_t count_black_cols(const uint8_t *packed, int x0, int x1)
{
    uint32_t n = 0;
    for (int y = 0; y < DITHER_OUT_H; y++) {
        const uint8_t *row = packed + (size_t)y * DITHER_OUT_ROW_BYTES;
        for (int x = x0; x < x1; x++) {
            if (row[x >> 3] & (0x80u >> (x & 7))) n++;
        }
    }
    return n;
}

/*
 * PBM P4 is packed 1bpp, MSB-first, 1 = black — byte for byte the same layout
 * we send to the panel. The image write is a straight fwrite of our buffer,
 * which is a small piece of luck worth using.
 */
static void write_pbm(const char *path, const uint8_t *packed)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P4\n%d %d\n", DITHER_OUT_W, DITHER_OUT_H);
    fwrite(packed, 1, DITHER_OUT_BYTES, f);
    fclose(f);
}

static void write_pgm(const char *path, const uint8_t *gray)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P5\n%d %d\n255\n", DITHER_SRC_W, DITHER_SRC_H);
    fwrite(gray, 1, (size_t)DITHER_SRC_W * DITHER_SRC_H, f);
    fclose(f);
}

/* ------------------------------------------------------------------ */

static uint8_t *src;
static uint8_t *out;
static int16_t *scratch;

static void fill_flat(uint8_t v)
{
    memset(src, v, (size_t)DITHER_SRC_W * DITHER_SRC_H);
}

static void fill_hgrad(void)
{
    for (int y = 0; y < DITHER_SRC_H; y++)
        for (int x = 0; x < DITHER_SRC_W; x++)
            src[(size_t)y * DITHER_SRC_W + (size_t)x] =
                (uint8_t)((x * 255) / (DITHER_SRC_W - 1));
}

static void fill_vgrad(void)
{
    for (int y = 0; y < DITHER_SRC_H; y++)
        for (int x = 0; x < DITHER_SRC_W; x++)
            src[(size_t)y * DITHER_SRC_W + (size_t)x] =
                (uint8_t)((y * 255) / (DITHER_SRC_H - 1));
}

/* Black bands exactly where the crop should remove them, white between. If the
 * crop offset is wrong by even one row, black leaks into the output. */
static void fill_crop_probe(void)
{
    for (int y = 0; y < DITHER_SRC_H; y++) {
        const int keep = (y >= DITHER_CROP_TOP &&
                          y <  DITHER_CROP_TOP + DITHER_OUT_H);
        memset(src + (size_t)y * DITHER_SRC_W, keep ? 255 : 0, DITHER_SRC_W);
    }
}

/* ------------------------------------------------------------------ */

int main(void)
{
    src     = malloc((size_t)DITHER_SRC_W * DITHER_SRC_H);
    out     = malloc(DITHER_OUT_BYTES);
    scratch = malloc(DITHER_SCRATCH_LEN * sizeof(int16_t));
    if (!src || !out || !scratch) { fprintf(stderr, "oom\n"); return 1; }

    const uint32_t total = (uint32_t)DITHER_OUT_W * DITHER_OUT_H;
    char detail[256];

    printf("dither_fs — %dx%d in, %dx%d out, crop %d rows top/bottom\n\n",
           DITHER_SRC_W, DITHER_SRC_H, DITHER_OUT_W, DITHER_OUT_H,
           DITHER_CROP_TOP);

    /* --- flat black ------------------------------------------------ */
    fill_flat(0);
    dither_fs(src, out, scratch);
    {
        const uint32_t n = count_black(out);
        snprintf(detail, sizeof detail, "expected %u black, got %u", total, n);
        check("flat 0 renders fully black", n == total, detail);
    }

    /* --- flat white ------------------------------------------------ */
    fill_flat(255);
    dither_fs(src, out, scratch);
    {
        const uint32_t n = count_black(out);
        snprintf(detail, sizeof detail, "expected 0 black, got %u", n);
        check("flat 255 renders fully white", n == 0, detail);
    }

    /* --- flat mid-grey --------------------------------------------- */
    fill_flat(128);
    dither_fs(src, out, scratch);
    {
        const uint32_t n = count_black(out);
        const double pct = 100.0 * n / total;
        snprintf(detail, sizeof detail, "expected ~50%%, got %.1f%%", pct);
        check("flat 128 renders ~50% black", pct > 45.0 && pct < 55.0, detail);
        write_pbm("out_flat128.pbm", out);
    }

    /* --- horizontal gradient --------------------------------------- */
    fill_hgrad();
    write_pgm("src_hgrad.pgm", src);
    dither_fs(src, out, scratch);
    {
        /* Dark on the left, so black density must fall left to right. */
        const uint32_t l = count_black_cols(out, 0, 100);
        const uint32_t r = count_black_cols(out, DITHER_OUT_W - 100, DITHER_OUT_W);
        snprintf(detail, sizeof detail, "left %u black, right %u", l, r);
        check("h-gradient is darker on the left", l > r * 4, detail);

        /* Monotonic across eighths — catches a scan-direction sign error that
         * the crude left-vs-right check above would miss. */
        int mono = 1;
        uint32_t prev = UINT32_MAX;
        for (int b = 0; b < 8; b++) {
            const uint32_t c = count_black_cols(out, b * 100, (b + 1) * 100);
            if (c > prev) mono = 0;
            prev = c;
        }
        check("h-gradient black density decreases monotonically", mono,
              "an eighth was darker than the one before it");
        write_pbm("out_hgrad.pbm", out);
    }

    /* --- vertical gradient ----------------------------------------- */
    fill_vgrad();
    dither_fs(src, out, scratch);
    {
        const uint32_t n = count_black(out);
        const double pct = 100.0 * n / total;
        /* Rows 60..539 of a 0..255 ramp average ~127, so ~50% black. */
        snprintf(detail, sizeof detail, "expected ~50%%, got %.1f%%", pct);
        check("v-gradient averages ~50% black", pct > 45.0 && pct < 55.0, detail);
        write_pbm("out_vgrad.pbm", out);
    }

    /* --- crop alignment -------------------------------------------- */
    fill_crop_probe();
    dither_fs(src, out, scratch);
    {
        const uint32_t n = count_black(out);
        snprintf(detail, sizeof detail,
                 "%u black pixels leaked in; crop offset is wrong", n);
        check("crop takes exactly rows 60..539", n == 0, detail);
    }

    /* --- output size ----------------------------------------------- */
    check("packed output is 48,000 bytes", DITHER_OUT_BYTES == 48000,
          "DITHER_OUT_BYTES is not 48000");

    printf("\n%d run, %d failed\n", tests_run, tests_failed);
    printf("wrote out_flat128.pbm out_hgrad.pbm out_vgrad.pbm src_hgrad.pgm\n");

    free(src); free(out); free(scratch);
    return tests_failed ? 1 : 0;
}
