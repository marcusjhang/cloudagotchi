/*
 * PCF85063A real-time clock on the board's I2C bus. It keeps time on the
 * coin cell while the chip is off, so on boot we seed the system clock from
 * it and the game's catch-up (pet_apply) sees the real dates that passed.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Adds the RTC to the I2C bus. If it has never been set (oscillator-stop
// flag) it is set from the build time. Then seeds time()/settimeofday and
// sets TZ. Safe to call once, early, before game_start().
// (Named pcf85063_* because ESP-IDF already owns the symbol `rtc_init`.)
void pcf85063_init(void);

// Chip time as epoch seconds, or -1 if the RTC is not answering.
int64_t pcf85063_now(void);

bool pcf85063_ok(void);
