# Wiring — XIAO ESP32-C6 + 5 V relay module

## Connections

| XIAO pad | C6 GPIO | Relay module | Notes |
|----------|---------|--------------|-------|
| 5V       | —       | VCC          | USB VBUS passthrough; coil + opto draw ~70–90 mA |
| GND      | —       | GND          | |
| D1       | GPIO1   | IN           | `CONFIG_ESPSW_RELAY_GPIO`, default 1 |

Onboard user LED: GPIO15, active-low, blinks during Zigbee Identify
(`CONFIG_ESPSW_LED_GPIO`, -1 disables).

## Why open-drain

Typical modules: IN → series resistor → optocoupler LED → VCC (5 V), so the relay is ON
when IN is pulled to GND ("active-low"). A 3.3 V push-pull HIGH leaves ~1.7 V across
the LED path and can half-trigger the opto. Driving the pin **open-drain** removes the
current entirely when OFF (Hi-Z) and sinks it when ON. It also means the relay is OFF
from power-up until the firmware asserts it, because an unconfigured pin is an input.

If your module has an **H/L jumper** and you set it to H (active-high), clear
`CONFIG_ESPSW_RELAY_ACTIVE_LOW` in menuconfig: the pin becomes push-pull, HIGH = ON.

## Pins to avoid on the XIAO ESP32-C6

- GPIO8, GPIO9, GPIO15: strapping (15 is only used as an LED output after boot).
- GPIO3, GPIO14: onboard/external antenna select.
- GPIO0: ADC pin used for battery sense in ESPIR (kept free by convention).

## Mains safety

The relay contacts switch mains. Use a proper enclosure, keep the low-voltage side
separated from the mains side, and respect the module's 10 A rating with margin.
