# PaperCam

An e-paper camera. Press the shutter, wait three seconds, and a photograph
appears on a 7.5" e-paper panel and stays there with no power at all — through
deep sleep, through a battery swap, indefinitely.

Tap to shoot. Hold to upload the last photo over WiFi. It sleeps after five
idle minutes and wakes when you press the button, with the picture still on
the glass.

Built on a Seeed XIAO ESP32S3 Sense and a 7.5" 800×480 UC8179 panel. The
interesting part is that the panel is sold as black-and-white, and this drives
it in **4-level greyscale** — which turns out to be the difference between a
photograph and a stippled mess.

## Hardware

| Part | Notes |
|---|---|
| Seeed XIAO ESP32S3 **Sense** | 8MB PSRAM, 8MB flash. Camera is an **OV3660** on recent batches, not the OV2640 the listing implies |
| Seeed **ePaper Driver Board** (SKU 114993558) | Not the Breakout Board — see below |
| 7.5" mono ePaper, 800×480 (SKU p-5788) | Panel T075A04, UC8179 controller |
| Momentary switch | shutter, D5 to GND |

## How it works

```
tap     → 3s self-timer (LED countdown)
        → burst of 6 frames, SVGA greyscale
        → score each by variance of Laplacian, drop the shaken ones
        → average the survivors            (noise ÷ √N)
        → tone curve                        (256-entry LUT)
        → Floyd-Steinberg, serpentine, to 4-level grey
        → straight into the panel's sprite buffer
        → refresh                           (~5.1s)
        → JPEG to flash                     (~40KB, survives deep sleep)

hold    → wifi up → POST the last JPEG → wifi off
idle 5m → deep sleep, panel keeps its image, wake on the shutter
```

About 7.7 seconds shutter to image, of which 5.1 is the panel itself.

WiFi is off unless you ask for it. Idle WiFi is what drains a battery; a
connect-send-disconnect cycle costs roughly 0.15 mAh, so uploading is
affordable and never happens behind your back. Because it is a deliberate
gesture there is no retry queue and no silent failure — if it fails, hold
again. Nothing about uploading can stop the camera taking photographs.

The dither and tone modules are plain C11 with no Arduino dependencies, so
they build natively on a host as well as inside the sketch. `dither/` has a
test harness and a tone-curve playground that runs against real captured
frames in milliseconds — considerably better than reflashing and squinting.

## Two things that cost us a day

**Seeed's Driver Board wiki has a copy-paste error.** It shows
`USE_XIAO_EPAPER_BREAKOUT_BOARD` in its example. The Driver Board needs
`USE_XIAO_EPAPER_DRIVER_BOARD`; the Breakout Board puts BUSY on a different
pin. Worse, Seeed's examples wrap their bodies in `#ifdef EPAPER_ENABLE`, so a
wrong combo compiles to an *empty program* and uploads without complaint. You
get a blank panel and no error to search for. Guard against it:

```c
#ifndef EPAPER_ENABLE
#error "EPAPER_ENABLE not defined — driver.h not picked up, or bad combo pair."
#endif
```

**The panel does 4-level greyscale**, despite the datasheet describing it as
B/W and storing a B/W waveform in OTP. Seeed GFX overrides that with its own
LUTs and gets four genuinely distinct levels via `initGrayMode(GRAY_LEVEL4)`.
It costs 5.07s a refresh against 3.43s for 1-bit, which is a bargain: at two
levels every pixel is wrong by up to 127 and the error diffusion has to smear
that over a wide area — that smearing *is* the visible stipple. At four levels
the worst case is ~42, diffusion stays local, and the texture mostly vanishes.

Do not assume the four levels are evenly spaced. They are not — the waveform
spaces them evenly in perceived lightness, so measured luminances are nearer
`0/60/138/255` than `0/85/170/255`. Error diffusion subtracts the value it
*believes* it wrote, so a wrong table becomes a systematic tonal shift that no
tone curve can undo. `bench/GrayCalib` measures them without a light meter.

## Building

Arduino IDE with the ESP32 core. Board **XIAO_ESP32S3**, and these matter:

