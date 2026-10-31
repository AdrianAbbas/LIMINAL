"""UNDERGROUND PARKING GARAGE: a low concrete deck - rows of square pillars receding, painted
bay lines and numbers, a drive aisle down the middle, pipes and cable trays under the slab,
pools of sodium-orange light with dark gaps between them, one cold fluorescent tube, an exit
sign. Light passes: base (almost nothing: a little daylight down the far ramp), near, mid, far
(the sodium lamps by distance), cold (a run of fluorescent tubes by the lift lobby), failing
(a sodium lamp that flickers with the hum), exit (the green sign), headlights (a car's lights
somewhere beyond the pillars: Distant Movement / Pursuit).
"""

import math

from mathutils import Vector

import common as c

H = 2.45
L = 60.0


def build():
    scene = c.reset()
    c.world_colour((0.6, 0.68, 0.8), 0.25)
    slab = c.matte("concrete slab", (0.5, 0.49, 0.46), roughness=0.85, noise_scale=0.25, noise_amount=0.25, stains=0.15, bump=0.6)
    floor = c.matte("sealed concrete", (0.36, 0.36, 0.35), roughness=0.38, noise_scale=0.2, noise_amount=0.3, stains=0.2, specular=0.6, bump=0.25)
    pillar = c.matte("pillar concrete", (0.58, 0.57, 0.54), roughness=0.8, noise_scale=0.8, noise_amount=0.2, stains=0.1, bump=0.5)
    paint = c.matte("bay paint", (0.82, 0.8, 0.7), roughness=0.55, noise_scale=6, noise_amount=0.25)
    yellow = c.matte("hazard paint", (0.75, 0.6, 0.12), roughness=0.5, noise_scale=4, noise_amount=0.3)
    pipe = c.matte("pipe", (0.42, 0.42, 0.4), roughness=0.5, specular=0.5)
    red_pipe = c.matte("sprinkler pipe", (0.45, 0.08, 0.06), roughness=0.5)
    wall = c.matte("block wall", (0.55, 0.54, 0.5), roughness=0.85, noise_scale=0.5, noise_amount=0.2, bump=0.4)
    housing = c.matte("lamp housing", (0.2, 0.2, 0.2), roughness=0.5)
    dark = c.matte("car body", (0.08, 0.09, 0.1), roughness=0.25, specular=0.8)
    glass = c.dark_glass("car glass")

    c.box("floor", (-14, -4, -0.1), (14, L + 4, 0), floor)
    c.box("slab", (-14, -4, H), (14, L + 4, H + 0.3), slab)
    # Downstand beams across the deck every 8 m.
    for k, y in enumerate(range(0, int(L) + 1, 8)):
        c.box(f"beam {k}", (-14, y - 0.25, H - 0.45), (14, y + 0.25, H), slab)
    # Pillars: two rows beside the drive aisle, two more rows beyond the bays.
    for x in (-8.6, -3.1, 3.1, 8.6):
        for k, y in enumerate(range(0, int(L) + 1, 8)):
            c.box(f"pillar {x} {k}", (x - 0.3, y - 0.3, 0), (x + 0.3, y + 0.3, H - 0.45), pillar)
            c.box(f"pillar band {x} {k}", (x - 0.31, y - 0.31, 0), (x + 0.31, y + 0.31, 0.6), yellow)
    # Side walls (block) and a far wall with a ramp opening going up to daylight.
    for side in (-1, 1):
        c.box(f"side wall {side}", (side * 14 - 0.3 * (side > 0), -4, 0), (side * 14 + 0.3 * (side < 0) + 0.0, L + 4, H), wall)
    c.box("near wall", (-14, -4.3, 0), (14, -4, H), wall)
    c.box("far wall left", (-14, L, 0), (-2.6, L + 0.3, H), wall)
    c.box("far wall right", (2.6, L, 0), (14, L + 0.3, H), wall)
    c.box("far lintel", (-2.6, L, H - 0.6), (2.6, L + 0.3, H), wall)
    c.ramp_box("ramp", -2.6, 2.6, L, L + 14, 0.0, 3.2, 0.3, floor)
    c.box("ramp wall l", (-2.9, L, 0), (-2.6, L + 14, 4.5), wall)
    c.box("ramp wall r", (2.6, L, 0), (2.9, L + 14, 4.5), wall)
    c.box("ramp roof", (-2.9, L, H), (2.9, L + 9, H + 0.3), slab)
    day = c.emitter("daylight at the top", (0.8, 0.86, 0.96), 2.0)
    p = c.plane("ramp sky", (0, L + 13.5, 3.4), (5.2, 2.6), day, rotation=(math.pi / 2, 0, 0))
    c.in_group("base", p)
    c.area_light("ramp daylight", "base", (0, L + 11, 4.0), (5.0, 4.0), 260.0, (0.8, 0.86, 0.96), rotation=(math.radians(-150), 0, 0))
    # Painted bay lines (perpendicular parking each side of the aisle) and the aisle's arrows.
    for side in (-1, 1):
        for k in range(0, int(L / 2.5) + 1):
            y = k * 2.5
            x0, x1 = sorted((side * 3.6, side * 8.2))
            c.box(f"bay line {side} {k}", (x0, y - 0.05, 0.0), (x1, y + 0.05, 0.003), paint)
        c.box(f"aisle edge {side}", (side * 3.55 - 0.05, -4, 0), (side * 3.55 + 0.05, L, 0.003), paint)
    for k, y in enumerate((6.0, 22.0, 38.0)):
        c.box(f"arrow stem {k}", (-0.1, y, 0), (0.1, y + 1.6, 0.003), paint)
        c.hexa(f"arrow head {k}", [(-0.45, y + 1.6, 0), (0.45, y + 1.6, 0), (0.0, y + 2.4, 0), (-0.02, y + 2.4, 0),
                                    (-0.45, y + 1.6, 0.003), (0.45, y + 1.6, 0.003), (0.0, y + 2.4, 0.003), (-0.02, y + 2.4, 0.003)], paint)
    # Services under the slab: two long pipes, a red sprinkler main with drops, a cable tray.
    c.cylinder("pipe a", (-1.6, -4, H - 0.2), 0.09, L + 8, pipe, segments=16, axis='y')
    c.cylinder("pipe b", (-1.25, -4, H - 0.16), 0.05, L + 8, pipe, segments=12, axis='y')
    c.cylinder("sprinkler main", (5.6, -4, H - 0.12), 0.05, L + 8, red_pipe, segments=12, axis='y')
    c.box("cable tray", (1.9, -4, H - 0.14), (2.3, L + 4, H - 0.1), pipe)
    # Three parked cars, far apart: simple dark shapes (nobody comes back for them).
    for k, (x, y) in enumerate([(-5.9, 17.5), (6.0, 31.0), (-6.0, 45.0)]):
        c.box(f"car {k} body", (x - 0.9, y - 2.2, 0.25), (x + 0.9, y + 2.2, 0.85), dark)
        c.box(f"car {k} cabin", (x - 0.78, y - 1.1, 0.85), (x + 0.78, y + 0.9, 1.4), glass)
        for wx in (-0.8, 0.8):
            for wy in (-1.4, 1.4):
                c.box(f"car {k} wheel {wx} {wy}", (x + wx - 0.11, y + wy - 0.32, 0), (x + wx + 0.11, y + wy + 0.32, 0.6), housing)

    # Sodium lamps: surface boxes on the slab over the aisle edges, every 8 m, offset from the
    # pillars, so pools of orange fall with dark gaps between.
    sodium = (1.0, 0.55, 0.18)
    for k, y in enumerate(range(4, int(L), 8)):
        for x in (-3.4, 3.4):
            group = "failing" if (x, k) == (3.4, 2) else "near" if y < 14 else "mid" if y < 34 else "far"
            c.box(f"sodium housing {x} {k}", (x - 0.25, y - 0.4, H - 0.12), (x + 0.25, y + 0.4, H), housing)
            glow = c.emitter(f"sodium lens {x} {k}", sodium, 14.0)
            lens = c.plane(f"sodium lens {x} {k}", (x, y, H - 0.125), (0.4, 0.7), glow, rotation=(math.pi, 0, 0))
            lens.visible_shadow = False
            c.in_group(group, lens)
            c.point_light(f"sodium {x} {k}", group, (x, y, H - 0.2), 300.0, sodium, radius=0.15)
    # The lift lobby on the left, under cold fluorescent tubes (the one cold light in the deck).
    c.box("lobby wall", (-13.7, 24.0, 0), (-11.0, 24.3, H), wall)
    c.box("lobby wall b", (-13.7, 29.7, 0), (-11.0, 30.0, H), wall)
    steel = c.metal("lift doors", (0.6, 0.6, 0.6), roughness=0.35)
    c.box("lift doors", (-13.72, 25.6, 0), (-13.68, 27.4, 2.1), steel)
    for j in range(2):
        c.tube(f"lobby tube {j}", "cold", (-12.4 + 0.5 * j, 24.6, H - 0.05), 29.4, (0.85, 0.95, 1.0), 120.0, glow=6.0)
    # The exit sign over the ramp.
    sign = c.emitter("exit sign", (0.15, 1.0, 0.4), 14.0)
    c.box("exit housing", (-0.4, L - 0.12, H - 0.85), (0.4, L, H - 0.62), housing)
    f = c.plane("exit face", (0, L - 0.125, H - 0.735), (0.7, 0.18), sign, rotation=(math.pi / 2, 0, 0))
    c.in_group("exit", f)
    c.point_light("exit glow", "exit", (0, L - 0.4, H - 0.8), 6.0, (0.2, 1.0, 0.45), radius=0.1)
    # A car parked nose-out in a right-hand bay, 20 m in, its headlights on across the aisle.
    for dy in (-0.6, 0.6):
        aim = Vector((-1.0, -0.55, -0.12))
        c.spot_light(f"headlight {dy}", "headlights", (4.85, 20.0 + dy, 0.72), aim.to_track_quat('-Z', 'Y').to_euler(), 1600.0, (1.0, 0.95, 0.85), math.radians(45), radius=0.06, blend=0.5)
        hl = c.emitter(f"headlamp {dy}", (1.0, 0.95, 0.85), 60.0)
        b = c.box(f"headlamp {dy}", (4.93, 20.0 + dy - 0.17, 0.66), (4.95, 20.0 + dy + 0.17, 0.78), hl)
        b.visible_shadow = False
        c.in_group("headlights", b)
    c.box("car body near", (4.95, 19.1, 0.25), (9.3, 20.9, 0.85), dark)
    c.box("car cabin near", (6.0, 19.22, 0.85), (8.1, 20.78, 1.38), glass)
    for wx in (5.7, 8.5):
        for wy in (19.05, 20.95):
            c.box(f"car near wheel {wx} {wy}", (wx - 0.32, wy - 0.11, 0), (wx + 0.32, wy + 0.11, 0.6), housing)

    c.camera((0.4, -2.6, 1.6), (0.2, L, 1.5), lens=24.0, shift_y=-0.02)
    return scene, None, None


BASE_DEFAULT = 1.0

PASSES = [
    {"name": "near", "group": "near", "kind": "lamp", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "lamp", "default": 1.0},
    {"name": "far", "group": "far", "kind": "lamp", "default": 1.0},
    {"name": "cold", "group": "cold", "kind": "tube", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
    {"name": "exit", "group": "exit", "kind": "sign", "default": 1.0},
    {"name": "headlights", "group": "headlights", "kind": "lamp", "default": 1.0},
]

# Vacant: every sodium lamp, nobody. Distant Movement: a car's lights somewhere across the deck,
# the far lamps dimmer. Pursuit: the near lamps dead, the middle ones stuttering with the hum,
# the headlights full on.
CHARACTERS = {
    "near": [1.0, 1.0, 0.0],
    "mid": [1.0, 0.65, 0.8],
    "far": [1.0, 0.35, 1.0],
    "headlights": [0.0, 0.6, 2.2],
}
FLICKER = {"mid": [False, False, True]}
