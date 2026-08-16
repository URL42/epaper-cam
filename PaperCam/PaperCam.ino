/*
 * PaperCam — the camera.
 *
 * Press the shutter: a 3-second self-timer, then a burst of frames, scored by
 * variance of the Laplacian, the shaken ones discarded and the rest averaged.
 * Tone mapped, dithered to 4-level grey, painted on the panel, where it stays
 * with no power.
 *
 * Grew out of the phased bring-up: panel, button, camera, ditherer, first
 * working camera, burst. Those sketches are gone; bench/ keeps the tools that
 * are still useful for diagnosing hardware.
 *
 * The target is NOISE, not blur — a deliberate departure from the original
 * plan of "keep the sharpest frame". That plan is right when the enemy is
 * camera shake. By the time we reached this phase focus was fixed, greyscale
 * was working and the grey levels were calibrated, and the one visible defect
 * left was sensor grain in indoor light. Averaging N frames cuts random noise
 * by sqrt(N) — six frames is about 2.4x — and it is the last lever available
 * that does not involve a better lens.
 *
 * Sharpness scoring survives, demoted from picking a winner to rejecting
 * losers: averaging a shaken frame into still ones smears the result, so any
 * frame scoring well below the best in the burst is dropped rather than
 * blended in.
 *
 * No deep sleep. That is Phase 7.
 */

#include "TFT_eSPI.h"
#include "esp_camera.h"
#include "img_converters.h"   // fmt2jpg, ships with esp32-camera
#include <LittleFS.h>
#include "src/dither.h"
#include "src/tone.h"

#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif

// ---------------------------------------------------------------------------
// Shutter on D5 (GPIO6): momentary switch to GND, INPUT_PULLUP.
//
// D5 was always the destination. GPIO0 served during bring-up because no
// switch was wired yet, but it cannot be the final choice — the ROM re-samples
// strapping pins coming out of deep sleep, so a wake triggered by holding
// GPIO0 low would drop into download mode instead of taking a photo. GPIO6 is
// RTC-capable and carries no strapping role.
//
// Deep sleep will need more than INPUT_PULLUP: the pad moves to the RTC domain
// where the internal pullup stops holding, and the board then wakes on noise.
// Either rtc_gpio_pullup_en() plus pad hold, or an external 10k to 3V3.
// ---------------------------------------------------------------------------

#define USE_BOOT_BUTTON 0

#if USE_BOOT_BUTTON
static constexpr uint8_t  PIN_SHUTTER = 0;
static constexpr char     PIN_LABEL[] = "BOOT button (GPIO0, temporary)";
#else
static constexpr uint8_t  PIN_SHUTTER = D5;
static constexpr char     PIN_LABEL[] = "D5 (GPIO6)";
#endif

static constexpr uint32_t DEBOUNCE_MS = 25;

/*
 * Tap versus hold.
 *
 * Tap  (release before HOLD_MS) — take a photo.
 * Hold (still down at HOLD_MS)  — upload the last photo. Not wired up yet.
 *
 * The trigger moves from the press edge to the release, which normally costs
 * responsiveness. Here it costs nothing: the self-timer already imposes three
 * seconds before capture, so up to 1.5s spent deciding tap-or-hold is
 * invisible. The two features fit together by luck rather than design.
 *
 * Hold fires the instant the threshold passes rather than waiting for release,
 * so the LED can acknowledge it while your finger is still down — the same
 * reasoning as the original Phase 2 button, and the reason for hold_fired,
 * which suppresses the tap that would otherwise follow on release.
 */
static constexpr uint32_t HOLD_MS = 1500;

static uint32_t press_started = 0;
static bool     hold_fired    = false;

/*
 * Self-timer, so you can get into the shot.
 *
 * GPIO21 is the XIAO's user LED and clashes with nothing — the camera holds
 * 10-18, 38-40 and 47-48, the panel holds 1, 2, 3, 4, 7 and 9. It is active
 * LOW, which is why ON is 0 below.
 *
 * The blink accelerates over the last second. A steady blink tells you a timer
 * is running but not when it will fire; speeding up is what makes it possible
 * to be looking at the lens at the right moment rather than guessing.
 */
static constexpr uint8_t  PIN_LED  = LED_BUILTIN;   /* GPIO21 */
static constexpr uint8_t  LED_ON   = LOW;
static constexpr uint8_t  LED_OFF  = HIGH;
static constexpr uint32_t SELF_TIMER_DEFAULT_MS = 3000;

