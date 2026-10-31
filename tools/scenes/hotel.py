"""EMPTY HOTEL: a long corridor of patterned carpet between faded wallpaper, warm wall sconces
between numbered doors, a dim window at the far end. One door - 404 - stands ajar, light
spilling out of the room across the carpet.
Light passes: base (the end window's night-blue light), near, mid, far (the sconces by
distance), ceiling (flush ceiling lights, off at night), failing (a sconce that flickers with
the hum), room404 (the light from the open room).
"""

import math

import common as c

H = 2.55
W = 1.0     # half width (a narrow hotel corridor)
END = 32.0


def build():
    scene = c.reset()
    c.world_colour((0.35, 0.42, 0.6), 0.6)
    carpet = carpet_material()
    paper = wallpaper()
    ceiling = c.matte("ceiling", (0.78, 0.74, 0.66), roughness=0.85, noise_scale=0.6, noise_amount=0.06)
    wood = c.matte("door veneer", (0.33, 0.2, 0.12), roughness=0.35, noise_scale=10.0, noise_amount=0.2, specular=0.5)
    trim = c.matte("trim", (0.88, 0.84, 0.74), roughness=0.4, specular=0.5)
    brass = c.metal("brass", (0.78, 0.6, 0.32), roughness=0.3)
    shade = c.emitter("sconce shade", (1.0, 0.72, 0.42), 1.1)
    curtain = c.matte("curtain", (0.4, 0.3, 0.22), roughness=0.95, noise_scale=12, noise_amount=0.2)
    room = c.matte("room wall", (0.7, 0.58, 0.44), roughness=0.85)
    bedding = c.matte("bedding", (0.85, 0.82, 0.76), roughness=0.9)
    lamp_glow = c.emitter("room lamp", (1.0, 0.75, 0.45), 4.0)

    c.box("floor", (-W - 6, -3, -0.1), (W + 6, END + 0.3, 0), carpet)
    c.box("ceiling", (-W - 0.3, -3, H), (W + 0.3, END + 0.3, H + 0.1), ceiling)
    c.box("near wall", (-W - 0.3, -3.2, 0), (W + 0.3, -3, H), paper)
    doors = [(-1, 2.5), (1, 5.0), (-1, 8.5), (1, 11.0), (-1, 14.5), (1, 17.0), (-1, 20.5), (1, 23.0), (-1, 26.5), (1, 29.0)]
    numbers = {d: 400 + 2 * i + (1 if d[0] > 0 else 0) for i, d in enumerate(doors)}
    ajar = (1, 11.0)
    numbers[ajar] = 404
    for side in (-1, 1):
        xw = side * W
        cuts = sorted(y for s, y in doors if s == side)
        y = -3.0
        for d in cuts + [END + 1]:
            y1 = min(d - 0.5, END)
            if y1 > y:
                c.box(f"wall {side} {y}", (min(xw, xw + side * 0.15), y, 0), (max(xw, xw + side * 0.15), y1, H), paper)
            y = d + 0.5
        for d in cuts:
            c.box(f"over door {side} {d}", (min(xw, xw + side * 0.15), d - 0.5, 2.15), (max(xw, xw + side * 0.15), d + 0.5, H), paper)
        # Chair rail and skirting run the length of the corridor.
        c.box(f"skirting {side}", (min(xw, xw - side * 0.02), -3, 0), (max(xw, xw - side * 0.02), END, 0.14), trim)
        c.box(f"crown {side}", (min(xw, xw - side * 0.04), -3, H - 0.1), (max(xw, xw - side * 0.04), END, H), trim)
    for (side, d) in doors:
        xw = side * W
        c.box(f"casing {side} {d}", (min(xw, xw - side * 0.025), d - 0.56, 0), (max(xw, xw - side * 0.025), d - 0.48, 2.2), trim)
        c.box(f"casing b {side} {d}", (min(xw, xw - side * 0.025), d + 0.48, 0), (max(xw, xw - side * 0.025), d + 0.56, 2.2), trim)
        c.box(f"casing top {side} {d}", (min(xw, xw - side * 0.025), d - 0.56, 2.15), (max(xw, xw - side * 0.025), d + 0.56, 2.23), trim)
        # Brass number plate beside the door.
        c.box(f"number {side} {d}", (min(xw, xw - side * 0.012), d + 0.62, 1.48), (max(xw, xw - side * 0.012), d + 0.76, 1.6), brass)
        if (side, d) == ajar:
            # Ajar: the leaf swung ~25 degrees into the room, light spilling through the gap.
            hinge = (xw + side * 0.1, d + 0.46)   # hinged on the far side: the gap faces the camera
            a = math.radians(35)
            lx, ly = math.sin(a) * 0.92, math.cos(a) * 0.92
            t = 0.045
            q0 = hinge                                         # hinge edge
            q1 = (hinge[0] + side * lx, hinge[1] - ly)         # free edge, swung into the room
            c.hexa("door 404", [(q0[0], q0[1], 0.01), (q0[0] + side * t, q0[1], 0.01), (q1[0] + side * t, q1[1], 0.01), (q1[0], q1[1], 0.01),
                                (q0[0], q0[1], 2.13), (q0[0] + side * t, q0[1], 2.13), (q1[0] + side * t, q1[1], 2.13), (q1[0], q1[1], 2.13)], wood)
            # The room: a dim warm box with a bedside lamp, a bed, a curtained window.
            rx0, rx1 = W + 0.15, W + 5.0
            c.box("room ceiling", (rx0, d - 2.0, H), (rx1, d + 2.0, H + 0.1), ceiling)
            c.box("room back", (rx1, d - 2.0, 0), (rx1 + 0.15, d + 2.0, H), curtain)
            c.box("room side a", (rx0, d - 2.15, 0), (rx1, d - 2.0, H), room)
            c.box("room side b", (rx0, d + 2.0, 0), (rx1, d + 2.15, H), room)
            c.box("bed", (rx0 + 1.6, d - 0.2, 0), (rx1 - 0.3, d + 1.7, 0.55), bedding)
            c.box("headboard", (rx1 - 0.3, d - 0.3, 0), (rx1 - 0.15, d + 1.8, 1.1), wood)
            c.box("nightstand", (rx1 - 0.7, d - 0.9, 0), (rx1 - 0.2, d - 0.4, 0.55), wood)
            lamp = c.cylinder("bedside lamp", (rx1 - 0.45, d - 0.65, 0.6), 0.15, 0.3, lamp_glow, segments=20)
            lamp.visible_shadow = False
            c.in_group("room404", lamp)
            c.point_light("bedside light", "room404", (rx1 - 0.45, d - 0.65, 0.78), 1100.0, (1.0, 0.72, 0.42), radius=0.15)
            c.area_light("room fill", "room404", (rx0 + 2.0, d, H - 0.05), (2.0, 2.0), 700.0, (1.0, 0.75, 0.5))
            continue
        c.box(f"door {side} {d}", (min(xw + side * 0.1, xw + side * 0.145), d - 0.47, 0.01), (max(xw + side * 0.1, xw + side * 0.145), d + 0.47, 2.13), wood)
        c.box(f"handle {side} {d}", (min(xw + side * 0.1, xw + side * 0.07), d + 0.3, 1.0), (max(xw + side * 0.1, xw + side * 0.07), d + 0.42, 1.04), brass)
        # A thin line of light under some doors? No: every room is dark (nobody is here).
    # Sconces between the doors, both walls, alternating.
    k = 0
    for side in (-1, 1):
        for y in [1.0 + 3.0 * i for i in range(11)]:
            if any(s == side and abs(y - d) < 0.9 for s, d in doors):
                y += 0.95
            xw = side * W
            group = "failing" if (side, k) == (-1, 4) else "near" if y < 8 else "mid" if y < 19 else "far"
            c.box(f"sconce plate {side} {y}", (min(xw, xw - side * 0.02), y - 0.06, 1.62), (max(xw, xw - side * 0.02), y + 0.06, 1.82), brass)
            s = c.cylinder(f"sconce shade {side} {y}", (xw - side * 0.12, y, 1.66), 0.09, 0.17, shade, segments=20)
            s.visible_shadow = False
            c.in_group(group, s)
            c.point_light(f"sconce {side} {y}", group, (xw - side * 0.12, y, 1.74), 9.0, (1.0, 0.7, 0.4), radius=0.08)
            if side < 0:
                k += 1
    # Flush ceiling lights down the middle (off at night: the "ceiling" pass).
    disc = c.emitter("ceiling light", (1.0, 0.86, 0.64), 2.0)
    for i in range(8):
        y = 0.5 + 4.0 * i
        d = c.cylinder(f"flush {i}", (0, y, H - 0.06), 0.16, 0.06, disc, segments=24)
        d.visible_shadow = False
        c.in_group("ceiling", d)
        c.point_light(f"flush light {i}", "ceiling", (0, y, H - 0.15), 22.0, (1.0, 0.84, 0.6), radius=0.12)
    # The far end: a tall window, curtains half drawn, night-blue light (the base pass).
    c.box("end wall l", (-W - 0.3, END, 0), (-0.55, END + 0.3, H), paper)
    c.box("end wall r", (0.55, END, 0), (W + 0.3, END + 0.3, H), paper)
    c.box("end sill", (-0.55, END, 0), (0.55, END + 0.3, 0.7), paper)
    c.box("end head", (-0.55, END, 2.2), (0.55, END + 0.3, H), paper)
    night = c.emitter("night window", (0.42, 0.52, 0.8), 1.4)
    pane = c.plane("end window", (0, END + 0.2, 1.45), (1.1, 1.5), night, rotation=(math.pi / 2, 0, 0))
    c.in_group("base", pane)
    for x in (-0.48, 0.48):
        c.box(f"curtain {x}", (x - 0.16, END - 0.12, 0.05), (x + 0.16, END - 0.02, 2.3), curtain)
    c.area_light("window light", "base", (0, END - 0.1, 1.45), (1.0, 1.4), 12.0, (0.45, 0.55, 0.85), rotation=(-math.pi / 2, 0, 0))
    c.camera((0.12, -2.4, 1.6), (0.12, END, 1.55), lens=26.0, shift_y=-0.03)
    return scene, None, None


