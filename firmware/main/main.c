/*
 * Cloudagotchi — article 3: the pet grows a brain.
 *
 * Interactions go up to Lambda, state comes back down, and the face on
 * screen is now rendered from DynamoDB truth. Ignore it and EventBridge
 * will make it visibly sad. You have been warned.
 */
#include <string.h>
#include "cJSON.h"
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

/* Cloud → device: the brain's verdict becomes the face on screen. */

static pet_mood_t mood_from_string(const char *s)
{
    if (strcmp(s, "happy") == 0)    return PET_MOOD_HAPPY;
    if (strcmp(s, "sad") == 0)      return PET_MOOD_SAD;
    if (strcmp(s, "sleeping") == 0) return PET_MOOD_SLEEPING;
    return PET_MOOD_NEUTRAL;
}

static void handle_state(cJSON *json)
{
    cJSON *hunger = cJSON_GetObjectItem(json, "hunger");
    cJSON *energy = cJSON_GetObjectItem(json, "energy");
    cJSON *mood   = cJSON_GetObjectItem(json, "mood");
    cJSON *face   = cJSON_GetObjectItem(json, "face");

    if (cJSON_IsNumber(hunger) && cJSON_IsNumber(energy) &&
        cJSON_IsNumber(mood) && cJSON_IsString(face)) {
        pet_ui_set_state(hunger->valueint, energy->valueint,
                         mood->valueint, mood_from_string(face->valuestring));
    }
}

static void on_cloud_message(const char *topic, const char *payload, int len)
{
    cJSON *json = cJSON_ParseWithLength(payload, len);
    if (json == NULL) return;

    if (strstr(topic, "/state") != NULL) {
        handle_state(json);
    } else {
        ESP_LOGI(TAG, "Cloud says [%s]: %.*s", topic, len, payload);
    }
    cJSON_Delete(json);
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
    // placeholders — the cloud overwrites them as soon as "hello" is answered.
    pet_ui_start(on_interaction);
    pet_ui_set_state(80, 80, 80, PET_MOOD_NEUTRAL);

    ESP_ERROR_CHECK(app_imu_start(on_shake));
    ESP_ERROR_CHECK(app_wifi_connect());
    ESP_ERROR_CHECK(app_mqtt_start(on_cloud_message));
}