static uint32_t self_timer_ms = SELF_TIMER_DEFAULT_MS;

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
 * The dither's error rows: 3,208 bytes, static, so they are reserved at link
 * time with nothing to allocate and nothing to fail. SRAM rather than PSRAM
 * because the ditherer touches these once per pixel, 384,000 times a photo.
 *
 * There is no packed output buffer. In 4-level grey the framebuffer is a 4bpp
 * sprite Seeed GFX already allocated — 192,001 bytes, 400 per row, high nibble
 * = even x — which is exactly what dither_fs_gray4 emits, so we write straight
 * into it via getPointer().
 */
static int16_t scratch[DITHER_SCRATCH_LEN];

/*
 * Tone mapping. The curve came out of dither/tune swept against real captures
 * — see TONE_DEFAULTS in tone.h for why gamma is neutral and sharpening off.
 *
 * tone_a/tone_b are the unsharp mask's scratch, 480KB each, and are allocated
 * only when sharpening is actually on. With it off they would be 960KB of
 * PSRAM reserved for a code path that never runs, and the burst wants that
 * memory. tone_apply skips the mask when they are NULL.
 */
// Named tone_cfg, not tone: the Arduino core declares tone(pin, freq, dur)
// for piezo buzzers in Arduino.h, and a variable called `tone` shadows it.
//
// Not const any more — the serial keys below retune it live. Tone is the one
// part of this pipeline with no correct answer, only a preferred one, and a
// 40-second reflash per guess is the wrong feedback loop for a judgement call.
static tone_params tone_cfg = TONE_DEFAULTS;
static uint8_t  tone_lut[256];
static uint8_t *tone_a = nullptr;
static uint8_t *tone_b = nullptr;

/* ---------------------------------------------------------------------------
 * Burst.
 *
 * Frames are copied out of the driver's single buffer into our own PSRAM
 * slots, because scoring cannot decide what to reject until every frame in
 * the burst has been seen — and by then the driver has long since recycled
 * its one buffer.
 *
 * 6 x 480,000 = 2.88MB of frames, plus a 960KB uint16 accumulator, out of
 * 6.7MB free. uint16 is enough: 16 frames x 255 is 4080, well inside it.
 * ------------------------------------------------------------------------ */

static constexpr int BURST_MAX     = 8;
static constexpr int BURST_DEFAULT = 6;

/* A frame scoring below this fraction of the burst MEDIAN is assumed shaken
 * and left out of the average. Loose on purpose: consecutive frames of a
 * static scene score within a few percent of each other, so anything down at
 * 80% really did move. Measured against the median rather than the max —
 * see merge_burst() for why that distinction cost a whole burst. */
static constexpr float REJECT_BELOW = 0.80f;

static uint8_t  *burst[BURST_MAX] = { nullptr };
static uint16_t *accum            = nullptr;
static int       burst_n          = BURST_DEFAULT;

/*
 * Sensor defaults, found by A/B on the bench.
 *
 * The module is an OV3660 (PID 0x3660), not the OV2640 this project assumed
 * for its first six phases. That matters: the OV2640 driver implements almost
 * none of these controls, the OV3660 driver implements all of them. Edge
 * enhancement and noise reduction happen in the sensor's DSP, before its own
 * downscale to SVGA, which is a better place to do either than anywhere we
 * can reach in software.
 *
 * sharpness 3 is the driver's ceiling and visibly better. Note the sharpness
 * score jumped 200 -> 341 with it, but that is not independent evidence:
 * edge enhancement raises variance-of-Laplacian by construction whether the
 * detail is real or amplified noise. The eye made this call, not the metric.
 *
 * denoise 5 targets the grain that averaging alone did not finish off.
 *
 * ae_level stays 0 — biasing exposure was tried and made things worse.
 */
static constexpr int CAM_SHARPNESS = 3;
static constexpr int CAM_DENOISE   = 5;
static constexpr int CAM_AE_LEVEL  = 0;

static sensor_t *cam = nullptr;
static int cam_sharpness = CAM_SHARPNESS;
static int cam_denoise   = CAM_DENOISE;
static int cam_ae_level  = CAM_AE_LEVEL;

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
    // Grey-mode constants, not TFT_WHITE/TFT_BLACK: in a 4bpp sprite only
    // values 0..3 are meaningful, and 0xFFFF would mask down to 15.
    epaper.fillScreen(TFT_GRAY_3);
    epaper.setTextColor(TFT_GRAY_0);
    epaper.setTextSize(4);
    epaper.drawString(line1, 60, 190);
    if (line2) {
        epaper.setTextSize(2);
        epaper.drawString(line2, 60, 250);
    }
    epaper.update();
}

