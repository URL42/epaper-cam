#!/usr/bin/env python3
"""Generate an 800x600 greyscale test chart as binary PGM.

Sized for the camera pipeline, so rows 0-59 and 540-599 are throwaway margin
that the 120-row centre crop removes. What survives is the 800x480 the panel
shows, and it goes through exactly the same tone + dither path a photo does.

Sections, top to bottom of the kept area:
  - smooth horizontal ramp     : is tonal gradation clean, or does it band?
  - 16-step wedge              : how many levels are actually distinguishable?
  - line pairs at 1/2/3/4 px   : the spatial resolution limit
  - checkerboards              : where fine texture collapses
  - vertical ramp              : gradation on the other axis
"""
import pathlib

W, H = 800, 600
CROP_TOP = 60
img = bytearray(b"\x80" * (W * H))

def put(x, y, v):
    if 0 <= x < W and 0 <= y < H:
        img[y * W + x] = v

def band(y0, y1, fn):
    """y0/y1 are in *output* coordinates (0..479); shifted into the kept area."""
    for y in range(y0 + CROP_TOP, y1 + CROP_TOP):
        for x in range(W):
            put(x, y, fn(x, y - CROP_TOP - y0))

# 0-95: smooth horizontal ramp, black to white
band(0, 96, lambda x, y: int(x * 255 / (W - 1)))

# 96-191: 16-step wedge
band(96, 192, lambda x, y: (min(15, x * 16 // W)) * 17)

# 192-287: line pairs, 1/2/3/4 px, on mid grey
def lines(x, y):
    section = x * 4 // W          # four zones across the width
    period = (section + 1) * 2    # 2,4,6,8 -> 1,2,3,4 px lines
    return 0 if (x // (period // 2)) % 2 == 0 else 255
band(192, 288, lines)

# 288-383: checkerboards at 1,2,4,8 px
def checks(x, y):
    section = x * 4 // W
    size = 1 << section
    return 0 if ((x // size) + (y // size)) % 2 == 0 else 255
band(288, 384, checks)

# 384-479: vertical ramp
band(384, 480, lambda x, y: int(y * 255 / 95))

out = pathlib.Path("frames/testchart.pgm")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(b"P5\n%d %d\n255\n" % (W, H) + bytes(img))
print(f"wrote {out} ({out.stat().st_size} bytes)")
