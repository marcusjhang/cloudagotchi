#include "game.h"

#include <time.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "config.h"
#include "journal.h"
#include "persist.h"
#include "pet_ui.h"

static const char *TAG = "game";

static pet_t s_pet;
static SemaphoreHandle_t s_lock;
static int64_t s_base_real;   // time(NULL) at boot
static int64_t s_base_game;   // game clock at boot
static int64_t s_last_save;

// Phase 3 has no RTC, so every boot starts the system clock at 1970. Game
// time therefore resumes from wherever the pet was last saved and advances
// with uptime; nothing happens while the board is off. Phase 4 replaces this
// with the real clock, and pet_apply()'s catch-up does the rest.
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
}

// Never call pet_ui_* while holding s_lock: the LVGL task takes s_lock from
// button callbacks while it already holds the display lock, and the display
// lock is what pet_ui_* takes. Snapshot under the lock, publish outside it.
void game_act(pet_action_t a)
{
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
            ESP_LOGI(TAG, "%s %s | food %u fun %u zzz %u clean %u hp %u | %s%smistakes %u | heap %u",
                     pet_stage_name(s.stage), pet_face_name(s.face),
                     s.stats[0], s.stats[1], s.stats[2], s.stats[3], s.stats[4],
                     s.asleep ? "asleep " : "", s.dirty ? "poop " : "", s.mistakes,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        }
    }
}

void game_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_base_real = (int64_t)time(NULL);

    if (persist_load(&s_pet)) {
        s_base_game = s_pet.updated_at;
        ESP_LOGI(TAG, "loaded: %s, age %llu h, %lu actions, %u mistakes",
                 pet_stage_name((pet_stage_t)s_pet.stage),
                 (unsigned long long)(pet_age_s(&s_pet, s_base_game) / 3600),
                 (unsigned long)s_pet.actions_taken, s_pet.care_mistakes);
    } else {
        s_base_game = s_base_real;
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
