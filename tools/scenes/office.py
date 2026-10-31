"""ENDLESS OFFICE: empty, yellowed rooms that go on too long - damp-looking carpet, faded
wallpaper, a drop ceiling of fluorescent panels (one of them failing), openings into more of
the same, nobody there. Light passes: base (nothing: the panels are the only light), near, mid,
far (the banks of panels by distance) and failing (the one that flickers with the hum).
"""

import common as c

H = 2.6
WALLS = [   # (x0, y0, x1, y1): partitions, 0.15 m thick, floor to ceiling
    (-4.15, -2.5, -4.0, 10.0), (-4.15, 12.2, -4.0, 34.0),           # left, with an opening at y 10-12
    (4.0, -2.5, 4.15, 6.0), (4.0, 8.6, 4.15, 34.0),                 # right, an opening at y 6-8.6
    (-4.0, 8.0, -1.1, 8.15), (1.3, 8.0, 4.0, 8.15),                 # first cross wall, doorway ahead
    (-4.0, 17.0, 0.4, 17.15), (2.3, 17.0, 4.0, 17.15),              # second, the doorway shifted right
    (-1.8, 11.0, -1.65, 14.5),                                       # a free-standing partition
    (-4.0, 26.0, -0.6, 26.15), (0.9, 26.0, 4.0, 26.15),             # third
    (-4.0, 33.5, 4.0, 33.65),                                        # the far end (for now)
    (4.15, 2.0, 10.0, 2.15), (4.15, 13.0, 10.0, 13.15), (10.0, 2.0, 10.15, 13.15),   # a side room on the right
    (-10.0, 9.0, -4.15, 9.15), (-10.0, 20.0, -4.15, 20.15), (-10.15, 9.0, -10.0, 20.15),   # and one on the left
    (-2.5, -2.65, 2.5, -2.5),                                        # behind the camera
]


def build():
    scene = c.reset()
    c.world_colour((0.0, 0.0, 0.0), 0.0)
    carpet = c.matte("carpet", (0.46, 0.38, 0.22), roughness=0.95, noise_scale=0.35, noise_amount=0.18, stains=0.12, specular=0.25, bump=0.35)
    paper = wallpaper()
    tile = c.tiles("ceiling tiles", (0.72, 0.68, 0.55), (0.42, 0.4, 0.33), size=0.6, grout_width=0.012, roughness=0.9, variation=0.06, glossy=0.2, bump=0.5)
    c.box("floor", (-10.2, -2.7, -0.1), (10.2, 34.0, 0), carpet)
    c.box("ceiling", (-10.2, -2.7, H), (10.2, 34.0, H + 0.1), tile)
    for i, (x0, y0, x1, y1) in enumerate(WALLS):
        c.box(f"wall {i}", (x0, y0, 0), (x1, y1, H), paper)
    # The panels: every 2.4 m along, 2.4 m across, 1.2 x 0.6 m, slightly warm white.
    colour = (1.0, 0.95, 0.8)
    failing = (-0.6, 2.4)   # near the camera, where its flicker is seen
    for gx in (-8.4, -6.0, -3.0, -0.6, 1.8, 6.0, 8.4):
        for gy in [y * 2.4 for y in range(0, 14)]:
            if abs(gx) > 4.2 and not (2.0 < gy < 13.0 if gx > 0 else 9.0 < gy < 20.0):
                continue   # side rooms only where they exist
            if (gx, gy) == failing:
                group = "failing"
            else:
                group = "near" if gy < 7.0 else "mid" if gy < 17.0 else "far"
            c.ceiling_panel(f"panel {gx} {gy}", group, (gx, gy), (0.6, 1.2), H, 11.0, colour, glow=3.5)
    c.camera((0.35, -1.7, 1.6), (0.35, 30.0, 1.6), lens=24.0, shift_y=-0.04)
    return scene, None, None


def wallpaper():
    """Faded yellow wallpaper: faint vertical stripes, a little uneven, darker near the floor."""
    import bpy
    mat = bpy.data.materials.new("wallpaper")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    coords = c.planar_coords(nt)
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.wave_type = 'BANDS'
    wave.bands_direction = 'X'
    wave.inputs["Scale"].default_value = 18.0
    wave.inputs["Distortion"].default_value = 0.4
    nt.links.new(coords, wave.inputs["Vector"])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 0.9
    noise.inputs["Detail"].default_value = 6.0
    nt.links.new(coords, noise.inputs["Vector"])
    stripes = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(wave.outputs["Fac"], stripes.inputs["Value"])
    stripes.inputs["To Min"].default_value = 0.94
    stripes.inputs["To Max"].default_value = 1.0
    blotch = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(noise.outputs["Fac"], blotch.inputs["Value"])
    blotch.inputs["From Min"].default_value = 0.4
    blotch.inputs["From Max"].default_value = 0.7
    blotch.inputs["To Min"].default_value = 1.0
    blotch.inputs["To Max"].default_value = 0.82
    mul = nt.nodes.new("ShaderNodeMath")
    mul.operation = 'MULTIPLY'
    nt.links.new(stripes.outputs["Result"], mul.inputs[0])
    nt.links.new(blotch.outputs["Result"], mul.inputs[1])
    colour = nt.nodes.new("ShaderNodeVectorMath")
    colour.operation = 'SCALE'
    colour.inputs[0].default_value = (0.78, 0.68, 0.36)
    nt.links.new(mul.outputs[0], colour.inputs["Scale"])
    nt.links.new(colour.outputs[0], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.75
    c.set_input(bsdf, ["Specular IOR Level", "Specular"], 0.35)
    return mat


PASSES = [
    {"name": "near", "group": "near", "kind": "tube", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "tube", "default": 1.0},
    {"name": "far", "group": "far", "kind": "tube", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
]

# After Hours: the far bank dimmed; Dream State: the near panels soft, the distance glowing;
# Infinite Cubicles: every panel the same, the far ones as bright as the near.
CHARACTERS = {
    "near": [1.0, 0.75, 1.0],
    "mid": [1.0, 1.1, 1.0],
    "far": [0.55, 1.3, 1.25],
}
