#include "rtc.h"

#include <sys/time.h>
#include <time.h>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "hw_math.h"

static const char *TAG = "rtc";

#define PCF85063_ADDR  0x51
#define REG_SECONDS    0x04  // bit 7 = OS (oscillator stopped): time is invalid

static i2c_master_dev_handle_t s_dev;
static bool s_ok;

static esp_err_t read_time(hm_civil_t *out, bool *os_flag)
{
    const uint8_t reg = REG_SECONDS;
    uint8_t b[7];  // sec, min, hour, day, weekday, month, year
    const esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, b, sizeof(b), 50);
    if (err != ESP_OK) {
        return err;
    }
    *os_flag = (b[0] & 0x80) != 0;
    out->sec  = bcd_dec(b[0] & 0x7F);
    out->min  = bcd_dec(b[1] & 0x7F);
    out->hour = bcd_dec(b[2] & 0x3F);
    out->day  = bcd_dec(b[3] & 0x3F);
    out->mon  = bcd_dec(b[5] & 0x1F);
    out->year = 2000 + bcd_dec(b[6]);
    return ESP_OK;
}

// Writing the seconds byte also clears the oscillator-stop flag.
static esp_err_t write_time(const hm_civil_t *t)
{
    const uint8_t buf[8] = {
        REG_SECONDS,
        bcd_enc(t->sec), bcd_enc(t->min), bcd_enc(t->hour),
        bcd_enc(t->day), 0 /* weekday, unused */,
        bcd_enc(t->mon), bcd_enc(t->year - 2000),
    };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
}

static void seed_system_clock(const hm_civil_t *t)
{
    setenv("TZ", TZ_POSIX, 1);
    tzset();
    struct tm seed = {
        .tm_year = t->year - 1900, .tm_mon = t->mon - 1, .tm_mday = t->day,
        .tm_hour = t->hour, .tm_min = t->min, .tm_sec = t->sec, .tm_isdst = -1,
    };
    const time_t epoch = mktime(&seed);
    if (epoch > 0) {
        const struct timeval tv = {.tv_sec = epoch};
        settimeofday(&tv, NULL);
    }
}

void pcf85063_init(void)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_ADDR,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &cfg, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063A not on the bus; running without a clock");
        return;
    }

    hm_civil_t now;
    bool os = false;
    if (read_time(&now, &os) != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063A not answering; running without a clock");
        return;
    }

    if (os || now.year < 2020) {
        hm_civil_t bt;
        hm_parse_build_time(__DATE__, __TIME__, &bt);
        ESP_LOGW(TAG, "clock unset (os=%d, year %d): setting from build time", os, now.year);
        if (write_time(&bt) != ESP_OK) {
            return;
        }
        now = bt;
    }

    seed_system_clock(&now);
    s_ok = true;
    ESP_LOGI(TAG, "time %04d-%02d-%02d %02d:%02d:%02d (%s)",
             now.year, now.mon, now.day, now.hour, now.min, now.sec, TZ_POSIX);
}

int64_t pcf85063_now(void)
{
    if (!s_ok) {
        return -1;
    }
    hm_civil_t now;
    bool os = false;
    if (read_time(&now, &os) != ESP_OK || os) {
        return -1;
    }
    struct tm t = {
        .tm_year = now.year - 1900, .tm_mon = now.mon - 1, .tm_mday = now.day,
        .tm_hour = now.hour, .tm_min = now.min, .tm_sec = now.sec, .tm_isdst = -1,
    };
    return (int64_t)mktime(&t);
}

bool pcf85063_ok(void)
{
    return s_ok;
}
