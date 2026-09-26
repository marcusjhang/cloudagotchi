# esp32-tamagotchi — Project Plan (v2, Phases 0–4)

A custom virtual pet on the **Waveshare ESP32-S3-Touch-AMOLED-1.8**, sprites AI-generated
from a photo. Runs on battery, keeps living while the screen is off, touch to feed / play /
clean, shake to wake.

**v2 headline:** someone already built 80 % of this on this exact board, MIT-licensed.
We fork it instead of starting from zero. See §2.

**This repo is that fork:** `marcusjhang/cloudagotchi` ← `tagazok/cloudagotchi`. Our work lives on
branch **`tamagotchi`**, cut from `article-2`. `upstream` remote points at the original.
All firmware paths below are relative to `firmware/`.

---

## 0. TL;DR

| | |
|---|---|
| Base | Fork **[tagazok/cloudagotchi](https://github.com/tagazok/cloudagotchi)** branch `article-2` (MIT): ESP-IDF + Waveshare BSP + LVGL 9, sprite faces, touch, shake — on this exact board |
| Strip | Wi-Fi, MQTT, AWS certs (its "brain" lives in Lambda; ours lives on-device) |
| Add | Game logic in C (port of its `pet-logic.mjs`, extended), NVS persistence, RTC offline catch-up, doze/sleep, battery, your sprites |
| Patterns from | **[toddsherman/pixelcat](https://github.com/toddsherman/pixelcat)** (same board, no licence → read, don't copy): `power.c`, `rtc.c`, offline catch-up, host-side sim |
| Landmines from | **[awesome-esp32 › Waveshare AMOLED 1.8 guide](https://github.com/s0lness/awesome-esp32/blob/main/guides/waveshare-amoled-18.md)** — field notes from 8 projects on this board |
| Toolchain | ESP-IDF v5.5, C, LVGL 9 via `waveshare/esp32_s3_touch_amoled_1_8` BSP ^2.0.3 |
| Time to Phase 4 | ~1 week of evenings once the board is in hand; Phases 1–2 in one evening |

---

## 1. Hardware

| Part | Chip | Notes |
|---|---|---|
| MCU | ESP32-S3R8, dual LX7 @ 240 MHz | 16 MB QIO flash, 8 MB octal PSRAM |
| Display | 1.8" AMOLED **368 × 448** RGB565, QSPI on SPI2 | V1: SH8601 · V2: **CO5300** (vendor docs say SH8601 for both; V2 is CO5300) |
| Touch | I²C, INT GPIO21, single-point | V1: FT3168 @ 0x38 · V2: CST820 @ 0x15 |
| IMU | QMI8658 | I²C 0x6B (or 0x6A — probe both) |
| RTC | PCF85063A | I²C 0x51, keeps time on battery |
| Audio | ES8311 + speaker amp + mic | I²S: MCLK 16, BCLK 9, WS 45, DOUT 8, DIN 10; PA enable GPIO46 |
| PMIC | AXP2101 | I²C 0x34 — charging, fuel gauge, PWR button |
| IO expander | TCA9554 (V2) | I²C 0x20 — panel reset, touch reset, SD CS, PWR button |
| Storage | microSD (SDMMC 1-bit: CLK 2, CMD 1, D0 3) | optional |
| Battery | 3.7 V MX1.25, ~350–400 mAh | board ships battery-optional |
| Buttons | BOOT (GPIO0), PWR (via expander + PMIC) | ~6 s PWR hold = hardware power-off, leave it |

Pins: QSPI D0–D3 = GPIO 4/5/6/7, SCLK 11, CS 12, **TE on GPIO13 — never configure GPIO13**.
I²C SDA 15, SCL 14.

**Revision: V2 (confirmed 2026-09-18)** — boot log: `co5300` panel driver, `Touch CST816S 0x15 found`.
Serial port on Marcus's Mac: `/dev/cu.usbmodem2101`. ESP32-S3 rev v0.2, 8 MB PSRAM OK.

---

## 2. Prior art (what exists, what we take)

| Project | Board | Stack | Licence | Take |
|---|---|---|---|---|
| **cloudagotchi** `article-2` | **this one** | ESP-IDF, BSP, LVGL 9 | **MIT** | **The base.** `pet_ui.c` (display, stat bars, sprite swap, blink, idle doze, tap targets, lock discipline), `app_imu.c` (shake), `sdkconfig.defaults`, sprite → RGB565A8 C-array pipeline. Game rules from `backend/lambda/brain/pet-logic.mjs` ported to C. |
| **pixelcat** | **this one** | ESP-IDF, direct CO5300, no LVGL | none (all rights reserved) | *Patterns only:* doze-on-USB vs light-sleep-on-battery, 300 ms poll slices, `esp_restart()` wake path, PCF85063 read/set-from-build-time, boot-time offline catch-up, NVS boot journal, host-side owner simulation. |
| **pocket-pet** | AMOLED-2.06 sibling | ESP-IDF | none | Sleep war stories: `SLEEP-HANDOFF.md` (dark drain 8 %/h → 3 %/h via panel sleep; PMIC rail state persists across resets; USB-JTAG doesn't survive light-sleep wake). |
| awesome-esp32 guide | this one | — | CC (docs) | Every landmine in §5. Required reading. |
| esp32-gameos | this one | ESP-IDF, direct | MIT | Touch parallax numbers, host simulator pattern, chip-synth audio (Phase 5). |
| ESP32-TamaPetchi | ESP32-S3 + ST7789 240² | Arduino, LVGL | MIT | Game-design reference: 7 mood levels, 4-bit palette sprite engine, mini-games. |
| TamaFi | ESP32-S3 + ST7789 | Arduino | MIT | Menu system + persistent state reference; 413 ★. |
| ArduinoGotchi / TamaLIB | any | Arduino | GPL-2 | Original P1 ROM emulator — not usable (ROM sprites), listed so we don't re-evaluate it. |

**Why cloudagotchi and not pixelcat as the base:** pixelcat is the better game but has no licence
and drives the panel by hand (band DMA, hand-rolled init) — a lot of surface area for a
Tamagotchi that redraws once a second. cloudagotchi is MIT, uses the vendor BSP + LVGL, and
its `pet_ui.c` is 231 lines we can read in one sitting. If we ever need 60 fps we know where
to look.

---

## 3. Decisions

| Decision | Choice | Why / alternative |
|---|---|---|
| Base | GitHub fork of cloudagotchi, branch `tamagotchi` from `article-2` | See §2. A fork of a public repo is public; that's accepted. Upstream `LICENSE` (MIT) stays at the root. `backend/` and `scripts/` are deleted in Phase 1 — we have no cloud. |
| Toolchain | **ESP-IDF v5.5** (pin it; commit `dependencies.lock`) | BSP handles both revisions. Arduino + PlatformIO on this board has an open black-screen issue (#3). |
| Display | Waveshare BSP + `esp_lvgl_port`, BSP defaults, partial-mode flush | cloudagotchi tried full-refresh → white frame. Don't fight the BSP. |
| UI | LVGL 9 | `lv_image` swap for states, `lv_animimg` for frame loops, `lv_bar` for stats, touch already wired. |
| Sprites | **Compiled-in RGB565A8 C arrays** (`lv_image_dsc_t`), memory-mapped from flash | Proven on this board; zero RAM; "one clean rectangular blit" fixed their corruption. 160×160 @ ~77 KB each; 12 MB app partition holds > 100 frames. SPIFFS + PNG decode is a Phase 5+ option for swappable packs. |
| Sprite size | 160 × 160, ≤ 32 colours, drawn at 2× with `lv_image_set_scale(512)` + `lv_image_set_antialias(false)` | 320 px on a 368 px screen. Nearest-neighbour keeps pixel edges. If flash gets tight, switch to I8 indexed (3× smaller). |
| Game logic | Pure C, `pet.c` — **elapsed-time decay** (`apply(pet, now)`), not tick counting | Same shape as `pet-logic.mjs`; offline catch-up is then free (one call at boot with RTC time). Compiles on the host for tests. |
| Persistence | NVS blob `pet` + `updated_at` epoch; write on every interaction + every 5 min | ~100 B; NVS wear-levels. |
| Time | PCF85063 via BSP I²C bus handle; set from `__DATE__/__TIME__` when invalid; seed `settimeofday` | pixelcat pattern. NTP is Phase 5+ (radio off by default). |
| Power | **On USB: doze** (screen off, CPU up). **On battery: light-sleep** in 300 ms slices polling BOOT/touch/IMU; wake = persist → `esp_restart()` | Light sleep on USB drops enumeration → unflashable. Restart re-enters the known-good boot path instead of resuming stale peripheral state. |
| Screen off | Display-off command (`0x28`) only — **not** sleep-in (`0x10`) | Sleep-in + init replay came back lit but frozen (pixelcat). |
| Language | C | ESP-IDF + LVGL are C; cloudagotchi is C. |

---

## 4. Architecture

```
main/
├── main.c             boot: nvs → rtc → pet_load → pet_apply(now) → ui → imu → power loop
├── config.h           every tunable with the measurement that set it (fluidbox style)
├── pet.c/.h           struct + pure functions: pet_new, pet_apply(now), pet_act(action), pet_face()
├── persist.c/.h       NVS load/save of pet_t (versioned blob)
├── rtc.c/.h           PCF85063: read, set, valid?, seed system clock
├── battery.c/.h       AXP2101: present bit, %, mV, vbus
├── power.c/.h         activity timer, doze/sleep state machine, wake sources
├── app_imu.c/.h       (from cloudagotchi) shake detector, 50 Hz task
├── pet_ui.c/.h        (from cloudagotchi, extended) screen, action bar, stat row, sprite/anim swap
├── journal.c/.h       NVS ring of boot reasons + warnings (serial is dead when unplugged)
└── sprites/
    ├── sprites.h      extern lv_image_dsc_t for every <stage>_<state>_<frame> + table lookup
    └── *.c            generated by tools/sprites/ (do not edit)
tools/
├── sprites/           PROMPTS.md · prepare_sprites.py (raw → clean PNG) · to_lvgl.py (PNG → .c)
└── sim/               host build of pet.c + test harness (30 simulated days, assertions)
```

Tasks: LVGL (BSP-owned, core 1) · `imu` 50 Hz (from cloudagotchi) · `game` 1 Hz: `pet_apply(now)`,
push to UI under `bsp_display_lock`, `power_idle_check()`. Everything touching LVGL takes the lock.

### 4.1 Pet state (the whole game in one struct)

```c
typedef struct {
    uint32_t version;        // schema, bump on change; persist rejects mismatches
    int64_t  born_at;        // epoch s
    int64_t  updated_at;     // epoch s — last time decay was applied
    uint8_t  fullness, happiness, energy, hygiene, health;   // 0..100
    uint8_t  stage;          // EGG, BABY, CHILD, TEEN, ADULT_GOOD, ADULT_BAD, DEAD
    uint8_t  asleep;         // lights off
    uint8_t  dirty;          // poop on screen
    uint16_t care_mistakes;
    int64_t  critical_since[5]; // per stat, 0 = not critical
    int64_t  next_poop_at, died_at;
} pet_t;
```

### 4.2 Time (`pet_apply(pet, now)`)

```
hours = (now - updated_at) / 3600, clamp to MAX_CATCHUP_H (7 days)
if asleep: energy += 40*h  else: energy -= 10*h
fullness -= 20*h · happiness -= 15*h
if now >= next_poop_at: dirty = 1, next_poop_at += ~3h
if any stat < CRITICAL or dirty: health -= 10*h
care mistakes: a stat below CRITICAL for > 30 min (tracked via critical_since)
stage: by age + care_mistakes (§4.4) · dead: health == 0 for > 2 h
updated_at = now
```

Elapsed-time form means: boot after 9 h off → one call, correct result. Same function runs
every second while awake with h ≈ 0.0003. `FAST_FORWARD` (build flag) multiplies `hours`.

### 4.3 Actions

| Action | Effect | Blocked when |
|---|---|---|
| feed meal | fullness +40 | fullness > 90, asleep, dead |
| feed snack | fullness +15, happiness +10, weight +1 | asleep, dead |
| play | happiness +25, energy −10 (mini-game is Phase 5; v1 = instant) | energy < 15, asleep |
| lights | toggle asleep | dead |
| clean | dirty = 0, hygiene = 100 | not dirty |
| medicine | health +30 | health ≥ 30 (not sick) |

### 4.4 Stages

| Stage | Enter when | Sprite set |
|---|---|---|
| egg | new pet | `egg_*` (wobble) |
| baby | age > 5 min | `baby_*` — decay × 1.5 |
| child | age > 65 min | `child_*` |
| teen | age > 3 d | `teen_*` |
| adult_good / adult_bad | age > 6 d, care_mistakes ≤ 3 / > 3 | `adult_*` / `adultbad_*` |
| dead | health 0 for > 2 h | `dead_*`; long-press → new egg |

Timings follow the original P1 (hatch 5 min, baby 65 min, ages 3 and 6 "years" where one
year = one day). We keep our richer stat set (energy, hygiene, health on top of the classic
hunger/happy) and a single good/bad adult branch rather than the P1's full rosters.

**Phase 0–4 ships baby only.** Stages are data (a table), so adding sets later is art, not code.

### 4.5 Face selection (`pet_face()` → `<stage>_<state>`)

`dead > sleeping > sick (health<30) > dirty > eating/playing (transient, 2 s) > sad (any stat<25)
> happy (all>60) > idle`. One image widget, swapped — exactly cloudagotchi's `set_face()`.

### 4.6 Screen (368 × 448)

Icon-only hero screen (v3, 2026-09-19 — see the `lvgl-ui` skill, `references/screens.md`).
No words at rest: hearts, the pet, one call, four candy tiles.

Monochrome rabbit, minimal chrome. The pet is the screen; a small **chatbox**
shows what it wants (an icon, subtle); giving **food** or **water** makes the item
appear in front of it and it **chews/drinks until it is gone** (a multi-frame
animation, not an instant stat bump); **sleep** switches the lights off and it naps.

```
┌────────────────────────────┐
│  (chatbox)                 │  food / water / sleep / ... icon, only when wanted
│            ( rabbit )      │  the hero; frame changes to eat / drink / sleep
│                            │
│   (food) (water) (sleep)   │  three round buttons, white icons, no captions
└────────────────────────────┘
```

Needs now include **thirst** (water) and **sleep** (a nap when energy is low),
ahead of boredom: `pet_need()` (pure, host-tested) returns MED > CLEAN > FOOD >
WATER > SLEEP > FUN. `pet_action_enabled()` drives each button. Tapping the
chatbox gives whatever it is asking for. Icons and the rabbit are a generated
white image set (`tools/sprites/make_icons.py` → RGB565A8). Settings hide behind a
long-press on the pet. A colour version lives in the mocked scene `A72ceSRfIgH`.

Touch rules from the field guide: parallax puts contact **15–25 px below** the target ⇒ every
hit zone extended ~20 px downward (`lv_obj_set_ext_click_area`), nothing under 40 px tall,
fire on press-edge or release-tap with a consumed guard, destructive actions release-only.
Log every touch at INFO permanently.

---

## 5. Hardware landmines (from the field guide — the ones that apply to us)

- **Never configure GPIO13** (TE). Latches the panel into corruption.
- **Don't light-sleep on USB.** Doze instead (screen off, CPU up). USB-JTAG does not survive wake.
- **Don't call `bsp_audio_codec_speaker_init()`** — boot-loops. Hand-roll I²S + `esp_codec_dev`, `pa_pin` GPIO46 (Phase 5).
- **AXP2101 answers with no battery, reporting 0 %.** Read REG 0x00 bit 3 (present) first, plus raw VBAT (0x34/0x35). Else low-battery UI fires forever on USB.
- **Panel can boot dark after a reflash** (reset line is on the expander; wedged state survives chip reset). Power-cycle via long PWR hold. BSP does the expander reset on boot; if black after flash, press reset once (cloudagotchi notes this too).
- **PMIC rail state persists across resets.** Don't touch rail enables (pocket-pet bricked the glass for an evening).
- **Panel IO isn't thread-safe.** All panel commands (brightness!) under `bsp_display_lock`.
- **Touch controller auto-sleeps after release and NACKs** — a failed read = "no finger", never "keep last".
- **IMU:** probe both addresses; axis orientation is per-unit — we only use magnitude (shake), so it doesn't matter.
- **Main task stack ≥ 6–8 KB.** Default 3.5 KB overflows on the first big local.
- **Oversized app "flashes successfully" then boots a stale slot.** Watch the size line; 12 MB app partition.
- **Touch parallax 15–25 px.** See §4.6.
- Keep `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240`, `COMPILER_OPTIMIZATION_PERF`, `SPIRAM_MODE_OCT`, `FREERTOS_HZ=1000`. cloudagotchi keeps PSRAM at 40 MHz on purpose (80 MHz + DIO corrupted the display for them) — start there.

---

## 6. Sprite pipeline (photo → flash)

1. **Generate** (one session, one reference): photo → chibi pixel-art character on flat `#00FF00`
   → sprite sheet 8 states × 3 frames for `baby`. Prompts in `tools/sprites/PROMPTS.md`.
   Tools that hold a character steady: PixelLab.ai (built for this), Midjourney `--cref`,
   ChatGPT image gen with the reference attached.
2. **Slice** → `tools/sprites/raw/baby_<state>_<n>.png`.
3. **Clean** — `prepare_sprites.py`: colour-key bg, crop, scale to 160 px nearest-neighbour,
   quantize ≤ 32 colours → `assets/sprites/*.png` + `manifest.json`.
4. **Convert** — `to_lvgl.py` wraps LVGL's `scripts/LVGLImage.py --cf RGB565A8` →
   `main/sprites/*.c` + regenerates `sprites.h` and the lookup table from the manifest.
5. **Build** — CMake globs `main/sprites/*.c`. `idf.py build` prints the app size; watch it.

Minimum for Phase 2: `baby_idle_{0,1,2}`. Minimum for Phase 3: + `happy, eating, sleeping,
sad, dirty, sick` × 2 = 15 images ≈ 1.2 MB.

---

## 7. Phases

### Phase 0 — Plan + repo  ✅ 2026-09-18
- [x] Research prior art (this doc, §2)
- [x] Approve: base = cloudagotchi `article-2`; toolchain = ESP-IDF 5.5; scope = baby stage, Phases 0–4
- [x] Fork `tagazok/cloudagotchi` → `marcusjhang/cloudagotchi` (public); branch `tamagotchi` from
      `article-2` @ `e1f21f3`; this plan committed; default branch set to `tamagotchi`
- **Done when:** repo exists, plan committed on the fork, nothing built yet.

### Phase 1 — It's alive  ✅ 2026-09-18
- [x] ESP-IDF v5.5 at `~/esp/esp-idf`, toolchain in `~/.espressif` (Python 3.14 env works)
- [x] Stripped `backend/`, `scripts/`, Wi-Fi, MQTT, certs, Kconfig; `main.c` = display + face + shake
- [x] `partitions.csv` 12 MB app; `sdkconfig.defaults` board/perf lines only
- [x] Fixed upstream: `article-2` referenced `face_talk_3` without shipping it → pulled from `main`
- [x] Build: 1.45 MB app, 88 % free. `dependencies.lock` committed (BSP 2.0.3, LVGL 9, co5300 2.2.0)
- [x] Flashed; boots; V2 detected; placeholder ghost face on screen
- [ ] → moved to Phase 3: `journal.c` (needs NVS init, which Phase 3 adds anyway)
- [ ] → moved to Phase 3: IMU logs a false "Shake detected!" 80 ms after boot — add a 1 s warm-up

### Phase 2 — Your sprite  ⏸ deferred (2026-09-18: "use the placeholder for now")
Phase 3 runs on cloudagotchi's placeholder faces; this phase slots in whenever art exists —
the tooling (`tools/sprites/`: PROMPTS.md, prepare_sprites.py, to_lvgl.py) is written and
proven end to end, so the dog sheet is a two-command drop-in.
State → placeholder mapping until then: idle/happy → `face_happy`, neutral → `face_neutral`,
sad/sick/dirty → `face_sad`, asleep → `face_sleeping`, startled → `face_talk_3`.
- [ ] `tools/sprites/PROMPTS.md`, `prepare_sprites.py`, `to_lvgl.py` (+ `requirements.txt`)
- [ ] Generate `baby_idle_{0,1,2}` from the photo; process; convert
- [ ] `sprites.h` + table: `const lv_image_dsc_t *sprite(stage, state, frame)`, `frames(stage, state)`
- [ ] `pet_ui.c`: replace the four faces with `lv_animimg` over the idle frames at 2×, antialias off;
      keep `set_face()` for one-shot states
- [ ] Delete cloudagotchi's `faces/`
- **Done when:** your pet breathes on the AMOLED, crisp pixels, no slivers after 10 min.
- **Effort:** 2 h code + your generation time.

### Phase 3 — Game loop  ✅ code done 2026-09-18, device-verified 2026-09-19
- [x] `config.h` with the numbers, each with a comment saying why (retuned after the sim: see below)
- [x] `pet.c`: `pet_new`, `pet_apply(now)`, `pet_act`, `pet_face` — pure, no ESP headers
- [x] `tools/sim/`: neglect / good owner (30 d) / lazy owner / one visit / edges, all asserting
- [x] `persist.c`: NVS blob, versioned; save on every action + 5 min timer
- [x] `game.c` task at 1 Hz → `pet_ui_update(snapshot)`
- [x] UI: action bar, 5 stat bars, poop blob, captions, toast, INFO overlay, long-press-on-dead → new egg
- [x] `-DFAST_FORWARD=60` wired into the build (`idf.py -DFAST_FORWARD=60 build`)
- [x] `journal.c`: boot reason + reset counter in NVS, printed at boot
- [x] IMU warm-up guard against the boot-time false shake
- [x] Sprites: placeholder faces via the Phase 2 mapping
- [x] **Verified on device** — lifecycle, all actions, snack, shake, INFO overlay, persistence
      across reset (2026-09-19)
- **Done when:** ignore it → sad → sick; feed/clean/medicine → happy; reset → same pet. Host sim green ✅.

What the sim changed (the first numbers were a chore): energy is never "neglect" (the pet naps);
cleaning resets the poop timer and poop is cosmetic for 2 h; sleep pauses needs (10 %); health
only recovers when every core stat > 50. Result: 3 visits/day → happy adult, 0 mistakes;
2 visits/day → alive but grumpy; 1 visit/day → dead in ~2 days; never touched → dead in ~34 h.

### Phase 4 — Time, sleep, battery  (code done 2026-09-19; battery-only paths still to verify)
- [x] `rtc.c`: PCF85063A on `bsp_i2c_get_handle()`; read; oscillator-stop flag; set from build
      time when invalid; seed `settimeofday`; `TZ` = Singapore (`<+08>-8`)
- [x] Boot order: NVS → RTC → `pet_load` → `pet_apply(now)` (catch-up, capped 7 d) → UI, plus a
      one-time re-anchor for pets carried over from Phase 3's uptime-based timestamps
- [x] `battery.c`: present bit, %, mV, VBUS — clock + battery in the status strip; "USB" when no cell
- [x] `power.c`: activity timer reset by touch / shake / button; 30 s → brightness 20 %;
      2 min → screen off; on VBUS **doze** (CPU up, USB alive, wake on any input); on battery
      persist → `esp_light_sleep_start` 300 ms slices, wake on BOOT + touch (GPIO21) + VBUS edge →
      `esp_restart()`
- [x] Wake while asleep on being picked up (`app_imu_moved()`), and a battery sample into the NVS
      journal ring every `BATT_SAMPLE_S` (300 s) so the overnight drain curve survives
- [x] Settings page stub (long-press INFO): time ±1 h, brightness cycle, reset pet (hold). Time and
      brightness exercised on device; reset button not yet
- [ ] Soak: unplug at night, plug in in the morning: pet hungry, clock right, journal shows
      one sleep entry and a battery curve
- **Done when:** the overnight test passes twice. Measure %/h dark; target < 5 %/h.
- **Effort:** 1–2 days; the soak tests gate it.
- **Verified on USB 2026-09-19:** clock seeds + retains while powered; pet re-anchors (age 0 h);
  battery reads (`present=0` with no cell); `power: idle 30s: dim`; `idle 120s: dozing on USB`.
  Wake-from-doze and the battery paths need the owner.

### Beyond (not planned in detail here)
Phase 5: full sprite set + stages + evolution, mini-game, sound (hand-rolled ES8311),
clock face. Phase 6: NTP, SPIFFS sprite packs, OTA, web status page.

---

## 8. Testing

| Layer | How | When |
|---|---|---|
| Game rules | `tools/sim/` host build of `pet.c`, assertions over simulated days | every change to `pet.c` / `config.h` (CI-able) |
| Pet E2E | `tools/e2e/` host build of `pet.c`: full lifecycle (egg → baby → child → teen → adult), needs, sleep, sickness, death/revive, catch-up, persistence round-trip, `pet_action_enabled` ↔ `pet_act` parity | every change to `pet.c` / `config.h` (CI-able) |
| UI | on device; `FAST_FORWARD=60`; touch log at INFO | Phase 3 |
| Power | journal in NVS: boot reason, sleep entries, battery samples; `tools/journal_dump.py` over serial | Phase 4 soaks |
| Health | free heap + `lv_mem` line every 60 s at INFO | always |

The two-strike rule (pocket-pet): two failed fixes for one symptom ⇒ the model is wrong;
build the instrument before the third patch.

---

## 9. Repo layout

```
.
├── PLAN.md
├── README.md                       (upstream's, rewritten in Phase 1; credits cloudagotchi, pixelcat, the guide)
├── LICENSE                         (upstream MIT — keep)
├── firmware/
│   ├── CMakeLists.txt · sdkconfig.defaults · partitions.csv · dependencies.lock
│   └── main/                       (§4)
├── assets/sprites/                 processed PNGs (source of truth for art)
└── tools/sprites/ · tools/sim/
(backend/ and scripts/ from upstream are deleted in Phase 1)
```

---

## 10. Commands

```bash
# once
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
. ~/esp/esp-idf/export.sh                       # each shell

# build / flash
cd firmware
idf.py set-target esp32s3 && idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor       # Ctrl+] exits; if the screen stays black, press reset once

# host sim
cc -O2 -DHOST -Ifirmware/main firmware/main/pet.c tools/sim/sim.c -o /tmp/sim && /tmp/sim

# sprites
python3 -m venv .venv && . .venv/bin/activate && pip install -r tools/sprites/requirements.txt
python3 tools/sprites/prepare_sprites.py && python3 tools/sprites/to_lvgl.py
```

---

## 11. Risks

| Risk | Mitigation |
|---|---|
| Board revision unknown until it arrives | BSP supports both; Phase 1 confirms. cloudagotchi + pixelcat both run on both. |
| Sprite drift between AI frames | One reference image, full sheets per session, regenerate the sheet not the cell. Start with 3 idle frames to learn the tool. |
| Flash size with many frames | 12 MB app partition; 160 px RGB565A8 = 77 KB; I8 fallback = 26 KB. |
| Dark-state battery drain | pocket-pet got 8 → 3 %/h; measure in the journal before optimising. |
| AMOLED burn-in | Idle dim → off is in Phase 4, not optional. |
| Light-sleep vs USB | Doze on VBUS; never sleep on USB (§5). |
| cloudagotchi upstream changes | `upstream` remote exists for cherry-picks; we never merge `main` (it's the cloud version). |

---

## 12. References

- cloudagotchi (MIT): https://github.com/tagazok/cloudagotchi · branch `article-2` · article: https://dev.to/aws/cloudagotchi-part-2-giving-it-a-face-sprites-and-touch-on-a-18-amoled-22nd
- pixelcat: https://github.com/toddsherman/pixelcat (`main/power.c`, `main/rtc.c`, `GAME_DESIGN.md`)
- pocket-pet: https://github.com/frolic/pocket-pet (`SLEEP-HANDOFF.md`, `CLAUDE.md`)
- Field guide: https://github.com/s0lness/awesome-esp32/blob/main/guides/waveshare-amoled-18.md
- Waveshare wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8 · repo: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8 · issue #3 (revisions): https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8/issues/3
- BSP: https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_8
- Other pets: TamaPetchi https://github.com/CyberXcyborg/ESP32-TamaPetchi · TamaFi https://github.com/cifertech/TamaFi · gameos https://github.com/MikeWilson/esp32-gameos
- LVGL 9: https://docs.lvgl.io/9.2/ (`lv_animimg`, `lv_image`, `LVGLImage.py`) · ESP-IDF 5.5: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/
