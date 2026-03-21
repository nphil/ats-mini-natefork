#!/usr/bin/env python3
"""
screen-grab.py  —  Capture the ATS-Mini display over USB serial.

Sends the 'C' command to the device, receives the hex-encoded RGB565 BMP,
decodes it, and saves a PNG that can be opened or shared with Claude.

Usage:
    python3 tools/screen-grab.py [port] [output.png]

Defaults:
    port       auto-detected (first /dev/cu.usbmodem* or /dev/ttyACM*)
    output     /tmp/ats-screen.png

Requirements:
    pip install pyserial Pillow

Note: USB Mode must be enabled on the ATS-Mini (Settings → USB Mode → On).
"""

import sys
import struct
import time
import glob
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("Missing dependency: pip install pyserial")

try:
    from PIL import Image
except ImportError:
    sys.exit("Missing dependency: pip install Pillow")


# ── constants ────────────────────────────────────────────────────────────────

W, H         = 320, 170          # ATS-Mini display size
HEADER_BYTES = 14 + 40 + 12      # BMP file header + BITMAPINFOHEADER + masks
PIXEL_BYTES  = W * H * 2         # RGB565, 2 bytes/pixel
TOTAL_HEX    = (HEADER_BYTES + PIXEL_BYTES) * 2   # expected hex chars


# ── port detection ───────────────────────────────────────────────────────────

def find_port():
    patterns = ["/dev/cu.usbmodem*", "/dev/ttyACM*", "/dev/ttyUSB*"]
    for pat in patterns:
        ports = glob.glob(pat)
        if ports:
            return sorted(ports)[0]
    return None


# ── main ─────────────────────────────────────────────────────────────────────

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    out  = Path(sys.argv[2] if len(sys.argv) > 2 else "/tmp/ats-screen.png")

    if not port:
        sys.exit("No ATS-Mini USB port found. Connect the device and enable USB Mode.")

    print(f"Port   : {port}")
    print(f"Output : {out}")

    with serial.Serial(port, 115200, timeout=15) as ser:
        ser.reset_input_buffer()
        time.sleep(0.05)

        print("Sending 'C' command...")
        ser.write(b'C')

        hex_buf = ""
        last_pct = -1

        while len(hex_buf) < TOTAL_HEX:
            raw = ser.readline()
            if not raw:
                sys.exit("Timeout — no data received. Is USB Mode enabled on the device?")

            line = raw.decode('ascii', errors='ignore').strip()
            if not line:
                continue

            # Accept only valid hex characters (skip echo, prompts, etc.)
            if not all(c in '0123456789abcdefABCDEF' for c in line):
                continue

            hex_buf += line
            pct = min(99, len(hex_buf) * 100 // TOTAL_HEX)
            if pct != last_pct:
                print(f"  Receiving... {pct}%", end='\r')
                last_pct = pct

        print("  Receiving... 100%")

    # ── decode BMP ────────────────────────────────────────────────────────────
    bmp_bytes   = bytes.fromhex(hex_buf[:TOTAL_HEX])
    pixel_bytes = bmp_bytes[HEADER_BYTES:]

    # Each pixel is a uint16 LE (htons was used when encoding, so the hex
    # bytes are in the correct order to decode as little-endian uint16).
    pixels_rgb = []
    for i in range(0, len(pixel_bytes), 2):
        px = struct.unpack_from('<H', pixel_bytes, i)[0]
        r  = ((px >> 11) & 0x1F) * 255 // 31
        g  = ((px >>  5) & 0x3F) * 255 // 63
        b  = ( px        & 0x1F) * 255 // 31
        pixels_rgb.append(bytes([r, g, b]))

    # BMP pixel rows are stored bottom-up; reverse to get top-down
    rows = [pixels_rgb[y * W : (y + 1) * W] for y in range(H)]
    rows.reverse()

    rgb_bytes = b''.join(px for row in rows for px in row)
    img = Image.frombytes('RGB', (W, H), rgb_bytes)

    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(str(out))
    print(f"\nSaved  : {out}")
    return str(out)


if __name__ == '__main__':
    main()
