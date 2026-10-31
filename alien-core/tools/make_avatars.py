#!/usr/bin/env python3
"""Draws the twelve avatars and writes them for the watch.

Each avatar is a list of shapes on a 100 x 100 grid: filled polygons, circles
and thick lines, each in the glow colour, a dimmer shade of it, or cut out in
the background colour. Curved outlines are worked out here, so the watch only
draws polygons, circles and lines. Each shape can also carry a motion, which
the watch plays to bring the avatar to life (see Motion in src/c/main.c).

Run from the project directory (needs Python with Pillow):

    python3 tools/make_avatars.py [preview.png]

It writes src/c/avatars.h, and a preview sheet of all twelve to the path
given (avatars-preview.png by default): each avatar large, then at about the
size it shows on a small watch.
"""

import math
import sys

from PIL import Image, ImageDraw

# What the watch reads: each shape starts with a byte of its motion (high four
# bits), colour (next two) and type (low two). A zero byte ends an avatar.
POLY, CIRCLE, LINE = 1, 2, 3
GLOW, DIM, CUT = 0, 1, 2

# Motions; they match the Motion enum in src/c/main.c.
STILL, WAVE, FLAP, BLINK, SWING, SQUASH, STEP_A, STEP_B, FLICK = range(9)

# The most points a shape can have; the watch keeps a buffer this long.
MAX_POINTS = 48


def P(points, colour=GLOW, motion=STILL, pivot=None):
    return (POLY, colour, motion, pivot, points)


def C(x, y, r, colour=GLOW, motion=STILL, pivot=None):
    return (CIRCLE, colour, motion, pivot, (x, y, r))


def L(width, points, colour=GLOW, motion=STILL, pivot=None):
    return (LINE, colour, motion, pivot, (width, points))


def ellipse(cx, cy, rx, ry, start=0, end=360, steps=18):
    """Points round an ellipse, or an arc of one, in degrees clockwise from
    the right."""
    points = []
    for i in range(steps + 1):
        a = math.radians(start + (end - start) * i / steps)
        points.append((cx + rx * math.cos(a), cy + ry * math.sin(a)))
    if end - start >= 360:
        points.pop()
    return points


def catmull(p0, p1, p2, p3, t):
    def axis(a, b, c, d):
        return 0.5 * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t * t
                      + (3 * b - a - 3 * c + d) * t * t * t)
    return axis(p0[0], p1[0], p2[0], p3[0]), axis(p0[1], p1[1], p2[1], p3[1])


def blob(points, steps=3):
    """A smooth closed outline through the points."""
    out = []
    n = len(points)
    for i in range(n):
        for s in range(steps):
            out.append(catmull(points[i - 1], points[i], points[(i + 1) % n],
                               points[(i + 2) % n], s / steps))
    return out


def curve(points, steps=4):
    """A smooth open line through the points."""
    ends = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(1, len(ends) - 2):
        for s in range(steps):
            out.append(catmull(ends[i - 1], ends[i], ends[i + 1], ends[i + 2], s / steps))
    return out + [points[-1]]


def flip(points):
    """Points mirrored left to right across the middle."""
    return [(100 - x, y) for x, y in points]


def pair(make):
    """A shape and its mirror image, from a function of a point mapper."""
    return make(lambda points: points) + make(flip)


def wave(x0, y0, x1, y1, amplitude, waves, steps=10):
    """A wavy line from one point to another."""
    points = []
    length = math.hypot(x1 - x0, y1 - y0)
    nx, ny = -(y1 - y0) / length, (x1 - x0) / length
    for i in range(steps + 1):
        t = i / steps
        s = amplitude * math.sin(t * waves * 2 * math.pi)
        points.append((x0 + (x1 - x0) * t + nx * s, y0 + (y1 - y0) * t + ny * s))
    return points


def eye(x, y, r, pupil):
    """An eye that blinks: cut out, with a pupil in the glow."""
    return [C(x, y, r, CUT, BLINK), C(x + pupil * 0.2, y + pupil * 0.2, pupil, GLOW, BLINK)]


