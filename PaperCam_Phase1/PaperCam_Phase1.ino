/*
 * PaperCam — Phase 1: Panel bring-up
 *
 * Proves, in order: the combo number is right, the FPC is seated, the panel
 * orientation is what we think it is, a full refresh completes, and BUSY
 * deasserts instead of hanging.
 *
 * Two refreshes happen, ~15s apart:
 *
 *   Test A — geometry. Asymmetric markers so orientation is unambiguous.
 *   Test B — bit polarity. A raw packed 1bpp buffer, the same format the
 *            Phase 4 ditherer will emit.
 *
 * No camera, no button, no sleep. Those are later phases.
 */

#include "TFT_eSPI.h"

// Seeed GFX defines EPAPER_ENABLE internally once it recognises the
// BOARD_SCREEN_COMBO / board macro pair in driver.h. Seeed's own examples
// wrap their whole body in #ifdef EPAPER_ENABLE, which means a bad combo
// number compiles cleanly to an empty program and uploads without complaint —
// you get a blank panel and no error, exactly the failure your CLAUDE.md
// warns about. Turning that silent no-op into a compile error is worth the
// four lines.
#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE is not defined. Seeed GFX did not pick up driver.h, or the BOARD_SCREEN_COMBO / board macro pair in it is not a combination the library recognises."
#endif

EPaper epaper;

// ---------------------------------------------------------------------------
// Panel geometry and the packed-buffer format Phase 4 will target.
// ---------------------------------------------------------------------------

static constexpr int16_t  PANEL_W    = 800;
static constexpr int16_t  PANEL_H    = 480;
static constexpr size_t   ROW_BYTES  = PANEL_W / 8;              // 100
static constexpr size_t   BUF_BYTES  = ROW_BYTES * PANEL_H;      // 48,000

/*
 * The polarity test buffer.
 *
 * `static` at file scope means this is reserved at link time in SRAM, not
 * allocated at runtime — there is no malloc that can fail and nothing to free.
 * The MicroPython equivalent would be a module-level bytearray(48000), except
 * that here the 48KB is committed whether or not the code path ever runs.
 *
 * 48KB is comfortable against the ESP32-S3's ~512KB of SRAM, and being static
 * removes a whole class of failure from Phase 1. Phase 4 will move to PSRAM
 * (ps_malloc) because holding several 480KB camera frames for the sharpness
 * comparison will not fit here — but that is a Phase 4 problem, and doing it
 * now would be scaffolding ahead.
 */
static uint8_t polarity_buf[BUF_BYTES];

// ---------------------------------------------------------------------------

static void banner(const char *msg)
{
    Serial.print("\n=== ");
    Serial.print(msg);
    Serial.println(" ===");
}

// Runs epaper.update() and reports how long the panel took. Your CLAUDE.md
// expects roughly 5s; a number far off that, or no number at all because we
// never returned, is the headline result of this phase.
static void timed_update(const char *label)
{
    Serial.print(label);
    Serial.print(" — refreshing... ");
    Serial.flush();

    const uint32_t t0 = millis();
    epaper.update();
    const uint32_t elapsed = millis() - t0;

    Serial.print("done in ");
    Serial.print(elapsed);
    Serial.println(" ms");
}

// ---------------------------------------------------------------------------
// Test A — geometry and orientation
// ---------------------------------------------------------------------------

static void test_geometry(void)
{
    banner("Test A: geometry");

    epaper.fillScreen(TFT_WHITE);

    // Asymmetric corner markers. Two identical squares would tell us something
    // rendered but nothing about which way up it is; filled-vs-hollow in
    // opposite corners pins the origin down.
    epaper.fillRect(0, 0, 60, 60, TFT_BLACK);                       // top-left: solid
    epaper.drawRect(PANEL_W - 60, PANEL_H - 60, 60, 60, TFT_BLACK); // bottom-right: outline

    // A 1px border. If the panel is cropping or the driver is off by a row or
    // column, an edge goes missing here.
    epaper.drawRect(0, 0, PANEL_W, PANEL_H, TFT_BLACK);

    // 1px vertical lines every 100px. Any column scaling, doubling or dropout
    // shows up as uneven spacing or missing lines.
    for (int16_t x = 100; x < PANEL_W; x += 100) {
        epaper.drawFastVLine(x, 70, PANEL_H - 140, TFT_BLACK);
    }

    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(3);
    epaper.drawString("PaperCam Phase 1", 80, 100);
    epaper.setTextSize(2);
    epaper.drawString("Test A: geometry + orientation", 80, 150);
    epaper.drawString("solid square = top-left origin", 80, 180);

    timed_update("Test A");
}

// ---------------------------------------------------------------------------
// Test B — bit polarity
// ---------------------------------------------------------------------------

/*
 * Fills polarity_buf so the LEFT half is set bits and the RIGHT half is clear.
 *
 * MSB-first: bit 7 of a byte is its leftmost pixel. At 100 bytes per row, the
 * left half is bytes 0..49 and the right half is bytes 50..99.
 *
 * `memset` per row rather than one memset over the whole buffer, because the
 * halves are interleaved row by row in a packed layout — the left half of the
 * image is not a contiguous run of bytes.
 */
static void fill_polarity_buf(void)
{
    for (int16_t y = 0; y < PANEL_H; y++) {
        uint8_t *row = polarity_buf + (size_t)y * ROW_BYTES;
        memset(row,                  0xFF, ROW_BYTES / 2);  // left  half: bits set
        memset(row + ROW_BYTES / 2,  0x00, ROW_BYTES / 2);  // right half: bits clear
    }
}

static void test_polarity(void)
{
    banner("Test B: bit polarity");

    fill_polarity_buf();

    epaper.fillScreen(TFT_WHITE);

    // drawBitmap treats each set bit as the foreground colour and leaves clear
    // bits untouched. We are asserting "set bit = black" — the test is whether
    // the panel agrees.
    epaper.drawBitmap(0, 0, polarity_buf, PANEL_W, PANEL_H, TFT_BLACK);

    Serial.println("Left half = 0xFF (bits set), right half = 0x00 (bits clear).");
    Serial.println("Expect: LEFT BLACK, RIGHT WHITE.");

    timed_update("Test B");
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);

    // USB CDC needs a moment after enumeration or the first prints vanish.
    // Not an infinite while(!Serial) — that would hang the board on battery
    // with no host attached, which matters from Phase 7 onward.
    delay(2000);

    banner("PaperCam Phase 1 — panel bring-up");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Combo: %d\n", BOARD_SCREEN_COMBO);

    Serial.print("epaper.begin()... ");
    Serial.flush();
    epaper.begin();
    Serial.println("returned");

    // If these do not read 800 x 480, the combo number selected a different
    // panel and nothing below is trustworthy.
    Serial.printf("Reported geometry: %d x %d (expected %d x %d)\n",
                  epaper.width(), epaper.height(), PANEL_W, PANEL_H);

    test_geometry();

    Serial.println("\nHolding 15s — look at the panel now.");
    delay(15000);

    test_polarity();

    banner("Phase 1 complete");
    Serial.println("Report back: what each refresh drew, and the two timings.");
}

void loop(void)
{
    // Deliberately empty. Everything runs once in setup(); re-running a full
    // refresh on a loop would just age the panel.
}
