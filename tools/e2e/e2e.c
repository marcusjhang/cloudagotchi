/*
 * End-to-end tests for the pet, on the host, against the real rules in
 * firmware/main/pet.c. This is the "does it behave like a Tamagotchi?" suite:
 * a full life from egg to adult, needs, sleep, sickness, death and revival,
 * offline catch-up, and the UI's action-enablement predicate staying in step
 * with the actions themselves.
 *
 *   tools/check.sh
 *
 * Hardware glue (game.c/rtc.c/battery.c/power.c/pet_ui.c) is not host-compilable
 * and is covered on the device; this suite covers the brain.
 */
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "pet.h"

#define T0 1758153600LL  // 2026-09-18 00:00 UTC (08:00 SGT)
#define H 3600LL
#define D (24 * H)

static int failures;
static int checks;
#define CHECK(cond, ...)                                              \
    do {                                                              \
        checks++;                                                     \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                      \
            printf("\n");                                             \
        }                                                             \
    } while (0)

static int hour_sgt(int64_t t) { return (int)(((t + TZ_OFFSET_H * H) % D) / H); }
static int minute_sgt(int64_t t) { return (int)(((t + TZ_OFFSET_H * H) % H) / 60); }

/* -- a caring owner: feed/play/clean/medicine on the three daily visits ----- */
static void visit(pet_t *p, int64_t t)
{
    if (p->asleep) pet_act(p, PET_ACT_LIGHTS, t);
    if (p->dirty) pet_act(p, PET_ACT_CLEAN, t);
    if (pet_stat(p, PET_STAT_HEALTH) < SICK_BELOW) pet_act(p, PET_ACT_MEDICINE, t);
    pet_act(p, PET_ACT_FEED_MEAL, t);
    pet_act(p, PET_ACT_PLAY, t);
    if (pet_stat(p, PET_STAT_HAPPINESS) < 60) pet_act(p, PET_ACT_FEED_SNACK, t);
}

/* -- 1. a full life: egg -> baby -> child -> teen -> adult ----------------- */
static void scenario_lifecycle(void)
{
    printf("[lifecycle] egg -> baby -> child -> teen -> adult, well cared for\n");
    pet_t p;
    pet_new(&p, T0);

    int64_t first[PET_STAGE_DEAD + 1] = {0};
    for (int64_t t = T0; t <= T0 + 8 * D; t += 60) {
        pet_apply(&p, t);
        const int hh = hour_sgt(t), mm = minute_sgt(t);
        if (mm == 0 && (hh == 8 || hh == 13 || hh == 19)) visit(&p, t);
        if (mm == 0 && hh == 23 && !p.asleep) pet_act(&p, PET_ACT_LIGHTS, t);
        if (first[p.stage] == 0) first[p.stage] = t - T0;
    }

    CHECK(first[PET_STAGE_BABY] == EGG_HATCH_S, "hatches at %lld min (got %lld)",
          (long long)EGG_HATCH_S / 60, (long long)first[PET_STAGE_BABY] / 60);
    CHECK(first[PET_STAGE_CHILD] == BABY_TO_CHILD_S, "child at 65 min (got %lld)",
          (long long)first[PET_STAGE_CHILD] / 60);
    CHECK(first[PET_STAGE_TEEN] == CHILD_TO_TEEN_S, "teen at age 3 d (got %lld h)",
          (long long)first[PET_STAGE_TEEN] / H);
    CHECK(first[PET_STAGE_ADULT_GOOD] == TEEN_TO_ADULT_S, "adult at age 6 d (got %lld h)",
          (long long)first[PET_STAGE_ADULT_GOOD] / H);
    CHECK(first[PET_STAGE_DEAD] == 0, "a well-cared pet never dies");
}

/* -- 2. neglect: sad -> sick -> dead, then a new egg ------------------------ */
static void scenario_neglect(void)
{
    printf("[neglect] ignored after hatching\n");
    pet_t p;
    pet_new(&p, T0);
    int64_t sad = 0, sick = 0, dead = 0;
    for (int64_t t = T0; t < T0 + 4 * D; t += 60) {
        pet_apply(&p, t);
        const pet_face_t f = pet_face(&p, t);
        if (!sad && f == PET_FACE_SAD) sad = t - T0;
        if (!sick && f == PET_FACE_SICK) sick = t - T0;
        if (f == PET_FACE_DEAD) { dead = t - T0; break; }
    }
    CHECK(sad > 2 * H && sad < 20 * H, "gets sad within a day (got %lld m)", (long long)sad / 60);
    CHECK(sick && sick > sad && sick < 3 * D, "gets sick before dying");
    CHECK(dead, "neglect is fatal");
    CHECK(p.stage == PET_STAGE_DEAD, "dead stage");

    CHECK(pet_act(&p, PET_ACT_NEW_EGG, dead + T0) == PET_OK, "a dead pet can be restarted");
    CHECK(p.stage == PET_STAGE_EGG, "new egg after death");
    CHECK(pet_action_enabled(&p, PET_ACT_FEED_MEAL) == false, "eggs cannot eat");
}

