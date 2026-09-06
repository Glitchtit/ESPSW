/* proto.h — ESPSW identity constants.
 *
 * Single source of truth shared by the firmware, z2m/espsw.js (zigbeeModel) and
 * tools/make_ota.py (reads ESPSW_FW_VERSION / ESPSW_MANUF_CODE from this file).
 * Change one, change all three. */
#ifndef ESPSW_PROTO_H
#define ESPSW_PROTO_H

/* Zigbee manufacturer code. Same as ESPIR so one Z2M OTA index can serve both. */
#define ESPSW_MANUF_CODE      0x1037

/* OTA image type advertised by the OTA client cluster. Disjoint from ESPIR's 1/2/3. */
#define ESPSW_OTA_IMAGE_TYPE  0x0010u

/* Firmware version, packed 0xMMmmppbb (major, minor, patch, build). Bump per release. */
#define ESPSW_FW_VERSION      0x00010000u

#define ESPSW_MANUF_NAME      "ESPSW"
#define ESPSW_MODEL           "ESPSW-1CH"
#define ESPSW_ENDPOINT        1

#endif /* ESPSW_PROTO_H */
