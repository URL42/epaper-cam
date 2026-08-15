# PaperCam — Hardware Reference

E-paper camera. Press a button, it takes a photo, dithers it to 1-bit, and
paints it onto a 7.5" e-paper panel where it stays with zero power draw.

This file is the authoritative hardware reference. Facts here were verified
against Seeed's wiki. Anything marked **VERIFY** has not been confirmed on
the bench yet — confirm before building on it.

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

---

## Libraries

- **Seeed GFX** (`Seeed-Studio/Seeed_Arduino_LCD`) — drives the panel.
  Conflicts with TFT_eSPI; only one may be installed.
- **esp32-camera** — bundled with the ESP32 Arduino core.

### driver.h

Generate with the config tool linked from the Driver Board wiki. Select the
7.5" mono panel and the **Driver Board**. Do not guess the combo number — a
wrong value produces a blank screen with no error.

```cpp
#define BOARD_SCREEN_COMBO <from generator>
#define USE_XIAO_EPAPER_DRIVER_BOARD
```

**VERIFY:** Seeed's Driver Board wiki page shows
`USE_XIAO_EPAPER_BREAKOUT_BOARD` in its example, which appears to be a
copy-paste error from the Breakout page. If the generator emits `BREAKOUT`
and the panel misbehaves, try `DRIVER` and check the library headers for the
exact macro name.

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
- Full refresh takes roughly 5 seconds. Do not attempt partial refresh.

### Packed buffer format

800x480 at 1bpp = 48,000 bytes, 100 bytes per row. MSB-first within each byte
(bit 7 = leftmost pixel).

**VERIFY:** bit polarity. Seeed's image-conversion docs list "Reverse color"
as checked for this panel, which suggests 0 = black. Confirm empirically in
Phase 1 with a half-black test pattern rather than trusting this.

---

## Known risks

**Brownout during refresh.** The panel's charge pump spikes hard. If the ESP
resets mid-refresh, add 100uF electrolytic across 3V3/GND near the FPC
connector.

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
