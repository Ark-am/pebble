#!/usr/bin/env python3
"""Draws GPS Speedometer's icons: a dial with its needle towards the fast end.

Writes the launcher icon (resources/images/menu_icon.png and its black and
white copy), the store icons (store/icon-small-80.png and icon-large-144.png)
and the large icon (gps-speedometer-icon.png).

Run from the project folder, after changing anything here:

    python3 tools/make_icons.py
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent

# The flat colours of the other apps' icons.
SLATE = (37, 44, 51, 255)
TEAL = (0, 190, 180, 255)
AMBER = (247, 178, 35, 255)
INK = (0, 0, 0, 255)

# The dial runs three quarters of the way round, open at the bottom. Angles
# are Pillow's: degrees clockwise from three o'clock.
DIAL_START = 135
DIAL_END = 405
FAST_FROM = 315     # the last third of the dial is amber
NEEDLE_AT = 318


def on_circle(centre, radius, degrees):
    angle = math.radians(degrees)
    return (centre[0] + radius * math.cos(angle), centre[1] + radius * math.sin(angle))


def dot(draw, at, radius, fill):
    draw.ellipse([at[0] - radius, at[1] - radius, at[0] + radius, at[1] + radius], fill=fill)


def large_icon(size):
    """The icon at any size, drawn 4x larger and scaled down for smooth edges."""
    s = size * 4
    image = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    centre = (s / 2, s * 0.54)
    radius = s * 0.40
    band = s * 0.11
    middle = radius - band / 2
    box = [centre[0] - radius, centre[1] - radius, centre[0] + radius, centre[1] + radius]

    draw.arc(box, DIAL_START, FAST_FROM, fill=TEAL, width=round(band))
    draw.arc(box, FAST_FROM, DIAL_END, fill=AMBER, width=round(band))
    # Round ends.
    dot(draw, on_circle(centre, middle, DIAL_START), band / 2, TEAL)
    dot(draw, on_circle(centre, middle, DIAL_END), band / 2, AMBER)

    # The needle: a long thin triangle from the hub.
    tip = on_circle(centre, radius - band - s * 0.035, NEEDLE_AT)
    left = on_circle(centre, s * 0.05, NEEDLE_AT - 90)
    right = on_circle(centre, s * 0.05, NEEDLE_AT + 90)
    tail = on_circle(centre, s * 0.07, NEEDLE_AT + 180)
    draw.polygon([tip, left, tail, right], fill=SLATE)
    dot(draw, centre, s * 0.075, SLATE)
    dot(draw, centre, s * 0.03, AMBER)
    return image.resize((size, size), Image.LANCZOS)


def menu_icons():
    """25 x 25, drawn without smoothing so it stays crisp. The launcher shows
    its icons in greys, so this one is black, like the other apps' launcher
    icons."""
    colour = Image.new("RGBA", (25, 25), (0, 0, 0, 0))
    bw = Image.new("1", (25, 25), 1)
    centre = (12, 13)
    for image, ink in ((colour, INK), (bw, 0)):
        draw = ImageDraw.Draw(image)
        draw.arc([1, 2, 23, 24], DIAL_START, DIAL_END, fill=ink, width=3)
        tip = on_circle(centre, 6.5, NEEDLE_AT)
        draw.line([centre, tip], fill=ink, width=2)
        draw.rectangle([centre[0] - 1, centre[1] - 1, centre[0] + 1, centre[1] + 1], fill=ink)
    images = ROOT / "resources" / "images"
    images.mkdir(parents=True, exist_ok=True)
    colour.save(images / "menu_icon.png")
    bw.save(images / "menu_icon~bw.png")


def main():
    menu_icons()
    (ROOT / "store").mkdir(exist_ok=True)
    large_icon(1254).save(ROOT / "gps-speedometer-icon.png")
    large_icon(144).save(ROOT / "store" / "icon-large-144.png")
    large_icon(80).save(ROOT / "store" / "icon-small-80.png")


if __name__ == "__main__":
    main()
