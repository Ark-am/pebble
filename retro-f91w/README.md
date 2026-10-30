# Retro F-91W

A digital watchface after the classic Casio F-91W, laid out from measurements
of a photo of the original. A blue band with large curved corners frames the
face plate, with a thin silver line just inside it. The brand and model sit
above a blue line, then labels pointing with small red arrows at the Pebble's
buttons: LIGHT at the top left by Back, MODE at the top right by Up, and
ALARM ON/OFF below the LCD by Down. Along the bottom, below a second blue line,
run WATER, a red italic WR in a box that the line runs into, and RESIST. The LCD shows the time in large seven-segment digits with the seconds
smaller beside them, a top row with the day and date (or the weather, steps or
heart rate), and at the left the hourly signal's sign and PM.

The brand on the case is the wearer's own text (PEBBLE by default); the face
does not use the Casio name or logo.

## Settings

- **LCD:** Classic pale grey-green (default), Classic grey, Backlit green, Blue,
  Amber, Dark with light segments, or Custom. Black-and-white watches show a
  white LCD, or a black one for Dark, and do not offer Custom.
- **Case:** Black with a blue frame, olive-gold model name and red WR (default),
  Gold, Silver, or Custom. Colour watches only.
- **Custom colours:** choosing Custom for the case or the LCD shows a colour
  picker for each of its parts, starting from the preset that was chosen
  before. The case has seven: the case, printed text, model name, the band
  (with the lines and the box), the thin line inside the band, the boxed word
  (with the arrows) and the LCD border; the LCD has two: the LCD and its
  digits. **Separate colour for every part** adds pickers for the parts those
  share: the button labels, the bottom label, the lines, the box, the arrows,
  the LCD's top row (with PM and the chime sign), and the black around the
  case. Choosing a preset again goes back to its colours; the custom ones are
  kept for next time. The pickers live in `src/pkjs/custom-clay.js`, which
  shows them and starts them from the presets.
- **Brand**, **Model** and **Label at the bottom:** any text (PEBBLE, F-91W and
  WATER WR RESIST by default); all but the brand can be left empty. A bottom
  label of three words is set out as on the original, with the middle word in
  a red box.
- **Button labels:** the text by the Back, Up and Down buttons (LIGHT, MODE and
  ALARM ON/OFF by default); leave one empty to hide it and its arrow.
- **Top row:** Day and date (default), Weather (a two-letter condition and the
  temperature), Steps, Heart rate (on watches with a sensor), or each in turn,
  changed with a shake of the wrist.
- **Beside the brand (F-91W)**, **Bottom left (WATER)** and **Bottom right
  (RESIST):** the printed text (default), or the weather (temperature), steps,
  heart rate or battery level in its place.
- **Time format:** the same as the watch (default), 12-hour (with a PM sign in
  the afternoon) or 24-hour.
- **Show seconds:** off by default. When on, the face updates every second,
  which uses more battery.
- **Hourly chime:** a short vibration on the hour, except during Quiet Time;
  off by default. While it is on, the LCD shows the signal's sign, (( . )).
- **Temperature:** Celsius or Fahrenheit.

Weather conditions show as two letters: CL clear, PC partly cloudy, CD cloudy,
FG fog, DZ drizzle, RA rain, SH showers, SN snow, TS thunderstorm.

## Weather

The companion JavaScript runs in the Pebble phone app. It asks the phone for
its location and fetches the weather from [Open-Meteo](https://open-meteo.com/),
which needs no account or API key. It only asks while the top row shows the
weather, and the watch requests an update every 30 minutes.

## Drawing

Every digit and letter on the LCD is drawn from segments rather than a font:
seven segments for digits and fourteen for letters, each a bar cut to a point at
both ends. The model name beside the brand, and anything shown in its place,
is set in IBM Plex Sans Condensed Bold Italic, bundled in `resources/fonts`
under the SIL Open Font License (`OFL-IBMPlex.txt`); the rest of the printed
text uses the watch's own fonts.

## Build and test

```
pebble build
pebble install --emulator emery
```

Supported platforms: Aplite, Basalt, Chalk, Diorite, Emery, Flint and Gabbro.
