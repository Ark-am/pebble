#!/usr/bin/env python3
"""Draws Qibla-Tasbih's images: the Arabic phrases and the icons.

Pebble's fonts have no Arabic, so each phrase is drawn here with IBM Plex
Sans Arabic Bold (in tools/fonts, under the SIL Open Font License) and
shipped as a bitmap. Each phrase gets the largest size that fits its box:

  small  for the 144 x 168 watches and the Pebble Time Round
  large  for the Pebble Time 2 and Pebble Round 2

Colour watches get four shades of grey, so the letters stay smooth. The
black-and-white watches get a 1-bit copy (the ~bw file), which Pebble picks
for them automatically. Both are white on black, which is what the app draws
them on.

The script also draws the menu icon, the store icons and the large icon, and
writes the phrase resources into package.json. It needs Python with Pillow
built with libraqm, which shapes the Arabic letters. Run it from anywhere:

    python3 tools/make_images.py
"""

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "tools" / "fonts" / "IBMPlexSansArabic-Bold.ttf"
PHRASES_DIR = ROOT / "resources" / "images" / "phrases"

# Resource name, file stem, Arabic text, and optionally the text on two lines
# for when one line would be too small to read. The names match src/c/main.c.
PHRASES = [
    ("IMAGE_SUBHANALLAH", "subhanallah", "سبحان الله"),
    ("IMAGE_ALHAMDULILLAH", "alhamdulillah", "الحمد لله"),
    ("IMAGE_ALLAHU_AKBAR", "allahu_akbar", "الله أكبر"),
    ("IMAGE_LA_ILAHA_ILLALLAH", "la_ilaha_illallah", "لا إله إلا الله"),
    ("IMAGE_ASTAGHFIRULLAH", "astaghfirullah", "أستغفر الله"),
    ("IMAGE_SUBHANALLAHI_WA_BIHAMDIHI", "subhanallahi_wa_bihamdihi", "سبحان الله وبحمده"),
    ("IMAGE_SALAWAT", "salawat", "اللهم صل على محمد"),
    ("IMAGE_LA_HAWLA", "la_hawla", "لا حول ولا قوة إلا بالله"),
    ("IMAGE_TAHLIL", "tahlil", "لا إله إلا الله وحده لا شريك له",
     ["لا إله إلا الله وحده", "لا شريك له"]),
]

# Box (width, height) and the largest font size tried, per size.
SIZES = {
    "small": {"box": (124, 28), "largest": 21,
              "platforms": ["aplite", "basalt", "chalk", "diorite", "flint"]},
    "large": {"box": (172, 40), "largest": 30,
              "platforms": ["emery", "gabbro"]},
}
SMALLEST = 11
# A phrase with two lines given uses them below this size, and may then be a
# little taller than its box.
TWO_LINES_BELOW = 14
TWO_LINE_HEIGHT = 34

# Pebble's colour screens have four levels per channel.
GREYS = [0, 85, 170, 255]


