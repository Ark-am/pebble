# Pebble Apps

A growing collection of independent apps and experiments for Pebble watches.
Each project lives in its own directory with its own source code, dependencies,
build output, and documentation.

## Projects

| Project | Description | Platforms |
| --- | --- | --- |
| [Qibla Compass](./qibla-compass/) | Points toward the Qibla using the watch compass, the phone's location, and magnetic-declination correction. | Emery (Pebble Time 2) |
| [Phone Sound Mode](./phone-sound-mode/) | Changes a paired Android phone's sound mode from the watch using a native Android companion app. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Meridian](./meridian/) | Analog watchface with weather, date, battery, connection and Quiet Time alerts, and health data. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Epoch](./epoch/) | Classic analog watchface with turned hour numbers and tapered hands, plus weather, health data, the date, a battery ring, and connection and Quiet Time alerts. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |
| [Baltic](./baltic/) | Dress-watch face with Breguet-style numbers, a minute scale, a small seconds dial, and five dial colours. | Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro |

Follow a project's link for its requirements, build instructions, testing
steps, and platform-specific notes.

## Repository layout

```text
.
├── .gitignore           # Shared ignore rules for every project
├── qibla-compass/       # Qibla direction app
├── phone-sound-mode/    # Watch app and Android companion
├── meridian/            # Analog watchface
├── epoch/               # Classic analog watchface
├── baltic/              # Dress-watch face
└── README.md            # Repository overview
```

Projects are intentionally self-contained. There is no root-level build
command, and commands should be run from the directory of the app being
developed.

## Getting started

Install the [Pebble SDK](https://developer.repebble.com/) and confirm that the
Pebble CLI is available:

```sh
pebble --version
```

Then choose a project and follow its README. A typical Pebble app workflow is:

```sh
cd <project-directory>
npm install      # when the project declares JavaScript dependencies
pebble build
```

The resulting `.pbw` file is written to that project's `build/` directory.
Some projects may require additional tooling or a companion app; those details
are documented by the project.

## Adding another project

Add each new app in a clearly named top-level directory and keep everything it
needs inside that directory. At minimum, a project should include:

- a `README.md` describing its purpose, requirements, supported watches, and
  build and installation steps;
- its Pebble manifest and build configuration;
- source code and required resources;
- additional project-specific `.gitignore` rules when it produces artifacts
  not covered by the root rules; and
- license information when it differs from the rest of the collection.

After adding a project, include it in the table above and update the repository
layout. Do not commit generated build output, dependency directories, local SDK
state, signing material, or secrets.

## Licenses

Licensing may vary between projects. Check the `LICENSE` file or README inside
the relevant project before using or redistributing its code.
