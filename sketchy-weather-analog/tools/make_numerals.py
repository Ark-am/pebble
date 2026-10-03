#!/usr/bin/env python3
"""Renders the hour numbers as smooth images.

Pebble's fonts are drawn without smoothing, so the hour numbers are drawn from
these images instead. Each number is rendered large from Noto Serif Display,
a high-contrast serif, slightly narrowed and thickened so its fine strokes
survive, then scaled down, which leaves soft edges.

Colour watches get black and white versions with transparency, for the light
and dark backgrounds. Black-and-white watches get the same without grey
edges, as their screens have no grey.

Run from the project directory after changing the font or sizes:

    python3 tools/make_numerals.py

It writes resources/images/numerals/*.png and the matching resource entries in
package.json.
"""

import json
import os

from PIL import Image, ImageDraw, ImageFont

FONT = '/usr/share/fonts/truetype/noto/NotoSerifDisplay-Regular.ttf'
CONDENSE = 0.82
SUPERSAMPLE = 8

# Digit height in pixels, extra outline in hundredths of that height (so the
# fine strokes survive being scaled down), and the watches each set is for.
COLOUR_SIZES = {
    'large': (16, 0.5, ['emery', 'gabbro']),
    'small': (12, 1.5, ['basalt', 'chalk']),
}
BLACK_AND_WHITE = (12, 2, ['aplite', 'diorite', 'flint'])
COLOURS = {'black': (0, 0, 0), 'white': (255, 255, 255)}

OUT_DIR = 'resources/images/numerals'


def render_mask(text, height, embolden):
    font = ImageFont.truetype(FONT, 100 * SUPERSAMPLE)
    canvas = Image.new('L', (len(text) * 90 * SUPERSAMPLE, 160 * SUPERSAMPLE), 0)
    ImageDraw.Draw(canvas).text((10 * SUPERSAMPLE, 0), text, font=font, fill=255,
                                stroke_width=round(embolden * SUPERSAMPLE), stroke_fill=255)
    mask = canvas.crop(canvas.getbbox())
    scale = height / mask.height
    size = (max(1, round(mask.width * scale * CONDENSE)), height)
    return mask.resize(size, Image.LANCZOS)


def colour_image(mask, colour):
    image = Image.new('RGBA', mask.size, colour + (0,))
    image.putalpha(mask)
    return image


def sharp(mask):
    return mask.point(lambda value: 255 if value >= 110 else 0)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    entries = []

    def add(name, image, platforms):
        path = os.path.join(OUT_DIR, name + '.png')
        image.save(path)
        entries.append({
            'type': 'png',
            'name': name.upper(),
            'file': os.path.relpath(path, 'resources'),
            'targetPlatforms': platforms,
        })

    for hour in range(1, 13):
        for size_name, (height, embolden, platforms) in COLOUR_SIZES.items():
            mask = render_mask(str(hour), height, embolden)
            for colour_name, colour in COLOURS.items():
                add('numeral_%s_%s_%d' % (size_name, colour_name, hour),
                    colour_image(mask, colour), platforms)
        height, embolden, platforms = BLACK_AND_WHITE
        mask = sharp(render_mask(str(hour), height, embolden))
        for colour_name, colour in COLOURS.items():
            add('numeral_bw_%s_%d' % (colour_name, hour), colour_image(mask, colour), platforms)

    with open('package.json') as f:
        package = json.load(f)
    media = [m for m in package['pebble']['resources']['media']
             if not m['name'].startswith('NUMERAL_')]
    package['pebble']['resources']['media'] = media + entries
    with open('package.json', 'w') as f:
        json.dump(package, f, indent=2)
        f.write('\n')


if __name__ == '__main__':
    main()
