# Phone Dialer for Pebble

Call anyone in your Android phone's address book from the watch.

- **Favorites** lists the numbers of your starred contacts.
- **Contacts** lists the letters A–Z (and `#` for everything else) with a
  contact count, then every number under the chosen letter.

Each row shows the contact's name with the number's type and the number
underneath, so a contact with several numbers appears once per number. Select
a row and the phone places the call. A short vibration confirms it, and the app
returns to the watchface so the phone's call screen can take over.

The included Android companion app reads the contacts and places the call. The
watch only holds one page of 20 rows at a time, so the app works the same with
a large address book on every watch, including Aplite. Long lists end with a
**More** row that loads the next page and start with a **Previous** row that
goes back.

## Requirements

- A Pebble-compatible Android companion app with PebbleKit Android 2 support
  (the current Pebble/Core app is supported).
- Android 7.0 or newer.
- JDK 21 or newer and the Android SDK (platform 36) to build the companion.
- The included Android companion app must be installed.
- The user must grant the **Contacts** and **Phone** permissions once.

This implementation is Android-only. iOS does not let a background app place
calls. There is intentionally no PebbleKit JS component either: Pebble routes
AppMessages to the native Android companion declared in `package.json`.

## Build the watch app

From this directory:

```sh
pebble build
```

The installable bundle is created at:

```text
build/phone-dialer.pbw
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

Open **Phone Dialer** on the phone once and tap **Grant contacts and phone
access**. If Android stops asking, the button changes to **Open app settings**,
where both permissions can be allowed by hand.

## Install and use

Install the Android APK first, grant access, and then install the PBW through
the Pebble phone app. Open **Phone Dialer** on the watch and choose
**Favorites** or **Contacts**.

If the watch says **Allow access on phone**, open the companion app and grant
the permissions. If the phone does not answer within 10 seconds, the list shows
**No response**; select it to retry.

## How it works

The watch and the companion exchange AppMessages with the keys listed in
`package.json`. Every request carries a token that the phone echoes back, so
replies to an abandoned request (after going back, for example) are ignored.

| Request | Watch sends | Phone replies |
| --- | --- | --- |
| List a page | `LIST` (0 favorites, 1 letters, 2 contacts), `FILTER` (the letter), `OFFSET`, `LIMIT`, `CAPACITY` | One or more messages with `TOTAL`, `OFFSET` and `ITEMS`; the last also has `FINAL` |
| Call | `ITEM_ID` (the phone-number row ID) | `RESULT` |

`ITEMS` packs entries as `id␟title␟subtitle`, separated by `␞` (ASCII unit and
record separators). The phone fills each message up to the `CAPACITY` the
watch reports for its inbox, and trims names to 31 bytes without splitting a
UTF-8 character. Phone numbers never leave the phone except as display text;
the call is looked up by row ID so it always uses the current number.

The companion caches the sorted address book for 30 seconds, so paging through
a list does not query the contacts provider for every page. Duplicate numbers
for the same contact, which synced accounts often create, are shown once.

Calls go through `TelecomManager.placeCall`, which shows the system's own call
screen. Starting an `ACTION_CALL` activity would not work here, because Android
10 and later block activity launches from the background.

## Publishing checklist

Before submitting publicly:

1. Publish the Android companion to Google Play or a stable download page.
2. Replace `pebble.companionApp.android.url` in `package.json` with that URL.
3. Produce a signed Android App Bundle for Google Play. Google Play asks apps
   that request `READ_CONTACTS` to explain the use in the listing and the data
   safety form; contacts are only read on the device and sent to the watch.
4. Build the release PBW and upload it to the Pebble store.
5. State clearly in both listings that the Android companion and the Contacts
   and Phone permissions are required. This implementation is Android-only.

The watch app UUID and Android package name are already matched:

```text
Watch UUID:      8269f312-4d4c-4149-95fa-9f5042fcf467
Android package: com.arkam.pebbledialer
```
