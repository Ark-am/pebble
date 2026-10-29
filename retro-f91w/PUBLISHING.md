# Retro F-91W publishing files

## Release

| Field | Value |
| --- | --- |
| Title | Retro F-91W |
| Type | Watchface |
| Author | ark-am |
| Version | 1.0.0 |
| UUID | `99a07f56-ca87-404b-bfe4-9ac34d93d0eb` |

| File | Purpose |
| --- | --- |
| `build/retro-f91w.pbw` | Installable release bundle for Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro. Includes the existing 25 × 25 watch menu icon. |
| `retro-f91w-icon.png` | 1254 × 1254 RGBA store icon with transparency. |
| `retro-f91w-screenshot.png` | The single screenshot supplied: native 200 × 228 Emery capture, without a watch frame. |

The PBW is generated output and is ignored by Git. Rebuild with `npm ci`
followed by `pebble build` from this directory.

## Store description

Retro F-91W brings the look of the classic Casio F-91W digital wristwatch. A
pale grey-green LCD sits inside a blue-framed face plate, with large segmented
digits for the time, the day and date above them, and the familiar printed
labels around the screen, right down to the boxed WR at the bottom.

Put your own brand and model name on the case, change or clear the printed
labels, and pick the LCD and case colours. The top row can show the day and
date, the local weather, your steps or your heart rate, or switch between them
with a shake of the wrist.

Settings are available in the Pebble phone app. Weather needs the paired
phone's location and internet access; no weather account or API key is needed.
Health data is unavailable on Aplite, and heart rate requires a supported
sensor. Not affiliated with or endorsed by Casio.

The full text is in `store/screenshots/STORE_DESCRIPTION.txt`.

## Verification and submission

- `pebble build` passed using Pebble Tool 5.0.40 and SDK 4.33.1 for all seven platforms.
- Both companion JavaScript source files passed `node --check`.
- The bundle contains all seven watch binaries and the bundled JavaScript;
  its ZIP integrity, binary names, and UUID checks passed.
- The screenshot shows the default appearance (classic pale grey-green LCD,
  black case, PEBBLE brand, day and date in the top row, seconds off), with
  the emulator time set to 10:10.
- Upload the PBW and artwork through your developer dashboard. The screenshot
  supplied covers Emery; other platform collections may require their own
  assets. No additional screenshots are supplied.
- This release has been prepared locally; it has not been uploaded or published.

Screenshot framing and asset collections follow the
[Pebble submission guide](https://developer.rebble.io/guides/appstore-publishing/preparing-a-submission/)
and [asset guide](https://developer.rebble.io/guides/appstore-publishing/appstore-assets/).

The bundled model-name font, IBM Plex Sans Condensed, is distributed under the
SIL Open Font License; its license is preserved in `resources/fonts/OFL-IBMPlex.txt`.

## Icon provenance

Drawn by `tools/make_icon.py` with Pillow, using the face's own colours: a blue
frame, black face plate with a silver line and red button arrows, a pale
grey-green LCD, and pointed seven-segment digits reading 10:10. It has no text
or logo. Regenerate with:

```
python3 tools/make_icon.py retro-f91w-icon.png
```
