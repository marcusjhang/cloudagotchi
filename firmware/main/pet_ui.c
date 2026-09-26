/*
 * The screen (368 x 448). Renders whatever snapshot the game last pushed; turns
 * touches into pet_action_t via the callback. Knows nothing about the rules.
 *
 * Icon-only, child-first screen (see the `lvgl-ui` skill, references/screens.md):
 * no words at rest. The pet is the hero; two rows of hearts show Food and Fun;
 * a single pulsing call shows what it needs (tap it to fix); four big candy tiles
 * are the only controls. Settings hide behind a long-press on the pet.
 *
 *      Food   o o o o        <- warm hearts
 *      Fun    o o o o        <- pink hearts
 *             ( pet )        <- hero, on a soft pedestal, call badge over its shoulder
 *      [feed] [play] [clean] [med]   <- four big tiles, white icons on candy colour
 *
 * Touch lands 15-25 px below where you aim (field guide): every target gets an
 * extended click area. Every touch is logged at INFO - that log is how the
 * numbers were found.
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

#define NEED_ICON_X      22
#define NEED_BAR_X       56
#define NEED_BAR_W       250
#define NEED_BAR_H       16
#define NEED_ROW1_Y      14
#define NEED_ROW2_Y      44

#define FACE_Y           (-13)
#define GROUND_DY        70
#define BADGE_D          56
#define BADGE_DX         8
#define BADGE_DY         (-6)

#define TILE_D           80
#define TILE_X0          8
#define TILE_DX          90
#define TILE_BOTTOM      (-20)

#define EXT_CLICK_PX     20

/* timers */
#define BLINK_PERIOD_MS  4200
#define BLINK_MS         120
#define REPAINT_MS       15000
#define PULSE_MS         600

static pet_ui_action_cb_t s_on_action;

static lv_obj_t *s_need_bar[2];             // 0 = food, 1 = fun
static lv_obj_t *s_ground;
static lv_obj_t *s_face;
static lv_obj_t *s_poop;
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
static const uint32_t TILE_BG[4] = {0xE8A15C, 0xE88AB0, 0x5AA9E6, 0x67C57A};

/* ---------- styles --------------------------------------------------------- */

static lv_style_t st_tile, st_tile_pressed;

static void tile_styles_init(void)
{
    lv_style_init(&st_tile);
    lv_style_set_radius(&st_tile, UI_RADIUS_LG);
    lv_style_set_border_width(&st_tile, 0);
    lv_style_set_shadow_width(&st_tile, 0);
    lv_style_set_pad_all(&st_tile, 0);

    lv_style_init(&st_tile_pressed);
    lv_style_set_translate_y(&st_tile_pressed, 2);
}

static void apply_tile(lv_obj_t *obj, lv_color_t bg)
{
    lv_obj_add_style(obj, &st_tile, 0);
    lv_obj_add_style(obj, &st_tile_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(obj, LV_OPA_30, LV_STATE_DISABLED);
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
    lv_obj_set_style_opa(s_badge, o > LV_OPA_70 ? LV_OPA_50 : LV_OPA_COVER, 0);
}

static void repaint_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_invalidate(lv_screen_active());
}

/* ---------- the call badge ------------------------------------------------- */

static void badge_look_for(pet_need_t n, const lv_image_dsc_t **icon, uint32_t *color)
{
    switch (n) {
    case PET_NEED_MED:   *icon = &ic_med;   *color = 0x5BD66F; break;
    case PET_NEED_CLEAN: *icon = &ic_clean; *color = 0x45B7F0; break;
    case PET_NEED_FOOD:  *icon = &ic_feed;  *color = 0xFF9F45; break;
    case PET_NEED_FUN:   *icon = &ic_play;  *color = 0xFF6FA5; break;
    default:             *icon = &ic_happy; *color = 0x9A7BFF; break;
    }
}

static void badge_cb(lv_event_t *e)
{
    (void)e;
    if (!s_have_snap) return;
    switch (s_snap.need) {  // tap the call to do what it asks for
    case PET_NEED_MED:   s_on_action(PET_ACT_MEDICINE);  break;
    case PET_NEED_CLEAN: s_on_action(PET_ACT_CLEAN);     break;
    case PET_NEED_FOOD:  s_on_action(PET_ACT_FEED_MEAL); break;
    case PET_NEED_FUN:   s_on_action(PET_ACT_PLAY);      break;
    default: break;
    }
}

/* ---------- render --------------------------------------------------------- */

static void render_needs(const pet_ui_snapshot_t *s)
{
    lv_bar_set_value(s_need_bar[0], s->stats[PET_STAT_FULLNESS], LV_ANIM_OFF);
    lv_bar_set_value(s_need_bar[1], s->stats[PET_STAT_HAPPINESS], LV_ANIM_OFF);
}

