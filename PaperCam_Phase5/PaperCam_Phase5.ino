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
/*
 * No packed output buffer any more. In 4-level grey mode the framebuffer is a
 * 4bpp sprite that Seeed GFX already allocated — 192,001 bytes, 400 bytes per
 * row, high nibble = even x — which is exactly the layout dither_fs_gray4
 * emits. So we dither straight into it via getPointer() and the old 48KB
 * 1-bit buffer disappears rather than growing to 192KB.
 */
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
//
// Not const any more — the serial keys below retune it live. Tone is the one
// part of this pipeline with no correct answer, only a preferred one, and a
// 40-second reflash per guess is the wrong feedback loop for a judgement call.
static tone_params tone_cfg = TONE_DEFAULTS;
static uint8_t  tone_lut[256];
static uint8_t *tone_a = nullptr;
static uint8_t *tone_b = nullptr;

/*
 * Sensor-side controls. The OV2640's DSP may implement edge enhancement and
 * noise reduction in hardware — free compared with doing either in software,
 * and aimed exactly at the two things still wrong with our photos.
 *
 * "May" is the operative word: sensor_t exposes set_sharpness and set_denoise
 * for every sensor, but each driver returns -1 for the ones it does not
 * implement, and esp32-camera ships precompiled so the only way to find out
 * is to ask the chip.
 */
static sensor_t *cam = nullptr;
static int cam_sharpness = 0;
static int cam_denoise   = 0;
static int cam_ae_level  = 0;

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

    /*
     * Dither straight into the sprite. getPointer() is the buffer the library
     * will hand to its own gray push, so there is no intermediate copy and no
     * second 192KB allocation.
     */
    uint8_t *fbuf = (uint8_t *)epaper.getPointer();
    if (!fbuf) {
        Serial.println("getPointer() returned NULL — not in grey mode?");
        esp_camera_fb_return(fb);
        return;
    }
    dither_fs_gray4(fb->buf, fbuf, scratch);
    const uint32_t t_dither = millis();

    /*
     * Hand the frame back before the refresh, not after. The panel update
     * blocks for ~3.4s, and holding the driver's only frame buffer across it
     * would stall the next capture for no reason. Borrow briefly, return
     * early — the dithered result is already safe in the sprite.
     */
    esp_camera_fb_return(fb);
    fb = nullptr;

    // No fillScreen: dither_fs_gray4 writes every pixel of the sprite, so
    // there is nothing left of the previous photo to clear.
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
    Serial.println("\nReady. Press the shutter, or 'x' to shoot.");
    Serial.println("Tone keys (lower = less, upper = more):");
    Serial.println("  s/S sharpen   g/G gamma   c/C contrast   r/R radius");
    Serial.println("  0   sharpening off        ?   show current values");
    Serial.println("Sensor keys (report UNSUPPORTED if the OV2640 lacks them):");
    Serial.println("  [/] sensor sharpness   ;/' denoise   -/= exposure bias");
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
