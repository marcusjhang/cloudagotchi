/*
 * The screen (368 x 448). Renders whatever snapshot the game last pushed; turns
 * touches into pet_action_t via the callback. Knows nothing about the rules.
 *
 * Monochrome rabbit. The pet is the screen; a small chatbox shows what it wants
 * (an icon, subtle - not in your face); giving food or water drops the item onto
 * it and it eats or drinks; the sleep button switches the lights off and it naps.
 *
 *            (chatbox)      <- food / water / sleep icon, only when it wants one
 *             ( rabbit )
 *        (food) (water) (sleep)
 *
 * Touch lands 15-25 px below where you aim (field guide): extended click areas.
 */
#include "pet_ui.h"

#include <stdio.h>
#include <time.h>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "esp_log.h"
#include "lvgl.h"

#include "sprites/sprites.h"
#include "game.h"
#include "power.h"
#include "rtc.h"
#include "ui_theme.h"

static const char *TAG = "pet_ui";

/* ---------- layout --------------------------------------------------------- */
#define SCR_W            368
#define SCR_H            448

#define PET_CENTER_Y     186
#define CHAT_X           46
#define CHAT_Y           34
#define CHAT_D           96
#define DROP_X           160
#define DROP_Y0          18
#define DROP_Y1          150

#define BTN_D            72
#define BTN_X0           28
#define BTN_STEP         120
#define BTN_BOTTOM       (-20)

#define EXT_CLICK_PX     22

#define REPAINT_MS       15000
#define PULSE_MS         700
#define DROP_MS          450

static pet_ui_action_cb_t s_on_action;

static lv_obj_t *s_pet;
static lv_obj_t *s_poop;
static lv_obj_t *s_chat;
static lv_obj_t *s_chat_icon;
static lv_obj_t *s_drop;
static lv_obj_t *s_btn[3];
static lv_obj_t *s_settings;
static lv_obj_t *s_settings_time;

static pet_ui_snapshot_t s_snap;
static bool s_have_snap;
static const lv_image_dsc_t *s_shown;
static bool s_chat_on;

typedef enum { GIVE_NONE, GIVE_FOOD, GIVE_WATER } give_t;
static give_t s_give;

static const pet_action_t BTN_ACT[3] = {PET_ACT_FEED_MEAL, PET_ACT_DRINK, PET_ACT_LIGHTS};
static const lv_image_dsc_t *BTN_ICON[3] = {&ic_feed, &ic_water, &ic_sleep};

/* ---------- styles --------------------------------------------------------- */

static lv_style_t st_btn, st_btn_pressed;

static void btn_styles_init(void)
{
    lv_style_init(&st_btn);
    lv_style_set_radius(&st_btn, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&st_btn, UI_SURFACE);
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_border_width(&st_btn, 0);
    lv_style_set_shadow_width(&st_btn, 0);
    lv_style_set_pad_all(&st_btn, 0);

    lv_style_init(&st_btn_pressed);
    lv_style_set_bg_color(&st_btn_pressed, UI_SURFACE_PRESSED);
}

/* ---------- rabbit frames -------------------------------------------------- */

static const lv_image_dsc_t *frame_for(const pet_ui_snapshot_t *s)
{
    if (s->stage == PET_STAGE_EGG) return &rabbit_egg;
    if (s->asleep)                 return &rabbit_sleep;
    if (s->face == PET_FACE_EATING) {
        if (s_give == GIVE_WATER)  return &rabbit_drink;
        if (s_give == GIVE_FOOD)   return &rabbit_eat;
        return &rabbit_eat;
    }
    return &rabbit_idle;
}

static void show_frame(const lv_image_dsc_t *src)
{
    if (src == s_shown) return;
    s_shown = src;
    lv_image_set_src(s_pet, src);
    lv_obj_invalidate(s_pet);
}

/* ---------- the chatbox ---------------------------------------------------- */