// ---------------------------------------------------------------------------

/*
 * Variance of the 3x3 Laplacian over the centre 400x300. Same metric as the
 * focus-assist tool, and the same reason for int64 accumulators: a single
 * Laplacian reaches +/-1020, its square ~1.04e6, and 120,000 of those sum
 * past what 32 bits holds. Overflow here would not crash, it would quietly
 * return a plausible wrong number and reject the wrong frames.
 */
static double frame_score(const uint8_t *buf)
{
    constexpr int WW = 400, WH = 300;
    constexpr int WX = (DITHER_SRC_W - WW) / 2;
    constexpr int WY = (DITHER_SRC_H - WH) / 2;

    int64_t sum = 0, sumsq = 0;

    for (int y = WY; y < WY + WH; y++) {
        const uint8_t *row = buf + (size_t)y * DITHER_SRC_W;
        const uint8_t *up  = row - DITHER_SRC_W;
        const uint8_t *dn  = row + DITHER_SRC_W;
        for (int x = WX; x < WX + WW; x++) {
            const int32_t lap = (int32_t)up[x] + dn[x] + row[x - 1] + row[x + 1]
                              - 4 * (int32_t)row[x];
            sum   += lap;
            sumsq += (int64_t)lap * lap;
        }
    }
    const double n = (double)WW * WH;
    const double m = (double)sum / n;
    return ((double)sumsq / n) - (m * m);
}

/* Grab burst_n frames into our own slots. Returns how many landed. */
static int capture_burst(void)
{
    const size_t expect = (size_t)DITHER_SRC_W * DITHER_SRC_H;
    int got = 0;

    /*
     * Throw the first frame away.
     *
     * With fb_count=1 and GRAB_WHEN_EMPTY the driver keeps one frame buffered
     * — which is why Phase 5 reported capture times of 0ms. That frame was
     * taken before the shutter, under whatever exposure the sensor had settled
     * on while idle, and it consistently scores ~30% above the frames captured
     * during the burst itself. Left in, it becomes a high outlier that drags
     * the rejection threshold up and disqualifies everything else.
     */
    camera_fb_t *stale = esp_camera_fb_get();
    if (stale) esp_camera_fb_return(stale);

    for (int i = 0; i < burst_n; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) continue;

        if (fb->format != PIXFORMAT_GRAYSCALE ||
            fb->width  != DITHER_SRC_W ||
            fb->height != DITHER_SRC_H ||
            fb->len    != expect) {
            Serial.printf("frame %d mismatch: %ux%u %u bytes — skipped\n",
                          i, fb->width, fb->height, (unsigned)fb->len);
            esp_camera_fb_return(fb);
            continue;
        }

        memcpy(burst[got], fb->buf, expect);
        esp_camera_fb_return(fb);
        got++;
    }
    return got;
}

/*
 * Score every frame, drop the ones well below the best, average the rest into
 * burst[0]. Returns how many frames went into the average.
 */
