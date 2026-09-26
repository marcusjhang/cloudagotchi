/*
 * The screen (368 x 448). Renders whatever snapshot the game last pushed; turns
 * touches into pet_action_t via the callback. Knows nothing about the rules.
 *
 * Reading order:
 *   layout constants -> styles -> face/caption -> render -> input -> settings
 *   -> build_* helpers -> pet_ui_start (which just calls the builders)
 *
 *   status strip   "teen · 3d 4h"  (left)        "14:07 87%" (right)
 *   stat row       FOOD FUN ZZZ CLEAN HP   (five bars)
 *   face           one image widget, swapped per state (+ poop blob)
 *   caption        "hungry" / "zzz" / "nom nom" ...
 *   toast          "not hungry" for blocked actions
 *   action bar     FEED PLAY LIGHT CLEAN MED INFO  (icon + caption)
 *   overlays       INFO (tap) and SETTINGS (hold INFO)
 *
 * Touch on this glass lands 15-25 px below where you aim (field guide), so every
 * target gets an extended click area (EXT_CLICK_PX) and nothing is under 40 px
 * tall. Every touch is logged at INFO - that log is how those numbers were found.
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
#define PAD_H            16                  // left/right margin
#define STATUS_Y         10
#define STAT_COL_W       60
#define STAT_COL_STEP    68                  // column width + gap
#define STAT_LABEL_Y     34
#define STAT_BAR_Y       56
#define STAT_BAR_H       10
#define FACE_Y           (-14)
#define CAPTION_Y        88
#define TOAST_BOTTOM     (-96)
#define BTN_ROW_X        7
#define BTN_ROW_STEP     60
#define BTN_ROW_BOTTOM   (-12)
#define OVERLAY_Y        (-20)
#define INFO_W           300
#define INFO_H           250
#define SET_W            330
#define SET_H            300
#define SBTN_H           44

/* timers */
#define BLINK_PERIOD_MS  4200
#define BLINK_MS         120
#define TOAST_MS         1500
#define REPAINT_MS       15000
#define EXT_CLICK_PX     20

static pet_ui_action_cb_t s_on_action;

static lv_obj_t *s_status;
static lv_obj_t *s_clock;
static lv_obj_t *s_bars[PET_STAT_COUNT];
static lv_obj_t *s_ground;
static lv_obj_t *s_face;
static lv_obj_t *s_poop;
static lv_obj_t *s_caption_box;
static lv_obj_t *s_caption;
static lv_obj_t *s_toast_box;
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
    [PET_STAT_FULLNESS]  = {"Food",  0x9CC959},
    [PET_STAT_HAPPINESS] = {"Fun",   0xFF6392},
    [PET_STAT_ENERGY]    = {"Nap",   0x2EC4B6},
    [PET_STAT_HYGIENE]   = {"Clean", 0x5DA9E9},
    [PET_STAT_HEALTH]    = {"HP",    0xFF5A5F},
};

/* Action bar: 0..4 are pet actions, 5 is INFO. Icon + caption + icon colour. */
static const struct {
    const char *icon;
    const char *text;
    pet_action_t act;
    uint32_t color;
} BTN[6] = {
    {LV_SYMBOL_PLUS,     "Feed",  PET_ACT_FEED_MEAL, 0x9CC959},
    {LV_SYMBOL_PLAY,     "Play",  PET_ACT_PLAY,      0xFF6392},
    {LV_SYMBOL_EYE_OPEN, "Light", PET_ACT_LIGHTS,    0xFFD166},
    {LV_SYMBOL_TRASH,    "Clean", PET_ACT_CLEAN,     0x5DA9E9},
    {LV_SYMBOL_TINT,     "Med",   PET_ACT_MEDICINE,  0xFF5A5F},
    {LV_SYMBOL_LIST,     "Info",  PET_ACT_COUNT,     0x9AA9B8},  // count = not an action
};

static lv_obj_t *s_btn[6];
static lv_obj_t *s_btn_icon[6];

/* ---------- button styles (created once) ---------------------------------- */

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

    lv_style_init(&st_btn_on);  // checked: lit border (e.g. LIGHT while asleep)
    lv_style_set_border_color(&st_btn_on, UI_ACCENT);
    lv_style_set_bg_color(&st_btn_on, lv_color_mix(UI_ACCENT, UI_SURFACE, 40));
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
    lv_obj_set_hidden(s_toast_box, true);
    s_toast_timer = NULL;
}

