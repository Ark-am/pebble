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

The Qibla bearing and magnetic-declination calculations are plain JavaScript
and can be tested with Node.js 22 or newer, without the Pebble SDK:

```sh
npm test
```

The declination tests use NOAA's official WMM2025 sea-level test values.

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

- The screen shows `Qibla: 277.6°`.
- Terminal 1 logs `Location: 23.8103, 90.4125`.
- Terminal 1 logs the WMM-2025 magnetic declination used to correct the compass.

### 5. Test compass directions

Message key `16000` is the app's emulator-only magnetic-heading input. It is
used because current SDK versions do not consistently deliver
`pebble emu-compass` changes to Alloy apps.

Run the following commands one at a time in terminal 2:

```sh
pebble send-app-message --emulator emery --int 16000=250
pebble send-app-message --emulator emery --int 16000=278
pebble send-app-message --emulator emery --int 16000=310
```

Expected results:

| Heading | Expected display |
| --- | --- |
| `250°` | White arrow and `Turn right 28°` |
| `278°` | Green arrow and `Qibla aligned` |
| `310°` | White arrow and `Turn left 32°` |

Entering the aligned range (`274°` through `281°` for the Dhaka test)
also triggers one short vibration. An emulator cannot reproduce the physical
feel of the vibration, but the green aligned state verifies the same condition.

### 6. Capture a screenshot

With the emulator running:

```sh
pebble screenshot --emulator emery --no-open qibla-compass.png
```

This creates `qibla-compass.png` in the current directory. Store-listing
screenshots should be captured from the final release build.

### 7. Repeat or stop the test

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
src/embeddedjs/qibla.js        Qibla bearing calculations
src/embeddedjs/manifest.json   Moddable manifest
src/pkjs/index.js              PebbleKit JS (phone-side) code
test/                          Node.js unit tests for the calculations
package.json                   Project metadata (UUID, platforms, resources)
wscript                        Build rules — usually no need to edit
```

## Documentation

Full SDK docs and tutorials: <https://developer.repebble.com>
