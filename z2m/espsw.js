/* Zigbee2MQTT external converter for ESPSW (XIAO ESP32-C6 + 5 V relay module).
 *
 * Install: copy to <z2m-data>/external_converters/espsw.js and restart Z2M (check the
 * log for "loaded external converter"). Requires Z2M 2.x (modernExtend API).
 *
 * The firmware uses only standard clusters, so this is just a model mapping:
 *   - On/Off server -> switch + select.power_on_behavior (StartUpOnOff 0x4003)
 *   - Identify -> identify button (blinks the onboard LED)
 *   - ota: true -> Z2M offers updates from the configured OTA index
 * zigbeeModel MUST equal ESPSW_MODEL in main/proto.h. */

const m = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['ESPSW-1CH'],
    model: 'ESPSW-1CH',
    vendor: 'ESPSW',
    description: 'Zigbee relay switch (XIAO ESP32-C6 + 5V relay module)',
    extend: [
        m.onOff({powerOnBehavior: true}),
        m.identify(),
    ],
    ota: true,   // top-level property, NOT m.ota() (see ESPIR z2m/espir.js note)
};

module.exports = definition;
