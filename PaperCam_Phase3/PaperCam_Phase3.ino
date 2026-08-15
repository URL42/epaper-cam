/*
 * PaperCam — Phase 3: Camera bring-up
 *
 * Init the OV2640 at SVGA grayscale, capture one frame, report what we
 * actually got and a luma histogram. Panel untouched — no TFT_eSPI, no
 * driver.h. One capture at boot; press RESET to take another.
 *
 * The question this phase answers: does PIXFORMAT_GRAYSCALE genuinely work
 * at FRAMESIZE_SVGA? The whole image pipeline is designed around the sensor
 * handing us 8-bit luma directly, with no JPEG decode and no RGB-to-luma
 * step. If that assumption is wrong, better to know now than at Phase 5.
 */

#include "esp_camera.h"

// ---------------------------------------------------------------------------
// Pin map for the XIAO ESP32S3 Sense.
//
// Copied verbatim from the ESP32 core's own camera_pins.h, under
// CAMERA_MODEL_XIAO_ESP32S3:
//
//   ~/Library/Arduino15/packages/esp32/hardware/esp32/3.3.11/libraries/
//       ESP32/examples/Camera/CameraWebServer/camera_pins.h
//
// Inlined rather than #included because that header lives in an examples
// folder that is not on the sketch's include path. If the core is updated,
// re-check it against this block.
//
// The OV2640 reaches these through the board-to-board connector under the
// XIAO, not the castellated pads. Nothing here touches the panel's pins
// (GPIO 1, 2, 3, 4, 7, 9), so camera and ePaper can coexist.
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

static constexpr uint16_t EXPECT_W = 800;   // SVGA
static constexpr uint16_t EXPECT_H = 600;
static constexpr uint8_t  WARMUP_FRAMES = 3;
static constexpr uint8_t  HIST_BUCKETS  = 16;

// ---------------------------------------------------------------------------

static const char *pixformat_name(pixformat_t f)
{
    switch (f) {
        case PIXFORMAT_GRAYSCALE: return "GRAYSCALE";
        case PIXFORMAT_JPEG:      return "JPEG";
        case PIXFORMAT_RGB565:    return "RGB565";
        case PIXFORMAT_YUV422:    return "YUV422";
        default:                  return "other";
    }
}

static bool camera_start(void)
{
    camera_config_t cfg = {};

    cfg.pin_pwdn     = PWDN_GPIO_NUM;
    cfg.pin_reset    = RESET_GPIO_NUM;
    cfg.pin_xclk     = XCLK_GPIO_NUM;
    cfg.pin_sccb_sda = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl = SIOC_GPIO_NUM;

    cfg.pin_d7  = Y9_GPIO_NUM;
    cfg.pin_d6  = Y8_GPIO_NUM;
    cfg.pin_d5  = Y7_GPIO_NUM;
    cfg.pin_d4  = Y6_GPIO_NUM;
    cfg.pin_d3  = Y5_GPIO_NUM;
    cfg.pin_d2  = Y4_GPIO_NUM;
    cfg.pin_d1  = Y3_GPIO_NUM;
    cfg.pin_d0  = Y2_GPIO_NUM;
    cfg.pin_vsync = VSYNC_GPIO_NUM;
    cfg.pin_href  = HREF_GPIO_NUM;
    cfg.pin_pclk  = PCLK_GPIO_NUM;

    cfg.xclk_freq_hz = 20000000;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.ledc_channel = LEDC_CHANNEL_0;

    // The whole point of this phase.
    cfg.pixel_format = PIXFORMAT_GRAYSCALE;
    cfg.frame_size   = FRAMESIZE_SVGA;      // 800x600

    // Ignored for GRAYSCALE, but the driver reads them regardless.
    cfg.jpeg_quality = 12;

    // One buffer for now. Phase 6 raises this for the burst-and-pick-sharpest
    // work; raising it here would be scaffolding ahead with no way to test it.
    cfg.fb_count    = 1;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;

    const esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("esp_camera_init FAILED: 0x%04x (%s)\n", err, esp_err_to_name(err));
        return false;
    }
    return true;
}

/*
 * Grab and immediately discard a few frames.
 *
 * The OV2640's auto-exposure and auto-gain converge over several frames. The
 * first one out of a cold init is typically far too dark or blown out, which
 * would make the histogram below describe the AGC's startup transient rather
 * than the scene. Cheap insurance.
 */
static void warmup(void)
{
    for (uint8_t i = 0; i < WARMUP_FRAMES; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            esp_camera_fb_return(fb);
        }
    }
}

struct LumaStats {
    uint8_t  lo;
    uint8_t  hi;
    uint32_t mean;
};

