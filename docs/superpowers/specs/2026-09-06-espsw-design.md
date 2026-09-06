# ESPSW — Zigbee relay switch (ESP32-C6) — design

Date: 2026-09-06
Status: approved design, pre-implementation

## Goal

A single-channel mains relay controlled from Home Assistant over Zigbee2MQTT (Z2M),
built on a Seeed XIAO ESP32-C6 driving a generic 5 V relay module (IN / GND / VCC,
Songle-class 10 A 250 VAC contacts). It behaves like a smart plug: HA shows a `switch`
and a `power_on_behavior` select; state survives power cuts; firmware updates go over
Zigbee OTA.

The project was originally pitched on an ESP32-C3. The C3 has no 802.15.4 radio, so
Zigbee is impossible without a second chip; the ESP32-C6 (same part as ESPIR and ESPINK)
was chosen instead.

## Non-goals (YAGNI)

- No physical button, no status LED beyond Identify.
- No energy metering, no multi-channel support, no Groups/Scenes clusters.
- No sleep / power management — the device is USB powered and always on.
- No custom ZCL cluster; the standard On/Off cluster covers everything.

## Hardware

| XIAO pad | C6 GPIO | Relay module | Notes |
|----------|---------|--------------|-------|
| 5V       | —       | VCC          | USB VBUS passthrough; coil + optocoupler draw ~70–90 mA |
| GND      | —       | GND          | |
| D1       | GPIO1   | IN           | default; overridable via `CONFIG_ESPSW_RELAY_GPIO` |
| (onboard)| GPIO15  | user LED     | active-low; blinks during Zigbee Identify |

