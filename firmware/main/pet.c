#include "pet.h"

#include <string.h>

#include "config.h"

// Stats are kept in micro-points so a one-second tick at 7 points/hour
// (1944 micro-points) rounds to nothing worth caring about. int32 holds it.
#define PT 1000000
#define MAX_U (100 * PT)

static int32_t clamp_u(int64_t v)
{
    return v < 0 ? 0 : v > MAX_U ? MAX_U : (int32_t)v;
}

static void add(pet_t *p, pet_stat_t s, int64_t delta_u)
{
    p->stats_u[s] = clamp_u((int64_t)p->stats_u[s] + delta_u);
}

// Micro-points lost/gained over dt seconds at `per_h` points per hour.
static int64_t rate_u(int per_h, int32_t dt)
{
    return (int64_t)per_h * PT * dt / 3600;
}

bool pet_is_night(int64_t now)
{
    if (now < CLOCK_VALID_EPOCH) {
        return false;  // no trustworthy clock: no night
    }
    const int h = (int)(((now + TZ_OFFSET_H * 3600) % 86400) / 3600);
    return h >= NIGHT_START_H || h < NIGHT_END_H;
}

void pet_new(pet_t *p, int64_t now)
{
    memset(p, 0, sizeof(*p));
    p->version = PET_SCHEMA_VERSION;
    p->born_at = now;
    p->updated_at = now;
    for (int s = 0; s < PET_STAT_COUNT; s++) {
        p->stats_u[s] = START_STAT * PT;
    }
    p->stats_u[PET_STAT_HEALTH] = MAX_U;
    p->stats_u[PET_STAT_HYGIENE] = MAX_U;
    p->stage = PET_STAGE_EGG;
    p->weight = START_WEIGHT;
    p->next_poop_at = now + EGG_HATCH_S + POOP_INTERVAL_S;
}

static void set_transient(pet_t *p, pet_face_t f, int64_t now)
{
    p->transient_face = (uint8_t)f;
    p->transient_until = now + TRANSIENT_S;
}

