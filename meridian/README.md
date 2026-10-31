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

Below the 12 o'clock marker the dial shows a name, "PEBBLE" by default. Set
any text under **Name on the dial** in the settings, or leave it empty to show
no name. Long names are cut off with "…". When nothing is shown at 12
o'clock, the name moves down into that space. **Name size** shows it small,
like the information's labels, or large, like its values.

On rectangular watches the minute track follows the edge of the screen; on
round watches it follows the circle. Turn off **Show minute lines** to leave
only the hour marks. When a Timeline Quick View covers part of
the screen, the dial shrinks to fit the remaining area.

The whole face can be turned to suit how the watch is worn: set **Rotate the
dial** in the settings, from -180 to 180 degrees in steps of 5 (positive is
clockwise). On round watches (Chalk and Gabbro, the Pebble Round 2) the face
turns as one piece: the ticks, 12 o'clock marker, hour numbers, information,
icons and hands, with the text turned too. The ticks, marker and hands are
drawn at the angle directly, so they stay smooth. Pebble cannot draw turned
text, so each hour number and each piece of information (with its icons) is
drawn upright, copied into a small image, and drawn back turned in its place,
each pixel blending its nearest neighbours so the edges stay soft. With the
watch's 64 colours the text is still a little less crisp than upright text.
On rectangular watches the ticks, numbers, information and hands move around
the dial and the text stays upright.

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the weather from [Open-Meteo](https://open-meteo.com/),
which needs no account or API key. The watch requests an update every 30
minutes and keeps the last reading if the phone is unavailable. When no
position shows the weather, the phone does not ask for the location at all.

## Font styles

Two settings choose the fonts: **Number font** (the hour numbers) and
**Information font** (the weather, date, battery and health information). Each offers:

| Style | Font |
| --- | --- |
| Default | the watch's own Gothic |
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

## Shine

Turn on **Shine** in the settings and light seems to catch the face as the
wrist turns. Each hand shows a lit side and a shaded side, which swap over
as the light moves across it towards whichever side of the watch is lower,
and the hour marks and the centre sparkle as it passes them. On the light
background the hour marks turn light grey instead of sparkling. Once the
watch has been still for about three seconds, or is turned away from face
up, the face looks exactly as it does with Shine off.

While the watch is still, the face reads the accelerometer once a second.
Movement switches it to about eight updates a second until the watch is
still again, so Shine mostly costs battery while the wrist is moving with
the face turned up. It is off by default, and only colour watches have it;
the setting is hidden on black and white ones.

How readily the shine starts, how far the light moves for a given tilt and how soon
it clears are set by the `SHINE_*` constants in the "Motion" section of
`src/c/main.c`.

## Rim glint

Turn on **Rim glint** and the edge of the screen catches the light like the
rim of a watch glass: short streaks, white in the middle and fading to grey,
slide along the edge as the wrist turns, and arcs slide round the rim on
round watches. It follows the same unseen light as Shine and can be on
with it or alone, with the same battery use and the same three still
seconds before it clears. It is off by default and on colour watches only.
The streaks' length is set by `GLINT_REACH` in the "Rim glint" section of
`src/c/main.c`.

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
pebble emu-accel --emulator basalt custom swivel.txt       # move the light (Shine on)
```

For Shine, `swivel.txt` holds one accelerometer reading per line as `x,y,z`
in thousandths of g, for example `0,0,-1000` for face up, then readings that
swing x between about -300 and 300. The face only shines while the
readings keep changing, so take screenshots while they play.

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
