"""Shared helpers for LIMINAL's scripted Blender scenes (Direction Update 2, Section 3).

Every scene is built from nothing by a Python script (procedural geometry and materials, no
downloaded assets), rendered with Cycles in linear light as separate *light passes*:

    base      the world/ambient light only, every lamp off
    <group>   one light group alone (its lamps and its glowing fixtures), world off

Because light adds linearly, the plug-in can composite base + sum(intensity_i * pass_i) and so
dim, brighten and flicker every group independently in real time. See tools/scenes/README.md.
"""

import json
import math
import os

import bpy
from mathutils import Vector

# The editor's canvas is 960 x 720 (4:3); renders are 1.5x that, sharp enough for Retina
# after the plug-in's grain and halation.
WIDTH, HEIGHT = 1440, 1080


# --- scene -----------------------------------------------------------------------------------

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.render.resolution_x = WIDTH
    scene.render.resolution_y = HEIGHT
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    c = scene.cycles
    c.use_denoising = True
    try:
        c.denoiser = 'OPENIMAGEDENOISE'
    except TypeError:
        pass
    c.max_bounces = 8
    c.diffuse_bounces = 4
    c.glossy_bounces = 4
    c.transmission_bounces = 8
    c.volume_bounces = 1
    c.transparent_max_bounces = 8
    c.caustics_reflective = False
    c.caustics_refractive = False
    c.sample_clamp_indirect = 4.0
    c.use_adaptive_sampling = True
    c.adaptive_threshold = 0.02
    world = bpy.data.worlds.new("World")
    scene.world = world
    world.use_nodes = True
    return scene


def camera(location, look_at, lens=26.0, shift_y=0.0):
    data = bpy.data.cameras.new("Camera")
    data.lens = lens
    data.sensor_width = 36.0
    data.shift_y = shift_y
    data.clip_end = 300.0
    obj = bpy.data.objects.new("Camera", data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    direction = Vector(look_at) - Vector(location)
    obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = obj
    return obj


def world_colour(rgb, strength):
    nodes = bpy.context.scene.world.node_tree.nodes
    bg = nodes.get("Background") or nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = (*rgb, 1.0)
    bg.inputs["Strength"].default_value = strength
    out = nodes.get("World Output") or nodes.new("ShaderNodeOutputWorld")
    bpy.context.scene.world.node_tree.links.new(bg.outputs[0], out.inputs[0])
    return bg


# --- geometry --------------------------------------------------------------------------------

def box(name, lo, hi, material, collection=None):
    """An axis-aligned box from corner lo to corner hi (metres)."""
    lo, hi = Vector(lo), Vector(hi)
    mesh = bpy.data.meshes.new(name)
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    verts = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    (collection or bpy.context.scene.collection).objects.link(obj)
    if material is not None:
        obj.data.materials.append(material)
    return obj


def plane(name, centre, size, material, rotation=(0, 0, 0), collection=None):
    sx, sy = size
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(-sx / 2, -sy / 2, 0), (sx / 2, -sy / 2, 0), (sx / 2, sy / 2, 0), (-sx / 2, sy / 2, 0)], [], [(0, 1, 2, 3)])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    (collection or bpy.context.scene.collection).objects.link(obj)
    obj.location = centre
    obj.rotation_euler = rotation
    if material is not None:
        obj.data.materials.append(material)
    return obj


def cylinder(name, centre, radius, height, material, segments=40, collection=None, axis='z'):
    """A cylinder from `centre` along +z (or along +y with axis='y': rails, pipes)."""
    cx, cy, z0 = centre
    mesh = bpy.data.meshes.new(name)
    verts, faces = [], []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        u, v = radius * math.cos(a), radius * math.sin(a)
        if axis == 'y':
            verts.append((cx + u, cy, z0 + v))
            verts.append((cx + u, cy + height, z0 + v))
        else:
            verts.append((cx + u, cy + v, z0))
            verts.append((cx + u, cy + v, z0 + height))
    for i in range(segments):
        j = (i + 1) % segments
        faces.append((2 * i, 2 * j, 2 * j + 1, 2 * i + 1))
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    for p in mesh.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    (collection or bpy.context.scene.collection).objects.link(obj)
    obj.data.materials.append(material)
    return obj


# --- materials -------------------------------------------------------------------------------