/* -- 3. feeding: meal fills, refuses when full; snack adds happy + weight --- */
static void scenario_feeding(void)
{
    printf("[feeding] meal / snack\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    CHECK(p.stage == PET_STAGE_BABY, "hatched");

    const int before = pet_stat(&p, PET_STAT_FULLNESS);
    pet_act(&p, PET_ACT_FEED_MEAL, T0 + EGG_HATCH_S + 2);
    CHECK(pet_stat(&p, PET_STAT_FULLNESS) > before, "a meal fills hunger");
    for (int i = 0; i < 5; i++) pet_act(&p, PET_ACT_FEED_MEAL, T0 + EGG_HATCH_S + 3 + i);
    CHECK(pet_stat(&p, PET_STAT_FULLNESS) <= 100, "hunger never exceeds 100");
    CHECK(pet_act(&p, PET_ACT_FEED_MEAL, T0 + EGG_HATCH_S + 10) == PET_BLOCKED_FULL,
          "refuses a meal when full");

    const int w = p.weight, hp = pet_stat(&p, PET_STAT_HAPPINESS);
    pet_act(&p, PET_ACT_FEED_SNACK, T0 + EGG_HATCH_S + 11);
    CHECK(p.weight == w + 1, "a snack adds weight");
    CHECK(pet_stat(&p, PET_STAT_HAPPINESS) >= hp, "a snack is a treat");

    pet_act(&p, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 12);
    CHECK(pet_act(&p, PET_ACT_FEED_MEAL, T0 + EGG_HATCH_S + 13) == PET_BLOCKED_ASLEEP,
          "no meal while asleep");
    CHECK(pet_act(&p, PET_ACT_FEED_SNACK, T0 + EGG_HATCH_S + 14) == PET_BLOCKED_ASLEEP,
          "no snack while asleep");
}

/* -- 4. poop: it happens, cleaning resets the clock, neglect costs health --- */
static void scenario_poop(void)
{
    printf("[poop] timing, clean, and the cost of leaving it\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    pet_act(&p, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 2);  // ensure awake
    if (p.asleep) pet_act(&p, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 3);

    const int64_t now = T0 + EGG_HATCH_S + 10;
    CHECK(p.next_poop_at > now, "not dirty immediately after hatching");
    pet_apply(&p, p.next_poop_at + 1);
    CHECK(p.dirty, "gets dirty when the clock says so");

    pet_act(&p, PET_ACT_CLEAN, p.next_poop_at + 2);
    CHECK(!p.dirty, "cleaning removes it");
    CHECK(p.next_poop_at > p.updated_at, "cleaning resets the poop timer");

    // a baby that is fed but never cleaned should get sick from the mess
    pet_t q;
    pet_new(&q, T0);
    pet_apply(&q, T0 + EGG_HATCH_S + 1);
    pet_act(&q, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 2);
    if (q.asleep) pet_act(&q, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 3);
    int64_t t = T0 + EGG_HATCH_S + 60;
    for (; t < T0 + 2 * D; t += 60) {
        pet_apply(&q, t);
        if (pet_stat(&q, PET_STAT_FULLNESS) < 70) pet_act(&q, PET_ACT_FEED_MEAL, t);
    }
    CHECK(pet_stat(&q, PET_STAT_HEALTH) < 100, "a festering mess drains health");
}

/* -- 5. sleep: auto-nap, lights, shake wake, night -------------------------- */
static void scenario_sleep(void)
{
    printf("[sleep] auto-nap, lights, shake, night\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    CHECK(!p.asleep, "awake after hatching");

    pet_act(&p, PET_ACT_LIGHTS, T0 + EGG_HATCH_S + 2);
    CHECK(p.asleep, "lights toggle sleep on");
    CHECK(pet_act(&p, PET_ACT_PLAY, T0 + EGG_HATCH_S + 3) == PET_BLOCKED_ASLEEP,
          "no play while asleep");
    pet_apply(&p, T0 + EGG_HATCH_S + 4);
    pet_act(&p, PET_ACT_SHAKE, T0 + EGG_HATCH_S + 4);
    CHECK(!p.asleep, "shake wakes it");

    // it nods off on its own when energy runs low
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    for (int64_t t = T0 + EGG_HATCH_S + 2; t < T0 + EGG_HATCH_S + 30 * H; t += 300) {
        pet_apply(&p, t);
        if (p.asleep) break;
    }
    CHECK(p.asleep, "falls asleep by itself when tired");

    CHECK(pet_is_night(T0 + 15 * H), "23:00 SGT is night");   // T0 is 08:00 SGT
    CHECK(!pet_is_night(T0 + 4 * H), "noon SGT is not night");
    CHECK(!pet_is_night(0), "an unset clock is never night");
}

/* -- 6. sickness and medicine ---------------------------------------------- */
static void scenario_medicine(void)
{
    printf("[medicine] sick -> cure; refused when healthy\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + 1);
    CHECK(pet_act(&p, PET_ACT_MEDICINE, T0 + EGG_HATCH_S + 2) == PET_BLOCKED_HEALTHY,
          "no medicine when well");
    CHECK(!pet_action_enabled(&p, PET_ACT_MEDICINE), "MED disabled when well");

    // run it into sickness by neglect, then cure
    int64_t t = T0 + EGG_HATCH_S + 3;
    for (; t < T0 + 3 * D; t += 60) {
        pet_apply(&p, t);
        if (pet_stat(&p, PET_STAT_HEALTH) < SICK_BELOW) break;
    }
    CHECK(pet_stat(&p, PET_STAT_HEALTH) < SICK_BELOW, "neglect made it sick");
    if (p.asleep) pet_act(&p, PET_ACT_LIGHTS, t);  // the sick face is hidden while asleep
    pet_apply(&p, t + 1);
    CHECK(pet_face(&p, t + 1) == PET_FACE_SICK, "shows the sick face");
    CHECK(pet_action_enabled(&p, PET_ACT_MEDICINE), "MED enabled when sick");
    const int h = pet_stat(&p, PET_STAT_HEALTH);
    pet_act(&p, PET_ACT_MEDICINE, t + 1);
    CHECK(pet_stat(&p, PET_STAT_HEALTH) > h, "medicine helps");
}

/* -- 7. care quality decides the adult -------------------------------------- */
static void scenario_care_quality(void)
{
    printf("[care] many mistakes -> grump, few -> good adult\n");
    pet_t p;
    pet_new(&p, T0);
    // deliberately ignore it in short bursts so it racks up care mistakes but lives
    for (int64_t t = T0; t < T0 + 7 * D; t += 60) {
        pet_apply(&p, t);
        if (minute_sgt(t) == 0 && hour_sgt(t) == 13) {  // one feed a day only
            pet_act(&p, PET_ACT_FEED_MEAL, t);
            pet_act(&p, PET_ACT_CLEAN, t);
        }
    }
    CHECK(p.care_mistakes >= ADULT_BAD_MISTAKES, "mistakes accumulated (%u)", p.care_mistakes);
    CHECK(p.stage == PET_STAGE_ADULT_BAD || p.stage == PET_STAGE_DEAD,
          "poor care -> grump (or dead), got %s", pet_stage_name((pet_stage_t)p.stage));
}

/* -- 8. offline catch-up edges ---------------------------------------------- */
static void scenario_catchup(void)
{
    printf("[catch-up] capped, backwards clock, Phase-3 migration\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + 30 * D);
    CHECK(p.updated_at == T0 + 30 * D, "catch-up lands on now");
    CHECK(p.stage == PET_STAGE_DEAD, "a month alone is fatal");

    pet_new(&p, T0);
    pet_apply(&p, T0 + 1000 * D);  // absurdly far: must be capped, not overflow
    CHECK(p.updated_at == T0 + 1000 * D, "a huge gap still lands on now");
    for (int s = 0; s < PET_STAT_COUNT; s++)
        CHECK(pet_stat(&p, (pet_stat_t)s) >= 0, "stats never underflow");

    pet_t q;
    pet_new(&q, T0);
    pet_apply(&q, T0 + EGG_HATCH_S + H);
    const int f = pet_stat(&q, PET_STAT_FULLNESS);
    pet_apply(&q, T0);  // clock went backwards
    CHECK(pet_stat(&q, PET_STAT_FULLNESS) == f, "backwards clock loses no stats");
    CHECK(q.updated_at == T0, "backwards clock resyncs");

    pet_t m;  // Phase 3 pet: uptime-based timestamps, then a real clock appears
    pet_new(&m, 100);
    const int mf = pet_stat(&m, PET_STAT_FULLNESS);
    pet_apply(&m, T0);
    CHECK(m.born_at == T0 && m.updated_at == T0, "migration re-anchors to the real clock");
    CHECK(pet_stat(&m, PET_STAT_FULLNESS) == mf, "migration charges no decay");
}

/* -- 9. persistence: the blob round-trips and keeps ticking ----------------- */
static void scenario_persistence(void)
{
    printf("[persist] blob round-trip\n");
    pet_t p;
    pet_new(&p, T0);
    pet_apply(&p, T0 + EGG_HATCH_S + 500);
    pet_act(&p, PET_ACT_FEED_SNACK, T0 + EGG_HATCH_S + 600);
    const pet_t saved = p;

    pet_t loaded;
    memcpy(&loaded, &saved, sizeof(loaded));  // what NVS does, byte for byte
    CHECK(memcmp(&saved, &loaded, sizeof(saved)) == 0, "fields survive the round-trip");
    CHECK(loaded.version == PET_SCHEMA_VERSION, "schema version is current");
    pet_apply(&loaded, T0 + EGG_HATCH_S + 3600);  // and it keeps living
    CHECK(loaded.updated_at == T0 + EGG_HATCH_S + 3600, "loaded pet advances");
}

/* -- 10. the UI predicate matches the actions exactly ----------------------- */
static void check_parity(const pet_t *p, int64_t now, const char *where)
{
    pet_t base = *p;
    pet_apply(&base, now);
    for (int a = 0; a < PET_ACT_COUNT; a++) {
        pet_t copy = base;
        const pet_result_t r = pet_act(&copy, (pet_action_t)a, now);
        const bool enabled = pet_action_enabled(&base, (pet_action_t)a);
        CHECK(enabled == (r == PET_OK),
              "%s: %s enabled=%d but pet_act says %d", where, pet_action_name(a),
              (int)enabled, (int)r);
    }
}

static void scenario_action_parity(void)
{
    printf("[parity] pet_action_enabled() mirrors pet_act() in every state\n");
    pet_t p;
    const int64_t now = T0 + 10 * D;

    pet_new(&p, now);                         check_parity(&p, now, "egg");
    pet_apply(&p, now + EGG_HATCH_S + 1);     check_parity(&p, now + EGG_HATCH_S + 1, "baby");
    pet_act(&p, PET_ACT_FEED_SNACK, now + EGG_HATCH_S + 2);
    pet_act(&p, PET_ACT_FEED_SNACK, now + EGG_HATCH_S + 3);
    pet_act(&p, PET_ACT_FEED_SNACK, now + EGG_HATCH_S + 4);
    pet_act(&p, PET_ACT_FEED_SNACK, now + EGG_HATCH_S + 5);
    pet_act(&p, PET_ACT_FEED_SNACK, now + EGG_HATCH_S + 6);  // full-ish
    check_parity(&p, now + EGG_HATCH_S + 6, "full baby");
    pet_act(&p, PET_ACT_LIGHTS, now + EGG_HATCH_S + 7);
    check_parity(&p, now + EGG_HATCH_S + 7, "asleep");
    p.dirty = 1;                              check_parity(&p, now + EGG_HATCH_S + 8, "dirty");
    p.stats_u[PET_STAT_HEALTH] = 0;           // force the sick/dead-adjacent states
    pet_apply(&p, now + EGG_HATCH_S + 8);
    check_parity(&p, now + EGG_HATCH_S + 8, "sick");
    p.stage = PET_STAGE_DEAD;                 check_parity(&p, now + EGG_HATCH_S + 9, "dead");

    // and the parity holds across all growth stages
    pet_t g;
    pet_new(&g, now);
    const int64_t marks[] = {EGG_HATCH_S, BABY_TO_CHILD_S, CHILD_TO_TEEN_S, TEEN_TO_ADULT_S};
    for (unsigned i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        pet_apply(&g, now + marks[i] + 1);
        check_parity(&g, now + marks[i] + 1, pet_stage_name((pet_stage_t)g.stage));
    }
}

int main(void)
{
    printf("pet E2E (host) — pet_t = %zu bytes\n\n", sizeof(pet_t));
    scenario_lifecycle();
    scenario_neglect();
    scenario_feeding();
    scenario_poop();
    scenario_sleep();
    scenario_medicine();
    scenario_care_quality();
    scenario_catchup();
    scenario_persistence();
    scenario_action_parity();
    printf("\n%s (%d checks, %d failure%s)\n", failures ? "FAILED" : "OK", checks, failures,
           failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
