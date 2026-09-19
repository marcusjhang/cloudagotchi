# Icons

Three strategies, best first depending on the look.

## 1. Pixel-art images (best for games / cute / chibi UIs)

Compile small PNGs to `lv_image_dsc_t` arrays (RGB565A8 if you need alpha) with
LVGL's `LVGLImage.py --cf RGB565A8`, or your own converter. Then:

- Draw at an integer scale with nearest-neighbour:
  `lv_image_set_antialias(img, false)` and `lv_image_set_scale(img, 512)` (2x).
- Use `lv_imagebutton` when a button needs released/pressed/disabled art:
  three image sources per state, same size, aligned.
- Or a styled `lv_button` + `lv_image` child when you want the shared surface to
  show through and only the glyph to change.

This is the only strategy that will match a pixel-art pet; use it when the UI has
a game aesthetic. Keep icons 24-32 px source, single flat palette, one outline
treatment across the whole set.

## 2. An icon font subset (best for clean tools)

Generate a subset with `lv_font_conv` (FontAwesome etc.) containing only the
glyphs you use, at one or two sizes. Crisp and scalable; not pixel-art.
```bash
lv_font_conv --font FontAwesome5-Solid+Brands+Regular.woff -r 0xf015,0xf11b \
  --size 24 --format lvgl -o my_icons_24.c --bpp 4
```

## 3. `LV_SYMBOL_*` built-ins (interim only)

LVGL's Montserrat fonts ship a small FontAwesome subset
(`LV_SYMBOL_PLAY`, `TRASH`, `EYE_OPEN/CLOSE`, `PLUS`, `TINT`, `LIST`, `SETTINGS`,
`WARNING`, `BELL`, ...). Fine to wire the structure fast, but the semantics are
approximate (there is no food/gamepad/broom), so treat it as a placeholder and
swap to 1 or 2 once the art exists.

## Pitfalls

- **Emoji.** Never use emoji as icons; coverage and metrics vary per build.
- **Mixed sets.** Do not mix a pixel icon with a line glyph in one bar.
- **Baseline jitter.** Different glyph sets sit on different baselines; keep one
  set and one size.
- **Sizing.** Icons smaller than ~20 px read as noise. Keep a consistent optical
  size across a row.
- **State.** An icon-only disabled button must still look disabled (dim/opacity),
  not just non-responsive.
