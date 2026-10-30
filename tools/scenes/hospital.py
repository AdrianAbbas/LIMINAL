"""VACANT HOSPITAL: a long corridor - pale green-grey walls, glossy linoleum mirroring the ceiling
tubes, recessed fluorescent panels (one failing), handrails, closed doors, an exit sign glowing
at the far end. Light passes: base (cold daylight through the far doors' windows), near, mid,
far (the panels by distance), failing (the one that flickers with the hum) and exit (the sign).
"""

import math

import common as c

H = 2.7
W = 1.3   # half width
END = 34.0


def build():
    scene = c.reset()
    c.world_colour((0.55, 0.65, 0.75), 0.6)
    lino = linoleum()
    upper = c.matte("wall paint", (0.62, 0.68, 0.62), roughness=0.55, noise_scale=0.5, noise_amount=0.06, specular=0.45)
    lower = c.matte("wall paint low", (0.42, 0.5, 0.46), roughness=0.4, noise_scale=0.8, noise_amount=0.08, specular=0.55)
    tile = c.tiles("ceiling tiles", (0.8, 0.82, 0.8), (0.55, 0.57, 0.55), size=0.6, grout_width=0.012, roughness=0.85, variation=0.04, glossy=0.25, bump=0.4)
    rail = c.matte("handrail", (0.72, 0.74, 0.7), roughness=0.3, specular=0.7)
    door = c.matte("door", (0.58, 0.56, 0.5), roughness=0.45, noise_scale=1.2, noise_amount=0.05, specular=0.5)
    frame = c.matte("door frame", (0.5, 0.54, 0.52), roughness=0.35, specular=0.6)
    glass = c.matte("door glass", (0.03, 0.04, 0.04), roughness=0.05, specular=1.0)
    kick = c.matte("skirting", (0.25, 0.3, 0.28), roughness=0.3, specular=0.6)

    c.box("floor", (-W - 0.3, -3, -0.1), (W + 0.3, END + 0.3, 0), lino)
    c.box("ceiling", (-W - 0.3, -3, H), (W + 0.3, END + 0.3, H + 0.1), tile)
    for side in (-1, 1):
        x0, x1 = (-W - 0.2, -W) if side < 0 else (W, W + 0.2)
        c.box(f"wall low {side}", (x0, -3, 0.1), (x1, END, 1.0), lower)
        c.box(f"wall high {side}", (x0, -3, 1.0), (x1, END, H), upper)
        c.box(f"skirting {side}", (x0 - 0.01 * side, -3, 0), (x1 - 0.01 * side, END, 0.1), kick)
        # Handrail on brackets, 0.9 m up.
        xr = side * (W - 0.07)
        c.cylinder(f"rail {side}", (xr, -3.0, 0.9), 0.025, END + 3.0, rail, segments=16, axis='y')
        for y in range(-2, int(END), 3):
            c.box(f"bracket {side} {y}", (min(xr, side * W), y - 0.02, 0.86), (max(xr, side * W), y + 0.02, 0.9), rail)
    # Closed doors along both walls, alternating, recessed frames with small dark windows.
    for i, y in enumerate([3.0, 7.5, 12.0, 16.5, 21.0, 25.5, 30.0]):
        for side in ((-1, 1) if i in (1, 4) else (-1,) if i % 2 == 0 else (1,)):   # alternating; two places have both
            xs = side * W
            depth = -0.04 * side
            c.box(f"door frame {i} {side}", (min(xs, xs + 3 * depth), y - 0.6, 0), (max(xs, xs + 3 * depth), y + 0.6, 2.2), frame)
            c.box(f"door {i} {side}", (min(xs + 3 * depth, xs + 4 * depth), y - 0.5, 0.02), (max(xs + 3 * depth, xs + 4 * depth), y + 0.5, 2.1), door)
            c.box(f"door window {i} {side}", (min(xs + 4 * depth, xs + 4.5 * depth), y - 0.12, 1.35), (max(xs + 4 * depth, xs + 4.5 * depth), y + 0.12, 1.75), glass)
    # The far end: double doors with windows (cold daylight beyond) and the exit sign above.
    c.box("end wall", (-W - 0.3, END, 0), (W + 0.3, END + 0.3, H), upper)
    c.box("end door l", (-0.9, END - 0.06, 0.02), (-0.02, END, 2.1), door)
    c.box("end door r", (0.02, END - 0.06, 0.02), (0.9, END, 2.1), door)
    c.box("end frame", (-1.0, END - 0.04, 0), (1.0, END - 0.02, 2.2), frame)
    daylight = c.emitter("daylight beyond", (0.75, 0.85, 1.0), 2.2)
    for x in (-0.45, 0.45):   # the end doors' windows: cold daylight from beyond (base pass only)
        pane = c.plane(f"end window {x}", (x, END - 0.065, 1.5), (0.3, 0.6), daylight, rotation=(math.pi / 2, 0, 0))
        c.in_group("base", pane)
    c.area_light("daylight spill", "base", (0, END - 0.2, 1.5), (0.9, 0.6), 4.0, (0.75, 0.85, 1.0), rotation=(-math.pi / 2, 0, 0))
    sign_glow = c.emitter("exit sign", (0.2, 1.0, 0.45), 14.0)
    c.box("exit housing", (-0.32, END - 0.12, 2.28), (0.32, END - 0.02, 2.5), frame)
    face = c.plane("exit face", (0, END - 0.125, 2.39), (0.56, 0.17), sign_glow, rotation=(math.pi / 2, 0, 0))
    c.in_group("exit", face)
    c.point_light("exit light", "exit", (0, END - 0.3, 2.35), 6.0, (0.25, 1.0, 0.5), radius=0.1)
    # The panels down the middle of the ceiling, every 3 m.
    colour = (0.92, 1.0, 0.96)
    failing_y = 10.5
    for k in range(12):
        y = 1.5 + 3.0 * k
        group = "failing" if y == failing_y else "near" if y < 7.0 else "mid" if y < 17.0 else "far"
        c.ceiling_panel(f"panel {k}", group, (0.0, y), (0.6, 1.2), H, 13.0, colour, glow=4.0)
    c.camera((0.15, -2.0, 1.6), (0.15, END, 1.6), lens=26.0, shift_y=-0.03)
    return scene, None, None


