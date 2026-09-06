/* ota.h — Zigbee OTA Upgrade *client* (ported from ESPIR components/espir_ota).
 *
 * Streams received image blocks into the inactive OTA partition, switches the boot
 * partition and reboots on completion. The new image must call ota_mark_valid() once
 * healthy (we do it on network join) or the bootloader rolls back. */
#ifndef ESPSW_OTA_H
#define ESPSW_OTA_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_zigbee_core.h"

/* Build the OTA Upgrade client cluster from ESPSW_FW_VERSION / ESPSW_MANUF_CODE /
 * ESPSW_OTA_IMAGE_TYPE. Caller adds it to the endpoint cluster list with
 * esp_zb_cluster_list_add_ota_cluster(..., ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE). */
esp_zb_attribute_list_t *ota_cluster_create(void);

/* Enable periodic image queries on the given endpoint (call once joined). */
void ota_start(uint8_t endpoint);

/* Feed an ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID message. Returns ESP_OK to continue. */
esp_err_t ota_handle_value(const void *message);

/* Confirm the running image (cancels bootloader rollback) if it is pending verify. */
void ota_mark_valid(void);

#endif /* ESPSW_OTA_H */