def render(text, size):
    """Draws the text white on black, cropped to the letters."""
    font = ImageFont.truetype(str(FONT), size, layout_engine=ImageFont.Layout.RAQM)
    canvas = Image.new("L", (size * len(text), size * 3), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((canvas.width // 2, canvas.height // 2), text, font=font, fill=255,
              anchor="mm", direction="rtl", language="ar")
    return canvas.crop(canvas.getbbox())


def fitted(text, box, largest):
    for size in range(largest, SMALLEST - 1, -1):
        image = render(text, size)
        if image.width <= box[0] and image.height <= box[1]:
            return image, size
    raise SystemExit(f"{text!r} does not fit {box} even at {SMALLEST}px")


def two_lines(lines, box, largest):
    for size in range(largest, SMALLEST - 1, -1):
        parts = [render(line, size) for line in lines]
        gap = max(1, size // 6)
        width = max(p.width for p in parts)
        height = sum(p.height for p in parts) + gap * (len(parts) - 1)
        if width <= box[0] and height <= TWO_LINE_HEIGHT:
            image = Image.new("L", (width, height), 0)
            y = 0
            for part in parts:
                image.paste(part, ((width - part.width) // 2, y))
                y += part.height + gap
            return image, size
    raise SystemExit(f"{lines!r} do not fit {box} even at {SMALLEST}px")


def to_greys(image):
    """Four shades of grey in a 2-bit palette, the smallest colour bitmap."""
    def nearest(value):
        return min(range(4), key=lambda i: abs(GREYS[i] - value))
    out = Image.frombytes("P", image.size, image.point(nearest).tobytes())
    out.putpalette(sum(([g, g, g] for g in GREYS), []))
    return out


def to_one_bit(image):
    return image.point(lambda v: 255 if v >= 110 else 0).convert("1")


def make_phrases():
    PHRASES_DIR.mkdir(parents=True, exist_ok=True)
    resources = []
    for name, stem, text, *lines in PHRASES:
        for size_name, size in SIZES.items():
            image, points = fitted(text, size["box"], size["largest"])
            if lines and points < TWO_LINES_BELOW:
                image, points = two_lines(lines[0], size["box"], size["largest"])
            path = PHRASES_DIR / f"{stem}_{size_name}.png"
            to_greys(image).save(path, bits=2, optimize=True)
            if size_name == "small":
                to_one_bit(image).save(PHRASES_DIR / f"{stem}_{size_name}~bw.png")
            print(f"{path.name}: {image.width} x {image.height} at {points}px")
            resources.append({
                "type": "bitmap",
                "name": name,
                "file": f"images/phrases/{path.name}",
                "targetPlatforms": size["platforms"],
            })
    return resources


# The icons: a loop of prayer beads with a larger bead and a tassel below, in
# the flat colours of the other apps' icons.
SLATE = (37, 44, 51, 255)
TEAL = (0, 190, 180, 255)
AMBER = (247, 178, 35, 255)
INK = (0, 0, 0, 255)


def draw_beads(draw, scale, centre, radius, bead, beads, colours, gap_beads=2):
    """Beads around a circle, leaving a gap at the bottom for the tassel."""
    import math
    cx, cy = centre
    total = beads + gap_beads
    for i in range(total):
        if i < gap_beads // 2 + 1 or i > total - gap_beads // 2 - 1:
            continue
        angle = math.pi / 2 + 2 * math.pi * i / total
        x = cx + radius * math.cos(angle)
        y = cy + radius * math.sin(angle)
        colour = colours[i % len(colours)]
        draw.ellipse([x - bead, y - bead, x + bead, y + bead], fill=colour)


def large_icon(size):
    """The icon at any size, drawn 4x larger and scaled down for smooth edges."""
    s = size * 4
    image = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    centre = (s / 2, s * 0.43)
    radius = s * 0.30
    draw_beads(draw, s, centre, radius, s * 0.042, 26, [TEAL, TEAL, TEAL])
    # The larger bead where the loop closes, and the tassel below it.
    bead_y = centre[1] + radius + s * 0.04
    draw.ellipse([s / 2 - s * 0.07, bead_y - s * 0.07, s / 2 + s * 0.07, bead_y + s * 0.07],
                 fill=AMBER)
    top = bead_y + s * 0.06
    draw.polygon([(s / 2 - s * 0.035, top), (s / 2 + s * 0.035, top),
                  (s / 2 + s * 0.10, s * 0.93), (s / 2 - s * 0.10, s * 0.93)], fill=AMBER)
    draw.rectangle([s / 2 - s * 0.05, top - s * 0.005, s / 2 + s * 0.05, top + s * 0.04],
                   fill=SLATE)
    return image.resize((size, size), Image.LANCZOS)


def menu_icons():
    """25 x 25, drawn pixel by pixel so it stays crisp: ten beads as 3 x 3
    squares (a round dot this small turns into a plus sign), the larger bead
    and the tassel. The launcher shows its icons in greys, so this one is
    black, like the other apps' launcher icons, and shows on the launcher's
    highlight too."""
    import math
    colour = Image.new("RGBA", (25, 25), (0, 0, 0, 0))
    bw = Image.new("1", (25, 25), 1)
    dc, db = ImageDraw.Draw(colour), ImageDraw.Draw(bw)

    def shape(draw_colour, draw_bw, method, xy):
        getattr(dc, method)(xy, fill=draw_colour)
        getattr(db, method)(xy, fill=0)

    cx, cy, radius = 12, 10, 8.5
    for i in range(1, 11):
        angle = math.pi / 2 + 2 * math.pi * i / 11
        x = round(cx + radius * math.cos(angle))
        y = round(cy + radius * math.sin(angle))
        shape(INK, 0, "rectangle", [x - 1, y - 1, x + 1, y + 1])
    shape(INK, 0, "rectangle", [10, 17, 14, 20])
    shape(INK, 0, "polygon", [(11, 21), (13, 21), (15, 24), (9, 24)])
    colour.save(ROOT / "resources" / "images" / "menu_icon.png")
    bw.save(ROOT / "resources" / "images" / "menu_icon~bw.png")


def write_resources(phrases):
    package_path = ROOT / "package.json"
    package = json.loads(package_path.read_text())
    media = [m for m in package["pebble"]["resources"]["media"]
             if not m["file"].startswith("images/phrases/")]
    package["pebble"]["resources"]["media"] = media + phrases
    package_path.write_text(json.dumps(package, indent=2, ensure_ascii=False) + "\n")


def main():
    write_resources(make_phrases())
    menu_icons()
    large_icon(1254).save(ROOT / "qibla-tasbih-icon.png")
    large_icon(144).save(ROOT / "store" / "icon-large-144.png")
    large_icon(80).save(ROOT / "store" / "icon-small-80.png")


if __name__ == "__main__":
    main()
