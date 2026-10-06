# Phone Dialer for Pebble

Dial a number, return a recent call, or call a favorite/contact from your Pebble
through the included Android companion. Version 1.1.0.

## Store artwork

- [App icon](phone-dialer-icon.png): transparent artwork matching Phone Sound Mode's charcoal, teal, and amber style.
- [Watch screenshot](phone-dialer-screenshot.png): actual 144×168 Aplite menu with Dialer, Recent calls, Favorites, and Contacts.

## Install

The prepared files are in `dist/`:

- `phone-dialer-1.1.0-android.apk`: signed debug APK, ready for direct installation.
- `phone-dialer-1.1.0.pbw`: watch app for Aplite, Basalt, Chalk, Diorite, Emery,
  Flint and Gabbro.
- `phone-dialer-1.1.0-install.zip`: both files and this guide.

1. Copy the APK to an Android 7.0+ phone and open it. Allow installation from
   that file/browser app when Android asks.
2. Open **Phone Dialer** on the phone. Tap **Allow Phone** to enable calling
   (and, on Android 9 and newer, ending calls from the watch),
   **Allow Contacts** for Favorites/Contacts, and **Allow Call history** for
   Recent calls. These permissions are independent; denying call history does
   not disable the keypad or contacts.
3. Open the PBW with your Pebble/Core phone app and install it on your paired
   watch. The phone app must support PebbleKit Android 2 (as used by Phone
   Sound Mode). Install **both** updated packages.
4. Open **Phone Dialer** on the watch.

This is a sideload build, signed with the build machine's Android debug key,
not a Play Store release. An existing installation signed with a different
key cannot be updated in place; Android reports a signature mismatch. Removing
that old companion resets its permission settings; install this APK afterwards.

For USB installation with Android debugging enabled:

```sh
adb install -r dist/phone-dialer-1.1.0-android.apk
```