def octavo():
    shapes = []
    for m in (lambda p: p, flip):
        shapes += [
            L(6, m(curve([(44, 58), (40, 72), (42, 84), (50, 90)])), DIM, WAVE),
            L(7, m(curve([(36, 58), (26, 72), (24, 86), (32, 94)])), DIM, WAVE),
            L(8, m(curve([(28, 52), (14, 60), (6, 74), (10, 86), (20, 84)])), GLOW, WAVE),
        ]
    shapes += [
        P(blob([(50, 6), (72, 12), (84, 32), (76, 52), (60, 62), (40, 62), (24, 52), (16, 32),
                (28, 12)])),
        C(68, 20, 4, DIM), C(76, 34, 3, DIM), C(58, 12, 3, DIM), C(30, 22, 3, DIM),
    ]
    shapes += eye(37, 40, 8, 4) + eye(63, 40, 8, 4)
    shapes.append(L(2, curve([(44, 53), (50, 56), (56, 53)]), CUT))
    return shapes


def sporelet():
    shapes = [C(40, 91, 6, GLOW, STEP_A), C(60, 91, 6, GLOW, STEP_B)]
    for m, pivot in ((lambda p: p, (33, 66)), (flip, (67, 66))):
        shapes.append(L(5, m(curve([(33, 66), (22, 70), (18, 80)])), GLOW, SWING, pivot))
    shapes += [
        P(blob([(38, 50), (62, 50), (70, 70), (64, 88), (36, 88), (30, 70)])),
        P(ellipse(50, 48, 34, 8), CUT),
        P(ellipse(50, 46, 32, 7), DIM),
        P(blob([(50, 6), (74, 12), (92, 30), (94, 44), (74, 46), (50, 47), (26, 46), (6, 44),
                (8, 30), (26, 12)]), GLOW, SQUASH),
        C(30, 28, 7, CUT, SQUASH), C(56, 18, 8, CUT, SQUASH), C(76, 32, 5, CUT, SQUASH),
        C(48, 36, 3, CUT, SQUASH),
        C(30, 28, 3, DIM, SQUASH), C(56, 18, 4, DIM, SQUASH),
    ]
    shapes += eye(42, 64, 5, 2) + eye(58, 64, 5, 2)
    shapes.append(L(2, curve([(45, 75), (50, 78), (55, 75)]), CUT))
    return shapes


def lumoth():
    shapes = []
    for m in (lambda p: p, flip):
        shapes += [
            P(m(blob([(46, 42), (34, 20), (16, 10), (4, 20), (6, 38), (22, 50), (44, 52)])),
              GLOW, FLAP),
            P(m(blob([(42, 44), (30, 28), (18, 22), (12, 30), (18, 42), (38, 48)])), DIM, FLAP),
            P(m(ellipse(22, 33, 7, 7)), CUT, FLAP),
            P(m(ellipse(22, 33, 3, 3, steps=8)), GLOW, FLAP),
            P(m(blob([(44, 56), (24, 56), (12, 68), (18, 84), (32, 90), (44, 72)])), GLOW, FLAP),
            P(m(blob([(42, 60), (26, 62), (20, 70), (24, 80), (32, 82), (40, 70)])), DIM, FLAP),
            P(m(ellipse(29, 72, 4, 4, steps=8)), CUT, FLAP),
        ]
        shapes.append(L(2, m(curve([(47, 26), (42, 14), (30, 6)])), GLOW, SWING, m([(47, 26)])[0]))
        shapes.append(L(2, m(curve([(38, 11), (34, 14)])), GLOW, SWING, m([(47, 26)])[0]))
    shapes += [
        P(ellipse(50, 56, 9, 27), CUT),
        P(ellipse(50, 56, 6.5, 24)),
        L(2, [(44, 50), (56, 50)], CUT), L(2, [(44, 60), (56, 60)], CUT),
        L(2, [(45, 70), (55, 70)], CUT),
        C(50, 30, 7),
    ]
    shapes += [C(47, 29, 2, CUT, BLINK), C(53, 29, 2, CUT, BLINK)]
    return shapes


