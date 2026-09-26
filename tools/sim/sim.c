/*
 * Host-side simulator for the game rules in firmware/main/pet.c.
 *
 * Runs scripted owners against the real pet code over simulated weeks and
 * asserts the pacing claims in config.h. Prints timelines so tuning a number
 * is "change it, run this, read the numbers" rather than waiting a day.
 *
 *   tools/sim/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "pet.h"

#define T0 1758153600LL  // 2026-09-18 00:00 UTC (08:00 SGT): clock counts as valid
#define H 3600LL
#define D (24 * H)

static int failures;
#define CHECK(cond, ...)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                      \
            printf("\n");                                             \
        }                                                             \
    } while (0)

static const char *hm(int64_t s)
{
    static char buf[4][32];
    static int i;
    char *b = buf[i++ & 3];
    snprintf(b, 32, "%lldd %02lldh %02lldm", s / D, (s % D) / H, (s % H) / 60);
    return b;
}

static int hour_sgt(int64_t t) { return (int)(((t + TZ_OFFSET_H * H) % D) / H); }
static int minute_sgt(int64_t t) { return (int)(((t + TZ_OFFSET_H * H) % H) / 60); }

/* -- 1. total neglect: how long until sad / sick / dead ------------------ */
static void scenario_neglect(void)
{
    printf("[neglect] never touched after hatching\n");
    pet_t p;
    pet_new(&p, T0);
    int64_t sad = 0, sick = 0, dead = 0, slept = 0;
    for (int64_t t = T0; t < T0 + 7 * D; t += 60) {
        pet_apply(&p, t);
        const pet_face_t f = pet_face(&p, t);
        if (!slept && p.asleep) slept = t - T0;
        if (!sad && f == PET_FACE_SAD) sad = t - T0;
        if (!sick && f == PET_FACE_SICK) sick = t - T0;
        if (f == PET_FACE_DEAD) { dead = t - T0; break; }
    }
    printf("  sad at %s, first nap at %s, sick at %s, dead at %s, mistakes %d\n",
           hm(sad), hm(slept), hm(sick), hm(dead), p.care_mistakes);
    CHECK(sad > 4 * H && sad < 12 * H, "sad should land in 4-12 h (a working day of neglect), got %s", hm(sad));
    CHECK(sick > sad, "sick after sad");
    CHECK(dead > 18 * H && dead < 60 * H, "dead should land in 18-60 h, got %s", hm(dead));
}

/* -- 2. good owner: three visits a day, lights out at night --------------- */
typedef struct { int min_stat[PET_STAT_COUNT]; int faces[PET_FACE_DEAD + 1]; int blocked; } tally_t;

static void visit(pet_t *p, int64_t t, tally_t *k)
{
    pet_result_t r;
    if (p->asleep) pet_act(p, PET_ACT_LIGHTS, t);
    if (p->dirty) pet_act(p, PET_ACT_CLEAN, t);
    if (pet_stat(p, PET_STAT_HEALTH) < SICK_BELOW) pet_act(p, PET_ACT_MEDICINE, t);
    r = pet_act(p, PET_ACT_FEED_MEAL, t);  if (r != PET_OK) k->blocked++;
    r = pet_act(p, PET_ACT_PLAY, t);       if (r != PET_OK) k->blocked++;
    if (pet_stat(p, PET_STAT_THIRST) < 70) pet_act(p, PET_ACT_DRINK, t);
}

