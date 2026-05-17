import base64
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from emoji48_payload import build_print_segments_body, rasterize_emoji, read_png_rgba


def pack_band(raster, width, height, y_offset):
    width_bytes = (width + 7) // 8
    out = bytearray(width * 3)
    for x in range(width):
        for band_y in range(24):
            y = y_offset + band_y
            if y >= height:
                break
            src_index = y * width_bytes + (x // 8)
            if raster[src_index] & (0x80 >> (x & 7)):
                out[x * 3 + band_y // 8] |= 0x80 >> (band_y & 7)
    return bytes(out)


def test_fixture_builds_48x48_print_segments_payload():
    source_width, source_height, _ = read_png_rgba(ROOT / "1F601.png")
    assert (source_width, source_height) == (72, 72)

    raster = rasterize_emoji(ROOT / "1F601.png", 48)
    assert len(raster) == 48 * 6
    assert any(raster)

    body = build_print_segments_body(ROOT / "1F601.png", 48)
    lines = body.splitlines()
    assert len(lines) == 1
    assert lines[0].startswith("IMAGE 48 48 ")

    decoded = base64.b64decode(lines[0].split(" ", 3)[3])
    assert decoded == raster
    assert len(pack_band(decoded, 48, 48, 0)) == 48 * 3
    assert len(pack_band(decoded, 48, 48, 24)) == 48 * 3
    assert pack_band(decoded, 48, 48, 0) != pack_band(decoded, 48, 48, 24)


if __name__ == "__main__":
    test_fixture_builds_48x48_print_segments_payload()
