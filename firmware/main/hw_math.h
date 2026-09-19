/*
 * Pure bit twiddling shared by the hardware drivers, kept free of ESP-IDF
 * headers so tools/tests/ can compile and check it on the host.
 */
#pragma once

#include <stdint.h>
#include <string.h>

/* ---- PCF85063A: BCD registers ------------------------------------------ */

static inline uint8_t bcd_enc(int v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static inline int bcd_dec(uint8_t v)
{
    return ((v >> 4) & 0x0F) * 10 + (v & 0x0F);
}

typedef struct {
    int year, mon, day, hour, min, sec;  // civil local time, year 2000+
} hm_civil_t;

// Compile-time `__DATE__` ("Mmm dd yyyy") and `__TIME__` ("hh:mm:ss") to
// civil fields. Passed in rather than baked, so the host can test it.
static inline void hm_parse_build_time(const char *date, const char *time,
                                       hm_civil_t *out)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char key[4] = {date[0], date[1], date[2], 0};
    const char *m = strstr(months, key);

    out->mon  = m ? (int)(m - months) / 3 + 1 : 1;
    out->day  = (date[4] == ' ' ? 0 : (date[4] - '0') * 10) + (date[5] - '0');
    out->year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 +
                (date[9] - '0') * 10 + (date[10] - '0');
    out->hour = (time[0] - '0') * 10 + (time[1] - '0');
    out->min  = (time[3] - '0') * 10 + (time[4] - '0');
    out->sec  = (time[6] - '0') * 10 + (time[7] - '0');
}

/* ---- AXP2101: battery ADC ---------------------------------------------- */

// VBAT is a 14-bit value split over two registers, high byte masked to 6 bits.
static inline int axp_vbat_mv(uint8_t vh, uint8_t vl)
{
    return (int)((((unsigned)(vh & 0x3F)) << 8) | vl);
}
