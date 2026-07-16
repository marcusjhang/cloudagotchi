/*
 * Speaker output: stream a WAV from a URL straight to the ES8311 codec.
 *
 * Because the cloud sends 16 kHz 16-bit mono PCM (Polly's `pcm` output
 * wrapped in a WAV header), there is no decoder here at all — we skip the
 * 44-byte header and shovel samples into the codec as they arrive.
 */
#include "app_audio.h"

#include <math.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp32_s3_touch_amoled_1_8.h"

static const char *TAG = "app_audio";

#define WAV_HEADER_BYTES 44
#define CHUNK_BYTES      16384   // bigger reads = fewer TLS round-trips; the
                                 // Wi-Fi side must sustain 32 KB/s or we underrun

static esp_codec_dev_handle_t s_speaker;

esp_err_t app_audio_init(void)
{
    s_speaker = bsp_audio_codec_speaker_init();
    if (s_speaker == NULL) {
        ESP_LOGE(TAG, "speaker codec init failed");
        return ESP_FAIL;
    }

    // Open once and keep it open — the codec (and its power amp) stay ready.
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_err_t err = esp_codec_dev_open(s_speaker, &fs);
    ESP_LOGI(TAG, "codec open: %s", esp_err_to_name(err));
    err = esp_codec_dev_set_out_vol(s_speaker, 80);
    ESP_LOGI(TAG, "set volume: %s", esp_err_to_name(err));
    return ESP_OK;
}

esp_err_t app_audio_beep(void)
{
    // 300 ms of a 880 Hz sine at 16 kHz — pure math, no network, no display.
    // If you hear a clean beep, the codec + amp + speaker chain works.
    const int samples = 16000 * 3 / 10;
    int16_t *buf = malloc(samples * sizeof(int16_t));
    if (buf == NULL) return ESP_ERR_NO_MEM;

    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(8000.0f * sinf(2.0f * (float)M_PI * 880.0f * i / 16000.0f));
    }

    esp_err_t err = esp_codec_dev_write(s_speaker, buf, samples * sizeof(int16_t));
    ESP_LOGI(TAG, "beep: %s", esp_err_to_name(err));
    free(buf);
    return err;
}

esp_err_t app_audio_play_url(const char *url)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach, // trust store for S3's TLS cert
        .timeout_ms = 15000,
        // Presigned S3 URLs are ~2 KB; the default 512-byte TX buffer can't
        // even hold the GET request line ("Out of buffer" at open).
        .buffer_size_tx = 4096,
        .buffer_size = 4096,
    };
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    if (http == NULL) return ESP_FAIL;

    esp_err_t err = esp_http_client_open(http, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "http open failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    int64_t len = esp_http_client_fetch_headers(http);
    int status = esp_http_client_get_status_code(http);
    ESP_LOGI(TAG, "Briefing incoming — HTTP %d, %lld bytes", status, len);

    static char buf[CHUNK_BYTES];
    int skipped = 0;
    int total = 0;
    int64_t t_start = esp_timer_get_time();
    int64_t last_report = t_start;

    while (true) {
        int n = esp_http_client_read(http, buf, sizeof(buf));
        if (n <= 0) break;
        total += n;

        // Throughput report every ~2s: audio needs a steady 32 KB/s.
        int64_t now = esp_timer_get_time();
        if (now - last_report > 2000000) {
            ESP_LOGI(TAG, "streaming: %d KB in, avg %d KB/s",
                     total / 1024, (int)(total * 1000000LL / (now - t_start) / 1024));
            last_report = now;
        }

        // Skip the 44-byte WAV header; everything after is raw PCM.
        int offset = 0;
        if (skipped < WAV_HEADER_BYTES) {
            offset = WAV_HEADER_BYTES - skipped;
            if (offset > n) { skipped += n; continue; }
            skipped = WAV_HEADER_BYTES;
        }

        // The codec write BLOCKS until the I2S DMA has room — that both
        // paces this loop to the audio rate and yields the CPU, so no
        // artificial sleep is needed (one would starve the stream and
        // make the ghost stutter like a robot; ask me how I know).
        esp_err_t w = esp_codec_dev_write(s_speaker, buf + offset, n - offset);
        if (w != ESP_OK) {
            ESP_LOGE(TAG, "codec write failed: %s", esp_err_to_name(w));
            break;
        }
    }
    ESP_LOGI(TAG, "Briefing done — %d bytes streamed", total);

cleanup:
    esp_http_client_close(http);
    esp_http_client_cleanup(http);
    return err;
}
