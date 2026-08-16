#!/usr/bin/env python3
"""Minimal HTTP endpoint that saves what PaperCam uploads.

    python3 tools/recv_upload.py            # listens on 0.0.0.0:8080
    python3 tools/recv_upload.py --port 9000 --dir ~/Pictures/papercam

Point UPLOAD_URL in PaperCam/secrets.h at this while testing:

    #define UPLOAD_URL "http://<your-mac-ip>:8080/papercam"

Exists so the upload path can be proven before an n8n webhook is built, and so
test photos stay on your own network rather than being posted to some public
request-inspection service.

Stdlib only. Accepts any POST, writes the body to a timestamped .jpg, and
prints what it got.
"""

from __future__ import annotations

import argparse
import datetime
import pathlib
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SAVE_DIR = pathlib.Path(".")


class Handler(BaseHTTPRequestHandler):
    def do_POST(self) -> None:  # noqa: N802  (name fixed by BaseHTTPRequestHandler)
        length = int(self.headers.get("Content-Length", 0))
        if length <= 0:
            self.send_error(411, "need a Content-Length")
            return

        body = self.rfile.read(length)

        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        path = SAVE_DIR / f"papercam-{stamp}.jpg"
        path.write_bytes(body)

        token = self.headers.get("X-PaperCam-Token", "")
        # JPEG starts FFD8 and ends FFD9. Cheap integrity check that catches a
        # truncated upload, which would otherwise look like a success.
        ok = body[:2] == b"\xff\xd8" and body[-2:] == b"\xff\xd9"

        print(f"{stamp}  {len(body):>7} bytes  -> {path.name}"
              f"  {'valid jpeg' if ok else 'NOT A COMPLETE JPEG'}"
              f"{'  token=' + token if token else ''}")

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"ok\n")

    def do_GET(self) -> None:  # noqa: N802
        # So you can check reachability from a browser before involving the
        # camera. Rules out firewall and IP mistakes in one step.
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"papercam receiver is up; POST a jpeg here\n")

    def log_message(self, *args) -> None:
        pass  # the do_POST print is the log we actually want


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--dir", default=".", help="where to save (default: cwd)")
    args = ap.parse_args()

    global SAVE_DIR
    SAVE_DIR = pathlib.Path(args.dir).expanduser()
    SAVE_DIR.mkdir(parents=True, exist_ok=True)

    server = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    print(f"saving to {SAVE_DIR.resolve()}")
    print(f"listening on 0.0.0.0:{args.port} — ctrl-c to stop")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
        sys.exit(0)


if __name__ == "__main__":
    main()
