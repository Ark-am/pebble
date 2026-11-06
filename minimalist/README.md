# Minimalist

A quiet watch face for the Pebble 2 Duo, in an analog or a digital style.

The analog style has twelve hour marks around the edge of the screen, longer
and heavier at 12, 3, 6 and 9, and two hands meeting at a small hub. Those
four marks can be numerals instead, and the hands come in five shapes. The
digital style is laid out like the LCD of [Retro F-91W](../retro-f91w/): a
top row with the day of the week at the left and the month and day of the
month at the right (FRI and NOV 6), and the time below on one line across
the width, with PM to its right in 12-hour time. The analog
style shows the day and date in small capitals below the centre. Both show a
name, PEBBLE by default: above the centre on the analog dial, and above the
top row on the digital face. Up to two widgets show the weather, steps or
battery: left and right of the centre on the dial, or in a row below the
digital time. Either style can hide the date, both come in dark or light, and
all the text comes in one of four font styles.

## Settings

- **Style:** Analog (the default) or Digital
- **Background:** Dark (the default) or Light
- **Font:** for all the text on the face:

  | Style | Font |
  | --- | --- |
  | Condensed (the default) | Antonio, with Pebble's Gothic for the name, PM and the analog date |
  | Serif | IBM Plex Serif |
  | Rounded | Varela Round |
  | Mono | DM Mono |

  Antonio is narrow, so the digital time fills the height it has. The other
  fonts are wider, so their digital time and top row are smaller, to fit the
  screen's width.
- **Show the date:** on by default
- **Name:** the text shown on the face, PEBBLE by default, up to 16
  characters. Leave it empty to hide it. A name too wide for the screen ends
  in an ellipsis.
- **Left widget** and **Right widget:** Nothing (the default), Weather,
  Steps or Battery. Each shows an icon with its value: the temperature with an
  icon for the conditions (a moon on a clear night), today's steps, or the
  battery's charge with a battery filled to match. On the dial the icon sits
  above the value; on the digital face it sits before it. A value shows `--`
  until there is one. The Pebble 2 Duo has no heart-rate sensor, so there is
  no heart-rate widget.
- **Temperature:** Celsius (the default) or Fahrenheit
- **Hands:** Baton (plain lines, the default), Dauphine (tapered from the
  centre), Skeleton (an open outline with a pointed tip), Lollipop (a thin
  line ending in a ring) or Leaf (widest halfway along) (analog style)
- **At 12, 3, 6 and 9:** long marks (the default) or numbers (analog style)
- **Time format:** the same as the watch (the default), 12-hour or 24-hour
  (digital style). In 12-hour time, PM sits to the right of the time, on its
  baseline, and the time is a little smaller to leave room for it. The room
  is kept in the morning too, so the time doesn't move at noon.
- **Leading zero on the hour:** shows 09 rather than 9 (digital style, on by
  default). Without it the first place is left blank, as on an LCD, so the
  colon never moves.

The watch keeps the settings, so they last without the phone.

## Weather

The companion JavaScript runs in the Pebble phone app. While a widget shows
the weather, it asks the phone for its location and fetches the weather from
[Open-Meteo](https://open-meteo.com/), which needs no account or API key. The
watch asks for an update every 30 minutes and keeps the last reading, so it
still shows one while the phone is away.

## Drawing

Everything is drawn in `src/c/main.c`. The hour marks follow the edge of the
rectangular screen rather than a circle, so the dial uses the whole display.
The sizes of the marks, the hands and the hub, and the gaps between the text,
are the constants in its "Layout" section. The hands are shapes drawn
pointing at 12 and turned, in `draw_hand`. The widgets' icons are drawn from
circles and lines in percentages of their size, as in Alien Core, in the
"Widgets" section. Each digit of the digital time is
drawn in a cell of its own, as wide as the widest digit, so the time keeps its
place as the digits change.

## Fonts

The bundled fonts are in `resources/fonts`, under the SIL Open Font License
(the `OFL-*.txt` files there). `tools/make_fonts.py` sizes each font style
for every piece of text: the digital time (and its smaller 12-hour size),
the top row, the dial's numerals, the analog date, and the name, PM and
widgets. Each gets the largest size that is no
taller than the Condensed style's text and still fits its width. The script
also works out where Pebble draws each font's glyphs in their text box. It
writes the font resources into `package.json` and the sizes into
`src/c/font_styles.h`. After changing it, run it again (it needs Python with
Pillow):

```
python3 tools/make_fonts.py
```

The Condensed style keeps fixed sizes of Antonio and Pebble's own Gothic. The
script's placement rule reproduces the positions measured for them in the
emulator.

## Build and test

```
pebble build
pebble install --emulator flint
pebble send-app-message --emulator flint --int 10000=1   # switch to digital
```

The message key numbers are in `build/js/message_keys.json`. Pass several
settings after a single `--int`.

Supported platform: Flint (Pebble 2 Duo).
