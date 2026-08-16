/*
 * PaperCam — grey level calibration (bench tool, not a build phase)
 *
 * dither_fs_gray4 assumes the four levels render at 0, 85, 170, 255 — evenly
 * spaced. They almost certainly do not. Since error diffusion subtracts the
 * *assumed* value and pushes the remainder to neighbours, a wrong table is not
 * self-correcting: it becomes a systematic tonal shift no tone curve can undo,
 * because the tone curve runs before the ditherer's bad assumption.
 *
 * This measures the two middle levels without a light meter.
 *
 * The trick is that reflectance blends linearly. A patch made of black and
 * white pixels in a known ratio reflects a known fraction of the way between
 * the panel's black and its white. So: put a solid level-1 patch next to a
 * row of black/white patches at known white-fractions, and whichever one it
 * disappears into tells you level 1's true value. The panel measures itself,
 * and a phone camera's auto-exposure cannot distort the answer because both
 * halves sit in the same photo under the same light.
 *
 * Read it from arm's length or further. Up close you see the dither pattern
 * rather than the tone it averages to, which is the whole point of it.
 */

#include "TFT_eSPI.h"

#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif

EPaper epaper;

/* 8x8 ordered Bayer. An ordered pattern rather than random, because a uniform
 * texture is much easier to compare against a solid patch than noise is. */
static const uint8_t BAYER8[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

/* Fill a rect with a black/white pattern that is `white_pct` percent white. */
static void patch(int16_t x0, int16_t y0, int16_t w, int16_t h, int white_pct)
{
    const int thresh = (white_pct * 64) / 100;
    for (int16_t y = 0; y < h; y++) {
        for (int16_t x = 0; x < w; x++) {
            const uint8_t b = BAYER8[(y0 + y) & 7][(x0 + x) & 7];
            epaper.drawPixel(x0 + x, y0 + y, (b < thresh) ? TFT_GRAY_3 : TFT_GRAY_0);
        }
    }
}

/*
 * One comparison row: a solid patch of `level` on the left, then eight
 * black/white patches stepping through white-fractions from `lo` to `hi`.
 */
static void row(int16_t y, uint8_t level, int lo, int hi)
{
    const int16_t H     = 170;
    const int16_t SOLID = 190;
    const int16_t PW    = (800 - SOLID) / 8;

    epaper.fillRect(0, y, SOLID, H, level);

    epaper.setTextSize(2);
    epaper.setTextColor(TFT_GRAY_0);
    epaper.drawString(level == 1 ? "LEVEL 1" : "LEVEL 2", 20, y + H + 8);

    for (int i = 0; i < 8; i++) {
        const int pct = lo + ((hi - lo) * i) / 7;
        const int16_t px = (int16_t)(SOLID + i * PW);

        patch(px, y, PW, H, pct);

        // Label sits below the patch, not inside it — text within the patch
        // would change the very average we are trying to read.
        char buf[8];
        snprintf(buf, sizeof buf, "%d", pct);
        epaper.drawString(buf, px + PW / 2 - 12, y + H + 8);
    }
}

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam — grey level calibration ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    epaper.begin();
    epaper.initGrayMode(GRAY_LEVEL4);
    epaper.fillScreen(TFT_GRAY_3);

    // Expected if the levels are perceptually even in L*: level 1 near 23%
    // white, level 2 near 56%. The ranges bracket those generously in case
    // they are not.
    row(0,   1, 10, 45);
    row(240, 2, 40, 75);

    Serial.print("refreshing... ");
    Serial.flush();
    const uint32_t t0 = millis();
    epaper.update();
    Serial.printf("%lu ms\n", (unsigned long)(millis() - t0));

    Serial.println("\nStand back a metre and find, in each row, the patterned");
    Serial.println("patch that best matches the solid block on its left.");
    Serial.println("Report the two numbers under those patches.");
    Serial.println();
    Serial.println("Those percentages ARE the level values: a level that matches");
    Serial.println("a 23%% white patch sits 23%% of the way from black to white,");
    Serial.println("so its true value is 0.23 * 255 = 59, not the 85 we assume.");
}

void loop(void)
{
}
