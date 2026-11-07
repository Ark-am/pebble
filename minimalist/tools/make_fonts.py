#!/usr/bin/env python3
"""Sizes the four font styles for every piece of text on the face.

Each piece of text is a "role" with the height its capitals (or digits) take
in the layout. For each style this picks the largest size whose visible
height is no taller than that and whose text still fits the width it has,
then works out where Pebble puts the glyphs: Pebble draws a glyph's baseline
`size` pixels below the top of its text box, so the visible text starts
(size - height above the baseline) pixels down. That rule reproduces the
offsets measured in the emulator for Antonio.

The Condensed style keeps the face's original look on the Pebble 2 Duo: Antonio
at fixed sizes for the time, the top row and the numerals, and Pebble's own
Gothic for the small text. The Pebble Time 2 and Pebble Round 2 get every size
TEXT_SCALE larger (see GROUPS).

Run from the project directory (needs Python with Pillow) after changing the
styles or the roles:

    python3 tools/make_fonts.py

It rewrites the font resources in package.json and src/c/font_styles.h.
"""

import json
import math
import os

from PIL import Image, ImageDraw, ImageFont

FONT_DIR = 'resources/fonts'
HEADER = 'src/c/font_styles.h'

# Name, label, regular file, bold file. Bold is for the small text.
STYLES = [
    ('CONDENSED', 'Condensed (Antonio)', 'Antonio-Regular.ttf', None),
    ('SERIF', 'Serif (IBM Plex Serif)', 'IBMPlexSerif-Regular.ttf', 'IBMPlexSerif-SemiBold.ttf'),
    ('ROUNDED', 'Rounded (Varela Round)', 'VarelaRound-Regular.ttf', 'VarelaRound-Regular.ttf'),
    ('MONO', 'Mono (DM Mono)', 'DMMono-Regular.ttf', 'DMMono-Medium.ttf'),
]

DIGITS = '0123456789'
CAPITALS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ASCII = ''.join(chr(c) for c in range(32, 127))
DEGREE = '\u00b0'  # for the weather

# Role: characters, bold, sample whose height counts, target height.
ROLES = {
    'TIME': (DIGITS + ':', False, '0', 58),       # the digital time
    'TIME_12H': (DIGITS + ':', False, '0', 58),   # the same, leaving room for PM
    'ROW': (DIGITS + CAPITALS + ' ', False, 'H', 19),  # the digital top row
    'NUMERALS': (DIGITS, False, '0', 19),          # 12, 3, 6 and 9 on the dial
    'DATE': (ASCII, True, 'H', 11),               # the analog date and the name
    'SMALL': (ASCII + DEGREE, True, 'H', 9),       # PM and the widgets
}
ROLE_ORDER = ['TIME', 'TIME_12H', 'ROW', 'NUMERALS', 'DATE', 'SMALL']

# Two sets of sizes: the Pebble 2 Duo's, also used by the other watches with
# screens up to 180 px wide, and one TEXT_SCALE larger for the Pebble Time 2
# and Pebble Round 2 (TEXT_SCALE in src/c/main.c).
#
# The Condensed style keeps the Pebble 2 Duo's fixed sizes of Antonio, and
# Pebble's own Gothic for the small text; the offsets and heights of Gothic
# were measured in the emulator, as was the width of PM in it.
#
# Widths from src/c/main.c: the time may be TIME_MAX_WIDTH wide, and in 12-hour
# time so may the time with PM_GAP and PM to its right; the top row spans the
# same width and must leave at least ROW_MIN_GAP between the day and the date.
GROUPS = {
    'small': {
        'platforms': ['aplite', 'basalt', 'chalk', 'diorite', 'flint'],
        'scale': 100,
        'condensed_sizes': {'TIME': 66, 'ROW': 22, 'NUMERALS': 22},
        'condensed_system': {
            'DATE': ('FONT_KEY_GOTHIC_18_BOLD', 7, 11),
            'SMALL': ('FONT_KEY_GOTHIC_14_BOLD', 5, 9),
        },
        'gothic_pm_width': 17,
    },
    'large': {
        'platforms': ['emery', 'gabbro'],
        'scale': 135,
        'condensed_sizes': {},
        'condensed_system': {
            'DATE': ('FONT_KEY_GOTHIC_24_BOLD', 10, 14),
            'SMALL': ('FONT_KEY_GOTHIC_18_BOLD', 7, 11),
        },
        'gothic_pm_width': 22,
    },
}
TIME_MAX_WIDTH = 130
PM_GAP = 3
ROW_MIN_GAP = 12
# Space each side of the colon, in 100ths of a digit's width.
COLON_SPACE = 15

