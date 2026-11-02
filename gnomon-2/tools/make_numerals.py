#!/usr/bin/env python3
"""Renders the hour numbers as smooth, pre-turned images.

Pebble's fonts cannot be rotated and are drawn without smoothing, so on colour
watches the hour numbers, classic and modern, are drawn from these images
instead. Each number is rendered large from Noto Serif Display Bold, narrowed to a condensed shape,
turned to follow the dial and scaled down, which leaves soft edges. Black and
white versions cover the light and dark backgrounds.

The condensed numbers are taller and come from Antonio. Black-and-white watches
draw their classic numbers as strokes, but use images for the condensed ones:
a single hard-edged mask each, drawn on or cleared from the dial for either
background.

Run from the project directory after changing the font, sizes or angles:

    python3 tools/make_numerals.py

It writes resources/images/numerals/*.png and the matching resource entries in
package.json.
"""

import json
import os

from PIL import Image, ImageDraw, ImageFont

# The styles: resource prefix, font, width scale (the classic numbers are
# narrowed to a tall shape), extra outline in hundredths of the digit height so
# thin strokes survive being scaled down, and the digit heights in pixels,
# matching CLASSIC_HEIGHT and CONDENSED_HEIGHT in src/c/main.c, for the colour
# watches of each size.
STYLES = [
    ('numeral', '/usr/share/fonts/truetype/noto/NotoSerifDisplay-Bold.ttf', 0.62, 2,
     {'large': (20, ['emery', 'gabbro']), 'small': (15, ['basalt', 'chalk'])}),
    ('numeral_condensed', 'resources/fonts/Antonio-Regular.ttf', 1, 0,
     {'large': (30, ['emery', 'gabbro']), 'small': (20, ['basalt', 'chalk'])}),
]
SUPERSAMPLE = 8

# The masks for black-and-white watches.
MASK_FONT = 'resources/fonts/Antonio-SemiBold.ttf'
MASK_HEIGHT = 20
MASK_PLATFORMS = ['aplite', 'diorite', 'flint']

COLOURS = {'black': (0, 0, 0), 'white': (255, 255, 255)}

# Clockwise turn for each number: tops face outward, except on the lower half
# of the dial, where that would turn them upside down. The 3 and 9 are for the
# modern numbers, which stay upright.
TURNS = {12: 0, 2: 60, 3: 0, 4: -60, 6: 0, 8: 60, 9: 0, 10: -60}

OUT_DIR = 'resources/images/numerals'


def render_mask(font_file, condense, embolden, text, height, turn):
    font = ImageFont.truetype(font_file, 100 * SUPERSAMPLE)
    canvas = Image.new('L', (len(text) * 80 * SUPERSAMPLE, 140 * SUPERSAMPLE), 0)
    ImageDraw.Draw(canvas).text((10 * SUPERSAMPLE, 0), text, font=font, fill=255,
                                stroke_width=embolden * SUPERSAMPLE, stroke_fill=255)
    mask = canvas.crop(canvas.getbbox())
    scale = height * SUPERSAMPLE / mask.height
    mask = mask.resize((round(mask.width * scale * condense), height * SUPERSAMPLE),
                       Image.LANCZOS)
    # PIL turns counter-clockwise.
    mask = mask.rotate(-turn, resample=Image.BICUBIC, expand=True)
    size = (max(1, round(mask.width / SUPERSAMPLE)), max(1, round(mask.height / SUPERSAMPLE)))
    return mask.resize(size, Image.LANCZOS)


def render(style, text, height, colour, turn):
    mask = render_mask(style[1], style[2], style[3], text, height, turn)
    image = Image.new('RGBA', mask.size, colour + (0,))
    image.putalpha(mask)
    return image


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    entries = []
    for style in STYLES:
        for size_name, (height, platforms) in style[4].items():
            for colour_name, colour in COLOURS.items():
                for hour, turn in TURNS.items():
                    name = '%s_%s_%s_%d' % (style[0], size_name, colour_name, hour)
                    path = os.path.join(OUT_DIR, name + '.png')
                    render(style, str(hour), height, colour, turn).save(path)
                    entries.append({
                        'type': 'png',
                        'name': name.upper(),
                        'file': os.path.relpath(path, 'resources'),
                        'targetPlatforms': platforms,
                    })
    for hour, turn in TURNS.items():
        name = 'numeral_condensed_mask_%d' % hour
        path = os.path.join(OUT_DIR, name + '.png')
        mask = render_mask(MASK_FONT, 1, 0, str(hour), MASK_HEIGHT, turn)
        mask.point(lambda alpha: 255 if alpha >= 128 else 0).convert('1').save(path)
        entries.append({
            'type': 'bitmap',
            'name': name.upper(),
            'file': os.path.relpath(path, 'resources'),
            'memoryFormat': '1Bit',
            'targetPlatforms': MASK_PLATFORMS,
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
