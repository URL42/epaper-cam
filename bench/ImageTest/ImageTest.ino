/*
 * PaperCam — image test (bench tool, not a build phase)
 *
 * Pushes a known-good image straight to the panel. No camera, no lens, no
 * sensor noise, no autoexposure.
 *
 * The point is to separate two questions that have been tangled together:
 *
 *   "how good can this panel look?"     <- this sketch answers it
 *   "how good can this camera get?"     <- everything else is bounded by it
 *
 * Whatever appears here is the ceiling. If a clean image still looks grainy,
 * the problem is the panel or our dither. If it looks excellent, the panel is
 * fine and every remaining complaint belongs to the OV2640.
 *
 * The image was processed on the Mac by dither/tune through the identical
 * tone + dither_fs_gray4 path the camera uses, so this is a fair comparison
 * rather than a flattering one.
 *
 * To use a different image:
 *
 *   ffmpeg -i photo.jpg -vf "scale=800:600:force_original_aspect_ratio=increase,crop=800:600,format=gray" frames/photo.pgm
 *   cd dither && ./tune ../frames/photo.pgm /tmp/x.pgm --gray4 \
 *       --emit-header ../PaperCam_ImageTest/image_data.h
 */

#include "TFT_eSPI.h"
#include "image_data.h"

#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif

EPaper epaper;

void setup(void)
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== PaperCam — image test ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("Image: %d bytes packed 4bpp\n", PAPERCAM_IMAGE_BYTES);

    epaper.begin();
    epaper.initGrayMode(GRAY_LEVEL4);

    uint8_t *fbuf = (uint8_t *)epaper.getPointer();
    if (!fbuf) {
        Serial.println("getPointer() returned NULL — not in grey mode?");
        return;
    }

    /*
     * The sprite is 4bpp at 800x480 with no row padding, which is exactly the
     * layout tune emitted — so this is a straight copy rather than a per-pixel
     * conversion. Verified against Sprite.cpp: it allocates ((w*h)>>1)+1 with
     * w forced even, so 192,001 bytes for our 192,000.
     */
    memcpy(fbuf, papercam_image, PAPERCAM_IMAGE_BYTES);

    Serial.print("refreshing... ");
    Serial.flush();
    const uint32_t t0 = millis();
    epaper.update();
    Serial.printf("%lu ms\n", (unsigned long)(millis() - t0));

    Serial.println("\nThis is the panel's ceiling. Anything the camera produces");
    Serial.println("can only be worse than what you are looking at.");
}

void loop(void)
{
}
