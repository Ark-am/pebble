# World Clock

A digital watchface that shows the time where you are and in two other
cities. Your own time is large at the top, with the day and date beside it.
Below it, each city shows its name, its time, and how it relates to yours:
`Today +5h`, or the day's name when it is already another day there
(`Sun +13h`). Half-hour and 45-minute time zones show as, for example,
`Today +9h30`.

In 12-hour mode, AM or PM appears under the date and under each city's time.

## Settings

- **Time font** and **Text font:** see Font styles below
- **Time format:** the same as the watch (the default), 12-hour or 24-hour
- **Background colour:** white, cream, light or dark grey, black, navy, dark
  green, burgundy or purple. The date, details and dividers turn black or
  white, whichever suits it. Black-and-white watches offer light or dark.
- **Time colour:** for your time and the cities' times: automatic (black or
  white to suit the background) or one of eleven colours (colour watches
  only)
- **Accent colour:** for the city names, the day and the divider (colour
  watches only)
- **City 1 and City 2:** a time zone from a list of 49 major cities, and an
  optional name to show instead of the city's (up to 15 characters). The
  defaults are London and Tokyo.

## How the times are worked out

The phone works out each city's current offset from UTC, including daylight
saving, and sends it to the watch. The watch keeps the last offsets it was
given, so the times stay right while the phone is away. It asks for fresh
offsets every hour, so a change to or from daylight saving shows up within
the hour.

The daylight-saving rules for the United States and Canada, the European
Union and UK, Egypt, south-eastern Australia and New Zealand are built in
(`src/pkjs/dst.js`), rather than relying on the phone's JavaScript having a
time zone database; the emulator's, for one, does not. The other zones in
the list do not use daylight saving. The rules were checked against Node's
time zone database every 15 minutes from 2025 to 2028.

To offer another city, add it to `src/pkjs/zones.js` with its standard
offset and, if it observes daylight saving, its rule.

## Font styles

Two settings choose the fonts: **Time font** (your time and the cities' times) and
**Text font** (the date, the city names and the details). Each offers:

| Style | Font |
| --- | --- |
| Default | the watch's own digital Leco and Gothic |
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

The installable bundle is created at `build/world-clock.pbw`.

## Test in the emulator

```sh
pebble install --emulator basalt --logs   # rectangular, colour
pebble install --emulator chalk           # round
pebble install --emulator aplite          # black and white
pebble install --emulator emery           # large rectangular
pebble emu-time-format --emulator basalt --format 12h
pebble emu-app-config --emulator basalt   # open the settings page
```

## Install

Install `build/world-clock.pbw` through the Pebble phone app, or directly
from a computer on the same network as the phone:

```sh
pebble install --phone <ip>
```
