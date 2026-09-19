# Review context — esp32-tamagotchi

Firmware for a Waveshare ESP32-S3-Touch-AMOLED-1.8 (ESP-IDF v5.5, C, LVGL 9, vendor BSP).
It is a self-contained virtual pet: no cloud, no Wi-Fi. Review for correctness on an
embedded target, not for web/cloud patterns.

## Severity
- P1 = critical/high: memory safety, use-after-free, buffer overrun, data races/deadlock,
  hardware misuse, data loss, wrong game/time logic, resource leak that grows unbounded.
- P2 = medium; P3 = low. Only P1 blocks a commit.

## Invariants to check
- **Pure code stays pure.** `firmware/main/pet.c/.h`, `config.h`, `hw_math.h` must not
  include ESP-IDF headers — they compile on the host for tests. No `#include "esp_*"`,
  `freertos/*`, `driver/*`, `lvgl.h` there.
- **Lock discipline.** LVGL calls run under `bsp_display_lock()`. Never hold the game mutex
  (`s_lock` in `game.c`) while taking the display lock, and never call `pet_ui_*` while
  holding `s_lock`. Snapshot under the lock, publish outside it.
- **Power.** Never light-sleep on USB (USB-JTAG does not survive wake). `power.c` must only
  light-sleep when VBUS is absent (or the PMIC read fails closed = treat as USB). Wake paths
  end in `esp_restart()` through the known-good boot.
- **Pins.** Never configure GPIO13 (panel TE). Touch INT is GPIO21, BOOT is GPIO0.
- **Audio.** Do not call `bsp_audio_codec_speaker_init()` (boot-loops). Sound is Phase 5.
- **I2C.** New `i2c_master` API: add each device once via `bsp_i2c_get_handle()`; every
  transfer has a bounded timeout; check `esp_err_t` return values.
- **NVS.** Versioned blob; check every return; no unbounded growth; save is idempotent.
- **Time.** int64 epoch seconds everywhere; catch-up capped (`MAX_CATCHUP_S`); guard the
  clock going backwards; migration re-anchors pre-RTC pets without charging decay.
- **Memory.** LVGL objects are created once at startup; the 1 Hz tick must not allocate on
  the heap per tick. Watch the free-heap log line.
- **Tests.** New pure logic gets a host test; `tools/check.sh` must be green.

## Not in scope
- Style nits, naming, comment wording — not P1.
- Wi-Fi/MQTT/AWS — the cloud half was removed on purpose; do not ask for network features.
