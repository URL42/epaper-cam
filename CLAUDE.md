# PaperCam — Hardware Reference

E-paper camera. Press a button, it takes a photo, dithers it to 4-level
greyscale, and paints it onto a 7.5" e-paper panel where it stays with zero
power draw.

This file is the authoritative hardware reference. Facts here were verified
against Seeed's wiki. Anything marked **VERIFY** has not been confirmed on
the bench yet — confirm before building on it. Items marked **CONFIRMED**
were measured on the bench, with the phase that measured them.

---

## Bill of materials (all on hand)

| Part | Notes |
|---|---|
| Seeed XIAO ESP32S3 **Sense** | **OV3660** camera (see below — not OV2640), 8MB PSRAM, 8MB flash |
| Seeed **ePaper Driver Board** (SKU 114993558) | JST BAT connector, charging IC, power switch, 24-pin FPC |
| Seeed 7.5" Monochrome ePaper, 800x480 (SKU p-5788) | Panel **T075A04**, UC8179 controller. Sold as B/W; does 4-level grey — see below |
| LiPo cell w/ JST 2.0mm | into the Driver Board's BAT connector |
| Momentary button | shutter, wired D5 to GND |

### Naming trap

Seeed sells three near-identical carriers. This project uses the **ePaper
Driver Board**, product URL slug `ePaper-breakout-Board-for-XIAO-V2-p-6374`
(the slug says "breakout"; the product is the Driver Board).

It is **not** the ePaper Breakout Board (p-5804) and **not** the EE04.
They have different pinouts. Getting this wrong means BUSY is on the wrong
pin and the panel hangs.

---

## Pin map

### Claimed by the ePaper Driver Board

| Signal | XIAO pin | ESP32-S3 GPIO |
|---|---|---|
| RST | D0 | GPIO1 |
| CS | D1 | GPIO2 |
| BUSY | D2 | GPIO3 |
| DC | D3 | GPIO4 |
| SCK | D8 | GPIO7 |
| MOSI | D10 | GPIO9 |

> The **Breakout** board puts BUSY on D5 instead. If you find a pinout with
> BUSY on D5, you are reading the wrong page.

### Free

D4 (GPIO5), D5 (GPIO6), D6 (GPIO43), D7 (GPIO44), D9 (GPIO8)

### Assignments for this project

| Pin | Use | Why |
|---|---|---|
| **D5 / GPIO6** | shutter button, INPUT_PULLUP to GND | RTC-capable, so it can wake from deep sleep |
| D4 / GPIO5 | reserved | pairs with D5 as I2C if a sensor is added later |
| D6, D7 | avoid | GPIO43/44 are UART0 TX/RX and not RTC-capable |
| D9 / GPIO8 | reserved | SPI MISO — needed only if the Sense's microSD is used |

### Camera

Connects through the board-to-board connector on the underside of the XIAO
itself, not the castellated pads. Uses internal GPIOs (10-18, 38-40, 47-48)
and does **not** collide with any panel pin. No wiring required. Pin map is in
the core's `camera_pins.h` under `CAMERA_MODEL_XIAO_ESP32S3`.

**CORRECTED (Phase 6): this board has an OV3660, not an OV2640.** Sensor PID
reads `0x3660`. Newer Sense batches ship the 3MP OV3660 in place of the 2MP
OV2640, and nothing in the listing or the wiki says so. This document asserted
OV2640 for six phases and it shaped several wrong conclusions.

It matters because of what the drivers implement. Probed at runtime — the only
reliable way, since esp32-camera ships precompiled:

| control | OV3660 (ours) |
|---|---|
| `set_sharpness` | **YES** |
| `set_denoise` | **YES** |
| `set_ae_level` | YES |
| `set_contrast` | YES |
| `set_brightness` | YES |

The OV2640 driver implements almost none of these. On the assumption of an
OV2640, the softness in our photos looked like a hardware ceiling; in fact
there was on-sensor edge enhancement and noise reduction sitting unused.

**Bench-chosen defaults: sharpness 3, denoise 5, ae_level 0.** Sharpness 3 is
the driver's ceiling. Note the sharpness metric rose 200 to 341 with it, but
that is not independent proof — edge enhancement raises variance-of-Laplacian
by construction. The visual comparison made the call. Exposure bias was tried
and made things worse.

**Native array is 2048x1536, not 1600x1200.** This retroactively invalidates
the Phase 5 scaler test, which concluded UXGA-plus-software-downsample was 10%
worse than SVGA-native: UXGA was already a downscale from the real array, so
the test did not measure what it claimed to. Unresolved, worth redoing.

---

## Arduino IDE settings

Board: **XIAO_ESP32S3**

| Setting | Value | Consequence if wrong |
|---|---|---|
| PSRAM | **OPI PSRAM** | off by default; camera init fails |
| USB CDC On Boot | **Enabled** | `Serial` output goes nowhere |
| Partition Scheme | 8M with spiffs, or Huge APP | build overflows |

