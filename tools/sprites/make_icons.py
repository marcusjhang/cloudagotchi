#!/usr/bin/env python3
"""Draw the UI icon set as clear, rounded PNGs (white silhouettes for the candy
tiles, so the meaning is obvious without words). Run alongside the sprites:

  python3 tools/sprites/make_icons.py            # -> assets/icons/*.png
  python3 tools/sprites/to_lvgl.py --in assets/icons --out firmware/main/sprites

Icons are supersampled 4x then downscaled, so edges are smooth (RGB565A8 keeps
the alpha). Shapes are chosen to be unmistakable: burger = feed, ball = play,
water drop = clean, cross = medicine, smiley = happy.
"""
from pathlib import Path

from PIL import Image, ImageDraw

SS = 4
OUT = Path("assets/icons")
WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)


def _new(size):
    n = size * SS
    img = Image.new("RGBA", (n, n), CLEAR)
    return img, ImageDraw.Draw(img), n


def draw_feed(size):
    img, d, n = _new(size)
    d.pieslice([n * 0.14, n * 0.16, n * 0.86, n * 0.54], 180, 360, fill=WHITE)                  # top bun
    d.rounded_rectangle([n * 0.10, n * 0.55, n * 0.90, n * 0.66], radius=n * 0.06, fill=WHITE)  # patty
    d.rounded_rectangle([n * 0.16, n * 0.69, n * 0.84, n * 0.84], radius=n * 0.09, fill=WHITE)  # bottom bun
    return img


def draw_play(size):
    img, d, n = _new(size)
    d.ellipse([n * 0.08, n * 0.08, n * 0.92, n * 0.92], fill=WHITE)
    d.ellipse([n * 0.34, n * 0.08, n * 0.66, n * 0.92], outline=CLEAR, width=int(n * 0.05))     # seams
    d.ellipse([n * 0.08, n * 0.34, n * 0.92, n * 0.66], outline=CLEAR, width=int(n * 0.05))
    return img


def draw_clean(size):
    img, d, n = _new(size)
    d.ellipse([n * 0.26, n * 0.46, n * 0.74, n * 0.90], fill=WHITE)                             # drop
    d.polygon([(n * 0.30, n * 0.60), (n * 0.70, n * 0.60), (n * 0.50, n * 0.14)], fill=WHITE)
    d.ellipse([n * 0.44, n * 0.12, n * 0.56, n * 0.24], fill=WHITE)                             # round tip
    return img


def draw_med(size):
    img, d, n = _new(size)
    arm, c = n * 0.30, n * 0.5
    d.rounded_rectangle([c - arm / 2, n * 0.14, c + arm / 2, n * 0.86], radius=arm / 2, fill=WHITE)
    d.rounded_rectangle([n * 0.14, c - arm / 2, n * 0.86, c + arm / 2], radius=arm / 2, fill=WHITE)
    return img


def draw_happy(size):
    img, d, n = _new(size)
    d.ellipse([n * 0.08, n * 0.08, n * 0.92, n * 0.92], fill=WHITE)
    d.ellipse([n * 0.31, n * 0.33, n * 0.42, n * 0.44], fill=CLEAR)                             # eyes
    d.ellipse([n * 0.58, n * 0.33, n * 0.69, n * 0.44], fill=CLEAR)
    d.arc([n * 0.28, n * 0.34, n * 0.72, n * 0.76], 20, 160, fill=CLEAR, width=int(n * 0.07))   # smile
    return img


def draw_water(size):
    img, d, n = _new(size)
    d.ellipse([n * 0.26, n * 0.46, n * 0.74, n * 0.90], fill=WHITE)
    d.polygon([(n * 0.30, n * 0.60), (n * 0.70, n * 0.60), (n * 0.50, n * 0.14)], fill=WHITE)
    d.ellipse([n * 0.44, n * 0.12, n * 0.56, n * 0.24], fill=WHITE)
    return img


def draw_sparkle(size):
    img, d, n = _new(size)
    d.polygon([(n * 0.50, n * 0.10), (n * 0.60, n * 0.40), (n * 0.90, n * 0.50),
               (n * 0.60, n * 0.60), (n * 0.50, n * 0.90), (n * 0.40, n * 0.60),
               (n * 0.10, n * 0.50), (n * 0.40, n * 0.40)], fill=WHITE)
    d.ellipse([n * 0.72, n * 0.18, n * 0.86, n * 0.32], fill=WHITE)  # a little spark
    return img


