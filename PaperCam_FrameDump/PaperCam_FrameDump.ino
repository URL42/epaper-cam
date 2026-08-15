/*
 * PaperCam — frame dump (bench tool, not a build phase)
 *
 * Sends one raw 800x600 grayscale frame over serial so tone mapping can be
 * developed on the Mac against real photographs instead of guessed at through
 * 3.7-second panel refreshes.
 *
 * Trigger by sending 'c' (what tools/recv_frame.py does) or by pressing BOOT.
 *
 * Wire format:
 *
 *     PCFRAME <w> <h> <len> <sum32-hex>\n
 *     <len raw bytes, no encoding>
 *     PCEND\n
 *
 * Raw rather than base64: this is native USB CDC, where the nominal baud rate
 * is ignored and throughput is USB-limited, so 480,000 bytes moves in well
 * under a second. Base64 would cost 33% more for no benefit given the
 * receiver opens the port in raw mode.
 */

#include "esp_camera.h"

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

static constexpr int      SRC_W       = 800;
static constexpr int      SRC_H       = 600;
static constexpr uint8_t  PIN_SHUTTER = 0;     // BOOT
static constexpr uint32_t DEBOUNCE_MS = 25;

static bool     raw_pressed     = false;
static bool     stable_pressed  = false;
static uint32_t last_raw_change = 0;

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

static void dump_frame(void)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("ERR fb_get NULL");
        return;
    }

    const size_t expect = (size_t)SRC_W * SRC_H;
    if (fb->format != PIXFORMAT_GRAYSCALE ||
        fb->width != SRC_W || fb->height != SRC_H || fb->len != expect) {
        Serial.printf("ERR frame %ux%u %u bytes\n",
                      fb->width, fb->height, (unsigned)fb->len);
        esp_camera_fb_return(fb);
        return;
    }

    // Sum32 rather than a real CRC. The transport is USB with its own error
    // detection; this only needs to catch a truncated or misaligned read on
    // the host side, which it does perfectly well.
    uint32_t sum = 0;
    for (size_t i = 0; i < fb->len; i++) sum += fb->buf[i];

    Serial.printf("PCFRAME %d %d %u %08lx\n",
                  SRC_W, SRC_H, (unsigned)fb->len, (unsigned long)sum);

    // Chunked so a single enormous write cannot starve the USB task and trip
    // the watchdog.
    const size_t CHUNK = 4096;
    size_t sent = 0;
    while (sent < fb->len) {
        size_t n = fb->len - sent;
        if (n > CHUNK) n = CHUNK;
        Serial.write(fb->buf + sent, n);
        sent += n;
    }
    Serial.flush();

    Serial.println("PCEND");
    esp_camera_fb_return(fb);
}

// ---------------------------------------------------------------------------

void setup(void)
{
    Serial.begin(921600);
    delay(2000);

    Serial.println("\n=== PaperCam — frame dump ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    pinMode(PIN_SHUTTER, INPUT_PULLUP);
    delay(10);
    raw_pressed     = (digitalRead(PIN_SHUTTER) == LOW);
    stable_pressed  = raw_pressed;
    last_raw_change = millis();

    Serial.print("esp_camera_init... ");
    if (!camera_start()) return;
    Serial.println("OK");

    for (int i = 0; i < 4; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    Serial.println("READY — send 'c' or press BOOT to dump a frame.");
}

void loop(void)
{
    if (Serial.available()) {
        const int c = Serial.read();
        if (c == 'c' || c == 'C') dump_frame();
    }

    const uint32_t now = millis();
    const bool now_pressed = (digitalRead(PIN_SHUTTER) == LOW);

    if (now_pressed != raw_pressed) {
        raw_pressed     = now_pressed;
        last_raw_change = now;
    }
    if ((now - last_raw_change) >= DEBOUNCE_MS && raw_pressed != stable_pressed) {
        stable_pressed = raw_pressed;
        if (stable_pressed) dump_frame();
    }
}
