# Phone Voice Commands for Pebble

Speak a command into your Pebble and have your Android phone carry it out.

The watch turns your speech into text with Pebble dictation and sends the
text to an Android companion app. The companion works out what you asked for,
does it, and sends a short reply back to the watch.

> **Work in progress.** Only the watch app exists so far. Until the Android
> companion is added, the watch shows **Phone unavailable** or
> **No response from phone** after you speak.

## Requirements

- A Pebble with a microphone: Pebble Time, Time Steel, Time Round, Pebble 2
  (not the 2 SE), Pebble 2 Duo, Pebble Time 2 or Pebble Round 2. The original
  Pebble and Pebble Steel (Aplite) have no microphone and are not supported.
- A Pebble/Core phone app with dictation enabled.

## Build the watch app

From this directory:

```sh
pebble build
```

The installable bundle is created at:

```text
build/phone-voice-commands.pbw
```

## Use

Open **Phone Voice Commands** on the watch. It starts listening straight
away. Speak, check the transcription on the confirmation screen, and accept
it. Press **Select** to speak again, or **Back** to leave.

The phone's reply appears in the bar at the bottom of the screen. One short
vibration means the command worked; two mean it did not.

## Watch–phone messages

| Key | Name | Direction | Value |
| --- | --- | --- | --- |
| 1 | `TRANSCRIPT` | watch → phone | The dictated text |
| 2 | `RESULT` | phone → watch | `0` OK, `1` failed, `2` not understood, `3` permission required |
| 3 | `REPLY` | phone → watch | Optional text to show, up to 127 bytes |
