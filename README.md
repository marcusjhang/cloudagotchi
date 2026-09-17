# esp32-tamagotchi

A virtual pet for the **Waveshare ESP32-S3-Touch-AMOLED-1.8** (368×448 AMOLED, touch,
RTC, IMU, speaker, battery). Sprites are AI-generated from a photo. Everything runs on
the device — no cloud, no Wi-Fi.

**Status:** Phase 1 — cloud stripped, builds offline. See [PLAN.md](PLAN.md).

## Where the code comes from

This is a fork of [cloudagotchi](https://github.com/tagazok/cloudagotchi) by Olivier Leplus
(MIT), branched from its `article-2` state — ESP-IDF + Waveshare BSP + LVGL 9 on this exact
board. Its AWS brain is removed; the pet's logic lives on the chip. Sleep/RTC patterns are
informed by [pixelcat](https://github.com/toddsherman/pixelcat) and the
[Waveshare AMOLED 1.8 field guide](https://github.com/s0lness/awesome-esp32/blob/main/guides/waveshare-amoled-18.md).

## Build

```bash
. ~/esp/esp-idf/export.sh            # ESP-IDF v5.5
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor   # Ctrl+] exits; black screen after flash → press reset once
```

First build downloads the Waveshare BSP (`waveshare/esp32_s3_touch_amoled_1_8`) and the
QMI8658 driver via the IDF Component Manager — needs internet.

## Layout

| Path | What |
|---|---|
| `firmware/main/` | ESP-IDF app: `main.c`, `pet_ui.c` (LVGL screen), `app_imu.c` (shake), `faces/` (placeholder sprites) |
| `firmware/partitions.csv` | 12 MB app partition — sprites compile in |
| `PLAN.md` | the plan, phase by phase |
