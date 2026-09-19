/*
 * The screen. Renders whatever snapshot the game last pushed; turns touches
 * into pet_action_t via the callback. Knows nothing about the rules.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "battery.h"
#include "pet.h"

typedef struct {
    uint8_t     stats[PET_STAT_COUNT];  // 0..100
    pet_face_t  face;
    pet_stage_t stage;
    bool        asleep;
    bool        dirty;
    uint32_t    age_s;
    uint8_t     weight;
    uint16_t    mistakes;
    uint32_t    boots;
    int64_t     clock_s;                // wall clock; shown when clock_ok
    bool        clock_ok;
    bool        batt_ok;
    battery_status_t batt;
    bool        enabled[PET_ACT_COUNT]; // pet_action_enabled(): grey out the rest
} pet_ui_snapshot_t;

typedef void (*pet_ui_action_cb_t)(pet_action_t a);

void pet_ui_start(pet_ui_action_cb_t on_action);   // brings up the display; call first
void pet_ui_update(const pet_ui_snapshot_t *s);     // safe from any task
void pet_ui_toast(const char *msg);                 // safe from any task; ~1.5 s
