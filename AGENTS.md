# AGENTS.md — ESPSW build/test reference

XIAO ESP32-C6 Zigbee relay switch. One ESP-IDF app in `main/`. Target chip:
**esp32c6**. Toolchain: **ESP-IDF v5.4+** at `~/esp/esp-idf`.

## Environment

```sh
. ~/esp/esp-idf/export.sh
```

`esp-zigbee-lib` / `esp-zboss-lib` are **managed components** fetched from
`main/idf_component.yml` on first build. Do not vendor them.

## Build / flash / monitor

```sh
idf.py set-target esp32c6      # once / after fullclean
idf.py build
idf.py -p /dev/ttyACM0 flash monitor   # console is the native USB-Serial-JTAG
idf.py fullclean               # after major config changes
```

## Tests

```sh
make -C test/host              # pure-logic unit tests, no IDF needed
python tools/test_make_ota.py  # make_ota.py self-test, no IDF and no extra packages
```

Hardware checklist: `docs/bringup.md`.

## Configuration

`sdkconfig.defaults` holds the curated defaults (router role, custom partitions,
rollback). Project knobs under **ESPSW Configuration** in `idf.py menuconfig`:
relay GPIO, relay active-low, LED GPIO, Zigbee TX power (default 10 dBm; see README).

## Identity contract

`main/proto.h` is the single source of truth for the manufacturer code (`0x1037`),
OTA image type (`0x0010`), firmware version and model string `ESPSW-1CH`.
`z2m/espsw.js` (`zigbeeModel`) and `tools/make_ota.py` (reads the header) depend on
it — change one, change all.

## OTA release

1. Bump `ESPSW_FW_VERSION` in `main/proto.h` (packed `0xMMmmppbb`; v0.1.0 → v0.1.1 is
   `0x00010000` → `0x00010100`).
2. `idf.py build`
3. ```sh
   python tools/make_ota.py --bin build/espsw.bin --proto main/proto.h \
     --url-base https://raw.githubusercontent.com/Glitchtit/ESPSW/main/z2m/ota \
     --out-dir z2m/ota --model ESPSW-1CH --image-type 16 --out-name espsw.ota
   ```
4. Commit `z2m/ota/espsw.ota` + `z2m/ota/index.json`, push.
5. Z2M → device → OTA → Check, then Update. The device reboots into the new image
   and confirms it on rejoin; if it cannot join, the bootloader rolls back.

## Conventions

- Zigbee role: Router (`CONFIG_ZB_ZCZR`). No sleep.
- Commits authored as `Glitchtit <thomas@wredlund.com>`. No AI co-author trailers.
