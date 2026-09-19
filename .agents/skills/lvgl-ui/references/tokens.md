# Tokens

One header, `ui_theme.h`, names every value. Widget code reads the macros; it
never contains a raw `lv_color_hex()`.

## Build the surface scale *up from the background*

On an AMOLED panel, black = pixels off, which is best for power and burn-in. So
the surfaces are dark and get **lighter as they come forward**:

```
UI_BG               0x000000   screen (pixels off)
UI_SURFACE          0x121A22   buttons, bars track
UI_SURFACE_PRESSED  0x1D2836   pressed feedback
UI_SURFACE_OFF      0x0C1218   disabled (barely above black)
UI_PANEL            0x0B1420   overlays
UI_BORDER           0x2A3644
UI_TEXT             0xE8EEF4
UI_TEXT_DIM         0x7C8A99   secondary only
UI_ACCENT           0x2EC4B6   active / checked
UI_SUCCESS/WARN/DANGER                 semantic only
```

Everything is `lv_color_hex(0x......)` behind a macro, e.g.
`#define UI_SURFACE lv_color_hex(0x121A22)`.

## Light panels (TFT / LCD)

Invert the logic: a true-white or very light neutral background, surfaces a step
*darker*, ink near-black, the same accent/semantic roles. Do not tint the whole
UI warm "because it feels cosy" — pick the neutral deliberately.

## Geometry

```
UI_RADIUS     12    UI_RADIUS_SM  8
UI_PAD        12    UI_GAP        8
UI_BTN_W      54    UI_BTN_H      56
```

Keep values on an 8 px grid. Radii: rounded rect, not pill, not square.

## Rules

- 2 px strokes; no heavy borders.
- Shadows off on tiny screens (they cost fill and read as mush).
- Colour = meaning. If a colour does not encode state or category, remove it.
- Change the theme by editing `ui_theme.h` only. If you reach for a hex in a
  screen file, stop and add a token.
