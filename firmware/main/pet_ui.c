/*
 * The screen (368 x 448). Renders whatever snapshot the game last pushed; turns
 * touches into pet_action_t via the callback. Knows nothing about the rules.
 *
 * Design (from the Tamagotchi UI research): the pet IS the screen. At rest there
 * are only three big thumb-sized buttons and one need "call"; the full icon menu
 * is summoned behind the gear, like the original's A-button icon grid.
 *
 *   top      status: "teen · 3d 4h" (left)   "14:07 87%" (right)   [adult info]
 *   needs    Food ♥♥♥♡   Fun ♥♥♥♡     (four hearts each, like the original)
 *   middle   the pet on a soft pedestal, with a pulsing call badge when it needs
 *            something; tap the badge to fix it
 *   bottom   FEED  PLAY  CLEAN  (three big rounded tiles)        gear (menu)
 *   overlays MENU (icon grid: Feed/Snack/Lights/Clean/Med/Meter/Settings/Close),
 *            METER (adult stats) and SETTINGS, both summoned
 *
 * Touch on this glass lands 15-25 px below where you aim (field guide), so every
 * target gets an extended click area (EXT_CLICK_PX) and nothing is under 48 px.
 * Every touch is logged at INFO - that log is how those numbers were found.
 */
#include "pet_ui.h"

#include <stdio.h>
#include <time.h>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "esp_log.h"
#include "lvgl.h"

#include "faces/faces.h"
#include "game.h"
#include "power.h"
#include "rtc.h"
#include "ui_theme.h"

static const char *TAG = "pet_ui";

/* ---------- layout: every position lives here ----------------------------- */
#define SCR_W            368
#define SCR_H            448
#define PAD_H            16
#define STATUS_Y         10

#define NEEDS_LABEL_X    16
#define NEEDS_HEARTS_X   62
#define NEEDS_HEART_DX   30
#define NEEDS_ROW1_Y     36
#define NEEDS_ROW2_Y     72

#define FACE_Y           (-8)
#define GROUND_DY        70
#define BADGE_DX         8
#define BADGE_DY         (-8)

#define BIG_W            94
#define BIG_H            80
#define BIG_X0           6
#define BIG_DX           100
#define BIG_BOTTOM       (-14)
#define GEAR_W           44
#define GEAR_BOTTOM      (-32)

#define TOAST_BOTTOM     (-108)
#define EXT_CLICK_PX     20

#define HEARTS           4

/* timers */
#define BLINK_PERIOD_MS  4200
#define BLINK_MS         120
#define TOAST_MS         1500
#define REPAINT_MS       15000
#define PULSE_MS         600

static pet_ui_action_cb_t s_on_action;

static lv_obj_t *s_status;
static lv_obj_t *s_clock;
static lv_obj_t *s_hearts[2][HEARTS];   // 0 = food, 1 = fun
static lv_obj_t *s_ground;
static lv_obj_t *s_face;
static lv_obj_t *s_poop;
static lv_obj_t *s_badge;
static lv_obj_t *s_badge_icon;
static lv_obj_t *s_toast_box;
static lv_obj_t *s_toast;

static lv_obj_t *s_big[3];              // Feed, Play, Clean
static const pet_action_t BIG_ACT[3] = {PET_ACT_FEED_MEAL, PET_ACT_PLAY, PET_ACT_CLEAN};
static const char *const BIG_ICON[3] = {LV_SYMBOL_PLUS, LV_SYMBOL_PLAY, LV_SYMBOL_TRASH};
static const char *const BIG_TEXT[3] = {"Feed", "Play", "Clean"};
static const uint32_t BIG_COLOR[3] = {0x9CC959, 0xFF6392, 0x5DA9E9};

static lv_obj_t *s_menu;
static lv_obj_t *s_meter;
static lv_obj_t *s_meter_text;
static lv_obj_t *s_settings;
static lv_obj_t *s_settings_time;
static lv_timer_t *s_toast_timer;

static pet_ui_snapshot_t s_snap;
static bool s_have_snap;
static const lv_image_dsc_t *s_shown;
static bool s_badge_on;

