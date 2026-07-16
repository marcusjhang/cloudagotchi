#include "app_mqtt.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "sdkconfig.h"

static const char *TAG = "app_mqtt";

// The provisioning script drops the credentials in certs/ and the build
// embeds them into the binary (see CMakeLists.txt EMBED_TXTFILES).
extern const char root_ca_pem[]     asm("_binary_AmazonRootCA1_pem_start");
extern const char device_cert_pem[] asm("_binary_device_cert_pem_start");
extern const char device_key_pem[]  asm("_binary_device_key_pem_start");

static esp_mqtt_client_handle_t s_client;
static app_mqtt_message_cb_t s_on_message;
static bool s_connected;

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        s_connected = true;
        ESP_LOGI(TAG, "Connected to AWS IoT Core");

        // Listen to everything the cloud sends to *this* pet.
        char topic[96];
        snprintf(topic, sizeof(topic), "cloudagotchi/%s/#", CONFIG_CLOUDAGOTCHI_THING_NAME);
        esp_mqtt_client_subscribe(s_client, topic, 1);

        // Announce ourselves — the cloud answers with the pet's current state.
        app_mqtt_publish("hello", "{\"firmware\":\"article-1\"}");
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Disconnected — the client retries automatically");
        break;
    case MQTT_EVENT_DATA:
        // A message bigger than the RX buffer arrives in several DATA events;
        // only the first carries the topic. We size the buffer to fit our
        // largest payload, so a topic-less chunk means truncation — skip it
        // rather than feed a broken fragment to the JSON parser.
        if (s_on_message != NULL && event->topic_len > 0) {
            if (event->data_len < event->total_data_len) {
                ESP_LOGW(TAG, "payload truncated (%d/%d bytes) — raise buffer.size",
                         event->data_len, event->total_data_len);
            }
            // Topic/payload are not NUL-terminated: copy the topic.
            char t[128] = {0};
            memcpy(t, event->topic, event->topic_len < 127 ? event->topic_len : 127);
            s_on_message(t, event->data, event->data_len);
        }
        break;
    default:
        break;
    }
}

esp_err_t app_mqtt_start(app_mqtt_message_cb_t on_message)
{
    s_on_message = on_message;

    const esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = "mqtts://" CONFIG_CLOUDAGOTCHI_IOT_ENDPOINT ":8883",
            .verification.certificate = root_ca_pem,
        },
        .credentials = {
            // AWS IoT authenticates the device with its X.509 certificate...
            .authentication = {
                .certificate = device_cert_pem,
                .key = device_key_pem,
            },
            // ...and our IoT policy requires client id == thing name.
            .client_id = CONFIG_CLOUDAGOTCHI_THING_NAME,
        },
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);
}

esp_err_t app_mqtt_publish(const char *subtopic, const char *json)
{
    char topic[96];
    snprintf(topic, sizeof(topic), "cloudagotchi/%s/%s", CONFIG_CLOUDAGOTCHI_THING_NAME, subtopic);
    int id = esp_mqtt_client_publish(s_client, topic, json, 0, 1, 0);
    return id >= 0 ? ESP_OK : ESP_FAIL;
}

bool app_mqtt_is_connected(void)
{
    return s_connected;
}
