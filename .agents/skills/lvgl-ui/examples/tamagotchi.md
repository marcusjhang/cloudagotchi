# Worked example — an on-device Tamagotchi

A virtual pet on a Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED, I2C touch),
built with ESP-IDF v5.5 + LVGL 9. The conventions in this skill come from here.

## The screen: icon-only, no words (see screens.md)

```
┌────────────────────────────┐
│          ♥ ♥ ♥ ♡           │  Food hearts (warm)
│          ♥ ♥ ♡ ♡           │  Fun hearts (pink)
│              ( ! )         │  one pulsing call over the pet; tap = the fix
│            ( pet )         │  pet centred on a soft pedestal (the hero)
│  [feed][play][clean][med]  │  four 80px tiles, white icons on candy colours
└────────────────────────────┘
```

There is **no text at rest** — no clock, no stage, no captions, no toasts. A
pre-reader can use the whole screen. Settings (time, brightness, reset) hide
behind a long-press on the pet; a dead pet restarts on a tap.

## Icons are images, not glyphs

Every pictogram is generated (`tools/sprites/make_icons.py`) as a rounded PNG and
compiled to RGB565A8 with LVGL's `LVGLImage.py`. Icons are **white silhouettes**
so they sit on colour tiles, and supersampled 4x then downscaled for smooth
edges. This avoids the two failure modes of glyph icons: no emoji, and no
mismatched FontAwesome line-art that reads as a developer default.

Tile colours are candy accents on black (`UI_C_FEED` orange, `UI_C_PLAY` pink,
`UI_C_CLEAN` sky, `UI_C_MED` green). Hearts are a warm and a pink set plus a dim
"empty" heart.

## State without words

- `pet_need()` (pure, host-tested) returns the single most urgent need
  (MED > CLEAN > FOOD > FUN). One pulsing badge shows it; **tapping the badge
  performs the fix**; the matching tile gets a bright ring.
- `pet_action_enabled()` (pure) drives each tile's disabled state, so a refused
  action is simply not pressable — no "not hungry" toast.
- The face, the poop sprite and sleep carry the rest.