static LumaStats histogram(const uint8_t *buf, size_t len)
{
    uint32_t bucket[HIST_BUCKETS] = {0};
    uint32_t sum   = 0;
    uint8_t  lo    = 255;
    uint8_t  hi    = 0;

    for (size_t i = 0; i < len; i++) {
        const uint8_t v = buf[i];
        bucket[v >> 4]++;          // 256 levels into 16 buckets
        sum += v;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }

    Serial.printf("\nLuma: min %u  max %u  mean %lu\n",
                  lo, hi, (unsigned long)(sum / len));

    // Scale bars against the fullest bucket so the shape is always readable.
    uint32_t peak = 1;
    for (uint8_t b = 0; b < HIST_BUCKETS; b++) {
        if (bucket[b] > peak) peak = bucket[b];
    }

    for (uint8_t b = 0; b < HIST_BUCKETS; b++) {
        const uint8_t bars = (uint8_t)((bucket[b] * 40) / peak);
        Serial.printf("%3u-%3u |", b * 16, b * 16 + 15);
        for (uint8_t i = 0; i < bars; i++) Serial.print('#');
        Serial.printf("  %lu\n", (unsigned long)bucket[b]);
    }

    return LumaStats{ lo, hi, sum / len };
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam Phase 3 — camera bring-up ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    // If PSRAM is missing the frame buffer has nowhere to live and init will
    // fail confusingly. Check the cause before the symptom.
    Serial.printf("PSRAM: %u bytes total, %u free\n",
                  (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
    if (ESP.getPsramSize() == 0) {
        Serial.println("PSRAM NOT DETECTED — set Tools > PSRAM to OPI PSRAM.");
        return;
    }

    Serial.print("esp_camera_init... ");
    if (!camera_start()) {
        return;
    }
    Serial.println("OK");
    Serial.printf("PSRAM after init: %u free\n", (unsigned)ESP.getFreePsram());

    Serial.printf("Discarding %u warmup frames for AGC/AWB to settle...\n", WARMUP_FRAMES);
    warmup();

    /*
     * esp_camera_fb_get() hands back a pointer to a driver-owned buffer, not
     * a copy. We borrow it; the driver still owns it. Every successful get
     * must be matched by exactly one esp_camera_fb_return(), or the driver
     * runs out of frame buffers and the next get() blocks or returns NULL.
     *
     * This is the ownership discipline that has no MicroPython equivalent —
     * there is no GC to notice we are done and no context manager to close
     * it for us. Phase 5 and Phase 6 will hold several of these at once, so
     * the habit is worth forming here where there is only one to lose.
     */
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("esp_camera_fb_get returned NULL — no frame captured.");
        return;
    }

    Serial.printf("\nCaptured: %u x %u, format %s, %u bytes\n",
                  fb->width, fb->height, pixformat_name(fb->format),
                  (unsigned)fb->len);

    // Copy the fields we need before handing the buffer back. After
    // esp_camera_fb_return the struct belongs to the driver again and may be
    // reused by the next capture — reading fb->anything past that point is a
    // use-after-free that will usually appear to work, which is worse than
    // crashing.
    const uint16_t     got_w   = fb->width;
    const uint16_t     got_h   = fb->height;
    const size_t       got_len = fb->len;
    const pixformat_t  got_fmt = fb->format;

    const LumaStats stats = histogram(fb->buf, got_len);

    esp_camera_fb_return(fb);
    fb = nullptr;   // the pointer is dangling now; make that explicit

    // --- Verdict ----------------------------------------------------------
    //
    // Printed last, on purpose. A serial monitor scrolled to the bottom shows
    // the end of the output, and the answer this phase exists to produce
    // should not be sitting off-screen above a 16-line histogram.

    const size_t expect_len = (size_t)EXPECT_W * EXPECT_H;

    const bool ok_fmt = (got_fmt == PIXFORMAT_GRAYSCALE);
    const bool ok_dim = (got_w == EXPECT_W && got_h == EXPECT_H);
    const bool ok_len = (got_len == expect_len);

    Serial.println("\n--- VERDICT ---");
    Serial.printf("Format : %-10s (want GRAYSCALE)   %s\n",
                  pixformat_name(got_fmt), ok_fmt ? "OK" : "MISMATCH");
    Serial.printf("Size   : %u x %u    (want %u x %u)   %s\n",
                  got_w, got_h, EXPECT_W, EXPECT_H, ok_dim ? "OK" : "MISMATCH");
    Serial.printf("Length : %u bytes  (want %u)      %s\n",
                  (unsigned)got_len, (unsigned)expect_len, ok_len ? "OK" : "MISMATCH");
    Serial.printf("Luma   : min %u  max %u  mean %lu\n",
                  stats.lo, stats.hi, (unsigned long)stats.mean);

    if (ok_fmt && ok_dim && ok_len) {
        Serial.println("\nPHASE 3 PASS — grayscale SVGA confirmed, one byte per pixel.");
    } else {
        Serial.println("\nPHASE 3 FAIL — the driver did not give us what we asked for.");
        Serial.println("Phase 4 assumes 8-bit luma; do not proceed on a mismatch.");
    }

    Serial.println("Press RESET to capture again.");
}

void loop(void)
{
    // Single capture in setup(). Nothing here yet; the button arrives in
    // Phase 5 when there is something worth triggering.
}
