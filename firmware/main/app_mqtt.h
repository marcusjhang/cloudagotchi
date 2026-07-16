#pragma once

#include <stdbool.h>
#include "esp_err.h"

/** Called for every message arriving on cloudagotchi/<thing>/# */
typedef void (*app_mqtt_message_cb_t)(const char *topic, const char *payload, int len);

/** Connect to AWS IoT Core over mutual-TLS MQTT. */
esp_err_t app_mqtt_start(app_mqtt_message_cb_t on_message);

/** Publish a JSON payload on cloudagotchi/<thing>/<subtopic>. */
esp_err_t app_mqtt_publish(const char *subtopic, const char *json);

bool app_mqtt_is_connected(void);
