/* ESPSW — Zigbee relay switch, application entry point. */
#include "esp_log.h"
#include "proto.h"

static const char *TAG = "espsw";

void app_main(void)
{
    ESP_LOGI(TAG, "ESPSW %s booting, fw 0x%08x", ESPSW_MODEL, (unsigned)ESPSW_FW_VERSION);
}
