/*
 * Layout (368 x 448):
 *
 *   status strip     "baby · 2h 13m"                       (later: battery)
 *   stat row         FOOD  FUN  ZZZ  CLEAN  HP   five bars
 *   face             one image widget, swapped per state (+ poop blob)
 *   caption          "hungry" / "zzz" / "nom nom" ...
 *   toast            "not hungry" for blocked actions
 *   action bar       FEED  PLAY  LIGHT  CLEAN  MED  INFO   (long-press FEED = snack)
 *
 * Touch on this glass lands 15-25 px below where you aim (field guide), so
 * every target gets an extended click area and nothing is under 40 px tall.
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

static const char *TAG = "pet_ui";

#define BLINK_PERIOD_MS 4200
#define BLINK_MS        120
#define TOAST_MS        1500
#define REPAINT_MS      15000
#define EXT_CLICK_PX    20

static pet_ui_action_cb_t s_on_action;

static lv_obj_t *s_status;
static lv_obj_t *s_clock;
static lv_obj_t *s_bars[PET_STAT_COUNT];
static lv_obj_t *s_face;
static lv_obj_t *s_poop;
static lv_obj_t *s_caption;
static lv_obj_t *s_toast;
static lv_obj_t *s_info;
static lv_obj_t *s_info_text;
static lv_obj_t *s_settings;
static lv_obj_t *s_settings_time;
static lv_timer_t *s_toast_timer;

static pet_ui_snapshot_t s_snap;
static bool s_have_snap;
static const lv_image_dsc_t *s_shown;

static const struct {
    const char *label;
    uint32_t color;
} STAT_STYLE[PET_STAT_COUNT] = {
    [PET_STAT_FULLNESS]  = {"FOOD",  0x9CC959},
    [PET_STAT_HAPPINESS] = {"FUN",   0xFF6392},
    [PET_STAT_ENERGY]    = {"ZZZ",   0x2EC4B6},
    [PET_STAT_HYGIENE]   = {"CLEAN", 0x5DA9E9},
    [PET_STAT_HEALTH]    = {"HP",    0xFF5A5F},
};

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

static const char *caption_for(const pet_ui_snapshot_t *s)
{
    switch (s->face) {
    case PET_FACE_EGG:      return "an egg. wait for it";
    case PET_FACE_EATING:   return "nom nom";
    case PET_FACE_PLAYING:  return "wheee";
    case PET_FACE_STARTLED: return "!!";
    case PET_FACE_SLEEPING: return "zzz";
    case PET_FACE_SICK:     return "sick - needs MED";
    case PET_FACE_DIRTY:    return "clean me";
    case PET_FACE_DEAD:     return "R.I.P.  (hold face: new egg)";
    case PET_FACE_SAD: {
        // Name the lowest core need so the owner knows which button to press.
        pet_stat_t worst = PET_STAT_FULLNESS;
        for (int i = 0; i < PET_STAT_HEALTH; i++) {
            if (i == PET_STAT_ENERGY) continue;
            if (s->stats[i] < s->stats[worst]) worst = (pet_stat_t)i;
        }
        switch (worst) {
        case PET_STAT_HAPPINESS: return "bored";
        case PET_STAT_HYGIENE:   return "stinky";
        default:                 return "hungry";
        }
    }
    case PET_FACE_HAPPY:    return "";
    default:                return "";
    }
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

static void repaint_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_invalidate(lv_screen_active());
}

static void toast_hide_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_set_hidden(s_toast, true);
    s_toast_timer = NULL;
}

/* ---------- rendering a snapshot ------------------------------------------- */

static void fmt_age(char *buf, size_t n, uint32_t s)
{
    const uint32_t d = s / 86400, h = (s / 3600) % 24, m = (s / 60) % 60;
    if (d) snprintf(buf, n, "%lud %luh", (unsigned long)d, (unsigned long)h);
    else   snprintf(buf, n, "%luh %02lum", (unsigned long)h, (unsigned long)m);
}

// Must hold the display lock.
static void render_settings_clock(void)
{
    if (!s_settings || lv_obj_is_hidden(s_settings)) return;
    if (!s_have_snap || !s_snap.clock_ok) {
        lv_label_set_text(s_settings_time, "--:--");
        return;
    }
    struct tm lt;
    const time_t t = (time_t)s_snap.clock_s;
    localtime_r(&t, &lt);
    char hm[8];
    strftime(hm, sizeof(hm), "%H:%M", &lt);
    lv_label_set_text(s_settings_time, hm);
}

