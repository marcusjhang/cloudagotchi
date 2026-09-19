/*
 * Shake detection on the QMI8658 6-axis IMU.
 *
 * No gesture-recognition library needed: a shake is simply "acceleration
 * magnitude far from 1 g, several times, within a short window".
 */
#include "app_imu.h"

#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "qmi8658.h"

static const char *TAG = "app_imu";

#define SHAKE_THRESHOLD_MG  1800.0f // |a| beyond this = one "jolt" (rest ≈ 1000 mg)
#define SHAKE_JOLTS_NEEDED  3       // this many jolts...
#define SHAKE_WINDOW_MS     800     // ...within this window = a shake
#define SHAKE_COOLDOWN_MS   1500    // then ignore for a bit (debounce)
#define WAKE_MOTION_MG      450.0f  // deviation from 1 g that means "picked up"

static app_imu_shake_cb_t s_on_shake;
static qmi8658_dev_t s_imu;
static volatile bool s_moved;

static void imu_task(void *arg)
{
    int jolts = 0;
    TickType_t window_start = 0;
    // The first samples after init read as a jolt (the board was just reset,
    // and the sensor's filters are settling): ignore the first 1.5 s.
    TickType_t cooldown_until = xTaskGetTickCount() + pdMS_TO_TICKS(1500);

    while (true) {
        float ax, ay, az;
        if (qmi8658_read_accel(&s_imu, &ax, &ay, &az) == ESP_OK) {
            TickType_t now = xTaskGetTickCount();

            // 1. Total acceleration. At rest this is ~1000 mg (gravity).
            float mag = sqrtf(ax * ax + ay * ay + az * az);

            // Coarse "was it handled" flag for power.c's light-sleep wake.
            if (fabsf(mag - 1000.0f) > WAKE_MOTION_MG) {
                s_moved = true;
            }

            // 2. A jolt = a reading well above resting gravity.
            if (mag > SHAKE_THRESHOLD_MG && now > cooldown_until) {
                if (jolts == 0) window_start = now;
                jolts++;

                // 3. Enough jolts, fast enough → that's a shake.
                if (jolts >= SHAKE_JOLTS_NEEDED &&
                    (now - window_start) < pdMS_TO_TICKS(SHAKE_WINDOW_MS)) {
                    ESP_LOGI(TAG, "Shake detected!");
                    jolts = 0;
                    cooldown_until = now + pdMS_TO_TICKS(SHAKE_COOLDOWN_MS);
                    if (s_on_shake) s_on_shake();
                }
            }

            // 4. Window expired without enough jolts → start over.
            if (jolts > 0 && (now - window_start) > pdMS_TO_TICKS(SHAKE_WINDOW_MS)) {
                jolts = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz is plenty for human hands
    }
}

bool app_imu_moved(void)
{
    const bool moved = s_moved;
    s_moved = false;
    return moved;
}

esp_err_t app_imu_start(app_imu_shake_cb_t on_shake)
{
    s_on_shake = on_shake;

    // The IMU shares the board's I2C bus, already set up by the BSP.
    // On this board the QMI8658 answers at the HIGH address (0x6B).
    ESP_ERROR_CHECK(bsp_i2c_init());
    esp_err_t err = qmi8658_init(&s_imu, bsp_i2c_get_handle(), QMI8658_ADDRESS_HIGH);
    if (err != ESP_OK) {
        // A missing/unhappy IMU just means no shake-to-play — the pet lives on.
        ESP_LOGW(TAG, "QMI8658 init failed (%s); shake detection disabled",
                 esp_err_to_name(err));
        return ESP_OK;
    }

    // 8 g range leaves headroom above the 1.8 g shake threshold;
    // 250 Hz is far more than our 50 Hz polling needs.
    qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_250HZ);
    qmi8658_enable_accel(&s_imu, true);

    // qmi8658_read_accel() then returns milli-g (rest ≈ 1000 mg),
    // which is what SHAKE_THRESHOLD_MG is expressed in.
    qmi8658_set_accel_unit_mg(&s_imu, true);

    xTaskCreate(imu_task, "imu", 4096, NULL, 4, NULL);
    return ESP_OK;
}
