/* zigbee.h — Zigbee router transport: endpoint 1 with Basic, Identify, On/Off
 * (server) and, from Task 6, the OTA Upgrade client. */
#ifndef ESPSW_ZIGBEE_H
#define ESPSW_ZIGBEE_H

#include <stdbool.h>

/* Start the Zigbee stack task. initial_on_off seeds the OnOff attribute so the
 * coordinator's first read matches the relay that main() already asserted. */
void zigbee_start(bool initial_on_off);

#endif /* ESPSW_ZIGBEE_H */