/* ---------- rendering a snapshot ------------------------------------------- */

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

    char clock[32];
    fmt_clock(clock, sizeof(clock), s);
    lv_label_set_text(s_clock, clock);

    // Action bar: grey out what the pet would refuse; LIGHT lit while asleep.
    for (int i = 0; i < 5; i++) {
        lv_obj_set_state(s_btn[i], LV_STATE_DISABLED, !s->enabled[BTN[i].act]);
    }
    lv_label_set_text(s_btn_icon[2], s->asleep ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    lv_obj_set_state(s_btn[2], LV_STATE_CHECKED, s->asleep);

    for (int i = 0; i < PET_STAT_COUNT; i++) {
        lv_bar_set_value(s_bars[i], s->stats[i], LV_ANIM_OFF);
    }

    show_face(face_for(s->face, false));
    lv_obj_set_hidden(s_poop, !s->dirty);
    const char *cap = caption_for(s);
    lv_label_set_text(s_caption, cap);
    lv_obj_set_hidden(s_caption_box, cap[0] == '\0');

    if (!lv_obj_is_hidden(s_info)) {
        char power[48];
        power_line(s, power, sizeof(power));  // keep the bound on power for -Wformat-truncation
        char info[224];
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

/* ---------- small widget helpers ------------------------------------------- */

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

// A compact secondary button for the overlays, positioned by hand.
static lv_obj_t *make_sbtn(lv_obj_t *parent, const char *text, int x, int y, int w)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, SBTN_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_style(btn, &st_btn_base, 0);
    lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);

    lv_obj_t *label = make_label(btn, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
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

static void build_stat_row(lv_obj_t *scr)
{
    for (int i = 0; i < PET_STAT_COUNT; i++) {
        const int x = PAD_H + i * STAT_COL_STEP;

        lv_obj_t *label = make_label(scr, &lv_font_montserrat_14,
                                     lv_color_hex(STAT_STYLE[i].color));
        lv_label_set_text(label, STAT_STYLE[i].label);
        lv_obj_set_width(label, STAT_COL_W);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x, STAT_LABEL_Y);

        lv_obj_t *bar = lv_bar_create(scr);
        lv_obj_set_size(bar, STAT_COL_W, UI_BAR_H);
        lv_obj_set_pos(bar, x, STAT_BAR_Y);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_set_style_bg_color(bar, UI_TRACK, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(STAT_STYLE[i].color), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, UI_BAR_H / 2, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, UI_BAR_H / 2, LV_PART_INDICATOR);
        s_bars[i] = bar;
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
    lv_obj_align(s_ground, LV_ALIGN_CENTER, 0, FACE_Y + 70);

    s_face = lv_image_create(scr);
    lv_obj_align(s_face, LV_ALIGN_CENTER, 0, FACE_Y);
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

static void build_caption_and_toast(lv_obj_t *scr)
{
    // caption as a soft speech bubble under the pet
    s_caption_box = make_pill(scr, 280, 38, UI_BUBBLE);
    lv_obj_set_style_radius(s_caption_box, 14, 0);
    lv_obj_align(s_caption_box, LV_ALIGN_CENTER, 0, CAPTION_Y);
    s_caption = make_label(s_caption_box, &lv_font_montserrat_18, UI_TEXT);
    lv_obj_set_width(s_caption, 264);
    lv_obj_set_style_text_align(s_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_caption);
    lv_label_set_text(s_caption, "");
    lv_obj_set_hidden(s_caption_box, true);

    // toast as a pill; dark ink on the warn colour so it stays readable
    s_toast_box = make_pill(scr, 220, 36, UI_WARN);
    lv_obj_align(s_toast_box, LV_ALIGN_BOTTOM_MID, 0, TOAST_BOTTOM);
    s_toast = make_label(s_toast_box, &lv_font_montserrat_14, UI_ON_WARN);
    lv_obj_set_width(s_toast, 208);
    lv_obj_set_style_text_align(s_toast, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_toast);
    lv_obj_set_hidden(s_toast_box, true);
}

static lv_obj_t *make_action_button(lv_obj_t *parent, int i)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, UI_BTN_W, UI_BTN_H);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, BTN_ROW_X + i * BTN_ROW_STEP, BTN_ROW_BOTTOM);
    lv_obj_set_ext_click_area(btn, EXT_CLICK_PX);
    lv_obj_add_style(btn, &st_btn_base, 0);
    lv_obj_add_style(btn, &st_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &st_btn_off, LV_STATE_DISABLED);
    lv_obj_add_style(btn, &st_btn_on, LV_STATE_CHECKED);
    lv_obj_set_style_radius(btn, UI_RADIUS_LG, 0);  // rounder than the overlay buttons

    lv_obj_t *icon = make_label(btn, &lv_font_montserrat_24, lv_color_hex(BTN[i].color));
    lv_label_set_text(icon, BTN[i].icon);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *text = make_label(btn, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_label_set_text(text, BTN[i].text);
    lv_obj_align(text, LV_ALIGN_BOTTOM_MID, 0, -2);

    s_btn[i] = btn;
    s_btn_icon[i] = icon;
    return btn;
}

static void build_action_bar(lv_obj_t *scr)
{
    for (int i = 0; i < 5; i++) {
        lv_obj_t *btn = make_action_button(scr, i);
        if (BTN[i].act == PET_ACT_FEED_MEAL) {
            lv_obj_add_event_cb(btn, feed_cb, LV_EVENT_SHORT_CLICKED, NULL);
            lv_obj_add_event_cb(btn, feed_cb, LV_EVENT_LONG_PRESSED, NULL);   // snack
        } else {
            lv_obj_add_event_cb(btn, action_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)BTN[i].act);
        }
    }
    lv_obj_t *info_btn = make_action_button(scr, 5);
    lv_obj_add_event_cb(info_btn, info_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(info_btn, settings_open_cb, LV_EVENT_LONG_PRESSED, NULL);  // hold: settings
}

static void build_info_overlay(lv_obj_t *scr)
{
    s_info = lv_obj_create(scr);
    lv_obj_set_size(s_info, INFO_W, INFO_H);
    lv_obj_align(s_info, LV_ALIGN_CENTER, 0, OVERLAY_Y);
    lv_obj_set_style_bg_color(s_info, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_info, UI_BORDER, 0);
    lv_obj_set_style_border_width(s_info, 1, 0);
    lv_obj_set_style_radius(s_info, UI_RADIUS, 0);
    lv_obj_set_scrollable(s_info, false);
    lv_obj_add_event_cb(s_info, info_cb, LV_EVENT_CLICKED, NULL);  // tap to close

    s_info_text = make_label(s_info, &lv_font_montserrat_14, UI_TEXT);
    lv_obj_align(s_info_text, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_hidden(s_info, true);
}

static void build_settings_overlay(lv_obj_t *scr)
{
    s_settings = lv_obj_create(scr);
    lv_obj_set_size(s_settings, SET_W, SET_H);
    lv_obj_align(s_settings, LV_ALIGN_CENTER, 0, OVERLAY_Y);
    lv_obj_set_style_bg_color(s_settings, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_settings, UI_BORDER, 0);
    lv_obj_set_style_border_width(s_settings, 1, 0);
    lv_obj_set_style_radius(s_settings, UI_RADIUS, 0);
    lv_obj_set_scrollable(s_settings, false);

    lv_obj_t *title = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_pos(title, 16, 8);

    // time row
    lv_obj_t *time_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(time_lbl, "Time");
    lv_obj_set_pos(time_lbl, 16, 50);
    s_settings_time = make_label(s_settings, &lv_font_montserrat_14, UI_ACCENT);
    lv_obj_set_pos(s_settings_time, 16, 76);
    lv_obj_add_event_cb(make_sbtn(s_settings, "-1h", 176, 46, 66), settings_time_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)-3600);
    lv_obj_add_event_cb(make_sbtn(s_settings, "+1h", 248, 46, 66), settings_time_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)3600);

    // brightness row
    lv_obj_t *br_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(br_lbl, "Brightness");
    lv_obj_set_pos(br_lbl, 16, 120);
    lv_obj_add_event_cb(make_sbtn(s_settings, "cycle", 190, 114, 124), settings_bright_cb,
                        LV_EVENT_CLICKED, NULL);

    // reset row (hold to confirm)
    lv_obj_t *reset_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(reset_lbl, "Reset pet");
    lv_obj_set_pos(reset_lbl, 16, 186);
    lv_obj_add_event_cb(make_sbtn(s_settings, "hold", 190, 180, 124), settings_reset_cb,
                        LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_add_event_cb(make_sbtn(s_settings, "CLOSE", 115, 240, 100), settings_close_cb,
                        LV_EVENT_CLICKED, NULL);
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
    btn_styles_init();
    bsp_display_brightness_set(100);  // a panel command on the pixel bus: under the lock

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_BG, 0);  // AMOLED: black = pixels off
    lv_obj_set_scrollable(scr, false);

    build_status_bar(scr);
    build_stat_row(scr);
    build_face(scr);
    build_caption_and_toast(scr);
    build_action_bar(scr);
    build_info_overlay(scr);
    build_settings_overlay(scr);
    register_touch_log();

    lv_timer_create(blink_cb, BLINK_PERIOD_MS, NULL);
    lv_timer_create(repaint_cb, REPAINT_MS, NULL);  // self-heal against dropped SPI chunks

    bsp_display_unlock();
}
