/*
 * Every game tunable, in one place. Rates are points per hour on 0..100
 * stats. Each number carries the reason it is what it is; tools/sim/ prints
 * the timelines these produce, so change a number, re-run the sim, and look.
 *
 * Build with -DFAST_FORWARD=60 to run game time at 60x (a day in 24 min).
 */
#pragma once

#ifndef FAST_FORWARD
#define FAST_FORWARD 1
#endif

/* ---- time ------------------------------------------------------------- */
#define STEP_S                 60          // catch-up granularity; 7 days = 10080 steps, trivial
#define MAX_CATCHUP_S          (7 * 24 * 3600)  // a week away is "dead", further back changes nothing
#define CLOCK_VALID_EPOCH      1577836800LL     // 2020-01-01: earlier means "clock not set" (Phase 4 adds the RTC)
#define TZ_OFFSET_H            8           // Singapore
#define TZ_POSIX               "<+08>-8"    // the same zone as a POSIX TZ string
#define NIGHT_START_H          22          // pet will not auto-wake between these hours
#define NIGHT_END_H            7

/* ---- decay (points per hour, awake, non-baby) --------------------------- */
// Paced for a desk pet you visit ~3x a day: feed + play + clean each visit
// keeps every bar out of the red; skip a whole day and it is sad, skip two
// and it is sick. The sim's "good owner" scenario asserts this.
#define DECAY_FULLNESS_PER_H       6       // 100 -> red in ~12 h awake; overnight gap survives on a 19:00 meal
#define DECAY_HAPPINESS_PER_H      5
#define DECAY_ENERGY_AWAKE_PER_H   7       // ~11 h awake before it nods off on its own (energy is never "neglect")
#define GAIN_ENERGY_ASLEEP_PER_H   40      // 2.5 h nap refills it
#define DECAY_HYGIENE_PER_H        4
#define DECAY_HEALTH_PER_H         5       // while a core stat is critical, or poop has sat past its grace
#define RECOVER_HEALTH_PER_H       6       // only while every core stat is above HEALTH_RECOVER_ABOVE
#define BABY_DECAY_X100            120     // a baby needs 20 % more attention
#define ASLEEP_DECAY_X100          10      // sleep nearly pauses needs, like the original toy

/* ---- thresholds (points) ---------------------------------------------- */
#define CRITICAL_BELOW         25          // stat is "in the red": health drains, care-mistake clock runs
#define SAD_BELOW              25          // any core stat below this -> sad face
#define HAPPY_ABOVE            60          // all core stats above this -> happy face
#define SICK_BELOW             30          // health below this -> sick face, medicine allowed
#define HEALTH_RECOVER_ABOVE   50          // health only climbs while fullness/happiness/hygiene are all above this
#define AUTO_SLEEP_BELOW       20          // energy below this -> falls asleep by itself
#define PLAY_NEEDS_ENERGY      15
#define FULL_ABOVE             90          // refuses a meal above this (snacks always go down)

/* ---- timings (seconds) ------------------------------------------------ */
#define CARE_MISTAKE_S         (30 * 60)   // a stat left critical this long = one care mistake
#define POOP_INTERVAL_S        (5 * 3600)  // awake time after a clean-up until the next poop (~3 a day)
#define DIRTY_GRACE_S          (2 * 3600)  // poop is cosmetic until it has sat this long; then it counts as neglect
#define DEATH_AFTER_ZERO_HEALTH_S (4 * 3600)
#define EGG_HATCH_S            (5 * 60)
#define BABY_TO_CHILD_S        (24 * 3600)
#define CHILD_TO_ADULT_S       (4 * 24 * 3600)
#define ADULT_BAD_MISTAKES     4           // this many mistakes by adulthood -> the grumpy adult
#define TRANSIENT_S            2           // how long eating/playing/startled faces show

/* ---- action effects (points) ------------------------------------------ */
#define MEAL_FULLNESS          40
#define SNACK_FULLNESS         15
#define SNACK_HAPPINESS        10
#define PLAY_HAPPINESS         30
#define PLAY_ENERGY_COST       10
#define MEDICINE_HEALTH        30
#define SHAKE_HAPPINESS_AWAKE  2           // being shaken is not fun
#define SHAKE_HAPPINESS_ASLEEP 5           // being shaken awake is less fun
#define START_STAT             80
#define START_WEIGHT           10

/* ---- firmware --------------------------------------------------------- */
#define TICK_MS                1000
#define SAVE_INTERVAL_S        300         // plus a save on every action

/* ---- power (seconds of idle) ------------------------------------------- */
#define DIM_AFTER_S            30          // AMOLED burn-in: dim before the screen goes
#define SCREEN_OFF_AFTER_S     120         // then black; on battery this leads into light sleep
#define BATT_SAMPLE_S          600         // while asleep, log a battery sample this often (12 h ring)