/* stat → 0..HEARTS, rounded to the nearest heart */
static int hearts_of(int stat)
{
    const int h = (stat * HEARTS + 50) / 100;
    return h < 0 ? 0 : h > HEARTS ? HEARTS : h;
}

/* ---------- styles (created once) ----------------------------------------- */

static lv_style_t st_tile, st_tile_pressed, st_tile_off, st_tile_on;

static void tile_styles_init(void)
{
    lv_style_init(&st_tile);
    lv_style_set_bg_color(&st_tile, UI_SURFACE);
    lv_style_set_bg_opa(&st_tile, LV_OPA_COVER);
    lv_style_set_radius(&st_tile, UI_RADIUS_LG);
    lv_style_set_border_width(&st_tile, 1);
    lv_style_set_border_color(&st_tile, UI_BORDER);
    lv_style_set_shadow_width(&st_tile, 0);
    lv_style_set_pad_all(&st_tile, 2);

    lv_style_init(&st_tile_pressed);
    lv_style_set_bg_color(&st_tile_pressed, UI_SURFACE_PRESSED);
    lv_style_set_translate_y(&st_tile_pressed, 2);

    lv_style_init(&st_tile_off);
    lv_style_set_bg_color(&st_tile_off, UI_SURFACE_OFF);
    lv_style_set_opa(&st_tile_off, LV_OPA_50);

    lv_style_init(&st_tile_on);
    lv_style_set_border_color(&st_tile_on, UI_ACCENT);
    lv_style_set_bg_color(&st_tile_on, lv_color_mix(UI_ACCENT, UI_SURFACE, 40));
}

static void apply_tile_style(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &st_tile, 0);
    lv_obj_add_style(obj, &st_tile_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(obj, &st_tile_off, LV_STATE_DISABLED);
    lv_obj_add_style(obj, &st_tile_on, LV_STATE_CHECKED);
}

/* ---------- face selection ------------------------------------------------ */

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

// Swap the sprite and invalidate: a dropped QSPI transfer can leave slivers
// of the previous sprite, and a plain set_src only repaints the diff.
static void show_face(const lv_image_dsc_t *src)
{
    if (src == s_shown) return;
    s_shown = src;
    lv_image_set_src(s_face, src);
    lv_obj_invalidate(s_face);
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
    if (s_badge_on && s_badge) {
        lv_obj_set_style_opa(s_badge, lv_obj_get_style_opa(s_badge, 0) > LV_OPA_70 ? LV_OPA_50
                                                                                 : LV_OPA_COVER, 0);
    }
}

static void repaint_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_invalidate(lv_screen_active());
}

static void toast_hide_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_set_hidden(s_toast_box, true);
    s_toast_timer = NULL;
}

/* ---------- formatting helpers --------------------------------------------- */

static void fmt_age(char *buf, size_t n, uint32_t s)
{
    const uint32_t d = s / 86400, h = (s / 3600) % 24, m = (s / 60) % 60;
    if (d) snprintf(buf, n, "%lud %luh", (unsigned long)d, (unsigned long)h);
    else   snprintf(buf, n, "%luh %02lum", (unsigned long)h, (unsigned long)m);
}

static void fmt_clock(char *buf, size_t n, const pet_ui_snapshot_t *s)
{
    if (!s->clock_ok) {
        buf[0] = '\0';
        return;
    }
    struct tm lt;
    const time_t t = (time_t)s->clock_s;
    localtime_r(&t, &lt);
    char hm[8];
    strftime(hm, sizeof(hm), "%H:%M", &lt);

    if (s->batt_ok && s->batt.present) {
        snprintf(buf, n, "%s  %d%%", hm, s->batt.percent);
    } else if (s->batt_ok && s->batt.vbus) {
        snprintf(buf, n, "%s  USB", hm);
    } else {
        snprintf(buf, n, "%s", hm);
    }
}

static const char *power_line(const pet_ui_snapshot_t *s, char *buf, size_t n)
{
    if (!s->batt_ok) {
        return "unknown";
    }
    if (s->batt.present) {
        snprintf(buf, n, "%d%%  %d mV%s", s->batt.percent, s->batt.millivolts,
                 s->batt.charging ? "  charging" : "");
    } else {
        snprintf(buf, n, "USB power, no battery");
    }
    return buf;
}