def hornback():
    shapes = []
    for m, a, b in ((lambda p: p, STEP_A, STEP_B), (flip, STEP_B, STEP_A)):
        shapes += [
            L(4, m(curve([(36, 42), (20, 36), (12, 22)])), GLOW, a),
            L(4, m(curve([(30, 60), (14, 62), (6, 54)])), GLOW, b),
            L(4, m(curve([(34, 78), (20, 86), (14, 96)])), GLOW, a),
        ]
    shapes += [
        P(blob([(50, 42), (70, 46), (78, 64), (70, 86), (50, 96), (30, 86), (22, 64), (30, 46)])),
        P(ellipse(37, 66, 6, 19), DIM), P(ellipse(63, 66, 6, 19), DIM),
        L(2, [(50, 44), (50, 96)], CUT),
        P(blob([(32, 42), (36, 30), (50, 25), (64, 30), (68, 42), (50, 46)])),
        L(2, curve([(30, 43), (50, 48), (70, 43)]), CUT),
        P([(44, 28), (42, 14), (34, 3), (44, 8), (50, 15), (56, 8), (66, 3), (58, 14), (56, 28)]),
        C(40, 34, 3, CUT, BLINK), C(60, 34, 3, CUT, BLINK),
        L(3, [(50, 30), (50, 40)], DIM),
    ]
    return shapes


def wyrmling():
    shapes = []
    for m in (lambda p: p, flip):
        shapes.append(P(m(blob([(36, 56), (22, 34), (6, 30), (10, 44), (2, 52), (12, 60),
                                (10, 70), (32, 70)], 2)), DIM, FLAP))
    tail = (66, 80)
    shapes += [
        L(8, curve([(62, 82), (80, 88), (92, 76), (86, 62)]), GLOW, SWING, tail),
        P([(86, 66), (84, 52), (94, 58)], GLOW, SWING, tail),
        C(38, 91, 7, GLOW, STEP_A), C(62, 91, 7, GLOW, STEP_B),
        P(blob([(50, 48), (66, 58), (70, 78), (58, 92), (42, 92), (30, 78), (34, 58)])),
        P(ellipse(50, 74, 12, 15), DIM),
        L(2, [(41, 70), (59, 70)], CUT), L(2, [(40, 78), (60, 78)], CUT),
    ]
    for m in (lambda p: p, flip):
        shapes += [P(m([(34, 18), (24, 1), (44, 11)])), P(m([(22, 32), (6, 24), (20, 42)]))]
    shapes += [
        P(blob([(50, 10), (70, 15), (80, 32), (72, 48), (50, 54), (28, 48), (20, 32), (30, 15)])),
        P(ellipse(50, 44, 15, 8), DIM),
        C(44, 43, 1.5, CUT), C(56, 43, 1.5, CUT),
    ]
    return shapes + eye(36, 30, 7, 3) + eye(64, 30, 7, 3)


def driftjelly():
    shapes = []
    for x, a in ((24, 3), (36, -3), (64, 3), (76, -3)):
        shapes.append(L(2, wave(x, 46, x + (x - 50) * 0.2, 94, a, 1.5), DIM, WAVE))
    shapes += [L(5, wave(44, 46, 42, 90, 4, 2, 12), GLOW, WAVE),
               L(5, wave(56, 46, 58, 90, -4, 2, 12), GLOW, WAVE)]
    rim = []
    for i in range(8):
        x = 88 - i * 76 / 8
        rim += [(x, 46), (x - 4.75, 52)]
    shapes += [
        P(ellipse(50, 42, 38, 36, 180, 360, 16) + rim + [(12, 46)], GLOW, SQUASH),
        P(ellipse(50, 42, 27, 26, 180, 360, 12), DIM, SQUASH),
        P(ellipse(50, 40, 16, 14, 180, 360, 8), GLOW, SQUASH),
        C(41, 44, 3, CUT, SQUASH), C(59, 44, 3, CUT, SQUASH),
        L(2, curve([(46, 48), (50, 50), (54, 48)]), CUT, SQUASH),
    ]
    return shapes