static void render_call(const pet_ui_snapshot_t *s)
{
    const bool on = s->need != PET_NEED_NONE && s->stage != PET_STAGE_DEAD;
    s_badge_on = on;
    lv_obj_set_hidden(s_badge, !on);
    if (!on) return;
    const lv_image_dsc_t *icon;
    uint32_t color;
    badge_look_for(s->need, &icon, &color);
    lv_image_set_src(s_badge_icon, icon);
    lv_obj_set_style_bg_color(s_badge, lv_color_hex(color), 0);
    lv_obj_set_style_opa(s_badge, LV_OPA_COVER, 0);
}


static void render_tiles(const pet_ui_snapshot_t *s)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_set_state(s_tile[i], LV_STATE_DISABLED, !s->enabled[TILE_ACT[i]]);
        // the tile the call points at gets a bright ring
        const bool hot = (s->need == PET_NEED_FOOD && i == 0) ||
                         (s->need == PET_NEED_FUN && i == 1) ||
                         (s->need == PET_NEED_CLEAN && i == 2) ||
                         (s->need == PET_NEED_MED && i == 3);
        lv_obj_set_style_border_width(s_tile[i], hot ? 3 : 0, 0);
        lv_obj_set_style_border_color(s_tile[i], UI_ON_COLOR, 0);
    }
}

static void render(const pet_ui_snapshot_t *s)
{
    render_needs(s);
    render_call(s);
    render_tiles(s);

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
// (the tiles themselves are disabled, so it rarely happens).
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
    lv_obj_add_style(btn, &st_tile, 0);
    lv_obj_add_style(btn, &st_tile_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

    lv_obj_t *label = make_label(btn, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, cb, hold ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED, user);
    return btn;
}

// Ambient "cozy night": a few stars, a moon, and a ground band. Cheap to draw,
// and it makes the black screen feel like a place rather than an empty panel.
static void build_scene(lv_obj_t *scr)
{
    static const struct { int x, y, d; } STARS[] = {
        {40, 120, 6}, {90, 70, 4}, {152, 150, 5}, {212, 62, 7},
        {300, 122, 5}, {66, 210, 4}, {330, 200, 6}, {252, 238, 4},
    };
    for (unsigned i = 0; i < sizeof(STARS) / sizeof(STARS[0]); i++) {
        lv_obj_t *s = lv_obj_create(scr);
        lv_obj_set_size(s, STARS[i].d, STARS[i].d);
        lv_obj_set_style_radius(s, STARS[i].d / 2, 0);
        lv_obj_set_style_bg_color(s, UI_STAR, 0);
        lv_obj_set_style_border_width(s, 0, 0);
        lv_obj_set_pos(s, STARS[i].x, STARS[i].y);
    }

    lv_obj_t *moon = lv_obj_create(scr);
    lv_obj_set_size(moon, 46, 46);
    lv_obj_set_style_radius(moon, 23, 0);
    lv_obj_set_style_bg_color(moon, UI_MOON, 0);
    lv_obj_set_style_border_width(moon, 0, 0);
    lv_obj_set_pos(moon, SCR_W - 16 - 46, 22);

    lv_obj_t *ground = lv_obj_create(scr);
    lv_obj_set_size(ground, SCR_W, 150);
    lv_obj_set_style_radius(ground, 28, 0);
    lv_obj_set_style_bg_color(ground, UI_GROUND, 0);
    lv_obj_set_style_border_width(ground, 0, 0);
    lv_obj_set_pos(ground, 0, SCR_H - 150);
}

static void build_needs(lv_obj_t *scr)
{
    const lv_image_dsc_t *icon[2] = {&ic_feed_s, &ic_happy_s};  // food, fun
    const int y[2] = {NEED_ROW1_Y, NEED_ROW2_Y};
    for (int g = 0; g < 2; g++) {
        lv_obj_t *im = lv_image_create(scr);
        lv_image_set_src(im, icon[g]);
        lv_obj_set_pos(im, NEED_ICON_X, y[g] - 3);

        lv_obj_t *bar = lv_bar_create(scr);
        lv_obj_set_size(bar, NEED_BAR_W, NEED_BAR_H);
        lv_obj_set_pos(bar, NEED_BAR_X, y[g]);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_set_style_bg_color(bar, UI_TRACK, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(TILE_BG[g]), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, NEED_BAR_H / 2, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, NEED_BAR_H / 2, LV_PART_INDICATOR);
        s_need_bar[g] = bar;
    }
}

static void build_face(lv_obj_t *scr)
{
    s_ground = lv_obj_create(scr);
    lv_obj_set_size(s_ground, 210, 28);
    lv_obj_set_style_radius(s_ground, 14, 0);
    lv_obj_set_style_bg_color(s_ground, UI_SURFACE_OFF, 0);
    lv_obj_set_style_border_width(s_ground, 0, 0);
    lv_obj_set_scrollable(s_ground, false);
    lv_obj_align(s_ground, LV_ALIGN_CENTER, 0, FACE_Y + GROUND_DY);

    s_face = lv_image_create(scr);
    lv_obj_align(s_face, LV_ALIGN_CENTER, 0, FACE_Y);
    lv_obj_set_clickable(s_face, true);
    lv_obj_set_ext_click_area(s_face, EXT_CLICK_PX);
    lv_obj_add_event_cb(s_face, face_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_face, face_cb, LV_EVENT_LONG_PRESSED, NULL);
    show_face(&face_neutral);

    s_poop = lv_obj_create(scr);
    lv_obj_set_size(s_poop, 26, 20);
    lv_obj_set_style_radius(s_poop, 10, 0);
    lv_obj_set_style_bg_color(s_poop, lv_color_hex(0x6B4423), 0);
    lv_obj_set_style_border_width(s_poop, 0, 0);
    lv_obj_align_to(s_poop, s_face, LV_ALIGN_BOTTOM_RIGHT, 34, 6);
    lv_obj_set_hidden(s_poop, true);

    s_badge = lv_button_create(scr);
    lv_obj_set_size(s_badge, BADGE_D, BADGE_D);
    lv_obj_set_style_radius(s_badge, BADGE_D / 2, 0);
    lv_obj_set_style_border_width(s_badge, 3, 0);
    lv_obj_set_style_border_color(s_badge, UI_BG, 0);
    lv_obj_set_style_shadow_width(s_badge, 0, 0);
    lv_obj_set_ext_click_area(s_badge, EXT_CLICK_PX);
    lv_obj_align_to(s_badge, s_face, LV_ALIGN_TOP_RIGHT, BADGE_DX, BADGE_DY);
    s_badge_icon = lv_image_create(s_badge);
    lv_image_set_src(s_badge_icon, &ic_feed);
    lv_obj_center(s_badge_icon);
    lv_obj_add_event_cb(s_badge, badge_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_badge, true);
}

static void build_tiles(lv_obj_t *scr)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *tile = lv_button_create(scr);
        lv_obj_set_size(tile, TILE_D, TILE_D);
        lv_obj_set_pos(tile, TILE_X0 + i * TILE_DX, SCR_H + TILE_BOTTOM - TILE_D);
        lv_obj_set_ext_click_area(tile, EXT_CLICK_PX);
        apply_tile(tile, lv_color_hex(TILE_BG[i]));

        lv_obj_set_style_radius(tile, TILE_D / 2, 0);  // round candy button

        lv_obj_t *icon = lv_image_create(tile);
        lv_image_set_src(icon, TILE_ICON[i]);
        lv_obj_center(icon);

        lv_obj_add_event_cb(tile, action_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)TILE_ACT[i]);
        s_tile[i] = tile;
    }
}

