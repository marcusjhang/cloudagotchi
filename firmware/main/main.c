/*
 * esp32-tamagotchi — Phase 1: it's alive.
 *
 * Boots the Waveshare BSP, puts the (placeholder) pet face on the AMOLED,
 * and wires touch + shake to local reactions. No cloud, no radio: from
 * Phase 3 the pet's state lives in NVS on this chip.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_imu.h"
#include "pet_ui.h"

static const char *TAG = "tamagotchi";

// Human → pet. Phase 3 routes these into the game state (pet_act).
static void on_interaction(pet_interaction_t what)
{
    switch (what) {
    case PET_INTERACTION_PET:  ESP_LOGI(TAG, "interaction: pet");  break;
    case PET_INTERACTION_FEED: ESP_LOGI(TAG, "interaction: feed"); break;
    case PET_INTERACTION_PLAY: ESP_LOGI(TAG, "interaction: play"); break;
    }
}

static void on_shake(void)
{
    pet_ui_react_startled();
    on_interaction(PET_INTERACTION_PLAY);
}

void app_main(void)
{
    ESP_LOGI(TAG, "booting");

    // The panel occasionally drops QSPI colour transfers when the bus is
    // busy; LVGL repaints heal it. Logging every drop is expensive, so mute.
    esp_log_level_set("lcd_panel.io.spi", ESP_LOG_NONE);
    esp_log_level_set("co5300_spi", ESP_LOG_NONE);

    pet_ui_start(on_interaction);
    pet_ui_set_state(80, 80, 80, PET_MOOD_NEUTRAL);  // placeholder until Phase 3

    ESP_ERROR_CHECK(app_imu_start(on_shake));
}
