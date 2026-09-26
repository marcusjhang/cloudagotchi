#!/usr/bin/env python3
"""Draw the UI icon set as clean, rounded PNGs (white silhouettes for the candy
tiles, coloured hearts for the need rows). Run alongside prepare/to_lvgl:

  python3 tools/sprites/make_icons.py            # -> assets/icons/*.png
  python3 tools/sprites/to_lvgl.py --in assets/icons --out firmware/main/sprites

Icons are supersampled 4x then downscaled, so the edges are smooth (RGB565A8
keeps the alpha). No emoji, no line-glyph font: one consistent chunky set.
"""
import math
from pathlib import Path

from PIL import Image, ImageDraw

S = 48          # final icon size
SS = 4          # supersample
N = S * SS
OUT = Path("assets/icons")

WHITE = (255, 255, 255, 255)
FOOD_H = (255, 180, 76, 255)
FUN_H = (255, 111, 165, 255)
EMPTY_H = (50, 56, 72, 255)


def canvas():
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def save(img, name):
    OUT.mkdir(parents=True, exist_ok=True)
    img.resize((S, S), Image.LANCZOS).save(OUT / f"{name}.png")
    print("icon", name)


def icon_feed():
    img, d = canvas()
    d.pieslice([N * 0.14, N * 0.30, N * 0.86, N * 0.90], 0, 180, fill=WHITE)  # bowl
    d.rounded_rectangle([N * 0.10, N * 0.34, N * 0.90, N * 0.46], radius=N * 0.06, fill=WHITE)  # rim
    for cx in (0.36, 0.50, 0.64):  # steam
        d.ellipse([N * cx - N * 0.035, N * 0.10, N * cx + N * 0.035, N * 0.28], fill=WHITE)
    save(img, "ic_feed")


def icon_play():
    img, d = canvas()
    pts = []
    for i in range(10):
        r = 0.46 if i % 2 == 0 else 0.20
        a = -math.pi / 2 + i * math.pi / 5
        pts.append((N / 2 + r * N * math.cos(a), N / 2 + r * N * math.sin(a)))
    d.polygon(pts, fill=WHITE)
    for i in range(0, 10, 2):  # round the outer points
        x, y = pts[i]
        d.ellipse([x - N * 0.05, y - N * 0.05, x + N * 0.05, y + N * 0.05], fill=WHITE)
    save(img, "ic_play")


def icon_clean():
    img, d = canvas()
    d.ellipse([N * 0.12, N * 0.12, N * 0.82, N * 0.82], outline=WHITE, width=int(N * 0.13))
    d.ellipse([N * 0.30, N * 0.24, N * 0.41, N * 0.35], fill=WHITE)   # highlight
    d.ellipse([N * 0.72, N * 0.66, N * 0.90, N * 0.84], fill=WHITE)   # little bubble
    save(img, "ic_clean")


def icon_med():
    img, d = canvas()
    arm = N * 0.30
    c = N * 0.5
    d.rounded_rectangle([c - arm / 2, N * 0.14, c + arm / 2, N * 0.86], radius=arm / 2, fill=WHITE)
    d.rounded_rectangle([N * 0.14, c - arm / 2, N * 0.86, c + arm / 2], radius=arm / 2, fill=WHITE)
    save(img, "ic_med")


def heart(color, name, size=28):
    n = size * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = n * 0.26
    d.ellipse([n * 0.12, n * 0.16, n * 0.12 + 2 * r, n * 0.16 + 2 * r], fill=color)
    d.ellipse([n * 0.88 - 2 * r, n * 0.16, n * 0.88, n * 0.16 + 2 * r], fill=color)
    d.polygon([(n * 0.12, n * 0.16 + r), (n * 0.88, n * 0.16 + r), (n * 0.5, n * 0.88)], fill=color)
    OUT.mkdir(parents=True, exist_ok=True)
    img.resize((size, size), Image.LANCZOS).save(OUT / f"{name}.png")
    print("icon", name)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    icon_feed()
    icon_play()
    icon_clean()
    icon_med()
    heart(FOOD_H, "ic_heart_food")
    heart(FUN_H, "ic_heart_fun")
    heart(EMPTY_H, "ic_heart_empty")


if __name__ == "__main__":
    main()