static int merge_burst(int got)
{
    const size_t n = (size_t)DITHER_SRC_W * DITHER_SRC_H;

    double score[BURST_MAX] = {0};
    for (int i = 0; i < got; i++) score[i] = frame_score(burst[i]);

    /*
     * Reject against the MEDIAN, not the maximum.
     *
     * Measured against the max, a single unusually sharp frame pulls the
     * threshold up and disqualifies the whole burst — which is exactly what
     * happened on the first run: scores of 266/204/204/205/204/204 threw away
     * five good frames because one was an outlier. The median is immune to
     * that by construction, which is the entire reason to prefer it here.
     *
     * Only frames BELOW the threshold are dropped. A frame sharper than its
     * neighbours is not a problem to be solved.
     */
    double sorted[BURST_MAX];
    memcpy(sorted, score, sizeof(double) * (size_t)got);
    for (int i = 1; i < got; i++) {              // insertion sort; got <= 8
        const double v = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = v;
    }
    const double median = (got & 1) ? sorted[got / 2]
                                    : (sorted[got / 2 - 1] + sorted[got / 2]) / 2.0;
    const double floor_score = median * REJECT_BELOW;

    Serial.print("  scores:");
    for (int i = 0; i < got; i++) Serial.printf(" %.0f", score[i]);
    Serial.printf("  (median %.0f, floor %.0f)\n", median, floor_score);

    memset(accum, 0, n * sizeof(uint16_t));

    int used = 0;
    for (int i = 0; i < got; i++) {
        if (median > 0 && score[i] < floor_score) {
            Serial.printf("  frame %d rejected (%.0f < %.0f)\n",
                          i, score[i], floor_score);
            continue;
        }
        const uint8_t *s = burst[i];
        for (size_t p = 0; p < n; p++) accum[p] = (uint16_t)(accum[p] + s[p]);
        used++;
    }

    if (used == 0) return 0;

    // Rounded divide: +used/2 before the division, or the average biases dark
    // by up to half a level across the whole frame.
    const uint16_t half = (uint16_t)(used / 2);
    uint8_t *out = burst[0];
    for (size_t p = 0; p < n; p++) {
        out[p] = (uint8_t)((accum[p] + half) / used);
    }
    return used;
}

/*
 * Blink the LED for the self-timer, accelerating over the final second, then
 * leave it solid while the shot is taken and processed.
 *
 * Blocking, deliberately. A shot already blocks for seven seconds and nothing
 * else is competing for the loop, so a state machine here would be complexity
 * bought with no return.
 */
static void self_timer(void)
{
    if (self_timer_ms == 0) return;

    Serial.printf("self-timer %lu ms...\n", (unsigned long)self_timer_ms);

    const uint32_t t0 = millis();
    uint32_t elapsed;
    while ((elapsed = millis() - t0) < self_timer_ms) {
        const uint32_t left = self_timer_ms - elapsed;
        const uint32_t half = (left > 1000) ? 250 : 75;   /* 2Hz, then ~6.7Hz */
        digitalWrite(PIN_LED, ((millis() / half) & 1) ? LED_OFF : LED_ON);
        delay(5);
    }
    digitalWrite(PIN_LED, LED_ON);   /* solid = capturing, hold still */
}

/* ---------------------------------------------------------------------------
 * Saving the last photo.
 *
 * One file, overwritten each shot, on the SPIFFS partition the 8M-with-spiffs
 * scheme already gives us. Flash rather than PSRAM because it has to survive
 * deep sleep — a photo you cannot upload after the device naps is not much of
 * an archive. A single ~60KB file rewritten a few times a day will not
 * meaningfully wear the flash in this decade.
 *
 * Encoded from the CROPPED, TONE-MAPPED frame, so the JPEG is exactly what the
 * panel shows minus the dithering. The alternative — the untoned negative —
 * would be the better choice for reprocessing a corpus later, but these are
 * meant to be looked at, and the tone curve is nearly neutral now anyway.
 * Switch the pointer below if that preference ever flips.
 * ------------------------------------------------------------------------ */

static constexpr char JPEG_PATH[]    = "/last.jpg";
static constexpr uint8_t JPEG_QUALITY = 85;

static size_t last_jpeg_bytes = 0;

static bool save_jpeg(const uint8_t *toned)
{
    // Rows are contiguous, so skipping to the first kept row and asking for
    // 480 of them frames it exactly as the panel does — no copy needed.
    const uint8_t *cropped = toned + (size_t)DITHER_CROP_TOP * DITHER_SRC_W;

    uint8_t *jpg = nullptr;
    size_t   len = 0;

    if (!fmt2jpg((uint8_t *)cropped,
                 (size_t)DITHER_OUT_W * DITHER_OUT_H,
                 DITHER_OUT_W, DITHER_OUT_H,
                 PIXFORMAT_GRAYSCALE, JPEG_QUALITY, &jpg, &len)) {
        Serial.println("  jpeg encode FAILED");
        return false;
    }

    // fs::File, not File: Seeed GFX inherits TFT_eSPI's FS_NO_GLOBALS, which
    // suppresses the `using fs::File` that would normally make it global.
    fs::File f = LittleFS.open(JPEG_PATH, "w");
    if (!f) {
        Serial.printf("  could not open %s for writing\n", JPEG_PATH);
        free(jpg);
        return false;
    }
    const size_t written = f.write(jpg, len);
    f.close();
    free(jpg);   // fmt2jpg allocates; the caller owns it

    if (written != len) {
        Serial.printf("  short write: %u of %u bytes (filesystem full?)\n",
                      (unsigned)written, (unsigned)len);
        return false;
    }

    last_jpeg_bytes = len;
    return true;
}