static void need_icon(pet_need_t n, const lv_image_dsc_t **icon)
{
    switch (n) {
    case PET_NEED_MED:   *icon = &ic_med;   break;
    case PET_NEED_CLEAN: *icon = &ic_clean; break;
    case PET_NEED_FOOD:  *icon = &ic_feed;  break;
    case PET_NEED_WATER: *icon = &ic_water; break;
    case PET_NEED_SLEEP: *icon = &ic_sleep; break;
    case PET_NEED_FUN:   *icon = &ic_play;  break;
    default:             *icon = &ic_feed;  break;
    }
}

static void drop_done_cb(lv_anim_t *a)
{
    (void)a;
    lv_obj_set_hidden(s_drop, true);
}

// Give the pet what it is asking for, and (for food/water) drop the item in.
static void give(pet_action_t a)
{
    if (!s_on_action) return;
    if (a == PET_ACT_FEED_MEAL) s_give = GIVE_FOOD;
    else if (a == PET_ACT_DRINK) s_give = GIVE_WATER;

    if (a == PET_ACT_FEED_MEAL || a == PET_ACT_DRINK) {
        lv_image_set_src(s_drop, a == PET_ACT_DRINK ? &ic_water : &ic_feed);
        lv_obj_set_pos(s_drop, DROP_X, DROP_Y0);
        lv_obj_set_hidden(s_drop, false);
        lv_anim_t an;
        lv_anim_init(&an);
        lv_anim_set_var(&an, s_drop);
        lv_anim_set_exec_cb(&an, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&an, DROP_Y0, DROP_Y1);
        lv_anim_set_time(&an, DROP_MS);
        lv_anim_set_path_cb(&an, lv_anim_path_ease_in);
        lv_anim_set_ready_cb(&an, drop_done_cb);
        lv_anim_start(&an);
    }
    s_on_action(a);
}

static void chat_cb(lv_event_t *e)
{
    (void)e;
    if (!s_have_snap) return;
    switch (s_snap.need) {
    case PET_NEED_MED:   give(PET_ACT_MEDICINE);  break;
    case PET_NEED_CLEAN: give(PET_ACT_CLEAN);     break;
    case PET_NEED_FOOD:  give(PET_ACT_FEED_MEAL); break;
    case PET_NEED_WATER: give(PET_ACT_DRINK);     break;
    case PET_NEED_SLEEP: give(PET_ACT_LIGHTS);    break;
    case PET_NEED_FUN:   give(PET_ACT_PLAY);      break;
    default: break;
    }
}

/* ---------- timers --------------------------------------------------------- */

static void pulse_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_chat_on || !s_chat) return;
    const lv_opa_t o = lv_obj_get_style_opa(s_chat, 0);
    lv_obj_set_style_opa(s_chat, o > LV_OPA_70 ? LV_OPA_40 : LV_OPA_COVER, 0);
}

static void repaint_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_invalidate(lv_screen_active());
}

/* ---------- render --------------------------------------------------------- */

static void render_chat(const pet_ui_snapshot_t *s)
{
    const bool on = s->need != PET_NEED_NONE && s->stage != PET_STAGE_DEAD;
    s_chat_on = on;
    lv_obj_set_hidden(s_chat, !on);
    if (!on) return;
    const lv_image_dsc_t *icon;
    need_icon(s->need, &icon);
    lv_image_set_src(s_chat_icon, icon);
    lv_obj_set_style_opa(s_chat, LV_OPA_COVER, 0);
}

static void render(const pet_ui_snapshot_t *s)
{
    render_chat(s);

    for (int i = 0; i < 3; i++) {
        lv_obj_set_state(s_btn[i], LV_STATE_DISABLED, !s->enabled[BTN_ACT[i]]);
    }

    if (s->face != PET_FACE_EATING) s_give = GIVE_NONE;  // clear the eat/drink frame
    show_frame(frame_for(s));
    lv_obj_set_hidden(s_poop, !s->dirty);

    if (!lv_obj_is_hidden(s_settings) && lv_obj_is_valid(s_settings_time)) {
        char hm[8] = "--:--";
        if (s->clock_ok) {
            struct tm lt;
            const time_t t = (time_t)s->clock_s;
            localtime_r(&t, &lt);
            strftime(hm, sizeof(hm), "%H:%M", &lt);
        }
        lv_label_set_text(s_settings_time, hm);
    }
}

