#!/usr/bin/env python3
"""Writes app/package.json, the Alien Core watch app's, from package.json.

The watch app is built from the watch face's own code and resources (app/src
and app/resources link to them), so its package.json is the face's with a
uuid of its own, the name of an app, and "watchface" off. Run from the
project directory after changing package.json:

    python3 tools/make_app_package.py
"""

import json

APP_UUID = 'c7098e00-ae97-4fd9-ae34-72a0c0d3328e'
APP_VERSION = '1.0.0'

with open('package.json') as f:
    package = json.load(f)

package['name'] = 'alien-core-app'
package['version'] = APP_VERSION
package['pebble']['uuid'] = APP_UUID
package['pebble']['watchapp'] = {'watchface': False}

with open('app/package.json', 'w') as f:
    json.dump(package, f, indent=2)
    f.write('\n')
