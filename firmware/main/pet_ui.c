/*
 * The screen (368 x 448). Renders whatever snapshot the game last pushed; turns
 * touches into pet_action_t via the callback. Knows nothing about the rules.
 *
 * Monochrome, face-first (the owner's reference: a big friendly face on black,
 * no chrome). At rest there is only the pet, one tiny status word, and four
 * near-invisible buttons; settings hide behind a long-press on the pet.
 *
 *            ( pet )        <- hero; a pulsing call ring when it needs something
 *             hungry        <- one small lowercase word, dim
 *         ( o ) ( o ) ( o ) ( o )   <- Feed / Play / Clean / Med, white icons
 *
 * Touch lands 15-25 px below where you aim (field guide): every target gets an
 * extended click area. Every touch is logged at INFO.
 */
#include "pet_ui.h"

#include <stdio.h>
#include <time.h>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "esp_log.h"
#include "lvgl.h"

#include "faces/faces.h"
#include "sprites/sprites.h"
#include "game.h"
#include "power.h"
#include "rtc.h"
#include "ui_theme.h"

static const char *TAG = "pet_ui";

/* ---------- layout --------------------------------------------------------- */
#define SCR_W            368
#define SCR_H            448

#define FACE_Y           (-16)
#define WORD_Y           104
#define BADGE_D          60
#define BADGE_DX         6
#define BADGE_DY         (-6)

#define BTN_D            72
#define BTN_X0           16
#define BTN_DX           88
#define BTN_BOTTOM       (-18)

#define EXT_CLICK_PX     20

/* timers */
#define BLINK_PERIOD_MS  4200
#define BLINK_MS         120
#define REPAINT_MS       15000
#define PULSE_MS         650

static pet_ui_action_cb_t s_on_action;

static lv_obj_t *s_face;
static lv_obj_t *s_poop;
static lv_obj_t *s_word;
static lv_obj_t *s_badge;
static lv_obj_t *s_badge_icon;
static lv_obj_t *s_tile[4];
static lv_obj_t *s_settings;
static lv_obj_t *s_settings_time;

static pet_ui_snapshot_t s_snap;
static bool s_have_snap;
static const lv_image_dsc_t *s_shown;
static bool s_badge_on;

static const pet_action_t TILE_ACT[4] = {PET_ACT_FEED_MEAL, PET_ACT_PLAY,
                                         PET_ACT_CLEAN, PET_ACT_MEDICINE};
static const lv_image_dsc_t *TILE_ICON[4] = {&ic_feed, &ic_play, &ic_clean, &ic_med};

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

/* ---------- face selection ------------------------------------------------- */

// Placeholder mapping (cloudagotchi's ghost) until Phase 2 delivers real art.
static const lv_image_dsc_t *face_for(pet_face_t f, bool blink)
{
    switch (f) {
    case PET_FACE_HAPPY:
    case PET_FACE_EATING:
    case PET_FACE_PLAYING:  return blink ? &face_blink_happy : &face_happy;
    case PET_FACE_SAD:
    case PET_FACE_SICK:
    case PET_FACE_DEAD:     return blink ? &face_blink_sad : &face_sad;
    case PET_FACE_SLEEPING: return &face_sleeping;
    case PET_FACE_STARTLED: return &face_talk_3;
    case PET_FACE_EGG:
    case PET_FACE_IDLE:
    case PET_FACE_DIRTY:
    default:                return blink ? &face_blink_neutral : &face_neutral;
    }
}

static bool blinkable(pet_face_t f)
{
    return f == PET_FACE_IDLE || f == PET_FACE_HAPPY || f == PET_FACE_SAD ||
           f == PET_FACE_DIRTY || f == PET_FACE_EGG;
}

static void show_face(const lv_image_dsc_t *src)
{
    if (src == s_shown) return;
    s_shown = src;
    lv_image_set_src(s_face, src);
    lv_obj_invalidate(s_face);
}

// One small lowercase word, the only text on the screen.
static const char *status_word(const pet_ui_snapshot_t *s)
{
    if (s->stage == PET_STAGE_DEAD) return "bye";
    if (s->stage == PET_STAGE_EGG)  return "hatching";
    if (s->asleep)                  return "sleepy";
    switch (s->need) {
    case PET_NEED_MED:   return "sick";
    case PET_NEED_CLEAN: return "dirty";
    case PET_NEED_FOOD:  return "hungry";
    case PET_NEED_FUN:   return "bored";
    default:             return s->face == PET_FACE_HAPPY ? "happy" : "ok";
    }
}

/* ---------- timers --------------------------------------------------------- */

static void blink_open_cb(lv_timer_t *t)
{
    if (s_have_snap) show_face(face_for(s_snap.face, false));
    lv_timer_delete(t);
}

static void blink_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_have_snap || !blinkable(s_snap.face)) return;
    show_face(face_for(s_snap.face, true));
    lv_timer_create(blink_open_cb, BLINK_MS, NULL);
}

static void pulse_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_badge_on || !s_badge) return;
    const lv_opa_t o = lv_obj_get_style_opa(s_badge, 0);
    lv_obj_set_style_opa(s_badge, o > LV_OPA_70 ? LV_OPA_40 : LV_OPA_COVER, 0);
}

static void repaint_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_invalidate(lv_screen_active());
}

/* ---------- the call ring -------------------------------------------------- */

