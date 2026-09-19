# Agent orientation — esp32-tamagotchi

Read this first. Then `TASKS.md` (where we are) and `PLAN.md` (the design).

## What this is

A Tamagotchi on the **Waveshare ESP32-S3-Touch-AMOLED-1.8** (368×448 AMOLED, touch, RTC,
IMU, speaker, battery). Everything runs on the device — no cloud, no Wi-Fi. Fork of
[cloudagotchi](https://github.com/tagazok/cloudagotchi) `article-2` (MIT); the AWS brain is
gone and the pet's rules live in `firmware/main/pet.c`.

- **Repo:** `marcusjhang/cloudagotchi`, work happens on branch **`tamagotchi`** (default).
  `upstream` = tagazok/cloudagotchi; never merge its `main` (that's the cloud version).
- **Owner's board:** revision **V2** (CO5300 panel + CST820 touch), shows up as
  `/dev/cu.usbmodem2101` on the owner's Mac. ESP32-S3 rev v0.2, 8 MB PSRAM.
- **Owner's preference:** plan → approve → build, one phase at a time. Confirm before
  starting a phase. Placeholder ghost sprites are fine for now; don't push for art.

## Where things are

| Path | What |
|---|---|
| `TASKS.md` | Live task board. Update it when you finish, start, or block on something. |
| `PLAN.md` | Design + phases 0–4, hardware, landmines (§5), prior art. |
| `firmware/main/pet.c` `.h` `config.h` | Game rules. **Pure C, no ESP headers** — compiles on the host. |
| `firmware/main/game.c` | 1 Hz tick task, one mutex, NVS saves, snapshots → UI. |
| `firmware/main/pet_ui.c` | LVGL 9 screen: stat bars, face, poop, caption, toast, action bar, INFO. |
| `firmware/main/persist.c` `journal.c` | NVS blob for the pet; boot count + reset reason. |
| `firmware/main/app_imu.c` | Shake detector (QMI8658, 50 Hz task). |
| `firmware/main/faces/` | Placeholder sprites (RGB565A8 `lv_image_dsc_t`, from upstream). |
| `tools/sim/`, `tools/tests/` | Host tests: game rules + hardware math. **`tools/check.sh` before you commit.** |

**Structure diagram:** Excalidraw *"esp32-tamagotchi — Structure"* —
https://app.excalidraw.com/s/919s34P0y0E/4WRHwsoGZ8m (workspace `919s34P0y0E`, scene
`4WRHwsoGZ8m`). **Keep it current: any change to modules, tasks, boot order, or data flow
updates this scene in the same step.**

## Commands

```bash
# host tests (no hardware, < 1 s): game rules + hardware math
tools/check.sh
# ...or just the game rules
tools/sim/run.sh

# firmware
. ~/esp/esp-idf/export.sh                      # ESP-IDF v5.5 lives at ~/esp/esp-idf
cd firmware
idf.py build                                   # first build downloads the BSP + LVGL
idf.py -p /dev/cu.usbmodem2101 flash           # NOT `flash monitor` — monitor is interactive
```

Read the serial log without `idf.py monitor` (interactive tools hang agent sessions):

```bash
. ~/esp/esp-idf/export.sh && python3 - <<'PY'
import serial, time
s = serial.Serial(); s.port = "/dev/cu.usbmodem2101"; s.baudrate = 115200; s.timeout = 0.2
s.dtr = False; s.rts = False; s.open()        # deasserted: opening with DTR high reboots the chip
s.rts = True; time.sleep(0.1); s.rts = False  # optional: pulse reset to capture a boot log
t0 = time.time(); buf = b""
while time.time() - t0 < 15: buf += s.read(4096)
s.close(); print(buf.decode("utf-8", "replace"))
PY
```

`FAST_FORWARD` (game time ×N) is wired to a build flag: `idf.py -DFAST_FORWARD=60 build`.
The touch log (`pet_ui: touch down x,y`) is permanent by design.

## Rules of the road

- `tools/check.sh` must be green before you commit: it runs the rules sim
  (`firmware/main/pet.c`) and the hardware-math tests (`firmware/main/hw_math.h`).
  Any new pure logic (no ESP headers) should get a host test there.
- **UI craft.** Any screen/button work uses the vendored `lvgl-ui` skill
  (`.agents/skills/lvgl-ui`, source `marcusjhang/lvgl-ui`, pinned in `skills-lock.json`):
  design tokens, the single action-button component with pressed/active/disabled states,
  icon strategy and the pre-ship checklist. Read it before editing `pet_ui.c`.
- **Code-review gate.** Clearing any feature or milestone requires an `ocr` review with
  **all P1 (critical/high) findings fixed** before the commit. Use delegation mode so the
  agent does the thinking:
  ```bash
  ocr delegate preview --format json [--from <base> --to HEAD]
  ocr delegate rule --format json --background-file .ocr/background.md <path...>
  ```
  (`.ocr/background.md` holds this project's invariants and severity mapping.)
  Fix P1s, re-run the loop, and repeat until **≤2 P1s** remain (report any survivors with
  reasoning). Do not silently skip previewed files — coverage is mandatory.
- Commit `firmware/dependencies.lock`; never commit `firmware/sdkconfig`, `build/`, `managed_components/`.
- Changing `sdkconfig.defaults` needs `rm firmware/sdkconfig && idf.py set-target esp32s3` to take effect.
- One commit per phase or per meaningful step; the message says what was **verified on the device** vs only built.
- Never commit the owner's photos. `tools/sprites/raw/` is git-ignored for that reason. The repo is public.
- Everything that touches LVGL runs under `bsp_display_lock()`. Never hold the game mutex while taking the display lock (see the comment in `game.c`).
- Hardware landmines: `PLAN.md §5`. The ones that bite most: never configure GPIO13; no light-sleep on USB; `bsp_audio_codec_speaker_init()` boot-loops; AXP2101 reports 0 % with no battery; touch lands 15–25 px low.
- If the flasher says "No serial data received": unplug/replug USB; if still stuck, hold BOOT + tap PWR, then flash with `--before no_reset`.

## Who's who

- Owner: Marcus (`marcusjhang` on GitHub). Git on this machine commits as `maekuss`; push over HTTPS with `git -c credential.helper='!gh auth git-credential' push` (the SSH key belongs to the other account).
- Upstream author: Olivier Leplus (cloudagotchi). Credit stays in README.
