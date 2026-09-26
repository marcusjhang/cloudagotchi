/*
 * The pet: one struct, a handful of pure functions. No ESP-IDF headers on
 * purpose - this file compiles on the host (tools/sim/) so the rules can be
 * tested over simulated weeks in a millisecond.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PET_SCHEMA_VERSION 2  // v2 added the teen stage; persist rejects older blobs

typedef enum {
    PET_STAT_FULLNESS,
    PET_STAT_HAPPINESS,
    PET_STAT_ENERGY,
    PET_STAT_HYGIENE,
    PET_STAT_HEALTH,
    PET_STAT_COUNT
} pet_stat_t;

typedef enum {
    PET_STAGE_EGG,
    PET_STAGE_BABY,
    PET_STAGE_CHILD,
    PET_STAGE_TEEN,
    PET_STAGE_ADULT_GOOD,
    PET_STAGE_ADULT_BAD,
    PET_STAGE_DEAD,
} pet_stage_t;

// What the screen should show. Ordered by priority in pet_face().
typedef enum {
    PET_FACE_EGG,
    PET_FACE_IDLE,
    PET_FACE_HAPPY,
    PET_FACE_EATING,
    PET_FACE_PLAYING,
    PET_FACE_STARTLED,
    PET_FACE_SLEEPING,
    PET_FACE_SAD,
    PET_FACE_SICK,
    PET_FACE_DIRTY,
    PET_FACE_DEAD,
} pet_face_t;

typedef enum {
    PET_ACT_FEED_MEAL,
    PET_ACT_FEED_SNACK,
    PET_ACT_PLAY,
    PET_ACT_LIGHTS,     // toggle sleep
    PET_ACT_CLEAN,
    PET_ACT_MEDICINE,
    PET_ACT_SHAKE,      // the IMU, not a button
    PET_ACT_NEW_EGG,    // only when dead
    PET_ACT_COUNT
} pet_action_t;

typedef enum {
    PET_OK,
    PET_BLOCKED_EGG,
    PET_BLOCKED_DEAD,
    PET_BLOCKED_ASLEEP,
    PET_BLOCKED_FULL,
    PET_BLOCKED_TIRED,
    PET_BLOCKED_CLEAN,
    PET_BLOCKED_HEALTHY,
    PET_BLOCKED_ALIVE,
} pet_result_t;

typedef struct {
    uint32_t version;                     // PET_SCHEMA_VERSION; persist rejects others
    int64_t  born_at;                     // epoch seconds (game time)
    int64_t  updated_at;                  // last time decay was applied
    int32_t  stats_u[PET_STAT_COUNT];     // micro-points: 0..100 000 000 (see PT)
    uint8_t  stage;                       // pet_stage_t
    uint8_t  asleep;
    uint8_t  dirty;                       // poop on screen
    uint8_t  weight;
    uint16_t care_mistakes;
    int64_t  critical_since[PET_STAT_COUNT]; // 0 = fine; >0 = when it went red; -1 = mistake already counted
    int64_t  next_poop_at;
    int64_t  dirty_since;                 // 0 = clean
    int64_t  zero_health_since;
    int64_t  died_at;
    int64_t  transient_until;             // eating/playing/startled face shown until then
    uint8_t  transient_face;              // pet_face_t
    uint32_t actions_taken;
} pet_t;

void         pet_new(pet_t *p, int64_t now);
void         pet_apply(pet_t *p, int64_t now);                    // let time pass up to `now`
pet_result_t pet_act(pet_t *p, pet_action_t a, int64_t now);      // apply first, then act

// Would `pet_act` succeed right now? Pure; the UI greys out what it can't do.
bool         pet_action_enabled(const pet_t *p, pet_action_t a);
pet_face_t   pet_face(const pet_t *p, int64_t now);
int          pet_stat(const pet_t *p, pet_stat_t s);              // 0..100
int64_t      pet_age_s(const pet_t *p, int64_t now);
bool         pet_is_night(int64_t now);

const char  *pet_face_name(pet_face_t f);
const char  *pet_stage_name(pet_stage_t s);
const char  *pet_action_name(pet_action_t a);
const char  *pet_result_text(pet_result_t r);                     // short, for a toast
