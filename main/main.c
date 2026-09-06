/* ESPSW — Zigbee relay switch, application entry point.
 *
 * Boot order matters: NVS -> resolve power-on state (power-on resets only) -> drive
 * the relay -> start Zigbee. The relay settles long before the stack is up. */
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "proto.h"
#include "relay.h"
#include "startup.h"
#include "store.h"
#include "zigbee.h"

static const char *TAG = "espsw";

/* ZCL's StartUpOnOff is defined for power-on only; a software restart (OTA,
 * factory reset, panic reboot) must keep whatever state was last stored. A reset
 * issued through the native USB-Serial-JTAG peripheral (ESP_RST_USB) is what
 * `idf.py flash` produces on the XIAO ESP32-C6, so a reflash is treated as a fresh
 * start too: StartUpOnOff is applied and the boot log shows it. */
static bool is_power_cycle(esp_reset_reason_t reason)
{
    return reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT || reason == ESP_RST_UNKNOWN ||
           reason == ESP_RST_USB;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESPSW %s booting, fw 0x%08x", ESPSW_MODEL, (unsigned)ESPSW_FW_VERSION);

    ESP_ERROR_CHECK(store_init());

    uint8_t mode = store_get_startup();
    bool last = store_get_state();

    esp_reset_reason_t reason = esp_reset_reason();
    bool boot_state;
    if (is_power_cycle(reason)) {
        boot_state = startup_resolve(mode, last);
        ESP_LOGI(TAG, "reset reason %d: power-on: applying startup mode 0x%02x, last %d -> boot %d",
                 reason, mode, last, boot_state);
    } else {
        boot_state = last;
        ESP_LOGI(TAG, "reset reason %d: soft reset: keeping last state %d", reason, last);
    }

#ifdef CONFIG_ESPSW_RELAY_ACTIVE_LOW
    const bool active_low = true;
#else
    const bool active_low = false;
#endif
    relay_init(CONFIG_ESPSW_RELAY_GPIO, active_low);
    relay_set(boot_state);
    /* Persist the applied state so TOGGLE mode flips again on the next power cut. */
    if (boot_state != last) {
        store_set_state(boot_state);
    }

    zigbee_start(boot_state);
    ESP_LOGI(TAG, "ready");
}