static void scenario_good_owner(int days)
{
    printf("[good owner] visits 08:00 / 13:00 / 19:00, lights out 23:00, for %d days\n", days);
    pet_t p;
    pet_new(&p, T0);
    tally_t k = {0};
    for (int s = 0; s < PET_STAT_COUNT; s++) k.min_stat[s] = 100;
    int64_t first_sad = 0;
    for (int64_t t = T0; t < T0 + days * D; t += 60) {
        pet_apply(&p, t);
        const int hh = hour_sgt(t), mm = minute_sgt(t);
        if (mm == 0 && (hh == 8 || hh == 13 || hh == 19)) visit(&p, t, &k);
        if (mm == 0 && hh == 23 && !p.asleep) pet_act(&p, PET_ACT_LIGHTS, t);
        const pet_face_t f = pet_face(&p, t);
        k.faces[f]++;
        if (f == PET_FACE_SAD && !first_sad) first_sad = t - T0;
        for (int s = 0; s < PET_STAT_COUNT; s++)
            if (pet_stat(&p, (pet_stat_t)s) < k.min_stat[s]) k.min_stat[s] = pet_stat(&p, (pet_stat_t)s);
    }
    const int total = days * 24 * 60;
    printf("  end: stage %s, mistakes %d, weight %d, blocked actions %d\n",
           pet_stage_name((pet_stage_t)p.stage), p.care_mistakes, p.weight, k.blocked);
    printf("  min stats: full %d  happy %d  energy %d  hygiene %d  health %d\n",
           k.min_stat[0], k.min_stat[1], k.min_stat[2], k.min_stat[3], k.min_stat[4]);
    printf("  time shown: happy %d%%  ok %d%%  sleeping %d%%  sad %d%%  dirty %d%%  sick %d%%\n",
           k.faces[PET_FACE_HAPPY] * 100 / total, k.faces[PET_FACE_IDLE] * 100 / total,
           k.faces[PET_FACE_SLEEPING] * 100 / total, k.faces[PET_FACE_SAD] * 100 / total,
           k.faces[PET_FACE_DIRTY] * 100 / total, k.faces[PET_FACE_SICK] * 100 / total);
    CHECK(p.stage != PET_STAGE_DEAD, "a well-kept pet must not die");
    CHECK(p.stage == PET_STAGE_ADULT_GOOD, "should be the good adult by day %d, is %s", days,
          pet_stage_name((pet_stage_t)p.stage));
    CHECK(p.care_mistakes < ADULT_BAD_MISTAKES, "mistakes %d", p.care_mistakes);
    CHECK(k.faces[PET_FACE_SAD] * 100 / total < 5, "sad %d%% of the time is a chore",
          k.faces[PET_FACE_SAD] * 100 / total);
    CHECK(k.faces[PET_FACE_SICK] == 0, "never sick with three visits a day");
    CHECK(k.faces[PET_FACE_HAPPY] * 100 / total > 25, "happy only %d%% of the time",
          k.faces[PET_FACE_HAPPY] * 100 / total);
}

/* -- 3. lazy owner: two visits a day -> alive but grumpy ------------------- */
static void scenario_lazy_owner(int days)
{
    printf("[lazy owner] visits 08:00 / 20:00 only, for %d days\n", days);
    pet_t p;
    pet_new(&p, T0);
    tally_t k = {0};
    for (int64_t t = T0; t < T0 + days * D; t += 60) {
        pet_apply(&p, t);
        if (minute_sgt(t) == 0 && (hour_sgt(t) == 8 || hour_sgt(t) == 20)) visit(&p, t, &k);
        if (minute_sgt(t) == 0 && hour_sgt(t) == 23 && !p.asleep) pet_act(&p, PET_ACT_LIGHTS, t);
        k.faces[pet_face(&p, t)]++;
    }
    printf("  end: stage %s, mistakes %d, sad %d%%, sick %d%% of the time\n",
           pet_stage_name((pet_stage_t)p.stage), p.care_mistakes,
           k.faces[PET_FACE_SAD] * 100 / (days * 24 * 60),
           k.faces[PET_FACE_SICK] * 100 / (days * 24 * 60));
    CHECK(p.stage != PET_STAGE_DEAD, "two visits a day should keep it alive");
    CHECK(p.care_mistakes >= ADULT_BAD_MISTAKES, "two visits a day should earn the grumpy adult");
}

/* -- 3b. neglectful owner: one visit a day -> it does not make it ---------- */
static void scenario_one_visit(int days)
{
    printf("[one visit] 20:00 only, for %d days\n", days);
    pet_t p;
    pet_new(&p, T0);
    tally_t k = {0};
    int64_t dead = 0;
    for (int64_t t = T0; t < T0 + days * D; t += 60) {
        pet_apply(&p, t);
        if (minute_sgt(t) == 0 && hour_sgt(t) == 20) visit(&p, t, &k);
        if (pet_face(&p, t) == PET_FACE_DEAD) { dead = t - T0; break; }
    }
    printf("  %s\n", dead ? hm(dead) : "survived");
    CHECK(dead > 2 * D && dead < days * D, "one visit a day should end in 2-%d days, got %s", days, dead ? hm(dead) : "survived");
}