static void take_photo(void)
{
    shot_count++;
    Serial.printf("\n--- shot %lu ---\n", (unsigned long)shot_count);

    if (!accum || !burst[0]) {
        Serial.println("burst buffers not allocated");
        return;
    }

    // Printed once, on the first shot, because anything in setup() scrolls
    // away behind the shots that follow it.
    if (shot_count == 1 && cam) {
        Serial.printf("  sensor 0x%04x supports: sharpness %s | denoise %s | "
                      "ae_level %s | contrast %s | brightness %s\n",
                      cam->id.PID,
                      cam->set_sharpness  && cam->set_sharpness(cam, 0) == 0 ? "Y" : "n",
                      cam->set_denoise    && cam->set_denoise(cam, 0) == 0 ? "Y" : "n",
                      cam->set_ae_level   && cam->set_ae_level(cam, 0) == 0 ? "Y" : "n",
                      cam->set_contrast   && cam->set_contrast(cam, 0) == 0 ? "Y" : "n",
                      cam->set_brightness && cam->set_brightness(cam, 0) == 0 ? "Y" : "n");
    }

    // Countdown first, then start the clock — the timer is not shutter lag
    // and should not be reported as if it were.
    self_timer();

    const uint32_t t0 = millis();

    const int got = capture_burst();
    if (got == 0) {
        Serial.println("capture FAILED — no usable frames");
        return;
    }
    const uint32_t t_capture = millis();

    const int used = merge_burst(got);
    if (used == 0) {
        Serial.println("merge FAILED — every frame rejected");
        return;
    }
    const uint32_t t_merge = millis();

    // The merged result lives in burst[0], which we own outright — unlike
    // Phase 5, where tone mapped in place into the driver's buffer.
    tone_apply(burst[0], DITHER_SRC_W, DITHER_SRC_H, &tone_cfg, tone_lut,
               tone_a, tone_b);
    const uint32_t t_tone = millis();

    uint8_t *fbuf = (uint8_t *)epaper.getPointer();
    if (!fbuf) {
        Serial.println("getPointer() returned NULL — not in grey mode?");
        return;
    }
    dither_fs_gray4(burst[0], fbuf, scratch);
    const uint32_t t_dither = millis();

    epaper.update();
    const uint32_t t_done = millis();

    // After the refresh, deliberately. The photo is already on the panel by
    // this point, so encoding and writing costs the viewer nothing.
    const bool saved = save_jpeg(burst[0]);
    const uint32_t t_saved = millis();

    Serial.printf("burst %d/%d used | capture %lu | merge %lu | tone %lu | "
                  "dither %lu | paint %lu | total %lu ms\n",
                  used, got,
                  (unsigned long)(t_capture - t0),
                  (unsigned long)(t_merge   - t_capture),
                  (unsigned long)(t_tone    - t_merge),
                  (unsigned long)(t_dither  - t_tone),
                  (unsigned long)(t_done    - t_dither),
                  (unsigned long)(t_done    - t0));
    if (saved) {
        Serial.printf("saved %s: %u bytes in %lu ms\n", JPEG_PATH,
                      (unsigned)last_jpeg_bytes,
                      (unsigned long)(t_saved - t_done));
    }
    Serial.printf("free heap %u, free PSRAM %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());

    digitalWrite(PIN_LED, LED_OFF);
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Shutter: %s\n", PIN_LABEL);

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LED_OFF);

    pinMode(PIN_SHUTTER, INPUT_PULLUP);
    delay(10);
    raw_pressed     = (digitalRead(PIN_SHUTTER) == LOW);
    stable_pressed  = raw_pressed;
    last_raw_change = millis();

    Serial.print("epaper.begin()... ");
    epaper.begin();
    Serial.println("OK");

    /*
     * Four-level grey. The panel datasheet calls this a B/W display and keeps
     * a B/W waveform in OTP; Seeed GFX overrides it with its own LUTs. Off
     * spec, but confirmed on the bench as four distinct levels, and worth it:
     * at 2 levels the dither's worst-case error is 127 and the diffusion has
     * to smear it, which is the stipple. At 4 levels it is ~42 and the texture
     * largely disappears.
     */
    epaper.initGrayMode(GRAY_LEVEL4);
    Serial.println("initGrayMode(GRAY_LEVEL4)");

    Serial.print("esp_camera_init... ");
    if (!camera_start()) {
        show_message("CAMERA FAIL", "see serial");
        return;
    }
    Serial.println("OK");
    Serial.printf("free PSRAM after init: %u\n", (unsigned)ESP.getFreePsram());

    /*
     * Probe what this sensor actually implements. Each setter returns 0 on
     * success and -1 when the driver has no implementation for it, so calling
     * with a harmless value is a direct capability test.
     */
    cam = esp_camera_sensor_get();
    if (cam) {
        // Apply the bench-chosen values. Each returns 0 on success; a -1 here
        // would mean this is a different sensor from the one they were tuned
        // on, which is worth hearing about rather than silently ignoring.
        const int rs = cam->set_sharpness ? cam->set_sharpness(cam, CAM_SHARPNESS) : -1;
        const int rd = cam->set_denoise   ? cam->set_denoise(cam, CAM_DENOISE)     : -1;
        const int ra = cam->set_ae_level  ? cam->set_ae_level(cam, CAM_AE_LEVEL)   : -1;
        Serial.printf("sensor defaults: sharpness=%d(rc %d) denoise=%d(rc %d) ae=%d(rc %d)\n",
                      CAM_SHARPNESS, rs, CAM_DENOISE, rd, CAM_AE_LEVEL, ra);

        Serial.printf("sensor PID 0x%04x\n", cam->id.PID);
        Serial.println("control support:");
        Serial.printf("  sharpness  %s\n",
                      cam->set_sharpness && cam->set_sharpness(cam, 0) == 0 ? "YES" : "no");
        Serial.printf("  denoise    %s\n",
                      cam->set_denoise && cam->set_denoise(cam, 0) == 0 ? "YES" : "no");
        Serial.printf("  ae_level   %s\n",
                      cam->set_ae_level && cam->set_ae_level(cam, 0) == 0 ? "YES" : "no");
        Serial.printf("  contrast   %s\n",
                      cam->set_contrast && cam->set_contrast(cam, 0) == 0 ? "YES" : "no");
        Serial.printf("  brightness %s\n",
                      cam->set_brightness && cam->set_brightness(cam, 0) == 0 ? "YES" : "no");
    }

    // format-on-failure: a fresh board has no filesystem, and there is nothing
    // on it worth preserving over a working camera.
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount FAILED — photos will not be saved");
    } else {
        Serial.printf("LittleFS: %u of %u bytes used\n",
                      (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    }

    tone_build_lut(&tone_cfg, tone_lut);

    /*
     * Tone scratch is only for the unsharp mask, and sharpening defaults off,
     * so allocating 960KB unconditionally would be reserving PSRAM for a code
     * path that never runs — PSRAM the burst now wants. If sharpening is
     * enabled at runtime without these, tone_apply sees NULL and skips the
     * mask rather than misbehaving.
     */
    if (tone_cfg.sharp_amt > 0.0f) {
        tone_a = (uint8_t *)ps_malloc((size_t)DITHER_SRC_W * DITHER_SRC_H);
        tone_b = (uint8_t *)ps_malloc((size_t)DITHER_SRC_W * DITHER_SRC_H);
        Serial.printf("tone scratch: %s\n", (tone_a && tone_b) ? "allocated" : "FAILED");
    } else {
        Serial.println("tone scratch: skipped (sharpening off)");
    }

    const size_t frame_bytes = (size_t)DITHER_SRC_W * DITHER_SRC_H;
    for (int i = 0; i < BURST_MAX; i++) {
        burst[i] = (uint8_t *)ps_malloc(frame_bytes);
        if (!burst[i]) {
            // Not fatal. Cap the burst at whatever did fit and carry on —
            // fewer frames still averages, it just averages less.
            Serial.printf("burst slot %d failed to allocate; capping burst at %d\n", i, i);
            if (burst_n > i) burst_n = i;
            break;
        }
    }
    accum = (uint16_t *)ps_malloc(frame_bytes * sizeof(uint16_t));
    if (!accum || burst_n < 1) {
        Serial.println("burst allocation FAILED");
        show_message("BURST ALLOC FAIL", "see serial");
        return;
    }
    Serial.printf("burst %d frames + accumulator, free PSRAM: %u\n",
                  burst_n, (unsigned)ESP.getFreePsram());

    // Discard a few frames so the first photo is not the AGC still settling.
    for (int i = 0; i < 3; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    show_message("PaperCam", "press the shutter");
    Serial.println("\nReady. Press the shutter, or 'x' to shoot.");
    Serial.println("Tone keys (lower = less, upper = more):");
    Serial.println("  s/S sharpen   g/G gamma   c/C contrast   r/R radius");
    Serial.println("  0   sharpening off        ?   show current values");
    Serial.println("  n/N burst frames (1 = no averaging)   t/T self-timer +/-1s");
    Serial.println("  j   show the stored JPEG");
    Serial.println("Sensor keys (OV3660 supports all of these):");
    Serial.println("  [/] sensor sharpness   ;/' denoise   -/= exposure bias");
}

/*
 * Hold action. Upload lands here in step 3 of the Phase 7 build; for now it
 * only acknowledges, so the button behaviour can be confirmed on its own
 * before WiFi is in the picture.
 *
 * Three quick flashes — distinct from the self-timer's blink and from the
 * solid "capturing", so the gestures stay distinguishable across a room.
 */
static void on_hold(void)
{
    Serial.println("HOLD — upload not wired up yet");
    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED, LED_ON);  delay(60);
        digitalWrite(PIN_LED, LED_OFF); delay(60);
    }
}

