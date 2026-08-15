/*
 * PaperCam — greyscale mode test (bench tool, not a build phase)
 *
 * CLAUDE.md says this panel is 1-bit only. Seeed GFX disagrees: it exposes
 * initGrayMode(GRAY_LEVEL4) and GRAY_LEVEL16, and its GrayLevel16 example is
 * explicitly written for a "black and white dual-color screen". Multi-level
 * grey on a two-state panel comes from running several waveform passes.
 *
 * Two questions, and nothing else:
 *
 *   1. Do 4- and 16-level grey actually work on THIS panel?
 *   2. What does each refresh cost in seconds?
 *
 * Question 2 decides whether this is viable. Mono refresh is 3433 ms. If
 * 16-level costs 30s the whole shutter-to-image story changes and Phase 7's
 * power budget changes with it.
 *
 * Draws bands only. Whether photographs look better is the next question,
 * not this one — one variable at a time.
 */

#include "TFT_eSPI.h"

#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif

EPaper epaper;

static uint32_t timed_update(const char *label)
{
    Serial.printf("%s — refreshing... ", label);
    Serial.flush();

    const uint32_t t0 = millis();
    epaper.update();
    const uint32_t ms = millis() - t0;

    Serial.printf("%lu ms\n", (unsigned long)ms);
    return ms;
}

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam — greyscale mode test ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    epaper.begin();
    Serial.printf("Geometry: %d x %d\n", epaper.width(), epaper.height());

    const int16_t w = epaper.width();
    const int16_t h = epaper.height();

    // --- Mono reference, so the comparison is against this panel today -----
    epaper.fillScreen(TFT_WHITE);
    epaper.fillRect(0, 0, w / 2, h, TFT_BLACK);
    const uint32_t t_mono = timed_update("1-bit  (2 levels)");

    delay(3000);

    // --- 4 levels ----------------------------------------------------------
    Serial.println("\ninitGrayMode(GRAY_LEVEL4)");
    epaper.initGrayMode(GRAY_LEVEL4);

    const uint8_t g4[4] = { TFT_GRAY_0, TFT_GRAY_1, TFT_GRAY_2, TFT_GRAY_3 };
    for (uint8_t i = 0; i < 4; i++) {
        const int16_t y  = (int16_t)(i * (h / 4));
        const int16_t bh = (i == 3) ? (int16_t)(h - y) : (int16_t)(h / 4);
        epaper.fillRect(0, y, w, bh, g4[i]);
    }
    const uint32_t t_g4 = timed_update("4-level grey");

    /*
     * No 16-level test. GRAY_LEVEL16 and TFT_GRAY_4..15 do not exist in the
     * installed Seeed GFX — the GrayLevel16 example is newer than this
     * version. Not worth a library update until 4-level proves the idea.
     */

    // --- Verdict -----------------------------------------------------------
    Serial.println("\n--- VERDICT ---");
    Serial.printf("1-bit     %6lu ms   (Phase 1 measured 3433)\n", (unsigned long)t_mono);
    Serial.printf("4-level   %6lu ms   %.1fx mono\n",
                  (unsigned long)t_g4, (double)t_g4 / (double)t_mono);
    Serial.println("\nLook at the panel. Four genuinely distinct steps, or does the");
    Serial.println("middle pair collapse together? The datasheet gives white L*=63");
    Serial.println("and black L*=32, so there is not much room between them.");
}

void loop(void)
{
}