**Drive mode.** The relay GPIO is configured **open-drain**: driven low = relay ON
(module input pulled to GND), released (Hi-Z) = relay OFF (module's own pull-up to 5 V).
Rationale: these modules are almost universally active-low with a 5 V optocoupler LED
in series with a resistor to VCC. A 3.3 V push-pull HIGH leaves ~1.7 V across the LED
path and can partially trigger it; Hi-Z removes the current entirely. Open-drain also
guarantees the relay is OFF from power-up until firmware explicitly asserts it, because
the pin defaults to input.

`CONFIG_ESPSW_RELAY_ACTIVE_LOW` (default `y`) selects this. Setting it to `n` (modules
with an H/L jumper set to H) switches to push-pull active-high.

**Pins avoided:** GPIO8/9/15 strapping (15 only used as LED output after boot), GPIO0
(ADC, ESPIR battery-sense convention), GPIO3/14 (XIAO antenna select).

**Safety.** The relay module switches mains; contacts, creepage and the enclosure are the
builder's responsibility. The firmware default power-on behaviour is *previous state*, so
a load that was on before a power cut comes back on.

## Firmware architecture

Single ESP-IDF (v5.4+) app, target `esp32c6`, `esp-zigbee-lib` / `esp-zboss-lib` `~1.6.0`
as managed components. Layout mirrors ESPINK (single `main/`), with the OTA client
lifted from ESPIR's `espir_ota` component.

```
ESPSW/
  CMakeLists.txt            project(espsw)
  partitions.csv            dual-slot OTA layout (copied from ESPIR master)
  sdkconfig.defaults        router role, custom partitions, rollback, USB-Serial-JTAG console
  main/
    CMakeLists.txt
    idf_component.yml       esp-zigbee-lib / esp-zboss-lib ~1.6.0
    Kconfig.projbuild       ESPSW Configuration: relay GPIO, active-low, LED GPIO
    proto.h                 FW version, manufacturer code, OTA image type, model strings
    main.c                  boot sequence (NVS -> startup state -> relay -> zigbee)
    relay.c/.h              GPIO driver, no Zigbee dependency
    startup.c/.h            pure logic: (mode, last_state) -> boot state
    store.c/.h              NVS persistence of last state + startup mode
    zigbee.c/.h             stack, endpoint, clusters, action + signal handlers
    ota.c/.h                Zigbee OTA Upgrade client (from espir_ota)
  z2m/
    espsw.js                Z2M external converter
    ota/index.json          OTA index served from GitHub raw
  tools/make_ota.py         from ESPIR; wraps .bin into .ota and upserts index.json
  test/host/                host-compiled unit test for startup.c
  docs/
    wiring.md               table above + photos/notes
    bringup.md              hardware verification checklist
    superpowers/specs/      this file
  README.md, AGENTS.md, .gitignore
```

### Modules

**`relay`** — `relay_init(gpio, active_low)`, `relay_set(bool on)`, `relay_get()`.
Implements the open-drain / push-pull choice. Knows nothing about Zigbee or NVS.

**`startup`** — `bool startup_resolve(uint8_t mode, bool last_state)` where `mode` uses
the ZCL StartUpOnOff encoding: `0x00` off, `0x01` on, `0x02` toggle, `0xFF` previous.
Unknown values are treated as previous. Pure C, host-testable.

**`store`** — NVS namespace `espsw`, keys `state` (u8) and `startup` (u8). Defaults when
absent: `state=0`, `startup=0xFF`. Writes are immediate (a relay change is rare and the
NVS wear budget is irrelevant at this rate).

**`zigbee`** — owns the stack task. Configuration:

- Role: **Router** (`CONFIG_ZB_ZCZR=y`, `ESP_ZB_DEVICE_TYPE_ROUTER`). No `esp_zb_sleep_enable`,
  no PM.
- Endpoint **1**, HA profile, device id `ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID`.
- Clusters (all server role):
  - Basic: ZCL version, manufacturer `ESPSW`, model `ESPSW-1CH`, power source `0x01` mains.
  - Identify: standard.
  - On/Off: `OnOff` (bool, reportable) + `StartUpOnOff` (enum8, R/W, `0x4003`).
  - OTA Upgrade (client role): via `ota.c`.
- `esp_zb_core_action_handler`:
  - `SET_ATTR_VALUE` on On/Off cluster, attr `OnOff` → `relay_set`, `store` state.
  - `SET_ATTR_VALUE` on On/Off cluster, attr `StartUpOnOff` → `store` startup mode.
  - `OTA_UPGRADE_VALUE` → `ota_handle_value`.
- Signal handler:
  - `SKIP_STARTUP` → `BDB_MODE_INITIALIZATION`.
  - `DEVICE_FIRST_START` / `DEVICE_REBOOT`: factory-new → start network steering;
    otherwise mark joined, `ota_mark_valid()`, `ota_start()`.
  - `STEERING` success → joined, `ota_mark_valid()`, `ota_start()`; failure → retry
    steering with backoff 1 s → 2 s → … capped at 60 s, forever.
  - `ZDO_SIGNAL_LEAVE` → `esp_zb_factory_reset()` (wipes `zb_storage`) and reboot;
    the next boot steers again. This is the factory-reset path ("Remove device" in Z2M).
    A REJOIN-type leave (`ESP_ZB_NWK_LEAVE_TYPE_REJOIN`) is left to the stack instead —
    only other leave types trigger the factory reset.
  - Identify: `esp_zb_identify_notify_handler_register` toggles the LED at 2 Hz while
    identify time > 0.
- The `OnOff` attribute is seeded with the resolved boot state before `esp_zb_start`, so
  Z2M's first read after rejoin matches the relay. Attribute reporting on `OnOff` is
  configured by Z2M's converter (`m.onOff` sets up the binding + report config); the
  firmware only needs the attribute flagged reportable.

**`ota`** — `espir_ota.c` copied and renamed (`ota_cluster_create`, `ota_start`,
`ota_handle_value`, `ota_mark_valid`). Identical block-streaming into
`esp_ota_get_next_update_partition`, `esp_ota_end` + `esp_ota_set_boot_partition` on
finish, then `esp_restart`. `ota_mark_valid` calls `esp_ota_mark_app_valid_cancel_rollback`
once joined so a broken image that cannot join rolls back automatically.

**`main`** — `nvs_flash_init` → `store_load` → `startup_resolve` (power-on resets only,
gated on `esp_reset_reason()` being `ESP_RST_POWERON`, `ESP_RST_BROWNOUT` or
`ESP_RST_UNKNOWN`; any other reset reason keeps the stored last state unchanged, since
ZCL StartUpOnOff is defined for power-on only) → `relay_init` + `relay_set(boot_state)`
→ `store_save_state(boot_state)` (matters for `toggle` mode) → `zigbee_start(boot_state)`.
The relay is asserted before the Zigbee stack, so it settles well under 100 ms after
power returns.

### Identity constants (`proto.h`)

| Constant | Value | Note |
|----------|-------|------|
| `ESPSW_MANUF_CODE` | `0x1037` | same code as ESPIR so one Z2M OTA index can hold both |
| `ESPSW_OTA_IMAGE_TYPE` | `0x0010` | disjoint from ESPIR's 1/2/3 |
| `ESPSW_FW_VERSION` | `0x00010000` (v0.1.0) | packed `0xMMmmppbb`, bumped per release |
| `ESPSW_MANUF_NAME` | `"ESPSW"` | Basic cluster |
| `ESPSW_MODEL` | `"ESPSW-1CH"` | Basic cluster; must match `zigbeeModel` in `z2m/espsw.js` |
| `ESPSW_ENDPOINT` | `1` | |

`z2m/espsw.js` and `tools/make_ota.py` are the consumers; changing a constant means
changing all three (documented in AGENTS.md).

### Partition table

Copied from ESPIR master: `nvs` 24 K, `phy_init`, `otadata`, `ota_0` / `ota_1` 1.5 MB
each, `zb_storage` 16 K, `zb_fct` 4 K. Requires `CONFIG_PARTITION_TABLE_OFFSET=0x8000`
and 4 MB flash (XIAO C6 has 4 MB).

### sdkconfig.defaults (curated)

```
CONFIG_ZB_ENABLED=y
CONFIG_ZB_ZCZR=y
CONFIG_ZB_RADIO_NATIVE=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_OFFSET=0x8000
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
```

## Zigbee2MQTT converter (`z2m/espsw.js`)

```js
const m = require('zigbee-herdsman-converters/lib/modernExtend');
module.exports = {
    zigbeeModel: ['ESPSW-1CH'],
    model: 'ESPSW-1CH',
    vendor: 'ESPSW',
    description: 'Zigbee relay switch (XIAO ESP32-C6 + 5V relay module)',
    extend: [m.onOff({powerOnBehavior: true}), m.identify()],
    ota: true,
};
```

`m.onOff` yields `switch.<name>` and `select.<name>_power_on_behavior` (off / on /
toggle / previous) in HA through Z2M's MQTT discovery. No HA-side YAML.

Install: copy to `<z2m-data>/external_converters/espsw.js`, restart Z2M, pair.

## OTA release flow

Same as ESPIR, one product:

1. Bump `ESPSW_FW_VERSION` in `main/proto.h`.
2. `idf.py build`.
3. `python tools/make_ota.py --bin build/espsw.bin --proto main/proto.h --url-base
   https://raw.githubusercontent.com/Glitchtit/ESPSW/main/z2m/ota --out-dir z2m/ota
   --model ESPSW-1CH --image-type 16 --out-name espsw.ota`
4. Commit `z2m/ota/*` and push.
5. Z2M → OTA tab → Check / Update.

`make_ota.py` is copied from ESPIR with its define names parameterised (`--proto` header
is read for `ESPSW_FW_VERSION` / `ESPSW_MANUF_CODE`; the prefix becomes a `--prefix`
argument defaulting to `ESPSW`).

**Shared index constraint.** Z2M accepts a single `ota.zigbee_ota_override_index_location`.
If ESPIR already uses it, point Z2M at a *local* index file in its data directory whose
entries are the union of `ESPIR/z2m/ota/index.json` and `ESPSW/z2m/ota/index.json`
(entries carry absolute GitHub URLs, so a local index can reference both repos). This is
documented in `docs/bringup.md`; nothing in the firmware depends on it.

## Error handling

- Zigbee steering failure: retry with capped backoff, forever; relay keeps working from
  its stored state regardless of network status.
- NVS corrupted / version mismatch: erase + reinit (ESPINK pattern), falling back to
  `state=0`, `startup=previous`.
- OTA block write failure: abort the transfer (`esp_ota_abort`), stay on current image;
  Z2M reports the failure and can retry.
- New image fails to join within the rollback window: bootloader reverts on next reset
  because the image never called `mark_valid`.
- Relay GPIO misconfiguration (e.g. same pin as LED): Kconfig ranges restrict choices;
  no runtime check.

## Testing

**Host (`test/host`, `make -C test/host`)** — `startup_resolve` truth table: 4 modes ×
2 last-states plus an unknown mode value. Compiled with the host `cc`, no IDF.

**Hardware (`docs/bringup.md` checklist)**

1. Flash over USB-C, console on `/dev/ttyACM0`; relay is OFF at boot before join.
2. Pair with Z2M (permit join); device appears as `ESPSW-1CH` with switch + select.
3. Toggle from HA: relay clicks, HA state follows (attribute report).
4. Power-on behaviour: for each of off / on / toggle / previous, set the select, set
   the relay, pull USB, replug, confirm relay state and HA state.
5. Identify from Z2M: onboard LED blinks.
6. "Remove device" in Z2M: device leaves, reboots, re-enters pairing.
7. OTA: build v0.1.1, publish, update from Z2M, confirm version in Z2M and that a
   power cycle keeps the new image.

## Repository

- `git init` in `~/GIT/ESPSW`, commits as `Glitchtit <thomas@wredlund.com>`, no AI
  co-author trailers.
- Public GitHub repo `Glitchtit/ESPSW` is created at the OTA step (needed for the raw
  index URL), not before.
- `.gitignore` as ESPINK: `build/`, `sdkconfig`, `sdkconfig.old`, `dependencies.lock`,
  `managed_components/`, `test/host/build/`.