static void print_tone(void)
{
    Serial.printf("tone: black=%u gamma=%.2f contrast=%.2f sharp=%.2f radius=%d\n",
                  tone_cfg.black, (double)tone_cfg.gamma, (double)tone_cfg.contrast,
                  (double)tone_cfg.sharp_amt, tone_cfg.sharp_rad);

    // Repeated on demand rather than only at boot. Anything printed during
    // setup scrolls off behind the first few shots, and this is the report
    // that decides whether the OV2640 can help us at all.
    if (!cam) { Serial.println("sensor: no handle"); return; }
    Serial.printf("sensor 0x%04x  sharpness=%d denoise=%d ae_level=%d\n",
                  cam->id.PID, cam_sharpness, cam_denoise, cam_ae_level);
    Serial.printf("  supports: sharpness %s | denoise %s | ae_level %s | "
                  "contrast %s | brightness %s\n",
                  cam->set_sharpness  && cam->set_sharpness(cam, cam_sharpness) == 0 ? "Y" : "n",
                  cam->set_denoise    && cam->set_denoise(cam, cam_denoise) == 0 ? "Y" : "n",
                  cam->set_ae_level   && cam->set_ae_level(cam, cam_ae_level) == 0 ? "Y" : "n",
                  cam->set_contrast   && cam->set_contrast(cam, 0) == 0 ? "Y" : "n",
                  cam->set_brightness && cam->set_brightness(cam, 0) == 0 ? "Y" : "n");
}

