#include "battery.h"

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "hw_math.h"

static const char *TAG = "battery";

#define AXP2101_ADDR     0x34
#define REG_STATUS1      0x00  // bit 3: battery present · bit 5: VBUS good
#define REG_STATUS2      0x01  // bits 7:5: charge direction (001 = charging)
#define REG_VBAT_H       0x34
#define REG_VBAT_L       0x35
#define REG_BAT_PERCENT  0xA4

static i2c_master_dev_handle_t s_dev;
static bool s_ok;

static esp_err_t read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, 50);
}

void battery_init(void)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &cfg, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 not on the bus; no battery readout");
        return;
    }
    uint8_t id = 0;
    if (read_reg(REG_STATUS1, &id) != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 not answering; no battery readout");
        return;
    }
    s_ok = true;
}

bool battery_read(battery_status_t *out)
{
    if (!s_ok) {
        return false;
    }
    uint8_t st1 = 0, st2 = 0, pct = 0, vh = 0, vl = 0;
    if (read_reg(REG_STATUS1, &st1) != ESP_OK ||
        read_reg(REG_STATUS2, &st2) != ESP_OK ||
        read_reg(REG_BAT_PERCENT, &pct) != ESP_OK ||
        read_reg(REG_VBAT_H, &vh) != ESP_OK ||
        read_reg(REG_VBAT_L, &vl) != ESP_OK) {
        return false;
    }

    out->present = (st1 >> 3) & 0x1;
    out->vbus = (st1 >> 5) & 0x1;
    out->charging = ((st2 >> 5) & 0x7) == 0x1;
    out->percent = pct <= 100 ? pct : 100;
    out->millivolts = axp_vbat_mv(vh, vl);
    return true;
}
