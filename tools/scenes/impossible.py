"""IMPOSSIBLE ROOM: a pastel room lit evenly from nowhere - no shadows, no visible lamps. A
staircase climbs into the wall and stops; a door is set in the ceiling, ajar, light behind it;
another door hangs high on the far wall with nothing to reach it; a smaller room stands inside
the room; and through the far doorway, the same room again.
Light passes: base (the even, shadowless light), ceiling (the light behind the ceiling door),
stairs (light at the top of the stairs, from no source), inner (the room inside the room, its
light flickering with the hum), beyond (the next room, the same room, warmer).
"""

import math

import common as c

W = 6.0     # half width
D = 15.0    # depth
H = 4.6


def build():
    scene = c.reset()
    c.world_colour((1.0, 1.0, 1.0), 0.0)
    plaster = c.matte("pastel plaster", (0.86, 0.76, 0.74), roughness=0.9, noise_scale=0.25, noise_amount=0.03)
    plaster_b = c.matte("pastel plaster b", (0.62, 0.7, 0.8), roughness=0.9, noise_scale=0.25, noise_amount=0.03)
    floor = c.tiles("pale floor", (0.86, 0.84, 0.8), (0.74, 0.72, 0.68), size=0.5, grout_width=0.002, roughness=0.3, variation=0.02, glossy=0.4, bump=0.03)
    trim = c.matte("white trim", (0.92, 0.92, 0.9), roughness=0.5)
    door = c.matte("door", (0.9, 0.88, 0.84), roughness=0.45)
    stair = c.matte("stair", (0.9, 0.86, 0.82), roughness=0.7)

    def room(prefix, y0, colour_wall):
        """The room: floor, walls, ceiling; a doorway in the far wall into the next one."""
        c.box(f"{prefix} floor", (-W, y0, -0.1), (W, y0 + D, 0), floor)
        c.box(f"{prefix} ceiling", (-W, y0, H), (W, y0 + D, H + 0.1), plaster)
        c.box(f"{prefix} left", (-W - 0.2, y0, 0), (-W, y0 + D, H), colour_wall)
        c.box(f"{prefix} right", (W, y0, 0), (W + 0.2, y0 + D, H), colour_wall)
        # Far wall with a centred doorway (2.2 x 3.0) into the same room again.
        c.box(f"{prefix} far l", (-W, y0 + D, 0), (-1.1, y0 + D + 0.3, H), colour_wall)
        c.box(f"{prefix} far r", (1.1, y0 + D, 0), (W, y0 + D + 0.3, H), colour_wall)
        c.box(f"{prefix} far head", (-1.1, y0 + D, 3.0), (1.1, y0 + D + 0.3, H), colour_wall)
        for side in (-1, 1):
            c.box(f"{prefix} skirting {side}", (min(side * W, side * (W - 0.02)), y0, 0), (max(side * W, side * (W - 0.02)), y0 + D, 0.12), trim)

    room("room", 0.0, plaster)
    room("room again", D + 0.3, plaster)   # through the doorway: the same room
    c.box("room again end", (-W - 0.2, 2 * D + 0.6, 0), (W + 0.2, 2 * D + 0.9, H), plaster)
    c.box("near wall", (-W - 0.2, -3.2, 0), (W + 0.2, -3.0, H), plaster)
    c.box("near floor", (-W, -3.0, -0.1), (W, 0.0, 0), floor)
    c.box("near ceiling", (-W, -3.0, H), (W, 0.0, H + 0.1), plaster)
    c.box("near left", (-W - 0.2, -3.0, 0), (-W, 0, H), plaster)
    c.box("near right", (W, -3.0, 0), (W + 0.2, 0, H), plaster)

    # Even, shadowless light: the whole ceiling glows faintly, and so do the walls' tops (base).
    glow = c.emitter("light from nowhere", (1.0, 0.97, 0.95), 0.9)
    for y0 in (-3.0, 0.0, D + 0.3):
        p = c.plane(f"ceiling glow {y0}", (0, y0 + (3.0 if y0 < 0 else D) / 2, H - 0.01), (2 * W, 3.0 if y0 < 0 else D), glow, rotation=(math.pi, 0, 0))
        p.visible_camera = False
        p.visible_glossy = False
        p.visible_shadow = False
        c.in_group("base", p)
    c.area_light("even fill", "base", (0, D / 2, 0.05), (2 * W, D), 220.0, (1.0, 0.97, 0.95), rotation=(math.pi, 0, 0))

    stairs_to_nowhere(0.0, "stairs")
    stairs_to_nowhere(D + 0.3, "beyond")   # the same stairs, in the same room again

    # A door in the ceiling, ajar: frame, a leaf hanging open downward, light from above it.
    cx, cy = 1.6, 7.5
    c.box("ceiling door frame a", (cx - 0.5, cy - 1.05, H - 0.05), (cx + 0.5, cy - 0.95, H), trim)
    c.box("ceiling door frame b", (cx - 0.5, cy + 0.95, H - 0.05), (cx + 0.5, cy + 1.05, H), trim)
    c.box("ceiling door frame c", (cx - 0.55, cy - 1.05, H - 0.05), (cx - 0.45, cy + 1.05, H), trim)
    c.box("ceiling door frame d", (cx + 0.45, cy - 1.05, H - 0.05), (cx + 0.55, cy + 1.05, H), trim)
    a = math.radians(28)
    hz = H - 0.02
    c.hexa("ceiling door", [(cx - 0.44, cy - 0.94, hz), (cx + 0.44, cy - 0.94, hz), (cx + 0.44, cy - 0.94 + 1.88 * math.cos(a), hz - 1.88 * math.sin(a)), (cx - 0.44, cy - 0.94 + 1.88 * math.cos(a), hz - 1.88 * math.sin(a)),
                           (cx - 0.44, cy - 0.94, hz + 0.04), (cx + 0.44, cy - 0.94, hz + 0.04), (cx + 0.44, cy - 0.94 + 1.88 * math.cos(a), hz - 1.88 * math.sin(a) + 0.04), (cx - 0.44, cy - 0.94 + 1.88 * math.cos(a), hz - 1.88 * math.sin(a) + 0.04)], door)
    # (the ceiling under the door is cut: the ceiling glow and slab have a hole there)
    hole_cut(scene, cx, cy)
    c.box("above the door", (cx - 0.6, cy - 1.1, H + 1.2), (cx + 0.6, cy + 1.1, H + 1.3), plaster)
    for side in (-1, 1):
        c.box(f"above walls {side}", (cx + side * 0.55 - 0.05, cy - 1.1, H), (cx + side * 0.55 + 0.05, cy + 1.1, H + 1.3), plaster)
    c.box("above walls f", (cx - 0.6, cy - 1.15, H), (cx + 0.6, cy - 1.05, H + 1.3), plaster)
    c.box("above walls b", (cx - 0.6, cy + 1.05, H), (cx + 0.6, cy + 1.15, H + 1.3), plaster)
    c.area_light("light above the ceiling door", "ceiling", (cx, cy, H + 1.15), (0.9, 1.9), 260.0, (1.0, 0.9, 0.75))

    # A door high on the far wall with nothing leading to it.
    c.box("high door frame", (-4.0, D - 0.04, 2.3), (-2.8, D, 4.4), trim)
    c.box("high door", (-3.92, D - 0.06, 2.35), (-2.88, D - 0.04, 4.35), door)
    c.box("high door knob", (-3.05, D - 0.1, 3.3), (-2.98, D - 0.06, 3.37), trim)

    inner_room(0.0, "inner")
    inner_room(D + 0.3, "beyond")

    # The next room (the same room) has its own even light, warmer: "beyond".
    c.area_light("beyond fill", "beyond", (0, D + 0.3 + D / 2, H - 0.05), (2 * W, D), 420.0, (1.0, 0.86, 0.7))
    c.camera((0.6, -2.6, 1.6), (0.2, D, 1.85), lens=24.0, shift_y=0.0)
    return scene, None, None