/*
 * Live tone tuning. Lower case decreases, upper case increases. Only the LUT
 * needs rebuilding after a change, and that is 256 iterations — free.
 */
static void handle_key(int c)
{
    switch (c) {
        case 's': tone_cfg.sharp_amt -= 0.1f; break;
        case 'S': tone_cfg.sharp_amt += 0.1f; break;
        case 'g': tone_cfg.gamma     -= 0.05f; break;
        case 'G': tone_cfg.gamma     += 0.05f; break;
        case 'c': tone_cfg.contrast  -= 0.05f; break;
        case 'C': tone_cfg.contrast  += 0.05f; break;
        case 'r': tone_cfg.sharp_rad  = (tone_cfg.sharp_rad > 1) ? tone_cfg.sharp_rad - 1 : 1; break;
        case 'R': tone_cfg.sharp_rad += 1; break;
        case '0': tone_cfg.sharp_amt  = 0.0f; break;   // sharpening fully off
        case 'x': take_photo();  return;
        case '?': print_tone();  return;

        /* Burst size. 1 disables averaging entirely, which is the direct A/B
         * against Phase 5 — same pipeline, single frame. */
        case 'j': {
            fs::File f = LittleFS.open(JPEG_PATH, "r");
            if (!f) { Serial.printf("%s: not present\n", JPEG_PATH); return; }
            Serial.printf("%s: %u bytes  (fs %u/%u used)\n", JPEG_PATH,
                          (unsigned)f.size(), (unsigned)LittleFS.usedBytes(),
                          (unsigned)LittleFS.totalBytes());
            f.close();
            return;
        }

        case 't':
            self_timer_ms = (self_timer_ms >= 1000) ? self_timer_ms - 1000 : 0;
            Serial.printf("self-timer = %lu ms\n", (unsigned long)self_timer_ms);
            return;
        case 'T':
            if (self_timer_ms < 10000) self_timer_ms += 1000;
            Serial.printf("self-timer = %lu ms\n", (unsigned long)self_timer_ms);
            return;

        case 'n':
            if (burst_n > 1) burst_n--;
            Serial.printf("burst = %d frames\n", burst_n);
            return;
        case 'N':
            if (burst_n < BURST_MAX && burst[burst_n]) burst_n++;
            Serial.printf("burst = %d frames\n", burst_n);
            return;

        /* Sensor-side controls. Reports the driver's return value so an
         * unsupported control says so rather than silently doing nothing. */
        case '[': case ']': case ';': case '\'': case '-': case '=': {
            if (!cam) { Serial.println("no sensor handle"); return; }
            int rc = -1;
            const char *what = "";
            switch (c) {
                case '[': cam_sharpness--; goto set_sharp;
                case ']': cam_sharpness++;
                set_sharp:
                    if (cam_sharpness < -3) cam_sharpness = -3;
                    if (cam_sharpness >  3) cam_sharpness =  3;
                    what = "sharpness";
                    rc = cam->set_sharpness ? cam->set_sharpness(cam, cam_sharpness) : -1;
                    Serial.printf("%s = %d  (rc %d%s)\n", what, cam_sharpness, rc,
                                  rc ? ", UNSUPPORTED" : "");
                    return;
                case ';': cam_denoise--; goto set_denoise;
                case '\'': cam_denoise++;
                set_denoise:
                    if (cam_denoise < 0) cam_denoise = 0;
                    if (cam_denoise > 8) cam_denoise = 8;
                    what = "denoise";
                    rc = cam->set_denoise ? cam->set_denoise(cam, cam_denoise) : -1;
                    Serial.printf("%s = %d  (rc %d%s)\n", what, cam_denoise, rc,
                                  rc ? ", UNSUPPORTED" : "");
                    return;
                case '-': cam_ae_level--; goto set_ae;
                case '=': cam_ae_level++;
                set_ae:
                    if (cam_ae_level < -2) cam_ae_level = -2;
                    if (cam_ae_level >  2) cam_ae_level =  2;
                    what = "ae_level";
                    rc = cam->set_ae_level ? cam->set_ae_level(cam, cam_ae_level) : -1;
                    Serial.printf("%s = %d  (rc %d%s)\n", what, cam_ae_level, rc,
                                  rc ? ", UNSUPPORTED" : "");
                    return;
            }
            return;
        }

        default:                 return;
    }

    if (tone_cfg.sharp_amt < 0.0f) tone_cfg.sharp_amt = 0.0f;
    if (tone_cfg.gamma     < 0.1f) tone_cfg.gamma     = 0.1f;
    if (tone_cfg.contrast  < 0.1f) tone_cfg.contrast  = 0.1f;

    tone_build_lut(&tone_cfg, tone_lut);
    print_tone();
    Serial.println("press shutter or 'x' to re-shoot");
}

void loop(void)
{
    if (Serial.available()) {
        handle_key(Serial.read());
    }

    const uint32_t now = millis();
    const bool now_pressed = (digitalRead(PIN_SHUTTER) == LOW);

    if (now_pressed != raw_pressed) {
        raw_pressed     = now_pressed;
        last_raw_change = now;
    }

    if ((now - last_raw_change) >= DEBOUNCE_MS && raw_pressed != stable_pressed) {
        stable_pressed = raw_pressed;

        if (stable_pressed) {
            // Press: start the clock, commit to nothing. Whether this is a tap
            // or a hold is not knowable yet.
            press_started = now;
            hold_fired    = false;
        } else if (!hold_fired) {
            // Released before the hold threshold, so it was a tap. If hold
            // already fired we stay quiet: one physical press, one action.
            Serial.printf("TAP (%lu ms)\n", (unsigned long)(now - press_started));
            take_photo();
        }
    }

    // Hold fires at the threshold rather than on release, so the LED can
    // acknowledge it while your finger is still down.
    if (stable_pressed && !hold_fired && (now - press_started) >= HOLD_MS) {
        hold_fired = true;
        on_hold();
    }
}
