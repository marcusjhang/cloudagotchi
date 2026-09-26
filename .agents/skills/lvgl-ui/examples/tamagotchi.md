# Worked example — an on-device Tamagotchi

The conventions in this skill come from a real device: a virtual pet on a
Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED, I2C touch), built with
ESP-IDF v5.5 + LVGL 9.

## Screen (368 x 448)

```
status strip   "baby · 2h 13m"            (left, 14px dim)
               "14:07  87%"               (right, 14px dim)
stat row       5 labelled bars: FOOD FUN ZZZ CLEAN HP
face           one image widget, swapped per mood (pixel-art sprite at 2x)
caption        one line, 18px ("hungry", "zzz", ...)
toast          "not hungry" (14px, warn colour), ~1.5 s
action bar     FEED PLAY LIGHT CLEAN MED INFO   (icon + caption, 54x56 each)
overlays       INFO (stats) and SETTINGS, one panel each, tap/hold to toggle
background     true black
```

## Buttons

Six `make_action_button()`s in one row. Each is a rounded `UI_SURFACE` tile with
a 1px border, a 24px `LV_SYMBOL_*` icon and a 14px dim caption. Styles give
pressed (lighter surface + 1px down), disabled (near-black + 50% opacity) and
checked (accent border).

Icon mapping (interim; pixel-art images land with the sprite set):

| Action | Symbol | Notes |
|---|---|---|
| FEED   | `LV_SYMBOL_PLUS`  | "add food" |
| PLAY   | `LV_SYMBOL_PLAY`  | |
| LIGHT  | `LV_SYMBOL_EYE_OPEN` / `EYE_CLOSE` | reflects sleep |
| CLEAN  | `LV_SYMBOL_TRASH` | |
| MED    | `LV_SYMBOL_TINT`  | droplet |
| INFO   | `LV_SYMBOL_LIST`  | long-press = SETTINGS |

## Real states (the payoff)

A pure predicate on the app side is the single source of truth:

```c
bool pet_action_enabled(const pet_t *p, pet_action_t a);
```

The snapshot carries `enabled[PET_ACT_COUNT]`; the bar greys out **FEED/PLAY when
asleep**, **MED when healthy**, **CLEAN when already clean**, and lights **LIGHT**
while the pet sleeps. The UI never re-derives the rules, and the predicate is
host-tested.

## Theme

`ui_theme.h` holds every colour and measurement. The stat-bar colours are the one
content palette (food green, fun pink, energy teal, clean blue, health red);
everything else is surface/border/ink/accent. Changing the look is a one-file
edit.

## Soft motifs used here

The child-friendly pass applies `references/motifs.md`: capsule stat bars with a
lighter track and mixed-case labels, rounder candy action tiles with colour-coded
icons, the caption in a speech bubble, a dark ground pedestal under the sprite,
and the toast as a warn-coloured pill with dark ink.
