#include "journal.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "journal";
static uint32_t s_boots;

// Battery ring for the overnight soak. Small enough to rewrite cheaply every
// few minutes; read back by tools/journal_dump.py.
#define BATT_RING 24
typedef struct {
    int32_t up_s;   // uptime when sampled
    int16_t pct;    // fuel gauge %
    int16_t mv;     // VBAT
} batt_sample_t;
typedef struct {
    uint16_t head;
    uint16_t n;
    batt_sample_t s[BATT_RING];
} batt_ring_t;

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
    if (nvs_get_blob(h, "batt", &r, &len) != ESP_OK) {
        r.head = 0;
        r.n = 0;
    }
    r.s[r.head].up_s = (int32_t)(esp_timer_get_time() / 1000000);
    r.s[r.head].pct = (int16_t)percent;
    r.s[r.head].mv = (int16_t)millivolts;
    r.head = (uint16_t)((r.head + 1) % BATT_RING);
    if (r.n < BATT_RING) {
        r.n++;
    }
    nvs_set_blob(h, "batt", &r, sizeof(r));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "battery sample: %d%% %d mV", percent, millivolts);
}
