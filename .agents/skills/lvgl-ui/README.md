# lvgl-ui

An agent skill for building good-looking **LVGL 9** interfaces on small embedded
displays (ESP32 + AMOLED/TFT, I2C touch). It packages the conventions that keep a
widget screen from looking like a prototype:

- a **design-token scale** built up from a true-black background (`ui_theme.h`),
- one **action-button component** with real pressed / active / disabled states,
- **icon** strategies (pixel-art images, icon-font subset, `LV_SYMBOL_*` interim),
- layout, contrast, motion and a **pre-ship checklist**.

## Install

```bash
npx skills add marcusjhang/lvgl-ui@lvgl-ui
```

## Layout

```
SKILL.md                     the skill entry point
references/screens.md        hero/companion screen: zones, summoned controls, one call
references/tokens.md         colour scale, light/dark panels, geometry
references/buttons.md        button anatomy, styles, state wiring
references/icons.md          icon strategies and pitfalls
references/taste.md          hierarchy, spacing, motion, checklist
references/motifs.md         capsule bars, candy tiles, speech bubble, pedestal
assets/templates/ui_theme.h  drop-in tokens
assets/templates/action_button.c   the component
examples/tamagotchi.md       a complete worked example
```

## License

MIT — see `LICENSE`.