def draw_sleep(size):
    img, d, n = _new(size)
    d.ellipse([n * 0.14, n * 0.14, n * 0.86, n * 0.86], fill=WHITE)          # moon
    d.ellipse([n * 0.34, n * 0.02, n * 1.06, n * 0.74], fill=CLEAR)          # bite out the crescent
    return img


def draw_rabbit(size, eyes="open", mouth="line"):
    img, d, n = _new(size)
    # ears + inner ear
    d.rounded_rectangle([n * 0.28, n * 0.05, n * 0.43, n * 0.46], radius=n * 0.075, fill=WHITE)
    d.rounded_rectangle([n * 0.57, n * 0.05, n * 0.72, n * 0.46], radius=n * 0.075, fill=WHITE)
    d.rounded_rectangle([n * 0.315, n * 0.11, n * 0.395, n * 0.40], radius=n * 0.04, fill=CLEAR)
    d.rounded_rectangle([n * 0.605, n * 0.11, n * 0.685, n * 0.40], radius=n * 0.04, fill=CLEAR)
    # head
    d.ellipse([n * 0.18, n * 0.34, n * 0.82, n * 0.94], fill=WHITE)
    # eyes
    r = n * 0.05
    if eyes == "closed":
        d.rounded_rectangle([n * 0.40 - r, n * 0.61, n * 0.40 + r, n * 0.635], radius=n * 0.015, fill=CLEAR)
        d.rounded_rectangle([n * 0.60 - r, n * 0.61, n * 0.60 + r, n * 0.635], radius=n * 0.015, fill=CLEAR)
    else:
        d.ellipse([n * 0.40 - r, n * 0.62 - r, n * 0.40 + r, n * 0.62 + r], fill=CLEAR)
        d.ellipse([n * 0.60 - r, n * 0.62 - r, n * 0.60 + r, n * 0.62 + r], fill=CLEAR)
    # nose + mouth
    d.polygon([(n * 0.47, n * 0.70), (n * 0.53, n * 0.70), (n * 0.50, n * 0.745)], fill=CLEAR)
    if mouth == "open":
        d.ellipse([n * 0.45, n * 0.755, n * 0.55, n * 0.83], fill=CLEAR)
    elif mouth == "line":
        d.rounded_rectangle([n * 0.47, n * 0.775, n * 0.53, n * 0.795], radius=n * 0.01, fill=CLEAR)
    return img


def draw_egg(size):
    img, d, n = _new(size)
    d.ellipse([n * 0.22, n * 0.14, n * 0.78, n * 0.90], fill=WHITE)
    for cx, cy in ((0.42, 0.40), (0.60, 0.52), (0.46, 0.66), (0.58, 0.32)):
        d.ellipse([n * cx - n * 0.03, n * cy - n * 0.03, n * cx + n * 0.03, n * cy + n * 0.03], fill=CLEAR)
    return img


def save(img, name, size):
    OUT.mkdir(parents=True, exist_ok=True)
    img.resize((size, size), Image.LANCZOS).save(OUT / f"{name}.png")
    print("icon", name)


def main():
    if OUT.exists():
        for p in OUT.glob("*.png"):
            p.unlink()
    # action / need icons (white silhouettes)
    save(draw_feed(48), "ic_feed", 48)       # food (burger)
    save(draw_water(48), "ic_water", 48)     # water (drop)
    save(draw_sleep(48), "ic_sleep", 48)     # sleep (moon)
    save(draw_sparkle(48), "ic_clean", 48)   # clean (sparkle)
    save(draw_med(48), "ic_med", 48)         # medicine (cross)
    save(draw_play(48), "ic_play", 48)       # fun (ball)
    # the pet itself (monochrome rabbit)
    save(draw_rabbit(220), "rabbit_idle", 220)
    save(draw_rabbit(220, mouth="open"), "rabbit_eat", 220)
    save(draw_rabbit(220, mouth="open"), "rabbit_drink", 220)
    save(draw_rabbit(220, eyes="closed"), "rabbit_sleep", 220)
    save(draw_egg(200), "rabbit_egg", 200)


if __name__ == "__main__":
    main()
