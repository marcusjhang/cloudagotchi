#include "power.h"

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.h"
#include "game.h"

static const char *TAG = "power";

#define IDLE_TICK_MS   500
#define SLEEP_SLICE_US 300000  // 300 ms; light sleep never gets long stretches
                                // because the expander/VBUS still need polling

static const gpio_num_t BOOT_GPIO = GPIO_NUM_0;
static const gpio_num_t TOUCH_INT = (gpio_num_t)BSP_LCD_TOUCH_INT;  // GPIO21

static int64_t s_last_activity;
static bool s_dimmed;
static bool s_screen_off;
static int s_awake_pct = 100;  // what screen_on() restores; see power_set_awake_brightness

static void screen_brightness(int pct)
{
    bsp_display_lock(0);
    bsp_display_brightness_set(pct);
    bsp_display_unlock();
}

static void screen_on(void)
{
    if (!s_dimmed && !s_screen_off) {
        return;
    }
    screen_brightness(s_awake_pct);
    s_dimmed = false;
    s_screen_off = false;
    ESP_LOGI(TAG, "screen on");
}

void power_set_awake_brightness(int pct)
{
    if (pct < 10)  pct = 10;
    if (pct > 100) pct = 100;
    s_awake_pct = pct;
    if (!s_screen_off) {
        screen_brightness(pct);
    }
}

static void screen_off(void)
{
    if (s_screen_off) {
        return;
    }
    screen_brightness(0);
    s_screen_off = true;
    s_dimmed = false;
    ESP_LOGI(TAG, "screen off");
}

void power_note_activity(void)
{
    s_last_activity = esp_timer_get_time();
    if (s_screen_off || s_dimmed) {
        screen_on();
    }
}

// On battery: persist, then sleep in short slices until something wakes us.
// Every wake path ends in esp_restart(), so the board re-enters its known-good
// boot (panel reset, RTC seed, catch-up) rather than resuming stale state.
static void sleep_now(void)
{
    ESP_LOGW(TAG, "on battery: sleeping (touch, BOOT or plugging in wakes)");
    game_save_now();
    screen_off();

    // BOOT is a plain GPIO: wake instantly on press (active low).
    gpio_config_t boot = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&boot);
    gpio_wakeup_enable(BOOT_GPIO, GPIO_INTR_LOW_LEVEL);

    // Touch has no interrupt of its own routed to the CPU, but its INT pin is
    // on GPIO21 and pulses low on contact: enough for a GPIO wake.
    gpio_wakeup_enable(TOUCH_INT, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    bool was_vbus = false;
    battery_status_t b;
    if (battery_read(&b)) {
        was_vbus = b.vbus;
    }

    for (;;) {
        esp_sleep_enable_timer_wakeup(SLEEP_SLICE_US);
        const esp_err_t slept = esp_light_sleep_start();

        if (slept == ESP_OK && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
            ESP_LOGW(TAG, "woken by touch or BOOT");
            break;
        }
        if (gpio_get_level(BOOT_GPIO) == 0) {
            ESP_LOGW(TAG, "woken by BOOT");
            break;
        }
        // USB plugged in is the one wake we want to be sure about: it is how
        // the owner charges the pet, and it means doze instead of sleep next.
        if (battery_read(&b)) {
            if (b.vbus && !was_vbus) {
                ESP_LOGW(TAG, "woken by USB power");
                break;
            }
            was_vbus = b.vbus;
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // let the idle task breathe
    }

    game_save_now();
    esp_restart();
}

static void power_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(IDLE_TICK_MS));

        const int64_t idle_s = (esp_timer_get_time() - s_last_activity) / 1000000;

        if (idle_s < DIM_AFTER_S) {
            screen_on();
            continue;
        }
        if (idle_s < SCREEN_OFF_AFTER_S) {
            if (!s_dimmed && !s_screen_off) {
                screen_brightness(20);
                s_dimmed = true;
                ESP_LOGI(TAG, "idle %ds: dim", (int)idle_s);
            }
            continue;
        }

        battery_status_t b;
        const bool pmic_ok = battery_read(&b);
        const bool vbus = !pmic_ok || b.vbus;  // unknown PMIC: assume powered

        if (vbus) {
            // Doze: screen off, CPU up, USB alive. Any activity brings it back.
            if (!s_screen_off) {
                screen_off();
                ESP_LOGW(TAG, "idle %ds: dozing on USB", (int)idle_s);
            }
            continue;
        }

        sleep_now();  // does not return
    }
}

void power_start(void)
{
    s_last_activity = esp_timer_get_time();
    xTaskCreatePinnedToCore(power_task, "power", 4096, NULL, 2, NULL, 0);
}

bool power_dozing(void)
{
    return s_screen_off;
}
