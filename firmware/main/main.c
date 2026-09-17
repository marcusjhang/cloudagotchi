/*
 * esp32-tamagotchi — Phase 3: it has needs.
 *
 * Boot order matters: NVS first (journal + pet live there), then the screen
 * (so the pet can be shown as soon as it is loaded), then the game task,
 * then the IMU (its callback feeds the game).
 */
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "app_imu.h"
#include "game.h"
#include "journal.h"
#include "pet_ui.h"

static const char *TAG = "tamagotchi";

static void on_shake(void)
{
    game_act(PET_ACT_SHAKE);
}

void app_main(void)
{
    ESP_LOGI(TAG, "booting");

    // The panel occasionally drops QSPI colour transfers when the bus is
    // busy; LVGL repaints heal it. Logging every drop is expensive, so mute.
    esp_log_level_set("lcd_panel.io.spi", ESP_LOG_NONE);
    esp_log_level_set("co5300_spi", ESP_LOG_NONE);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erasing (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    journal_init();
    pet_ui_start(game_act);
    game_start();
    ESP_ERROR_CHECK(app_imu_start(on_shake));
}