/* ---------- the "call" badge ----------------------------------------------- */

static void badge_icon_for(pet_need_t n, const char **icon, uint32_t *color)
{
    switch (n) {
    case PET_NEED_MED:   *icon = LV_SYMBOL_WARNING; *color = 0xFF5A5F; break;
    case PET_NEED_CLEAN: *icon = LV_SYMBOL_TRASH;   *color = 0x5DA9E9; break;
    case PET_NEED_FOOD:  *icon = LV_SYMBOL_PLUS;    *color = 0x9CC959; break;
    case PET_NEED_FUN:   *icon = LV_SYMBOL_PLAY;    *color = 0xFF6392; break;
    default:             *icon = LV_SYMBOL_OK;      *color = 0x2EC4B6; break;
    }
}

static void badge_cb(lv_event_t *e)
{
    (void)e;
    if (!s_have_snap) return;
    switch (s_snap.need) {  // tap the call to do the thing it is asking for
    case PET_NEED_MED:   s_on_action(PET_ACT_MEDICINE);   break;
    case PET_NEED_CLEAN: s_on_action(PET_ACT_CLEAN);      break;
    case PET_NEED_FOOD:  s_on_action(PET_ACT_FEED_MEAL);  break;
    case PET_NEED_FUN:   s_on_action(PET_ACT_PLAY);       break;
    default: break;
    }
}

/* ---------- rendering a snapshot ------------------------------------------- */

static void render_needs(const pet_ui_snapshot_t *s)
{
    const int hearts[2] = {hearts_of(s->stats[PET_STAT_FULLNESS]),
                           hearts_of(s->stats[PET_STAT_HAPPINESS])};
    for (int g = 0; g < 2; g++) {
        for (int i = 0; i < HEARTS; i++) {
            lv_obj_set_style_opa(s_hearts[g][i], i < hearts[g] ? LV_OPA_COVER : LV_OPA_20, 0);
        }
    }
}

static void render_call(const pet_ui_snapshot_t *s)
{
    const bool on = s->need != PET_NEED_NONE && s->stage != PET_STAGE_DEAD;
    s_badge_on = on;
    lv_obj_set_hidden(s_badge, !on);
    if (!on) return;

    const char *icon;
    uint32_t color;
    badge_icon_for(s->need, &icon, &color);
    lv_label_set_text(s_badge_icon, icon);
    lv_obj_set_style_bg_color(s_badge, lv_color_hex(color), 0);
    lv_obj_set_style_opa(s_badge, LV_OPA_COVER, 0);
}

static void render_bottom(const pet_ui_snapshot_t *s)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_set_state(s_big[i], LV_STATE_DISABLED, !s->enabled[BIG_ACT[i]]);
    }
    // highlight the button the call is pointing at (no hidden gestures for kids)
    lv_obj_set_state(s_big[0], LV_STATE_CHECKED, s->need == PET_NEED_FOOD);
    lv_obj_set_state(s_big[1], LV_STATE_CHECKED, s->need == PET_NEED_FUN);
    lv_obj_set_state(s_big[2], LV_STATE_CHECKED, s->need == PET_NEED_CLEAN);
}

