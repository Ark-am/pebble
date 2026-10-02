# Epoch publishing files

## Release

Title: **Epoch**  
Type: **Watchface**  
Author: **ark-am**  
Version: **1.0.0**  
UUID: `363239a9-545d-431d-8d1d-3c8104189ff0`

| File | Purpose |
| --- | --- |
| `build/epoch.pbw` | Installable release bundle for Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro. |
| `epoch-icon.png` | 1254 × 1254 RGBA store icon with transparency. |
| `epoch-screenshot.png` | The single screenshot supplied: native 200 × 228 Emery capture, without a watch frame. |

The PBW is generated output and is ignored by Git. Rebuild with `npm ci`
followed by `pebble build` from this directory.

## Store description

Epoch brings a classic analog dial to Pebble, with tapered hands, a minute track
that follows the screen, and a choice of turned classic or upright modern hour
numbers.

See local weather, the date, and battery and connection status at a glance.
Tap the watch to cycle through steps, distance, active calories, active minutes,
sleep, and heart rate where supported. Health data is unavailable on Aplite.

Choose a light or dark background, personalize the name on the dial, select
Celsius or Fahrenheit, and optionally enable a second hand. A disconnect alert
respects Quiet Time. Weather uses the paired phone's location and requires
internet access; no weather account or API key is needed.

## Verification and submission

- `pebble build` passed using Pebble Tool 5.0.40 and SDK 4.33.1 for all seven platforms.
- Both companion JavaScript source files passed `node --check`.
- The bundle contains all seven watch binaries and the bundled JavaScript;
  its ZIP integrity check passed.
- The screenshot uses emulator sample data: 24°C, clear daytime weather,
  8,432 steps, and 80% battery. It shows the default light/classic appearance.
- Upload the PBW and artwork through your developer dashboard. The screenshot
  supplied covers Emery; other platform collections may require their own
  assets. No additional screenshots are supplied.
- This release has been prepared locally; it has not been uploaded or published.

Screenshot framing and asset collections follow the
[Pebble submission guide](https://developer.rebble.io/guides/appstore-publishing/preparing-a-submission/)
and [asset guide](https://developer.rebble.io/guides/appstore-publishing/appstore-assets/).

## Icon provenance

Created using the built-in ImageGen tool with this prompt:

> Use case: logo-brand
> Asset type: square Pebble app-store icon for the Epoch analog watchface.
> Primary request: Create one polished, minimal icon of a classic analog watch dial. Ivory-white circular dial, charcoal thin minute marks, longer hour marks, elegant tapered charcoal leaf hands pointing to 10:10, a small teal centre hub and subtle teal detail. A clean charcoal circular outline. The clock fills roughly 78 percent of the square with generous clear margins. Flat, crisp vector-like artwork, strong legibility at small sizes, understated and timeless. No watch strap, no phone, no typography, no letters, no numbers, no complications, no extra symbols, no shadows, no gradients, no watermark. Fully transparent background outside the dial. One icon only, square composition.
