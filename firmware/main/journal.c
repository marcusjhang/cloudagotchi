#include "journal.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"

#include "batt_ring.h"

static const char *TAG = "journal";
static uint32_t s_boots;

static const char *reason_name(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "int-wdt";
    case ESP_RST_TASK_WDT:  return "task-wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "sdio";
    case ESP_RST_USB:       return "usb";
    case ESP_RST_JTAG:      return "jtag";
    default:                return "unknown";
    }
}

void journal_init(void)
{
    const esp_reset_reason_t reason = esp_reset_reason();
    nvs_handle_t h;
    if (nvs_open("journal", NVS_READWRITE, &h) == ESP_OK) {
        nvs_get_u32(h, "boots", &s_boots);
        s_boots++;
        nvs_set_u32(h, "boots", s_boots);
        nvs_set_u8(h, "last_rr", (uint8_t)reason);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "boot #%lu, reset reason: %s", (unsigned long)s_boots, reason_name(reason));
    journal_battery_dump();  // after a soak, plugging in reveals the night's curve
}

uint32_t journal_boots(void)
{
    return s_boots;
}

void journal_battery_sample(int percent, int millivolts)
{
    batt_ring_t r = {0};
    nvs_handle_t h;
    if (nvs_open("journal", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(r);
    if (nvs_get_blob(h, "batt", &r, &len) != ESP_OK || len != sizeof(r)) {
        r.head = 0;
        r.n = 0;  // missing or a different schema: start a fresh ring
    }
    batt_ring_append(&r, (int32_t)(esp_timer_get_time() / 1000000), percent, millivolts);
    nvs_set_blob(h, "batt", &r, sizeof(r));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "battery sample: %d%% %d mV", percent, millivolts);
}

void journal_battery_dump(void)
{
    batt_ring_t r = {0};
    nvs_handle_t h;
    if (nvs_open("journal", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(r);
    const bool ok = (nvs_get_blob(h, "batt", &r, &len) == ESP_OK && len == sizeof(r));
    nvs_close(h);
    if (!ok || r.n == 0) {
        return;
    }
    ESP_LOGI(TAG, "battery ring: %u samples, oldest first", (unsigned)r.n);
    for (uint16_t i = 0; i < r.n; i++) {
        const batt_sample_t *s = &r.s[batt_ring_index(&r, i)];
        const int32_t u = s->up_s;
        ESP_LOGI(TAG, "  +%ld:%02ld  %d%%  %d mV",
                 (long)(u / 3600), (long)((u / 60) % 60), s->pct, s->mv);
    }
}
