#include "journal.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"

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
}

uint32_t journal_boots(void)
{
    return s_boots;
}