def carpet_material():
    """Patterned hotel carpet: a repeating diamond/medallion motif in faded wine and gold, fibrous."""
    import bpy
    mat = bpy.data.materials.new("carpet")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    coords = c.planar_coords(nt)
    # Diamond lattice: rotate the coordinates 45 degrees, take a checker, then a wave for rings.
    rot = nt.nodes.new("ShaderNodeVectorRotate")
    rot.rotation_type = 'Z_AXIS'
    rot.inputs["Angle"].default_value = math.radians(45)
    nt.links.new(coords, rot.inputs["Vector"])
    checker = nt.nodes.new("ShaderNodeTexChecker")
    checker.inputs["Scale"].default_value = 3.0
    checker.inputs["Color1"].default_value = (0.32, 0.07, 0.07, 1)
    checker.inputs["Color2"].default_value = (0.26, 0.05, 0.06, 1)
    nt.links.new(rot.outputs[0], checker.inputs["Vector"])
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.feature = 'F1'
    vor.distance = 'MANHATTAN'
    vor.inputs["Scale"].default_value = 2.1
    vor.inputs["Randomness"].default_value = 0.0
    nt.links.new(coords, vor.inputs["Vector"])
    rings = nt.nodes.new("ShaderNodeMath")
    rings.operation = 'PINGPONG'
    rings.inputs[1].default_value = 0.08
    nt.links.new(vor.outputs["Distance"], rings.inputs[0])
    gold = nt.nodes.new("ShaderNodeMapRange")
    gold.interpolation_type = 'SMOOTHSTEP'
    nt.links.new(rings.outputs[0], gold.inputs["Value"])
    gold.inputs["From Min"].default_value = 0.055
    gold.inputs["From Max"].default_value = 0.075
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    nt.links.new(gold.outputs["Result"], mix.inputs["Factor"])
    nt.links.new(checker.outputs["Color"], mix.inputs[6])
    mix.inputs[7].default_value = (0.55, 0.4, 0.17, 1)
    # Wear: lighter, flattened down the middle where people walked.
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 0.8
    nt.links.new(coords, noise.inputs["Vector"])
    wear = nt.nodes.new("ShaderNodeMix")
    wear.data_type = 'RGBA'
    wear.blend_type = 'MULTIPLY'
    wear.inputs["Factor"].default_value = 0.35
    nt.links.new(mix.outputs[2], wear.inputs[6])
    nt.links.new(noise.outputs["Color"], wear.inputs[7])
    nt.links.new(wear.outputs[2], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    c.set_input(bsdf, ["Specular IOR Level", "Specular"], 0.2)
    c.set_input(bsdf, ["Sheen Weight", "Sheen"], 0.6)
    fibre = nt.nodes.new("ShaderNodeTexNoise")
    fibre.inputs["Scale"].default_value = 900.0
    nt.links.new(coords, fibre.inputs["Vector"])
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = 0.5
    b.inputs["Distance"].default_value = 0.002
    nt.links.new(fibre.outputs["Fac"], b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def wallpaper():
    """Faded damask-ish wallpaper: warm beige with a soft vertical motif, a little stained."""
    import bpy
    mat = bpy.data.materials.new("wallpaper")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    coords = c.planar_coords(nt)
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.wave_type = 'BANDS'
    wave.bands_direction = 'X'
    wave.inputs["Scale"].default_value = 9.0
    wave.inputs["Distortion"].default_value = 3.0
    wave.inputs["Detail"].default_value = 1.5
    nt.links.new(coords, wave.inputs["Vector"])
    motif = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(wave.outputs["Fac"], motif.inputs["Value"])
    motif.inputs["To Min"].default_value = 0.9
    motif.inputs["To Max"].default_value = 1.0
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 0.7
    noise.inputs["Detail"].default_value = 5.0
    nt.links.new(coords, noise.inputs["Vector"])
    stain = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(noise.outputs["Fac"], stain.inputs["Value"])
    stain.inputs["From Min"].default_value = 0.45
    stain.inputs["From Max"].default_value = 0.75
    stain.inputs["To Min"].default_value = 1.0
    stain.inputs["To Max"].default_value = 0.8
    mul = nt.nodes.new("ShaderNodeMath")
    mul.operation = 'MULTIPLY'
    nt.links.new(motif.outputs["Result"], mul.inputs[0])
    nt.links.new(stain.outputs["Result"], mul.inputs[1])
    col = nt.nodes.new("ShaderNodeVectorMath")
    col.operation = 'SCALE'
    col.inputs[0].default_value = (0.7, 0.58, 0.4)
    nt.links.new(mul.outputs[0], col.inputs["Scale"])
    nt.links.new(col.outputs[0], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    c.set_input(bsdf, ["Specular IOR Level", "Specular"], 0.3)
    return mat


BASE_DEFAULT = 1.0

PASSES = [
    {"name": "near", "group": "near", "kind": "lamp", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "lamp", "default": 1.0},
    {"name": "far", "group": "far", "kind": "lamp", "default": 1.0},
    {"name": "ceiling", "group": "ceiling", "kind": "lamp", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
    {"name": "room404", "group": "room404", "kind": "lamp", "default": 1.0},
]

# Late Night: warm sconces only, the ceiling lights off. Faded Luxury: the ceiling lights on as
# well, the corridor golden. Room 404: the sconces low, door 404 ajar with its light spilling out.
CHARACTERS = {
    "base": [1.0, 0.6, 0.7],
    "near": [1.0, 1.1, 0.25],
    "mid": [1.0, 1.1, 0.2],
    "far": [1.0, 1.1, 0.15],
    "ceiling": [0.0, 1.0, 0.0],
    "failing": [1.0, 1.0, 0.6],
    "room404": [0.0, 0.0, 1.0],
}
