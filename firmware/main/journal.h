/*
 * Black box for a device that is usually unplugged: boot count and last
 * reset reason in NVS, printed at boot. Grows in Phase 4 (sleep entries,
 * battery samples).
 */
#pragma once

#include <stdint.h>

void     journal_init(void);
uint32_t journal_boots(void);
