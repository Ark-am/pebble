# Phone Sound Mode for Pebble

A deliberately small Pebble watch app with three actions:

- Vibrate
- Normal
- Silent

The watch app sends the selected mode to the included Android companion app.
The companion changes Android's ringer mode and returns the result to the watch.

## Requirements

- A Pebble-compatible Android companion app with PebbleKit Android 2 support
  (the current Pebble/Core app is supported).
- Android 7.0 or newer.
- JDK 21 or newer when building the Android companion.
- The included Android companion app must be installed.
- The user must grant **Do Not Disturb access** once. Android requires this for
  reliable transitions into and out of silent mode.

There is intentionally no PebbleKit JS component. Pebble routes AppMessages to
the native Android companion app declared in `package.json`.

## Build the watch app

From this directory:

```sh
pebble build
```

The installable bundle is created at:

```text
build/phone-sound-mode.pbw
```

## Build the Android companion

From `android/`:

```sh
./gradlew assembleDebug
```

Install it on a connected Android phone:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Open **Phone Sound Mode** once and tap **Grant Do Not Disturb access**. Enable
access for the app, return to it, and confirm that it says it is ready.

## Install and use

Install the Android APK first, grant access, and then install the PBW through
the Pebble phone app. Open **Phone Sound Mode** on the watch and choose one of
the three modes. A short vibration confirms that Android applied the mode, and
the app then returns to the watchface after one second. If the change fails,
the app stays open and shows why. If the phone does not answer within 10
seconds, select the same mode again to retry.

## Publishing checklist

Before submitting publicly:

1. Publish the Android companion to Google Play or a stable download page.
2. Replace `pebble.companionApp.android.url` in `package.json` with that URL.
3. Produce a signed Android App Bundle for Google Play.
4. Build the release PBW and upload it to the Pebble store.
5. State clearly in both listings that the Android companion and Do Not
   Disturb access are required. This implementation is Android-only.

The watch app UUID and Android package name are already matched:

```text
Watch UUID:      9e7fbef8-d934-4538-ae25-11f29029b3a4
Android package: com.arkam.pebblesoundmode
```
