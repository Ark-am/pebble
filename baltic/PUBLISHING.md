# Baltic publishing files

## Release

| Field | Value |
| --- | --- |
| Title | Baltic |
| Type | Watchface |
| Author | ark-am |
| Version | 1.0.0 |
| UUID | `cc7fc976-cb70-4bc7-b868-864149968a66` |

| File | Purpose |
| --- | --- |
| `build/baltic.pbw` | Installable release bundle for Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro. Includes the existing 25 × 25 watch menu icon. |
| `baltic-icon.png` | 1254 × 1254 RGBA store icon with transparency. |
| `baltic-screenshot.png` | The single screenshot supplied: native 180 × 180 Chalk capture, without a watch frame. |

The PBW is generated output and is ignored by Git. Rebuild with `npm ci`
followed by `pebble build` from this directory.

## Store description

Baltic brings a classic dress-watch dial to Pebble: elegant serif hour numbers,
slim leaf hands, a railroad minute track, and a small seconds dial at the
half-past-seven position.

Choose Salmon, Navy blue, Silver, Black and gold, or Azure blue. Personalize
the dial name with up to 12 characters, or leave it blank for a clean dial.
Running seconds can be switched off to reduce battery use.

The circular dial fills round watches; rectangular watches show a black
surround. Black-and-white watches use light or dark monochrome equivalents
of the selected dial. Settings are available in the Pebble phone app. No
separate companion app, account, or API key is needed.

## Verification and submission

- `pebble build` passed using Pebble Tool 5.0.40 and SDK 4.33.1 for all seven platforms.
- Both companion JavaScript source files passed `node --check`.
- The bundle contains all seven watch binaries and the bundled JavaScript;
  its ZIP integrity, binary names, and UUID checks passed.
- The screenshot shows the default Salmon dial, BALTIC name, and running seconds,
  with the emulator time set near 10:10.
- Upload the PBW and artwork through your developer dashboard. The screenshot
  supplied covers Chalk; other platform collections may require their own
  assets. No additional screenshots are supplied.
- This release has been prepared locally; it has not been uploaded or published.

Screenshot framing and asset collections follow the
[Pebble submission guide](https://developer.rebble.io/guides/appstore-publishing/preparing-a-submission/)
and [asset guide](https://developer.rebble.io/guides/appstore-publishing/appstore-assets/).

The bundled numeral font is distributed under the SIL Open Font License;
its license is preserved in `resources/fonts/OFL.txt`.

## Icon provenance

Created using the built-in ImageGen tool with this prompt:

> Use case: logo-brand
> Asset type: square Pebble app-store icon for the Baltic analog watchface.
> Primary request: Create one elegant, polished, minimal icon of a classic small-seconds dress-watch dial. A salmon-pink circular dial, fine dark burgundy railroad minute track around the edge, white elegant tapered leaf hands pointing to 10:10 with a subtle charcoal outline, a small central hub, and a clearly visible small seconds subdial at the lower left, centred near the 7:30 position. Use simple white hour indices; no numerals or text. The dial fills approximately 80 percent of the square with clear margins. Crisp flat vector-like artwork, readable at small sizes, understated classic design. No watch strap, no phone, no typography, no letters, no logo, no extra symbols, no gradients, no cast shadows, no watermark. Transparent background outside the circular dial. One icon only, square composition.
