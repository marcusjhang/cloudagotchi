#include "persist.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "persist";

#define NS  "pet"
#define KEY "state"

bool persist_load(pet_t *out)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;  // first boot
    }
    size_t len = sizeof(*out);
    const esp_err_t err = nvs_get_blob(h, KEY, out, &len);
    nvs_close(h);
    if (err != ESP_OK) {
        return false;
    }
    if (len != sizeof(*out) || out->version != PET_SCHEMA_VERSION) {
        ESP_LOGW(TAG, "stored pet is schema %lu / %u bytes, want %u / %u: starting over",
                 (unsigned long)out->version, (unsigned)len, PET_SCHEMA_VERSION, (unsigned)sizeof(*out));
        return false;
    }
    return true;
}

bool persist_save(const pet_t *p)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, KEY, p, sizeof(*p));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
    return err == ESP_OK;
}

void persist_erase(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
}
