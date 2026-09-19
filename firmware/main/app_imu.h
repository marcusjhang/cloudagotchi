#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef void (*app_imu_shake_cb_t)(void);

/** Start polling the QMI8658 accelerometer; fires the callback on a shake. */
esp_err_t app_imu_start(app_imu_shake_cb_t on_shake);

/** True once since the last call if the board was picked up/moved. Used by
    power.c to wake from light sleep. */
bool app_imu_moved(void);
