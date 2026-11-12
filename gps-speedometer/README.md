# GPS Speedometer

Shows your speed from the phone's GPS on a colourful dial that lights up as you speed up. It also has large-digit and trip views. It is built mainly for the Pebble Time 2, works on every Pebble, and supports touch on the Pebble Time 2 and Pebble Round 2.

The phone side (`src/pkjs`) follows the phone's GPS and sends the speed, heading, accuracy and distance travelled about once a second while the app is open. It uses the phone's own GPS speed when the phone reports one. Otherwise it works the speed out from how far the position has moved. It ignores vague fixes, GPS jumps and the drift of a phone standing still, and it bridges gaps such as tunnels.

## Views

- **Dial:** the speed in the middle of a three-quarter dial that lights up through the theme's colours. The bottom row shows the trip distance, the heading and the top speed.
- **Large digits:** the speed as big as the screen allows, with a row of coloured blocks for the dial.
- **Trip:** distance, moving time, average moving speed and top speed.

The top row shows the GPS signal as four bars, and the time. When there is no speed to show, the screen says why: Starting GPS, Finding GPS, GPS signal lost, Location is off, or Phone not connected.

## Controls

| Input | Action |
| --- | --- |
| Up / Down | Previous / next view |
| Select | Menu |
| Hold Select | Reset the trip. Hold again within 6 seconds to undo the reset |
| Swipe left / right (touch) | Next / previous view |
| Swipe up (touch) | Menu |
| Tap the speed (touch) | Change units |
| Hold on the trip view (touch) | Reset the trip, or undo a reset |

## Settings (menu)

- **Units:** km/h, mph or knots. The default is mph on watches set to US English and km/h elsewhere.
- **Dial range:** either follows your speed (the default) or is fixed at 10 to 320.
- **Speed alert / Alert over:** vibrates twice when you pass the set speed, and turns the speed red until you drop back under it. Each unit keeps its own alert speed.
- **Colours** (colour watches): Aurora, Ice, Sunset, Neon or White.
- **Screen light:** automatic, or kept on while the app is open.
- **How to use:** the controls, on the watch.

The settings and the trip are kept between launches.

## Build and test

```
pebble build
node --test                      # phone-side speed and distance maths
pebble install --emulator emery
pebble send-app-message --emulator emery --int 10000=0 10001=2500 10002=90 10003=5 10004=0
```

The message keys are `STATUS` (0 fix, 1 searching, 2 denied, 3 error), `SPEED` (cm/s), `HEADING` (degrees, or -1), `ACCURACY` (metres, or -1) and `ODOMETER` (decimetres). Check `build/js/message_keys.json` for their numbers. Put all the values after a single `--int` flag. The emulator has no touch input and no moving GPS, so test touch and real speeds on a watch.

`python3 tools/make_icons.py` redraws the launcher, store and large icons. The digits use Antonio SemiBold (SIL Open Font License, in `resources/fonts`).

Supported platforms: Aplite, Basalt, Chalk, Diorite, Emery, Flint and Gabbro.

The `store` folder holds the store icons, the store description and four screenshots for each watch (dial, large digits, trip, and over the speed alert), named by watch model. They were taken in the emulators from a temporary build with staged trip values.
