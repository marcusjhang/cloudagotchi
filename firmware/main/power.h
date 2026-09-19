/*
 * Idle and power: dim after a while, screen off after a while longer, and
 * on battery go into light sleep (waking by touch, BOOT or a USB plug-in).
 * On USB we only doze — light sleep suspends the USB peripheral, which drops
 * the serial port and makes the board unflashable until it is reset.
 */
#pragma once

#include <stdbool.h>

void power_start(void);          // starts the idle/sleep task; call once
void power_note_activity(void);  // touch, button or shake: reset the idle timer
bool power_dozing(void);         // true while the screen is off on USB power
