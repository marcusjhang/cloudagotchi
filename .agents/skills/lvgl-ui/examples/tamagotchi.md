# Worked example — an on-device Tamagotchi

A virtual pet on a Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED, I2C touch),
built with ESP-IDF v5.5 + LVGL 9. The conventions in this skill come from here.

## Final screen: monochrome, face-first

The owner's reference was a small device showing a **big friendly face on black
with no chrome** (and a single tiny word). That is the whole design: cuteness
comes from the face and from restraint, not colour. White ink on true black is
also the best choice for an AMOLED (pixels off, no burn-in).

```
┌────────────────────────────┐
│                            │
│          ( pet )           │  the hero, centred
│         (call ring)        │  a pulsing white ring when it needs something
│           hungry           │  one tiny lowercase word, dim
│                            │
│  (o) (o) (o) (o)           │  Feed / Play / Clean / Med - dark circles,
└────────────────────────────┘  white icons, no captions
```

Design rules that got us here:

- **One hero, one word, one call.** The pet fills the screen; a single pulsing
  **call ring** (from the pure `pet_need()`) shows what it needs and is itself
  the button; one lowercase word (`hungry / bored / sleepy / sick / dirty / ok`)
  is the only text.
- **Controls recede.** Four dark circles with white icons, only 72 px, no tiles
  and no captions, so at rest the screen is mostly the face. `pet_action_enabled()`
  dims what the pet would refuse.
- **Monochrome beats candy here.** Two earlier colour attempts (bright pastel, then
  "cozy night" navy) both read as generic. Removing colour entirely is what made it
  look intentional. Keep a colour variant mocked (scene `A72ceSRfIgH`) if needed.
- **Settings hide.** Long-press the pet for time/brightness/reset; tap a dead pet to
  restart. No hidden gestures for the core loop.

## Icons are images, not glyphs

Every pictogram is generated (`tools/sprites/make_icons.py`) as a rounded PNG and
compiled to RGB565A8 with LVGL's `LVGLImage.py`: burger = feed, ball = play, water
drop = clean, medical cross = medicine, smiley = happy. They are **white
silhouettes**, supersampled 4x then downscaled for smooth edges. This avoids the
two failure modes of glyph icons: no emoji, and no mismatched FontAwesome line-art
that reads as a developer default. Concrete shapes, not abstract ones — an early
set (a star for play, a ring for clean) failed clear user testing.

## State without words

- `pet_need()` (pure, host-tested) returns the single most urgent need
  (MED > CLEAN > FOOD > FUN). One pulsing ring shows it; tapping the ring performs
  the fix.
- `pet_action_enabled()` (pure) drives each button's disabled state, so a refused
  action is simply not pressable.
- The face, the poop sprite and sleep carry the rest.