void pet_ui_update(const pet_ui_snapshot_t *s)
{
    bsp_display_lock(0);
    s_snap = *s;
    s_have_snap = true;
    render(s);
    bsp_display_unlock();
}

void pet_ui_toast(const char *msg)
{
    ESP_LOGI(TAG, "blocked: %s", msg);
}

/* ---------- input ----------------------------------------------------------- */

static void touch_log_cb(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    ESP_LOGI(TAG, "touch %s %d,%d", lv_event_get_code(e) == LV_EVENT_PRESSED ? "down" : "up  ",
             (int)p.x, (int)p.y);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        power_note_activity();
    }
}

static void btn_cb(lv_event_t *e)
{
    give((pet_action_t)(uintptr_t)lv_event_get_user_data(e));
}

static void pet_cb(lv_event_t *e)
{
    const lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_LONG_PRESSED) {
        lv_obj_set_hidden(s_settings, false);
        if (s_have_snap) render(&s_snap);
    } else if (c == LV_EVENT_CLICKED && s_have_snap && s_snap.stage == PET_STAGE_DEAD &&
               s_on_action) {
        s_on_action(PET_ACT_NEW_EGG);
    }
}

static void settings_close_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_settings, true);
}

static void settings_time_cb(lv_event_t *e)
{
    pcf85063_adjust((int64_t)(intptr_t)lv_event_get_user_data(e));
}

static void settings_bright_cb(lv_event_t *e)
{
    (void)e;
    static const int steps[] = {100, 60, 30};
    static int idx;
    idx = (idx + 1) % (int)(sizeof(steps) / sizeof(steps[0]));
    power_set_awake_brightness(steps[idx]);
}

static void settings_reset_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    game_reset();
    lv_obj_set_hidden(s_settings, true);
}

/* ---------- builders ------------------------------------------------------- */

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static lv_obj_t *make_sbtn(lv_obj_t *parent, const char *text, int x, int y, int w,
                           lv_event_cb_t cb, void *user, bool hold)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, 40);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
    lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, UI_RADIUS, 0);
    lv_obj_set_style_bg_color(btn, UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_t *label = make_label(btn, &lv_font_montserrat_14, UI_INK);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, cb, hold ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED, user);
    return btn;
}

static void build_pet(lv_obj_t *scr)
{
    s_pet = lv_image_create(scr);
    lv_obj_align(s_pet, LV_ALIGN_CENTER, 0, PET_CENTER_Y - SCR_H / 2);
    lv_obj_set_clickable(s_pet, true);
    lv_obj_set_ext_click_area(s_pet, EXT_CLICK_PX);
    lv_obj_add_event_cb(s_pet, pet_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_pet, pet_cb, LV_EVENT_LONG_PRESSED, NULL);
    show_frame(&rabbit_idle);

    s_poop = lv_obj_create(scr);
    lv_obj_set_size(s_poop, 24, 18);
    lv_obj_set_style_radius(s_poop, 9, 0);
    lv_obj_set_style_bg_color(s_poop, UI_INK_DIM, 0);
    lv_obj_set_style_border_width(s_poop, 0, 0);
    lv_obj_align_to(s_poop, s_pet, LV_ALIGN_BOTTOM_RIGHT, 40, 6);
    lv_obj_set_hidden(s_poop, true);

    // the chatbox: a small rounded bubble with the need icon; tap it to give
    s_chat = lv_button_create(scr);
    lv_obj_set_size(s_chat, CHAT_D, CHAT_D);
    lv_obj_set_style_radius(s_chat, CHAT_D / 2, 0);
    lv_obj_set_style_bg_color(s_chat, UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_chat, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_chat, 2, 0);
    lv_obj_set_style_border_color(s_chat, UI_LINE, 0);
    lv_obj_set_style_shadow_width(s_chat, 0, 0);
    lv_obj_set_pos(s_chat, CHAT_X, CHAT_Y);
    lv_obj_set_ext_click_area(s_chat, EXT_CLICK_PX);
    s_chat_icon = lv_image_create(s_chat);
    lv_image_set_src(s_chat_icon, &ic_feed);
    lv_obj_center(s_chat_icon);
    lv_obj_add_event_cb(s_chat, chat_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_chat, true);

    // the falling food/water
    s_drop = lv_image_create(scr);
    lv_image_set_src(s_drop, &ic_feed);
    lv_obj_set_pos(s_drop, DROP_X, DROP_Y0);
    lv_obj_set_hidden(s_drop, true);
}

