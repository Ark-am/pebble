# Sketchy Weather Analog

An analog watchface drawn in a pencil-sketch style. A hand-drawn weather
widget sits in the centre of the dial: an icon for the current conditions,
with the date and temperature underneath. The hour and minute hands start
just outside a sketched ring around the widget, so they never cover it.

| Conditions | Icon |
| --- | --- |
| Clear | Shaded sun by day, crescent moon at night |
| Fair | Sun or moon behind a cloud |
| Cloudy | Two clouds |
| Fog | Cloud over mist lines |
| Drizzle, rain, showers | Cloud with drops (showers add the sun or moon) |
| Snow | Cloud with snowflakes |
| Storm | Cloud with a lightning bolt |

Lines are drawn twice, slightly offset, like a pencil going back over a
stroke. The sun and moon are cross-hatched. On colour watches the sun is
amber, the moon pale yellow, and clouds and rain blue. On black-and-white
watches the shapes are outlined and the hatching gives them their tone.

At 10:30 a struck-through Bluetooth mark shows while the phone is
disconnected, and a crescent shows during Quiet Time. A battery gauge at
1:30 appears when the battery is low (20% or less) or charging, or all the
time if you choose. The watch vibrates twice when the phone disconnects,
except during Quiet Time.

On 144-pixel-wide watches (Aplite, Basalt, Diorite, Flint) and on Chalk, the
temperature is shown as just the number and degree sign (`70°`) to fit inside
the ring. Emery and Gabbro add the unit (`70°F`).

## Settings

- **Background:** Paper (light) or Chalkboard (dark)
- **Battery:** shown when low or charging, or always
- **Temperature:** Celsius or Fahrenheit
- **Vibrate when the phone disconnects**

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the current weather from
[Open-Meteo](https://open-meteo.com/), which needs no account or API key. The
watch requests an update every 30 minutes and keeps the last reading if the
phone is unavailable. Until the first reading arrives, the widget shows an
empty cloud and `--°`.

## Build

From this directory:

```sh
npm install
pebble build
```

The installable bundle is created at `build/sketchy-weather-analog.pbw`.

## Test in the emulator

```sh
pebble install --emulator basalt --logs   # rectangular, colour
pebble install --emulator chalk           # round
pebble install --emulator aplite          # black and white
pebble install --emulator emery           # large rectangular
```

Useful checks in a second terminal:

```sh
pebble emu-battery --emulator basalt --percent 15          # low battery
pebble emu-battery --emulator basalt --percent 60 --charging
pebble emu-bt-connection --emulator basalt --connected no  # disconnected alert
pebble emu-set-timeline-quick-view --emulator basalt on    # Quick View layout
```

## Install

Install `build/sketchy-weather-analog.pbw` through the Pebble phone app, or
directly from a computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
