/*
 * PaperCam — Phase 5: the first working camera
 *
 * Press the shutter: capture a frame, dither it, paint it on the panel.
 * Everything the previous four phases proved, wired together.
 *
 *   Phase 1  panel, combo 502, set-bit-is-black, top-left origin
 *   Phase 2  debounced button
 *   Phase 3  OV2640 at SVGA grayscale, 480,000 bytes in PSRAM
 *   Phase 4  Floyd-Steinberg into packed 1bpp
 *
 * No deep sleep, no burst, no sharpness selection. Those are Phases 6 and 7.
 */

#include "TFT_eSPI.h"
#include "esp_camera.h"
#include "src/dither.h"
#include "src/tone.h"

#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif

// ---------------------------------------------------------------------------
// Shutter. Still the onboard BOOT button; set to 0 once a switch is wired to
// D5. GPIO0 cannot be the final choice — the ROM re-samples strapping pins on
// deep-sleep wake, so Phase 7 needs D5 regardless.
// ---------------------------------------------------------------------------

#define USE_BOOT_BUTTON 1

#if USE_BOOT_BUTTON
static constexpr uint8_t  PIN_SHUTTER = 0;
static constexpr char     PIN_LABEL[] = "BOOT button (GPIO0, temporary)";
#else
static constexpr uint8_t  PIN_SHUTTER = D5;
static constexpr char     PIN_LABEL[] = "D5 (GPIO6)";
#endif

static constexpr uint32_t DEBOUNCE_MS = 25;

// ---------------------------------------------------------------------------
// Camera pins — XIAO ESP32S3 Sense, from the core's camera_pins.h.
// ---------------------------------------------------------------------------

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

// ---------------------------------------------------------------------------

EPaper epaper;

/*
 * Both buffers are static rather than heap or PSRAM.
 *
 * 48,000 + 3,208 bytes is comfortable in the S3's ~512KB of SRAM, and static
 * means they are reserved at link time: no allocation to fail, nothing to
 * free, and no fragmentation from repeated shots. The 480KB camera frame is a
 * different matter and lives in PSRAM, but the driver owns that one.
 *
 * SRAM is also meaningfully faster than PSRAM, and the ditherer touches the
 * scratch rows once per pixel — 384,000 times per photo.
 */
static uint8_t packed[DITHER_OUT_BYTES];
static int16_t scratch[DITHER_SCRATCH_LEN];

/*
 * Tone mapping. The curve came out of dither/tune, swept against three real
 * captures rather than guessed at — see TONE_DEFAULTS in tone.h for why the
 * contrast is so mild.
 *
 * The unsharp mask needs two full-frame scratch buffers, 480KB each. Those go
 * in PSRAM rather than SRAM for the obvious reason that 960KB does not fit in
 * 512KB, and we have 7.4MB spare. Allocated once at boot: a failure there is
 * something we want to hear about immediately, not on the first shutter press.
 */
// Named tone_cfg, not tone: the Arduino core declares tone(pin, freq, dur)
// for piezo buzzers in Arduino.h, and a variable called `tone` shadows it.
static const tone_params tone_cfg = TONE_DEFAULTS;
static uint8_t  tone_lut[256];
static uint8_t *tone_a = nullptr;
static uint8_t *tone_b = nullptr;

static bool     raw_pressed     = false;
static bool     stable_pressed  = false;
static uint32_t last_raw_change = 0;
static uint32_t shot_count      = 0;

// ---------------------------------------------------------------------------

static bool camera_start(void)
{
    camera_config_t cfg = {};

    cfg.pin_pwdn     = PWDN_GPIO_NUM;
    cfg.pin_reset    = RESET_GPIO_NUM;
    cfg.pin_xclk     = XCLK_GPIO_NUM;
    cfg.pin_sccb_sda = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl = SIOC_GPIO_NUM;

    cfg.pin_d7    = Y9_GPIO_NUM;
    cfg.pin_d6    = Y8_GPIO_NUM;
    cfg.pin_d5    = Y7_GPIO_NUM;
    cfg.pin_d4    = Y6_GPIO_NUM;
    cfg.pin_d3    = Y5_GPIO_NUM;
    cfg.pin_d2    = Y4_GPIO_NUM;
    cfg.pin_d1    = Y3_GPIO_NUM;
    cfg.pin_d0    = Y2_GPIO_NUM;
    cfg.pin_vsync = VSYNC_GPIO_NUM;
    cfg.pin_href  = HREF_GPIO_NUM;
    cfg.pin_pclk  = PCLK_GPIO_NUM;

    cfg.xclk_freq_hz = 20000000;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.pixel_format = PIXFORMAT_GRAYSCALE;
    cfg.frame_size   = FRAMESIZE_SVGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 1;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    const esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("esp_camera_init FAILED: 0x%04x (%s)\n", err, esp_err_to_name(err));
        return false;
    }
    return true;
}

static void show_message(const char *line1, const char *line2)
{
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(4);
    epaper.drawString(line1, 60, 190);
    if (line2) {
        epaper.setTextSize(2);
        epaper.drawString(line2, 60, 250);
    }
    epaper.update();
}

// ---------------------------------------------------------------------------

