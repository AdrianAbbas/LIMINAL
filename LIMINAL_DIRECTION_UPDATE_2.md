# LIMINAL — Direction Update 2: Real liminal spaces (sound + visuals)

Read after LIMINAL_CLAUDE_BRIEF.md and LIMINAL_DIRECTION_UPDATE_1.md. This file overrides them where they conflict.

## Developer feedback that triggered this

- **Visuals look fake.** The procedural 2D corridors read as vector drawings, not places. Target: photoreal, dreamlike renders in the style of contemporary liminal / "dream pool" 3D art — e.g. softly path-traced white-tiled pools with still turquoise water, warm sunlight patches against shadowed corners (mood reference only; never copy anyone's artwork).
- **Room sounds are wrong.** Far too much activity: random risers, whooshes, waves of white noise. A liminal space is **still**. The pool should be the sound of an instrument reflecting off still water and tile, not waves. The office should be a fluorescent hum, not white noise.

---

## 1. What makes a space liminal (the design rules)

Research summary (liminal-space aesthetics, the Backrooms, "dream pool" renders, mallsoft):
- **Emptiness and stillness.** Transitional places (corridors, pools, malls, hotels, stations) seen out of context: built for crowds, now empty. Nothing moves. Nobody is there. The unease comes from what is *missing*.
- **A constant, mechanical hum.** The iconic sound is the steady **fluorescent hum-buzz** of Backrooms Level 0 — constant, slightly too loud, mildly headache-inducing — plus HVAC and electrical room tone. It is *tonal and steady*, not noisy or eventful.
- **The room is the sound.** The architecture's acoustics — long tiled reverb, carpet deadness, concrete slap, distant reflections — are what you hear. The space "answers" the instrument.
- **Distance and nostalgia.** Sound heard from far away or through walls: muffled, band-limited, reverberant — the "music playing in another room" / dead-mall muzak effect.
- **Rare, tiny events.** Something happens perhaps once a minute or less, far away: a single drip, a distant door, a ballast tick, an elevator chime two floors down. Silence between them is the point.
- **Visually:** soft, even, artificial or hazy light; warm light patches against unlit corners; clean repeating geometry (tiles, ceiling grids, doors) going on too long; no people, no clutter; slightly off proportions.

### Hard audio rules (all environments)
- **No risers, whooshes, swells, noise sweeps or "waves" of broadband noise** in any ambience. Remove them.
- Ambience = **steady tonal bed** (hums, buzzes, drones at specific frequencies) + **very quiet room tone** + **rare, sparse, distant events**.
- **Default Activity is low.** At default, at most one audible event every 20–90 s per environment, quiet and distant. The Activity macro can raise it, but even 100% must stay sparse and believable, never busy.
- Broadband noise is allowed only as a very low, heavily filtered air/HVAC component (well below the hum).
- Mains hum is **60 Hz with harmonics** (120, 180, 240 Hz…, North American), with subtle slow beating; fluorescent ballast buzz centres around **120 Hz and its harmonics** with a gritty upper buzz. Add a tiny amount of random amplitude flicker, synced to the visual light flicker (Section 3).
- **The instrument + the room is the main event.** Reverb, early reflections, flutter echoes, damping, distance and "through-the-wall" filtering are where each environment's identity lives.
- Keep ambience about 10–15 dB under the instruments at default.

---

## 2. The ten environments — sound and image

For each: **Bed** (steady sound), **Acoustics** (what the instrument goes through), **Events** (rare), **Image** (what to render). Characters (from Update 1) vary these.

### Vacant Hospital
- **Bed:** 60 Hz mains hum + fluorescent ballast buzz; faint HVAC hiss far below; a soft steady tone from some distant machine (a sine around 1–2 kHz, extremely quiet, sometimes absent).
- **Acoustics:** hard, sterile surfaces; long narrow corridor → strong flutter echo between parallel walls, RT60 ~1.5–2 s, bright-ish but cold (slight high-mid emphasis, thin low end).
- **Events:** a single distant monitor beep pattern (rare, minutes apart), a ballast tick, a far door click.
- **Image:** long corridor, pale green-grey walls and linoleum with glossy reflections of ceiling tubes, recessed fluorescent panels (one flickering), handrails, closed doors, an exit sign glow at the end.

### Empty Shopping Mall
- **Bed:** big-building HVAC drone (low, smooth), escalator motor hum (steady tone ~50–100 Hz with a soft mechanical periodicity), faint fountain trickle far away (very quiet, not splashy).
- **Acoustics:** huge atrium, RT60 6–8 s, diffuse, slightly dark highs. **Distant PA mode:** the instruments can be heard as if "playing over the mall PA in another wing": band-limited (≈300 Hz–4 kHz), slight tinny distortion, long reverb, distant (dead-mall / mallsoft effect). This is a signature of this environment.
- **Events:** a distant PA chime, a shutter rattling far away, very rare.
- **Image:** multi-level atrium with skylights (soft daylight or night), empty escalators, glossy floor reflections, planters, closed shop fronts, a dry or still fountain.

### Abandoned School
- **Bed:** fluorescent hum; an analog wall clock **ticking** (quiet, steady, one per second — iconic); radiator hum.
- **Acoustics:** hallway with lockers and hard floors: RT60 ~1.2–1.6 s, metallic early reflections from lockers, slightly boxy mids.
- **Events:** radiator ping, a locker settling, distant bell tone (very rare, muffled).
- **Image:** long hallway of lockers, yellowed light, polished floor, classroom doors with small windows, a wall clock, bulletin boards with faded paper.

### Underground Parking Garage
- **Bed:** low ventilation fan rumble (~30–60 Hz, steady), sodium/fluorescent lamp buzz, faint electrical hum.
- **Acoustics:** concrete — strong hard early reflections, RT60 ~2.5–3.5 s, boomy low-mids, dark highs; slap-back off pillars.
- **Events:** one extremely distant car door or barrier clank, minutes apart; an occasional drip.
- **Image:** low ceiling, concrete pillars receding, painted bay lines, sodium-orange or cold fluorescent pools of light with dark gaps between, a single exit sign.

### Abandoned Indoor Pool  *(reference image mood)*
- **Bed:** almost silent; very faint filtration-pump hum (low steady tone) and distant ventilation. **No wave sounds.** The water is still.
- **Acoustics:** **the instrument reflecting off still water and tile:** very bright, long, diffuse tiled reverb (RT60 3–5 s), distinct early reflections and flutter between parallel tiled walls, subtle, slow "water-surface shimmer" on the reflections (a very gentle modulation of the late reverb, like light on water — not audible as movement, just alive).
- **Events:** a single drip into water every 30–90 s (soft plink with a long tiled tail), very rarely a faint ripple lap.
- **Image:** white grid-tiled pool room with rounded pillars, still turquoise water with gentle caustics, soft warm sunlight patches against shadowed corners, low ceiling, openings leading to more pool rooms.

### Empty Hotel
- **Bed:** very quiet room tone; a distant ice-machine or HVAC hum; carpet makes everything close and dead.
- **Acoustics:** short, warm, muffled (carpet + soft furnishings): RT60 ~0.5–0.8 s, rolled-off highs. **Through-the-wall mode:** instruments can sound as if heard from the next room (low-pass, slight resonance of the wall).
- **Events:** a distant elevator chime, a door closing far down the hall, very rare.
- **Image:** long patterned-carpet corridor, warm wall sconces, numbered doors, faded wallpaper, a dim end of the hall.

### Deserted Subway Station
- **Bed:** steady tunnel airflow (very low, filtered), electrical transformer hum, faint distant rail hum.
- **Acoustics:** long tiled platform: directional, long echoes down the tunnel, RT60 ~2.5–4 s, metallic coloration.
- **Events:** a very distant train rumble that never arrives (rare), a rail tick.
- **Image:** empty tiled platform, edge line, pillars, a dark tunnel mouth, signage, flickering fluorescent strip.

### Empty Supermarket
- **Bed:** refrigeration compressor hum that **cycles** on and off every few minutes (a steady low tone with a soft start/stop), fluorescent buzz, very faint muzak from a ceiling speaker (optional, Character-dependent).
- **Acoustics:** large, reflective commercial interior with glossy floors, RT60 ~1.5–2.5 s, slightly bright.
- **Events:** a freezer door seal thunk, a cart settling, rare.
- **Image:** long aisles under even fluorescent light, glossy floor reflections, glowing fridge cases, sparse shelves.

### Endless Office
- **Bed:** **fluorescent hum-buzz is the dominant sound** (Backrooms Level 0 style, slightly too loud); computer/transformer whine very faint; HVAC far below. **No white noise.**
- **Acoustics:** carpet + ceiling tiles + cubicle partitions: dry, short, RT60 ~0.4–0.7 s, damped highs, slightly boxy.
- **Events:** almost none. A ballast flicker that makes the hum stutter (synced with the visual flicker).
- **Image:** endless yellowed office/cubicle space or empty yellow rooms, damp-looking carpet, drop-ceiling grid of fluorescent panels (one flickering), no people.

### Impossible Room
- **Bed:** a room tone that is *too* quiet, with occasional moments of total silence (near-anechoic dropouts); a hum whose pitch slowly drifts in a way real hums never do.
- **Acoustics:** acoustics that don't match the visible size (a small room with a cathedral tail, or a vast room that's dry); reflections subtly pitch-shifted; reverb that swells after the note instead of decaying.
- **Events:** very rare, wrong-feeling: a reflection of a note you didn't play, a sound from a direction with no wall.
- **Image:** geometrically unsettling architecture — staircases to nowhere, doors in ceilings, rooms nested in rooms, perfectly even shadowless light, pastel or white.

