/*
 * PaperCam — Focus assist (bench tool, not a build phase)
 *
 * Prints a sharpness score several times a second so a fixed-focus lens can
 * be set by watching a number peak instead of guessing. Rotate the OV2640's
 * lens barrel a little at a time; the bar fills as the image gets sharper.
 *
 * Serial only. No panel, no driver.h, no dither — a 3.7s e-paper refresh in
 * this loop would make it useless.
 *
 * The metric is variance of the Laplacian, which is Phase 6's sharpness score
 * brought forward as a diagnostic. A sharp image has strong local intensity
 * changes, so its second derivative has a wide spread; a blurred one has the
 * detail smeared out and the variance collapses. It is scene-dependent —
 * compare readings of the SAME subject at the same distance, never across
 * different scenes.
 */

#include "esp_camera.h"

// --- XIAO ESP32S3 Sense, from the core's camera_pins.h ---------------------

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

static constexpr int SRC_W = 800;
static constexpr int SRC_H = 600;

/*
 * Score the centre of the frame rather than all of it. That is where you are
 * pointing at whatever you are focusing on, and 400x300 costs a quarter of
 * the work of the full frame — which is the difference between a responsive
 * readout and a laggy one. Being interior also means the 3x3 Laplacian never
 * needs an edge case.
 */
static constexpr int WIN_W = 400;
static constexpr int WIN_H = 300;
static constexpr int WIN_X = (SRC_W - WIN_W) / 2;
static constexpr int WIN_Y = (SRC_H - WIN_H) / 2;

static constexpr uint32_t PERIOD_MS = 160;   // ~6 readings/sec, readable
static constexpr int      BAR_CELLS = 24;

static double peak = 1.0;

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

/*
 * Variance of the 3x3 Laplacian over the centre window, plus mean luma.
 *
 *      0  1  0
 *      1 -4  1
 *      0  1  0
 *
 * Sum and sum-of-squares go in int64_t deliberately. A single Laplacian value
 * reaches +/-1020, so its square reaches ~1.04e6, and 120,000 of those sum to
 * ~1.2e11 — comfortably past what a 32-bit accumulator holds. Overflowing it
 * would not crash; it would quietly report a nonsense score, which is worse.
 */
static double laplacian_variance(const uint8_t *buf, uint32_t *mean_out)
{
    int64_t  sum   = 0;
    int64_t  sumsq = 0;
    uint64_t luma  = 0;

    for (int y = WIN_Y; y < WIN_Y + WIN_H; y++) {
        const uint8_t *row  = buf + (size_t)y * SRC_W;
        const uint8_t *up   = row - SRC_W;
        const uint8_t *down = row + SRC_W;

        for (int x = WIN_X; x < WIN_X + WIN_W; x++) {
            const int32_t lap = (int32_t)up[x] + down[x] + row[x - 1] + row[x + 1]
                              - 4 * (int32_t)row[x];
            sum   += lap;
            sumsq += (int64_t)lap * lap;
            luma  += row[x];
        }
    }

    const double n    = (double)WIN_W * WIN_H;
    const double m    = (double)sum / n;
    const double var  = ((double)sumsq / n) - (m * m);

    *mean_out = (uint32_t)(luma / (uint64_t)(WIN_W * WIN_H));
    return var;
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam — focus assist ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Scoring centre %dx%d of %dx%d\n", WIN_W, WIN_H, SRC_W, SRC_H);

    Serial.print("esp_camera_init... ");
    if (!camera_start()) return;
    Serial.println("OK");

    for (int i = 0; i < 3; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    Serial.println("\nPoint at a high-contrast subject 2-3m away, then turn the");
    Serial.println("lens barrel slowly. Watch for the score to peak.");
    Serial.println("The bar is relative to the best score seen recently.\n");
}

void loop(void)
{
    const uint32_t t0 = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("fb_get NULL");
        delay(PERIOD_MS);
        return;
    }

    if (fb->format != PIXFORMAT_GRAYSCALE ||
        fb->width != SRC_W || fb->height != SRC_H) {
        Serial.printf("unexpected frame %ux%u\n", fb->width, fb->height);
        esp_camera_fb_return(fb);
        delay(PERIOD_MS);
        return;
    }

    uint32_t     mean  = 0;
    const double score = laplacian_variance(fb->buf, &mean);

    esp_camera_fb_return(fb);

    /*
     * Peak decays slowly rather than latching. A plain running maximum is
     * useless the moment you overshoot the sweet spot — the bar would sit at
     * a fraction of a number you can never beat again and tell you nothing.
     * Decaying lets the reference drift back down so the bar stays meaningful
     * while you hunt.
     */
    peak *= 0.98;
    if (score > peak) peak = score;

    int cells = (int)((score / peak) * BAR_CELLS);
    if (cells < 0)         cells = 0;
    if (cells > BAR_CELLS) cells = BAR_CELLS;

    char bar[BAR_CELLS + 1];
    for (int i = 0; i < BAR_CELLS; i++) bar[i] = (i < cells) ? '#' : ' ';
    bar[BAR_CELLS] = '\0';

    Serial.printf("sharp %8.1f [%s] peak %8.1f  luma %3lu%s\n",
                  score, bar, peak, (unsigned long)mean,
                  (mean < 40) ? "  (DARK — add light)" :
                  (mean > 215) ? "  (BLOWN — too bright)" : "");

    const uint32_t elapsed = millis() - t0;
    if (elapsed < PERIOD_MS) delay(PERIOD_MS - elapsed);
}