DAYS = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT']
MONTHS = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC']


def advance(font, glyph):
    # Pebble keeps each glyph's advance in whole pixels, rounded down (Antonio's
    # 27.5 px digits at 66 px are drawn 27 px apart).
    return int(font.getlength(glyph))


def width(font, text):
    return sum(advance(font, c) for c in text)


def metrics(font, size, sample):
    """Where the visible text starts in Pebble's text box, and its height."""
    left, top, right, bottom = font.getbbox(sample, anchor='ls')
    return size + top, bottom - top


def digit_cell(font):
    return max(advance(font, d) for d in DIGITS)


def ink(font, glyph):
    """The left and right edges of a glyph's ink, from its origin."""
    pad = font.size
    image = Image.new('L', (font.size * 3, font.size * 3))
    ImageDraw.Draw(image).text((pad, pad), glyph, font=font, fill=255)
    box = image.getbbox()
    if box is None:
        return 0, 0
    return box[0] - pad, box[2] - pad


def colon_cell(font):
    left, right = ink(font, ':')
    return (right - left) + 2 * math.ceil(digit_cell(font) * COLON_SPACE / 100)


def time_width(font):
    return digit_cell(font) * 4 + colon_cell(font)


def row_width(font):
    day = max(width(font, d) for d in DAYS)
    date = max(width(font, '%s %d' % (m, n)) for m in MONTHS for n in range(1, 32))
    return day + date


def scaled(value, group):
    # As TEXT() in src/c/main.c.
    return value * GROUPS[group]['scale'] // 100


def choose(path, role, fits, group):
    """The largest size within the role's height for which fits(font) holds."""
    _, _, sample, target = ROLES[role]
    target = scaled(target, group)
    best = None
    for size in range(6, target * 3):
        font = ImageFont.truetype(path, size)
        top, height = metrics(font, size, sample)
        if height > target or not fits(font):
            break
        best = size
    return best


def main():
    resources = {}  # (file, size) -> [set of characters, set of platforms]
    tables = {}     # group -> style -> role -> (resource or system key, top, height)
    extras = {}     # group -> style -> colon cells of the two times
    for group in GROUPS:
        tables[group], extras[group] = generate(group, resources)
    write_package(resources)
    write_header(tables, extras)


def generate(group, resources):
    spec = GROUPS[group]
    max_width = scaled(TIME_MAX_WIDTH, group)
    pm_gap = scaled(PM_GAP, group)
    min_gap = scaled(ROW_MIN_GAP, group)
    table = {}
    extras = {}
    for name, label, regular, bold in STYLES:
        table[name] = {}
        fonts = {}
        pm = pm_width(name, bold, group)
        for role in ROLE_ORDER:
            chars, is_bold, sample, target = ROLES[role]
            if name == 'CONDENSED' and role in spec['condensed_system']:
                table[name][role] = spec['condensed_system'][role]
                continue
            file = bold if is_bold else regular
            path = os.path.join(FONT_DIR, file)
            if name == 'CONDENSED' and role in spec['condensed_sizes']:
                size = spec['condensed_sizes'][role]
            elif role == 'TIME':
                size = choose(path, role, lambda f: time_width(f) <= max_width, group)
            elif role == 'TIME_12H':
                size = choose(path, role, lambda f: time_width(f) + pm_gap + pm <= max_width,
                              group)
            elif role == 'ROW':
                budget = min(time_width(fonts['TIME']),
                             time_width(fonts['TIME_12H']) + pm_gap + pm) - min_gap
                size = choose(path, role, lambda f: row_width(f) <= budget, group)
            else:
                size = choose(path, role, lambda f: True, group)
            font = ImageFont.truetype(path, size)
            fonts[role] = font
            top, height = metrics(font, size, sample)
            entry = resources.setdefault((file, size), [set(), set()])
            entry[0].update(chars)
            entry[1].update(spec['platforms'])
            table[name][role] = (resource_name(file, size), top, height)
        extras[name] = (colon_cell(fonts['TIME']), colon_cell(fonts['TIME_12H']))
        print('%-5s %-9s time %2d px (12-hour %2d px), row %2d px, numerals %2d px' % (
            group, name, table[name]['TIME'][2], table[name]['TIME_12H'][2],
            table[name]['ROW'][2], table[name]['NUMERALS'][2]))
    return table, extras