static void take_photo(void)
{
    shot_count++;
    Serial.printf("\n--- shot %lu ---\n", (unsigned long)shot_count);

    const uint32_t t0 = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("capture FAILED — esp_camera_fb_get returned NULL");
        return;
    }
    const uint32_t t_capture = millis();

    /*
     * Validate before dithering. dither_fs trusts its input completely — it
     * indexes DITHER_SRC_W * DITHER_SRC_H bytes with no bounds checks, because
     * adding them to a per-pixel inner loop would cost more than it is worth.
     * That trust has to be earned somewhere, and here is the somewhere: if the
     * driver ever hands back a different framesize, this catches it instead of
     * reading 480KB off the end of a smaller buffer.
     */
    const size_t expect = (size_t)DITHER_SRC_W * DITHER_SRC_H;
    if (fb->format != PIXFORMAT_GRAYSCALE ||
        fb->width  != DITHER_SRC_W ||
        fb->height != DITHER_SRC_H ||
        fb->len    != expect) {
        Serial.printf("frame mismatch: %ux%u, %u bytes — expected %ux%u, %u. Skipping.\n",
                      fb->width, fb->height, (unsigned)fb->len,
                      DITHER_SRC_W, DITHER_SRC_H, (unsigned)expect);
        esp_camera_fb_return(fb);
        return;
    }

    /*
     * Tone maps in place, into the driver's own frame buffer. That is safe —
     * we hold the borrow until fb_return, and the driver overwrites it on the
     * next capture anyway — and it saves copying 480KB for no benefit.
     */
    tone_apply(fb->buf, DITHER_SRC_W, DITHER_SRC_H, &tone_cfg, tone_lut,
               tone_a, tone_b);
    const uint32_t t_tone = millis();

    dither_fs(fb->buf, packed, scratch);
    const uint32_t t_dither = millis();

    /*
     * Hand the frame back before the refresh, not after. The panel update
     * blocks for ~3.4s, and holding the driver's only frame buffer across it
     * would stall the next capture for no reason. Borrow briefly, return
     * early — the dithered result is already safe in `packed`.
     */
    esp_camera_fb_return(fb);
    fb = nullptr;

    // drawBitmap only paints the set bits, so the background has to be
    // cleared first or the previous photo shows through.
    epaper.fillScreen(TFT_WHITE);
    epaper.drawBitmap(0, 0, packed, DITHER_OUT_W, DITHER_OUT_H, TFT_BLACK);
    epaper.update();
    const uint32_t t_done = millis();

    Serial.printf("capture %lu | tone %lu | dither %lu | paint %lu | total %lu ms\n",
                  (unsigned long)(t_capture - t0),
                  (unsigned long)(t_tone    - t_capture),
                  (unsigned long)(t_dither  - t_tone),
                  (unsigned long)(t_done    - t_dither),
                  (unsigned long)(t_done    - t0));
    Serial.printf("free heap %u, free PSRAM %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam Phase 5 — first working camera ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Shutter: %s\n", PIN_LABEL);

    pinMode(PIN_SHUTTER, INPUT_PULLUP);
    delay(10);
    raw_pressed     = (digitalRead(PIN_SHUTTER) == LOW);
    stable_pressed  = raw_pressed;
    last_raw_change = millis();

    Serial.print("epaper.begin()... ");
    epaper.begin();
    Serial.println("OK");

    Serial.print("esp_camera_init... ");
    if (!camera_start()) {
        show_message("CAMERA FAIL", "see serial");
        return;
    }
    Serial.println("OK");
    Serial.printf("free PSRAM after init: %u\n", (unsigned)ESP.getFreePsram());

    tone_build_lut(&tone_cfg, tone_lut);
    tone_a = (uint8_t *)ps_malloc((size_t)DITHER_SRC_W * DITHER_SRC_H);
    tone_b = (uint8_t *)ps_malloc((size_t)DITHER_SRC_W * DITHER_SRC_H);
    if (!tone_a || !tone_b) {
        Serial.println("ps_malloc for tone scratch FAILED");
        show_message("TONE ALLOC FAIL", "see serial");
        return;
    }
    Serial.printf("tone scratch allocated, free PSRAM: %u\n",
                  (unsigned)ESP.getFreePsram());

    // Discard a few frames so the first photo is not the AGC still settling.
    for (int i = 0; i < 3; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    show_message("PaperCam", "press the shutter");
    Serial.println("\nReady. Press the shutter.");
}

void loop(void)
{
    const uint32_t now = millis();
    const bool now_pressed = (digitalRead(PIN_SHUTTER) == LOW);

    if (now_pressed != raw_pressed) {
        raw_pressed     = now_pressed;
        last_raw_change = now;
    }

    if ((now - last_raw_change) >= DEBOUNCE_MS && raw_pressed != stable_pressed) {
        stable_pressed = raw_pressed;

        /*
         * Fires on the press edge, not the release. Phase 2 reported TAP on
         * release so it could tell a tap from a hold; with hold detection out
         * of the picture here, waiting for release would just add the length
         * of your press to the shutter lag. A camera should respond when you
         * push the button.
         */
        if (stable_pressed) {
            take_photo();
        }
    }
}
