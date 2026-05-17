# ESP32 TM-T88 Printer Bridge

## Inline segment printing

`POST /printSegments` prints native printer text, small inline monochrome images, and 48x48 emoji images without rasterizing an entire text line.

This endpoint intentionally uses a small `text/plain` protocol instead of JSON so the firmware does not need an added JSON dependency. Each command is one UTF-8 text line:

```text
TEXT <base64 utf8 text bytes>
IMAGE <widthDots> <heightDots> <base64 1-bit raster bytes>
LF
```

`TEXT` writes decoded bytes directly to the printer without adding a newline. `IMAGE` writes an inline bit image at the current print position when its height is 24 dots or less. Taller images up to 48 dots are printed as consecutive 24-dot bands for multi-line emoji output. `LF` writes exactly one line feed. No extra blank line is inserted between segments. An optional query parameter, `feed=0..10`, adds trailing feed lines after all commands; the default is `0`.

Inline images use the same input packing as `/printImage`: 1-bit monochrome, row-major, each row packed left-to-right, MSB first, black pixels as `1` bits. The decoded byte count must be `ceil(widthDots / 8) * heightDots`.

Image segment limits are `widthDots <= 576` and `heightDots <= 48`. Clients can use `48x48` for standalone emoji output. The firmware converts the row-major input into ESC/POS `ESC *` 24-dot bit-image data; images taller than 24 dots are split into multiple bands with 24-dot line spacing. The existing `/printImage` endpoint still uses `GS v 0`; that raster command is block-oriented and should not be used when text must continue after an image on the same baseline.

Generate a 48x48 test payload from `1F601.png`:

```bash
python tools/emoji48_payload.py 1F601.png
```

Send it to a printer bridge:

```bash
python tools/emoji48_payload.py 1F601.png --host http://printer.local
```

Example:

```bash
python - <<'PY'
import base64
import urllib.request

host = "http://printer.local"

def b64(data):
    return base64.b64encode(data).decode("ascii")

# 24x24 test icon: an X shape. Replace this with a 24x24 dithered OpenMoji
# raster generated from:
# https://raw.githubusercontent.com/hfg-gmuend/openmoji/refs/heads/master/black/72x72/1F601.png
rows = []
for y in range(24):
    row = 0
    for x in range(24):
        if x == y or x == 23 - y:
            row |= 1 << (23 - x)
    rows.extend([(row >> 16) & 0xff, (row >> 8) & 0xff, row & 0xff])

body = "\n".join([
    "TEXT " + b64("[ ] Buy milk  ".encode("utf-8")),
    "IMAGE 24 24 " + b64(bytes(rows)),
    "TEXT " + b64(" after emoji".encode("utf-8")),
    "LF",
])

req = urllib.request.Request(
    host + "/printSegments",
    data=body.encode("ascii"),
    method="POST",
    headers={"Content-Type": "text/plain"},
)
print(urllib.request.urlopen(req).read().decode())
PY
```

Manual verification requests:

1. Text-only line: `TEXT SGVsbG8=\nLF`
2. 48x48 emoji: `python tools/emoji48_payload.py 1F601.png --host http://printer.local`
3. Invalid image length: `IMAGE 24 24 AA==\nLF` should return `{"ok":false,"error":"data size mismatch"}`.
4. Multiple lines: `TEXT TGluZSAx\nLF\nTEXT TGluZSAy\nLF`