// Must hold the display lock.
static void render(const pet_ui_snapshot_t *s)
{
    char age[24], line[64], clock[32];
    fmt_age(age, sizeof(age), s->age_s);
    snprintf(line, sizeof(line), "%s  ·  %s", pet_stage_name(s->stage), age);
    lv_label_set_text(s_status, line);
    fmt_clock(clock, sizeof(clock), s);
    lv_label_set_text(s_clock, clock);

    render_needs(s);
    render_call(s);
    render_bottom(s);

    show_face(face_for(s->face, false));
    lv_obj_set_hidden(s_poop, !s->dirty);

    if (!lv_obj_is_hidden(s_meter)) {
        char power[48];
        power_line(s, power, sizeof(power));
        char info[224];
        snprintf(info, sizeof(info),
                 "stage      %s\nage        %s\nweight     %u\nmistakes   %u\n"
                 "power      %s\nboots      %lu",
                 pet_stage_name(s->stage), age, s->weight, s->mistakes,
                 power, (unsigned long)s->boots);
        lv_label_set_text(s_meter_text, info);
    }

    if (!lv_obj_is_hidden(s_settings) && lv_obj_is_valid(s_settings_time)) {
        struct tm lt;
        const time_t t = (time_t)s->clock_s;
        if (s->clock_ok) {
            localtime_r(&t, &lt);
            strftime(clock, sizeof(clock), "%H:%M", &lt);
        } else {
            snprintf(clock, sizeof(clock), "--:--");
        }
        lv_label_set_text(s_settings_time, clock);
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
    bsp_display_lock(0);
    lv_label_set_text(s_toast, msg);
    lv_obj_set_hidden(s_toast_box, false);
    if (s_toast_timer) lv_timer_reset(s_toast_timer);
    else s_toast_timer = lv_timer_create(toast_hide_cb, TOAST_MS, NULL);
    bsp_display_unlock();
}

/* ---------- input ----------------------------------------------------------- */

static void touch_log_cb(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    ESP_LOGI(TAG, "touch %s %d,%d", lv_event_get_code(e) == LV_EVENT_PRESSED ? "down" : "up  ",
             (int)p.x, (int)p.y);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        power_note_activity();  // any touch wakes the screen, not just buttons
    }
}

static void action_cb(lv_event_t *e)
{
    const pet_action_t a = (pet_action_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_on_action) s_on_action(a);
}

static void face_cb(lv_event_t *e)
{
    // A tap on a dead pet starts a new egg; nothing else is hidden behind the pet.
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && s_have_snap &&
        s_snap.stage == PET_STAGE_DEAD && s_on_action) {
        s_on_action(PET_ACT_NEW_EGG);
    }
}

static void menu_close_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_menu, true);
}

static void menu_open_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_menu, false);
}

static void menu_act_cb(lv_event_t *e)
{
    const pet_action_t a = (pet_action_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_set_hidden(s_menu, true);
    if (s_on_action) s_on_action(a);
}

static void meter_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_menu, true);
    lv_obj_set_hidden(s_meter, !lv_obj_is_hidden(s_meter));
    if (!lv_obj_is_hidden(s_meter) && s_have_snap) render(&s_snap);
}

static void settings_open_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_menu, true);
    lv_obj_set_hidden(s_settings, false);
    if (s_have_snap) render(&s_snap);
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
    pet_ui_toast(delta > 0 ? "clock +1h" : "clock -1h");
}

static void settings_bright_cb(lv_event_t *e)
{
    (void)e;
    static const int steps[] = {100, 60, 30};
    static int idx;
    idx = (idx + 1) % (int)(sizeof(steps) / sizeof(steps[0]));
    power_set_awake_brightness(steps[idx]);
    char msg[32];
    snprintf(msg, sizeof(msg), "brightness %d%%", steps[idx]);
    pet_ui_toast(msg);
}

static void settings_reset_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;  // hold to confirm
    game_reset();
    lv_obj_set_hidden(s_settings, true);
    pet_ui_toast("new egg");
}

/* ---------- small widget helpers ------------------------------------------- */

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static lv_obj_t *make_pill(lv_obj_t *parent, int w, int h, lv_color_t bg)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_color(box, bg, 0);
    lv_obj_set_style_radius(box, h / 2, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_scrollable(box, false);
    return box;
}

// A big bottom button: rounded tile, top icon, bottom caption.
static lv_obj_t *make_big_button(lv_obj_t *parent, int i)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, BIG_W, BIG_H);
    lv_obj_set_pos(btn, BIG_X0 + i * BIG_DX, SCR_H + BIG_BOTTOM - BIG_H);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
    apply_tile_style(btn);

    lv_obj_t *icon = make_label(btn, &lv_font_montserrat_24, lv_color_hex(BIG_COLOR[i]));
    lv_label_set_text(icon, BIG_ICON[i]);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *text = make_label(btn, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(text, BIG_TEXT[i]);
    lv_obj_align(text, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_add_event_cb(btn, action_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)BIG_ACT[i]);
    s_big[i] = btn;
    return btn;
}

