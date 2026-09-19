#!/usr/bin/env python3
"""Turn raw AI sprite output into clean PNGs ready for LVGL.

  tools/sprites/raw/<stage>_<state>_<frame>.png   (any size, flat background)
    -> assets/sprites/<same>.png                  (background keyed out, cropped,
                                                   padded to --size, quantized)
    -> assets/sprites/manifest.json

The background colour is taken from the image corners unless --key is given.
Run through tools/sprites/to_lvgl.py afterwards to get C arrays.

  python3 tools/sprites/prepare_sprites.py --raw tools/sprites/raw --out assets/sprites
"""
import argparse
import json
import sys
from collections import Counter
from pathlib import Path

from PIL import Image


def corner_key(img):
    w, h = img.size
    pts = [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]
    return Counter(img.getpixel(p)[:3] for p in pts).most_common(1)[0][0]


def key_out(img, key, tol):
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, _a = px[x, y]
            if abs(r - key[0]) <= tol and abs(g - key[1]) <= tol and abs(b - key[2]) <= tol:
                px[x, y] = (0, 0, 0, 0)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", default="tools/sprites/raw")
    ap.add_argument("--out", default="assets/sprites")
    ap.add_argument("--size", type=int, default=160, help="output canvas is size x size")
    ap.add_argument("--colors", type=int, default=32, help="0 = keep original colours")
    ap.add_argument("--key", default=None, help="hex background to key out, e.g. 00ff00")
    ap.add_argument("--tol", type=int, default=24)
    args = ap.parse_args()

    raw = Path(args.raw)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    files = sorted(raw.glob("*.png"))
    if not files:
        print(f"no PNGs in {raw}", file=sys.stderr)
        return 1

    manifest = {}
    for f in files:
        img = Image.open(f).convert("RGBA")
        key = (tuple(int(args.key[i:i + 2], 16) for i in (0, 2, 4))
               if args.key else corner_key(img))
        img = key_out(img, key, args.tol)

        bbox = img.getbbox()
        if bbox:
            img = img.crop(bbox)

        scale = args.size / max(img.size)
        img = img.resize((max(1, round(img.width * scale)),
                          max(1, round(img.height * scale))), Image.NEAREST)

        canvas = Image.new("RGBA", (args.size, args.size), (0, 0, 0, 0))
        canvas.paste(img, ((args.size - img.width) // 2, args.size - img.height), img)

        if args.colors:
            canvas = canvas.quantize(colors=args.colors, method=Image.FASTOCTREE)

        dest = out / f.name
        canvas.save(dest)
        manifest[f.stem] = {"file": dest.name, "w": args.size, "h": args.size}
        print(f"prepared {f.name} -> {dest} (key #{key[0]:02x}{key[1]:02x}{key[2]:02x})")

    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"{len(files)} sprite(s) -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
