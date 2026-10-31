"""DESERTED SUBWAY STATION: an empty side platform - white running-bond tiles with a coloured
band, a row of riveted steel columns, a yellow edge strip, the track bed below with its rails,
station-name panels across the tracks, a dark tunnel mouth at the far end. Fluorescent strips
over the platform, one failing.
Light passes: base (almost nothing - a little light from the stairs behind), near, mid, far
(the strips by distance), failing (a strip that flickers with the hum), signs (the lit name
panels and route signs), tunnel (the signal lamps in the tunnel), train (a headlight deep in
the tunnel: Approaching).
"""

import math

import common as c

L = 64.0
PH = 3.4      # platform to ceiling
TRACK = -1.15  # track bed below the platform


def build():
    scene = c.reset()
    c.world_colour((0.6, 0.65, 0.7), 0.2)
    wall = c.tiles("subway tile", (0.84, 0.84, 0.8), (0.45, 0.45, 0.42), size=0.15, height=0.075, offset=0.5, grout_width=0.003, roughness=0.1, variation=0.05, glossy=0.6, bump=0.2)
    band = c.tiles("band tile", (0.22, 0.46, 0.42), (0.3, 0.32, 0.3), size=0.15, height=0.075, offset=0.5, grout_width=0.003, roughness=0.1, variation=0.08, glossy=0.6, bump=0.2)
    platform = c.tiles("platform", (0.5, 0.49, 0.46), (0.3, 0.3, 0.28), size=0.3, grout_width=0.004, roughness=0.35, variation=0.12, glossy=0.45, bump=0.15)
    edge = c.matte("edge strip", (0.78, 0.62, 0.1), roughness=0.5, noise_scale=6, noise_amount=0.25, bump=0.3)
    ballast = c.matte("ballast", (0.12, 0.11, 0.1), roughness=0.95, noise_scale=40, noise_amount=0.5, bump=1.0)
    sleeper = c.matte("sleeper", (0.16, 0.14, 0.12), roughness=0.9)
    rail = c.metal("rail", (0.55, 0.5, 0.46), roughness=0.22)
    column = c.matte("column paint", (0.2, 0.3, 0.3), roughness=0.4, noise_scale=3, noise_amount=0.15, specular=0.6)
    ceiling = c.matte("ceiling", (0.6, 0.6, 0.57), roughness=0.85, noise_scale=0.3, noise_amount=0.2, stains=0.15)
    tunnel = c.matte("tunnel concrete", (0.18, 0.18, 0.17), roughness=0.9, noise_scale=0.5, noise_amount=0.3)
    housing = c.matte("fixture", (0.3, 0.3, 0.3), roughness=0.5)
    bench_wood = c.matte("bench", (0.35, 0.22, 0.12), roughness=0.5, noise_scale=12, noise_amount=0.2)

    # Platform (x 0..6), track bed (x -4.6..0), far wall at -4.6, near wall at 6.
    c.box("platform", (0, -4, -0.1), (6, L, 0), platform)
    c.box("platform edge", (-0.05, -4, -0.12), (0.6, L, 0.003), edge)
    c.box("platform face", (-0.06, -4, TRACK), (0, L, -0.05), tunnel)
    c.box("track bed", (-4.6, -4, TRACK - 0.2), (0, L + 40, TRACK), ballast)
    for k in range(int((L + 40 + 4) / 0.65)):
        y = -4 + 0.65 * k
        c.box(f"sleeper {k}", (-3.6, y, TRACK), (-1.0, y + 0.22, TRACK + 0.12), sleeper)
    for x in (-3.05, -1.55):
        c.box(f"rail {x}", (x - 0.035, -4, TRACK + 0.12), (x + 0.035, L + 40, TRACK + 0.27), rail)
    c.box("third rail", (-0.75, -4, TRACK + 0.2), (-0.65, L + 40, TRACK + 0.32), rail)
    # Walls: tiled, with a coloured band at head height, from track bed to ceiling.
    c.box("far wall", (-4.9, -4, TRACK), (-4.6, L, PH), wall)
    c.box("far band", (-4.61, -4, 1.6), (-4.6, L, 1.9), band)
    c.box("near wall", (6, -4, 0), (6.3, L, PH), wall)
    c.box("near band", (5.99, -4, 1.6), (6.0, L, 1.9), band)
    c.box("back wall", (-4.9, -4.3, TRACK), (6.3, -4, PH), wall)
    c.box("ceiling", (-4.9, -4.3, PH), (6.3, L, PH + 0.3), ceiling)
    # The far end: the platform stops at a tiled wall; the tracks go on into the tunnel mouth.
    c.box("end wall platform", (0, L, 0), (6.3, L + 0.3, PH), wall)
    c.box("end wall over tunnel", (-4.9, L, 2.1), (0, L + 0.3, PH), wall)
    c.box("tunnel left", (-4.9, L, TRACK), (-4.4, L + 40, 2.1), tunnel)
    c.box("tunnel right", (-0.2, L, TRACK), (0.3, L + 40, 2.1), tunnel)
    c.box("tunnel roof", (-4.9, L, 2.1), (0.3, L + 40, 2.5), tunnel)
    c.box("tunnel end", (-4.9, L + 40, TRACK), (0.3, L + 40.3, 2.5), tunnel)
    # Riveted steel columns down the platform, benches between some of them.
    for k, y in enumerate(range(2, int(L), 6)):
        c.box(f"column {k}", (2.3, y - 0.15, 0), (2.6, y + 0.15, PH), column)
        c.box(f"column flange {k}", (2.25, y - 0.2, 0), (2.65, y - 0.16, PH), column)
        c.box(f"column flange b {k}", (2.25, y + 0.16, 0), (2.65, y + 0.2, PH), column)
        if k % 2 == 1:
            c.box(f"bench {k}", (5.2, y - 1.0, 0.42), (5.7, y + 1.0, 0.47), bench_wood)
            c.box(f"bench back {k}", (5.7, y - 1.0, 0.47), (5.75, y + 1.0, 0.9), bench_wood)
            for e in (-0.9, 0.9):
                c.box(f"bench leg {k} {e}", (5.3, y + e - 0.03, 0), (5.6, y + e + 0.03, 0.42), housing)
    # Station-name panels across the tracks (dark lettering band on a lit panel: the signs pass).
    panel = c.emitter("name panel", (0.92, 0.94, 0.9), 1.6)
    lettering = c.matte("panel lettering", (0.03, 0.03, 0.03), roughness=0.5)
    for k, y in enumerate((8.0, 26.0, 44.0)):
        c.box(f"panel frame {k}", (-4.6, y - 1.9, 1.75), (-4.52, y + 1.9, 2.45), housing)
        p = c.plane(f"panel {k}", (-4.51, y, 2.1), (3.6, 0.6), panel, rotation=(math.pi / 2, 0, math.pi / 2))
        c.in_group("signs", p)
        for j in range(7):   # blocks of "lettering" (no real name)
            c.box(f"letter {k} {j}", (-4.508, y - 1.4 + 0.42 * j, 1.98), (-4.5, y - 1.1 + 0.42 * j, 2.22), lettering)
    route = c.emitter("route sign", (0.95, 0.75, 0.15), 3.0)
    for k, y in enumerate((14.0, 38.0)):
        c.box(f"route box {k}", (1.2, y - 0.6, PH - 0.55), (1.35, y + 0.6, PH - 0.2), housing)
        p = c.plane(f"route sign {k}", (1.19, y, PH - 0.375), (1.1, 0.28), route, rotation=(math.pi / 2, 0, -math.pi / 2))
        c.in_group("signs", p)
        c.box(f"route hanger {k}", (1.26, y - 0.02, PH - 0.2), (1.29, y + 0.02, PH), housing)
    # Fluorescent strips: two continuous runs over the platform, in 4 m fixtures.
    colour = (0.88, 0.97, 1.0)
    for run_x in (0.9, 4.4):
        for k in range(int(L / 4)):
            y0 = 4.0 * k + 0.2
            group = "failing" if (run_x, k) == (0.9, 4) else "near" if y0 < 12 else "mid" if y0 < 32 else "far"
            c.box(f"strip housing {run_x} {k}", (run_x - 0.08, y0, PH - 0.06), (run_x + 0.08, y0 + 3.6, PH), housing)
            c.tube(f"strip {run_x} {k}", group, (run_x, y0 + 0.1, PH - 0.09), y0 + 3.5, colour, 48.0, glow=4.0)
    # Signal lamps in the tunnel (red), and a train's headlight far down it.
    red = c.emitter("signal red", (1.0, 0.08, 0.04), 30.0)
    for k, y in enumerate((L + 4.0, L + 11.0)):
        c.box(f"signal head {k}", (-0.15, y - 0.12, 1.2), (0.0, y + 0.12, 1.75), housing)
        s = c.cylinder(f"signal lamp {k}", (-0.18, y, 1.6), 0.06, 0.02, red, segments=16, axis='x')
        c.in_group("tunnel", s)
        c.point_light(f"signal glow {k}", "tunnel", (-0.3, y, 1.6), 6.0, (1.0, 0.1, 0.05), radius=0.05)
    head = c.emitter("train headlight", (1.0, 0.97, 0.9), 80.0)
    for x in (-3.0, -1.6):
        b = c.cylinder(f"train lamp {x}", (x, L + 16.0, 0.4), 0.12, 0.02, head, segments=16, axis='y')
        c.in_group("train", b)
        c.spot_light(f"train beam {x}", "train", (x, L + 15.8, 0.4), (-math.pi / 2, 0, 0), 3500.0, (1.0, 0.96, 0.88), math.radians(50), radius=0.1, blend=0.7)
    # A little light from the stairs behind the camera (the base pass).
    c.area_light("stairs light", "base", (4.5, -3.8, 2.4), (2.5, 1.5), 60.0, (0.9, 0.92, 0.95), rotation=(math.radians(-90), 0, 0))
    c.camera((3.6, -3.0, 1.6), (0.6, L, 1.2), lens=24.0, shift_y=0.0)
    return scene, None, None


BASE_DEFAULT = 1.0

PASSES = [
    {"name": "near", "group": "near", "kind": "tube", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "tube", "default": 1.0},
    {"name": "far", "group": "far", "kind": "tube", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
    {"name": "signs", "group": "signs", "kind": "sign", "default": 1.0},
    {"name": "tunnel", "group": "tunnel", "kind": "sign", "default": 1.0},
    {"name": "train", "group": "train", "kind": "lamp", "default": 1.0},
]

# Last Train: every strip and sign on, the signals red. Abandoned: closed for years - most strips
# dead, the signs dark, one run still flickering. Approaching: a light growing in the tunnel.
CHARACTERS = {
    "base": [1.0, 0.6, 1.0],
    "near": [1.0, 0.0, 0.75],
    "mid": [1.0, 0.35, 0.7],
    "far": [1.0, 0.0, 0.5],
    "failing": [1.0, 1.0, 1.0],
    "signs": [1.0, 0.0, 0.6],
    "tunnel": [1.0, 0.0, 1.0],
    "train": [0.0, 0.0, 1.0],
}
FLICKER = {"mid": [False, True, False]}