def hootan():
    shapes = [L(5, curve([(4, 90), (50, 88), (96, 92)]), DIM)]
    for m in (lambda p: p, flip):
        shapes.append(P(m([(26, 24), (16, 2), (42, 14)])))
    shapes.append(P(blob([(50, 10), (72, 16), (82, 42), (76, 68), (62, 86), (38, 86), (24, 68),
                          (18, 42), (28, 16)])))
    for m, pivot in ((lambda p: p, (22, 38)), (flip, (78, 38))):
        shapes.append(P(m(blob([(22, 38), (12, 54), (16, 74), (30, 82), (32, 56)], 2)),
                        DIM, SWING, pivot))
    for x, y in ((42, 64), (58, 64), (50, 70), (42, 76), (58, 76)):
        shapes.append(L(2, [(x - 4, y - 2), (x, y + 2), (x + 4, y - 2)], CUT))
    shapes += [C(36, 38, 14, DIM), C(64, 38, 14, DIM)]
    shapes += eye(36, 38, 10, 5) + eye(64, 38, 10, 5)
    shapes += [P([(44, 46), (56, 46), (50, 58)], CUT),
               P([(46, 47), (54, 47), (50, 55)]),
               L(4, [(40, 86), (38, 92)]), L(4, [(60, 86), (62, 92)])]
    return shapes


def coilfang():
    shapes = [
        P(ellipse(50, 88, 38, 10), DIM),
        P(ellipse(50, 80, 33, 10), CUT),
        P(ellipse(50, 78, 31, 9)),
        P(ellipse(50, 70, 25, 9), CUT),
        P(ellipse(50, 68, 23, 8), DIM),
        L(11, curve([(50, 66), (46, 58), (50, 50)]), GLOW, WAVE),
        P(blob([(50, 6), (68, 12), (82, 30), (74, 48), (58, 60), (50, 62), (42, 60), (26, 48),
                (18, 30), (32, 12)]), GLOW, SQUASH),
        P(blob([(50, 30), (64, 28), (70, 36), (60, 48), (50, 56), (40, 48), (30, 36), (36, 28)]),
          DIM, SQUASH),
        P(ellipse(50, 20, 16, 13), CUT, SQUASH),
        P(ellipse(50, 20, 13, 11), GLOW, SQUASH),
        C(44, 18, 3, CUT, SQUASH), C(56, 18, 3, CUT, SQUASH),
        L(2, [(50, 30), (50, 40)], GLOW, FLICK),
        L(2, [(50, 40), (46, 45)], GLOW, FLICK), L(2, [(50, 40), (54, 45)], GLOW, FLICK),
    ]
    return shapes


def stingtail():
    pivot = (50, 76)
    tail = curve([(50, 76), (44, 92), (66, 96), (86, 80), (90, 56), (80, 38)])
    shapes = [
        L(8, tail, GLOW, SWING, pivot),
        L(2, tail, DIM, SWING, pivot),
        P([(82, 42), (72, 26), (88, 32)], GLOW, SWING, pivot),
    ]
    for m, a, b in ((lambda p: p, STEP_A, STEP_B), (flip, STEP_B, STEP_A)):
        shapes += [L(3, m(curve([(40, 48), (24, 48), (16, 58)])), GLOW, a),
                   L(3, m(curve([(38, 58), (22, 62), (16, 72)])), GLOW, b),
                   L(3, m(curve([(42, 68), (28, 76), (24, 86)])), GLOW, a)]
    for m in (lambda p: p, flip):
        arm = m([(42, 40)])[0]
        shapes += [
            L(6, m(curve([(42, 40), (30, 36), (22, 26)])), GLOW, SWING, arm),
            P(m(blob([(24, 30), (10, 26), (4, 12), (12, 2), (18, 14), (26, 6), (32, 16), (30, 26)],
                     2)), GLOW, SWING, arm),
        ]
    shapes += [
        P(blob([(50, 28), (63, 36), (65, 58), (58, 76), (50, 80), (42, 76), (35, 58), (37, 36)])),
        P(ellipse(50, 56, 7, 18), DIM),
        L(2, curve([(38, 48), (50, 51), (62, 48)]), CUT),
        L(2, curve([(38, 58), (50, 61), (62, 58)]), CUT),
        L(2, curve([(41, 68), (50, 71), (59, 68)]), CUT),
        C(45, 34, 2, CUT, BLINK), C(55, 34, 2, CUT, BLINK),
    ]
    return shapes


