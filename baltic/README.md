# Baltic

A dress-watch face modelled on a classic small-seconds wristwatch: a round
dial with a minute scale and railroad track around the edge, Breguet-style hour
numbers, slim leaf hands, and a small seconds dial at half past seven, where the
7 and 8 would be. If you choose, a matching small dial at half past four shows
the date, weather, battery or health data, in place of the 4 and 5. On
rectangular watches the area around the dial is black.

## Settings

Open the watchface's settings in the Pebble phone app:

| Setting | Options |
| --- | --- |
| Dial | Salmon (default), Navy blue, Silver, Black and gold, or Azure blue |
| Running seconds | On (default) or off. Shows a hand on the small seconds dial; it updates every second, which uses more battery. |
| Name on the dial | Any text up to 12 characters, in spaced capitals below the 12 (default "BALTIC"). Leave it empty to show no name. |
| Small dial at half past four | Nothing (default), Date, Weather, Battery, or Health. Health shows steps; tap the watch to cycle through distance, active calories, active minutes, sleep and heart rate (on watches with a sensor). The original Pebble has no health data and shows the date instead. |
| Temperature | Celsius (default) or Fahrenheit, for the weather |

The weather comes from [Open-Meteo](https://open-meteo.com/), which needs no
account or API key, for the phone's location. The phone only asks for the
location while the weather is chosen, and the watch refreshes it every 30
minutes.

| Dial | Dial colour | Numbers and hands |
| --- | --- | --- |
| Salmon | Salmon pink | White, with a grey shadow |
| Navy blue | Dark blue | White |
| Silver | Light grey | Dark blue, like blued steel |
| Black and gold | Black | Gold |
| Azure blue | Mid blue | White |

Pebble screens have 64 colours, so the dial colours are the nearest available.
Black-and-white watches show Salmon and Silver as a white dial with black
markings, and the other dials as black with white markings.

## Hour numbers font

The hour numbers use Noto Serif Display Bold Italic, which is close to the
Breguet numerals of the original. The font is in `resources/fonts/` under the
SIL Open Font License (see `resources/fonts/OFL.txt`). Only the digits are
included in the watchface.

## Build

From this directory:

```sh
npm install     # settings page library (Clay), first time only
pebble build
```

The installable bundle is created at `build/baltic.pbw`.

## Test in the emulator

```sh
pebble install --emulator emery          # large rectangular, colour
pebble install --emulator chalk          # round
pebble install --emulator aplite         # black and white
pebble emu-app-config --emulator emery   # open the settings page
```

## Install

Install `build/baltic.pbw` through the Pebble phone app, or directly from a
computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