static void build_buttons(lv_obj_t *scr)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, BTN_D, BTN_D);
        lv_obj_set_pos(btn, BTN_X0 + i * BTN_STEP, SCR_H + BTN_BOTTOM - BTN_D);
        lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
        lv_obj_add_style(btn, &st_btn, 0);
        lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
        lv_obj_set_style_opa(btn, LV_OPA_30, LV_STATE_DISABLED);

        lv_obj_t *icon = lv_image_create(btn);
        lv_image_set_src(icon, BTN_ICON[i]);
        lv_obj_center(icon);

        lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)BTN_ACT[i]);
        s_btn[i] = btn;
    }
}

static void build_settings(lv_obj_t *scr)
{
    s_settings = lv_obj_create(scr);
    lv_obj_set_size(s_settings, 300, 250);
    lv_obj_align(s_settings, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_settings, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_settings, UI_LINE, 0);
    lv_obj_set_style_border_width(s_settings, 1, 0);
    lv_obj_set_style_radius(s_settings, UI_RADIUS_LG, 0);
    lv_obj_set_scrollable(s_settings, false);

    lv_obj_t *title = make_label(s_settings, &lv_font_montserrat_14, UI_INK_DIM);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_pos(title, 16, 10);

    lv_obj_t *time_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_INK);
    lv_label_set_text(time_lbl, "Time");
    lv_obj_set_pos(time_lbl, 16, 54);
    s_settings_time = make_label(s_settings, &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_settings_time, 16, 80);
    make_sbtn(s_settings, "-1h", 150, 50, 60, settings_time_cb, (void *)(intptr_t)-3600, false);
    make_sbtn(s_settings, "+1h", 218, 50, 60, settings_time_cb, (void *)(intptr_t)3600, false);

    lv_obj_t *br_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_INK);
    lv_label_set_text(br_lbl, "Brightness");
    lv_obj_set_pos(br_lbl, 16, 118);
    make_sbtn(s_settings, "cycle", 170, 114, 108, settings_bright_cb, NULL, false);

    lv_obj_t *reset_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_INK);
    lv_label_set_text(reset_lbl, "Reset pet");
    lv_obj_set_pos(reset_lbl, 16, 166);
    make_sbtn(s_settings, "hold", 170, 162, 108, settings_reset_cb, NULL, true);

    make_sbtn(s_settings, "CLOSE", 100, 206, 100, settings_close_cb, NULL, false);
    lv_obj_set_hidden(s_settings, true);
}

static void register_touch_log(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (!indev) return;
    lv_indev_add_event_cb(indev, touch_log_cb, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(indev, touch_log_cb, LV_EVENT_RELEASED, NULL);
}

void pet_ui_start(pet_ui_action_cb_t on_action)
{
    s_on_action = on_action;

    bsp_display_start();
    bsp_display_lock(0);
    btn_styles_init();
    bsp_display_brightness_set(100);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_BG, 0);  // AMOLED: black = pixels off
    lv_obj_set_scrollable(scr, false);

    build_pet(scr);
    build_buttons(scr);
    build_settings(scr);
    register_touch_log();

    lv_timer_create(pulse_cb, PULSE_MS, NULL);
    lv_timer_create(repaint_cb, REPAINT_MS, NULL);

    bsp_display_unlock();
}
