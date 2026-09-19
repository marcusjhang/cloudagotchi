/*
 * Host-side unit tests for the pure hardware math in firmware/main/hw_math.h
 * (PCF85063A BCD + build-time parsing, AXP2101 VBAT). No ESP-IDF needed.
 *
 *   tools/check.sh
 */
#include <stdio.h>
#include <string.h>

#include "batt_ring.h"
#include "hw_math.h"

static int failures;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            failures++;                                   \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static void test_bcd(void)
{
    printf("[bcd] round-trips and known encodings\n");
    for (int v = 0; v <= 99; v++) {
        CHECK(bcd_dec(bcd_enc(v)) == v, "bcd round-trip %d", v);
    }
    CHECK(bcd_enc(0) == 0x00, "0 -> 0x00");
    CHECK(bcd_enc(9) == 0x09, "9 -> 0x09");
    CHECK(bcd_enc(10) == 0x10, "10 -> 0x10");
    CHECK(bcd_enc(59) == 0x59, "59 -> 0x59");
    CHECK(bcd_enc(99) == 0x99, "99 -> 0x99");
    CHECK(bcd_dec(0x19) == 19, "0x19 -> 19");
    CHECK(bcd_dec(0x99) == 99, "0x99 -> 99");
}

static void test_build_time(void)
{
    printf("[build time] __DATE__ / __TIME__ parsing\n");
    hm_civil_t t;

    hm_parse_build_time("Sep 19 2026", "15:24:14", &t);
    CHECK(t.year == 2026 && t.mon == 9 && t.day == 19, "date Sep 19 2026 -> %04d-%02d-%02d",
          t.year, t.mon, t.day);
    CHECK(t.hour == 15 && t.min == 24 && t.sec == 14, "time 15:24:14 -> %02d:%02d:%02d",
          t.hour, t.min, t.sec);

    hm_parse_build_time("Jan  3 2001", "00:00:00", &t);  // single-digit day is space-padded
    CHECK(t.year == 2001 && t.mon == 1 && t.day == 3, "Jan  3 2001 -> %04d-%02d-%02d",
          t.year, t.mon, t.day);

    hm_parse_build_time("Dec 31 2099", "23:59:59", &t);
    CHECK(t.year == 2099 && t.mon == 12 && t.day == 31, "Dec 31 2099 -> %04d-%02d-%02d",
          t.year, t.mon, t.day);
}

static void test_vbat(void)
{
    printf("[vbat] AXP2101 14-bit decode\n");
    CHECK(axp_vbat_mv(0x0F, 0xA0) == 4000, "0x0FA0 -> 4000 mV (got %d)", axp_vbat_mv(0x0F, 0xA0));
    CHECK(axp_vbat_mv(0x10, 0x00) == 4096, "0x1000 -> 4096 mV (got %d)", axp_vbat_mv(0x10, 0x00));
    CHECK(axp_vbat_mv(0x00, 0x00) == 0, "0x0000 -> 0 mV");
    CHECK(axp_vbat_mv(0xFF, 0xFF) == 16383, "high byte masks to 6 bits (got %d)",
          axp_vbat_mv(0xFF, 0xFF));
}

static void test_batt_ring(void)
{
    printf("[batt ring] append, wrap, oldest-first\n");
    static batt_ring_t r;

    memset(&r, 0, sizeof(r));
    CHECK(r.n == 0, "starts empty");
    batt_ring_append(&r, 10, 90, 4000);
    batt_ring_append(&r, 20, 80, 3900);
    CHECK(r.n == 2, "two samples counted");
    CHECK(r.s[batt_ring_index(&r, 0)].pct == 90, "oldest first");
    CHECK(r.s[batt_ring_index(&r, 1)].pct == 80, "newer second");

    memset(&r, 0, sizeof(r));
    for (int i = 0; i < BATT_RING + 3; i++) {
        batt_ring_append(&r, i, i % 100, 3000 + i);
    }
    CHECK(r.n == BATT_RING, "count clamps at the ring size");
    CHECK(r.head == 3, "head wraps modulo BATT_RING");
    CHECK(r.s[batt_ring_index(&r, 0)].up_s == 3, "index(0) is the oldest retained");
    CHECK(r.s[batt_ring_index(&r, BATT_RING - 1)].up_s == BATT_RING + 2, "last is newest");
}

int main(void)
{
    printf("hw_math host tests\n\n");
    test_bcd();
    test_build_time();
    test_vbat();
    test_batt_ring();
    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures,
           failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
