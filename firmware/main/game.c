#include "game.h"

#include <time.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "battery.h"
#include "config.h"
#include "journal.h"
#include "persist.h"
#include "pet_ui.h"
#include "power.h"

static const char *TAG = "game";

static pet_t s_pet;
static SemaphoreHandle_t s_lock;
static int64_t s_base_real;   // time(NULL) at boot (RTC-seeded)
static int64_t s_base_game;   // game clock at boot
static int64_t s_last_save;

// Game time is the real epoch clock (seeded from the RTC by rtc_init()). At
// FAST_FORWARD 1 this is simply time(NULL), so time that passed while the
// board was off shows up in pet_apply()'s catch-up. FAST_FORWARD scales the
// rate for testing: the game clock runs Nx faster, starting at boot.
int64_t game_now(void)
{
    return s_base_game + ((int64_t)time(NULL) - s_base_real) * FAST_FORWARD;
}

// Must hold s_lock.
static void snapshot(pet_ui_snapshot_t *s, int64_t now)
{
    for (int i = 0; i < PET_STAT_COUNT; i++) {
        s->stats[i] = (uint8_t)pet_stat(&s_pet, (pet_stat_t)i);
    }
    s->face = pet_face(&s_pet, now);
    s->stage = (pet_stage_t)s_pet.stage;
    s->asleep = s_pet.asleep;
    s->dirty = s_pet.dirty;
    s->age_s = (uint32_t)pet_age_s(&s_pet, now);
    s->weight = s_pet.weight;
    s->mistakes = s_pet.care_mistakes;
    s->boots = journal_boots();
    s->clock_s = (int64_t)time(NULL);
    s->clock_ok = s->clock_s >= CLOCK_VALID_EPOCH;
    s->batt_ok = battery_read(&s->batt);
    for (int a = 0; a < PET_ACT_COUNT; a++) {
        s->enabled[a] = pet_action_enabled(&s_pet, (pet_action_t)a);
    }
}

// Never call pet_ui_* while holding s_lock: the LVGL task takes s_lock from
// button callbacks while it already holds the display lock, and the display
// lock is what pet_ui_* takes. Snapshot under the lock, publish outside it.
void game_act(pet_action_t a)
{
    power_note_activity();
    pet_ui_snapshot_t s;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const int64_t now = game_now();
    const pet_result_t r = pet_act(&s_pet, a, now);
    if (r == PET_OK) {
        persist_save(&s_pet);
        s_last_save = now;
    }
    snapshot(&s, now);
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, "%s -> %s", pet_action_name(a), pet_result_text(r));
    if (r != PET_OK) {
        pet_ui_toast(pet_result_text(r));
    }
    pet_ui_update(&s);
}

void game_save_now(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    persist_save(&s_pet);
    s_last_save = game_now();
    xSemaphoreGive(s_lock);
}

void game_reset(void)
{
    power_note_activity();
    pet_ui_snapshot_t s;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const int64_t now = game_now();
    pet_new(&s_pet, now);
    persist_save(&s_pet);
    s_last_save = now;
    snapshot(&s, now);
    xSemaphoreGive(s_lock);

    pet_ui_update(&s);
    ESP_LOGW(TAG, "reset: new egg");
}

static void game_task(void *arg)
{
    (void)arg;
    uint32_t ticks = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));

        pet_ui_snapshot_t s;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        const int64_t now = game_now();
        pet_apply(&s_pet, now);
        if (now - s_last_save >= SAVE_INTERVAL_S) {
            persist_save(&s_pet);
            s_last_save = now;
        }
        snapshot(&s, now);
        xSemaphoreGive(s_lock);

        pet_ui_update(&s);

        if (++ticks % 60 == 0) {
            ESP_LOGI(TAG, "%s %s | food %u fun %u zzz %u clean %u hp %u | %s%smistakes %u | "
                          "batt %s%u%% %dmV | heap %u",
                     pet_stage_name(s.stage), pet_face_name(s.face),
                     s.stats[0], s.stats[1], s.stats[2], s.stats[3], s.stats[4],
                     s.asleep ? "asleep " : "", s.dirty ? "poop " : "", s.mistakes,
                     s.batt_ok ? "" : "? ",
                     s.batt_ok ? (unsigned)s.batt.percent : 0,
                     s.batt_ok ? s.batt.millivolts : -1,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        }
    }
}

void game_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_base_real = (int64_t)time(NULL);
    s_base_game = s_base_real;

    if (persist_load(&s_pet)) {
        ESP_LOGI(TAG, "loaded: %s, age %llu h, %lu actions, %u mistakes (was off %lld min)",
                 pet_stage_name((pet_stage_t)s_pet.stage),
                 (unsigned long long)(pet_age_s(&s_pet, s_base_game) / 3600),
                 (unsigned long)s_pet.actions_taken, s_pet.care_mistakes,
                 (long long)(s_base_game - s_pet.updated_at) / 60);
    } else {
        pet_new(&s_pet, s_base_game);
        persist_save(&s_pet);
        ESP_LOGI(TAG, "new egg");
    }
    s_last_save = s_base_game;

    pet_ui_snapshot_t s;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snapshot(&s, game_now());
    xSemaphoreGive(s_lock);
    pet_ui_update(&s);

    xTaskCreatePinnedToCore(game_task, "game", 4096, NULL, 3, NULL, 0);
}