// A compact text button for the settings overlay, positioned by hand.
static lv_obj_t *make_sbtn(lv_obj_t *parent, const char *text, int x, int y, int w,
                           lv_event_cb_t cb, void *user, bool hold)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, 40);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
    apply_tile_style(btn);
    lv_obj_set_style_radius(btn, UI_RADIUS, 0);

    lv_obj_t *label = make_label(btn, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, cb, hold ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED, user);
    return btn;
}

// A tile in the summoned menu grid.
static lv_obj_t *make_menu_tile(lv_obj_t *parent, int x, int y, const char *icon,
                                const char *text, uint32_t color, lv_event_cb_t cb,
                                void *user, bool hold)
{
    lv_obj_t *tile = lv_button_create(parent);
    lv_obj_set_size(tile, 76, 76);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_ext_click_area(tile, EXT_CLICK_PX);
    apply_tile_style(tile);

    if (icon) {
        lv_obj_t *ic = make_label(tile, &lv_font_montserrat_24, lv_color_hex(color));
        lv_label_set_text(ic, icon);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 6);
    }
    lv_obj_t *lb = make_label(tile, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_label_set_text(lb, text);
    lv_obj_align(lb, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_obj_add_event_cb(tile, cb, hold ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED, user);
    return tile;
}

/* ---------- builders: one per region of the screen ------------------------- */

static void build_status_bar(lv_obj_t *scr)
{
    s_status = make_label(scr, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_obj_set_pos(s_status, PAD_H, STATUS_Y);
    lv_label_set_text(s_status, "");

    s_clock = make_label(scr, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_obj_align(s_clock, LV_ALIGN_TOP_RIGHT, -PAD_H, STATUS_Y);
    lv_obj_set_style_text_align(s_clock, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_clock, "");
}

static void build_needs(lv_obj_t *scr)
{
    static const char *const names[2] = {"Food", "Fun"};
    for (int g = 0; g < 2; g++) {
        const int y = g == 0 ? NEEDS_ROW1_Y : NEEDS_ROW2_Y;
        lv_obj_t *lb = make_label(scr, &lv_font_montserrat_14, UI_TEXT_DIM);
        lv_label_set_text(lb, names[g]);
        lv_obj_set_pos(lb, NEEDS_LABEL_X, y + 6);
        for (int i = 0; i < HEARTS; i++) {
            lv_obj_t *h = lv_image_create(scr);
            lv_image_set_src(h, &icon_heart);
            lv_obj_set_pos(h, NEEDS_HEARTS_X + i * NEEDS_HEART_DX, y);
            s_hearts[g][i] = h;
        }
    }
}

static void build_face(lv_obj_t *scr)
{
    // a soft pedestal under the pet, so it stands on something instead of floating
    s_ground = lv_obj_create(scr);
    lv_obj_set_size(s_ground, 210, 28);
    lv_obj_set_style_radius(s_ground, 14, 0);
    lv_obj_set_style_bg_color(s_ground, UI_GROUND, 0);
    lv_obj_set_style_border_width(s_ground, 0, 0);
    lv_obj_set_scrollable(s_ground, false);
    lv_obj_align(s_ground, LV_ALIGN_CENTER, 0, FACE_Y + GROUND_DY);

    s_face = lv_image_create(scr);
    lv_obj_align(s_face, LV_ALIGN_CENTER, 0, FACE_Y);
    lv_obj_set_clickable(s_face, true);
    lv_obj_set_ext_click_area(s_face, EXT_CLICK_PX);
    lv_obj_add_event_cb(s_face, face_cb, LV_EVENT_CLICKED, NULL);
    show_face(&face_neutral);

    // poop: a brown blob by its feet (placeholder art)
    s_poop = lv_obj_create(scr);
    lv_obj_set_size(s_poop, 26, 20);
    lv_obj_set_style_radius(s_poop, 10, 0);
    lv_obj_set_style_bg_color(s_poop, lv_color_hex(0x6B4423), 0);
    lv_obj_set_style_border_width(s_poop, 0, 0);
    lv_obj_align_to(s_poop, s_face, LV_ALIGN_BOTTOM_RIGHT, 34, 6);
    lv_obj_set_hidden(s_poop, true);

    // the call badge, over the pet's shoulder; tap it to do the needed action
    s_badge = lv_button_create(scr);
    lv_obj_set_size(s_badge, 48, 48);
    lv_obj_set_style_radius(s_badge, 24, 0);
    lv_obj_set_style_border_width(s_badge, 2, 0);
    lv_obj_set_style_border_color(s_badge, UI_BG, 0);
    lv_obj_set_style_shadow_width(s_badge, 0, 0);
    lv_obj_set_ext_click_area(s_badge, EXT_CLICK_PX);
    lv_obj_align_to(s_badge, s_face, LV_ALIGN_TOP_RIGHT, BADGE_DX, BADGE_DY);
    s_badge_icon = make_label(s_badge, &lv_font_montserrat_24, UI_BG);
    lv_obj_center(s_badge_icon);
    lv_obj_add_event_cb(s_badge, badge_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_badge, true);
}

static void build_toast(lv_obj_t *scr)
{
    s_toast_box = make_pill(scr, 220, 36, UI_WARN);
    lv_obj_align(s_toast_box, LV_ALIGN_BOTTOM_MID, 0, TOAST_BOTTOM);
    s_toast = make_label(s_toast_box, &lv_font_montserrat_14, UI_ON_WARN);
    lv_obj_set_width(s_toast, 208);
    lv_obj_set_style_text_align(s_toast, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_toast);
    lv_obj_set_hidden(s_toast_box, true);
}

static void build_bottom_bar(lv_obj_t *scr)
{
    for (int i = 0; i < 3; i++) make_big_button(scr, i);

    // the gear summons the full icon menu (the original's summoned icon grid)
    lv_obj_t *gear = lv_button_create(scr);
    lv_obj_set_size(gear, GEAR_W, GEAR_W);
    lv_obj_align(gear, LV_ALIGN_BOTTOM_RIGHT, -PAD_H + 2, GEAR_BOTTOM);
    apply_tile_style(gear);
    lv_obj_set_style_radius(gear, GEAR_W / 2, 0);
    lv_obj_t *gi = make_label(gear, &lv_font_montserrat_24, UI_TEXT_DIM);
    lv_label_set_text(gi, LV_SYMBOL_SETTINGS);
    lv_obj_center(gi);
    lv_obj_add_event_cb(gear, menu_open_cb, LV_EVENT_CLICKED, NULL);
}

static void build_menu(lv_obj_t *scr)
{
    s_menu = lv_obj_create(scr);
    lv_obj_set_size(s_menu, 348, 210);
    lv_obj_align(s_menu, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(s_menu, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_menu, UI_BORDER, 0);
    lv_obj_set_style_border_width(s_menu, 1, 0);
    lv_obj_set_style_radius(s_menu, UI_RADIUS_LG, 0);
    lv_obj_set_scrollable(s_menu, false);

    const int x0 = 10, y0 = 12, dx = 82, dy = 94;
    make_menu_tile(s_menu, x0 + 0 * dx, y0, LV_SYMBOL_PLUS, "Feed", 0x9CC959,
                   menu_act_cb, (void *)(uintptr_t)PET_ACT_FEED_MEAL, false);
    make_menu_tile(s_menu, x0 + 1 * dx, y0, LV_SYMBOL_BELL, "Snack", 0x9CC959,
                   menu_act_cb, (void *)(uintptr_t)PET_ACT_FEED_SNACK, false);
    make_menu_tile(s_menu, x0 + 2 * dx, y0, LV_SYMBOL_EYE_OPEN, "Lights", 0xFFD166,
                   menu_act_cb, (void *)(uintptr_t)PET_ACT_LIGHTS, false);
    make_menu_tile(s_menu, x0 + 3 * dx, y0, LV_SYMBOL_TRASH, "Clean", 0x5DA9E9,
                   menu_act_cb, (void *)(uintptr_t)PET_ACT_CLEAN, false);

    make_menu_tile(s_menu, x0 + 0 * dx, y0 + dy, LV_SYMBOL_TINT, "Med", 0xFF5A5F,
                   menu_act_cb, (void *)(uintptr_t)PET_ACT_MEDICINE, false);
    make_menu_tile(s_menu, x0 + 1 * dx, y0 + dy, LV_SYMBOL_LIST, "Meter", 0x9AA9B8,
                   meter_cb, NULL, false);
    make_menu_tile(s_menu, x0 + 2 * dx, y0 + dy, LV_SYMBOL_SETTINGS, "Setup", 0x9AA9B8,
                   settings_open_cb, NULL, false);
    make_menu_tile(s_menu, x0 + 3 * dx, y0 + dy, LV_SYMBOL_CLOSE, "Close", 0x9AA9B8,
                   menu_close_cb, NULL, false);

    lv_obj_set_hidden(s_menu, true);
}

static void build_meter(lv_obj_t *scr)
{
    s_meter = lv_obj_create(scr);
    lv_obj_set_size(s_meter, 300, 200);
    lv_obj_align(s_meter, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(s_meter, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_meter, UI_BORDER, 0);
    lv_obj_set_style_border_width(s_meter, 1, 0);
    lv_obj_set_style_radius(s_meter, UI_RADIUS_LG, 0);
    lv_obj_set_scrollable(s_meter, false);
    lv_obj_add_event_cb(s_meter, meter_cb, LV_EVENT_CLICKED, NULL);  // tap to close

    s_meter_text = make_label(s_meter, &lv_font_montserrat_14, UI_TEXT);
    lv_obj_align(s_meter_text, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_hidden(s_meter, true);
}

static void build_settings(lv_obj_t *scr)
{
    s_settings = lv_obj_create(scr);
    lv_obj_set_size(s_settings, 330, 300);
    lv_obj_align(s_settings, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(s_settings, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_settings, UI_BORDER, 0);
    lv_obj_set_style_border_width(s_settings, 1, 0);
    lv_obj_set_style_radius(s_settings, UI_RADIUS_LG, 0);
    lv_obj_set_scrollable(s_settings, false);

    lv_obj_t *title = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_pos(title, 16, 8);

    lv_obj_t *time_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(time_lbl, "Time");
    lv_obj_set_pos(time_lbl, 16, 50);
    s_settings_time = make_label(s_settings, &lv_font_montserrat_14, UI_ACCENT);
    lv_obj_set_pos(s_settings_time, 16, 76);
    make_sbtn(s_settings, "-1h", 176, 46, 66, settings_time_cb, (void *)(intptr_t)-3600, false);
    make_sbtn(s_settings, "+1h", 248, 46, 66, settings_time_cb, (void *)(intptr_t)3600, false);

    lv_obj_t *br_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(br_lbl, "Brightness");
    lv_obj_set_pos(br_lbl, 16, 120);
    make_sbtn(s_settings, "cycle", 190, 114, 124, settings_bright_cb, NULL, false);

    lv_obj_t *reset_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(reset_lbl, "Reset pet");
    lv_obj_set_pos(reset_lbl, 16, 186);
    make_sbtn(s_settings, "hold", 190, 180, 124, settings_reset_cb, NULL, true);

    make_sbtn(s_settings, "CLOSE", 115, 240, 100, settings_close_cb, NULL, false);
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

    // Vendor defaults: partial-mode flush is the only path this BSP's SPI
    // pipeline supports well (a full-refresh experiment upstream produced a
    // blank white frame).
    bsp_display_start();
    bsp_display_lock(0);
    tile_styles_init();
    bsp_display_brightness_set(100);  // a panel command on the pixel bus: under the lock

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_BG, 0);  // AMOLED: black = pixels off
    lv_obj_set_scrollable(scr, false);

    build_status_bar(scr);
    build_needs(scr);
    build_face(scr);
    build_toast(scr);
    build_bottom_bar(scr);
    build_menu(scr);
    build_meter(scr);
    build_settings(scr);
    register_touch_log();

    lv_timer_create(blink_cb, BLINK_PERIOD_MS, NULL);
    lv_timer_create(pulse_cb, PULSE_MS, NULL);
    lv_timer_create(repaint_cb, REPAINT_MS, NULL);  // self-heal against dropped SPI chunks

    bsp_display_unlock();
}
