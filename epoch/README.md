# Epoch

A classic analog watchface. The minute track follows the edge of the screen,
the hands have a tapered leaf around the centre, and two small complications
sit below the centre:

| Position | Shows |
| --- | --- |
| Lower left | Day of the week and day of the month |
| Lower right | Battery level as a ring of ten segments, with a dot in the middle that is filled while the phone is connected |

## Settings

Open the watchface's settings in the Pebble phone app:

| Setting | Options |
| --- | --- |
| Background | Light (default) or Dark |
| Hour numbers | Classic: 12, 2, 4, 6, 8 and 10, turned to follow the dial (default). Modern: 12, 3, 6 and 9, upright. |

Hours without a number get a long index line instead.

The system fonts cannot be rotated, so the classic numbers are drawn as line
strokes from glyph outlines in `src/c/main.c`. Only the digits 0, 1, 2, 4, 6 and
8 are defined.

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
pebble emu-bt-connection --emulator emery --connected no
```

## Install

Install `build/epoch.pbw` through the Pebble phone app, or directly from a
computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
