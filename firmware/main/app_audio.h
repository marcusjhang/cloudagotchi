#pragma once

#include "esp_err.h"

/** Initialize the ES8311 codec + speaker via the BSP. */
esp_err_t app_audio_init(void);

/**
 * Hardware self-test: play a short generated beep (no network needed).
 * Not called in normal operation — handy when debugging the speaker path:
 * drop a call into app_main() after app_audio_init().
 */
esp_err_t app_audio_beep(void);

/**
 * Download a WAV file over HTTPS and play it through the speaker,
 * streaming chunk by chunk (no need to fit the file in RAM).
 */
esp_err_t app_audio_play_url(const char *url);