The USB CDC trap bit us in Phase 1 and is worth spelling out: this board has
no USB-to-UART bridge, so with CDC off the core maps `Serial` to UART0 on
GPIO43/44 — physical pins with nothing attached. The port still enumerates
and esptool still flashes fine, because flashing uses the USB-Serial/JTAG
peripheral rather than `Serial`. **A working upload is not evidence that
`Serial` works.**

Don't trust the Tools menu; read what was actually compiled:

```bash
cat ~/Library/Caches/arduino/sketches/*/build.options.json
```

The `fqbn` field lists every menu selection. Want `CDCOnBoot=cdc` and
`PSRAM=opi`; `CDCOnBoot=default` means disabled.

---

## Libraries

- **Seeed GFX** (`Seeed-Studio/Seeed_Arduino_LCD`) — drives the panel.
  Conflicts with TFT_eSPI; only one may be installed.
- **esp32-camera** — bundled with the ESP32 Arduino core.

### driver.h

Lives in the sketch folder, not the library folder, so it survives library
updates and does not leak into other sketches.

```cpp
#define BOARD_SCREEN_COMBO 502   // 7.5" mono 800x480, UC8179
#define USE_XIAO_EPAPER_DRIVER_BOARD
```

**CONFIRMED (Phase 1).** Both values are right. The macro question is settled:
the Driver Board wiki's `USE_XIAO_EPAPER_BREAKOUT_BOARD` is a copy-paste error
from the Breakout page. Source for the correct pair is Seeed's 7.5" panel
Arduino cookbook, not the Driver Board page:

  https://wiki.seeedstudio.com/xiao_075inch_epaper_panel_arduino/

**Guard against the silent failure.** Seeed's examples wrap their bodies in
`#ifdef EPAPER_ENABLE`, which the library defines only when it recognises the
combo/board pair. A wrong combo therefore compiles to an empty program and
uploads without error — blank panel, nothing to debug. Always include:

```cpp
#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif
```

---

## Image pipeline constraints

- **CORRECTED (Phase 5): the panel does 4-level greyscale, and we use it.**
  This file previously claimed 1-bit only and that dithering was the only way
  to render tone. Both were wrong, and the error cost real time — it is the
  single most expensive mistake in this document so far.

  The T075A04 datasheet does say B/W, and stores a B/W waveform in on-chip
  OTP. Seeed GFX overrides it with its own LUTs (`LUT_VCOM_GRAY`,
  `LUT_WW_GRAY`, `LUT_KW_GRAY`, `LUT_WK_GRAY`, `LUT_KK_GRAY` in
  `UC8179_Defines.h`) and gets four genuinely distinct levels. Off-spec — the
  datasheet guarantees its optical figures "only under the controller &
  waveform provided by XingTai" — but confirmed on the bench.

  `epaper.initGrayMode(GRAY_LEVEL4)`. Note `GRAY_LEVEL16` exists in Seeed's
  newer examples but **not** in the installed library, and the panel's
  contrast would not support it anyway: white is L\*=63 and black L\*=32, so
  16 levels means ~2 L\* per step against a ghosting spec of ΔE ≤ 2. The steps
  would be the size of the noise. Four is the right number.

  Why it matters so much: at 2 levels every pixel is wrong by up to 127 and
  Floyd-Steinberg must smear that error over a wide area — that smearing *is*
  the visible stipple. At 4 levels worst-case error drops to ~42, diffusion
  stays local, and the texture largely disappears.

- No partial refresh planned.
- The sensor supports `PIXFORMAT_GRAYSCALE` natively — no JPEG decode, no
  RGB-to-luma step. The sensor hands over exactly what the dither needs.

- **`fb_count = 1` means one frame of latency, and it is bigger than it
  sounds.** With `GRAB_WHEN_EMPTY` the driver keeps a frame buffered, so
  `esp_camera_fb_get()` returns instantly — Phase 5's `capture 0 ms` was not a
  fast capture, it was no capture at all. That buffered frame was filled at
  the *start of the previous panel refresh*, so every Phase 5 photo showed a
  scene from roughly five seconds before the shutter. Invisible while the
  scene was static; obvious the moment a burst made it show the previous shot.
  **Always discard one frame before capturing anything you intend to keep.**
  **CONFIRMED (Phase 3):** `FRAMESIZE_SVGA` + `PIXFORMAT_GRAYSCALE` returns
  exactly 800x600 at 480,000 bytes, one byte per pixel.
- **Aspect ratio mismatch.** SVGA capture is 800x600 (4:3), panel is 800x480
  (5:3). Center-crop 120 rows: skip the first 60 and last 60. Every photo is
  landscape.