// One slice of time, dt seconds, ending at `now`.
static void step(pet_t *p, int64_t now, int32_t dt)
{
    if (p->stage == PET_STAGE_DEAD) {
        return;
    }

    /* -- growing up ------------------------------------------------------ */
    const int64_t age = now - p->born_at;
    if (p->stage == PET_STAGE_EGG) {
        if (age < EGG_HATCH_S) {
            return;  // eggs are not hungry
        }
        p->stage = PET_STAGE_BABY;
    }
    if (p->stage == PET_STAGE_BABY && age >= BABY_TO_CHILD_S) {
        p->stage = PET_STAGE_CHILD;
    }
    if (p->stage == PET_STAGE_CHILD && age >= CHILD_TO_TEEN_S) {
        p->stage = PET_STAGE_TEEN;
    }
    if (p->stage == PET_STAGE_TEEN && age >= TEEN_TO_ADULT_S) {
        p->stage = p->care_mistakes >= ADULT_BAD_MISTAKES ? PET_STAGE_ADULT_BAD
                                                          : PET_STAGE_ADULT_GOOD;
    }

    /* -- decay ------------------------------------------------------------ */
    const int need = p->stage == PET_STAGE_BABY ? BABY_DECAY_X100 : 100;
    const int awake = p->asleep ? ASLEEP_DECAY_X100 : 100;
    add(p, PET_STAT_FULLNESS,  -rate_u(DECAY_FULLNESS_PER_H,  dt) * need * awake / 10000);
    add(p, PET_STAT_THIRST,    -rate_u(DECAY_THIRST_PER_H,    dt) * need * awake / 10000);
    add(p, PET_STAT_HAPPINESS, -rate_u(DECAY_HAPPINESS_PER_H, dt) * need * awake / 10000);
    add(p, PET_STAT_HYGIENE,   -rate_u(DECAY_HYGIENE_PER_H,   dt) * awake / 100);
    if (p->asleep) {
        add(p, PET_STAT_ENERGY, rate_u(GAIN_ENERGY_ASLEEP_PER_H, dt));
    } else {
        add(p, PET_STAT_ENERGY, -rate_u(DECAY_ENERGY_AWAKE_PER_H, dt) * need / 100);
    }

    /* -- sleeping --------------------------------------------------------- */
    if (!p->asleep && pet_stat(p, PET_STAT_ENERGY) < AUTO_SLEEP_BELOW) {
        p->asleep = 1;
    } else if (p->asleep && p->stats_u[PET_STAT_ENERGY] >= MAX_U && !pet_is_night(now)) {
        p->asleep = 0;
    }

    /* -- poop ------------------------------------------------------------- */
    if (!p->dirty && now >= p->next_poop_at) {
        if (p->asleep) {
            p->next_poop_at = now + 15 * 60;  // holds it in while asleep; goes soon after waking
        } else {
            p->dirty = 1;
            p->dirty_since = now;
        }
    }

    /* -- neglect ---------------------------------------------------------- */
    // Energy is the pet's own business (it naps); health is the consequence.
    // Only fullness, happiness, hygiene and a poop left too long count as neglect.
    bool any_critical = p->dirty && now - p->dirty_since >= DIRTY_GRACE_S;
    bool all_fine = true;
    for (int s = 0; s < PET_STAT_HEALTH; s++) {
        if (s == PET_STAT_ENERGY) {
            continue;
        }
        if (pet_stat(p, (pet_stat_t)s) <= HEALTH_RECOVER_ABOVE) {
            all_fine = false;
        }
        if (pet_stat(p, (pet_stat_t)s) < CRITICAL_BELOW) {
            any_critical = true;
            if (p->critical_since[s] == 0) {
                p->critical_since[s] = now;
            } else if (p->critical_since[s] > 0 && now - p->critical_since[s] >= CARE_MISTAKE_S) {
                p->care_mistakes++;
                p->critical_since[s] = -1;  // one mistake per episode
            }
        } else {
            p->critical_since[s] = 0;
        }
    }

    /* -- health ----------------------------------------------------------- */
    if (any_critical) {
        add(p, PET_STAT_HEALTH, -rate_u(DECAY_HEALTH_PER_H, dt) * (p->asleep ? 50 : 100) / 100);
    } else if (all_fine && !p->dirty) {
        add(p, PET_STAT_HEALTH, rate_u(RECOVER_HEALTH_PER_H, dt));
    }

    /* -- death ------------------------------------------------------------ */
    if (p->stats_u[PET_STAT_HEALTH] <= 0) {
        if (p->zero_health_since == 0) {
            p->zero_health_since = now;
        } else if (now - p->zero_health_since >= DEATH_AFTER_ZERO_HEALTH_S) {
            p->stage = PET_STAGE_DEAD;
            p->died_at = now;
            p->asleep = 0;
        }
    } else {
        p->zero_health_since = 0;
    }
}

void pet_apply(pet_t *p, int64_t now)
{
    if (p->updated_at < CLOCK_VALID_EPOCH && now >= CLOCK_VALID_EPOCH) {
        // Born before the RTC existed (Phase 3): its timestamps are uptime
        // seconds near zero. Re-anchor the timeline to real time instead of
        // charging it for the decades that would otherwise appear to pass.
        const int64_t shift = now - p->updated_at;
        p->born_at += shift;
        p->updated_at += shift;
        for (int s = 0; s < PET_STAT_COUNT; s++) {
            if (p->critical_since[s] > 0) p->critical_since[s] += shift;
        }
        if (p->next_poop_at > 0)  p->next_poop_at += shift;
        if (p->dirty_since > 0)   p->dirty_since += shift;
        if (p->zero_health_since > 0) p->zero_health_since += shift;
        if (p->died_at > 0)       p->died_at += shift;
        if (p->transient_until > 0) p->transient_until += shift;
    }
    if (now < p->updated_at) {
        // Clock went backwards (RTC reset, or a build time in the past):
        // resync without charging the pet for time that did not pass.
        p->updated_at = now;
        return;
    }
    int64_t elapsed = now - p->updated_at;
    if (elapsed > MAX_CATCHUP_S) {
        p->updated_at = now - MAX_CATCHUP_S;
        elapsed = MAX_CATCHUP_S;
    }
    while (elapsed > 0) {
        const int32_t dt = elapsed > STEP_S ? STEP_S : (int32_t)elapsed;
        p->updated_at += dt;
        step(p, p->updated_at, dt);
        elapsed -= dt;
    }
}

