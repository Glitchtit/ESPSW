/* ESPSW — Zigbee relay switch, application entry point.
 *
 * Boot order matters: NVS -> resolve power-on state -> drive the relay -> (Task 4)
 * start Zigbee. The relay settles long before the stack is up. */
#include "esp_log.h"
#include "sdkconfig.h"
#include "proto.h"
#include "relay.h"
#include "startup.h"
#include "store.h"
#include "zigbee.h"

static const char *TAG = "espsw";

void app_main(void)
{
    ESP_LOGI(TAG, "ESPSW %s booting, fw 0x%08x", ESPSW_MODEL, (unsigned)ESPSW_FW_VERSION);

    ESP_ERROR_CHECK(store_init());

    uint8_t mode = store_get_startup();
    bool last = store_get_state();
    bool boot_state = startup_resolve(mode, last);
    ESP_LOGI(TAG, "startup mode 0x%02x, last %d -> boot %d", mode, last, boot_state);

    relay_init(CONFIG_ESPSW_RELAY_GPIO, CONFIG_ESPSW_RELAY_ACTIVE_LOW);
    relay_set(boot_state);
    /* Persist the applied state so TOGGLE mode flips again on the next power cut. */
    if (boot_state != last) {
        store_set_state(boot_state);
    }

    zigbee_start(boot_state);
    ESP_LOGI(TAG, "ready");
}
