"""Composite a packed scene the way the plug-in does (linear sum of passes, then the tone curve)
and save a PNG, to look at a scene while working on it.

    python3 tools/scenes/preview.py <scene> out.png [pass=intensity ...] [caustic=frame] [character=1..3]
    python3 tools/scenes/preview.py <scene> sheet.jpg --sheet     (the three Characters side by side)
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


def composite(scene, args):
    folder = os.path.join(ROOT, "assets", "scenes", scene)
    m = json.load(open(os.path.join(folder, f"{scene}_manifest.json")))
    k = {p["name"]: p.get("default", 1.0) for p in m["passes"]}
    caustic = 0
    character = None
    for arg in args:
        name, value = arg.split("=")
        if name == "caustic":
            caustic = int(value)
        elif name == "character":
            character = int(value) - 1
        else:
            k[name] = float(value)
    if character is not None:   # the Character's light, as the plug-in applies it
        for p in m["passes"]:
            k[p["name"]] *= p.get("characters", [1, 1, 1])[character]
        if "caustics" in m:
            k["caustics"] = k.get("caustics", 1.0) * m["caustics"].get("characters", [1, 1, 1])[character]
    total = None
    for p in m["passes"]:
        x = linear(os.path.join(folder, f"{scene}_{p['name']}.jpg")) * k[p["name"]]
        total = x if total is None else total + x
    if "caustics" in m and m["caustics"]["frames"] and k.get("caustics", 1.0) > 0:
        c = linear(os.path.join(folder, m["caustics"]["frames"][caustic % len(m["caustics"]["frames"])]))
        c = np.asarray(Image.fromarray((c * 255).astype(np.uint8)).resize((total.shape[1], total.shape[0]), Image.BILINEAR), dtype=np.float32) / 255.0
        total = total + c * k.get("caustics", 1.0)
    total = total / m.get("headroom", 1.0)
    return Image.fromarray((tone(total) * 255 + 0.5).astype(np.uint8))


def main():
    scene, out = sys.argv[1], sys.argv[2]
    if "--sheet" in sys.argv:
        pics = [composite(scene, [f"character={n}"]) for n in (1, 2, 3)]
        w, h = pics[0].size
        sheet = Image.new("RGB", (w * 3 // 2, h // 2))
        for i, p in enumerate(pics):
            sheet.paste(p.resize((w // 2, h // 2), Image.LANCZOS), (i * w // 2, 0))
        sheet.save(out, quality=88)
    else:
        composite(scene, sys.argv[3:]).save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