// Must hold the display lock.
static void render(const pet_ui_snapshot_t *s)
{
    char age[24], line[64];
    fmt_age(age, sizeof(age), s->age_s);
    snprintf(line, sizeof(line), "%s  ·  %s", pet_stage_name(s->stage), age);
    lv_label_set_text(s_status, line);

    // clock + battery, top right. "USB" when there is no cell to report.
    char clock[32] = "";
    if (s->clock_ok) {
        struct tm lt;
        const time_t t = (time_t)s->clock_s;
        localtime_r(&t, &lt);
        char hm[8];
        strftime(hm, sizeof(hm), "%H:%M", &lt);
        if (s->batt_ok && s->batt.present) {
            snprintf(clock, sizeof(clock), "%s  %d%%", hm, s->batt.percent);
        } else if (s->batt_ok && s->batt.vbus) {
            snprintf(clock, sizeof(clock), "%s  USB", hm);
        } else {
            snprintf(clock, sizeof(clock), "%s", hm);
        }
    } else if (s->batt_ok && s->batt.present) {
        snprintf(clock, sizeof(clock), "%d%%", s->batt.percent);
    }
    lv_label_set_text(s_clock, clock);

    for (int i = 0; i < PET_STAT_COUNT; i++) {
        lv_bar_set_value(s_bars[i], s->stats[i], LV_ANIM_OFF);
    }

    show_face(face_for(s->face, false));
    lv_obj_set_hidden(s_poop, !s->dirty);
    lv_label_set_text(s_caption, caption_for(s));

    if (!lv_obj_is_hidden(s_info)) {
        char power[48] = "unknown";
        if (s->batt_ok) {
            if (s->batt.present) {
                snprintf(power, sizeof(power), "%d%%  %d mV%s", s->batt.percent,
                         s->batt.millivolts, s->batt.charging ? "  charging" : "");
            } else {
                snprintf(power, sizeof(power), "USB power, no battery");
            }
        }
        char info[192];
        snprintf(info, sizeof(info),
                 "stage      %s\nage        %s\nweight     %u\nmistakes   %u\n"
                 "power      %s\nboots      %lu\n\n"
                 "hold FEED for a snack\nhold the face when dead",
                 pet_stage_name(s->stage), age, s->weight, s->mistakes,
                 power, (unsigned long)s->boots);
        lv_label_set_text(s_info_text, info);
    }
    render_settings_clock();
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
    lv_obj_set_hidden(s_toast, false);
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

static void feed_cb(lv_event_t *e)
{
    // Short tap = meal, long press = snack. LVGL sends LONG_PRESSED at
    // long_press_time and SHORT_CLICKED only if released before it.
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED)     s_on_action(PET_ACT_FEED_MEAL);
    else if (code == LV_EVENT_LONG_PRESSED) s_on_action(PET_ACT_FEED_SNACK);
}

static void face_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) s_on_action(PET_ACT_NEW_EGG);
}

static void info_cb(lv_event_t *e)
{
    (void)e;
    const bool hidden = lv_obj_is_hidden(s_info);
    lv_obj_set_hidden(s_info, !hidden);
    if (hidden && s_have_snap) render(&s_snap);
}

/* ---------- settings (long-press INFO) ------------------------------------ */

static const int BRIGHT_STEPS[] = {100, 60, 30};
static int s_bright_idx;

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
    s_bright_idx = (s_bright_idx + 1) % (int)(sizeof(BRIGHT_STEPS) / sizeof(BRIGHT_STEPS[0]));
    power_set_awake_brightness(BRIGHT_STEPS[s_bright_idx]);
    char msg[32];
    snprintf(msg, sizeof(msg), "brightness %d%%", BRIGHT_STEPS[s_bright_idx]);
    pet_ui_toast(msg);
}

static void settings_reset_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;  // hold to confirm
    game_reset();
    lv_obj_set_hidden(s_settings, true);
    pet_ui_toast("new egg");
}

static void settings_open_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_settings, false);
    if (s_have_snap) render(&s_snap);
}

/* ---------- building the screen ---------------------------------------------- */

