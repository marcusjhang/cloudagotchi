/*
 * The pet's face — minimal and boring, on purpose.
 *
 * Display setup: 100% BSP defaults. No custom buffers, no config overrides,
 * no idle animations, no position animations. The screen shows: three stat
 * bars and one face sprite. The face only changes
 * by swapping which sprite is shown — a blink, a mood, a talk frame are all
 * the same operation. Nothing here can outrun the display.
 */
#include "pet_ui.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "lvgl.h"
#include "faces/faces.h"

static pet_interaction_cb_t s_on_interaction;

static lv_obj_t *s_face;     // the one image widget
static lv_obj_t *s_bars[3];  // hunger, energy, mood
static pet_mood_t s_mood = PET_MOOD_NEUTRAL;

static lv_timer_t *s_blink_timer;
static lv_timer_t *s_idle_timer;
static bool s_dozing;   // fell asleep from boredom (local nap, not cloud night)

/* ---------- helpers ------------------------------------------------------ */

static const lv_image_dsc_t *mood_sprite(pet_mood_t mood)
{
    switch (mood) {
    case PET_MOOD_HAPPY:    return &face_happy;
    case PET_MOOD_SAD:      return &face_sad;
    case PET_MOOD_SLEEPING: return &face_sleeping;
    default:                return &face_neutral;
    }
}

// Swap the face sprite AND invalidate the whole widget area. Without the
// explicit invalidate, a dropped SPI transfer can leave slivers of the
// previous sprite on screen (the panel silently loses chunks now and then),
// so a wide happy smile "sticks" behind a narrower neutral mouth.
static void set_face(const lv_image_dsc_t *src)
{
    lv_image_set_src(s_face, src);
    lv_obj_invalidate(s_face);
}

