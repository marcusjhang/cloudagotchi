# Worked example — an on-device Tamagotchi

A virtual pet on a Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED, I2C touch),
built with ESP-IDF v5.5 + LVGL 9. The conventions in this skill come from here.

## The screen (hero layout, see screens.md)

```
top      "teen · 3d 4h"  (left)          "14:07 87%" (right)      small, dim
need     Food ♥♥♥♡      Fun ♥♥♥♡     four hearts each, like the original
middle   the pet, centred, on a soft pedestal
         a pulsing call badge over its shoulder when it needs something
bottom   [ Feed ]  [ Play ]  [ Clean ]            (three big tiles)      gear
overlays MENU (icon grid), METER, SETTINGS - all summoned
```

At rest there are **three buttons and one call**. Everything else lives behind the
gear, exactly like the original Tamagotchi's summoned icon row.

## Buttons

Three `94x80` tiles at the bottom, ~7.4 mm on this panel, colour-coded icons
(Feed green, Play pink, Clean blue) and one-word captions. Shared pressed /
disabled / checked styles; the checked state marks the button the call points at.

The gear opens an 8-tile icon grid: Feed, Snack, Lights, Clean, Med, Meter,
Settings, Close. Snack and Lights live here rather than behind a long-press, so
nothing important is a hidden gesture.

## The call

`pet_need()` (pure, host-tested) returns the one most urgent need — MED over
CLEAN over FOOD over FUN. The snapshot carries it; the UI shows one pulsing badge
with a matching icon and colour, and **tapping the badge performs the fix**.
Sickness outranks everything; a sleeping pet is not nagged.

## Theme

`ui_theme.h` holds every colour and measurement: surfaces stepped up from black,
one accent, semantic success/warn/danger, plus the soft-motif tokens
(`UI_TRACK`, `UI_GROUND`, `UI_BUBBLE`, `UI_ON_WARN`). Changing the look is a
one-file edit.
