# Soft motifs (child-friendly look)

Small, reusable pieces that make a screen feel friendly rather than like raw
widgets. All are built from a rounded `lv_obj` plus a label or bar — no assets.

## Capsule stat bars

A thick bar with fully rounded ends and a lighter track reads as a candy meter,
not a progress bar. Label it in mixed case, in the bar's own colour.

```c
#define UI_BAR_H 12
lv_obj_t *bar = lv_bar_create(scr);
lv_obj_set_size(bar, 60, UI_BAR_H);
lv_bar_set_range(bar, 0, 100);
lv_obj_set_style_bg_color(bar, UI_TRACK, LV_PART_MAIN);      // e.g. #1C2836
lv_obj_set_style_bg_color(bar, stat_color, LV_PART_INDICATOR);
lv_obj_set_style_radius(bar, UI_BAR_H / 2, LV_PART_MAIN);
lv_obj_set_style_radius(bar, UI_BAR_H / 2, LV_PART_INDICATOR);
```

Label above: 14 px, mixed case ("Food", "Nap"), colour = the bar colour.

## Candy action tiles

Rounder than overlay buttons, one friendly colour per action, icon on top and a
short caption under it.

```c
lv_obj_set_style_radius(btn, UI_RADIUS_LG, 0);             // 16-18
lv_obj_t *icon = label(btn, 24, lv_color_hex(action_color));  // colour-code it
lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 4);
```

Keep the shared pressed/disabled/checked styles (see `buttons.md`); only the
radius and icon colour differ per tile.

## Speech-bubble caption

Put the caption in a rounded pill so the pet "talks" instead of floating text.
Hide the pill when the caption is empty.

```c
lv_obj_t *box = pill(scr, 280, 38, UI_BUBBLE);   // radius h/2, no border
lv_obj_align(box, LV_ALIGN_CENTER, 0, 88);
lv_obj_t *txt = label(box, 18, UI_TEXT);
lv_obj_set_width(txt, 264);
lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
lv_obj_center(txt);
// on render: lv_obj_set_hidden(box, caption[0] == '\0');
```

## Ground pedestal

A wide, very dark rounded ellipse under the pet stops it floating on black.
Create it *before* the character so the sprite draws on top. Keep it near-black
(it is mostly there for grounding, not brightness).

```c
lv_obj_t *ground = lv_obj_create(scr);
lv_obj_set_size(ground, 210, 28);
lv_obj_set_style_radius(ground, 14, 0);
lv_obj_set_style_bg_color(ground, UI_GROUND, 0);   // e.g. #0F1822
lv_obj_set_style_border_width(ground, 0, 0);
```

## Toast pill

Status/refusal messages: a small rounded pill, in the warn colour, with **dark
ink** so the contrast holds on a bright fill.

```c
lv_obj_t *box = pill(scr, 220, 36, UI_WARN);
lv_obj_t *txt = label(box, 14, UI_ON_WARN);        // near-black
```

## Spacing these together (368 x 448 example)

```
top    status strip (dim, small)
       capsule stat row
       pet sprite on its pedestal
       speech bubble (when there is something to say)
bottom action tiles, toast just above them
```

Leave a clear gap between the toast and the action row; keep the action row the
widest, lowest element so the screen has one obvious place for thumbs.
