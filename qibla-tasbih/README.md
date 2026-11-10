# Qibla-Tasbih

A dhikr counter for every Pebble watch. Each press counts one, the ring
around the number fills towards the target, and the watch vibrates at the end
of each set, so you can count without looking. Everything runs on the watch:
there is no phone-side code, no settings page and no account.

The screen shows the phrase in Arabic at the top, its transliteration below,
the count in the middle with its target ("of 33"), and a bottom row with
today's count. On rectangular watches the ring sits around the number; on the
round ones it runs around the edge of the screen.

## Using it

- **Up, Select or Down:** count one. All three count, so whichever button is
  under your thumb works.
- **Tap the screen** (Pebble Time 2 and Pebble Round 2): count one.
- **Hold Up:** undo the last count. Straight after a reset, it brings the
  count back instead.
- **Hold Down** (about a second): reset the count for the dhikr on screen.
  The watch buzzes, and the bottom row says to hold Up to undo.
- **Hold Select:** the menu, with the list of dhikr and the settings.

The watch vibrates twice when a set within the after-prayer tasbih is done,
and once, for longer, when a whole set is complete. With no target, it
vibrates twice every 100.

## Dhikr

| Dhikr | Target to start with |
| --- | --- |
| After prayer: SubhanAllah, Alhamdulillah, Allahu Akbar | 33 + 33 + 34 |
| SubhanAllah | 33 |
| Alhamdulillah | 33 |
| Allahu Akbar | 33 |
| La ilaha illallah | 100 |
| Astaghfirullah | 100 |
| SubhanAllahi wa bihamdihi | 100 |
| Salawat (Allahumma salli 'ala Muhammad) | 100 |
| La hawla wa la quwwata illa billah | 100 |
| Free count, with no phrase | none |

**After prayer** moves on by itself: on the 33rd SubhanAllah the screen
changes to Alhamdulillah at 0, and dots in the bottom row show which set you
are on. After the 100th, the ring turns bright green and says Complete, and
the next press starts again. The other dhikr count in rounds: once the target
is reached, the next press starts round 2, shown in the bottom row.

Each dhikr keeps its own count, so you can switch between them and come back.
The counts, today's count and the all-time total are kept when you leave the
app. Today's count starts again at midnight.

## Settings

In the menu, below the list of dhikr:

- **Target** (or **Last of the 100** for the after-prayer tasbih): press to
  step through 10, 33, 99, 100 and 1000 times, or no target. For the
  after-prayer tasbih it chooses how the 100 ends: Allahu Akbar a 34th time
  (the default), or Allahu Akbar 33 times and then La ilaha illallahu wahdahu
  la sharika lah once. Both are in Sahih Muslim.
- **Vibrate:** at the end of sets only (the default), or a light tap on every
  count too, which helps when counting by touch.
- **Reset count:** resets the dhikr on screen, the same as holding Down.
- **Totals:** today's count and the all-time total.

## Images

Pebble's fonts have no Arabic, so each phrase is a bitmap drawn by
`tools/make_images.py` with IBM Plex Sans Arabic Bold (in `tools/fonts`,
under the SIL Open Font License). Pillow, built with libraqm, joins the
letters. Each phrase is drawn at one size for the 144 x 168 watches and the
Pebble Time Round, and a larger one for the Pebble Time 2 and Pebble Round 2,
shrinking only when it would not fit; the Tahlil goes onto two lines on the
small screens. Colour watches get four shades of grey, so the letters stay
smooth, and black-and-white watches get a 1-bit copy (the `~bw` files).

The script also draws the menu icon, `qibla-tasbih-icon.png` and the store icons,
and writes the phrase resources into `package.json`. After changing it, run:

```
python3 tools/make_images.py
```

The transliterations and meanings are in the `PHRASES` table at the top of
`src/c/main.c`. The screen layout for each size of watch is in `layout_init`.

## Build and test

```
pebble build
pebble install --emulator emery
pebble emu-button --emulator emery click select --repeat 33   # one set
pebble emu-button --emulator emery click up --duration 900    # undo
pebble emu-button --emulator emery click select --duration 900  # menu
```

The emulator has no touch input, so test tapping on a watch.

Supported platforms: Aplite, Basalt, Chalk, Diorite, Emery, Flint and Gabbro.
The `store` folder holds the store description, the store icons, and
screenshots for the Pebble 2 Duo, Pebble Time 2 and Pebble Round 2, named by
watch model.
