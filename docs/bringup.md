# Bring-up checklist

Tick each item on real hardware. Console: `idf.py -p /dev/ttyACM0 monitor`.

## 1. Flash and boot

- [x] `idf.py -p /dev/ttyACM0 flash monitor` succeeds.
- [x] Log shows `power-on: applying startup mode 0xff, last 0 -> boot 0` and `relay OFF`; no relay click.
- [x] Log shows `factory new; starting network steering`.
- [x] No `ESP_ERROR_CHECK` abort in the first console lines — in particular
      `esp_zb_on_off_cluster_add_attr` for StartUpOnOff (`0x4003`) returned `ESP_OK`.

## 2. Pair with Zigbee2MQTT

- [x] `z2m/espsw.js` is in `<z2m-data>/external_converters/`; Z2M log says
      `loaded external converter`.
- [x] Note: on this setup Z2M's data dir is on the Unraid NAS and `external_converters/`
      is root-owned over NFS — copy the file from the Unraid side (or the HA file
      editor), not from a workstation NFS mount.
- [x] Permit join → device log `joined network successfully`; Z2M shows `ESPSW-1CH`.
- [x] HA has `switch.<name>` and `select.<name>_power_on_behavior`.

## 3. Switching

- [x] HA switch ON → relay clicks, log `relay ON`, HA state ON.
- [x] HA switch OFF → relay releases, log `relay OFF`, HA state OFF.
- [x] The device log's `boot N` state (from step 1) equals Z2M's first reported state
      right after pairing — confirms the stack did not itself re-apply StartUpOnOff to
      the OnOff attribute.

## 4. Power-on behaviour (pull USB, replug for each)

This table applies to USB power cuts only. A soft reset (OTA update, "Remove device",
a crash/panic reboot) keeps the last state by design — StartUpOnOff is only applied
on `ESP_RST_POWERON` / `ESP_RST_BROWNOUT` / `ESP_RST_UNKNOWN` / `ESP_RST_USB` (a USB reflash therefore counts as a power cycle).

| select value | relay before cut | expected after replug |
|--------------|------------------|-----------------------|
| off          | ON               | OFF                   |
| on           | OFF              | ON                    |
| toggle       | OFF              | ON (and OFF on the next cut) |
| previous     | ON               | ON                    |

- [ ] All four rows pass and HA state matches the relay after rejoin.
      (2026-09-06: `on` verified via a USB-JTAG reset — relay boots ON and HA follows; the
      other three rows and a true USB power cut are still untested.)

## 5. Identify

- [x] Z2M → Identify: onboard LED blinks at 2 Hz for the identify time, then stops.

## 6. Factory reset via Z2M

- [ ] "Remove device" → log `left network ... factory reset + reboot`; device reboots
      and, with permit-join open, pairs again.

## 7. OTA

- [ ] Bump `ESPSW_FW_VERSION` to `0x00010100`, build, run `tools/make_ota.py`, commit and
      push `z2m/ota/`.
- [ ] Z2M is configured to serve the index (see below). OTA → Check shows an update.
- [ ] Update → device logs `OTA start`, then `OTA complete, rebooting`; after rejoin
      `new image confirmed valid (rollback cancelled)` and `fw 0x00010100`.
- [ ] Power cycle → still on `0x00010100`.

## Sharing the Z2M OTA index with ESPIR

Z2M accepts one `ota.zigbee_ota_override_index_location`. If ESPIR already uses it,
point Z2M at a **local** file in its data directory (e.g. `ota_index.json`) that lists
the union of both repos' `z2m/ota/index.json` entries. Entries carry absolute GitHub
URLs, so one local index can reference both repos. Keep manufacturerCode+imageType
unique: ESPIR uses 1/2/3, ESPSW uses 16.