def stairs_to_nowhere(oy, group):
    stair = bpy_mat("stair")
    trim = bpy_mat("white trim")
    # A staircase on the left, climbing sideways into the left wall and ending against it: nowhere.
    run, rise = 0.35, 0.18
    sy0, sy1 = 5.0 + oy, 6.3 + oy
    for k in range(14):
        x = -1.2 - (k + 1) * run
        c.box(f"stair {k} {oy}", (x, sy0, 0), (x + run + 0.01, sy1, (k + 1) * rise), stair)
    c.box(f"stair cheek {oy}", (-1.2 - 14 * run, sy0 - 0.05, 0), (-1.2, sy0, 0.08), trim)
    top = -1.2 - 14 * run
    c.area_light(f"stair top light {oy}", group, (top + 0.4, (sy0 + sy1) / 2, 14 * rise + 0.7), (0.6, 1.2), 140.0, (1.0, 0.93, 0.85))
    c.area_light(f"stair top glow {oy}", group, (-W + 0.05, (sy0 + sy1) / 2, 14 * rise + 0.5), (1.2, 1.0), 70.0, (1.0, 0.93, 0.85), rotation=(0, -math.pi / 2, 0))



def inner_room(oy, group):
    plaster_b = bpy_mat("pastel plaster b")
    # The room inside the room: a small pastel-blue box with a doorway, its own light inside.
    ix0, ix1, iy0, iy1, ih = 2.2, 5.2, 8.0 + oy, 10.5 + oy, 2.5
    c.box(f"inner left {oy}", (ix0, iy0, 0), (ix0 + 0.1, iy1, ih), plaster_b)
    c.box(f"inner right {oy}", (ix1 - 0.1, iy0, 0), (ix1, iy1, ih), plaster_b)
    c.box(f"inner back {oy}", (ix0, iy1 - 0.1, 0), (ix1, iy1, ih), plaster_b)
    c.box(f"inner roof {oy}", (ix0, iy0, ih), (ix1, iy1, ih + 0.1), plaster_b)
    c.box(f"inner front l {oy}", (ix0, iy0, 0), (3.3, iy0 + 0.1, ih), plaster_b)
    c.box(f"inner front r {oy}", (4.3, iy0, 0), (ix1, iy0 + 0.1, ih), plaster_b)
    c.box(f"inner front head {oy}", (3.3, iy0, 2.0), (4.3, iy0 + 0.1, ih), plaster_b)
    c.area_light(f"inner light {oy}", group, ((ix0 + ix1) / 2, (iy0 + iy1) / 2, ih - 0.05), (2.6, 2.2), 120.0, (0.9, 0.95, 1.0))