/* -- 4. clock and catch-up edge cases -------------------------------------- */
static void scenario_edges(void)
{
    printf("[edges]\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + H);
    const int f = pet_stat(&p, PET_STAT_FULLNESS);
    pet_apply(&p, T0);  // clock went backwards
    CHECK(pet_stat(&p, PET_STAT_FULLNESS) == f, "backwards clock must not change stats");
    CHECK(p.updated_at == T0, "backwards clock resyncs updated_at");
    pet_apply(&p, T0 + 1);
    CHECK(pet_stat(&p, PET_STAT_FULLNESS) == f, "one second after resync: no visible change");

    pet_new(&p, T0);
    pet_apply(&p, T0 + 30 * D);  // a month away
    CHECK(p.stage == PET_STAGE_DEAD, "a month of neglect is death");
    CHECK(p.updated_at == T0 + 30 * D, "catch-up lands on now");
    CHECK(pet_act(&p, PET_ACT_FEED_MEAL, T0 + 30 * D) == PET_BLOCKED_DEAD, "dead pets do not eat");
    CHECK(pet_act(&p, PET_ACT_NEW_EGG, T0 + 30 * D) == PET_OK, "new egg from dead");
    CHECK(p.stage == PET_STAGE_EGG, "and it is an egg");

    pet_new(&p, T0);
    CHECK(pet_act(&p, PET_ACT_FEED_MEAL, T0 + 10) == PET_BLOCKED_EGG, "eggs do not eat");
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    CHECK(p.stage == PET_STAGE_BABY, "hatched");
    pet_act(&p, PET_ACT_FEED_MEAL, T0 + EGG_HATCH_S + 2);
    CHECK(pet_act(&p, PET_ACT_FEED_MEAL, T0 + EGG_HATCH_S + 3) == PET_BLOCKED_FULL, "refuses when full");
    CHECK(pet_act(&p, PET_ACT_MEDICINE, T0 + EGG_HATCH_S + 4) == PET_BLOCKED_HEALTHY, "no medicine when healthy");
    pet_act(&p, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 5);
    CHECK(p.asleep, "lights toggles sleep");
    CHECK(pet_act(&p, PET_ACT_PLAY, T0 + EGG_HATCH_S + 6) == PET_BLOCKED_ASLEEP, "no play while asleep");
    CHECK(pet_act(&p, PET_ACT_SHAKE, T0 + EGG_HATCH_S + 7) == PET_OK && !p.asleep, "shake wakes");
    CHECK(pet_face(&p, T0 + EGG_HATCH_S + 8) == PET_FACE_STARTLED, "startled face after shake");
    CHECK(pet_face(&p, T0 + EGG_HATCH_S + 7 + TRANSIENT_S + 1) != PET_FACE_STARTLED, "transient expires");

    // Phase 3 -> Phase 4 migration: a pet whose timestamps are uptime seconds
    // (near zero) is re-anchored to the real clock, not charged decades.
    pet_new(&p, 100);
    const int mf = pet_stat(&p, PET_STAT_FULLNESS);
    const int64_t poop_before = p.next_poop_at;
    pet_apply(&p, T0);
    CHECK(p.born_at == T0, "migration re-anchors born_at (got %lld)", (long long)p.born_at);
    CHECK(p.updated_at == T0, "migration re-anchors updated_at");
    CHECK(pet_stat(&p, PET_STAT_FULLNESS) == mf, "migration charges no decay");
    CHECK(pet_age_s(&p, T0) == 0, "migration leaves age at 0");
    CHECK(p.next_poop_at == poop_before + (T0 - 100), "migration shifts the poop timer");

    // pet_action_enabled mirrors the guards (the UI greys out blocked buttons)
    pet_new(&p, T0);
    CHECK(!pet_action_enabled(&p, PET_ACT_FEED_MEAL), "eggs cannot eat");
    CHECK(pet_action_enabled(&p, PET_ACT_SHAKE), "eggs can be shaken");
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    CHECK(pet_action_enabled(&p, PET_ACT_FEED_MEAL), "a hungry baby can eat");
    CHECK(!pet_action_enabled(&p, PET_ACT_MEDICINE), "a healthy pet needs no medicine");
    pet_act(&p, PET_ACT_CLEAN, T0 + EGG_HATCH_S + 2);  // resets hygiene to full
    CHECK(!pet_action_enabled(&p, PET_ACT_CLEAN), "a just-cleaned pet needs no clean");
    pet_act(&p, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 3);
    CHECK(!pet_action_enabled(&p, PET_ACT_PLAY), "an asleep pet cannot play");
    CHECK(!pet_action_enabled(&p, PET_ACT_FEED_MEAL), "an asleep pet cannot eat");
    p.dirty = 1;
    CHECK(pet_action_enabled(&p, PET_ACT_CLEAN), "a dirty pet can be cleaned");

    CHECK(sizeof(pet_t) < 256, "pet_t is %zu bytes; keep the NVS blob small", sizeof(pet_t));
}

int main(void)
{
    printf("pet_t = %zu bytes, FAST_FORWARD = %d\n\n", sizeof(pet_t), FAST_FORWARD);
    scenario_neglect();
    scenario_good_owner(30);
    scenario_lazy_owner(10);
    scenario_one_visit(14);
    scenario_edges();
    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