pet_result_t pet_act(pet_t *p, pet_action_t a, int64_t now)
{
    pet_apply(p, now);

    if (a == PET_ACT_NEW_EGG) {
        if (p->stage != PET_STAGE_DEAD) {
            return PET_BLOCKED_ALIVE;
        }
        pet_new(p, now);
        return PET_OK;
    }
    if (p->stage == PET_STAGE_DEAD) {
        return PET_BLOCKED_DEAD;
    }
    if (p->stage == PET_STAGE_EGG) {
        return a == PET_ACT_SHAKE ? PET_OK : PET_BLOCKED_EGG;  // a shaken egg just wobbles
    }

    switch (a) {
    case PET_ACT_FEED_MEAL:
        if (p->asleep) return PET_BLOCKED_ASLEEP;
        if (pet_stat(p, PET_STAT_FULLNESS) > FULL_ABOVE) return PET_BLOCKED_FULL;
        add(p, PET_STAT_FULLNESS, (int64_t)MEAL_FULLNESS * PT);
        set_transient(p, PET_FACE_EATING, now);
        break;
    case PET_ACT_DRINK:
        if (p->asleep) return PET_BLOCKED_ASLEEP;
        if (pet_stat(p, PET_STAT_THIRST) > WATER_FULL_ABOVE) return PET_BLOCKED_FULL;
        add(p, PET_STAT_THIRST, (int64_t)DRINK_THIRST * PT);
        set_transient(p, PET_FACE_EATING, now);
        break;
    case PET_ACT_PLAY:
        if (p->asleep) return PET_BLOCKED_ASLEEP;
        if (pet_stat(p, PET_STAT_ENERGY) < PLAY_NEEDS_ENERGY) return PET_BLOCKED_TIRED;
        add(p, PET_STAT_HAPPINESS, (int64_t)PLAY_HAPPINESS * PT);
        add(p, PET_STAT_ENERGY, -(int64_t)PLAY_ENERGY_COST * PT);
        if (p->weight > START_WEIGHT / 2) p->weight--;
        set_transient(p, PET_FACE_PLAYING, now);
        break;
    case PET_ACT_LIGHTS:
        p->asleep = !p->asleep;
        p->transient_until = 0;
        break;
    case PET_ACT_CLEAN:
        if (!p->dirty && p->stats_u[PET_STAT_HYGIENE] >= MAX_U) return PET_BLOCKED_CLEAN;
        p->dirty = 0;
        p->dirty_since = 0;
        p->next_poop_at = now + POOP_INTERVAL_S;
        p->stats_u[PET_STAT_HYGIENE] = MAX_U;
        set_transient(p, PET_FACE_HAPPY, now);
        break;
    case PET_ACT_MEDICINE:
        if (pet_stat(p, PET_STAT_HEALTH) >= SICK_BELOW) return PET_BLOCKED_HEALTHY;
        add(p, PET_STAT_HEALTH, (int64_t)MEDICINE_HEALTH * PT);
        set_transient(p, PET_FACE_HAPPY, now);
        break;
    case PET_ACT_SHAKE:
        if (p->asleep) {
            p->asleep = 0;
            add(p, PET_STAT_HAPPINESS, -(int64_t)SHAKE_HAPPINESS_ASLEEP * PT);
        } else {
            add(p, PET_STAT_HAPPINESS, -(int64_t)SHAKE_HAPPINESS_AWAKE * PT);
        }
        set_transient(p, PET_FACE_STARTLED, now);
        break;
    default:
        break;
    }
    p->actions_taken++;
    return PET_OK;
}

// Mirrors the guards in pet_act() so the UI can grey out what would be blocked
// (same thresholds, one source of truth). Pure: does not touch the pet.
bool pet_action_enabled(const pet_t *p, pet_action_t a)
{
    if (a == PET_ACT_NEW_EGG) {
        return p->stage == PET_STAGE_DEAD;
    }
    if (p->stage == PET_STAGE_DEAD) {
        return false;
    }
    if (p->stage == PET_STAGE_EGG) {
        return a == PET_ACT_SHAKE;
    }
    switch (a) {
    case PET_ACT_FEED_MEAL:
        return !p->asleep && pet_stat(p, PET_STAT_FULLNESS) <= FULL_ABOVE;
    case PET_ACT_DRINK:
        return !p->asleep && pet_stat(p, PET_STAT_THIRST) <= WATER_FULL_ABOVE;
    case PET_ACT_PLAY:
        return !p->asleep && pet_stat(p, PET_STAT_ENERGY) >= PLAY_NEEDS_ENERGY;
    case PET_ACT_LIGHTS:
        return true;
    case PET_ACT_CLEAN:
        return p->dirty || p->stats_u[PET_STAT_HYGIENE] < MAX_U;
    case PET_ACT_MEDICINE:
        return pet_stat(p, PET_STAT_HEALTH) < SICK_BELOW;
    case PET_ACT_SHAKE:
        return true;
    default:
        return false;
    }
}

