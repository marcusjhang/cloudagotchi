/*
 * The game loop: owns the pet, ticks it once a second, saves it, and pushes
 * snapshots to the UI. Actions come in from the LVGL task (buttons) and the
 * IMU task (shake); everything is serialised on one mutex.
 */
#pragma once

#include <stdint.h>

#include "pet.h"

void    game_start(void);          // load from NVS or lay a new egg; start the tick task
void    game_act(pet_action_t a);  // safe from any task; blocked actions show a toast
int64_t game_now(void);            // game-time epoch seconds (see FAST_FORWARD)
void    game_save_now(void);       // persist under the lock (call before sleeping)
void    game_reset(void);          // replace the pet with a fresh egg, whatever its stage