---

## 3. Photoreal visuals: the rendering pipeline

Procedural 2D drawing inside the plugin cannot reach the target look. Use **real 3D path-traced renders made offline**, shipped as image layers, and animate them in the plugin.

**Tooling.** Build each environment as a **scripted 3D scene in Blender** (Python scripts in `tools/scenes/<environment>.py`, fully procedural geometry and materials: tiles, carpets, concrete, linoleum, glossy floors, water, fluorescent panels). Render with **Cycles** (path tracing + denoising) headless. Ways to run Blender without a GUI: the `bpy` Python package from pip (`pip install bpy`, matching its required Python version), a Blender build from a package manager, or a GitHub Actions job that downloads Blender and renders. Use whichever works in your environment; document it. Renders are committed as assets (`assets/scenes/...`) so the plugin build never needs Blender.

**Look.** One-point or two-point perspective at eye height (~1.6 m), 24–35 mm lens, soft global illumination, realistic materials (tile grout, slight roughness variation, wet and dry surfaces, carpet fibres via texture), haze/volumetrics where suitable, warm light patches against shadow, the subtle imperfections that make renders feel like photographs. Final grade in the plugin: gentle film grain, slight bloom/halation on lights, mild vignette, per-environment/Character colour grade.

**Interactive lighting (from the DEVLOG realism notes).** Render each scene as separate **light passes** in linear light: a base pass with all lights off (ambient/sky only) plus one pass per light group (each tube or bank of tubes, sconces, exit sign, skylight). In the plugin, composite `base + Σ(intensity_i × pass_i)` in linear space, so each light can independently **flicker, dim and brighten in real time**:
- lights respond to played notes (nearest/brightest group swells subtly with note velocity) and to ambience events;
- a fluorescent flicker in the image is **synchronised with a stutter in the audible hum-buzz**;
- Characters change light colour/intensity (e.g. Something Wrong: one bank dead, one flickering).

