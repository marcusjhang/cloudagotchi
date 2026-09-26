# Worked example — an on-device Tamagotchi

A virtual pet on a Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED, I2C touch),
built with ESP-IDF v5.5 + LVGL 9. The conventions in this skill come from here.

## The screen: icon-only, no words (see screens.md)

```
┌────────────────────────────┐
│ (burger) ▰▰▰▰▰▰▱▱▱▱   *  *  │  hunger bar - the icon says what it is
│ (smiley) ▰▰▰▰▰▱▱▱▱▱ (moon) │  happy bar
│              ( ! )         │  one pulsing call over the pet; tap = the fix
│            ( pet )         │  pet centred on a soft pedestal (the hero)
│ ~~~~~~~~~~ ground ~~~~~~~~~│
│  (feed) (play) (clean)(med)│  four round candy buttons, white icons, no words
└────────────────────────────┘
```

The theme is **"cozy night"**: a deep navy sky with a few stars and a moon, a
ground band, muted candy buttons. It stays nearly black (AMOLED-friendly) while
still feeling like a place rather than an empty panel. The screen is truly
icon-only — no clock, no stage, no captions, no toasts, and no words on the
buttons (the owner asked for that after seeing labelled tiles).

Two lessons learned the hard way and worth copying:
- **A bar beats a heart row for needs.** Hearts look nice but a newcomer cannot
  tell what they measure; a rounded bar next to a *concrete icon* (a burger for
  hunger, a smiley for mood) reads instantly.
- **Pick unmistakable shapes.** Abstract glyphs (a star, a ring) failed clear
  user testing; a burger, a ball, a water drop and a medical cross did not.

Settings (time, brightness, reset) hide behind a long-press on the pet; a dead
pet restarts on a tap.

## Icons are images, not glyphs

Every pictogram is generated (`tools/sprites/make_icons.py`) as a rounded PNG and
compiled to RGB565A8 with LVGL's `LVGLImage.py`: burger = feed, ball = play,
water drop = clean, medical cross = medicine, smiley = happy. Icons are **white
silhouettes** so they sit on colour tiles, supersampled 4x then downscaled for
smooth edges. This avoids the two failure modes of glyph icons: no emoji, and no
mismatched FontAwesome line-art that reads as a developer default.

Tile colours are candy accents on black (`UI_C_FEED` orange, `UI_C_PLAY` pink,
`UI_C_CLEAN` sky, `UI_C_MED` green).

## State without words

- `pet_need()` (pure, host-tested) returns the single most urgent need
  (MED > CLEAN > FOOD > FUN). One pulsing badge shows it; **tapping the badge
  performs the fix**; the matching tile gets a bright ring.
- `pet_action_enabled()` (pure) drives each tile's disabled state, so a refused
  action is simply not pressable — no "not hungry" toast.
- The face, the poop sprite and sleep carry the rest.
