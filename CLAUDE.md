# PaperCam — Hardware Reference

E-paper camera. Press a button, it takes a photo, dithers it to 1-bit, and
paints it onto a 7.5" e-paper panel where it stays with zero power draw.

This file is the authoritative hardware reference. Facts here were verified
against Seeed's wiki. Anything marked **VERIFY** has not been confirmed on
the bench yet — confirm before building on it. Items marked **CONFIRMED**
were measured on the bench, with the phase that measured them.

---

## Bill of materials (all on hand)

| Part | Notes |
|---|---|
| Seeed XIAO ESP32S3 **Sense** | OV2640 camera, 8MB PSRAM, 8MB flash |
| Seeed **ePaper Driver Board** (SKU 114993558) | JST BAT connector, charging IC, power switch, 24-pin FPC |
| Seeed 7.5" Monochrome ePaper, 800x480 (SKU p-5788) | 1-bit only, no grayscale |
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

The OV2640 connects through the board-to-board connector on the underside of
the XIAO itself, not the castellated pads. It uses internal GPIOs (10, 13-18,
38-48) and does **not** collide with any panel pin. No wiring required.

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

- Panel is **1-bit**. No grayscale, no partial refresh planned. Dithering is
  not an aesthetic choice here, it is the only way to render tone.
- OV2640 supports `PIXFORMAT_GRAYSCALE` natively — no JPEG decode, no
  RGB-to-luma step. The sensor hands over exactly what the dither needs.
- **Aspect ratio mismatch.** SVGA capture is 800x600 (4:3), panel is 800x480
  (5:3). Center-crop 120 rows: skip the first 60 and last 60. Every photo is
  landscape.
- Memory: 800x600 grayscale = 480KB, packed 1-bit output = 48KB. Both trivial
  in 8MB PSRAM. Multiple frames can be held for sharpness selection.
- Full refresh measured at **3433 ms** — **CONFIRMED (Phase 1)**, identical
  across runs, so the waveform is deterministic. Use this figure for the
  Phase 7 power budget, not the ~5s estimate it replaces. Do not attempt
  partial refresh.

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
