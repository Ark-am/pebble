# Phone Voice Commands publishing files

## Release

| Field | Value |
| --- | --- |
| Title | Phone Voice Commands |
| Type | Watchapp |
| Author | ark-am |
| Version | 1.1.0 (Android versionCode 2) |
| UUID | `01430625-332e-46aa-b212-89d36377c0a5` |
| Android package | `com.arkam.pebblevoicecommands` |

| File | Purpose |
| --- | --- |
| `dist/phone-voice-commands-1.1.0.pbw` | Watch app for Basalt, Chalk, Diorite, Emery, Flint and Gabbro. Aplite has no microphone and is not a target. |
| `dist/phone-voice-commands-1.1.0-android.apk` | Android companion to share for direct installation. |
| `dist/SHA256SUMS.txt` | Checksums of the two files above. |
| `phone-voice-commands-icon.png` | 1254 × 1254 RGBA store icon with transparency. |
| `phone-voice-commands-icon.svg` | Source of the icon. |
| `store/icon-large-144.png`, `store/icon-small-80.png` | The icon at the dashboard's recommended large (144 × 144) and small (80 × 80) sizes, cropped to the artwork so it stays legible. |
| `phone-voice-commands-screenshot.png`, `store/screenshots/basalt.png` | 144 × 168 Basalt screenshot. |
| `store/screenshots/chalk.png` | 180 × 180 Chalk screenshot. |

`build/` and `dist/` are generated and ignored by Git. To rebuild them, from
this directory:

```sh
npm ci
pebble build

cd android
./gradlew assembleDebug testDebugUnitTest lintDebug
cd ..

mkdir -p dist
cp build/phone-voice-commands.pbw dist/phone-voice-commands-1.1.0.pbw
cp android/app/build/outputs/apk/debug/app-debug.apk dist/phone-voice-commands-1.1.0-android.apk
(cd dist && shasum -a 256 *.apk *.pbw > SHA256SUMS.txt)
```

Gradle needs JDK 21 or newer; Android Studio's bundled JDK works:
`export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"`.

## Android companion signing

Like Phone Dialer, the APK is a sideload build signed with the build machine's
Android debug key (certificate SHA-256
`c481f15e86e829c632d78d486c30fc8a047fde7644483f4532df41990a30623d`).
Updates install over it only when built on the same machine; a build signed
with any other key needs the old app uninstalled first, which resets its
permissions and Pebble link. Google Play would need a private release key
and a review of the SMS, Phone and Contacts permissions.

Before each new APK, increase `versionCode` and `versionName` in
`android/app/build.gradle.kts`, and `version` in `package.json` for the PBW.

## Store submission

1. Host the APK at the companion link the store description gives,
   https://ark-am.com/projects/pebble-apps. The PBW's `companionApp.android.url`
   in `package.json` points to https://github.com/ark-am/pebble-phone-voice-commands;
   make sure that page exists or change it to the same link and rebuild.
2. Publish the watch app, either through the developer dashboard with the
   PBW, `STORE_DESCRIPTION.txt` and the artwork above, or with the Pebble tool:

   ```sh
   pebble publish --non-interactive --no-gif-all-platforms \
     --description "$(cat STORE_DESCRIPTION.txt)" \
     --icon-large store/icon-large-144.png \
     --icon-small store/icon-small-80.png \
     --screenshots store/screenshots/basalt.png store/screenshots/chalk.png \
     --release-notes "Quick commands: run up to six saved commands with Up or Down, set in Settings. Did you mean…?: the watch offers a corrected command when one is not understood."
   ```

   `--screenshots` only works with `--non-interactive`, and each file name
   must start with its platform. Set the category on the dashboard, or pass
   the store's key with `--category`. The release stays hidden until it is
   published, unless `--is-published` is passed.

## Verification

- `pebble build` passed with Pebble Tool 5.0.40 and SDK 4.33.1 for all six
  platforms. The PBW passed a ZIP integrity check, and its `appinfo.json`
  has the UUID, version 1.1.0 and the companion package above.
- `./gradlew assembleDebug testDebugUnitTest lintDebug` passed: 45 unit
  tests, no lint errors, and four warnings about newer dependency and SDK
  versions.
- `apksigner verify` passed (APK Signature Scheme v2).
- The screenshots are emulator captures of the watch app: the transcription
  came from `pebble transcribe`, and the reply, "Timer: 5 min", is the text
  the companion sends for that command, delivered with
  `pebble send-app-message`. The Emery emulator did not start on the build
  machine, so there is no Emery screenshot.
- Not yet tested on a physical watch and phone, or uploaded to the store.

## Icon provenance

Drawn as SVG to match `../phone-dialer/phone-dialer-icon.png` and
`../phone-sound-mode/phone-sound-mode-icon.png`: the same charcoal phone with a
transparent screen, a teal microphone, and an amber speech bubble with a voice
waveform in place of Phone Dialer's clock badge. Rendered to PNG with headless
Chrome; the store sizes were cropped to the artwork and resized with ImageMagick.