- Memory: 800x600 grayscale = 480KB, packed 1-bit output = 48KB. Both trivial
  in 8MB PSRAM. **CONFIRMED (Phase 3):** one frame buffer costs 481,376 bytes
  of PSRAM (480,000 plus driver overhead), leaving 7,904,720 free after
  `esp_camera_init`. That is headroom for roughly 16 frames, so Phase 6's
  burst-and-pick-sharpest is not memory constrained.
- Refresh, both **CONFIRMED** and deterministic to the millisecond across runs:

  | mode | refresh | vs mono |
  |---|---|---|
  | 1-bit (Phase 1) | 3433 ms | — |
  | 4-level grey (Phase 5) | 5074 ms | 1.48x |

  Greyscale costs 1.6s a shot. Worth it without argument. Full shot pipeline
  is capture ~0ms (the driver keeps a frame buffered), tone 30ms, dither
  135ms, paint 5074ms — about 5.2s shutter to image.

### Panel datasheet figures (T075A04)

From the manufacturer spec. The PDF is kept locally and deliberately not
committed — every page is footered "SEEKINK Confidential".

| | |
|---|---|
| Controller | UC8179 |
| Pixel pitch / DPI | 0.204mm / 124 |
| White state | L\* = 63 (~31.6% reflectance) |
| Black state | L\* = 32 (~7.1% reflectance) |
| Contrast ratio | ~4.5:1 |
| Ghosting | ΔE ≤ 2 |
| Charge per update | 100 mAs (0.028 mAh) |
| Update current | 11 mA max |
| Panel deep sleep | 7 µA max |

**The panel is not the power problem.** A 1000mAh cell covers roughly 36,000
refreshes on panel energy alone. Phase 7 should point at the Sense board's
sleep current instead. The datasheet also suggests updating at least once a
day for pixel health, which matters for something that sits in a frame.

### Packed buffer format

800x480 at 1bpp = 48,000 bytes, 100 bytes per row. MSB-first within each byte
(bit 7 = leftmost pixel).

**CONFIRMED (Phase 1): set bit = black.** A buffer with the left half `0xFF`
and the right half `0x00`, pushed via `drawBitmap(..., TFT_BLACK)`, rendered
left-black / right-white. So the ditherer emits **1 for black pixels** and we
pass `TFT_BLACK` as the foreground colour.

The "Reverse color" checkbox in Seeed's image-conversion docs was a red
herring — it describes *their* converter's output format. We generate our own
buffer and go through `drawBitmap`, whose convention is "set bit = the
foreground colour you passed." The library owns the panel's native polarity;
we only have to be consistent with the library.

**CONFIRMED (Phase 1): origin is top-left, no rotation needed.** Test A drew
its solid marker top-left and its hollow marker bottom-right as intended, and
Test B ruled out both 180° rotation and horizontal mirroring. Row 0 of the
packed buffer is the top row of the panel; write rows in natural order.

### 4-level greyscale format (what we actually use)

`initGrayMode(GRAY_LEVEL4)` replaces the framebuffer with a **4bpp sprite**:
two pixels per byte, **high nibble = even x**, values 0..3 where 0 is black.
800x480 = 192,000 bytes, 400 bytes per row, no row padding — verified in
`Sprite.cpp`, which allocates `((w*h)>>1)+1` with w forced even.

`dither_fs_gray4` emits exactly this, so it writes **straight into the sprite
via `getPointer()`**. No intermediate buffer, no copy, and the 1-bit path's
48KB `packed` array is gone rather than grown to 192KB.

**OPEN:** the ditherer assumes the four levels render at 0/85/170/255, evenly
spaced. They almost certainly do not — if the levels are perceptually even in
L\*, the real luminances are nearer 0/58/142/255. Error diffusion subtracts
the *assumed* value, so a wrong table becomes a systematic tonal shift the
algorithm can never correct for. Needs measuring; see the calibration sketch.

---

## Known risks

**Brownout during refresh.** The panel's charge pump spikes hard. If the ESP
resets mid-refresh, add 100uF electrolytic across 3V3/GND near the FPC
connector. **Did not occur on USB power (Phase 1)** — repeated full refreshes
completed with no reset. Still open on battery, which is a weaker supply;
re-check at Phase 7.

**Deep sleep current.** A bare XIAO ESP32S3 sleeps around 14uA. The Sense
expansion board with the camera attached is substantially worse and reports
vary widely. **Measure before designing the battery around it.** If it is
bad, the standard fix is cutting power to the camera board through a MOSFET.

**Headers.** Seeed's wiki specifies the pre-soldered XIAO variant. The Sense
usually ships with loose headers. Dry-fit before soldering — the camera board
adds a couple of millimeters under the XIAO.

---

## Style

- Small, reviewable commits. Conventional Commits.
- Explain architectural decisions before implementing them.
- One hardware variable at a time. Panel works before the camera is touched.
- Prefer clarity over cleverness; this is a project to understand, not just
  to run.
