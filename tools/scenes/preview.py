"""Composite a packed scene the way the plug-in does (linear sum of passes, then the tone curve)
and save a PNG, to look at a scene while working on it.

    python3 tools/scenes/preview.py <scene> out.png [pass=intensity ...] [caustic=frame]
"""

import json
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def linear(path):
    x = np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0
    return np.where(x <= 0.04045, x / 12.92, np.power((x + 0.055) / 1.055, 2.4))


def tone(x):
    # The plug-in's curve: linear toe, soft shoulder (Source/UI/PhotoScene.cpp, toneCurve).
    y = x / (1.0 + x * 0.45)
    y = np.clip(y * 1.12, 0, 1)
    return np.where(y <= 0.0031308, 12.92 * y, 1.055 * np.power(y, 1 / 2.4) - 0.055)


def main():
    scene, out = sys.argv[1], sys.argv[2]
    folder = os.path.join(ROOT, "assets", "scenes", scene)
    m = json.load(open(os.path.join(folder, f"{scene}_manifest.json")))
    k = {p["name"]: p.get("default", 1.0) for p in m["passes"]}
    caustic = 0
    for arg in sys.argv[3:]:
        name, value = arg.split("=")
        if name == "caustic":
            caustic = int(value)
        else:
            k[name] = float(value)
    total = None
    for p in m["passes"]:
        x = linear(os.path.join(folder, f"{scene}_{p['name']}.jpg")) * k[p["name"]]
        total = x if total is None else total + x
    if "caustics" in m and m["caustics"]["frames"] and k.get("caustics", 1.0) > 0:
        c = linear(os.path.join(folder, m["caustics"]["frames"][caustic % len(m["caustics"]["frames"])]))
        c = np.asarray(Image.fromarray((c * 255).astype(np.uint8)).resize((total.shape[1], total.shape[0]), Image.BILINEAR), dtype=np.float32) / 255.0
        total = total + c * k.get("caustics", 1.0)
    total = total / m.get("headroom", 1.0)
    Image.fromarray((tone(total) * 255 + 0.5).astype(np.uint8)).save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