def _principled(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    return mat, nt, bsdf


def set_input(node, names, value):
    for n in names if isinstance(names, (list, tuple)) else [names]:
        if n in node.inputs:
            node.inputs[n].default_value = value
            return node.inputs[n]
    return None


def planar_coords(nt, scale=1.0):
    """World-space coordinates projected on the surface's dominant axis (floors: x,y; walls
    facing x: y,z; walls facing y: x,z), so tiles line up across every surface."""
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep_n = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Normal"], sep_n.inputs[0])
    sep_p = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep_p.inputs[0])

    def absolute(sock):
        m = nt.nodes.new("ShaderNodeMath")
        m.operation = 'ABSOLUTE'
        nt.links.new(sock, m.inputs[0])
        return m.outputs[0]

    def greater(sock, value):
        m = nt.nodes.new("ShaderNodeMath")
        m.operation = 'GREATER_THAN'
        nt.links.new(sock, m.inputs[0])
        m.inputs[1].default_value = value
        return m.outputs[0]

    fz = greater(absolute(sep_n.outputs[2]), 0.6)   # floor/ceiling
    fx = greater(absolute(sep_n.outputs[0]), 0.6)   # wall facing x
    comb_floor = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(sep_p.outputs[0], comb_floor.inputs[0])
    nt.links.new(sep_p.outputs[1], comb_floor.inputs[1])
    comb_x = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(sep_p.outputs[1], comb_x.inputs[0])
    nt.links.new(sep_p.outputs[2], comb_x.inputs[1])
    comb_y = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(sep_p.outputs[0], comb_y.inputs[0])
    nt.links.new(sep_p.outputs[2], comb_y.inputs[1])
    mix_wall = nt.nodes.new("ShaderNodeMix")
    mix_wall.data_type = 'VECTOR'
    nt.links.new(fx, mix_wall.inputs["Factor"])
    nt.links.new(comb_y.outputs[0], mix_wall.inputs[4])
    nt.links.new(comb_x.outputs[0], mix_wall.inputs[5])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = 'VECTOR'
    nt.links.new(fz, mix.inputs["Factor"])
    nt.links.new(mix_wall.outputs[1], mix.inputs[4])
    nt.links.new(comb_floor.outputs[0], mix.inputs[5])
    if scale != 1.0:
        sc = nt.nodes.new("ShaderNodeVectorMath")
        sc.operation = 'SCALE'
        nt.links.new(mix.outputs[1], sc.inputs[0])
        sc.inputs["Scale"].default_value = scale
        return sc.outputs[0]
    return mix.outputs[1]


