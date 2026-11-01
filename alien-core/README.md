# Alien Core

An alien-tech watchface: a glowing core inside a ring of twelve armoured
plates. The core shows the time as glowing digits or as hands, with a name of
your choice (PEBBLE by default) above it and the day and date below, or other
information in those places. The plate
for the current hour glows, and the plate for the current five minutes glows
more dimly, so the ring doubles as a track for both.

## Avatars

Each plate also holds one of twelve original avatars: Octavo, Sporelet,
Lumoth, Hornback, Wyrmling, Driftjelly, Hootan, Coilfang, Stingtail, Oculon,
Skyray and Pincera.

- **Open the selector:** long press the screen on the Pebble Time 2 and Pebble
  Round 2 (a quick tap or a wrist flick does nothing here), or flick your wrist
  on watches without a touchscreen. The core's iris closes over the time, the
  light runs round the plates from the hour and comes to rest on the last avatar
  chosen. Each avatar has its own hour plate, and its avatar springs up in the
  core with its name when the light lands there.
- **Step through them:** on touch watches, swipe across the core: left or up
  for the next avatar, right or down for the one before. You can also drag
  round the plates to move the light; it snaps to the nearest plate when you
  let go. On other watches, flick again for the next avatar. The core's ring
  charges up clockwise over the time set in Settings (three seconds to begin
  with); each swipe, drag or flick starts it again.
- **Transform:** tap the core, or let it finish charging. The iris closes, the
  plates flash, sparks and shock rings fly out, and the iris opens on the
  avatar, springing up large and glowing. A tap, or a flick on watches without
  a touchscreen, goes back to choosing.
- **Back to the time:** after four seconds the iris closes again, the ring
  runs on to the hour's plate, and the time returns.

The avatars are alive while they show: they blink and bob, and each has its
own movement, such as Lumoth's flapping wings, Stingtail's rocking claws and
tail, Driftjelly's pulsing bell and Coilfang's flicking tongue.

## Other animations

- **Power-up:** when the face comes on screen, the plates light one by one
  round the ring and the iris opens on the time.
- **On the hour:** a light runs once round the ring.

Nothing else moves while the time shows, so the face costs little battery.
Touch watches keep their touch sensor on while the animations are turned on,
which uses a little more.

The design is original; it does not use any character, logo or symbol from
other works.

## Sound

The Pebble Time 2 and Pebble 2 Duo have a speaker, so they add sound effects:

- **Opening:** the light ratchets round the plates, its ticks spreading out as it
  slows, over a rising hum.
- **Stepping:** each plate has its own note, on a scale round the ring, whether
  you swipe, drag or flick.
- **Charging:** two pips over the last two seconds, rising towards the
  transformation.
- **Transforming:** a surge up as the iris closes, a low boom as the plates
  flash, and bright notes falling as the sparks fly out.
- **Each avatar** has a call of its own as it arrives, such as Hootan's two
  hoots and Coilfang's hiss.
- **Closing:** a falling sweep as the time comes back.

The power-up and the hour's light have sounds too, played only if chosen in
Settings, as they happen without a touch of the watch. Nothing plays during
Quiet Time, or when the watch is muted.

## Settings

- **Time:** Digital (the default) or Analog
- **Time format:** the same as the watch (the default), 12-hour or 24-hour, for
  digital time. 12-hour time shows AM or PM beside the minutes.
- **Glow colour:** Alien green (the default), lime, cyan, blue, purple, red,
  orange or yellow (colour watches only; black-and-white watches glow white)
- **Above the time** and **Below the time:** the name, date, weather, steps,
  heart rate, battery or nothing (the name and the date by default). Weather,
  steps and heart rate show an icon beside the value, and the battery shows an
  icon filled to the charge. The weather's icon follows the conditions, with a
  moon on a clear night. Steps need a watch
  with health support, and heart rate a supported sensor; otherwise they show
  `--`.
- **Temperature:** Celsius or Fahrenheit
- **Name in the core:** any text up to 20 characters, PEBBLE by default; leave
  it empty to show none. A name too wide for the core ends in an ellipsis.
- **Name size:** Small, Medium (the default) or Large, for whatever shows above
  the time
- **Name font:** Pebble (the watch's own Gothic, the default), Serif (IBM Plex
  Serif), Rounded (Varela Round), Mono (DM Mono) or Sketchy (Patrick Hand)
- **Avatars and animations:** on by default; turn off to keep the face still
- **Time to choose:** how long the selector waits before transforming: 3 (the
  default), 5, 10, 15 or 30 seconds
- **Keep the light on:** on by default; the backlight stays on from the selector
  opening until the time comes back, then goes off as usual
- **Long press** (touch watches): how long to hold the screen to open the
  avatars: 0.5, 1 (the default), 1.5, 2 or 3 seconds
- **Sound effects** (watches with a speaker): Off, Avatars only (the default),
  or Avatars, power-up and on the hour
- **Volume** (watches with a speaker): Quiet, Medium (the default) or Loud

## Weather

The companion JavaScript runs in the Pebble phone app. While the weather is
shown, it asks the phone for its location and fetches the weather from
[Open-Meteo](https://open-meteo.com/), which needs no account or API key. The
watch asks for an update every 30 minutes and keeps the last reading.

## Drawing

Rectangular watches show the dial as large as their shorter side allows;
round watches fill the screen. The parts are sized from the dial's radius
(see `BEZEL_INNER`, `CORE_OUTER` and `CORE_INNER` in `src/c/main.c`), and the
digits use the watch's own LECO font at a size to suit the core. The
timings are set by the `*_MS` constants in the "States" section, and the
stages of each animation in `canvas_update_proc`.

The avatars are vector shapes on a 100 x 100 grid in two shades of the glow,
defined in `tools/make_avatars.py`. Each shape can carry a motion (see `Motion`
in `src/c/main.c`). The tool writes them to `src/c/avatars.h` and draws a
preview sheet (it needs Python with Pillow):

```
python3 tools/make_avatars.py preview.png
```

The bundled fonts are in `resources/fonts`, under the SIL Open Font License
(the `OFL-*.txt` files there). `tools/make_fonts.py` sizes them from
`tools/fonts.json` to match Pebble's own fonts, and rewrites the font entries
in `package.json` and `src/c/font_styles.h`.

The sounds are note sequences, defined in `tools/make_sounds.py`, which writes
them to `src/c/sounds.h` and can write a WAV file of each to listen to:

```
python3 tools/make_sounds.py previews
```

## Build and test

```
pebble build
pebble install --emulator emery
pebble emu-tap --emulator basalt       # open the selector, then step
```

Supported platforms: Aplite, Basalt, Chalk, Diorite, Emery, Flint and Gabbro.
