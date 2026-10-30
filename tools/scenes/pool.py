"""ABANDONED INDOOR POOL: a low white grid-tiled hall with rounded pillars, still turquoise
water, warm sunlight through high windows falling in patches against shadowed corners, and an
opening at the far end into the next pool room. Light passes: base (sky), sun, skylights,
beyond (the next room), caustics (a looping animated pattern of light off the water).
"""

import math

from mathutils import Vector

import common as c

WATER_Z = -0.12
CAUSTIC_FRAMES = 24


def build():
    scene = c.reset()
    c.world_colour((0.62, 0.74, 0.92), 0.8)

    deck = c.tiles("deck tiles", (0.86, 0.87, 0.85), (0.52, 0.54, 0.52), size=0.15, roughness=0.22)
    wall = c.tiles("wall tiles", (0.88, 0.89, 0.87), (0.56, 0.58, 0.56), size=0.15, roughness=0.14)
    basin = c.tiles("basin tiles", (0.62, 0.86, 0.86), (0.38, 0.55, 0.56), size=0.15, roughness=0.12)
    mosaic = c.tiles("pillar mosaic", (0.9, 0.9, 0.88), (0.6, 0.62, 0.6), size=0.05, grout_width=0.003, roughness=0.15)
    ceiling = c.matte("ceiling plaster", (0.82, 0.82, 0.8), roughness=0.85, noise_scale=0.6, noise_amount=0.08, bump=0.02)
    steel = c.matte("steel", (0.75, 0.76, 0.78), roughness=0.25, specular=0.8)
    pool_water = c.water("water")

    # Hall: x -7..7, y -3..24, floor 0, ceiling 3.0. The basin: x -4..4, y 2..30 (it carries on
    # through the far opening into the next room).
    H = 3.0
    c.box("deck left", (-7, -3, -0.3), (-4, 24, 0), deck)
    c.box("deck right", (4, -3, -0.3), (7, 24, 0), deck)
    c.box("deck near", (-4, -3, -0.3), (4, 2, 0), deck)
    c.box("basin floor", (-4, 2, -1.9), (4, 34, -1.6), basin)
    c.box("basin side left", (-4.3, 2, -1.9), (-4.0, 34, -0.02), basin)
    c.box("basin side right", (4.0, 2, -1.9), (4.3, 34, -0.02), basin)
    c.box("basin near wall", (-4.3, 1.7, -1.9), (4.3, 2.0, -0.02), basin)
    c.box("coping left", (-4.35, 1.65, -0.02), (-3.95, 24, 0.04), deck)
    c.box("coping right", (3.95, 1.65, -0.02), (4.35, 24, 0.04), deck)
    c.box("coping near", (-4.35, 1.65, -0.02), (4.35, 2.05, 0.04), deck)
    c.box("water", (-4.0, 2.0, -1.6), (4.0, 34, WATER_Z), pool_water)

    # Left wall with a band of high windows (the sun comes in here).
    c.box("left wall low", (-7.3, -3, 0), (-7.0, 24, 1.9), wall)
    c.box("left wall top", (-7.3, -3, 2.65), (-7.0, 24, H), wall)
    for i, y in enumerate(range(-3, 25, 4)):
        c.box(f"window mullion {i}", (-7.3, y - 0.25, 1.9), (-7.0, y + 0.25, 2.65), wall)
    # Right wall: solid, with an arch into a dark side hall.
    c.box("right wall a", (7.0, -3, 0), (7.3, 10, H), wall)
    c.box("right wall b", (7.0, 12.4, 0), (7.3, 24, H), wall)
    c.box("right wall lintel", (7.0, 10, 2.3), (7.3, 12.4, H), wall)
    c.box("side hall floor", (7.3, 8, -0.3), (12, 15, 0), deck)
    c.box("side hall wall", (12, 8, 0), (12.3, 15, H), wall)
    c.box("side hall ceiling", (7.3, 8, H), (12.3, 15, H + 0.3), ceiling)
    c.box("side hall back", (7.3, 7.7, 0), (12.3, 8, H), wall)
    c.box("side hall front", (7.3, 15, 0), (12.3, 15.3, H), wall)
    # Near wall (behind the camera) and the far wall with a wide opening to the next room.
    c.box("near wall", (-7.3, -3.3, 0), (7.3, -3, H), wall)
    c.box("far wall left", (-7.3, 24, 0), (-4.4, 24.3, H), wall)
    c.box("far wall right", (4.4, 24, 0), (7.3, 24.3, H), wall)
    c.box("far wall lintel", (-4.4, 24, 2.25), (4.4, 24.3, H), wall)
    # The next room: the same tiles, lit from its own skylight.
    c.box("next deck left", (-7, 24.3, -0.3), (-4, 34, 0), deck)
    c.box("next deck right", (4, 24.3, -0.3), (7, 34, 0), deck)
    c.box("next left wall", (-7.3, 24.3, 0), (-7.0, 34, H), wall)
    c.box("next right wall", (7.0, 24.3, 0), (7.3, 34, H), wall)
    c.box("next far wall", (-7.3, 34, -1.9), (7.3, 34.3, H), wall)
    c.box("next ceiling", (-7.3, 24.3, H), (7.3, 34.3, H + 0.3), ceiling)

    # Ceiling with two skylights over the water (openings, lit from above).
    sky = [(-1.6, 7.0, 1.6, 9.5), (-1.6, 15.0, 1.6, 17.5)]
    c.box("ceiling a", (-7.3, -3.3, H), (7.3, 7.0, H + 0.3), ceiling)
    c.box("ceiling b", (-7.3, 9.5, H), (7.3, 15.0, H + 0.3), ceiling)
    c.box("ceiling c", (-7.3, 17.5, H), (7.3, 24.3, H + 0.3), ceiling)
    for i, (x0, y0, x1, y1) in enumerate(sky):
        c.box(f"ceiling beside sky {i} l", (-7.3, y0, H), (x0, y1, H + 0.3), ceiling)
        c.box(f"ceiling beside sky {i} r", (x1, y0, H), (7.3, y1, H + 0.3), ceiling)
        for side, (a, b) in enumerate([((x0, y0, H), (x0 + 0.05, y1, H + 0.6)), ((x1 - 0.05, y0, H), (x1, y1, H + 0.6)),
                                        ((x0, y0, H), (x1, y0 + 0.05, H + 0.6)), ((x0, y1 - 0.05, H), (x1, y1, H + 0.6))]):
            c.box(f"skylight well {i}{side}", a, b, ceiling)
        c.area_light(f"skylight {i}", "skylights", ((x0 + x1) / 2, (y0 + y1) / 2, H + 0.55), (x1 - x0, y1 - y0), 420.0, (0.86, 0.92, 1.0))

    # Rounded pillars in two rows along the deck.
    for side in (-5.5, 5.5):
        for y in (1.0, 6.0, 11.0, 16.0, 21.0):
            c.cylinder(f"pillar {side} {y}", (side, y, 0), 0.36, H, mosaic)
    # A steel ladder into the water on the right.
    for dx in (0.0, 0.5):
        c.cylinder(f"ladder rail {dx}", (3.55 + dx, 9.0, -1.5), 0.025, 2.4, steel, segments=16)
    for k in range(4):
        c.box(f"ladder rung {k}", (3.55, 8.98, -1.3 + 0.3 * k), (4.05, 9.02, -1.27 + 0.3 * k), steel)

    # Warm low sun through the high windows: patches on the deck, the pillars and the right wall.
    sun_dir = Vector((0.78, 0.28, -0.56)).normalized()
    c.sun_light("sun", "sun", sun_dir.to_track_quat('-Z', 'Y').to_euler(), 9.0, (1.0, 0.8, 0.56), angle=0.8)
    # The next room and the side hall, lit from somewhere we cannot see.
    c.area_light("next room skylight", "beyond", (0, 29, H - 0.05), (6, 6), 900.0, (1.0, 0.95, 0.86))
    c.area_light("side hall lamp", "beyond", (9.6, 11.5, H - 0.05), (2.5, 3.0), 160.0, (1.0, 0.9, 0.75))

    # Caustics: light thrown off the water onto the ceiling and walls, and through it onto the
    # basin floor, shaped by an animated pattern (two shadow-only "gobo" planes).
    gobo = caustic_gobo()
    up = c.spot_light("caustic up", "caustics", (0, 12, WATER_Z + 0.02), (math.pi, 0, 0), 2200.0, (0.8, 1.0, 0.97), math.radians(165), radius=0.01, blend=0.6)
    down = c.spot_light("caustic down", "caustics", (-1.0, 12, H - 0.1), (0.25, 0.0, 0.0), 2600.0, (0.85, 1.0, 0.95), math.radians(110), radius=0.01, blend=0.5)
    g1 = c.plane("gobo up", (0, 12, WATER_Z + 0.3), (16, 40), gobo)
    g2 = c.plane("gobo down", (-1.0, 12, H - 0.35), (12, 30), gobo)
    for g in (g1, g2):
        g.visible_camera = False
        g.visible_diffuse = False
        g.visible_glossy = False
        g.visible_transmission = False
        c.in_group("caustics", g)

    c.camera((0.9, -2.2, 1.6), (0.4, 24.0, 1.6), lens=24.0, shift_y=-0.09)
    return scene, gobo, (up, down)


