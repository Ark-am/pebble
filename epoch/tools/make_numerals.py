#!/usr/bin/env python3
"""Renders the classic hour numbers as smooth, pre-turned images.

Pebble's fonts cannot be rotated and are drawn without smoothing, so on colour
watches the classic numbers are drawn from these images instead. Each number is
rendered large from Noto Serif Display Bold, narrowed to a condensed shape,
turned to follow the dial and scaled down, which leaves soft edges. Black and
white versions cover the light and dark backgrounds.

Run from the project directory after changing the font, sizes or angles:

    python3 tools/make_numerals.py

It writes resources/images/numerals/*.png and the matching resource entries in
package.json.
"""

import json
import os

from PIL import Image, ImageDraw, ImageFont

FONT = '/usr/share/fonts/truetype/noto/NotoSerifDisplay-Bold.ttf'
CONDENSE = 0.62  # Width scale that gives the tall, narrow classic numbers.
EMBOLDEN = 2  # Extra outline, in hundredths of the digit height, so thin
             # strokes survive being scaled down.
SUPERSAMPLE = 8

# Digit height in pixels, matching CLASSIC_HEIGHT in src/c/main.c, and the
# colour watches each size is for.
SIZES = {
    'large': (20, ['emery', 'gabbro']),
    'small': (15, ['basalt', 'chalk']),
}
COLOURS = {'black': (0, 0, 0), 'white': (255, 255, 255)}

# Clockwise turn for each number: tops face outward, except on the lower half
# of the dial, where that would turn them upside down.
TURNS = {12: 0, 2: 60, 4: -60, 6: 0, 8: 60, 10: -60}

OUT_DIR = 'resources/images/numerals'


def render(text, height, colour, turn):
    font = ImageFont.truetype(FONT, 100 * SUPERSAMPLE)
    canvas = Image.new('L', (len(text) * 80 * SUPERSAMPLE, 140 * SUPERSAMPLE), 0)
    ImageDraw.Draw(canvas).text((10 * SUPERSAMPLE, 0), text, font=font, fill=255,
                                stroke_width=EMBOLDEN * SUPERSAMPLE, stroke_fill=255)
    mask = canvas.crop(canvas.getbbox())
    scale = height * SUPERSAMPLE / mask.height
    mask = mask.resize((round(mask.width * scale * CONDENSE), height * SUPERSAMPLE),
                       Image.LANCZOS)
    # PIL turns counter-clockwise.
    mask = mask.rotate(-turn, resample=Image.BICUBIC, expand=True)
    size = (max(1, round(mask.width / SUPERSAMPLE)), max(1, round(mask.height / SUPERSAMPLE)))
    mask = mask.resize(size, Image.LANCZOS)
    image = Image.new('RGBA', size, colour + (0,))
    image.putalpha(mask)
    return image


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    entries = []
    for size_name, (height, platforms) in SIZES.items():
        for colour_name, colour in COLOURS.items():
            for hour, turn in TURNS.items():
                name = 'numeral_%s_%s_%d' % (size_name, colour_name, hour)
                path = os.path.join(OUT_DIR, name + '.png')
                render(str(hour), height, colour, turn).save(path)
                entries.append({
                    'type': 'png',
                    'name': name.upper(),
                    'file': os.path.relpath(path, 'resources'),
                    'targetPlatforms': platforms,
                })

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
