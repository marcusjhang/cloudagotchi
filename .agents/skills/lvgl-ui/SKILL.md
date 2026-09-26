---
name: lvgl-ui
description: Design and build good-looking LVGL 9 interfaces for small embedded displays (ESP32 + AMOLED/TFT, I2C touch). Covers a dark-surface design-token scale built up from black, one reusable action-button component with real pressed/active/disabled states, pixel-art icon conventions, layout spacing, and a taste checklist that keeps widget screens from looking like defaults. Use when building or restyling any LVGL screen, buttons, stat bars, overlays, or icons.
---

# lvgl-ui

Opinionated UI craft for LVGL on small screens. The goal is interfaces that look
*designed*, not like raw widgets. These conventions are extracted from a real
device UI (the `tamagotchi` example) and are meant to be copied and adapted.

## When to use

- Building or restyling an LVGL screen (ESP32/IDF, or LVGL on any small panel).
- Buttons, action bars, stat bars, toggles, overlay panels.
- Choosing colours, spacing, iconography; making a screen "not look vibecoded".

## Non-negotiables

1. **One token file.** Every colour, radius, and spacing value is a named macro.
   No raw `lv_color_hex(...)` in widget code. Changing the look must mean editing
   one file, not twenty call sites.
2. **One button component.** Every button is built by the same helper and gets the
   same anatomy and states. Never hand-style buttons individually.
3. **Real states.** At minimum pressed, disabled, and a checked/active state. A UI
   with only a default state reads as a prototype.
4. **Contrast.** Body text ≥ 4.5:1 against its surface; large/bold ≥ 3:1. Dim text
   is for secondary labels only, never for primary content.
5. **Cap the colour count.** ~5 semantic colours, each meaning something. Colour is
   not decoration.

## Workflow

0. If the screen has a character/companion at its centre, read
   `references/screens.md` first — hero layout, summoned controls, coarse needs,
   one "call". It changes everything below.
1. Read `references/tokens.md` and set up `ui_theme.h` for the project's panel
   (dark surfaces on AMOLED black; lighter neutrals on LCD).
2. Copy `assets/templates/action_button.c` and wire `make_action_button()` — see
   `references/buttons.md` for the anatomy and the three styles.
3. Decide which controls can be **disabled/active** and drive those states from a
   single source of truth (a `*_enabled()` rule function), not from UI guesses —
   `references/buttons.md`.
4. For a friendly / child-oriented screen, add the soft pieces from
   `references/motifs.md` (capsule bars, candy action tiles, a speech-bubble
   caption, a ground pedestal, a toast pill).
5. Pick icons per `references/icons.md` (pixel-art images for a game/cute look,
   a FontAwesome subset for a clean tool, `LV_SYMBOL_*` only as an interim).
6. Run the checklist at the end of `references/taste.md` before you ship.

## Layout defaults

- 8 px grid; siblings 8–12 px apart; keep a consistent outer margin.
- Touch targets **≥ 44–48 px**; on glossy panels expect 15–25 px parallax, so give
  every target `lv_obj_set_ext_click_area(obj, 20)` and never rely on the visual edge.
- Align siblings; equal gutters; a row of equal-size buttons beats scattered sizes.
- Overlays (info/settings) are one panel with internal padding, not nested cards.
- Keep one dominant reading direction and one visual hierarchy per screen.

## Motion

- Press feedback: change surface colour and/or `translate_y` by 1 px. 100–150 ms.
- Ease-out only; no bounce. Motion must serve a state change, not decorate.
- Prefer `lv_obj_set_style_transform_*`/styles over per-frame timers.

## Anti-patterns (rewrite if you catch yourself)

- Raw hex colours sprinkled through screen code.
- Text-only buttons at one size with a single state.
- Every control boxed in its own card; nested cards.
- Disabled controls that still look enabled (or pressable).
- A screen of identical tiles with no hierarchy.
- Icons that are actually emoji or mismatched glyph sets.

## Files

- `references/tokens.md` — token scale and panel-specific guidance.
- `references/buttons.md` — button anatomy, styles, state wiring.
- `references/icons.md` — icon strategies and pitfalls.
- `references/taste.md` — spacing, hierarchy, and the pre-ship checklist.
- `references/screens.md` — the hero/companion screen: zones, three-button rule,
  summoned menu, hearts, the one "call".
- `references/motifs.md` — capsule bars, candy tiles, speech bubble, pedestal, toast pill.
- `assets/templates/ui_theme.h` — drop-in token header.
- `assets/templates/action_button.c` — the component.
- `examples/tamagotchi.md` — a complete worked example (stat bars, action bar,
  INFO/SETTINGS overlays).