Android treats `READ_CALL_LOG` as a hard-restricted permission: the installer
must allowlist it before the user can grant it. If Call history remains blocked,
use the USB installation command above (do not use `--restrict-permissions`),
then reopen the companion and allow Call history. Device-management policy may
still disallow it. The app shows a permission error instead of an empty history.
See [Android's permission documentation](https://developer.android.com/reference/android/Manifest.permission#READ_CALL_LOG).

## Watch controls

- **Settings**: in the Pebble phone app, open **Phone Dialer** and tap the
  settings (gear) button. This is the only place to change these options, so
  nothing can be rearranged or switched off by accident on the watch:
  - **Theme**: Dark or Light. Until it is saved once, colour watches start
    dark and black-and-white watches start light.
  - **Main menu order**: what appears first, second, third and fourth.
    Choosing an item that is already in another place swaps the two.
  - **Touch input** (touch watches only): switch tapping and swiping on or off.
    When off, the watch uses the buttons only.
  - **Vibrate on tap** (touch watches only): a short tick when you tap a
    keypad key or the Call button.

  Saved settings reach the watch straight away if the app is open, otherwise
  the next time it opens, and the watch keeps them while the phone is away.
- **Touch** (Pebble Time 2, Pebble Round 2): menus and lists scroll with a
  swipe; tap a row to highlight it and tap it again to open it, as in the
  watch's own menus. Swipe right to go back. On the keypad, tap a key to press
  it. On the call screen, tap the green **Call** button.
- **Dialer**: a phone keypad with Delete (backspace icon), `+` and a green
  Call key along the bottom. Tap keys on touch watches. With the buttons,
  Up/Down move the highlight and Select presses the highlighted key; hold
  Up/Down to move faster. After using touch, the first button press only
  brings the highlight back. `+` is available at the beginning only. Press
  **Call**, review the number, and confirm with Select or the **Call** button.
  Back cancels. Up from the first key wraps directly to Call. Numbers can
  contain up to 31 characters.
- **Recent calls**: the latest 100 system call-log entries, newest first, with
  incoming/outgoing/missed/declined/blocked type and date/time. Select an entry,
  then Select again to call back. Private and unknown numbers are displayed
  but cannot be called.
- **Favorites**: phone numbers of starred contacts.
- **Contacts**: grouped by A–Z and `#`, then listed by name and number type.
  Contacts with multiple numbers appear once per number. Select a number,
  then confirm with Select.

Lists contain 20 rows per page, with **More** and **Previous** for navigation.
Contact names are shortened to fit the watch; dialing resolves the full number
from the phone's current contact or call-log record. No number is derived from
truncated display text.

Once Android accepts the call, the watch vibrates and shows the call screen:
**Calling**, the name, the number, and a red **End** button. Press Down (or tap
**End** on touch watches) to hang up; Back leaves the screen and the call
continues on the phone. When the call ends, whether from the watch, the phone
or the other person, the watch shows **Call ended** and returns to its
watchface. Calling means Android placed the call; it does not show whether the
other person is ringing or has answered. Android owns the call screen and SIM choice.
On dual-SIM phones, set a default calling SIM or complete selection on the phone.
Use the phone itself for emergency calls. Short/service-code support depends on
Android and the carrier.

If a list shows **No response**, verify the Pebble connection and select it to
retry. **Allow call history** means enable Call history in the companion;
**Allow access on phone** means enable Phone or Contacts as appropriate.
Permission buttons open app settings after Android stops showing the prompt.

A call is never automatically retried after a timeout: it might already have
reached the phone. Check the phone before going back and trying again. Repeated
Select presses on the same confirmation cannot create another request.

## Build and validate

Requirements: Pebble SDK/tool, JDK 21+, Android SDK platform/build tools 36.
Use Android Studio's bundled JDK if the default Java installation is older.

```sh
# From phone-dialer/
npm install      # the settings page library (Clay)
pebble build

cd android
./gradlew assembleDebug testDebugUnitTest lintDebug
```

Outputs: `build/phone-dialer.pbw` and
`android/app/build/outputs/apk/debug/app-debug.apk`.

Before relying on the app, check on a physical paired phone/watch: granting and
denying each permission independently, calls with the phone locked/backgrounded,
recent incoming/outgoing/missed calls, and any dual-SIM prompt. A desktop build
and emulator cannot verify the phone manufacturer's Telecom behavior.

## Protocol and data

The watch and companion use the AppMessage keys in `package.json`. The watch
app also has a small PebbleKit JS part, `src/pkjs/`, which only serves the
Settings page; the Pebble phone app delivers the watch's messages to both it
and the Android companion. The watch UUID and native Android package are
unchanged:

```text
Watch UUID:      8269f312-4d4c-4149-95fa-9f5042fcf467
Android package: com.arkam.pebbledialer
```

| Request | Watch sends | Phone replies |
| --- | --- | --- |
| List (`REQUEST=1`) | `LIST` (0 favorites, 1 letters, 2 contacts, 3 recents), `FILTER`, `OFFSET`, `LIMIT`, `CAPACITY` | `TOTAL`, `OFFSET`, `ITEMS`; last packet also has `FINAL` |
| Call record (`REQUEST=2`) | `ITEM_ID` (decimal string), `LIST` (3 for call log, otherwise contacts) | `RESULT` |
| Dial number (`REQUEST=3`) | `NUMBER` (validated keypad text) | `RESULT` |
| Hang up (`REQUEST=5`) | nothing else | `RESULT` |

A successful call reply also carries `NUMBER`, the number dialled, for the call
screen. While that call lasts, the companion checks Android's audio mode once
a second (`MODE_IN_CALL` needs no extra permission) and, when the call ends,
sends `CALL_STATE` (0, ended) to the watch unprompted. If the phone never
reports a call, the watch is not told anything, rather than being told a live
call has ended. Hanging up uses `TelecomManager.endCall()`, which needs
`ANSWER_PHONE_CALLS` and Android 9 or newer; `REQUEST=4` is unused, as it was
the old settings sync.

The Settings page sends `THEME` (0 dark, 1 light), `MENU_1` to `MENU_4`
(0 Dialer, 1 Recent calls, 2 Favorites, 3 Contacts), `TOUCH_ENABLED` and
`TOUCH_VIBE` straight to the watch; the companion is not involved.

Every list, call and dial request carries a persistent, incrementing `TOKEN`, echoed in the reply.
Replies to abandoned requests are ignored. The companion also deduplicates the
last 64 call tokens per service lifetime. It accepts legacy integer contact IDs;
new IDs use strings to avoid truncating Android's 64-bit provider IDs.

`ITEMS` packs `id␟title␟subtitle`, separated by `␞` (ASCII unit/record
separators). Fields are sanitized and limited to 31 UTF-8 bytes without splitting
characters. Replies respect the watch inbox capacity and wait for ACK between
packets. The watch stores only one page of each open list.

Contacts have a 30-second cache. Recent calls are freshly queried, limited to
100 entries. Calls resolve the current record by ID before using
[`TelecomManager.placeCall`](https://developer.android.com/reference/android/telecom/TelecomManager#placeCall(android.net.Uri,%20android.os.Bundle)).
Contacts, call history and dialed numbers are not sent to an external server.

Public distribution needs a stable companion download URL in `package.json`,
a private release signing key, and review of applicable store policies for
Contacts/Phone/Call Log permissions. This companion does not replace Android's
default phone app.
