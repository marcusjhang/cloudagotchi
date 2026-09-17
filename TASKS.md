# Tasks

**Status (2026-09-18):** Phase 3 is written, builds, and passes the host sim — **not yet
verified on the device**. Flashing is blocked by a USB-serial hiccup (chip stops answering
esptool; needs a cable replug). The board currently runs Phase 1 firmware (ghost face).

Phases live in `PLAN.md §7`. Keep this file current; it is the hand-off between sessions.

## Now

- [ ] **Flash Phase 3 and verify on the board.** Blocked on the owner replugging USB.
      Then check, with the serial log open:
  - [ ] boots, `journal: boot #N`, `game: new egg` (or `loaded:`)
  - [ ] egg hatches after 5 min → baby, stat bars move
  - [ ] FEED / PLAY / LIGHT / CLEAN / MED each log `game: <action> -> ok` and move a bar
  - [ ] long-press FEED = snack; blocked actions show a toast ("not hungry", "zzz")
  - [ ] INFO overlay opens/closes; touch log shows coordinates — note how far below the
        button centre presses land (parallax) and adjust `EXT_CLICK_PX` if needed
  - [ ] shake → startled face, `shake -> ok`; no false shake at boot
  - [ ] reset the board → same pet comes back (`game: loaded: ...`)
- [ ] Wire `FAST_FORWARD` into the build (`target_compile_definitions` in
      `firmware/main/CMakeLists.txt` from a CMake cache var) so a day passes in 24 min on the device
- [ ] Commit "Phase 3: verified on device" saying what was actually seen; tick PLAN.md §7 Phase 3

## Next — Phase 4: time, sleep, battery (needs the owner's go-ahead first)

- [ ] `rtc.c`: PCF85063 on `bsp_i2c_get_handle()`; set from build time when invalid; seed `settimeofday`; `TZ` Singapore
- [ ] `game_now()` → real clock; catch-up on boot (capped 7 d) — the sim already covers the rules side
- [ ] `battery.c`: AXP2101 present bit, %, mV, VBUS → status strip
- [ ] `power.c`: 30 s idle → dim, 2 min → screen off; doze on USB, light-sleep slices on battery; wake via BOOT / touch / IMU / VBUS edge → `esp_restart()`
- [ ] Overnight soak ×2; measure %/h dark

## Later

- [ ] Phase 2 (deferred by the owner): own sprites — `tools/sprites/PROMPTS.md`, `prepare_sprites.py`, `to_lvgl.py`; replace `faces/`
- [ ] Phase 5: stage/evolution art, mini-game, sound (hand-rolled ES8311, not the BSP helper), clock face
- [ ] Night schedule refinement (`pet_is_night` only prevents auto-wake today)
- [ ] `tools/journal_dump.py` to read the NVS journal over serial

## Blocked / open questions

- USB flashing (see Now). Software-side attempts exhausted: default_reset, usb_reset, no_reset, low baud, DTR/RTS release.
- Repo is public; the owner said that's fine.

## Done

- [x] Phase 0 — plan, fork `tagazok/cloudagotchi` → `marcusjhang/cloudagotchi`, branch `tamagotchi` (2026-09-18)
- [x] Phase 1 — cloud stripped, ESP-IDF 5.5 build, flashed, V2 board confirmed, ghost face on screen (2026-09-18)
- [x] Phase 3 code — rules + sim (neglect / good owner / lazy owner / one visit / edges, all green), game task, NVS, journal, new screen, IMU warm-up; builds, 1.5 MB (2026-09-18)
