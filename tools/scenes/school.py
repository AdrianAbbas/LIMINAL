"""ABANDONED SCHOOL: a long hallway of lockers under yellowed fluorescent light - polished vinyl
tile floor, classroom doors with small wired-glass windows, bulletin boards of faded paper, a
wall clock over the far doors. One classroom door stands open, afternoon sun inside.
Light passes: base (daylight through the far doors and the classroom windows), near, mid, far
(the ceiling fixtures by distance), failing (one that flickers with the hum), classroom (the
open room's low sun spilling into the hall), exit (the red sign over the far doors).
"""

import math

import common as c

H = 3.0
W = 1.7      # half width
END = 30.0


def build():
    scene = c.reset()
    c.world_colour((0.7, 0.75, 0.82), 0.5)
    floor = c.tiles("vinyl tile", (0.62, 0.58, 0.5), (0.42, 0.4, 0.35), size=0.3, grout_width=0.002, roughness=0.12, variation=0.16, glossy=0.55, bump=0.05)
    block = c.tiles("painted block", (0.82, 0.8, 0.7), (0.66, 0.64, 0.56), size=0.4, grout_width=0.008, roughness=0.6, variation=0.03, glossy=0.35, bump=0.3)
    ceiling = c.tiles("acoustic tiles", (0.8, 0.78, 0.7), (0.5, 0.48, 0.43), size=0.6, grout_width=0.012, roughness=0.9, variation=0.05, glossy=0.2, bump=0.4)
    locker_paint = [c.matte(f"locker paint {i}", col, roughness=0.38, noise_scale=1.4, noise_amount=0.1, specular=0.6)
                    for i, col in enumerate([(0.2, 0.36, 0.42), (0.22, 0.38, 0.44), (0.19, 0.34, 0.4)])]
    slot = c.matte("locker gap", (0.02, 0.02, 0.02), roughness=0.8)
    vent = c.ribbed("locker vents", (0.16, 0.3, 0.35), pitch=0.025, roughness=0.5, metallic=0.3)
    wood = c.matte("door wood", (0.5, 0.34, 0.2), roughness=0.4, noise_scale=8.0, noise_amount=0.2, specular=0.5)
    frame = c.metal("door frame", (0.45, 0.42, 0.38), roughness=0.4)
    glass = c.dark_glass("wired glass", tint=(0.04, 0.045, 0.04), roughness=0.08)
    cork = c.matte("cork", (0.45, 0.3, 0.17), roughness=0.9, noise_scale=30, noise_amount=0.25)
    papers = [c.matte(f"paper {i}", col, roughness=0.85, noise_scale=3, noise_amount=0.15)
              for i, col in enumerate([(0.85, 0.82, 0.68), (0.78, 0.66, 0.52), (0.7, 0.76, 0.72), (0.86, 0.72, 0.62), (0.74, 0.74, 0.62)])]
    base_trim = c.matte("cove base", (0.12, 0.1, 0.08), roughness=0.5)
    face = c.matte("clock face", (0.92, 0.9, 0.84), roughness=0.4)
    black = c.matte("clock hands", (0.03, 0.03, 0.03), roughness=0.4)
    room = c.matte("classroom", (0.72, 0.66, 0.55), roughness=0.8)

    c.box("floor", (-W - 6, -3, -0.1), (W + 0.3, END + 0.3, 0), floor)
    c.box("ceiling", (-W - 0.3, -3, H), (W + 0.3, END + 0.3, H + 0.1), ceiling)
    c.box("near wall", (-W - 0.3, -3.2, 0), (W + 0.3, -3, H), block)
    doors = {-1: [3.5, 9.0, 20.5], 1: [7.0, 16.5, 25.0]}
    OPEN = (-1, 9.0)   # this classroom's door stands open
    for side in (-1, 1):
        xw = side * W
        # Wall: painted block above the lockers; cut where the classroom doors are.
        cuts = sorted(doors[side])
        y = -3.0
        for d in cuts + [END + 0.01]:
            y1 = d - 0.55 if d <= END else END
            if y1 > y:
                c.box(f"wall {side} {y}", (min(xw, xw + side * 0.2), y, 0), (max(xw, xw + side * 0.2), y1, H), block)
            y = d + 0.55
        for d in cuts:   # over each door
            c.box(f"wall over door {side} {d}", (min(xw, xw + side * 0.2), d - 0.55, 2.25), (max(xw, xw + side * 0.2), d + 0.55, H), block)
        c.box(f"cove {side}", (min(xw - side * 0.01, xw + side * 0.01), -3, 0), (max(xw - side * 0.01, xw + side * 0.01), END, 0.1), base_trim)
        # Lockers: banks between the doors, 0.38 m wide, 1.85 m high, a dark gap between doors.
        y = -2.6
        k = 0
        while y + 0.38 < END - 0.6:
            if any(abs(y + 0.19 - d) < 1.4 for d in doors[side]) or 10.0 < y < 12.2 and side == 1:
                y += 0.38
                continue
            x0, x1 = sorted((xw, xw - side * 0.32))
            c.box(f"locker {side} {k}", (x0, y + 0.006, 0.12), (x1, y + 0.374, 1.97), locker_paint[(k * 7) % 3])
            c.box(f"locker vent {side} {k}", (min(xw - side * 0.322, xw - side * 0.326), y + 0.08, 1.62), (max(xw - side * 0.322, xw - side * 0.326), y + 0.3, 1.8), vent)
            c.box(f"locker gap {side} {k}", (x0, y - 0.006, 0.12), (x1, y + 0.006, 1.97), slot)
            c.box(f"locker handle {side} {k}", (min(xw - side * 0.322, xw - side * 0.34), y + 0.3, 1.0), (max(xw - side * 0.322, xw - side * 0.34), y + 0.33, 1.14), frame)
            y += 0.38
            k += 1
        c.box(f"locker top {side}", (min(xw, xw - side * 0.32), -2.6, 1.97), (max(xw, xw - side * 0.32), END - 0.6, 2.0), locker_paint[0])
        c.box(f"locker plinth {side}", (min(xw, xw - side * 0.3), -2.6, 0), (max(xw, xw - side * 0.3), END - 0.6, 0.12), base_trim)
        # A bulletin board over the lockers between two doors, with faded sheets of paper.
        if side == 1:
            yb = 11.1
            c.box("board", (W - 0.05, yb - 1.2, 1.2), (W, yb + 1.2, 2.1), cork)
            for j in range(9):
                py = yb - 1.0 + (j % 5) * 0.42 + 0.05 * (j % 2)
                pz = 1.35 + (j // 5) * 0.38 + 0.04 * (j % 3)
                c.plane(f"paper {j}", (W - 0.056, py, pz), (0.21, 0.28), papers[j % 5], rotation=(math.pi / 2, 0, math.pi / 2 + 0.03 * (j % 3 - 1)))
        # Classroom doors: a frame, a wooden door with a tall narrow window.
        for d in doors[side]:
            c.box(f"door frame {side} {d}", (min(xw, xw + side * 0.22), d - 0.55, 0), (max(xw, xw + side * 0.22), d + 0.55, 2.25), frame)
            if (side, d) == OPEN:
                # Swung open into the hall, flat against the lockers' end.
                c.box("open door", (min(xw - side * 0.05, xw - side * 0.9), d + 0.45, 0.02), (max(xw - side * 0.05, xw - side * 0.9), d + 0.5, 2.15), wood)
                c.box("door hole", (min(xw, xw + side * 0.25), d - 0.47, 0.0), (max(xw, xw + side * 0.25), d + 0.47, 2.2), None)
                continue
            c.box(f"door {side} {d}", (min(xw + side * 0.08, xw + side * 0.13), d - 0.47, 0.02), (max(xw + side * 0.08, xw + side * 0.13), d + 0.47, 2.18), wood)
            c.box(f"door window {side} {d}", (min(xw + side * 0.07, xw + side * 0.075), d - 0.32, 1.1), (max(xw + side * 0.07, xw + side * 0.075), d - 0.12, 1.85), glass)
    # Remove the frame's filling where the open door is: the frame was built solid above, so
    # make the opening by building the frame as two jambs and a head instead.
    import bpy
    bpy.data.objects.remove(scene.objects[f"door frame {OPEN[0]} {OPEN[1]}"], do_unlink=True)
    bpy.data.objects.remove(scene.objects["door hole"], do_unlink=True)
    xw, d = OPEN[0] * W, OPEN[1]
    for a, b in (((d - 0.55, d - 0.47)), ((d + 0.47, d + 0.55))):
        c.box(f"open jamb {a}", (min(xw, xw + OPEN[0] * 0.22), a, 0), (max(xw, xw + OPEN[0] * 0.22), b, 2.25), frame)
    c.box("open head", (min(xw, xw + OPEN[0] * 0.22), d - 0.55, 2.18), (max(xw, xw + OPEN[0] * 0.22), d + 0.55, 2.25), frame)
    # The classroom behind it: a room with a row of windows, low sun coming in.
    rx0, rx1 = -W - 6.0, -W - 0.2
    c.box("classroom ceiling", (rx0, d - 4, H), (rx1, d + 4, H + 0.1), ceiling)
    c.box("classroom back", (rx0 - 0.2, d - 4, 0), (rx0, d + 4, H), room)
    c.box("classroom side a", (rx0, d - 4.2, 0), (rx1, d - 4, H), room)
    c.box("classroom side b", (rx0, d + 4, 0), (rx1, d + 4.2, H), room)
    sun_glow = c.emitter("classroom windows", (1.0, 0.86, 0.62), 6.0)
    for k in range(3):
        p = c.plane(f"classroom window {k}", (rx0 + 0.01, d - 2.5 + 2.5 * k, 1.8), (1.6, 1.4), sun_glow, rotation=(math.pi / 2, 0, math.pi / 2))
        c.in_group("classroom", p)
    c.area_light("classroom sun", "classroom", (rx0 + 0.3, d, 1.9), (1.5, 6.0), 1800.0, (1.0, 0.8, 0.55), rotation=(0, math.radians(-70), 0))
    # A shaft of low sun through the open door, across the hall floor and onto the lockers.
    from mathutils import Vector
    aim = Vector((0.9, d + 1.2, 0.0)) - Vector((rx0 + 0.6, d - 0.6, 2.4))
    c.spot_light("sun through the door", "classroom", (rx0 + 0.6, d - 0.6, 2.4), aim.to_track_quat('-Z', 'Y').to_euler(), 3500.0, (1.0, 0.78, 0.5), math.radians(14), radius=0.05, blend=0.25)
    for k in range(4):   # desks in rows, just visible through the door
        for j in range(2):
            c.box(f"desk {k} {j}", (rx1 - 1.2 - 1.2 * k, d - 1.5 + 2.0 * j, 0.7), (rx1 - 0.6 - 1.2 * k, d - 0.9 + 2.0 * j, 0.74), wood)
            c.box(f"desk leg {k} {j}", (rx1 - 0.92 - 1.2 * k, d - 1.22 + 2.0 * j, 0), (rx1 - 0.88 - 1.2 * k, d - 1.18 + 2.0 * j, 0.7), frame)

    # The far end: double doors with windows (daylight beyond), the clock and the exit sign above.
    c.box("end wall", (-W - 0.3, END, 0), (W + 0.3, END + 0.3, H), block)
    c.box("end frame", (-1.1, END - 0.05, 0), (1.1, END - 0.02, 2.3), frame)
    for x0, x1 in ((-1.0, -0.03), (0.03, 1.0)):
        c.box(f"end door {x0}", (x0, END - 0.08, 0.02), (x1, END - 0.05, 2.2), frame)
    daylight = c.emitter("daylight beyond", (0.8, 0.86, 0.95), 2.6)
    for x in (-0.5, 0.5):
        pane = c.plane(f"end window {x}", (x, END - 0.085, 1.45), (0.55, 1.1), daylight, rotation=(math.pi / 2, 0, 0))
        c.in_group("base", pane)
    c.area_light("daylight spill", "base", (0, END - 0.25, 1.4), (1.8, 1.2), 45.0, (0.8, 0.86, 0.95), rotation=(-math.pi / 2, 0, 0))
    # The wall clock over the doors.
    c.cylinder("clock rim", (0.0, END - 0.02, 2.62), 0.2, -0.06, frame, segments=40, axis='y')
    c.cylinder("clock face", (0.0, END - 0.075, 2.62), 0.18, -0.005, face, segments=40, axis='y')
    c.box("clock hour", (-0.008, END - 0.09, 2.62), (0.008, END - 0.085, 2.72), black)
    c.box("clock minute", (0.0, END - 0.092, 2.616), (0.15, END - 0.087, 2.624), black)
    sign = c.emitter("exit sign", (1.0, 0.12, 0.08), 12.0)
    c.box("exit housing", (-0.98, END - 0.12, 2.42), (-0.48, END - 0.02, 2.6), frame)
    f = c.plane("exit face", (-0.73, END - 0.125, 2.51), (0.44, 0.13), sign, rotation=(math.pi / 2, 0, 0))
    c.in_group("exit", f)
    c.point_light("exit light", "exit", (-0.73, END - 0.35, 2.45), 5.0, (1.0, 0.2, 0.12), radius=0.1)

    # The ceiling fixtures: two rows, every 2.4 m, yellowed tubes.
    colour = (1.0, 0.9, 0.7)
    for k in range(12):
        y = 0.6 + 2.4 * k
        group = "failing" if k == 5 else "near" if y < 7.0 else "mid" if y < 17.0 else "far"
        c.ceiling_panel(f"fixture {k}", group, (0.0, y), (0.6, 1.2), H, 14.0, colour, glow=3.6)
    c.camera((0.2, -2.3, 1.6), (0.2, END, 1.6), lens=26.0, shift_y=-0.02)
    return scene, None, None


BASE_DEFAULT = 1.0

PASSES = [
    {"name": "near", "group": "near", "kind": "tube", "default": 1.0},
    {"name": "mid", "group": "mid", "kind": "tube", "default": 1.0},
    {"name": "far", "group": "far", "kind": "tube", "default": 1.0},
    {"name": "failing", "group": "failing", "kind": "failing", "default": 1.0},
    {"name": "classroom", "group": "classroom", "kind": "sun", "default": 1.0},
    {"name": "exit", "group": "exit", "kind": "sign", "default": 1.0},
]

# After Class: the tubes on, a little sun from the open classroom. Childhood Memory: a warm
# afternoon - the tubes off, low sun pouring out of the classroom, the hall glowing.
# Nightmare: night; the hall dark but for the far end, the middle tubes stuttering with the hum.
CHARACTERS = {
    "base": [1.0, 1.4, 0.05],
    "near": [1.0, 0.1, 0.0],
    "mid": [1.0, 0.15, 0.6],
    "far": [1.0, 0.3, 1.0],
    "failing": [1.0, 0.0, 1.0],
    "classroom": [0.35, 3.0, 0.0],
    "exit": [1.0, 0.6, 1.8],
}
FLICKER = {"mid": [False, False, True]}
