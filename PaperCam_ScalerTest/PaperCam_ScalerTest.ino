/*
 * PaperCam — scaler A/B (bench tool, not a build phase)
 *
 * Question: is the OV2640's internal downscaler costing us detail?
 *
 * The sensor is natively 1600x1200. Asking it for SVGA makes its DSP do the
 * downscaling, and that scaler has a reputation for being soft. This captures
 * at full UXGA instead and box-downsamples 2x in software to the same 800x600,
 * then scores the identical centre window with the identical metric.
 *
 * Both paths end at 800x600, so the numbers are directly comparable against
 * the SVGA-native baseline. Do not move the rig between the two — the scene,
 * the focus and the lighting all have to stay put for the comparison to mean
 * anything.
 *
 * RISK: the OV2640 is widely limited to SVGA for *uncompressed* output
 * because of DMA bandwidth; UXGA is often JPEG-only. On the S3 it may work.
 * If it does not, init or the frame check below will say so plainly, and that
 * is a useful answer too — it would mean SVGA is simply the ceiling for
 * grayscale on this sensor.
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

static constexpr int CAP_W = 1600;          // UXGA capture
static constexpr int CAP_H = 1200;
static constexpr int OUT_W = 800;           // after 2x box downsample
static constexpr int OUT_H = 600;

// Identical window to the focus-assist baseline, so the scores compare.
static constexpr int WIN_W = 400;
static constexpr int WIN_H = 300;
static constexpr int WIN_X = (OUT_W - WIN_W) / 2;
static constexpr int WIN_Y = (OUT_H - WIN_H) / 2;

static constexpr double SVGA_BASELINE = 824.0;
static constexpr uint32_t PERIOD_MS   = 300;

static uint8_t *down = nullptr;             // 800x600 in PSRAM

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
    cfg.frame_size   = FRAMESIZE_UXGA;       // the thing under test
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 1;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    const esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("esp_camera_init FAILED: 0x%04x (%s)\n", err, esp_err_to_name(err));
        Serial.println("If this is ESP_ERR_NO_MEM or a timeout, UXGA grayscale");
        Serial.println("is beyond this sensor's uncompressed bandwidth — that is");
        Serial.println("the answer, and SVGA is the ceiling.");
        return false;
    }
    return true;
}

/* 2x2 box average, 1600x1200 -> 800x600. The +2 rounds to nearest rather
 * than truncating, which otherwise biases the whole image ~1.5 levels dark. */
static void downsample2x(const uint8_t *src, uint8_t *dst)
{
    for (int y = 0; y < OUT_H; y++) {
        const uint8_t *r0 = src + (size_t)(2 * y) * CAP_W;
        const uint8_t *r1 = r0 + CAP_W;
        uint8_t       *d  = dst + (size_t)y * OUT_W;

        for (int x = 0; x < OUT_W; x++) {
            const int i = 2 * x;
            d[x] = (uint8_t)((r0[i] + r0[i + 1] + r1[i] + r1[i + 1] + 2) >> 2);
        }
    }
}

static double laplacian_variance(const uint8_t *buf, int stride, uint32_t *mean_out)
{
    int64_t  sum   = 0;
    int64_t  sumsq = 0;
    uint64_t luma  = 0;

    for (int y = WIN_Y; y < WIN_Y + WIN_H; y++) {
        const uint8_t *row  = buf + (size_t)y * stride;
        const uint8_t *up   = row - stride;
        const uint8_t *dn   = row + stride;

        for (int x = WIN_X; x < WIN_X + WIN_W; x++) {
            const int32_t lap = (int32_t)up[x] + dn[x] + row[x - 1] + row[x + 1]
                              - 4 * (int32_t)row[x];
            sum   += lap;
            sumsq += (int64_t)lap * lap;
            luma  += row[x];
        }
    }

    const double n = (double)WIN_W * WIN_H;
    const double m = (double)sum / n;
    *mean_out = (uint32_t)(luma / (uint64_t)(WIN_W * WIN_H));
    return ((double)sumsq / n) - (m * m);
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam — scaler A/B (UXGA + software 2x) ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("SVGA-native baseline to beat: %.0f\n", SVGA_BASELINE);
    Serial.printf("PSRAM free: %u\n", (unsigned)ESP.getFreePsram());

    Serial.print("esp_camera_init at UXGA... ");
    if (!camera_start()) return;
    Serial.println("OK");
    Serial.printf("PSRAM after init: %u\n", (unsigned)ESP.getFreePsram());

    down = (uint8_t *)ps_malloc((size_t)OUT_W * OUT_H);
    if (!down) {
        Serial.println("ps_malloc for the downsample buffer FAILED");
        return;
    }

    for (int i = 0; i < 4; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    Serial.println("\nSame scene, same focus as the SVGA run. Do not move the rig.\n");
}

void loop(void)
{
    if (!down) { delay(1000); return; }

    const uint32_t t0 = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("fb_get NULL — UXGA grayscale is not delivering frames.");
        delay(PERIOD_MS);
        return;
    }

    const size_t expect = (size_t)CAP_W * CAP_H;
    if (fb->format != PIXFORMAT_GRAYSCALE ||
        fb->width != CAP_W || fb->height != CAP_H || fb->len != expect) {
        Serial.printf("got %ux%u, %u bytes — wanted %dx%d, %u. "
                      "The driver refused UXGA grayscale.\n",
                      fb->width, fb->height, (unsigned)fb->len,
                      CAP_W, CAP_H, (unsigned)expect);
        esp_camera_fb_return(fb);
        delay(1000);
        return;
    }
    const uint32_t t_cap = millis();

    downsample2x(fb->buf, down);
    esp_camera_fb_return(fb);
    const uint32_t t_down = millis();

    uint32_t     mean  = 0;
    const double score = laplacian_variance(down, OUT_W, &mean);

    const double delta = 100.0 * (score - SVGA_BASELINE) / SVGA_BASELINE;

    Serial.printf("UXGA+2x %8.1f  vs SVGA %.0f  = %+6.1f%%   luma %3lu  "
                  "| cap %lu ms  down %lu ms\n",
                  score, SVGA_BASELINE, delta, (unsigned long)mean,
                  (unsigned long)(t_cap  - t0),
                  (unsigned long)(t_down - t_cap));

    const uint32_t elapsed = millis() - t0;
    if (elapsed < PERIOD_MS) delay(PERIOD_MS - elapsed);
}
