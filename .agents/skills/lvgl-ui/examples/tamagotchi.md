# Worked example — an on-device Tamagotchi

A virtual pet on a Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED, I2C touch),
built with ESP-IDF v5.5 + LVGL 9. The conventions in this skill come from here.

## Final screen: monochrome rabbit, minimal chrome

The owner's reference was a small device showing a big friendly **face on black
with no chrome**. That is the design: cuteness comes from the character and from
restraint, not colour. White ink on true black is also best for an AMOLED.

```
┌────────────────────────────┐
│  (chatbox)                 │  food / water / sleep icon, only when it wants one
│            ( rabbit )      │  the hero; frame changes to eat / drink / sleep
│                            │
│   (food) (water) (sleep)   │  three round buttons, white icons, no captions
└────────────────────────────┘
```

Behaviour that makes it feel alive, not like a form:

- **A chatbox, not a wall of meters.** When a need is low, a small rounded bubble
  shows just the icon for it (food / water / sleep / clean / med). It is subtle on
  purpose — the pet should not scream at you. Tapping the bubble gives the item.
- **Food and water drop in.** Pressing Food/Water spawns the icon above the pet and
  animates it down onto it (`lv_anim`, ~450 ms, ease-in), then the pet switches to
  its eat/drink frame for the transient window. Action and feedback are the same
  gesture.
- **Sleep visibly happens.** The sleep button toggles the lights; the rabbit's eyes
  close and it naps. No text explains it.
- **Three controls only.** Food, Water, Sleep. Everything else (clean, medicine) is
  contextual through the chatbox, so a small child sees the fewest possible buttons.
- **One need model.** `pet_need()` (pure, host-tested) returns the single most urgent
  need: MED > CLEAN > FOOD > WATER > SLEEP > FUN. `pet_action_enabled()` dims buttons.

## The art is generated, not a font

The rabbit (`rabbit_idle / eat / drink / sleep / egg`) and every icon are drawn by
`tools/sprites/make_icons.py` as **white silhouettes** with features cut out
(transparent), then compiled to RGB565A8 with LVGL's `LVGLImage.py`. Supersample 4x
then downscale for smooth edges. This gives a consistent, deliberately-drawn set
with no emoji and no mismatched FontAwesome line-art.

## Lessons (learned the hard way)

- Colour was never the problem; clutter was. Two colour passes read as generic;
  monochrome + one character + three controls reads as intentional.
- Abstract glyphs fail. A star for play and a ring for clean confused users; a
  burger, a water drop, a moon and a medical cross did not.
- A bar/label beats a heart row; a concrete icon beats an abstract one.
- Mock the screen (frames in Excalidraw) and let the owner pick before you flash.