def pm_width(style, bold, group):
    if style == 'CONDENSED':
        return GROUPS[group]['gothic_pm_width']
    path = os.path.join(FONT_DIR, bold)
    size = choose(path, 'SMALL', lambda f: True, group)
    return width(ImageFont.truetype(path, size), 'PM')


def resource_name(file, size):
    base = os.path.splitext(file)[0].replace('-', '_').upper()
    return 'FONT_%s_%d' % (base, size)


def character_regex(chars):
    out = ''
    if set(ASCII) <= chars:
        out += ' -~'
        chars = chars - set(ASCII)
    if set(DIGITS) <= chars:
        out += '0-9'
        chars = chars - set(DIGITS)
    if set(CAPITALS) <= chars:
        out += 'A-Z'
        chars = chars - set(CAPITALS)
    for c in sorted(chars):
        out += '\\' + c if c in '\\]^-' else c
    return '[%s]' % out


def write_package(resources):
    with open('package.json') as f:
        package = json.load(f)
    media = [m for m in package['pebble']['resources']['media'] if m['type'] != 'font']
    order = [p for group in GROUPS.values() for p in group['platforms']]
    for (file, size), (chars, platforms) in sorted(resources.items()):
        media.append({
            'type': 'font',
            'name': resource_name(file, size),
            'file': 'fonts/' + file,
            'characterRegex': character_regex(chars),
            'targetPlatforms': sorted(platforms, key=order.index),
        })
    package['pebble']['resources']['media'] = media
    with open('package.json', 'w') as f:
        json.dump(package, f, indent=2)
        f.write('\n')


def style_rows(table, extras):
    rows = []
    for name, label, _, _ in STYLES:
        cells = []
        for role in ROLE_ORDER:
            source, top, height = table[name][role]
            if source.startswith('FONT_KEY_'):
                cells.append('      { 0, %s, %d, %d },' % (source, top, height))
            else:
                cells.append('      { RESOURCE_ID_%s, NULL, %d, %d },' % (source, top, height))
        rows.append('  {  // %s\n    .roles = {\n%s\n    },\n    .colon_w = { %d, %d },\n  },' % (
            (label, '\n'.join(cells)) + extras[name]))
    return '\n'.join(rows)


def write_header(tables, extras):
    with open(HEADER, 'w') as f:
        f.write('''// Generated by tools/make_fonts.py; do not edit.
//
// The four font styles: for each piece of text, its font and where its
// visible text starts in Pebble's text box (top) and how tall it is (height).

#pragma once

#include <pebble.h>

enum { FONT_STYLE_CONDENSED, FONT_STYLE_SERIF, FONT_STYLE_ROUNDED, FONT_STYLE_MONO,
       FONT_STYLE_COUNT };

// The digital time, the same in 12-hour time (leaving room for PM to its
// right), the digital top row, the dial's numerals, the analog date and the
// name, and PM and the widgets.
enum { ROLE_TIME, ROLE_TIME_12H, ROLE_ROW, ROLE_NUMERALS, ROLE_DATE, ROLE_SMALL, ROLE_COUNT };

typedef struct {
  uint32_t resource;   // A bundled font, or 0 for Pebble's own...
  const char *system;  // ...named by its key.
  int8_t top;
  int8_t height;
} FontSpec;

typedef struct {
  FontSpec roles[ROLE_COUNT];
  int8_t colon_w[2];  // The width of each time's colon, with space each side.
} FontStyle;

// The Pebble Time 2 and Pebble Round 2 take the larger sizes.
#if PBL_DISPLAY_WIDTH >= 200
static const FontStyle FONT_STYLES[FONT_STYLE_COUNT] = {
%s
};
#else
static const FontStyle FONT_STYLES[FONT_STYLE_COUNT] = {
%s
};
#endif
''' % (style_rows(tables['large'], extras['large']), style_rows(tables['small'], extras['small'])))


if __name__ == '__main__':
    main()
