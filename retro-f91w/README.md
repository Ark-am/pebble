# Retro F-91W

A digital watchface after the classic Casio F-91W. Inside a blue frame sit the
brand and model, LIGHT and ALARM CHRONOGRAPH above the LCD, MODE and
ALARM ON-OFF/24HR below it, and WATER, a boxed red WR, and RESIST along the
bottom. The LCD shows the time in large seven-segment digits with the seconds
smaller beside them, a top row with the day and date (or the weather, steps or
heart rate), and at the left the hourly signal's sign and PM.

The brand on the case is the wearer's own text (PEBBLE by default); the face
does not use the Casio name or logo.

## Settings

- **LCD:** Classic grey (default), Backlit green, Blue, Amber, or Dark with
  light segments. Black-and-white watches show a white LCD, or a black one for
  Dark.
- **Case:** Black with a blue frame, orange model name and red WR (default),
  Gold or Silver. Colour watches only.
- **Brand**, **Model**, **Label above the LCD** and **Label at the bottom:** any
  text (PEBBLE, F-91W, ALARM CHRONOGRAPH and WATER WR RESIST by default); all
  but the brand can be left empty. A bottom label of three words is set out as
  on the original, with the middle word in a red box.
- **Top row:** Day and date (default), Weather (a two-letter condition and the
  temperature), Steps, Heart rate (on watches with a sensor), or each in turn,
  changed with a shake of the wrist.
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

Every digit and letter is drawn from segments rather than a font: seven
segments for digits and fourteen for letters, each a bar cut to a point at
both ends.

## Build and test

```
pebble build
pebble install --emulator emery
```

Supported platforms: Aplite, Basalt, Chalk, Diorite, Emery, Flint and Gabbro.
