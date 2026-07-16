/*
 * Cloudagotchi — article 4: the pet gets a job.
 *
 * Every morning the cloud drops a newspaper on the briefing topic:
 * a presigned URL to a WAV of the AWS news, summarized by Bedrock in the
 * pet's own voice and spoken by Polly. Tap the badge, the ghost reads it
 * out loud. Yes, out loud. It never gets old.
 */
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_wifi.h"
#include "app_mqtt.h"
#include "app_imu.h"
#include "app_audio.h"
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

/* The morning paper: remember the audio URL, show the badge. */

static char s_briefing_url[2048]; // presigned S3 URLs are LONG — with temporary
                                  // credentials the security token alone pushes
                                  // them past 1400 chars. Truncate = broken signature.

static void play_briefing_task(void *arg)
{
    // Put the paper away FIRST: the badge's circularly-scrolling headline
    // re-creates its animation forever (quiet mode can't kill it), and its
    // constant redraws on a Wi-Fi-congested SPI bus are what starved the
    // audio task into robot-voice. Then the pet settles down to read.
    pet_ui_hide_briefing();
    pet_ui_quiet_mode(true);
    app_audio_play_url(s_briefing_url);
    pet_ui_quiet_mode(false);
    vTaskDelete(NULL);
}

static void on_briefing_tapped(void)
{
    ESP_LOGI(TAG, "News badge tapped — playing %s", s_briefing_url);
    // Audio streaming blocks for ~1 minute: never do that in an LVGL callback.
    // Low priority + pinned to core 1: the download must never starve the
    // Wi-Fi/TCP stack (core 0) — the task watchdog taught us that one.
    xTaskCreatePinnedToCore(play_briefing_task, "briefing", 8192, NULL, 2, NULL, 1);
}

static void handle_briefing(cJSON *json)
{
    cJSON *url = cJSON_GetObjectItem(json, "url");
    cJSON *headline = cJSON_GetObjectItem(json, "headline");
    if (!cJSON_IsString(url) || !cJSON_IsString(headline)) return;

    if (strlcpy(s_briefing_url, url->valuestring, sizeof(s_briefing_url))
            >= sizeof(s_briefing_url)) {
        // A truncated presigned URL has a broken signature — S3 would just 403.
        ESP_LOGE(TAG, "briefing URL too long for buffer — enlarge s_briefing_url");
        return;
    }
    pet_ui_show_briefing(headline->valuestring, on_briefing_tapped);
    ESP_LOGI(TAG, "The paper is here: %s", headline->valuestring);
}

static void on_cloud_message(const char *topic, const char *payload, int len)
{
    cJSON *json = cJSON_ParseWithLength(payload, len);
    if (json == NULL) return;

    if (strstr(topic, "/state") != NULL) {
        handle_state(json);
    } else if (strstr(topic, "/briefing") != NULL) {
        handle_briefing(json);
    } else {
        ESP_LOGI(TAG, "Cloud says [%s]: %.*s", topic, len, payload);
    }
    cJSON_Delete(json);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Cloudagotchi booting...");

    // This panel occasionally drops SPI color transfers when the bus is
    // congested (Wi-Fi bringup, TLS downloads) — LVGL repaints heal it.
    // Printing every drop at 115200 baud costs ~7 ms of CPU per line,
    // which is enough to starve audio streaming. Diagnosed; now muted.
    esp_log_level_set("lcd_panel.io.spi", ESP_LOG_NONE);
    esp_log_level_set("co5300_spi", ESP_LOG_NONE);

    // Face first: the pet appears even before Wi-Fi is up. These stats are
    // placeholders — the cloud overwrites them as soon as "hello" is answered.
    pet_ui_start(on_interaction);
    pet_ui_set_state(80, 80, 80, PET_MOOD_NEUTRAL);

    ESP_ERROR_CHECK(app_imu_start(on_shake));
    ESP_ERROR_CHECK(app_audio_init());
    ESP_ERROR_CHECK(app_wifi_connect());
    ESP_ERROR_CHECK(app_mqtt_start(on_cloud_message));
}