def bpy_mat(name):
    import bpy
    return bpy.data.materials[name]


def hole_cut(scene, cx, cy):
    """Open the ceiling over the door: rebuild the room's ceiling as four slabs around the hole."""
    import bpy
    for name in ("room ceiling",):
        obj = scene.objects[name]
        mat = obj.data.materials[0]
        bpy.data.objects.remove(obj, do_unlink=True)
        x0, x1, y0, y1 = cx - 0.45, cx + 0.45, cy - 0.95, cy + 0.95
        c.box("ceiling a", (-W, 0.0, H), (W, y0, H + 0.1), mat)
        c.box("ceiling b", (-W, y1, H), (W, D, H + 0.1), mat)
        c.box("ceiling c", (-W, y0, H), (x0, y1, H + 0.1), mat)
        c.box("ceiling d", (x1, y0, H), (W, y1, H + 0.1), mat)


BASE_DEFAULT = 1.0

PASSES = [
    {"name": "ceiling", "group": "ceiling", "kind": "lamp", "default": 1.0},
    {"name": "stairs", "group": "stairs", "kind": "lamp", "default": 1.0},
    {"name": "inner", "group": "inner", "kind": "lamp", "default": 1.0},
    {"name": "beyond", "group": "beyond", "kind": "lamp", "default": 1.0},
]

# Familiar: almost a normal room - the even light, the room beyond. Unstable: nothing holds still
# - the inner room's light and the ceiling door's flicker with the hum. Beyond Reality: the room
# itself dims away; only the light from the ceiling door, the stairs and the next room remain.
CHARACTERS = {
    "base": [1.0, 0.85, 0.2],
    "ceiling": [0.5, 1.0, 1.8],
    "stairs": [0.3, 0.8, 1.6],
    "inner": [0.4, 1.0, 0.0],
    "beyond": [1.0, 1.2, 2.0],
}
FLICKER = {"inner": [True, True, False], "ceiling": [False, True, True]}
