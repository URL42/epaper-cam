#!/usr/bin/env python3
"""Receive one raw grayscale frame from PaperCam_FrameDump and save it as PGM.

    python3 tools/recv_frame.py frames/living_room.pgm

PGM is chosen because it is both directly viewable and, being a tiny ASCII
header followed by the raw bytes, trivial for the C harness in dither/ to load
without a decoder.

The Arduino Serial Monitor must be closed — it holds the port exclusively.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not installed:  pip3 install pyserial")

HEADER = b"PCFRAME"
FOOTER = b"PCEND"


def open_port(port: str, baud: int, timeout: float) -> "serial.Serial":
    # Configure DTR/RTS before opening. On the ESP32-S3's USB-Serial/JTAG,
    # pyserial's default of asserting them can reset the board, which would
    # drop us back into setup() just as we start listening.
    s = serial.Serial()
    s.port = port
    s.baudrate = baud
    s.timeout = timeout
    s.dtr = False
    s.rts = False
    try:
        s.open()
    except serial.SerialException as exc:
        sys.exit(f"cannot open {port}: {exc}\n"
                 "Is the Arduino Serial Monitor still open?")
    return s


def read_header(s: "serial.Serial", attempts: int) -> list[bytes]:
    """Send 'c' and wait for the PCFRAME line, retrying if the board is busy."""
    for attempt in range(attempts):
        s.reset_input_buffer()
        s.write(b"c")
        s.flush()

        deadline = time.time() + 3.0
        while time.time() < deadline:
            line = s.readline()
            if not line:
                break
            if line.startswith(HEADER):
                return line.split()
            # Anything else is the sketch's own status output; show it so a
            # camera failure is visible rather than looking like a timeout.
            text = line.decode("utf-8", "replace").strip()
            if text:
                print(f"  [board] {text}")
        print(f"  no frame yet, retrying ({attempt + 1}/{attempts})...")

    sys.exit("no PCFRAME header — is PaperCam_FrameDump flashed and running?")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("output", help="destination .pgm path")
    ap.add_argument("--port", default="/dev/cu.usbmodem101")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--timeout", type=float, default=10.0)
    ap.add_argument("--attempts", type=int, default=3)
    args = ap.parse_args()

    out = pathlib.Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)

    with open_port(args.port, args.baud, args.timeout) as s:
        time.sleep(0.2)

        parts = read_header(s, args.attempts)
        try:
            width, height, length = (int(parts[1]), int(parts[2]), int(parts[3]))
            expect_sum = int(parts[4], 16)
        except (IndexError, ValueError):
            sys.exit(f"malformed header: {parts!r}")

        print(f"receiving {width}x{height}, {length} bytes...")
        t0 = time.time()
        data = s.read(length)
        elapsed = time.time() - t0

        if len(data) != length:
            sys.exit(f"short read: got {len(data)} of {length} bytes")

        tail = s.readline()
        if not tail.startswith(FOOTER):
            print(f"  warning: expected {FOOTER!r}, got {tail!r}")

        actual_sum = sum(data) & 0xFFFFFFFF
        if actual_sum != expect_sum:
            sys.exit(f"checksum mismatch: got {actual_sum:08x}, "
                     f"expected {expect_sum:08x}")

    with out.open("wb") as f:
        f.write(b"P5\n%d %d\n255\n" % (width, height))
        f.write(data)

    rate = length / elapsed / 1024 if elapsed else 0
    lo, hi = min(data), max(data)
    mean = sum(data) // len(data)
    print(f"wrote {out}  ({elapsed:.2f}s, {rate:.0f} KB/s)")
    print(f"luma  min {lo}  max {hi}  mean {mean}")


if __name__ == "__main__":
    main()
