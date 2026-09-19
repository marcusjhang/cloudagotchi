# Tasks

**Status (2026-09-19):** Phase 3 is written, builds, passes the host sim, and is **verified on
the device** (V2 board) — lifecycle, all actions, snack, shake, INFO, persistence. `FAST_FORWARD`
is now wired into the build. Flashing works again. **Phase 3 is done.**

Phases live in `PLAN.md §7`. Keep this file current; it is the hand-off between sessions.

## Now

- [x] **Flash Phase 3 and verify on the board** (2026-09-19), with the serial log open:
  - [x] boots, `journal: boot #N`, `game: loaded: ...` (persistence) / `new egg`
  - [x] egg hatches after 5 min → baby (`loaded: baby`), stat bars move over time
  - [x] FEED / PLAY / LIGHT / CLEAN → `game: <action> -> ok`; MED → `medicine -> not sick`;
        asleep blocks feeding (`meal -> zzz`, `snack -> zzz`)
  - [x] long-press FEED = snack (`game: snack -> ok`)
  - [x] INFO overlay opens/closes — visual check (button reads `INFO`, x≈307–361; it logs nothing)
  - [x] shake → `app_imu: Shake detected!` + `game: shake -> ok`; no false shake at boot
        (1.5 s warm-up in `app_imu.c`)
  - [x] reset the board → same pet comes back (actions accumulate across boots)
  - [x] touch parallax: action-bar taps land at y≈440–447 (buttons span y 380–436), still
        inside the 20 px extended hit zone (`EXT_CLICK_PX`) — no adjustment needed
- [x] Wire `FAST_FORWARD` into the build (`target_compile_definitions` in
      `firmware/main/CMakeLists.txt` from a CMake cache var). Verified: default compiles with
      `-DFAST_FORWARD=1`, `idf.py -DFAST_FORWARD=60 build` compiles with `=60`
- [x] Commit "Phase 3: verified on device" saying what was actually seen; tick PLAN.md §7 Phase 3

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

- The "USB-serial hiccup" is understood now: attaching the serial monitor triggers a
  `USB_UART_CHIP_RESET` (reset reason `usb`) — once on open and again ~60 s later while the
  port is held. It is host-side, not firmware, and does not lose the pet (state is in NVS).
  Leave the board untouched for slow timers (e.g. the 5 min hatch) to run to completion.
- Repo is public; the owner said that's fine.

## Done

- [x] Phase 0 — plan, fork `tagazok/cloudagotchi` → `marcusjhang/cloudagotchi`, branch `tamagotchi` (2026-09-18)
- [x] Phase 1 — cloud stripped, ESP-IDF 5.5 build, flashed, V2 board confirmed, ghost face on screen (2026-09-18)
- [x] Phase 3 code — rules + sim (neglect / good owner / lazy owner / one visit / edges, all green), game task, NVS, journal, new screen, IMU warm-up; builds, 1.5 MB (2026-09-18)
- [x] Phase 3 device verification — flashed 2026-09-19, lifecycle + all actions + snack + shake + persistence confirmed over serial (INFO overlay pending)
