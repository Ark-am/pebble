# Epoch

A classic analog watchface. The minute track follows the edge of the screen,
the hands have a tapered leaf around the centre, and four small complications
sit around the centre:

| Position | Shows |
| --- | --- |
| Upper left | Current temperature and conditions for the phone's location |
| Upper right | Today's health data: steps, distance, active calories, active minutes, sleep, and heart rate (on watches with a sensor). **Tap the watch** to cycle through them. Not shown on Aplite, which has no health data. |
| Lower left | Day of the week and day of the month |
| Lower right | Battery level as a ring of ten segments. The dot in the middle is filled while the phone is connected and shows a crescent moon during Quiet Time. |

The watch vibrates twice if the phone disconnects, except during Quiet Time.

## Settings

Open the watchface's settings in the Pebble phone app:

| Setting | Options |
| --- | --- |
| Background | Light (default) or Dark |
| Hour numbers | Classic: 12, 2, 4, 6, 8 and 10, turned to follow the dial (default). Modern: 12, 3, 6 and 9, upright. |
| Temperature | Celsius (default) or Fahrenheit |
| Vibrate when the phone disconnects | On (default) or off |

Hours without a number get a long index line instead. The name "Pebble" sits
below the 12; change `DIAL_NAME` in `src/c/main.c` to show a different one.

The system fonts cannot be rotated, so the classic numbers are drawn as line
strokes from glyph outlines in `src/c/main.c`. Only the digits 0, 1, 2, 4, 6 and
8 are defined.

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the weather from [Open-Meteo](https://open-meteo.com/),
which needs no account or API key. The watch requests an update every 30
minutes and keeps the last reading if the phone is unavailable. Until the first
reading arrives, the temperature shows `--°`.

## Build

From this directory:

```sh
npm install     # settings page library (Clay), first time only
pebble build
```

The installable bundle is created at `build/epoch.pbw`.

## Test in the emulator

```sh
pebble install --emulator emery     # large rectangular, colour
pebble install --emulator chalk     # round
pebble install --emulator aplite    # black and white
pebble emu-app-config --emulator emery   # open the settings page
pebble emu-battery --emulator emery --percent 30
pebble emu-steps --emulator emery 8432      # set today's steps
pebble emu-tap --emulator emery             # cycle the health metric
pebble emu-bt-connection --emulator emery --connected no
```

## Install

Install `build/epoch.pbw` through the Pebble phone app, or directly from a
computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
