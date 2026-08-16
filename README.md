# PaperCam

An e-paper camera. Press the shutter, wait three seconds, and a photograph
appears on a 7.5" e-paper panel and stays there with no power at all.

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
shutter → 3s self-timer (LED countdown)
        → burst of 6 frames, SVGA greyscale
        → score each by variance of Laplacian, drop the shaken ones
        → average the survivors            (noise ÷ √N)
        → tone curve                        (256-entry LUT)
        → Floyd-Steinberg, serpentine, to 4-level grey
        → straight into the panel's sprite buffer
        → refresh                           (~5.1s)
```

About 7.7 seconds shutter to image, of which 5.1 is the panel itself.

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
tools/       recv_frame.py, make_testchart.py
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

Tone has no correct answer, only a preferred one, and a 40-second reflash per
guess is the wrong feedback loop for a judgement call.

## Status

Working: panel, camera, focus, greyscale, level calibration, tone, burst
averaging, self-timer.

Not done: deep sleep and battery operation, and an enclosure. The panel is
already ruled out as the power problem — 100 mAs a refresh and 7 µA asleep, so
a 1000mAh cell covers roughly 36,000 refreshes. The open question is the Sense
board's sleep current with the camera attached.

Also unresolved: capture runs at ~355ms a frame and it is not clear why, and
whether capturing above SVGA and downsampling in software beats the sensor's
internal scaler. An earlier test said no, but it reasoned from the wrong
native resolution and does not stand.
