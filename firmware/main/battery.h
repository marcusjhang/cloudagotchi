/*
 * AXP2101 power management IC on the board's I2C bus: is there a cell, is
 * external power present, how full is it. The fuel gauge answers even with
 * no cell attached (reporting 0 %), so `present` gates the percentage.
 */
#pragma once

#include <stdbool.h>

typedef struct {
    bool present;     // a battery is attached (STATUS1 bit 3)
    bool vbus;        // external power present (STATUS1 bit 5)
    bool charging;    // actively charging (STATUS2 bits 7:5 == 001)
    int  percent;     // 0..100; meaningful only when `present`
    int  millivolts;  // VBAT in mV, or -1 when unreadable
} battery_status_t;

void battery_init(void);
bool battery_read(battery_status_t *out);  // false if the PMIC is not answering