def caustic_gobo():
    """Opaque except along the edges of a 4D Voronoi pattern: the bright net of light that still
    water throws. A circle through the pattern's third and fourth dimensions makes it loop."""
    import bpy
    mat = bpy.data.materials.new("caustic gobo")
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    coord = nt.nodes.new("ShaderNodeTexCoord")
    phase = nt.nodes.new("ShaderNodeValue")
    phase.name = "phase"
    phase.outputs[0].default_value = 0.0
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    layers = []
    for scale, radius, weight in ((26.0, 0.32, 1.0), (47.0, 0.45, 0.55)):
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(coord.outputs["Object"], sep.inputs[0])
        sin_n = nt.nodes.new("ShaderNodeMath")
        sin_n.operation = 'SINE'
        nt.links.new(phase.outputs[0], sin_n.inputs[0])
        cos_n = nt.nodes.new("ShaderNodeMath")
        cos_n.operation = 'COSINE'
        nt.links.new(phase.outputs[0], cos_n.inputs[0])
        zs = nt.nodes.new("ShaderNodeMath")
        zs.operation = 'MULTIPLY'
        zs.inputs[1].default_value = radius
        nt.links.new(sin_n.outputs[0], zs.inputs[0])
        ws = nt.nodes.new("ShaderNodeMath")
        ws.operation = 'MULTIPLY'
        ws.inputs[1].default_value = radius
        nt.links.new(cos_n.outputs[0], ws.inputs[0])
        xs = nt.nodes.new("ShaderNodeMath")
        xs.operation = 'MULTIPLY'
        xs.inputs[1].default_value = scale
        nt.links.new(sep.outputs[0], xs.inputs[0])
        ys = nt.nodes.new("ShaderNodeMath")
        ys.operation = 'MULTIPLY'
        ys.inputs[1].default_value = scale * 0.8
        nt.links.new(sep.outputs[1], ys.inputs[0])
        comb = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(xs.outputs[0], comb.inputs[0])
        nt.links.new(ys.outputs[0], comb.inputs[1])
        nt.links.new(zs.outputs[0], comb.inputs[2])
        vor = nt.nodes.new("ShaderNodeTexVoronoi")
        vor.voronoi_dimensions = '4D'
        vor.feature = 'DISTANCE_TO_EDGE'
        vor.inputs["Scale"].default_value = 1.0
        vor.inputs["Randomness"].default_value = 0.9
        nt.links.new(comb.outputs[0], vor.inputs["Vector"])
        nt.links.new(ws.outputs[0], vor.inputs["W"])
        ramp = nt.nodes.new("ShaderNodeMapRange")
        ramp.interpolation_type = 'SMOOTHSTEP'
        ramp.inputs["From Min"].default_value = 0.0
        ramp.inputs["From Max"].default_value = 0.09
        ramp.inputs["To Min"].default_value = weight
        ramp.inputs["To Max"].default_value = 0.0
        nt.links.new(vor.outputs["Distance"], ramp.inputs["Value"])
        layers.append(ramp.outputs["Result"])
    add = nt.nodes.new("ShaderNodeMath")
    add.operation = 'ADD'
    add.use_clamp = True
    nt.links.new(layers[0], add.inputs[0])
    nt.links.new(layers[1], add.inputs[1])
    transparent = nt.nodes.new("ShaderNodeBsdfTransparent")
    black = nt.nodes.new("ShaderNodeBsdfDiffuse")
    black.inputs["Color"].default_value = (0, 0, 0, 1)
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(add.outputs[0], mix.inputs["Fac"])
    nt.links.new(black.outputs[0], mix.inputs[1])
    nt.links.new(transparent.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return mat


BASE_DEFAULT = 0.75   # the sky a little lower: warm patches against shadowed corners

PASSES = [
    {"name": "sun", "group": "sun", "kind": "sun", "default": 1.3},
    {"name": "skylights", "group": "skylights", "kind": "sky", "default": 1.0},
    {"name": "beyond", "group": "beyond", "kind": "lamp", "default": 1.0},
]
