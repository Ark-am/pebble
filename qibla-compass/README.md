# qibla-compass

A Pebble Alloy project — embedded JavaScript on the watch, powered by Moddable
XS, alongside C.

## Building & running

```sh
pebble build                          # build for all targetPlatforms
pebble install --emulator emery       # install on the emery emulator
pebble install --phone <ip>           # install to a paired phone
```

## Unit tests

The Qibla bearing, distance, sun-position and magnetic-declination
calculations are plain JavaScript and can be tested with Node.js 22 or newer,
without the Pebble SDK:

```sh
npm test
```

The declination tests use NOAA's official WMM2025 sea-level test values. The
sun-position tests use the worked example from NREL's Solar Position Algorithm
report.

## The screen

- **Top line:** the Qibla bearing from true north and its compass direction,
  such as `58.5° east-northeast`.
- **Second line:** your coordinates and the distance to Makkah.
- **Dial:** north is marked with a red `N`. The gold band runs clockwise from
  north to the Kaaba marker, which is the angle the bearing measures. You are
  the dot in the middle, and the sun appears on the rim while it is up.
- **Bottom:** what to do, a line of detail, the two modes, and a padlock.

### Modes

Press **Up** or tap **Compass** for compass mode, and **Down** or tap **Sun**
for Sun mode. The app remembers the last mode used.

### Still dial

The dial normally turns so that the top of the screen is the way you face.
To keep it still, with north at the top like a map, tap the **padlock** or
hold **Select**. Do the same again to let it turn. The app remembers the
choice.

With the dial still, compass mode shows a white arrow from you the way the
watch faces; turn until it points at the Kaaba. Sun mode shows the sun and
your shadow where they are. Pressing Select briefly still retries the sensors.

- **Compass:** the dial turns with the watch's heading, and the triangle at
  the top shows the way you face. Turn until the Kaaba reaches the triangle.
  The marker turns green and the watch vibrates when you are within 4°.
- **Sun:** the dial is set by the sun's position, worked out from your
  location and the time, so the compass is not needed. Face the sun and turn
  as shown. When the Qibla is more than 90° from the sun, the app uses your
  shadow instead, so the turn is shorter and you are not looking at the sun.
  Sun mode needs the sun above the horizon and no higher than 75°.

## Testing without a watch

These steps provide a repeatable test using the Pebble Time 2 (`emery`)
emulator. Run every command from the project directory.

### 1. Check the development environment

The Pebble SDK must be installed and have an active SDK containing the `emery`
emulator. Check it with:

```sh
pebble --version
```

Install the JavaScript dependency if `node_modules` is missing:

```sh
npm install
```

### 2. Build the app

```sh
pebble clean
pebble build
```

A successful build creates:

```text
build/qibla-compass.pbw
```

### 3. Launch the emulator

In terminal 1, install the app in the Pebble Time 2 emulator and keep the
terminal open to see runtime logs:

```sh
pebble install --emulator emery --logs
```

The emulator's automatic GeoIP lookup may fail and log `Location unavailable`.
That is expected; the next step injects a deterministic location.

### 4. Inject a test location

In terminal 2, simulate a phone reporting the coordinates of Dhaka:

```sh
pebble send-app-message --emulator emery \
  --app-uuid 3935bb82-a179-4900-b46c-8e7f12b6ff71 \
  --string '15026=1,23.8103,90.4125,,,,,,'
```

Expected result:

- The top line shows `277.6° west`, above `From 23.81°N 90.41°E · 5,172 km`.
- Terminal 1 logs `Location: 23.8103, 90.4125`.
- Terminal 1 logs the WMM-2025 magnetic declination used to correct the compass.

### 5. Test compass directions

Message key `16000` is the app's emulator-only magnetic-heading input. It is
used because current SDK versions do not consistently deliver
`pebble emu-compass` changes to Alloy apps. Only debug builds read it.

Run the following commands one at a time in terminal 2:

```sh
pebble send-app-message --emulator emery --int 16000=250
pebble send-app-message --emulator emery --int 16000=278
pebble send-app-message --emulator emery --int 16000=310
```

Expected results:

| Heading | Expected display |
| --- | --- |
| `250°` | Gold Kaaba marker and `Turn right 28°` |
| `278°` | Green Kaaba marker at the top and `Qibla aligned` |
| `310°` | Gold Kaaba marker and `Turn left 32°` |

Entering the aligned range (`274°` through `281°` for the Dhaka test)
also triggers one short vibration. An emulator cannot reproduce the physical
feel of the vibration, but the green aligned state verifies the same condition.

### 6. Test Sun mode

Press Down in the emulator to switch to Sun mode. Sun mode uses the
emulator's clock, so the result depends on the time of day at the injected
location. Set a known time and location instead:

```sh
pebble emu-set-time --emulator emery 1794232800    # 9 Nov 2026 14:00 UTC
pebble send-app-message --emulator emery \
  --app-uuid 3935bb82-a179-4900-b46c-8e7f12b6ff71 \
  --string '15026=1,40.7128,-74.006,,,,,,'
```

Expected result for New York at 9:00 in the morning:

- The sun is at the top of the dial, and the Kaaba marker is to the left.
- The screen shows `Turn left 80°` and `after facing the sun at 139° SE`.
- Hold Select (`pebble emu-button --emulator emery click select --duration 900`)
  and the dial turns north up, with the sun on the rim at 139°.

When the sun is down (for example, Dhaka at that time), the screen shows
`The sun has set`.

### 7. Capture a screenshot

With the emulator running:

```sh
pebble screenshot --emulator emery --no-open qibla-compass.png
```

This creates `qibla-compass.png` in the current directory. Store-listing
screenshots should be captured from the final release build.

### 8. Repeat or stop the test

After changing the source, rebuild and reinstall:

```sh
pebble build
pebble install --emulator emery --logs
```

Stop all SDK-managed emulator processes when finished:

```sh
pebble kill
```

### Troubleshooting

- If the first emulator launch reports `Connection refused`, wait for the
  emulator window to finish opening and run the install command again.
- If an injected value has no effect, make sure Qibla Compass is open in the
  `emery` emulator and repeat the install command.
- If the build cannot find `@moddable/pebbleproxy`, run `npm install` and build
  again.
- Magnetic headings are corrected to true north on the watch using WMM-2025,
  the acquired coordinates, and the current date. WMM-2025 is valid through
  the end of 2029.

## Target platform

This app targets **emery** (Pebble Time 2) with a fixed 200 × 228 layout.

## Project layout

```
src/c/mdbl.c                   C glue around the Moddable runtime
src/embeddedjs/main.js         JavaScript that runs on the watch
src/embeddedjs/qibla.js        Qibla bearing, distance and compass points
src/embeddedjs/sun.js          Sun position (NOAA solar equations)
src/embeddedjs/manifest.json   Moddable manifest
src/pkjs/index.js              PebbleKit JS (phone-side) code
test/                          Node.js unit tests for the calculations
package.json                   Project metadata (UUID, platforms, resources)
wscript                        Build rules — usually no need to edit
```

## Documentation

Full SDK docs and tutorials: <https://developer.repebble.com>
