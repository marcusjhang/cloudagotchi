# Tasks

**Status (2026-09-19):** Phase 3 done and device-verified. Phase 4 code is complete and
USB-verified (clock, catch-up, battery readout, idle dim, USB doze). This pass also landed
the UI taste pass, the journal readout, the sprite pipeline, host tests, and the `lvgl-ui`
skill. Left to finish: the **dog sprite** (art) and the **battery** (attach it, then the
Phase 4 gate: the overnight soak). The physical checks that need a person (wake-from-doze,
Reset button, brightness visual, UI sign-off) ride along with the battery session.

Phases live in `PLAN.md §7`. Structure diagram: Excalidraw scene `4WRHwsoGZ8m` (AGENTS.md).

## Now

- [ ] **Attach the LiPo (MX1.25).** Then verify:
  - [ ] `present=1` and sane mV; `batt N% NNNNmV` in the 60 s status log
  - [ ] unplug → `power: on battery: sleeping`; wake by touch / BOOT / movement
  - [ ] RTC keeps time across unplug (`os=0`, time advances)
  - [ ] overnight soak ×2 → `journal: battery ring` prints the curve; target <5 %/h
- [ ] Device checks needing hands/eyes (do with the battery): wake-from-doze tap,
      hold-to-Reset-pet, brightness cycle, UI sign-off
- [ ] **Dog sprite:** follow `tools/sprites/PROMPTS.md`, generate the sheet, run
      `prepare_sprites.py` + `to_lvgl.py`, then wire the sprite table (see the `lvgl-ui` skill)

## Host tests (run before every commit)

- [x] `tools/check.sh` — rules sim, hardware math (BCD, build time, VBAT), battery ring,
      and **pet E2E** (`tools/e2e`): full lifecycle, needs, sleep, sickness, death/revive,
      catch-up, persistence round-trip, and `pet_action_enabled()` ↔ `pet_act()` parity
- [ ] add a host test for any new pure logic

## Process

- [x] `ocr` code-review gate in `AGENTS.md` (fix all P1s before commit); ran over Phase 3+4,
      **0 P1**
- [x] Structure diagram kept current (Excalidraw `4WRHwsoGZ8m`)
- [x] `lvgl-ui` skill published (`marcusjhang/lvgl-ui`) and vendored + referenced

## Later

- Phase 2/5 art: per-stage sprites (child/adult/grump/dead), evolution art, mini-game, sound
- Night schedule refinement (`pet_is_night` only prevents auto-wake today)
- `tools/journal_dump.py` (the boot self-dump is already readable)

## Done

- [x] Phase 0 — plan, fork `tagazok/cloudagotchi` → `marcusjhang/cloudagotchi`, branch `tamagotchi` (2026-09-18)
- [x] Phase 1 — cloud stripped, ESP-IDF 5.5 build, flashed, V2 board confirmed, ghost face (2026-09-18)
- [x] Phase 3 — rules + sim, game task, NVS, journal, screen, IMU warm-up; **device-verified 2026-09-19**
- [x] Phase 4 code — `rtc.c`, `battery.c`, real-clock `game_now()` + boot catch-up, `power.c`
      (dim → screen off → doze/sleep), settings stub; **USB-verified** (clock seeds/retains
      while powered, pet re-anchors, battery reads, `idle 30s: dim`, `idle 120s: dozing on USB`)
- [x] UI taste pass — `ui_theme.h` tokens, one action-button component with pressed/active/
      disabled states driven by `pet_action_enabled()`, icon + caption bar, overlays restyled
- [x] Journal readout — `batt_ring.h` (72 samples = 12 h), self-dump at boot, host-tested
- [x] Sprite pipeline — `tools/sprites/` (PROMPTS, prepare, to_lvgl), proven end to end
- [x] Governance — `ocr` review gate + `lvgl-ui` skill + current structure diagram
- [x] Growth matched to the original P1 — egg 5 min, baby 65 min, child age 3, **teen age 6**,
      adult; `PET_SCHEMA_VERSION` bumped to 2 (adds the teen stage, so old NVS blobs reset)
- [x] UI code-friendliness — `pet_ui.c` split into `build_*` builders plus one layout-constants
      block. Modern-pet-UI inspiration (one large character, compact status, soft panels) is
      carried by `ui_theme.h` + the `lvgl-ui` skill.
- [x] UI hero redesign — after research (original P1's 3-button + summoned icon row, modern
      pets' "no HUD until asked"): pet as the hero, Food/Fun **hearts**, one pulsing **call**
      (pure `pet_need()`), **4 candy icon tiles**, settings behind a long-press. Removed the
      5-bar row, the 6-button bar, and the captions. `lvgl-ui` gains `references/screens.md`.
- [x] **Monochrome face-first UI** (owner reference: a big friendly face on black, no
      chrome). White on true black: big centred pet, a pulsing white **call ring** (pure
      `pet_need()`), **one tiny lowercase status word**, and **four dark circle buttons**
      with white icons, no captions. Settings behind a long-press. Icons via
      `tools/sprites/make_icons.py` → `assets/icons/` → RGB565A8. (Colour "cozy night"
      variant kept in the mocked options scene `A72ceSRfIgH`.)