def oculon():
    shapes = [L(4, ellipse(50, 54, 47, 13, 180, 360, 14), DIM)]
    for m in (lambda p: p, flip):
        shapes.append(P(m(blob([(24, 40), (10, 22), (2, 30), (6, 44), (2, 54), (20, 58)], 2)),
                        DIM, FLAP))
    shapes += [
        P([(38, 22), (50, 2), (62, 22)]),
        C(50, 50, 33),
        P(ellipse(50, 50, 31, 31, 25, 155, 10) + ellipse(50, 42, 34, 34, 150, 30, 10), DIM),
        C(50, 44, 19, CUT, BLINK), C(50, 44, 13, DIM, BLINK), C(50, 44, 7, CUT, BLINK),
        C(46, 40, 2.5, GLOW, BLINK),
        L(8, ellipse(50, 54, 47, 13, 20, 160, 12), CUT),
        L(4, ellipse(50, 54, 47, 13, 0, 180, 14)),
    ]
    return shapes


def skyray():
    shapes = [L(3, curve([(50, 70), (54, 84), (48, 96)]), GLOW, WAVE)]
    for m in (lambda p: p, flip):
        shapes += [
            P(m([(46, 30), (38, 12), (44, 14), (50, 28)])),
            P(m(blob([(50, 24), (34, 28), (16, 40), (0, 60), (18, 62), (36, 68), (50, 78)], 2)),
              GLOW, FLAP),
            P(m(blob([(38, 38), (22, 46), (12, 56), (24, 57), (38, 54)], 2)), DIM, FLAP),
        ]
    shapes += [
        P(blob([(50, 34), (60, 42), (58, 60), (50, 72), (42, 60), (40, 42)]), DIM),
        C(50, 50, 3, CUT), C(50, 60, 2, CUT),
        L(2, [(40, 58), (44, 60)], CUT), L(2, [(60, 58), (56, 60)], CUT),
        L(2, [(39, 63), (44, 65)], CUT), L(2, [(61, 63), (56, 65)], CUT),
        C(41, 33, 3, CUT, BLINK), C(59, 33, 3, CUT, BLINK),
    ]
    return shapes


def pincera():
    shapes = []
    for m, a, b in ((lambda p: p, STEP_A, STEP_B), (flip, STEP_B, STEP_A)):
        shapes += [L(4, m(curve([(28, 66), (12, 72), (6, 86)])), GLOW, a),
                   L(4, m(curve([(34, 72), (20, 82), (16, 96)])), GLOW, b),
                   L(4, m(curve([(42, 76), (32, 86), (30, 96)])), GLOW, a)]
    big, small = (26, 54), (74, 54)
    shapes += [
        L(7, curve([(26, 54), (14, 46), (14, 36)]), GLOW, SWING, big),
        P(blob([(16, 38), (2, 28), (0, 12), (8, 2), (14, 14), (24, 4), (30, 18), (26, 32)], 2),
          GLOW, SWING, big),
        L(5, curve([(74, 54), (84, 48), (84, 40)]), GLOW, SWING, small),
        P(blob([(84, 42), (74, 34), (74, 22), (80, 16), (84, 24), (90, 18), (94, 30), (92, 38)],
               2), GLOW, SWING, small),
        L(3, curve([(42, 46), (40, 32)])), L(3, curve([(58, 46), (60, 32)])),
        C(40, 29, 6), C(60, 29, 6),
        C(40, 29, 3, CUT, BLINK), C(60, 29, 3, CUT, BLINK),
        P(blob([(50, 42), (72, 44), (88, 56), (78, 72), (60, 80), (40, 80), (22, 72), (12, 56),
                (28, 44)])),
        P(ellipse(50, 70, 28, 8), DIM),
        C(36, 54, 3, DIM), C(50, 50, 3, DIM), C(64, 54, 3, DIM),
        L(2, curve([(42, 62), (50, 66), (58, 62)]), CUT),
    ]
    return shapes


