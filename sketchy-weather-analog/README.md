# Sketchy Weather Analog

An analog watchface drawn in a pencil-sketch style. A large hand-drawn widget
fills most of the dial: a picture with one or two values underneath, such as
the weather icon above the temperature and date. There is no hour hand: the
current hour's number shows instead, larger and in its own colour (red by
default). The minute hand is a short pointer between the widget and the
ticks, reaching in among the hour ticks, so it never covers the widget.

On rectangular watches the screen has heavily rounded corners, and the ticks
and hour numbers follow the rounded edge. Round watches use the full circle.

The minute hand and hour markers are pencil strokes, and the hour numbers
and all the text are handwritten, in Patrick Hand, unless another font is
chosen. The Pebble Time 2 (Emery), with the most room, has the largest
picture and text.

## Centre screens

The widget has up to four screens. Each screen has a main item, which sets
the picture and the first value, an optional second value after the
divider, and a bottom line under them that shows the name (the default) or a
third value. By default they are:

| Screen | Picture | Values |
| --- | --- | --- |
| 1 | Weather icon | Temperature, date |
| 2 | Calendar page with the day of the week | Date, battery |
| 3 | Footprints | Steps today, heart rate |
| 4 | (not used) | |

The items are weather, date, battery (a gauge, red when low and green while
charging), steps, and heart rate (a heart). **Tap the watch** to move to the
next screen, or turn on **Change screen every minute** to cycle through them
automatically. Set the main and second items on a screen to Nothing to skip
it; the bottom line alone does not keep a screen. A value on the bottom line
uses the name's smaller lettering and switches to its short form (12.3k
steps, 72 for heart rate) when the full one is too wide. The date there adds
the day of the week (Fri Oct 30), or drops the month (Fri 30) where that is
too wide, as with most fonts on the 144-pixel-wide watches.

If both values do not fit across the widget, the second, then the first,
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
- **Minutes:** how the minutes show: Hand (default), Highlighted tick (the
  current minute's tick as a bold coloured stroke), Minute number (the
  minutes as a number round the edge, taking the place of any hour number it
  lands on and moving aside from the current hour's), Progress arc (pencil
  strokes along the edge from 12 o'clock to the minute), Sketched bead,
  Edge pointer (a small triangle at the edge), Progress arc and bead, or
  Minute number and tick. All but the hand use the current hour's colour.
- **Minute hand** (when the minutes show as a hand): Pencil stroke (default), Sketched spear (a pencil outline with
  a line down the middle) or Sketched arrow (a pencil shaft with an open
  arrowhead and fletching)
- **Name under the information:** any text, PEBBLE by default, shown on the
  bottom line of each screen set to The name; leave it empty to show none. A
  name too wide for the space ends in an ellipsis.
- **Number font** and **Information font:** see Font styles below
- **Show hour numbers:** off by default; when on, the other hours' numbers
  sit just inside the hour ticks too
- **Current hour colour:** the colour of the current hour's number (colour
  watches only; red by default)
- **Temperature:** Celsius or Fahrenheit
- **Vibrate when the phone disconnects**

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the current weather from
[Open-Meteo](https://open-meteo.com/), which needs no account or API key. The
watch requests an update every 30 minutes and keeps the last reading if the
phone is unavailable. Until the first reading arrives, the widget shows an
empty cloud and `--°`.

## Font styles

Two settings choose the fonts: **Number font** (the hour numbers) and
**Information font** (the values, the calendar day and the name in the centre). Each offers:

| Style | Font |
| --- | --- |
| Sketchy (default) | Patrick Hand, a handwritten font |
| Pebble | the watch's own Gothic |
| Serif | IBM Plex Serif |
| Rounded | Varela Round |
| Mono | DM Mono |

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
