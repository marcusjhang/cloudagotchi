/*
 * Cloudagotchi — article 2: the pet gets a face.
 *
 * A sprite face lives on the AMOLED: it blinks, reacts to taps and shakes,
 * and dozes off when ignored. Every interaction is reported to the cloud
 * over MQTT. The cloud doesn't answer yet — that's article 3, when it
 * grows a brain.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_wifi.h"
#include "app_mqtt.h"
#include "app_imu.h"
#include "pet_ui.h"

static const char *TAG = "cloudagotchi";

/* Human → device → cloud */

static void on_interaction(pet_interaction_t what)
{
    switch (what) {
    case PET_INTERACTION_PET:
        app_mqtt_publish("interaction", "{\"type\":\"pet\"}");
        break;
    case PET_INTERACTION_FEED:
        app_mqtt_publish("interaction", "{\"type\":\"feed\"}");
        break;
    case PET_INTERACTION_PLAY:
        app_mqtt_publish("interaction", "{\"type\":\"play\"}");
        break;
    }
}

static void on_shake(void)
{
    pet_ui_react_startled();
    on_interaction(PET_INTERACTION_PLAY);
}

/* Cloud → device (state updates arrive here from article 3 onwards) */

static void on_cloud_message(const char *topic, const char *payload, int len)
{
    ESP_LOGI(TAG, "Cloud says [%s]: %.*s", topic, len, payload);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Cloudagotchi booting...");

    // This panel occasionally drops SPI color transfers when the bus is
    // congested (Wi-Fi bringup) — LVGL repaints heal it. Printing every
    // drop at 115200 baud is expensive; diagnosed, so muted.
    esp_log_level_set("lcd_panel.io.spi", ESP_LOG_NONE);
    esp_log_level_set("co5300_spi", ESP_LOG_NONE);

    // Face first: the pet appears even before Wi-Fi is up. These stats are
    // placeholders — in article 3 the cloud takes over as source of truth.
    pet_ui_start(on_interaction);
    pet_ui_set_state(80, 80, 80, PET_MOOD_NEUTRAL);

    ESP_ERROR_CHECK(app_imu_start(on_shake));
    ESP_ERROR_CHECK(app_wifi_connect());
    ESP_ERROR_CHECK(app_mqtt_start(on_cloud_message));
}
