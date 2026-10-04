#!/usr/bin/env python3
"""Sizes the bundled fonts to stand in for Pebble's own fonts.

The face lets the wearer choose a font style for its time and numbers and,
separately, for its information text. Pebble's own fonts are the default; the
other styles are bundled fonts in resources/fonts:

    Serif    IBM Plex Serif
    Rounded  Varela Round
    Mono     DM Mono

Each piece of text the face draws is a "role" in tools/fonts.json, with the
font it normally uses and the visible height of its capitals (or digits)
that the layout is built around. The usual font is one of Pebble's ("like"),
or a font file in resources/fonts at a given size ("like_file" and
"like_size"), whose digit height is then the visible height. For each role and style this script
picks the largest size whose capitals are no taller than that and whose text
is not much wider than Pebble's font, so the layout still fits.
The glyphs are centred on the same visible height.

Run from the project directory (needs Python with Pillow) after changing the
roles or fonts:

    python3 tools/make_fonts.py

It writes the font resources into package.json and their sizes and spacing
into src/c/font_styles.h.
"""

import json
import os

from PIL import ImageFont

STYLES = [
    # Resource prefix, label, regular file, bold file.
    ('SERIF', 'Serif', 'IBMPlexSerif-Regular.ttf', 'IBMPlexSerif-SemiBold.ttf'),
    ('ROUNDED', 'Rounded', 'VarelaRound-Regular.ttf', 'VarelaRound-Regular.ttf'),
    ('MONO', 'Mono', 'DMMono-Regular.ttf', 'DMMono-Medium.ttf'),
]

PLATFORMS = {
    'large': ['emery', 'gabbro'],
    'small': ['aplite', 'basalt', 'chalk', 'diorite', 'flint'],
}