// One stat row: [icon] [bar]. Three of them across the top of the screen.
// The icon is generously padded so it doubles as a TAP TARGET — tapping
// the burger feeds the pet (the only stat the pet can't refill itself).
static lv_obj_t *make_stat_row(lv_obj_t *parent, const lv_image_dsc_t *icon,
                               lv_color_t color, int x_ofs,
                               lv_event_cb_t on_tap)
{
    lv_obj_t *icon_w = lv_image_create(parent);
    lv_image_set_src(icon_w, icon);
    lv_obj_align(icon_w, LV_ALIGN_TOP_MID, x_ofs - 34, 12);
    if (on_tap != NULL) {
        lv_obj_add_flag(icon_w, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(icon_w, 14); // fat finger friendly
        lv_obj_add_event_cb(icon_w, on_tap, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 62, 12);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, x_ofs + 18, 20);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A2530), LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_opa(bar, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
    return bar;
}

/* ---------- blink: two-frame image swap --------------------------------- */

static void blink_close_cb(lv_timer_t *t)
{
    // Re-open the eyes — unless the pet started talking or dozed off
    // during the 120 ms the eyes were shut.
    if (!s_dozing) set_face(mood_sprite(s_mood));
    lv_timer_delete(t);
}

static void blink_timer_cb(lv_timer_t *t)
{
    if (s_dozing || s_mood == PET_MOOD_SLEEPING) return;
    // Dedicated blink frame: closed eyes, SAME mouth as the current mood —
    // so a blink only moves the eyes. (v1 borrowed the sleeping sprite here,
    // which flashed its zzz + different mouth for 120 ms. Uncanny.)
    switch (s_mood) {
    case PET_MOOD_HAPPY: set_face(&face_blink_happy);  break;
    case PET_MOOD_SAD:   set_face(&face_blink_sad);    break;
    default:             set_face(&face_blink_neutral); break;
    }
    lv_timer_create(blink_close_cb, 120, NULL);
}

// Self-healing: the panel occasionally drops SPI transfer chunks, leaving
// stale slivers on screen that nothing repaints. Wipe the slate every 15 s.
static void repaint_timer_cb(lv_timer_t *t)
{
    lv_obj_invalidate(lv_screen_active());
}

/* ---------- dozing: fall asleep when bored, wake on interaction ---------- */

// 2 minutes with no interaction → the pet nods off (zzz sprite). This is a
// LOCAL nap: cloud state is untouched and bars keep updating underneath.
static void idle_timer_cb(lv_timer_t *t)
{
    s_dozing = true;
    set_face(&face_sleeping);   // the zzz finally has its moment
}

// Any interaction resets the boredom clock — and wakes the pet if dozing.
static void note_activity(void)
{
    if (s_idle_timer) lv_timer_reset(s_idle_timer);
    if (s_dozing) {
        s_dozing = false;
        set_face(mood_sprite(s_mood));
    }
}

/* ---------- reactions ----------------------------------------------------*/

void pet_ui_react_happy(void)
{
    // Recursive lock: safe from the LVGL task (tap callbacks) AND required
    // from other tasks. Without it, cross-task calls (e.g. the IMU task's
    // shake) are undefined behavior — typically a silent no-op.
    bsp_display_lock(0);
    note_activity();
    set_face(&face_happy);
    bsp_display_unlock();
    // The next blink or state update restores the mood face.
}

void pet_ui_react_startled(void)
{
    bsp_display_lock(0);
    note_activity();          // a shake definitely wakes a napping pet
    set_face(&face_talk_3);   // wide eyes + "oh"
    bsp_display_unlock();
}

/* ---------- input ---------------------------------------------------------*/

static void face_clicked_cb(lv_event_t *e)
{
    pet_ui_react_happy();
    if (s_on_interaction) s_on_interaction(PET_INTERACTION_PET);
}

static void food_clicked_cb(lv_event_t *e)
{
    // Nom nom: happy flash + tell the cloud we ate.
    pet_ui_react_happy();
    if (s_on_interaction) s_on_interaction(PET_INTERACTION_FEED);
}

/* ---------- cloud state → face ------------------------------------------- */

void pet_ui_set_state(uint8_t hunger, uint8_t energy, uint8_t mood_value, pet_mood_t mood)
{
    bsp_display_lock(0);
    lv_bar_set_value(s_bars[0], hunger, LV_ANIM_OFF);
    lv_bar_set_value(s_bars[1], energy, LV_ANIM_OFF);
    lv_bar_set_value(s_bars[2], mood_value, LV_ANIM_OFF);
    s_mood = mood;
    // Cloud updates refresh the bars but do NOT wake a napping pet —
    // only human interaction does.
    if (!s_dozing) set_face(mood_sprite(mood));
    bsp_display_unlock();
}

/* ---------- entry point --------------------------------------------------*/

void pet_ui_start(pet_interaction_cb_t on_interaction)
{
    s_on_interaction = on_interaction;

    // Vendor defaults, nothing else — the BSP's partial-mode path is the
    // only rendering mode its SPI flush pipeline actually supports (a
    // full-refresh experiment here produced an uninitialized white frame).
    // Stale-sliver artifacts are handled by set_face() invalidation plus
    // the periodic repaint below.
    bsp_display_start();

    bsp_display_lock(0);

    // Brightness starts at 0 and is a panel command on the pixel bus:
    // send it under the lock.
    bsp_display_brightness_set(100);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0); // AMOLED: black = off

    // Stat rows: 🍔 hunger (orange, tappable = FEED), ⚡ energy (teal),
    // ❤️ mood (pink).
    // Hunger bar in avocado-flesh green (yellow-green, deliberately distinct
    // from the teal energy bar beside it).
    s_bars[0] = make_stat_row(screen, &icon_food,   lv_color_hex(0x9CC959), -122, food_clicked_cb);
    s_bars[1] = make_stat_row(screen, &icon_energy, lv_color_hex(0x2EC4B6),    0, NULL);
    s_bars[2] = make_stat_row(screen, &icon_heart,  lv_color_hex(0xFF6392),  122, NULL);

    // The face: ONE static image widget, centered. That's the whole pet.
    s_face = lv_image_create(screen);
    set_face(&face_neutral);
    lv_obj_align(s_face, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_flag(s_face, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_face, face_clicked_cb, LV_EVENT_CLICKED, NULL);

    // Blink every ~4 s by swapping to the closed-eyes sprite for 120 ms.
    s_blink_timer = lv_timer_create(blink_timer_cb, 4200, NULL);

    // Periodic self-heal against dropped SPI chunks (ghost slivers).
    lv_timer_create(repaint_timer_cb, 15000, NULL);

    // Doze off after 2 minutes without interaction; any touch/shake wakes.
    s_idle_timer = lv_timer_create(idle_timer_cb, 120000, NULL);

    bsp_display_unlock();
}
