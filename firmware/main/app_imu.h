#pragma once

#include "esp_err.h"

typedef void (*app_imu_shake_cb_t)(void);

/** Start polling the QMI8658 accelerometer; fires the callback on a shake. */
esp_err_t app_imu_start(app_imu_shake_cb_t on_shake);
