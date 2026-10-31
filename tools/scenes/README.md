# Photographic scenes (Direction Update 2)

Each place is a **scripted 3D scene** (`<place>.py`: geometry, materials and lights built from
nothing by Python - no downloaded models, textures or HDRIs) rendered with **Cycles** (path
tracing + OpenImageDenoise) as separate **light passes** in linear light:

| pass | what is lit |
|---|---|
| `base` | the world / daylight only (every lamp off) - plus anything in the special group `base` |
| one per light group | that group alone (its lamps *and* its glowing fixtures), the world off |
| caustics (water only) | a 24-frame loop of light thrown off still water, at a quarter of the size |

Light adds linearly, so the plug-in rebuilds the picture as `base + sum(level_i x pass_i)` and
can flicker, dim and swell every group on its own in real time (`Source/UI/PhotoScene.*`).

## Running it

Blender's Python module runs headless, no GUI or GPU needed:

```bash
python3.13 -m venv ~/bpyenv
~/bpyenv/bin/pip install bpy pillow      # bpy 5.2.x (Blender 5.2 LTS) needs Python 3.13
~/bpyenv/bin/python tools/scenes/render.py pool              # any of the ten: hospital mall school garage pool hotel subway supermarket office impossible
~/bpyenv/bin/python tools/scenes/render.py pool --preview    # quarter size, 16 samples: a quick look
~/bpyenv/bin/python tools/scenes/preview.py pool out.png sun=1.5 base=0.6   # composite like the plug-in
~/bpyenv/bin/python tools/scenes/preview.py pool out.png character=3        # as a Character lights it
~/bpyenv/bin/python tools/scenes/preview.py pool sheet.jpg --sheet          # the three Characters side by side
~/bpyenv/bin/python tools/scenes/render.py mall --pack-only                 # re-pack EXRs already rendered
~/bpyenv/bin/python tools/scenes/render.py mall --characters-only           # re-write the Character table only
```

Environment variables: `LIMINAL_SAMPLES` (default 160), `LIMINAL_ONLY_PASS=<name>` (re-render
one pass and repack). Linear EXRs go to `build/scenes/<place>/`; the packed assets the plug-in
bundles go to `assets/scenes/<place>/`:

- `<place>_<pass>.jpg` - each pass, sRGB-encoded, stored half a stop down (headroom for bright
  fixtures), with a little halation baked in (a blur of the brightest light: linear, so it adds
  up correctly across passes);
- `<place>_caustic_NN.jpg` - the caustic loop (pool);
- `<place>_manifest.json` - size, the passes (name, kind, default level, where the group sits
  in the picture), the caustic frames and their rate, the headroom.

The plug-in build never needs Blender: CMake bundles whatever is in `assets/scenes/` (BinaryData).
Rendered here on a 4-core cloud container (Linux, x86_64, CPU only), 1440 x 1080: the
prototype at 96 samples took 4.5-8.5 minutes a pass; Part 3's seven at `LIMINAL_SAMPLES=80`
about 3-5 minutes a pass (25-35 minutes a scene).

## Characters (Part 3)

Each scene script ends with `CHARACTERS` (per pass, its level under Characters 1-3; 1 = as
rendered) and optionally `FLICKER` (per pass, whether it follows the hum's stutter under each
Character; passes of kind `failing` always do). They go into the manifest (`characters`,
`flicker` per pass, `caustics.characters`), so the plug-in needs no table of its own. A
*variant* is a light group that only one Character switches on - e.g. the hotel's `room404`
(door 404 ajar, its room lit), the mall's `lastshop`, the school's `classroom` sun, the garage's
`headlights`, the subway's `train` - with the geometry rendered once for all three.

## The developer's own renders or photographs

`~/Library/Application Support/Veil Audio/LIMINAL/Scenes/` (inside GarageBand's sandbox, the host's
container): a folder `<place>/` with the same files as `assets/scenes/<place>/` replaces the
bundled renders; a single photograph `<n>.png` (n = 1..10, Milestone 4) still replaces the whole
scene, renders included.

## Look

One-point perspective at eye height (1.6 m), 24-26 mm lens with a slight vertical shift (no
converging verticals), soft global illumination, tile grout, roughness variation, glossy floors,
warm light against unlit corners. The plug-in adds the photographic finish: the film curve, the
Character's grade and light, a fade (second Character) or darker edges (third), vignette, grain.