// One need, most urgent first. Asleep/egg/dead are not "needs" to nag about.
pet_need_t pet_need(const pet_t *p)
{
    if (p->stage == PET_STAGE_EGG || p->stage == PET_STAGE_DEAD || p->asleep) {
        return PET_NEED_NONE;
    }
    if (pet_stat(p, PET_STAT_HEALTH) < SICK_BELOW) return PET_NEED_MED;
    if (p->dirty) return PET_NEED_CLEAN;
    if (pet_stat(p, PET_STAT_FULLNESS) < CRITICAL_BELOW) return PET_NEED_FOOD;
    if (pet_stat(p, PET_STAT_THIRST) < CRITICAL_BELOW) return PET_NEED_WATER;
    if (pet_stat(p, PET_STAT_ENERGY) < SLEEP_BELOW) return PET_NEED_SLEEP;
    if (pet_stat(p, PET_STAT_HAPPINESS) < CRITICAL_BELOW) return PET_NEED_FUN;
    return PET_NEED_NONE;
}

const char *pet_need_name(pet_need_t n)
{
    switch (n) {
    case PET_NEED_MED:   return "sick";
    case PET_NEED_CLEAN: return "clean";
    case PET_NEED_FOOD:  return "food";
    case PET_NEED_WATER: return "water";
    case PET_NEED_SLEEP: return "sleep";
    case PET_NEED_FUN:   return "fun";
    default:             return "ok";
    }
}

pet_face_t pet_face(const pet_t *p, int64_t now)
{
    if (p->stage == PET_STAGE_DEAD) return PET_FACE_DEAD;
    if (p->stage == PET_STAGE_EGG) return PET_FACE_EGG;
    if (now < p->transient_until) return (pet_face_t)p->transient_face;
    if (p->asleep) return PET_FACE_SLEEPING;
    if (pet_stat(p, PET_STAT_HEALTH) < SICK_BELOW) return PET_FACE_SICK;

    bool all_good = true;
    for (int s = 0; s < PET_STAT_HEALTH; s++) {
        if (s == PET_STAT_ENERGY) continue;  // tiredness shows as a nap, not a mood
        const int v = pet_stat(p, (pet_stat_t)s);
        if (v < SAD_BELOW) return PET_FACE_SAD;
        if (v <= HAPPY_ABOVE) all_good = false;
    }
    if (p->dirty) return PET_FACE_DIRTY;  // the UI also draws the poop itself
    return all_good ? PET_FACE_HAPPY : PET_FACE_IDLE;
}

int pet_stat(const pet_t *p, pet_stat_t s)
{
    return (p->stats_u[s] + PT / 2) / PT;
}

int64_t pet_age_s(const pet_t *p, int64_t now)
{
    return now - p->born_at;
}

const char *pet_face_name(pet_face_t f)
{
    static const char *const names[] = {"egg", "ok", "happy", "eating", "playing", "startled",
                                        "sleeping", "sad", "sick", "dirty", "dead"};
    return f < sizeof(names) / sizeof(names[0]) ? names[f] : "?";
}

const char *pet_stage_name(pet_stage_t s)
{
    static const char *const names[] = {"egg", "baby", "child", "teen", "adult", "grump", "dead"};
    return s < sizeof(names) / sizeof(names[0]) ? names[s] : "?";
}

const char *pet_action_name(pet_action_t a)
{
    static const char *const names[] = {"meal", "water", "play", "lights", "clean",
                                        "medicine", "shake", "new egg"};
    return a < sizeof(names) / sizeof(names[0]) ? names[a] : "?";
}

const char *pet_result_text(pet_result_t r)
{
    switch (r) {
    case PET_OK:              return "ok";
    case PET_BLOCKED_EGG:     return "still an egg";
    case PET_BLOCKED_DEAD:    return "...";
    case PET_BLOCKED_ASLEEP:  return "zzz";
    case PET_BLOCKED_FULL:    return "not hungry";
    case PET_BLOCKED_TIRED:   return "too tired";
    case PET_BLOCKED_CLEAN:   return "already clean";
    case PET_BLOCKED_HEALTHY: return "not sick";
    case PET_BLOCKED_ALIVE:   return "still alive";
    }
    return "?";
}