def tiles(name, tile, grout, size=0.15, grout_width=0.006, roughness=0.18, variation=0.04, glossy=0.5, bump=0.25):
    """Square grid tiles with grout lines, slight per-tile colour variation, roughness noise."""
    mat, nt, bsdf = _principled(name)
    coords = planar_coords(nt)
    brick = nt.nodes.new("ShaderNodeTexBrick")
    brick.offset = 0.0
    brick.squash = 1.0
    brick.inputs["Scale"].default_value = 1.0
    brick.inputs["Brick Width"].default_value = size
    brick.inputs["Row Height"].default_value = size
    brick.inputs["Mortar Size"].default_value = grout_width
    brick.inputs["Mortar Smooth"].default_value = 0.2
    brick.inputs["Bias"].default_value = 0.0
    t = Vector(tile)
    brick.inputs["Color1"].default_value = (*t, 1)
    brick.inputs["Color2"].default_value = (*(t * (1.0 - variation)), 1)
    brick.inputs["Mortar"].default_value = (*grout, 1)
    nt.links.new(coords, brick.inputs["Vector"])
    nt.links.new(brick.outputs["Color"], bsdf.inputs["Base Color"])
    # Grout is rough and slightly recessed; tile glaze is smooth with faint unevenness.
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 3.0
    nt.links.new(coords, noise.inputs["Vector"])
    rough = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(noise.outputs["Fac"], rough.inputs["Value"])
    rough.inputs["To Min"].default_value = roughness * 0.6
    rough.inputs["To Max"].default_value = roughness * 1.5
    mix_r = nt.nodes.new("ShaderNodeMix")
    mix_r.data_type = 'FLOAT'
    nt.links.new(brick.outputs["Fac"], mix_r.inputs["Factor"])
    nt.links.new(rough.outputs["Result"], mix_r.inputs[2])
    mix_r.inputs[3].default_value = 0.9
    nt.links.new(mix_r.outputs[0], bsdf.inputs["Roughness"])
    set_input(bsdf, ["Specular IOR Level", "Specular"], glossy)
    bump_node = nt.nodes.new("ShaderNodeBump")
    bump_node.inputs["Strength"].default_value = bump
    bump_node.inputs["Distance"].default_value = 0.002
    inv = nt.nodes.new("ShaderNodeMath")
    inv.operation = 'SUBTRACT'
    inv.inputs[0].default_value = 1.0
    nt.links.new(brick.outputs["Fac"], inv.inputs[1])
    nt.links.new(inv.outputs[0], bump_node.inputs["Height"])
    nt.links.new(bump_node.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def matte(name, colour, roughness=0.6, noise_scale=0.0, noise_amount=0.0, specular=0.5, bump=0.0, stains=0.0):
    """A painted or plain surface: colour with optional large-scale unevenness and stains."""
    mat, nt, bsdf = _principled(name)
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    set_input(bsdf, ["Specular IOR Level", "Specular"], specular)
    if noise_amount > 0 or stains > 0 or bump > 0:
        coords = planar_coords(nt)
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = noise_scale or 1.5
        noise.inputs["Detail"].default_value = 6.0
        nt.links.new(coords, noise.inputs["Vector"])
        ramp = nt.nodes.new("ShaderNodeMapRange")
        nt.links.new(noise.outputs["Fac"], ramp.inputs["Value"])
        ramp.inputs["From Min"].default_value = 0.35
        ramp.inputs["From Max"].default_value = 0.75
        ramp.inputs["To Min"].default_value = 1.0
        ramp.inputs["To Max"].default_value = 1.0 - (noise_amount + stains)
        mul = nt.nodes.new("ShaderNodeVectorMath")
        mul.operation = 'SCALE'
        mul.inputs[0].default_value = (*colour, )
        nt.links.new(ramp.outputs["Result"], mul.inputs["Scale"])
        nt.links.new(mul.outputs[0], bsdf.inputs["Base Color"])
        if bump > 0:
            fine = nt.nodes.new("ShaderNodeTexNoise")
            fine.inputs["Scale"].default_value = 400.0
            fine.inputs["Detail"].default_value = 2.0
            nt.links.new(coords, fine.inputs["Vector"])
            b = nt.nodes.new("ShaderNodeBump")
            b.inputs["Strength"].default_value = bump
            b.inputs["Distance"].default_value = 0.001
            nt.links.new(fine.outputs["Fac"], b.inputs["Height"])
            nt.links.new(b.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def emitter(name, colour, strength):
    """A glowing fixture surface (tube diffuser, sign). Belongs to one light group."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*colour, 1)
    em.inputs["Strength"].default_value = strength
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    mat["liminal_emission"] = strength
    return mat


def water(name, tint=(0.7, 0.95, 0.92), absorb=(0.3, 0.82, 0.8), density=0.5):
    """Still water: a clear refracting surface with a turquoise body (volume absorption)."""
    mat, nt, bsdf = _principled(name)
    bsdf.inputs["Base Color"].default_value = (*tint, 1)
    bsdf.inputs["Roughness"].default_value = 0.015
    bsdf.inputs["IOR"].default_value = 1.333
    set_input(bsdf, ["Transmission Weight", "Transmission"], 1.0)
    # A barely-there surface movement, frozen: still water is never a perfect mirror.
    coords = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 2.2
    noise.inputs["Detail"].default_value = 3.0
    nt.links.new(coords.outputs["Object"], noise.inputs["Vector"])
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = 0.035
    b.inputs["Distance"].default_value = 0.01
    nt.links.new(noise.outputs["Fac"], b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], bsdf.inputs["Normal"])
    vol = nt.nodes.new("ShaderNodeVolumeAbsorption")
    vol.inputs["Color"].default_value = (*absorb, 1)
    vol.inputs["Density"].default_value = density
    out = nt.nodes.get("Material Output")
    nt.links.new(vol.outputs[0], out.inputs["Volume"])
    # Light reaches the basin through the surface (shadow rays pass, tinted): without this, and
    # with refractive caustics off for speed, everything under the water would be unlit.
    path = nt.nodes.new("ShaderNodeLightPath")
    clear = nt.nodes.new("ShaderNodeBsdfTransparent")
    clear.inputs["Color"].default_value = (*tint, 1)
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(path.outputs["Is Shadow Ray"], mix.inputs["Fac"])
    nt.links.new(bsdf.outputs[0], mix.inputs[1])
    nt.links.new(clear.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return mat


# --- lights and light groups -----------------------------------------------------------------

GROUPS = {}   # group name -> list of objects (lamps and emissive fixtures)


def in_group(group, obj):
    GROUPS.setdefault(group, []).append(obj)
    obj["liminal_group"] = group
    return obj


def area_light(name, group, location, size, power, colour, rotation=(0, 0, 0), shape='RECTANGLE', spread=None):
    data = bpy.data.lights.new(name, 'AREA')
    data.shape = shape
    data.size = size[0]
    data.size_y = size[1]
    data.energy = power
    data.color = colour
    if spread is not None:
        data.spread = spread
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = rotation
    obj.visible_camera = False
    return in_group(group, obj)


def point_light(name, group, location, power, colour, radius=0.05):
    data = bpy.data.lights.new(name, 'POINT')
    data.energy = power
    data.color = colour
    data.shadow_soft_size = radius
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.visible_camera = False
    return in_group(group, obj)


def spot_light(name, group, location, rotation, power, colour, angle, radius=0.02, blend=0.3):
    data = bpy.data.lights.new(name, 'SPOT')
    data.energy = power
    data.color = colour
    data.spot_size = angle
    data.spot_blend = blend
    data.shadow_soft_size = radius
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = rotation
    obj.visible_camera = False
    return in_group(group, obj)


def sun_light(name, group, rotation, strength, colour, angle=0.6):
    data = bpy.data.lights.new(name, 'SUN')
    data.energy = strength
    data.color = colour
    data.angle = math.radians(angle)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.rotation_euler = rotation
    return in_group(group, obj)


def _set_group(group_or_none, world_strength, base_strength, only_objects=()):
    """Switch every light group off except `group_or_none`; the world on only for the base."""
    for g, objs in GROUPS.items():
        on = (group_or_none is None) if g == "base" else g == group_or_none   # "base": glowing things lit only in the base pass
        for o in objs:
            o.hide_render = not on
    for o in only_objects:
        o.hide_render = False
    bg = bpy.context.scene.world.node_tree.nodes.get("Background")
    if bg is not None:
        bg.inputs["Strength"].default_value = base_strength if group_or_none is None else 0.0


def project(obj_or_point):
    """Where a point (or an object's origin) lands in the picture, 0..1 from the top left."""
    from bpy_extras.object_utils import world_to_camera_view
    scene = bpy.context.scene
    p = obj_or_point.matrix_world.translation if hasattr(obj_or_point, "matrix_world") else Vector(obj_or_point)
    v = world_to_camera_view(scene, scene.camera, p)
    return [round(v.x, 4), round(1.0 - v.y, 4)]


def render_passes(out_dir, passes, samples=128, base_strength=1.0, extra=None, base_default=1.0):
    """Render the base pass and each light group to linear EXR files in out_dir.
    `passes`: list of dicts {name, group, kind, default, anchors: [objects]}."""
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    scene.cycles.samples = samples
    s = scene.render.image_settings
    s.file_format = 'OPEN_EXR'
    s.color_depth = '16'
    s.exr_codec = 'ZIP'
    try:
        s.color_management = 'OVERRIDE'
        s.linear_colorspace_settings.name = 'Linear Rec.709'
    except Exception:
        pass
    manifest = {"width": WIDTH, "height": HEIGHT, "passes": []}
    jobs = [{"name": "base", "group": None, "kind": "base", "default": base_default}] + passes
    only = os.environ.get("LIMINAL_ONLY_PASS")
    for job in jobs:
        _set_group(job["group"], 0.0, base_strength)
        path = os.path.join(out_dir, job["name"] + ".exr")
        entry = {k: v for k, v in job.items() if k not in ("group", "anchors")}
        anchors = job.get("anchors") or GROUPS.get(job["group"], [])
        if anchors:
            pts = [project(o) for o in anchors]
            entry["anchor"] = [round(sum(p[0] for p in pts) / len(pts), 4), round(sum(p[1] for p in pts) / len(pts), 4)]
        manifest["passes"].append(entry)
        if only and job["name"] != only:
            continue
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("rendered", path, flush=True)
    if extra:
        manifest.update(extra)
    with open(os.path.join(out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    return manifest


def ceiling_panel(name, group, centre, size, z, power, colour, glow=None):
    """A recessed fluorescent troffer: a glowing diffuser (seen by the camera and in reflections)
    and an area light just below it that does the lighting. Both belong to `group`."""
    x, y = centre
    diffuser = emitter(name + " diffuser", colour, glow if glow is not None else power / (size[0] * size[1]) * 0.012)
    p = plane(name + " diffuser", (x, y, z - 0.005), size, diffuser, rotation=(math.pi, 0, 0))
    p.visible_shadow = False
    in_group(group, p)
    frame = matte(name + " frame", (0.78, 0.78, 0.74), roughness=0.4, specular=0.7)
    t = 0.03
    sx, sy = size
    for i, (a, b) in enumerate([((x - sx / 2 - t, y - sy / 2 - t), (x + sx / 2 + t, y - sy / 2)), ((x - sx / 2 - t, y + sy / 2), (x + sx / 2 + t, y + sy / 2 + t)),
                                 ((x - sx / 2 - t, y - sy / 2), (x - sx / 2, y + sy / 2)), ((x + sx / 2, y - sy / 2), (x + sx / 2 + t, y + sy / 2))]):
        box(f"{name} frame {i}", (a[0], a[1], z - 0.02), (b[0], b[1], z), frame)
    return area_light(name, group, (x, y, z - 0.03), size, power, colour)