static lv_obj_t *make_bar(lv_obj_t *parent, int i, int x)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, STAT_STYLE[i].label);
    lv_obj_set_style_text_color(label, lv_color_hex(STAT_STYLE[i].color), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, x, 34);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 60, 10);
    lv_obj_set_pos(bar, x, 56);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A2530), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(STAT_STYLE[i].color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
    return bar;
}

static lv_obj_t *make_button(lv_obj_t *parent, int i, const char *text)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 54, 56);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 7 + i * 60, -12);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2530), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE8EEF4), 0);
    lv_obj_center(label);
    return btn;
}

// A smaller button for the settings overlay, positioned by hand.
static lv_obj_t *make_sbtn(lv_obj_t *parent, const char *text, int x, int y, int w)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, 44);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2530), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE8EEF4), 0);
    lv_obj_center(label);
    return btn;
}

void pet_ui_start(pet_ui_action_cb_t on_action)
{
    s_on_action = on_action;

    // Vendor defaults: partial-mode flush is the only path this BSP's SPI
    // pipeline supports well (a full-refresh experiment upstream produced a
    // blank white frame).
    bsp_display_start();
    bsp_display_lock(0);
    bsp_display_brightness_set(100);  // a panel command on the pixel bus: under the lock

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);  // AMOLED: black = pixels off
    lv_obj_set_scrollable(scr, false);

    // status strip
    s_status = lv_label_create(scr);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(0x8A9BB0), 0);
    lv_obj_set_pos(s_status, 16, 10);
    lv_label_set_text(s_status, "");

    // status strip, right: clock and battery
    s_clock = lv_label_create(scr);
    lv_obj_set_style_text_font(s_clock, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_clock, lv_color_hex(0x8A9BB0), 0);
    lv_obj_align(s_clock, LV_ALIGN_TOP_RIGHT, -16, 10);
    lv_obj_set_style_text_align(s_clock, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_clock, "");

    // stat row: 5 columns of 64 px, 4 px gaps, centred
    for (int i = 0; i < PET_STAT_COUNT; i++) {
        s_bars[i] = make_bar(scr, i, 16 + i * 68);
    }

    // the face
    s_face = lv_image_create(scr);
    lv_obj_align(s_face, LV_ALIGN_CENTER, 0, -14);
    lv_obj_set_clickable(s_face, true);
    lv_obj_set_ext_click_area(s_face, EXT_CLICK_PX);
    lv_obj_add_event_cb(s_face, face_cb, LV_EVENT_LONG_PRESSED, NULL);
    show_face(&face_neutral);

    // poop: a brown blob by its feet (placeholder art)
    s_poop = lv_obj_create(scr);
    lv_obj_set_size(s_poop, 26, 20);
    lv_obj_set_style_radius(s_poop, 10, 0);
    lv_obj_set_style_bg_color(s_poop, lv_color_hex(0x6B4423), 0);
    lv_obj_set_style_border_width(s_poop, 0, 0);
    lv_obj_align_to(s_poop, s_face, LV_ALIGN_BOTTOM_RIGHT, 34, 6);
    lv_obj_set_hidden(s_poop, true);

    // caption under the face
    s_caption = lv_label_create(scr);
    lv_obj_set_style_text_font(s_caption, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_caption, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(s_caption, 340);
    lv_obj_set_style_text_align(s_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_caption, LV_ALIGN_CENTER, 0, 96);
    lv_label_set_text(s_caption, "");

    // toast for blocked actions
    s_toast = lv_label_create(scr);
    lv_obj_set_style_text_font(s_toast, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_toast, lv_color_hex(0xFFD166), 0);
    lv_obj_set_width(s_toast, 340);
    lv_obj_set_style_text_align(s_toast, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_set_hidden(s_toast, true);

    // action bar
    static const struct { const char *text; pet_action_t act; } BTN[] = {
        {"FEED", PET_ACT_FEED_MEAL}, {"PLAY", PET_ACT_PLAY}, {"LIGHT", PET_ACT_LIGHTS},
        {"CLEAN", PET_ACT_CLEAN},    {"MED", PET_ACT_MEDICINE},
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *btn = make_button(scr, i, BTN[i].text);
        if (BTN[i].act == PET_ACT_FEED_MEAL) {
            lv_obj_add_event_cb(btn, feed_cb, LV_EVENT_SHORT_CLICKED, NULL);
            lv_obj_add_event_cb(btn, feed_cb, LV_EVENT_LONG_PRESSED, NULL);
        } else {
            lv_obj_add_event_cb(btn, action_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)BTN[i].act);
        }
    }
    lv_obj_t *info_btn = make_button(scr, 5, "INFO");
    lv_obj_add_event_cb(info_btn, info_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(info_btn, settings_open_cb, LV_EVENT_LONG_PRESSED, NULL);  // hold: settings

    // info overlay (INFO toggles; tap it to close)
    s_info = lv_obj_create(scr);
    lv_obj_set_size(s_info, 300, 250);
    lv_obj_align(s_info, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(s_info, lv_color_hex(0x0B1420), 0);
    lv_obj_set_style_border_color(s_info, lv_color_hex(0x2F9BFF), 0);
    lv_obj_set_style_border_width(s_info, 1, 0);
    lv_obj_set_style_radius(s_info, 12, 0);
    lv_obj_set_scrollable(s_info, false);
    lv_obj_add_event_cb(s_info, info_cb, LV_EVENT_CLICKED, NULL);
    s_info_text = lv_label_create(s_info);
    lv_obj_set_style_text_font(s_info_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_info_text, lv_color_hex(0xE8EEF4), 0);
    lv_obj_align(s_info_text, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_hidden(s_info, true);

    // settings overlay (long-press INFO)
    s_settings = lv_obj_create(scr);
    lv_obj_set_size(s_settings, 330, 300);
    lv_obj_align(s_settings, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(s_settings, lv_color_hex(0x0B1420), 0);
    lv_obj_set_style_border_color(s_settings, lv_color_hex(0x2F9BFF), 0);
    lv_obj_set_style_border_width(s_settings, 1, 0);
    lv_obj_set_style_radius(s_settings, 12, 0);
    lv_obj_set_scrollable(s_settings, false);

    lv_obj_t *st_title = lv_label_create(s_settings);
    lv_label_set_text(st_title, "SETTINGS");
    lv_obj_set_style_text_font(st_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(st_title, lv_color_hex(0x8A9BB0), 0);
    lv_obj_set_pos(st_title, 16, 8);

    lv_obj_t *st_time = lv_label_create(s_settings);
    lv_label_set_text(st_time, "Time");
    lv_obj_set_style_text_font(st_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(st_time, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_pos(st_time, 16, 50);
    s_settings_time = lv_label_create(s_settings);
    lv_obj_set_style_text_font(s_settings_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_settings_time, lv_color_hex(0x2EC4B6), 0);
    lv_obj_set_pos(s_settings_time, 16, 76);
    lv_obj_add_event_cb(make_sbtn(s_settings, "-1h", 176, 46, 66), settings_time_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)-3600);
    lv_obj_add_event_cb(make_sbtn(s_settings, "+1h", 248, 46, 66), settings_time_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)3600);

    lv_obj_t *st_br = lv_label_create(s_settings);
    lv_label_set_text(st_br, "Brightness");
    lv_obj_set_style_text_font(st_br, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(st_br, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_pos(st_br, 16, 120);
    lv_obj_add_event_cb(make_sbtn(s_settings, "cycle", 190, 114, 124), settings_bright_cb,
                        LV_EVENT_CLICKED, NULL);

    lv_obj_t *st_reset = lv_label_create(s_settings);
    lv_label_set_text(st_reset, "Reset pet");
    lv_obj_set_style_text_font(st_reset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(st_reset, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_pos(st_reset, 16, 186);
    lv_obj_add_event_cb(make_sbtn(s_settings, "hold", 190, 180, 124), settings_reset_cb,
                        LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_add_event_cb(make_sbtn(s_settings, "CLOSE", 115, 240, 100), settings_close_cb,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_settings, true);

    // touch log: every press/release with coordinates, permanently
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_indev_add_event_cb(indev, touch_log_cb, LV_EVENT_PRESSED, NULL);
        lv_indev_add_event_cb(indev, touch_log_cb, LV_EVENT_RELEASED, NULL);
    }

    lv_timer_create(blink_cb, BLINK_PERIOD_MS, NULL);
    lv_timer_create(repaint_cb, REPAINT_MS, NULL);  // self-heal against dropped SPI chunks

    bsp_display_unlock();
}
