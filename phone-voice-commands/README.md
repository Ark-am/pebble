# Phone Voice Commands for Pebble

Speak a command into your Pebble and have your Android phone carry it out.

The watch turns your speech into text with Pebble dictation and sends the
text to an Android companion app. The companion works out what you asked for,
does it, and sends a short reply back to the watch.

> **Work in progress.** The companion does not run any commands yet. It
> replies with **Heard: …** and the words it received, which confirms that the
> watch, the Pebble phone app and the companion are talking to each other.

## Requirements

- A Pebble with a microphone: Pebble Time, Time Steel, Time Round, Pebble 2
  (not the 2 SE), Pebble 2 Duo, Pebble Time 2 or Pebble Round 2. The original
  Pebble and Pebble Steel (Aplite) have no microphone and are not supported.
- An Android 7.0+ phone with a Pebble/Core phone app that supports
  dictation and PebbleKit Android 2.
- The included Android companion app.
- JDK 21 or newer and Android SDK platform 36 when building the companion.

## Build the watch app

From this directory:

```sh
pebble build
```

The installable bundle is created at:

```text
build/phone-voice-commands.pbw
```

## Build the Android companion

From `android/`:

```sh
./gradlew assembleDebug testDebugUnitTest lintDebug
```

Install it on a connected Android phone:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

This is a sideload build signed with the build machine's debug key.

## Use

Install the Android APK first, then install the PBW through the Pebble phone
app.

Open **Phone Voice Commands** on the watch. It starts listening straight
away. Speak, check the transcription on the confirmation screen, and accept
it. Press **Select** to speak again, or **Back** to leave.

The phone's reply appears in the bar at the bottom of the screen. One short
vibration means the command worked; two mean it did not.

The companion's screen on the phone shows the last command it received and
the reply it sent. It updates while open, which helps when testing.

## Watch–phone messages

| Key | Name | Direction | Value |
| --- | --- | --- | --- |
| 1 | `TRANSCRIPT` | watch → phone | The dictated text |
| 2 | `RESULT` | phone → watch | `0` OK, `1` failed, `2` not understood, `3` permission required |
| 3 | `REPLY` | phone → watch | Optional text to show, up to 127 bytes |

The watch app UUID and Android package name are matched:

```text
Watch UUID:      01430625-332e-46aa-b212-89d36377c0a5
Android package: com.arkam.pebblevoicecommands
```
