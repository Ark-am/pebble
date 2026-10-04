# Sketchy Weather Analog

An analog watchface drawn in a pencil-sketch style. A hand-drawn widget sits
in the centre of the dial: a picture with one or two values underneath, such
as the weather icon above the temperature and date. The hour and minute hands
start just outside a sketched ring around the widget, so they never cover it.

On rectangular watches the screen has heavily rounded corners, and the ticks
and hour numbers follow the rounded edge. Round watches use the full circle.

The dial comes in two styles:

- **Elegant** (default): slim dauphine hands and small matching wedge
  markers, each with one solid and one shaded side (12 o'clock has a pair),
  a smooth ring around the centre, and hour numbers in Noto Serif Display, a
  fine high-contrast serif.
- **Sketchy**: pencil-stroke hands and markers, a hand-drawn ring, and hour
  numbers in the watch's own font.

The centre widget is drawn in the pencil-sketch style either way.

## Centre screens

The widget has up to four screens. Each screen has a main item, which sets
the picture and the first value, and an optional second value after the
divider. By default they are:

| Screen | Picture | Values |
| --- | --- | --- |
| 1 | Weather icon | Temperature, date |
| 2 | Calendar page with the day of the week | Date, battery |
| 3 | Footprints | Steps today, heart rate |
| 4 | (not used) | |

The items are weather, date, battery (a gauge, red when low and green while
charging), steps, and heart rate (a heart). **Tap the watch** to move to the
next screen, or turn on **Change screen every minute** to cycle through them
automatically. Set both items on a screen to Nothing to skip it.

If both values do not fit inside the ring, the second, then the first,
switches to a short form: `17°` instead of `17°C`, `Oct 3` instead of
`Oct 03`, `8.4k` instead of `8,432`, `72` instead of `72 bpm`. Steps show
`--` on Aplite, and heart rate shows `--` on watches without a heart-rate
sensor.

## Weather icons

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
disconnected, and a crescent shows during Quiet Time. The watch vibrates
twice when the phone disconnects, except during Quiet Time. To see the
battery, choose it as one of the centre screen items.

## Settings

- **Background:** Paper (light) or Chalkboard (dark)
- **Centre circle:** the four screens and whether they change every minute (see above)
- **Dial style:** Elegant or Sketchy
- **Number font** and **Information font:** see Font styles below
- **Show hour numbers:** off by default; when on, 1 to 12 sit just inside the hour ticks
- **Temperature:** Celsius or Fahrenheit
- **Vibrate when the phone disconnects**

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the current weather from
[Open-Meteo](https://open-meteo.com/), which needs no account or API key. The
watch requests an update every 30 minutes and keeps the last reading if the
phone is unavailable. Until the first reading arrives, the widget shows an
empty cloud and `--°`.

## Hour number images

Pebble's fonts are drawn without smoothing, so the hour numbers are images
made by `tools/make_numerals.py` (it needs Python with Pillow and the Noto
Serif Display font). After changing the font or sizes, run it from this
directory; it rewrites `resources/images/numerals/` and the matching entries
in `package.json`:

```sh
python3 tools/make_numerals.py
```

## Font styles

Two settings choose the fonts: **Number font** (the hour numbers) and
**Information font** (the values and the calendar day in the centre circle). Each offers:

| Style | Font |
| --- | --- |
| Default | each dial style's own numbers and the watch's own Gothic |
| Serif | IBM Plex Serif |
| Rounded | Varela Round |
| Mono | DM Mono |

Choosing a number font draws the hour numbers in it in both dial styles;
Default keeps the Elegant style's serif images and the Sketchy style's
Gothic.

The bundled fonts are in `resources/fonts`, under the SIL Open Font License
(the `OFL-*.txt` files there). Pebble's own fonts are very narrow, so the
others are sized by `tools/make_fonts.py` to match their capital height
without running much wider, and the layout still fits; where space is tight
they come out a little smaller. Each piece of text and the font it normally
uses are listed in `tools/fonts.json`. After changing either, regenerate the
font resources (it needs Python with Pillow):

```sh
python3 tools/make_fonts.py
```

It rewrites the font entries in `package.json` and `src/c/font_styles.h`.
Drawing text in a bundled font needs more of the watch's small app stack
than Pebble's own fonts, so keep large local variables out of the drawing
code's path to the text.

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
pebble emu-tap --emulator basalt                           # next centre screen
pebble emu-steps --emulator basalt 8432                    # set today's steps
pebble emu-heart-rate --emulator emery 72                  # heart rate (emery)
pebble emu-set-timeline-quick-view --emulator basalt on    # Quick View layout
```

## Install

Install `build/sketchy-weather-analog.pbw` through the Pebble phone app, or
directly from a computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