static void badge_icon_for(pet_need_t n, const lv_image_dsc_t **icon)
{
    switch (n) {
    case PET_NEED_MED:   *icon = &ic_med;   break;
    case PET_NEED_CLEAN: *icon = &ic_clean; break;
    case PET_NEED_FOOD:  *icon = &ic_feed;  break;
    case PET_NEED_FUN:   *icon = &ic_play;  break;
    default:             *icon = &ic_happy; break;
    }
}

static void badge_cb(lv_event_t *e)
{
    (void)e;
    if (!s_have_snap) return;
    switch (s_snap.need) {
    case PET_NEED_MED:   s_on_action(PET_ACT_MEDICINE);  break;
    case PET_NEED_CLEAN: s_on_action(PET_ACT_CLEAN);     break;
    case PET_NEED_FOOD:  s_on_action(PET_ACT_FEED_MEAL); break;
    case PET_NEED_FUN:   s_on_action(PET_ACT_PLAY);      break;
    default: break;
    }
}

/* ---------- render --------------------------------------------------------- */

static void render_call(const pet_ui_snapshot_t *s)
{
    const bool on = s->need != PET_NEED_NONE && s->stage != PET_STAGE_DEAD;
    s_badge_on = on;
    lv_obj_set_hidden(s_badge, !on);
    if (!on) return;
    const lv_image_dsc_t *icon;
    badge_icon_for(s->need, &icon);
    lv_image_set_src(s_badge_icon, icon);
    lv_obj_set_style_opa(s_badge, LV_OPA_COVER, 0);
}

static void render_tiles(const pet_ui_snapshot_t *s)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_set_state(s_tile[i], LV_STATE_DISABLED, !s->enabled[TILE_ACT[i]]);
    }
}

static void render(const pet_ui_snapshot_t *s)
{
    render_call(s);
    render_tiles(s);

    lv_label_set_text(s_word, status_word(s));
    show_face(face_for(s->face, false));
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

// No visible words on the play screen; a refused action is silently ignored
// (the buttons themselves are disabled, so it rarely happens).
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

static void action_cb(lv_event_t *e)
{
    const pet_action_t a = (pet_action_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_on_action) s_on_action(a);
}

static void face_cb(lv_event_t *e)
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
    const int64_t delta = (int64_t)(intptr_t)lv_event_get_user_data(e);
    pcf85063_adjust(delta);
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
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;  // hold to confirm
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

static void build_face(lv_obj_t *scr)
{
    s_face = lv_image_create(scr);
    lv_obj_align(s_face, LV_ALIGN_CENTER, 0, FACE_Y);
    lv_obj_set_clickable(s_face, true);
    lv_obj_set_ext_click_area(s_face, EXT_CLICK_PX);
    lv_obj_add_event_cb(s_face, face_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_face, face_cb, LV_EVENT_LONG_PRESSED, NULL);
    show_face(&face_neutral);

    // poop: a dim blob by its feet, monochrome like everything else
    s_poop = lv_obj_create(scr);
    lv_obj_set_size(s_poop, 24, 18);
    lv_obj_set_style_radius(s_poop, 9, 0);
    lv_obj_set_style_bg_color(s_poop, UI_INK_DIM, 0);
    lv_obj_set_style_border_width(s_poop, 0, 0);
    lv_obj_align_to(s_poop, s_face, LV_ALIGN_BOTTOM_RIGHT, 30, 8);
    lv_obj_set_hidden(s_poop, true);

    // the call: a white ring with the need icon, over the pet's shoulder
    s_badge = lv_button_create(scr);
    lv_obj_set_size(s_badge, BADGE_D, BADGE_D);
    lv_obj_set_style_radius(s_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_badge, UI_BG, 0);
    lv_obj_set_style_bg_opa(s_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_badge, 3, 0);
    lv_obj_set_style_border_color(s_badge, UI_INK, 0);
    lv_obj_set_style_shadow_width(s_badge, 0, 0);
    lv_obj_set_ext_click_area(s_badge, EXT_CLICK_PX);
    lv_obj_align_to(s_badge, s_face, LV_ALIGN_TOP_RIGHT, BADGE_DX, BADGE_DY);
    s_badge_icon = lv_image_create(s_badge);
    lv_image_set_src(s_badge_icon, &ic_feed);
    lv_obj_center(s_badge_icon);
    lv_obj_add_event_cb(s_badge, badge_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_badge, true);

    s_word = make_label(scr, &lv_font_montserrat_14, UI_INK_DIM);
    lv_obj_set_style_text_align(s_word, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_word, LV_ALIGN_CENTER, 0, WORD_Y);
    lv_label_set_text(s_word, "");
}

static void build_tiles(lv_obj_t *scr)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, BTN_D, BTN_D);
        lv_obj_set_pos(btn, BTN_X0 + i * BTN_DX, SCR_H + BTN_BOTTOM - BTN_D);
        lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
        lv_obj_add_style(btn, &st_btn, 0);
        lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
        lv_obj_set_style_opa(btn, LV_OPA_30, LV_STATE_DISABLED);

        lv_obj_t *icon = lv_image_create(btn);
        lv_image_set_src(icon, TILE_ICON[i]);
        lv_obj_center(icon);

        lv_obj_add_event_cb(btn, action_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)TILE_ACT[i]);
        s_tile[i] = btn;
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

    build_face(scr);
    build_tiles(scr);
    build_settings(scr);
    register_touch_log();

    lv_timer_create(blink_cb, BLINK_PERIOD_MS, NULL);
    lv_timer_create(pulse_cb, PULSE_MS, NULL);
    lv_timer_create(repaint_cb, REPAINT_MS, NULL);

    bsp_display_unlock();
}