| Setting | Value | If wrong |
|---|---|---|
| PSRAM | OPI PSRAM | camera init fails |
| USB CDC On Boot | Enabled | `Serial` output goes nowhere |
| Partition Scheme | 8M with spiffs | build overflows |

This board has no USB-to-UART bridge, so with CDC off `Serial` maps to pins
with nothing attached — and the port still enumerates and still flashes fine.
**A working upload is not evidence that `Serial` works.** To check what
actually compiled rather than what the menu claims:

```bash
cat ~/Library/Caches/arduino/sketches/*/build.options.json
```

You also need [Seeed GFX](https://github.com/Seeed-Studio/Seeed_GFX),
installed from a ZIP. It conflicts with TFT_eSPI; only one may be present.

## Layout

```
PaperCam/    the camera
bench/       FocusAssist  GrayCalib  GrayTest  ImageTest  FrameDump
dither/      dither + tone modules, native harness, tone playground
tools/       recv_frame.py, recv_upload.py, make_testchart.py
```

Arduino only compiles sources inside a sketch folder, so `PaperCam/src/`
carries copies of the modules. `dither/` is canonical — `make sync` pushes
them out, `make verify` fails if a copy has drifted.

The bench tools each answer one question. `FocusAssist` prints a sharpness
score several times a second so a fixed-focus lens can be set by watching a
number peak; it is how we discovered the lens was badly out of focus, a 40×
swing on the metric. `ImageTest` pushes a known-good image with no camera in
the loop, which separates "how good can this panel look" from "how good can
this camera get".

## Runtime controls

Serial at 115200, one character at a time:

| | |
|---|---|
| `x` shoot · `?` show all settings | `t`/`T` self-timer ±1s |
| `n`/`N` burst frames | `g`/`G` gamma · `c`/`C` contrast |
| `s`/`S` software sharpen · `0` off | `[`/`]` sensor sharpness |
| `;`/`'` sensor denoise | `-`/`=` exposure bias |
| `j` show the stored JPEG · `u` upload now | `z` sleep now |

Tone has no correct answer, only a preferred one, and a 40-second reflash per
guess is the wrong feedback loop for a judgement call.

## Uploading

Hold the shutter and the camera joins WiFi, POSTs `/last.jpg`, and switches the
radio off again.

Credentials go in `PaperCam/secrets.h` — copy `secrets.h.example` and fill it
in. It is gitignored. The sketch builds fine without it; upload is simply
disabled and a hold says so.

The intended target is an n8n webhook, which then writes to Google Drive or
wherever else. That split is deliberate: n8n holds the OAuth credential so the
microcontroller never has to. Token refresh, expiry, clock skew and certificate
storage on an ESP32 are all miserable, and changing destination later becomes
one line in `secrets.h` rather than a firmware change.

`tools/recv_upload.py` is a stdlib HTTP endpoint for testing before any of that
exists, so test photos stay on your own network. It verifies the JPEG's
FFD8/FFD9 markers, because a truncated upload otherwise looks like success at
both ends.

## Status

Working: everything the camera is for. Panel, focus, 4-level greyscale with
measured levels, tone, burst averaging, self-timer, tap-and-hold, JPEG to
flash, WiFi upload, deep sleep with the image preserved across wake.

Not done: an enclosure, and battery operation.

The panel is already ruled out as the power problem — 100 mAs a refresh and
7 µA asleep, so a 1000mAh cell covers roughly 36,000 refreshes. The open
question is the Sense board's sleep current, and it is not a small one:
`PWDN_GPIO_NUM` is -1 on this hardware, so the camera's power-down pin is not
wired to the ESP32 and software cannot cut its power. `esp_camera_deinit()`
stops the clock and that is all it can do. At a reported ~3mA a 1000mAh cell
lasts about a fortnight, which makes idle draw rather than photography the
entire battery design. Gating that supply with a MOSFET is the known fix.

Sleep current is unmeasured — nothing here resolves microamps — so the battery
will answer it: a fortnight means ~3mA, a couple of months means we got away
with it.

Also unresolved: capture runs at ~355ms a frame and it is not clear why, and
whether capturing above SVGA and downsampling in software beats the sensor's
internal scaler. An earlier test said no, but it reasoned from the wrong
native resolution and does not stand.