**Water and movement.** For water scenes, render a short seamless loop (e.g. 24–48 frames) of the water surface/caustics as an overlay layer, or a caustics texture animated in the plugin. Keep the water **still**, with only slow gentle caustic drift; very small ripples may appear on a drip event. Optional subtle parallax from depth layers.

**Characters.** Each of the 30 Characters gets its own render variant or lighting/grade set (e.g. Hotel: Late Night = warm sconces only; Room 404 = one door ajar with light spilling out).

**Performance and size.** Ship images as compressed JPEG/WebP-quality assets sized for the editor (with a 2× set for Retina if size allows); keep the whole asset set reasonable (target < 80 MB in the bundle). Composite on the CPU from cached images at ≤ 30 fps, only redrawing when a light changes; never block the message thread; the MacBook Air must stay cool.

**Asset override.** Keep the drop-in folder from the brief so the developer's own photographs or renders can replace any scene.

---

## 4. Work order

1. **Audio first (fast win):** strip all risers/whooshes/noise waves; rebuild every environment's bed, acoustics and events to Sections 1–2; set default Activity low. Update the listening render (same phrase through all ten rooms, plus 60 s of each room's bed alone).
2. **Render pipeline + 3 scenes as a prototype:** Pool, Office, Hospital — photoreal renders with light passes, interactive flicker synced to the hum, water caustic loop. Snapshots to `docs/snapshots/`. **Stop for the developer's approval of the look.**
3. After approval: the remaining seven environments and the Character variants.

Same working rules as the brief.
