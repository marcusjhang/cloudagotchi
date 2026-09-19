/*
 * Battery-sample ring, kept pure so tools/tests can exercise the wrap and
 * ordering on the host. journal.c stores one of these in NVS.
 */
#pragma once

#include <stdint.h>

#define BATT_RING 72  // 12 h at BATT_SAMPLE_S (600 s) with a little headroom

typedef struct {
    int32_t up_s;  // uptime when sampled
    int16_t pct;   // fuel gauge %
    int16_t mv;    // VBAT
} batt_sample_t;

typedef struct {
    uint16_t head;  // next slot to write
    uint16_t n;     // how many valid samples (<= BATT_RING)
    batt_sample_t s[BATT_RING];
} batt_ring_t;

// Append one sample, wrapping and clamping the count. Returns the slot index
// the sample was written to.
static inline uint16_t batt_ring_append(batt_ring_t *r, int32_t up_s, int pct, int mv)
{
    const uint16_t at = r->head;
    r->s[at].up_s = up_s;
    r->s[at].pct = (int16_t)pct;
    r->s[at].mv = (int16_t)mv;
    r->head = (uint16_t)((r->head + 1) % BATT_RING);
    if (r->n < BATT_RING) {
        r->n++;
    }
    return at;
}

// Oldest-first index of the i-th valid sample (0 = oldest).
static inline uint16_t batt_ring_index(const batt_ring_t *r, uint16_t i)
{
    return (uint16_t)((r->head + BATT_RING - r->n + i) % BATT_RING);
}
