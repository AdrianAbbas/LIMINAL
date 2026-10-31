"""EMPTY SHOPPING MALL: a two-level atrium under a long skylight - polished stone floor mirroring
everything, closed shop fronts behind dark glass and half-lowered shutters, an upper gallery
with a glass balustrade, a pair of stopped escalators, planters, a still fountain basin.
Light passes: base (daylight through the skylight), atrium (the gallery's downlights onto the
floor), shops (the closed shops' sign strips and night lights), lastshop (one shop still lit,
its shutter half up: Closing Time), fountain (the basin's underwater lamps), failing (a gallery
downlight that flickers with the hum).
"""

import math

import common as c

W = 7.0       # half width of the atrium (shop fronts at +-W)
L = 54.0      # length
UP = 5.0      # upper gallery floor
GAL = 3.2     # gallery depth (the void between galleries is +-(W - GAL))
ROOF = 10.5


def build():
    scene = c.reset()
    c.world_colour((0.78, 0.84, 0.92), 0.25)
    stone = c.tiles("polished stone", (0.56, 0.52, 0.46), (0.36, 0.34, 0.31), size=0.6, grout_width=0.003, roughness=0.07, variation=0.12, glossy=0.6, bump=0.08)
    plaster = c.matte("plaster", (0.78, 0.76, 0.72), roughness=0.7, noise_scale=0.4, noise_amount=0.05)
    fascia = c.matte("fascia", (0.78, 0.77, 0.74), roughness=0.4, specular=0.6)
    glass = c.dark_glass("shop glass", tint=(0.025, 0.03, 0.035))
    shutter = c.ribbed("shutter", (0.55, 0.56, 0.57), pitch=0.07)
    steel = c.metal("brushed steel", (0.72, 0.72, 0.72), roughness=0.28)
    rail_glass = c.matte("balustrade glass", (0.32, 0.38, 0.38), roughness=0.02, specular=1.0)
    rubber = c.matte("escalator rubber", (0.04, 0.04, 0.04), roughness=0.5)
    soil = c.matte("planter soil", (0.07, 0.05, 0.04), roughness=0.95)
    leaf = c.matte("dry leaves", (0.18, 0.2, 0.09), roughness=0.7, noise_scale=6, noise_amount=0.3)
    basin_tile = c.tiles("fountain tiles", (0.32, 0.5, 0.52), (0.2, 0.3, 0.3), size=0.1, roughness=0.12)
    pool_water = c.water("fountain water", tint=(0.8, 0.92, 0.9), absorb=(0.4, 0.75, 0.72), density=0.8)
    interior = c.matte("shop interior", (0.5, 0.48, 0.44), roughness=0.8)

    # Floors and the gallery slabs (the void in the middle runs the length of the atrium).
    c.box("floor", (-W - 4, -4, -0.1), (W + 4, L + 4, 0), stone)
    inner = W - GAL
    for side in (-1, 1):
        x0, x1 = sorted((side * inner, side * (W + 4)))
        c.box(f"gallery slab {side}", (x0, -4, UP - 0.45), (x1, L + 4, UP), stone)
        c.box(f"gallery fascia {side}", (min(side * inner, side * (inner - 0.04)), -4, UP - 0.7), (max(side * inner, side * (inner - 0.04)), L + 4, UP + 0.05), fascia)
        # Glass balustrade on a steel handrail.
        xb = side * (inner - 0.08)
        c.box(f"balustrade {side}", (xb - 0.01, -4, UP), (xb + 0.01, L + 4, UP + 1.05), rail_glass)
        c.cylinder(f"handrail {side}", (xb, -4, UP + 1.08), 0.03, L + 8, steel, segments=14, axis='y')
        # The gallery's ceiling (under the roof) and the shop wall above it.
        c.box(f"gallery ceiling {side}", (x0, -4, UP + 3.6), (x1, L + 4, UP + 3.75), plaster)
        for level, z0 in ((0, 0.0), (1, UP)):
            xw = side * W
            # Shop fronts every 6 m: glass, a fascia band with the dark sign, pilasters between.
            for k, y in enumerate(range(0, int(L), 6)):
                yc = y + 3.0
                c.box(f"pilaster {side} {level} {k}", (min(xw, xw + side * 0.3), y - 0.3, z0), (max(xw, xw + side * 0.3), y + 0.3, z0 + 3.6), plaster)
                c.box(f"fascia {side} {level} {k}", (min(xw, xw + side * 0.25), y + 0.3, z0 + 2.9), (max(xw, xw + side * 0.25), y + 5.7, z0 + 3.6), fascia)
                lit = (side, level, k) == LAST_SHOP
                if lit:
                    # The last shop still open: shutter half up, its interior lit.
                    c.box(f"shutter {side} {level} {k}", (min(xw + side * 0.2, xw + side * 0.24), y + 0.3, z0 + 1.6), (max(xw + side * 0.2, xw + side * 0.24), y + 5.7, z0 + 2.9), shutter)
                    deep = side * 5.0
                    c.box(f"shop floor {k}", (min(xw, xw + deep), y + 0.3, z0 - 0.1), (max(xw, xw + deep), y + 5.7, z0 + 0.0), stone)
                    c.box(f"shop back {k}", (min(xw + deep, xw + deep + side * 0.2), y + 0.3, z0), (max(xw + deep, xw + deep + side * 0.2), y + 5.7, z0 + 3.0), interior)
                    c.box(f"shop ceiling {k}", (min(xw, xw + deep), y + 0.3, z0 + 3.0), (max(xw, xw + deep), y + 5.7, z0 + 3.1), plaster)
                    for e, ye in enumerate((y + 0.2, y + 5.7)):
                        c.box(f"shop side {k} {e}", (min(xw, xw + deep), ye, z0), (max(xw, xw + deep), ye + 0.1, z0 + 3.0), interior)
                    for r in range(3):   # empty display shelves
                        c.box(f"shelf {k} {r}", (min(xw + side * 1.0, xw + side * 4.6), y + 1.0 + r * 1.5, z0), (max(xw + side * 1.0, xw + side * 4.6), y + 1.4 + r * 1.5, z0 + 1.1), interior)
                    for r in range(2):
                        c.ceiling_panel(f"shop panel {k} {r}", "lastshop", (xw + side * (1.6 + 2.0 * r), yc), (0.6, 1.2), z0 + 3.0, 22.0, (1.0, 0.95, 0.85), glow=4.0)
                    sign = c.emitter("open sign", (1.0, 0.85, 0.6), 6.0)
                    s = c.plane(f"shop sign {k}", (xw + side * 0.26, yc, z0 + 3.25), (4.6, 0.35), sign, rotation=(math.pi / 2, 0, -side * math.pi / 2))
                    c.in_group("lastshop", s)
                    continue
                # Closed: dark glass, then (most of them) a lowered shutter.
                c.box(f"glass {side} {level} {k}", (min(xw + side * 0.18, xw + side * 0.2), y + 0.3, z0), (max(xw + side * 0.18, xw + side * 0.2), y + 5.7, z0 + 2.9), glass)
                down = (k * 7 + level * 3 + (side > 0)) % 3
                if down:
                    h = 2.9 if down == 1 else 1.3 + 0.4 * (k % 3)
                    c.box(f"shutter {side} {level} {k}", (min(xw + side * 0.12, xw + side * 0.16), y + 0.3, z0 + 2.9 - h), (max(xw + side * 0.12, xw + side * 0.16), y + 5.7, z0 + 2.9), shutter)
                # The shop's dim sign strip and a security light deep inside (the "shops" pass).
                if (k + level + (side > 0)) % 2 == 0:
                    strip = c.emitter(f"sign strip {side} {level} {k}", (0.85, 0.9, 1.0) if k % 3 else (1.0, 0.8, 0.55), 2.2)
                    s = c.plane(f"strip {side} {level} {k}", (xw + side * 0.26, yc, z0 + 3.42), (3.8, 0.06), strip, rotation=(math.pi / 2, 0, -side * math.pi / 2))
                    s.visible_shadow = False
                    c.in_group("shops", s)
            # The wall above the shops, up to the gallery ceiling or the roof.
            c.box(f"upper wall {side} {level}", (min(xw, xw + side * 0.3), -4, z0 + 3.6), (max(xw, xw + side * 0.3), L + 4, z0 + 3.6 + (1.4 if level == 0 else 0.15)), plaster)
        # The clerestory wall up to the roof on the void side.
        c.box(f"clerestory {side}", (min(side * inner, side * (inner + 0.3)), -4, UP + 3.75), (max(side * inner, side * (inner + 0.3)), L + 4, ROOF), plaster)
        # Gallery downlights (the atrium pass), one failing.
        for k, y in enumerate(range(1, int(L), 4)):
            x = side * (inner + 1.3)
            group = "failing" if (side, k) == FAILING else "atrium"
            c.downlight(f"gallery light {side} {k}", group, (x, y + 1.0, UP - 0.45), 170.0, (1.0, 0.9, 0.74), angle=80, glow=9.0)
            c.downlight(f"upper light {side} {k}", group if group == "atrium" else "atrium", (x, y + 1.0, UP + 3.6), 60.0, (1.0, 0.9, 0.74), angle=80, glow=9.0)
    # Ends of the atrium: a far wall with a wide closed entrance, a near wall behind the camera.
    c.box("far wall", (-W - 4, L, 0), (W + 4, L + 0.3, ROOF), plaster)
    c.box("far doors", (-2.5, L - 0.05, 0), (2.5, L, 3.2), glass)
    c.box("far canopy", (-3.0, L - 0.8, 3.2), (3.0, L, 3.5), fascia)
    c.box("near wall", (-W - 4, -4.3, 0), (W + 4, -4, ROOF), plaster)

    # The roof: a long barrel of glass over the void (lit by the sky), solid elsewhere.
    sky_w = inner - 0.6
    for k, y in enumerate(range(-2, int(L), 8)):
        y0, y1 = y + 0.6, y + 7.4
        c.box(f"roof beam {k}", (-inner, y, ROOF - 0.5), (inner, y + 0.6, ROOF), steel)
        c.area_light(f"skylight {k}", "base", (0, (y0 + y1) / 2, ROOF + 0.4), (2 * sky_w, y1 - y0), 160.0, (0.86, 0.9, 1.0))
    c.box("roof well left", (-inner, -4, ROOF - 0.5), (-sky_w, L + 4, ROOF + 0.3), plaster)
    c.box("roof well right", (sky_w, -4, ROOF - 0.5), (inner, L + 4, ROOF + 0.3), plaster)
    c.box("roof left", (-W - 4, -4, ROOF), (-sky_w, L + 4, ROOF + 0.3), plaster)
    c.box("roof right", (sky_w, -4, ROOF), (W + 4, L + 4, ROOF + 0.3), plaster)
    sky_glow = c.emitter("sky through the glass", (0.82, 0.88, 0.96), 0.7)
    sky_plane = c.plane("sky", (0, L / 2, ROOF + 0.9), (2 * sky_w + 0.5, L + 8), sky_glow, rotation=(math.pi, 0, 0))
    sky_plane.visible_shadow = False
    c.in_group("base", sky_plane)

    # Two stopped escalators in the void, side by side, rising away from the camera.
    for e, xe in enumerate((-1.4, 0.2)):
        y0, y1 = 20.0, 20.0 + UP / math.tan(math.radians(30))
        c.ramp_box(f"escalator truss {e}", xe, xe + 1.2, y0, y1, 0.05, UP, 0.6, steel)
        c.stairs(f"escalator {e}", xe + 0.08, xe + 1.12, y0, 0.0, 0.2 / math.tan(math.radians(30)), 0.2, int(UP / 0.2), rubber)
        for s in (0, 1):
            xs = xe + (0.0 if s == 0 else 1.12)
            c.ramp_box(f"escalator side {e} {s}", xs, xs + 0.08, y0 - 1.0, y1 + 1.0, 1.0, UP + 1.0, 0.95, rail_glass)
            c.ramp_box(f"escalator rail {e} {s}", xs - 0.01, xs + 0.09, y0 - 1.0, y1 + 1.0, 1.06, UP + 1.06, 0.06, rubber)
    c.box("escalator landing", (-1.5, 20.0 + UP / math.tan(math.radians(30)), UP - 0.45), (1.5, 20.0 + UP / math.tan(math.radians(30)) + 3.0, UP), stone)

    # Planters with dry plants, and the fountain: a round basin of still water, its lamps off.
    for k, (x, y) in enumerate([(-3.0, 6.0), (3.0, 6.0), (-3.0, 34.0), (3.0, 34.0), (0.0, 46.0)]):
        c.box(f"planter {k}", (x - 0.7, y - 0.7, 0), (x + 0.7, y + 0.7, 0.55), fascia)
        c.box(f"planter soil {k}", (x - 0.62, y - 0.62, 0.45), (x + 0.62, y + 0.62, 0.5), soil)
        for j in range(5):
            a = j * 1.3 + k
            c.cylinder(f"plant {k} {j}", (x + 0.25 * math.cos(a), y + 0.25 * math.sin(a), 0.5), 0.05 + 0.03 * (j % 2), 0.6 + 0.25 * (j % 3), leaf, segments=8)
    fx, fy, fr = 0.6, 12.0, 2.2
    c.cylinder("fountain rim", (fx, fy, 0), fr, 0.5, fascia, segments=64)
    c.cylinder("fountain basin", (fx, fy, -0.4), fr - 0.15, 0.85, basin_tile, segments=64)
    c.cylinder("fountain water", (fx, fy, -0.4), fr - 0.16, 0.78, pool_water, segments=64)
    c.cylinder("fountain spout", (fx, fy, 0), 0.12, 1.1, steel, segments=20)
    for j in range(6):   # underwater lamps around the basin (the fountain pass)
        a = 2 * math.pi * j / 6
        c.point_light(f"fountain lamp {j}", "fountain", (fx + 1.6 * math.cos(a), fy + 1.6 * math.sin(a), 0.05), 25.0, (0.6, 0.9, 1.0), radius=0.08)
    fountain_glow = c.emitter("fountain glow", (0.5, 0.85, 1.0), 1.0)
    g = c.cylinder("fountain glow disc", (fx, fy, -0.36), fr - 0.2, 0.02, fountain_glow, segments=48)
    g.visible_shadow = False
    c.in_group("fountain", g)

    c.camera((1.1, -3.0, 1.6), (0.7, L, 2.4), lens=24.0, shift_y=0.06)
    return scene, None, None


LAST_SHOP = (1, 0, 2)    # right side, ground floor, the third shop: still lit (Closing Time)
FAILING = (-1, 3)        # a gallery downlight on the left, 14 m in

BASE_DEFAULT = 1.0

PASSES = [
    {"name": "atrium", "group": "atrium", "kind": "lamp", "default": 1.0},
    {"name": "shops", "group": "shops", "kind": "sign", "default": 1.0},
    {"name": "lastshop", "group": "lastshop", "kind": "lamp", "default": 1.0},
    {"name": "fountain", "group": "fountain", "kind": "lamp", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
]

# Closing Time: daylight going, the gallery lights on, the last shop still lit with its shutter
# half up. Forgotten: years empty - only the daylight through dirty glass, nothing running.
# Endless: night; every downlight on, the fountain's lamps under the still water.
CHARACTERS = {
    "base": [0.7, 1.5, 0.05],
    "atrium": [0.8, 0.0, 1.2],
    "shops": [0.5, 0.0, 1.0],
    "lastshop": [1.0, 0.0, 0.0],
    "fountain": [0.0, 0.0, 1.0],
    "failing": [1.0, 0.0, 1.0],
}
