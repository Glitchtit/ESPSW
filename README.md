# ESPSW — Zigbee relay switch for Home Assistant

A single-channel mains relay controlled from **Home Assistant** over **Zigbee2MQTT**,
built on a **Seeed XIAO ESP32-C6** driving a generic 5 V relay module. It behaves like
a smart plug: HA gets a `switch` and a `power_on_behavior` select (off / on / toggle /
previous), the state survives power cuts, and firmware updates arrive over Zigbee OTA.

Design spec: `docs/superpowers/specs/2026-09-06-espsw-design.md`.

## Topology

```
Home Assistant <--MQTT--> Zigbee2MQTT (z2m/espsw.js) <--Zigbee--> XIAO ESP32-C6 (Router)
                                                                     |  GPIO1 (open-drain)
                                                                     v
                                                              5 V relay module --> mains load
```

Standard ZCL only: On/Off server (with StartUpOnOff), Identify, OTA client. No custom
cluster, so the Z2M converter is a plain model mapping.

## Hardware

| XIAO pad | C6 GPIO | Relay module |
|----------|---------|--------------|
| 5V       | —       | VCC          |
| GND      | —       | GND          |
| D1       | GPIO1   | IN           |

The relay pin is driven **open-drain** (sink = ON, Hi-Z = OFF) because these modules
are active-low with a 5 V optocoupler; see `docs/wiring.md` for why and for the
active-high alternative. The onboard user LED (GPIO15) blinks during Zigbee Identify.

**Mains warning:** the relay switches 230 V. Enclosure, creepage and fusing are on you.

## Build & flash

```sh
. ~/esp/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Pins and relay polarity: `idf.py menuconfig` → **ESPSW Configuration**.
Zigbee transmit power lives there too (`ESPSW_ZB_TX_POWER_DBM`, default 10 dBm): each
transmit is a ~300 mA burst at 20 dBm, and a weak 12 V supply shows it as an LED dip every
15 s; lower the value before blaming the wiring, raise it only if link quality suffers.

## Host tests

```sh
make -C test/host        # power-on state resolver truth table
```

## Zigbee2MQTT setup

1. Copy `z2m/espsw.js` to `<z2m-data>/external_converters/`.
2. Restart Z2M; confirm "loaded external converter" in the log.
3. Enable permit-join. The device appears as `ESPSW-1CH`; HA gets
   `switch.<name>` and `select.<name>_power_on_behavior`.

**Factory reset:** "Remove device" in Z2M. The device wipes its network data, reboots
and re-enters pairing. There is no button.

## OTA updates

See `AGENTS.md` → *OTA release*. Z2M needs its OTA index pointed at
`z2m/ota/index.json` (see `docs/bringup.md` for sharing the index with ESPIR).

## Layout

```
main/         firmware: relay driver, startup resolver, NVS store, Zigbee, OTA client
z2m/          Zigbee2MQTT external converter + OTA index
tools/        make_ota.py (wrap .bin -> .ota, update index)
test/host/    host-compiled unit tests
docs/         wiring, bring-up checklist, design spec + plan
```
