"""EMPTY SUPERMARKET: long aisles of half-empty shelving under rows of fluorescent tubes,
a glossy vinyl floor mirroring them, a wall of glowing refrigerated cases along the back,
hanging aisle signs, a checkout lane by the camera.
Light passes: base (night through the front windows behind), near, mid, far (the tube rows by
distance), fridges (the refrigerated cases' own light), checkout (the till's lamp and display),
failing (a tube that flickers with the hum).
"""

import math
import random

import common as c

H = 4.2
L = 34.0
AISLES = [-6.0, -2.0, 2.0, 6.0]   # shelving runs (centres), aisles between them


def build():
    scene = c.reset()
    c.world_colour((0.3, 0.35, 0.5), 0.3)
    floor = c.tiles("vinyl floor", (0.6, 0.59, 0.56), (0.48, 0.47, 0.45), size=0.3, grout_width=0.002, roughness=0.08, variation=0.05, glossy=0.6, bump=0.03)
    wall = c.matte("wall", (0.82, 0.82, 0.8), roughness=0.7)
    ceiling = c.matte("ceiling", (0.5, 0.5, 0.5), roughness=0.85)
    shelf = c.matte("shelving", (0.78, 0.79, 0.8), roughness=0.4, specular=0.6)
    upright = c.matte("upright", (0.55, 0.56, 0.58), roughness=0.4, specular=0.6)
    kick = c.matte("kick plate", (0.25, 0.26, 0.28), roughness=0.4)
    packs = [c.matte(f"goods {i}", col, roughness=0.5, specular=0.5) for i, col in enumerate([
        (0.55, 0.16, 0.12), (0.7, 0.62, 0.3), (0.2, 0.3, 0.48), (0.8, 0.78, 0.72), (0.25, 0.4, 0.28), (0.62, 0.42, 0.2), (0.45, 0.25, 0.4)])]
    case_body = c.matte("fridge case", (0.85, 0.86, 0.87), roughness=0.35, specular=0.6)
    case_glass = c.matte("fridge glass", (0.3, 0.36, 0.38), roughness=0.02, specular=1.0)
    sign_board = c.matte("aisle sign", (0.1, 0.3, 0.5), roughness=0.4)
    belt = c.matte("belt", (0.05, 0.05, 0.05), roughness=0.6)
    till = c.matte("till", (0.2, 0.2, 0.22), roughness=0.4)
    rng = random.Random(7)

    c.box("floor", (-11, -6, -0.1), (11, L + 3, 0), floor)
    c.box("ceiling", (-11, -6, H), (11, L + 3, H + 0.2), ceiling)
    for side in (-1, 1):
        c.box(f"side wall {side}", (min(side * 10.0, side * 10.3), -6, 0), (max(side * 10.0, side * 10.3), L + 3, H), wall)
    c.box("back wall", (-11, L + 2.4, 0), (11, L + 2.7, H), wall)
    c.box("front wall", (-11, -6.3, 0), (11, -6, H), wall)
    # Exposed ceiling: open-web joists and ducts (a warehouse store).
    duct = c.metal("duct", (0.6, 0.6, 0.6), roughness=0.45)
    for k, y in enumerate(range(-4, int(L) + 2, 4)):
        c.box(f"joist {k}", (-10, y - 0.08, H - 0.35), (10, y + 0.08, H), upright)
    c.cylinder("duct a", (-4.0, -6, H - 0.7), 0.3, L + 8, duct, segments=24, axis='y')
    c.cylinder("duct b", (4.0, -6, H - 0.6), 0.22, L + 8, duct, segments=20, axis='y')

    # Gondola shelving: each run 1.2 m deep, 2.0 m high, from y 2 to y 26, five shelves a side,
    # stocked unevenly (gaps where things ran out and nobody came back).
    for a, x in enumerate(AISLES):
        y0, y1 = 2.0, 26.0
        c.box(f"gondola spine {a}", (x - 0.03, y0, 0), (x + 0.03, y1, 2.0), upright)
        c.box(f"gondola base {a}", (x - 0.6, y0, 0), (x + 0.6, y1, 0.15), kick)
        for end in (y0, y1):
            c.box(f"gondola end {a} {end}", (x - 0.62, end - 0.04, 0), (x + 0.62, end + 0.04, 2.05), upright)
        for lvl in range(5):
            z = 0.15 + 0.38 * lvl
            c.box(f"shelf {a} {lvl}", (x - 0.58, y0, z), (x + 0.58, y1, z + 0.025), shelf)
            for side in (-1, 1):
                y = y0 + 0.05
                while y < y1 - 0.3:
                    w = rng.uniform(0.6, 2.2)
                    if rng.random() < 0.72:   # a block of the same product
                        hgt = rng.uniform(0.18, 0.32)
                        dep = rng.uniform(0.25, 0.5)
                        xa = x + side * 0.05
                        xb = x + side * (0.05 + dep)
                        c.box(f"goods {a} {lvl} {side} {y:.2f}", (min(xa, xb), y, z + 0.025), (max(xa, xb), min(y + w, y1 - 0.05), z + 0.025 + hgt), packs[rng.randrange(len(packs))])
                    y += w + 0.04
        # A hanging aisle sign over each aisle (dark board, the "number" a white square).
    for a, x in enumerate([-8.0, -4.0, 0.0, 4.0, 8.0]):
        c.box(f"sign {a}", (x - 0.6, 12.0, 2.9), (x + 0.6, 12.03, 3.4), sign_board)
        c.box(f"sign num {a}", (x - 0.12, 11.99, 3.0), (x + 0.12, 12.0, 3.3), packs[3])
        c.box(f"sign wire {a}", (x - 0.005, 12.0, 3.4), (x + 0.005, 12.03, H - 0.35), upright)
    # Refrigerated cases along the back: glass doors, their own cold light (the fridges pass).
    fridge_light = (0.86, 0.96, 1.0)
    for k in range(18):
        x0 = -9.6 + 1.05 * k
        c.box(f"case frame {k}", (x0, L - 0.2, 0), (x0 + 1.05, L + 2.4, 2.4), case_body)
        c.box(f"case glass {k}", (x0 + 0.05, L - 0.22, 0.25), (x0 + 1.0, L - 0.2, 2.2), case_glass)
        inner = c.emitter(f"case interior {k}", fridge_light, 1.4)
        p = c.plane(f"case glow {k}", (x0 + 0.525, L - 0.1, 1.2), (0.9, 1.9), inner, rotation=(math.pi / 2, 0, 0))
        p.visible_shadow = False
        c.in_group("fridges", p)
        for lvl in range(4):
            if rng.random() < 0.7:
                c.box(f"case goods {k} {lvl}", (x0 + 0.1, L - 0.05, 0.35 + 0.45 * lvl), (x0 + 0.1 + rng.uniform(0.3, 0.85), L + 0.3, 0.6 + 0.45 * lvl), packs[rng.randrange(len(packs))])
    c.area_light("fridge spill", "fridges", (0, L - 0.4, 1.3), (19.0, 1.9), 260.0, fridge_light, rotation=(math.pi / 2, 0, 0))
    # A checkout lane by the camera, on the left: belt, till, a lamp on a pole with its number.
    c.box("checkout counter", (-6.0, -3.0, 0), (-5.2, 0.5, 0.9), case_body)
    c.box("checkout belt", (-6.0, -1.0, 0.9), (-5.2, 0.5, 0.93), belt)
    c.box("till", (-6.6, -2.6, 0.9), (-6.1, -2.1, 1.2), till)
    c.box("till pole", (-5.25, -2.8, 0.9), (-5.2, -2.75, 2.3), upright)
    num = c.emitter("lane lamp", (1.0, 0.9, 0.7), 6.0)
    lamp = c.box("lane lamp", (-5.4, -2.85, 2.3), (-5.05, -2.7, 2.6), num)
    c.in_group("checkout", lamp)
    display = c.emitter("till display", (0.3, 1.0, 0.6), 4.0)
    d = c.plane("till display", (-6.35, -2.09, 1.12), (0.3, 0.12), display, rotation=(math.pi / 2, 0, 0))
    c.in_group("checkout", d)
    c.point_light("lane glow", "checkout", (-5.2, -2.4, 2.2), 40.0, (1.0, 0.9, 0.7), radius=0.1)
    # Rows of bare fluorescent tubes on strut channels, crosswise rows every 3 m.
    colour = (0.95, 1.0, 1.0)
    for k in range(11):
        y = -2.0 + 3.0 * k
        for x in (-8.0, -4.0, 0.0, 4.0, 8.0):
            group = "failing" if (k, x) == (3, 4.0) else "near" if y < 6 else "mid" if y < 18 else "far"
            c.box(f"channel {k} {x}", (x - 1.3, y - 0.05, H - 1.0), (x + 1.3, y + 0.05, H - 0.95), upright)
            for dy in (-0.04, 0.04):
                mat = c.emitter(f"tube {k} {x} {dy}", colour, 3.0)
                t = c.cylinder(f"tube {k} {x} {dy}", (x - 1.2, y + dy, H - 1.03), 0.016, 2.4, mat, segments=10, axis='x')
                t.visible_shadow = False
                c.in_group(group, t)
            c.area_light(f"tubes {k} {x}", group, (x, y, H - 1.07), (2.4, 0.15), 20.0, colour)
    c.camera((0.0, -4.5, 1.6), (0.0, L, 1.5), lens=24.0, shift_y=0.0)
    return scene, None, None


BASE_DEFAULT = 1.0

PASSES = [
    {"name": "near", "group": "near", "kind": "tube", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "tube", "default": 1.0},
    {"name": "far", "group": "far", "kind": "tube", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
    {"name": "fridges", "group": "fridges", "kind": "lamp", "default": 1.0},
    {"name": "checkout", "group": "checkout", "kind": "lamp", "default": 1.0},
]

# Closing Shift: the far rows off, only the near tubes, the fridges and the one open till.
# Nostalgia: everything on, a warm Saturday (the grade does the rest). Fluorescent Nightmare:
# every tube too bright, the middle rows stuttering with the hum.
CHARACTERS = {
    "near": [0.8, 0.8, 1.7],
    "mid": [0.3, 0.7, 1.7],
    "far": [0.0, 0.55, 1.7],
    "failing": [0.8, 0.8, 1.7],
    "fridges": [1.0, 0.5, 1.8],
    "checkout": [1.0, 0.0, 0.0],
}
FLICKER = {"mid": [False, False, True]}
