# PaperCam — Claude Code Kickoff

Paste everything below into Claude Code in an empty repo, with `CLAUDE.md`
already present.

---

## Prompt

I'm building PaperCam: an e-paper camera. Press a button, it captures a
photo, dithers it to 1-bit, paints it onto a 7.5" e-paper panel, and sleeps.
The image persists on the panel with no power.

Read `CLAUDE.md` first — it has the verified pin map, board settings, and
known traps. Treat anything marked **VERIFY** as unconfirmed.

Hardware is on hand and assembled: XIAO ESP32S3 Sense seated in a Seeed
ePaper Driver Board, 7.5" mono 800x480 panel in the FPC connector.

**Environment:** Arduino/C++ (ESP32 Arduino core). I normally write
MicroPython, so explain C-specific choices — memory ownership, pointer
handling, why a buffer is allocated where it is. I want to understand this
code, not just flash it.

**How I want to work:**
- One phase at a time. Stop at the end of each and wait for bench results.
- Every phase ends in something flashable that proves one thing.
- Don't scaffold ahead. No stubs for later phases.
- When a design decision has alternatives, state the tradeoff before choosing.
- I'll paste back serial output and describe what the panel shows.

### Build order

**Phase 1 — Panel bring-up.** Nothing else. Corner markers in opposite
corners, 1px vertical lines, a text string, full refresh. Proves the combo
number, wiring, orientation, and refresh timing. Also settle the VERIFY on
bit polarity here with a half-black test pattern.

**Phase 2 — Button.** D5 with INPUT_PULLUP, debounced, plus a 1.5s hold
detect. Serial print on trigger. No capture yet.

**Phase 3 — Camera.** Init OV2640 at SVGA grayscale. Capture one frame,
report dimensions and a histogram over serial. Panel untouched.

**Phase 4 — Dither.** Floyd-Steinberg, serpentine scan, 800x600 grayscale in,
packed 1-bit 800x480 out with the 120-row center crop. Pure function, no
hardware. Testable against a synthetic gradient.

**Phase 5 — Wire it together.** Button, capture, dither, blit. This is the
first flashable camera.

**Phase 6 — Burst and sharpness.** Capture several frames, score each by
variance of Laplacian, keep the best. This is where PSRAM earns its keep.

**Phase 7 — Power.** LED countdown, deep sleep between shots, EXT0 wake on
D5. Measure sleep current before committing to a battery design.

**Phase 8 — Enclosure.** Mounting geometry for a Bambu A1 print, bezel sized
to drop into an off-the-shelf photo frame.

Start with Phase 1 only. Before writing code, tell me your plan for it and
flag anything in `CLAUDE.md` you think is wrong or underspecified.
