# Gnomon 2

A classic analog watchface. The minute track follows the edge of the screen,
the hands have a tapered leaf around the centre, and four small complications
sit around the centre:

| Position | Shows |
| --- | --- |
| Upper left | Current temperature for the phone's location, with an icon for the conditions: sun (or moon at night), sun or moon behind a cloud, cloud, fog, drizzle, rain, snow, or a storm. Conditions without an icon are written out. |
| Upper right | Today's health data: steps, distance, active calories, active minutes, sleep, and heart rate (on watches with a sensor). **Tap the watch** to cycle through them. Not shown on Aplite, which has no health data. |
| Lower left | Day of the week and day of the month |
| Lower right | Battery level as a ring of ten segments. The dot in the middle is blue while the phone is connected and red when it is not (filled or empty on black-and-white watches), and shows a crescent moon during Quiet Time. |

The watch vibrates twice if the phone disconnects, except during Quiet Time.

On rectangular watches the dial has rounded corners with black behind them,
and the minute track follows the curve.

## Settings

Open the watchface's settings in the Pebble phone app:

| Setting | Options |
| --- | --- |
| Background | Light (default) or Dark |
| Hour numbers | Classic: 12, 2, 4, 6, 8 and 10, turned to follow the dial (default). Modern: 12, 3, 6 and 9, upright. |
| Second hand | Off (default) or on. It updates every second, which uses more battery. |
| Second hand colour | Red (default), Orange, Amber, Green, Teal, Blue, Violet, Rose, or the same black or white as the text. Colour watches only. |
| Name on the dial | Any text, shown below the 12 (default "Pebble"). Leave it empty to show no name. Long names are cut off with "…". |
| Weather, Health, Date, Battery and connection | Each can be turned off to hide that item; all are on by default |
| Move information clear of the hands | Off (default) or on. Each item slides around the centre, and outward if needed, when a hand would cover it. When there is no room it stays where it is, partly covered. |
| Temperature | Celsius (default) or Fahrenheit |
| Vibrate when the phone disconnects | On (default) or off |

Hours without a number get a long index line instead.

Pebble's fonts cannot be rotated and are drawn without smoothing, so on colour
watches the classic numbers are images with soft edges, already turned to
follow the dial. `tools/make_numerals.py` renders them from Noto Serif Display
Bold, narrowed to a condensed shape, into `resources/images/numerals/`; run it
again after changing the font, sizes or angles. Black-and-white watches cannot
show soft edges, so they draw the numbers as line strokes from glyph outlines
in `src/c/main.c`, where only the digits 0, 1, 2, 4, 6 and 8 are defined.

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