# Widths in pixels of the samples below in Pebble's own fonts, measured on the
# watch: digits, capitals, lower case. Leco has digits only.
SAMPLES = ['0123456789', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz']
SYSTEM_WIDTHS = {
    'GOTHIC_09': (40, 146, 122),
    'GOTHIC_14': (60, 172, 148),
    'GOTHIC_14_BOLD': (60, 204, 176),
    'GOTHIC_18': (70, 202, 168),
    'GOTHIC_18_BOLD': (80, 228, 194),
    'GOTHIC_24': (90, 213, 188),
    'GOTHIC_24_BOLD': (100, 246, 216),
    'GOTHIC_28_BOLD': (110, 304, 268),
    'LECO_20_BOLD_NUMBERS': (127,),
    'LECO_26_BOLD_NUMBERS_AM_PM': (157,),
    'LECO_32_BOLD_NUMBERS': (196,),
    'LECO_42_NUMBERS': (255,),
}

# How much wider than Pebble's font the text may be. Pebble's Gothic is very
# narrow, so other fonts at the same width would be much smaller; letting them
# run a little wider keeps them readable. A role can set its own
# "width_allowance" where space is tight.
WIDTH_ALLOWANCE = {'numbers': 1.15, 'info': 1.3}

FONT_DIR = 'resources/fonts'
HEADER = 'src/c/font_styles.h'


def cap_height(font, numbers):
    # Height above the baseline of a capital, or a digit for number roles.
    return -font.getbbox('0' if numbers else 'H', anchor='ls')[1]


def text_width(font, numbers):
    samples = SAMPLES[:1] if numbers else SAMPLES
    return sum(font.getlength(sample) for sample in samples)


def usual_font(spec, numbers):
    """The visible height the layout expects and the usual font's text width."""
    if 'like_file' in spec:
        font = ImageFont.truetype(os.path.join(FONT_DIR, spec['like_file']), spec['like_size'])
        return cap_height(font, numbers), text_width(font, numbers)
    reference = SYSTEM_WIDTHS[spec['like']]
    return spec['height'], reference[0] if numbers else sum(reference)


def choose_size(path, numbers, height, usual_width, allowance):
    """The largest size that fits the height and width, and its top spacing."""
    allowed = usual_width * allowance
    best = None
    for size in range(6, height * 3):
        font = ImageFont.truetype(path, size)
        cap = cap_height(font, numbers)
        if cap > height or text_width(font, numbers) > allowed:
            break
        best = (size, cap)
    size, cap = best
    # Pebble draws a glyph's top (size - cap) below the top of the text box;
    # the layouts expect the visible text to start pad pixels down and be
    # height tall, so centre the glyphs in that.
    pad = size - cap - (height - cap) // 2
    return size, pad


def main():
    with open('tools/fonts.json') as f:
        roles = json.load(f)['roles']

    entries = []
    tables = {group: [] for group in PLATFORMS}
    for role, spec in roles.items():
        numbers = spec['kind'] == 'numbers'
        for group, platforms in PLATFORMS.items():
            height, usual_width = usual_font(spec[group], numbers)
            rows = []
            for prefix, label, regular, bold in STYLES:
                file = bold if spec.get('bold') else regular
                allowance = spec.get('width_allowance', WIDTH_ALLOWANCE[spec['kind']])
                size, pad = choose_size(os.path.join(FONT_DIR, file), numbers, height,
                                        usual_width, allowance)
                name = 'FONT_%s_%s_%s_%d' % (prefix, role, group.upper(), size)
                entries.append({
                    'type': 'font',
                    'name': name,
                    'file': 'fonts/' + file,
                    'characterRegex': spec['characters'],
                    'targetPlatforms': platforms,
                })
                rows.append('  { RESOURCE_ID_%s, %d },  // %s %dpx' % (name, pad, label, size))
            tables[group].append(
                'static const CustomFont FONTS_%s[CUSTOM_FONT_COUNT] = {\n%s\n};' %
                (role, '\n'.join(rows)))

    with open('package.json') as f:
        package = json.load(f)
    prefixes = tuple('FONT_%s_' % style[0] for style in STYLES)
    media = [m for m in package['pebble']['resources']['media']
             if not m['name'].startswith(prefixes)]
    package['pebble']['resources']['media'] = media + entries
    with open('package.json', 'w') as f:
        json.dump(package, f, indent=2)
        f.write('\n')

    with open(HEADER, 'w') as f:
        f.write('''// Generated by tools/make_fonts.py from tools/fonts.json; do not edit.
//
// The bundled font styles, sized to stand in for Pebble's own fonts: for each
// piece of text, the font resource for each style and how far below the top
// of the text box its visible text starts.

#pragma once

#include <pebble.h>

// Style 0 is the face's usual font; the bundled ones follow.
enum { FONT_STYLE_PEBBLE, FONT_STYLE_SERIF, FONT_STYLE_ROUNDED, FONT_STYLE_MONO,
       FONT_STYLE_COUNT };
#define CUSTOM_FONT_COUNT (FONT_STYLE_COUNT - 1)

typedef struct {
  uint32_t resource;
  int8_t pad;
} CustomFont;

// A font in use and where its visible text starts.
typedef struct {
  GFont font;
  int8_t pad;
  bool custom;
} StyledFont;

// Loads the font for a style: Pebble's own font (the usual one, unless the
// face passes its own), or a bundled one.
static inline void styled_font_load(StyledFont *styled, uint8_t style, const char *system_key,
                                    int8_t system_pad, const CustomFont *custom) {
  if (style == FONT_STYLE_PEBBLE || style >= FONT_STYLE_COUNT) {
    *styled = (StyledFont) { fonts_get_system_font(system_key), system_pad, false };
    return;
  }
  const CustomFont *chosen = &custom[style - 1];
  *styled = (StyledFont) {
    fonts_load_custom_font(resource_get_handle(chosen->resource)), chosen->pad, true,
  };
}

static inline void styled_font_unload(StyledFont *styled) {
  if (styled->custom) {
    fonts_unload_custom_font(styled->font);
    styled->custom = false;
  }
}

#if PBL_DISPLAY_WIDTH >= 200
%s
#else
%s
#endif
''' % ('\n'.join(tables['large']), '\n'.join(tables['small'])))


if __name__ == '__main__':
    main()
