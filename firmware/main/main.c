/*
 * Cloudagotchi — article 1: proof of life.
 *
 * Connects the board to Wi-Fi and AWS IoT Core, announces itself,
 * and logs every message the cloud sends back. No pet on screen yet —
 * that's article 2. First, the nervous system.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_wifi.h"
#include "app_mqtt.h"

static const char *TAG = "cloudagotchi";

static void on_cloud_message(const char *topic, const char *payload, int len)
{
    ESP_LOGI(TAG, "Cloud says [%s]: %.*s", topic, len, payload);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Cloudagotchi booting...");

    ESP_ERROR_CHECK(app_wifi_connect());
    ESP_ERROR_CHECK(app_mqtt_start(on_cloud_message));

    // Heartbeat so you can watch the pet's pulse in the AWS console
    // (MQTT test client, subscribe to cloudagotchi/#).
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (app_mqtt_is_connected()) {
            app_mqtt_publish("heartbeat", "{\"alive\":true}");
        }
    }
}
