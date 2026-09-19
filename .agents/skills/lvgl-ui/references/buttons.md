# Buttons

Every button uses one helper and one set of styles. No per-button styling.

## Anatomy

```
+----------+   rounded rect, 1px border
|   ICON   |   icon glyph or image, ~24-28 px
|  LABEL   |   small dim caption (optional but helps disambiguate icons)
+----------+   target >= 48px tall; ext_click_area 20px for parallax
```

Icon on top, caption at the bottom. Icon is `UI_TEXT`; caption `UI_TEXT_DIM`.

## Styles (created once, applied to every button)

```c
lv_style_init(&st_btn_base);
lv_style_set_bg_color(&st_btn_base, UI_SURFACE);
lv_style_set_bg_opa(&st_btn_base, LV_OPA_COVER);
lv_style_set_radius(&st_btn_base, UI_RADIUS);
lv_style_set_border_width(&st_btn_base, 1);
lv_style_set_border_color(&st_btn_base, UI_BORDER);
lv_style_set_shadow_width(&st_btn_base, 0);

lv_style_init(&st_btn_pressed);
lv_style_set_bg_color(&st_btn_pressed, UI_SURFACE_PRESSED);
lv_style_set_translate_y(&st_btn_pressed, 1);   // 1px "press"

lv_style_init(&st_btn_off);
lv_style_set_bg_color(&st_btn_off, UI_SURFACE_OFF);
lv_style_set_opa(&st_btn_off, LV_OPA_50);

lv_style_init(&st_btn_on);                       // checked / active
lv_style_set_border_color(&st_btn_on, UI_ACCENT);
lv_style_set_bg_color(&st_btn_on, lv_color_mix(UI_ACCENT, UI_SURFACE, 40));
```

```c
lv_obj_add_style(btn, &st_btn_base, 0);
lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
lv_obj_add_style(btn, &st_btn_off, LV_STATE_DISABLED);
lv_obj_add_style(btn, &st_btn_on, LV_STATE_CHECKED);
```

## State wiring (the important part)

The UI must not guess the rules. Expose **one pure predicate** that mirrors the
app's own guards and put the result in the snapshot:

```c
// app side (pure, host-testable)
bool pet_action_enabled(const pet_t *p, pet_action_t a);

// snapshot
bool enabled[PET_ACT_COUNT];

// UI side, on every render
for (int i = 0; i < N; i++)
    lv_obj_set_state(btn[i], LV_STATE_DISABLED, !s->enabled[act[i]]);

// a toggle shows its state
lv_label_set_text(icon[LIGHT], asleep ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
lv_obj_set_state(btn[LIGHT], LV_STATE_CHECKED, asleep);
```

Why: the enabled/disabled logic lives once (and gets a host test), and the UI
stays dumb. Greying out what would fail is the single biggest "this is a real
app" signal.

## Events

- Use `LV_EVENT_CLICKED` for actions; `LV_EVENT_SHORT_CLICKED` +
  `LV_EVENT_LONG_PRESSED` for a button with a secondary gesture.
- Destructive actions: `LV_EVENT_LONG_PRESSED` only, so a tap cannot fire them.
- Keep an `ext_click_area` so a near-miss still lands (parallax).

## Calling LVGL from your own tasks

If a background task changes brightness or a style, wrap it in the LVGL lock.
The esp_lvgl_port mutex is **recursive**, so calling the lock from inside an LVGL
event callback (same task) is safe. If you use a different port, confirm this.
