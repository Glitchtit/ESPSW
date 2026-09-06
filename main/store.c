#include "store.h"
#include "startup.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "store";

#define NS          "espsw"
#define KEY_STATE   "state"
#define KEY_STARTUP "startup"

static nvs_handle_t s_nvs;
static bool         s_open;

esp_err_t store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_open(NS, NVS_READWRITE, &s_nvs);
    s_open = (err == ESP_OK);
    if (!s_open) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    }
    return err;
}

static uint8_t get_u8(const char *key, uint8_t dflt)
{
    uint8_t v = dflt;
    if (s_open && nvs_get_u8(s_nvs, key, &v) != ESP_OK) {
        v = dflt;
    }
    return v;
}

static void set_u8(const char *key, uint8_t v)
{
    if (!s_open) {
        return;
    }
    esp_err_t err = nvs_set_u8(s_nvs, key, v);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write %s failed: %s", key, esp_err_to_name(err));
    }
}

bool store_get_state(void)            { return get_u8(KEY_STATE, 0) != 0; }
void store_set_state(bool on)         { set_u8(KEY_STATE, on ? 1 : 0); }
uint8_t store_get_startup(void)       { return get_u8(KEY_STARTUP, STARTUP_PREVIOUS); }
void store_set_startup(uint8_t mode)  { set_u8(KEY_STARTUP, mode); }
