#pragma once

#include "esp_err.h"

/** Connect to Wi-Fi (blocking, retries forever — a pet is patient). */
esp_err_t app_wifi_connect(void);
