# Meridian

A clean analog watchface that shows everything the watch can report in four
complications around the dial. Each position can show any of them, or nothing,
from the settings; by default they are:

| Position | Shows |
| --- | --- |
| 12 o'clock | Current temperature (°C or °F) and conditions for the phone's location |
| 3 o'clock | Date and day of the week |
| 6 o'clock | Today's health data: steps, distance, active calories, active minutes, sleep, and heart rate (on watches with a sensor). **Tap the watch** to cycle through them. |
| 9 o'clock | Battery percentage and gauge (red when low, green while charging), plus alerts for a disconnected phone and Quiet Time |

The watch vibrates twice if the phone disconnects, except during Quiet Time.
Aplite has no health data, so its health position shows the month and year.

On rectangular watches the minute track follows the edge of the screen; on
round watches it follows the circle. When a Timeline Quick View covers part of
the screen, the dial shrinks to fit the remaining area.

The whole face can be turned to suit how the watch is worn: set **Rotate the
dial** in the settings, from -180 to 180 degrees in steps of 5 (positive is
clockwise). The ticks, 12 o'clock marker, hour numbers, information and hands
all turn together; the text stays upright.

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the weather from [Open-Meteo](https://open-meteo.com/),
which needs no account or API key. The watch requests an update every 30
minutes and keeps the last reading if the phone is unavailable. When no
position shows the weather, the phone does not ask for the location at all.

## Build

From this directory:

```sh
pebble build
```

The installable bundle is created at `build/meridian.pbw`.

## Test in the emulator

```sh
pebble install --emulator basalt --logs   # rectangular, colour
pebble install --emulator chalk           # round
pebble install --emulator aplite          # black and white, no health
pebble install --emulator emery           # large rectangular
```

Useful checks in a second terminal:

```sh
pebble emu-battery --emulator basalt --percent 15          # low battery (red)
pebble emu-battery --emulator basalt --percent 60 --charging
pebble emu-bt-connection --emulator basalt --connected no  # disconnected alert
pebble emu-steps --emulator basalt 8432                    # set today's steps
pebble emu-heart-rate --emulator emery 72                  # heart rate (emery)
pebble emu-tap --emulator basalt                           # cycle health metric
pebble emu-set-timeline-quick-view --emulator basalt on    # Quick View layout
```

The emulator's location lookup may fail, in which case the weather shows
`--°` until a location is available.

Disconnecting Bluetooth also cuts the SDK's own link to the emulator, so check
the alert in the emulator window rather than with `pebble screenshot`. If later
commands time out, run `pebble kill` and install again.

## Install

Install `build/meridian.pbw` through the Pebble phone app, or directly from a
computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
