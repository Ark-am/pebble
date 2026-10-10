# Pebble Apps

A growing collection of independent apps and watch faces for the Pebble watches. Each project lives in its own directory with its own source code, dependencies, build output, and documentation.

| Code name | Watch | Screen |
| --- | --- | --- |
| Aplite | Pebble (original) and Pebble Steel | 144 × 168, black and white |
| Basalt | Pebble Time and Pebble Time Steel | 144 × 168, colour |
| Chalk | Pebble Time Round | 180 × 180 round, colour |
| Diorite | Pebble 2 (SE and HR) | 144 × 168, black and white |
| Emery | Pebble Time 2 | 200 × 228, colour, touch |
| Flint | Pebble 2 Duo | 144 × 168, black and white |
| Gabbro | Pebble Round 2 | 260 × 260 round, colour, touch |

## Projects

| Project | Description | Platforms |
| --- | --- | --- |
| [Phone Dialer](./phone-dialer/) | Dial numbers, returns Recent Calls, and calls from Favorites and/or Contacts on a paired Android phone using the native companion app. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Phone Sound Mode](./phone-sound-mode/) | Changes the Android phone's sound mode (Vibrate, Normal, Silent) using the native Android companion app. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Qibla Compass](./qibla-compass/) | Points toward the Qibla using the watch compass, the phone's location, and magnetic-declination correction. | Emery and Flint, but currently not fully operational |
| [Phone Voice Commands](./phone-voice-commands/) | Sends dictated commands from the watch to a paired Android phone: calls, texts, directions, timers, alarms, media and more. | Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Baltic](./baltic/) | Dress-watch face with Breguet-style numbers, a minute scale, a small seconds dial, and five dial colours. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Gnomon 2](./epoch/) | Classic analog watch-face with turned hour numbers and tapered hands, plus weather, health data, the date, a battery ring, and connection and Quiet Time alerts. Heavily inspired by the Gnomon watch-face. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Meridian](./meridian/) | Analog watch-face with weather, date, battery, connection and Quiet Time alerts, and health data. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Sketchy Weather Analog](./sketchy-weather-analog/) | Pencil-sketch analog watch-face with a hand-drawn weather widget in the centre where the hands stay clear. Inspired by the Sketchy Weather watch-face. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [World Clock](./world-clock/) | Digital face with the current local time being large and at the top. Beside is the date, and below are the times of two other cities. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |

Follow a project's link for its requirements, build instructions, testing steps, and platform-specific notes, but please try not to follow because we are still new to Python and C languages. 

## Apps' & Watch-faces' descriptions

| Watch faces | Apps |
| --- | --- |
| [Baltic](./baltic/STORE_DESCRIPTION.txt) | [Phone Dialer](./phone-dialer/STORE_DESCRIPTION.txt) |
| [Gnomon 2](./gnomon-2/STORE_DESCRIPTION.txt) | [Phone Sound Mode](./phone-sound-mode/STORE_DESCRIPTION.txt) |
| [Meridian](./meridian/STORE_DESCRIPTION.txt) | [Qibla Compass](./qibla-compass/STORE_DESCRIPTION.txt) |
| [Sketchy Weather Analog](./sketchy-weather-analog/STORE_DESCRIPTION.txt) |
| [World Clock](./world-clock/STORE_DESCRIPTION.txt) |

> Created by Ark-am LLC. Discover our work and custom software development services:\
> https://ark-am.com/projects/pebble-apps

## Repository layout

```text
.
├── .gitignore               # Shared ignore rules for every project
├── qibla-compass/           # Qibla direction app
├── phone-sound-mode/        # Watch app and Android companion
├── phone-dialer/            # Watch dialer and Android companion
├── phone-voice-commands/    # Watch dictation app and Android companion
├── meridian/                # Analog watch-face
├── epoch/                   # Classic analog watch-face
├── baltic/                  # Dress-watch face
├── sketchy-weather-analog/  # Pencil-sketch weather watch-face
├── world-clock/             # Digital world clock
└── README.md                # Repository overview
```

Projects are intentionally self-contained. There is no root-level build command, and commands should be run from the directory of the app being developed.

## Licenses

All of the code in this repository is open source. Every project is released under the MIT License; see the `LICENSE` file inside each project's directory. You are free to use, modify, and redistribute the code, provided the copyright and license notice are kept with the directory.