AVATARS = [
    ('OCTAVO', octavo), ('SPORELET', sporelet), ('LUMOTH', lumoth), ('HORNBACK', hornback),
    ('WYRMLING', wyrmling), ('DRIFTJELLY', driftjelly), ('HOOTAN', hootan),
    ('COILFANG', coilfang), ('STINGTAIL', stingtail), ('OCULON', oculon),
    ('SKYRAY', skyray), ('PINCERA', pincera),
]


def grid(value):
    """A coordinate as the watch stores it: half units, from 0 to 200."""
    return max(0, min(200, int(round(value * 2))))


def encode(shapes):
    """The shapes as bytes for the watch."""
    data = []
    for kind, colour, motion, pivot, body in shapes:
        data.append(motion << 4 | colour << 2 | kind)
        if motion == SWING:
            data += [grid(pivot[0]), grid(pivot[1])]
        if kind == POLY:
            assert len(body) <= MAX_POINTS, len(body)
            data.append(len(body))
            data += [grid(v) for point in body for v in point]
        elif kind == CIRCLE:
            data += [grid(v) for v in body]
        else:
            width, points = body
            assert len(points) <= MAX_POINTS, len(points)
            data += [grid(width), len(points)]
            data += [grid(v) for point in points for v in point]
    return data + [0]


def render(shapes, size, glow=(0, 255, 0), dim=(0, 170, 0), background=(0, 0, 0)):
    """A preview of an avatar as the watch draws it, at rest."""
    scale = size * 4 / 100
    image = Image.new('RGB', (size * 4, size * 4), background)
    draw = ImageDraw.Draw(image)
    colours = {GLOW: glow, DIM: dim, CUT: background}
    for kind, colour, _, _, body in shapes:
        fill = colours[colour]
        if kind == POLY:
            draw.polygon([(x * scale, y * scale) for x, y in body], fill=fill)
        elif kind == CIRCLE:
            x, y, r = (v * scale for v in body)
            draw.ellipse((x - r, y - r, x + r, y + r), fill=fill)
        else:
            width = max(4, round(body[0] * scale))
            points = [(x * scale, y * scale) for x, y in body[1]]
            draw.line(points, fill=fill, width=width, joint='curve')
            for x, y in points:
                draw.ellipse((x - width / 2, y - width / 2, x + width / 2, y + width / 2),
                             fill=fill)
    return image.resize((size, size), Image.LANCZOS)


def main():
    lines = [
        '// Generated by tools/make_avatars.py; do not edit.',
        '//',
        '// The avatars, each a run of shapes on a grid of half units ending in 0',
        '// (see draw_avatar in main.c).',
        '',
        '#pragma once',
        '',
        '#include <pebble.h>',
        '',
        '#define AVATAR_COUNT %d' % len(AVATARS),
        '#define AVATAR_MAX_POINTS %d' % MAX_POINTS,
        '',
    ]
    total = 0
    for name, make in AVATARS:
        data = encode(make())
        total += len(data)
        lines.append('static const uint8_t AVATAR_%s[] = { %s };'
                     % (name, ', '.join(str(v) for v in data)))
    lines += ['', 'static const char *const AVATAR_NAMES[AVATAR_COUNT] = {']
    lines += ['  "%s",' % name for name, _ in AVATARS]
    lines += ['};', '', 'static const uint8_t *const AVATAR_SHAPES[AVATAR_COUNT] = {']
    lines += ['  AVATAR_%s,' % name for name, _ in AVATARS]
    lines += ['};', '']
    with open('src/c/avatars.h', 'w') as f:
        f.write('\n'.join(lines))
    print('%d bytes of avatars' % total)

    cell, small = 150, 56
    sheet = Image.new('RGB', (cell * 6, (cell + small + 24) * 2), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    for i, (name, make) in enumerate(AVATARS):
        x, y = (i % 6) * cell, (i // 6) * (cell + small + 24)
        sheet.paste(render(make(), cell - 10), (x + 5, y + 5))
        sheet.paste(render(make(), small), (x + 5, y + cell))
        draw.text((x + small + 12, y + cell + 20), name, fill=(200, 200, 200))
    sheet.save(sys.argv[1] if len(sys.argv) > 1 else 'avatars-preview.png')


if __name__ == '__main__':
    main()
