"""Render one LIMINAL scene's light passes and pack them for the plug-in.

    python3 tools/scenes/render.py <pool|office|hospital> [--preview]

Needs Blender's Python module (`pip install bpy`, Blender 5.2 on Python 3.13; see
tools/scenes/README.md) and Pillow. Writes linear EXR passes to build/scenes/<scene>/, then
assets/scenes/<scene>/: one sRGB-encoded JPEG per pass, <scene>_<pass>.jpg (stored half a stop
down for headroom), the caustic loop if the scene has one, and <scene>_manifest.json (pass names, kinds, defaults, where
each light group sits in the picture). The plug-in decodes them to linear light, adds them
with live intensities and grades the result (Source/UI/PhotoScene.*).
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

import common  # noqa: E402

HEADROOM = 2.0 ** -0.5   # passes are stored at -0.5 EV; the plug-in multiplies back
QUALITY = 88


def load_exr(path):
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = 'Non-Color'
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return np.flipud(px.reshape(h, w, 4)[:, :, :3])


def blur(a, radius):
    """Three box blurs ~ a Gaussian (linear, so per-pass halation adds up exactly)."""
    for _ in range(3):
        for axis in (0, 1):
            c = np.cumsum(np.pad(a, [(radius + 1, radius) if i == axis else (0, 0) for i in range(3)], mode='edge'), axis=axis)
            hi = np.take(c, np.arange(2 * radius + 1, c.shape[axis]), axis=axis)
            lo = np.take(c, np.arange(0, c.shape[axis] - 2 * radius - 1), axis=axis)
            a = (hi - lo) / (2 * radius + 1)
    return a


def to_srgb(linear):
    x = np.clip(linear, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def pack(exr, jpg, halation=0.0, scale=1.0):
    x = load_exr(exr) * HEADROOM * scale
    if halation > 0:   # soft glow around bright fixtures (only where it is bright)
        bright = np.clip(x - 0.6, 0.0, None)
        x = x + halation * blur(bright, max(4, x.shape[1] // 90))
    Image.fromarray((to_srgb(x) * 255.0 + 0.5).astype(np.uint8)).save(jpg, quality=QUALITY, optimize=True, progressive=True)
    return float(x.max())


def main():
    name = sys.argv[1]
    preview = "--preview" in sys.argv
    module = __import__(name)
    built = module.build()
    scene = bpy.context.scene
    if preview:
        scene.render.resolution_percentage = 25
    work = os.path.join(ROOT, "build", "scenes", name)
    assets = os.path.join(ROOT, "assets", "scenes", name)
    os.makedirs(assets, exist_ok=True)
    samples = 16 if preview else int(os.environ.get("LIMINAL_SAMPLES", "160"))
    manifest = common.render_passes(work, module.PASSES, samples=samples, extra=getattr(module, "EXTRA", None), base_default=getattr(module, "BASE_DEFAULT", 1.0))
    # The caustic loop: rendered at half size, the pattern stepping once around its circle.
    loop = getattr(module, "CAUSTIC_FRAMES", 0)
    if loop and os.environ.get("LIMINAL_ONLY_PASS") in (None, "caustics"):
        _, gobo, _ = built
        common._set_group("caustics", 0.0, 1.0)
        # The pattern is soft: a quarter of the size and few samples are plenty (and ~20 s a frame).
        scene.cycles.samples = 16 if preview else int(os.environ.get("LIMINAL_CAUSTIC_SAMPLES", "24"))
        scene.render.resolution_percentage = 12 if preview else int(os.environ.get("LIMINAL_CAUSTIC_PERCENT", "25"))
        for f in range(loop):
            gobo.node_tree.nodes["phase"].outputs[0].default_value = 2 * math.pi * f / loop
            scene.render.filepath = os.path.join(work, f"caustic_{f:02d}.exr")
            bpy.ops.render.render(write_still=True)
            print("rendered caustic frame", f, flush=True)
    # Pack for the plug-in.
    for p in manifest["passes"]:
        exr = os.path.join(work, p["name"] + ".exr")
        if os.path.exists(exr):
            p["peak"] = round(pack(exr, os.path.join(assets, f"{name}_{p['name']}.jpg"), halation=0.35 if p["kind"] in ("tube", "failing", "sign", "lamp") else 0.15), 3)
    if loop:
        frames = []
        for f in range(loop):
            exr = os.path.join(work, f"caustic_{f:02d}.exr")
            if os.path.exists(exr):
                pack(exr, os.path.join(assets, f"{name}_caustic_{f:02d}.jpg"))
                frames.append(f"{name}_caustic_{f:02d}.jpg")
        manifest["caustics"] = {"frames": frames, "fps": 5, "default": 0.6}   # slow and faint: light, not motion
    manifest["headroom"] = HEADROOM
    manifest["scene"] = name
    # A small picture for the plug-in's film strip of places: every pass at its default level.
    total = None
    for p in manifest["passes"]:
        jpg = os.path.join(assets, f"{name}_{p['name']}.jpg")
        if os.path.exists(jpg):
            x = np.asarray(Image.open(jpg).convert("RGB"), dtype=np.float32) / 255.0
            x = np.where(x <= 0.04045, x / 12.92, np.power((x + 0.055) / 1.055, 2.4)) * p.get("default", 1.0)
            total = x if total is None else total + x
    if total is not None:
        y = np.clip(total / HEADROOM / (1.0 + total / HEADROOM * 0.45) * 1.12, 0, 1)
        Image.fromarray((to_srgb(y) * 255 + 0.5).astype(np.uint8)).resize((320, 240), Image.LANCZOS).save(
            os.path.join(assets, f"{name}_thumb.jpg"), quality=85)
        manifest["thumbnail"] = f"{name}_thumb.jpg"
    with open(os.path.join(assets, f"{name}_manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print("packed", assets)


if __name__ == "__main__":
    main()