static void build_settings(lv_obj_t *scr)
{
    s_settings = lv_obj_create(scr);
    lv_obj_set_size(s_settings, 330, 260);
    lv_obj_align(s_settings, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_settings, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_settings, UI_BORDER, 0);
    lv_obj_set_style_border_width(s_settings, 1, 0);
    lv_obj_set_style_radius(s_settings, UI_RADIUS_LG, 0);
    lv_obj_set_scrollable(s_settings, false);

    lv_obj_t *title = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT_DIM);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_pos(title, 16, 10);

    lv_obj_t *time_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(time_lbl, "Time");
    lv_obj_set_pos(time_lbl, 16, 56);
    s_settings_time = make_label(s_settings, &lv_font_montserrat_14, UI_ACCENT);
    lv_obj_set_pos(s_settings_time, 16, 82);
    make_sbtn(s_settings, "-1h", 176, 52, 66, settings_time_cb, (void *)(intptr_t)-3600, false);
    make_sbtn(s_settings, "+1h", 248, 52, 66, settings_time_cb, (void *)(intptr_t)3600, false);

    lv_obj_t *br_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(br_lbl, "Brightness");
    lv_obj_set_pos(br_lbl, 16, 122);
    make_sbtn(s_settings, "cycle", 190, 116, 124, settings_bright_cb, NULL, false);

    lv_obj_t *reset_lbl = make_label(s_settings, &lv_font_montserrat_14, UI_TEXT);
    lv_label_set_text(reset_lbl, "Reset pet");
    lv_obj_set_pos(reset_lbl, 16, 172);
    make_sbtn(s_settings, "hold", 190, 166, 124, settings_reset_cb, NULL, true);

    make_sbtn(s_settings, "CLOSE", 115, 210, 100, settings_close_cb, NULL, false);
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
    tile_styles_init();
    bsp_display_brightness_set(100);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_BG, 0);  // AMOLED: black = pixels off
    lv_obj_set_scrollable(scr, false);

    build_scene(scr);
    build_needs(scr);
    build_face(scr);
    build_tiles(scr);
    build_settings(scr);
    register_touch_log();

    lv_timer_create(blink_cb, BLINK_PERIOD_MS, NULL);
    lv_timer_create(pulse_cb, PULSE_MS, NULL);
    lv_timer_create(repaint_cb, REPAINT_MS, NULL);

    bsp_display_unlock();
}
