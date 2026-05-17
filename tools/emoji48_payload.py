#!/usr/bin/env python3
import argparse
import base64
import struct
import sys
import urllib.request
import zlib
from pathlib import Path

BAYER8 = (
    (0, 128, 32, 160, 8, 136, 40, 168),
    (192, 64, 224, 96, 200, 72, 232, 104),
    (48, 176, 16, 144, 56, 184, 24, 152),
    (240, 112, 208, 80, 248, 120, 216, 88),
    (12, 140, 44, 172, 4, 132, 36, 164),
    (204, 76, 236, 108, 196, 68, 228, 100),
    (60, 188, 28, 156, 52, 180, 20, 148),
    (252, 124, 220, 92, 244, 116, 212, 84),
)


def _paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png_rgba(path):
    data = Path(path).read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("not a PNG file")

    width = height = bit_depth = color_type = None
    palette = []
    transparency = []
    idat = bytearray()
    pos = 8
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        pos += length + 12

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if compression != 0 or filter_method != 0 or interlace != 0:
                raise ValueError("unsupported PNG encoding")
            if bit_depth != 8:
                raise ValueError("only 8-bit PNGs are supported")
        elif chunk_type == b"PLTE":
            palette = [tuple(payload[i:i + 3]) for i in range(0, len(payload), 3)]
        elif chunk_type == b"tRNS":
            transparency = list(payload)
        elif chunk_type == b"IDAT":
            idat.extend(payload)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise ValueError("missing PNG header")

    channels_by_type = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
    if color_type not in channels_by_type:
        raise ValueError(f"unsupported PNG color type {color_type}")
    channels = channels_by_type[color_type]
    stride = width * channels
    raw = zlib.decompress(bytes(idat))

    rows = []
    src = 0
    prev = bytearray(stride)
    for _ in range(height):
        filter_type = raw[src]
        src += 1
        row = bytearray(raw[src:src + stride])
        src += stride
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = prev[i]
            upper_left = prev[i - channels] if i >= channels else 0
            if filter_type == 1:
                row[i] = (row[i] + left) & 0xFF
            elif filter_type == 2:
                row[i] = (row[i] + up) & 0xFF
            elif filter_type == 3:
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                row[i] = (row[i] + _paeth(left, up, upper_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}")
        rows.append(bytes(row))
        prev = row

    rgba = bytearray(width * height * 4)
    for y, row in enumerate(rows):
        for x in range(width):
            dst = (y * width + x) * 4
            src_px = x * channels
            if color_type == 0:
                v = row[src_px]
                rgba[dst:dst + 4] = bytes((v, v, v, 255))
            elif color_type == 2:
                rgba[dst:dst + 4] = row[src_px:src_px + 3] + b"\xff"
            elif color_type == 3:
                idx = row[src_px]
                r, g, b = palette[idx]
                a = transparency[idx] if idx < len(transparency) else 255
                rgba[dst:dst + 4] = bytes((r, g, b, a))
            elif color_type == 4:
                v, a = row[src_px:src_px + 2]
                rgba[dst:dst + 4] = bytes((v, v, v, a))
            elif color_type == 6:
                rgba[dst:dst + 4] = row[src_px:src_px + 4]

    return width, height, bytes(rgba)


def rasterize_emoji(path, size=48):
    src_w, src_h, rgba = read_png_rgba(path)
    width_bytes = (size + 7) // 8
    packed = bytearray(width_bytes * size)

    for y in range(size):
        sy = min(src_h - 1, (y * src_h) // size)
        for x in range(size):
            sx = min(src_w - 1, (x * src_w) // size)
            idx = (sy * src_w + sx) * 4
            r, g, b, a = rgba[idx:idx + 4]
            r = (r * a + 255 * (255 - a) + 127) // 255
            g = (g * a + 255 * (255 - a) + 127) // 255
            b = (b * a + 255 * (255 - a) + 127) // 255
            grey = (r * 299 + g * 587 + b * 114) // 1000
            if grey <= BAYER8[y & 7][x & 7]:
                packed[y * width_bytes + (x // 8)] |= 0x80 >> (x & 7)

    return bytes(packed)


def build_print_segments_body(path, size=48):
    data = rasterize_emoji(path, size)
    b64 = base64.b64encode(data).decode("ascii")
    return f"IMAGE {size} {size} {b64}\n"


def main():
    parser = argparse.ArgumentParser(description="Build or send a 48x48 emoji /printSegments payload.")
    parser.add_argument("png", nargs="?", default="1F601.png")
    parser.add_argument("--host", help="ESP32 base URL, for example http://192.168.1.50")
    parser.add_argument("--size", type=int, default=48)
    args = parser.parse_args()

    body = build_print_segments_body(args.png, args.size)
    if args.host:
        url = args.host.rstrip("/") + "/printSegments?feed=1"
        req = urllib.request.Request(
            url,
            data=body.encode("ascii"),
            method="POST",
            headers={"Content-Type": "text/plain"},
        )
        print(urllib.request.urlopen(req, timeout=15).read().decode())
    else:
        sys.stdout.write(body)


if __name__ == "__main__":
    main()