def linoleum():
    """Sheet linoleum: pale green-grey with a fine speckle, a polished (glossy) surface."""
    import bpy
    mat = bpy.data.materials.new("linoleum")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    coords = c.planar_coords(nt)
    speck = nt.nodes.new("ShaderNodeTexNoise")
    speck.inputs["Scale"].default_value = 120.0
    speck.inputs["Detail"].default_value = 2.0
    nt.links.new(coords, speck.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(speck.outputs["Fac"], ramp.inputs["Value"])
    ramp.inputs["From Min"].default_value = 0.45
    ramp.inputs["From Max"].default_value = 0.65
    ramp.inputs["To Min"].default_value = 0.92
    ramp.inputs["To Max"].default_value = 1.0
    col = nt.nodes.new("ShaderNodeVectorMath")
    col.operation = 'SCALE'
    col.inputs[0].default_value = (0.5, 0.56, 0.52)
    nt.links.new(ramp.outputs["Result"], col.inputs["Scale"])
    nt.links.new(col.outputs[0], bsdf.inputs["Base Color"])
    wear = nt.nodes.new("ShaderNodeTexNoise")
    wear.inputs["Scale"].default_value = 0.7
    nt.links.new(coords, wear.inputs["Vector"])
    rough = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(wear.outputs["Fac"], rough.inputs["Value"])
    rough.inputs["To Min"].default_value = 0.06
    rough.inputs["To Max"].default_value = 0.2
    nt.links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    c.set_input(bsdf, ["Specular IOR Level", "Specular"], 0.6)
    return mat


PASSES = [
    {"name": "near", "group": "near", "kind": "tube", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "tube", "default": 1.0},
    {"name": "far", "group": "far", "kind": "tube", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
    {"name": "exit", "group": "exit", "kind": "sign", "default": 1.0},
]
