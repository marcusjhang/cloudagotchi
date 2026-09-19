# Tasks

**Status (2026-09-19):** Phase 3 done and device-verified. Phase 4 (time, battery, sleep) is
written and builds; the clock, boot catch-up, battery readout, idle dim and USB doze are
verified on the board. The **battery-only** paths (light sleep, overnight soaks) and the
settings stub are still open — the owner has no battery attached, so the RTC also loses time
on unplug (it only retains while powered).

Phases live in `PLAN.md §7`. Keep this file current; it is the hand-off between sessions.

## Now

- [ ] Phase 4 device checks still open:
  - [ ] **Wake from USB doze by touch** — tap after ~2 min idle, expect `power: screen on`
  - [ ] **With a battery**: RTC keeps time across unplug; `power: on battery: sleeping`,
        wake by touch / BOOT / plugging in
  - [ ] **Overnight soak ×2** (unplug at night): pet hungry, clock right, journal shows the sleep
  - [ ] Settings "Reset pet" button (hold) — built, not yet exercised
  - [x] Settings page stub — long-press INFO: time ± and brightness cycle work; `rtc: clock adjusted`
        confirmed on device (2026-09-19)

## Host tests (run before committing)

- [x] `tools/check.sh` — rules sim (`tools/sim`) + hardware-math tests (`tools/tests/test_hw_math.c`)
- [x] `firmware/main/hw_math.h` isolates the pure RTC/PMIC math so it compiles off-target
- [x] sim covers the Phase 3 → Phase 4 timestamp migration (uptime → real epoch)

## Next

- [ ] Phase 2 (deferred by the owner): own sprites — `tools/sprites/PROMPTS.md`, `prepare_sprites.py`, `to_lvgl.py`; replace `faces/`
- [ ] Phase 5: stage/evolution art, mini-game, sound (hand-rolled ES8311, not the BSP helper), clock face
- [ ] Night schedule refinement (`pet_is_night` only prevents auto-wake today)
- [ ] `tools/journal_dump.py` to read the NVS journal over serial

## Blocked / open questions

- **No battery attached** → battery-only paths and the soaks can't be verified. The AXP2101
  answers with no cell (reports `present=0`, bogus VBAT); the UI shows "USB" instead.
- The "USB-serial hiccup" is host-side: attaching the monitor triggers `USB_UART_CHIP_RESET`,
  and once the chip stops answering esptool the cable must be replugged. State is in NVS.
- Repo is public; the owner said that's fine.

## Done

- [x] Phase 0 — plan, fork `tagazok/cloudagotchi` → `marcusjhang/cloudagotchi`, branch `tamagotchi` (2026-09-18)
- [x] Phase 1 — cloud stripped, ESP-IDF 5.5 build, flashed, V2 board confirmed, ghost face on screen (2026-09-18)
- [x] Phase 3 — rules + sim, game task, NVS, journal, screen, IMU warm-up; **device-verified 2026-09-19**
- [x] Phase 4 code — `rtc.c` (PCF85063A + build-time seed + TZ), `battery.c` (AXP2101), real-clock
      `game_now()` with boot catch-up, `power.c` (dim → screen off → doze/sleep). Verified on USB:
      clock seeds, pet re-anchors, battery reads, `idle 30s: dim`, `idle 120s: dozing on USB` (2026-09-19)
