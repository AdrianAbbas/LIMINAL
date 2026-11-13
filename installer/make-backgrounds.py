#!/usr/bin/env python3
"""Draw installer/background.png and installer/background-dark.png: the Windward Audio wordmark,
bottom-left, with a short rule in the product's accent colour (installer/product.json) above it.
Transparent elsewhere, 1240 x 836 px at 144 dpi (620 x 418 pt, sharp on Retina displays).
Needs Pillow and the Inter font (SIL OFL); run it when the accent colour changes, commit the PNGs.

    python3 installer/make-backgrounds.py [path to Inter-SemiBold.otf]
"""
import json
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
FONT = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf"
SCALE = 2
W, H = 620 * SCALE, 418 * SCALE
accent = json.loads((HERE / "product.json").read_text())["accent"]
ar, ag, ab = (int(accent[i:i + 2], 16) for i in (1, 3, 5))

def draw(path, ink, accent_rgb):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, 13 * SCALE)
    x, base = 22 * SCALE, H - 22 * SCALE
    bbox = d.textbbox((0, 0), "Windward Audio", font=font, anchor="ls")
    d.text((x, base), "Windward Audio", font=font, fill=ink, anchor="ls")
    top = base + bbox[1] - 7 * SCALE
    d.rectangle([x, top - 2 * SCALE, x + 18 * SCALE, top], fill=accent_rgb + (255,))
    img.save(path, dpi=(144, 144), optimize=True)

def shade(f):
    return (int(ar * f), int(ag * f), int(ab * f))

draw(HERE / "background.png", (29, 29, 31, 150), shade(0.62))      # light mode: dark grey, darker accent
draw(HERE / "background-dark.png", (245, 245, 247, 150), shade(1.0))  # dark mode: light grey, accent as is
print("wrote background.png and background-dark.png")
