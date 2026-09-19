/*
 * action_button.c — one component for every button.
 *
 * Copy into your screen file. Create the styles once at startup
 * (btn_styles_init()), then make_action_button() per control. Never hand-style
 * a button; add a state here instead.
 */
#include "ui_theme.h"

#define EXT_CLICK_PX 20  // panels with parallax (touch lands 15-25 px low)

static lv_style_t st_btn_base, st_btn_pressed, st_btn_off, st_btn_on;

static void btn_styles_init(void)
{
    lv_style_init(&st_btn_base);
    lv_style_set_bg_color(&st_btn_base, UI_SURFACE);
    lv_style_set_bg_opa(&st_btn_base, LV_OPA_COVER);
    lv_style_set_radius(&st_btn_base, UI_RADIUS);
    lv_style_set_border_width(&st_btn_base, 1);
    lv_style_set_border_color(&st_btn_base, UI_BORDER);
    lv_style_set_shadow_width(&st_btn_base, 0);
    lv_style_set_pad_all(&st_btn_base, 2);

    lv_style_init(&st_btn_pressed);
    lv_style_set_bg_color(&st_btn_pressed, UI_SURFACE_PRESSED);
    lv_style_set_translate_y(&st_btn_pressed, 1);

    lv_style_init(&st_btn_off);
    lv_style_set_bg_color(&st_btn_off, UI_SURFACE_OFF);
    lv_style_set_opa(&st_btn_off, LV_OPA_50);

    lv_style_init(&st_btn_on);  // checked / active
    lv_style_set_border_color(&st_btn_on, UI_ACCENT);
    lv_style_set_bg_color(&st_btn_on, lv_color_mix(UI_ACCENT, UI_SURFACE, 40));
}

// icon: an LV_SYMBOL_* string for now, or swap for an lv_image once art exists.
static lv_obj_t *make_action_button(lv_obj_t *parent, int i,
                                    const char *icon, const char *caption)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, UI_BTN_W, UI_BTN_H);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 7 + i * 60, -12);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
    lv_obj_add_style(btn, &st_btn_base, 0);
    lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &st_btn_off, LV_STATE_DISABLED);
    lv_obj_add_style(btn, &st_btn_on, LV_STATE_CHECKED);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ic, UI_TEXT, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *lb = lv_label_create(btn);
    lv_label_set_text(lb, caption);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lb, UI_TEXT_DIM, 0);
    lv_obj_align(lb, LV_ALIGN_BOTTOM_MID, 0, -2);

    return btn;
}

// On every render, from the snapshot's rule predicate:
//   lv_obj_set_state(btn, LV_STATE_DISABLED, !snap->enabled[act]);
//   lv_obj_set_state(toggle_btn, LV_STATE_CHECKED, snap->asleep);
