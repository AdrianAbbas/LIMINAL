# Characters, evolution, scenes and macros (Milestone 5)

Code: `Source/DSP/Characters.h/.cpp` (the table, Unreal flavours, macro responses,
`Evolution`), applied in `DSP/Environment` (room, bed, events, the place's processing),
`DSP/Effects` (default effects) and `PluginProcessor` (macros, scenes).

## The thirty Characters

A Character keeps the place but changes: which events happen and how often (within 0.6–1.6x,
never below the place's minimum gap; one extra event kind; a flickering Character's tube fails more often), the bed (level, brightness, hum), the room (decay, brightness, level),
the place's processing of the instruments (low-pass, drift, wow, chorus, frequency shift,
wandering image, tremolo, nearness), the default effects (drift, tape, chorus, granular,
echo, room), evolution speed and depth, how strongly Unreal acts, and the look (grade,
light, flicker — Milestone 4 paints light and flicker). Character 1 of each place is the
place much as tuned in 2b; 2 and 3 move further away.

| Place | Character 1 | Character 2 | Character 3 |
|---|---|---|---|
| Hospital | **Sterile Silence** — Clean, bright, almost nothing moves *(piano + glass)* | **After Hours** — The night shift: dim, monitors somewhere *(piano + drone)* | **Something Wrong** — Too many alarms, the lights stutter, pitch slips *(glass + drone)* |
| Mall | **Closing Time** — The last shoppers far off, shutters coming down *(synth + piano)* | **Forgotten** — Years empty: dust, faded colour, nothing running *(music box + piano)* | **Endless** — The atrium goes on forever; everything far away *(drone + choir)* |
| School | **After Class** — Everyone just left; the building settles *(piano + music box)* | **Childhood Memory** — Warm, hazy, half-remembered voices *(music box + glass)* | **Nightmare** — The corridor stretches; nothing stays in tune *(choir + drone)* |
| Garage | **Vacant** — Nobody here; the fans and the concrete *(piano + drone)* | **Distant Movement** — Cars on other levels, a lift, something far off *(synth + drone)* | **Pursuit** — Engines, doors, footsteps you cannot place *(synth + drone)* |
| Pool | **Still Water** — Flat water, a drip now and then *(glass + piano)* | **Forgotten Summer** — Sun through the skylights, echoes of voices *(synth + music box)* | **Submerged** — Under the surface: muffled, slow, enveloping *(drone + choir)* |
| Hotel | **Late Night** — Everyone asleep behind their doors *(piano + synth)* | **Faded Luxury** — The lobby: marble, gold, a lounge that closed *(piano + glass)* | **Room 404** — The room that should not be on this floor *(choir + drone)* |
| Subway | **Last Train** — It just left; the rails are still singing *(synth + piano)* | **Abandoned** — Closed for years: leaks, dead power, dark tunnels *(drone + glass)* | **Approaching** — Something is coming down the tunnel *(synth + drone)* |
| Supermarket | **Closing Shift** — Lights half off, a till still beeping *(synth + music box)* | **Nostalgia** — A 1990s Saturday, through worn tape *(music box + synth)* | **Fluorescent Nightmare** — The tubes are screaming; everything hums *(synth + drone)* |
| Office | **After Hours** — Screens asleep, the HVAC never stops *(synth + piano)* | **Dream State** — Soft focus; the room breathes *(choir + synth)* | **Infinite Cubicles** — Every aisle the same, every sound repeated *(synth + glass)* |
| Impossible | **Familiar** — Almost a normal room. Almost. *(piano + glass)* | **Unstable** — Nothing holds still: pitch, space, light *(glass + drone)* | **Beyond Reality** — The walls are gone; only the reverb remains *(choir + drone + glass)* |

## Macros

| Macro | What it does | Per place |
|---|---|---|
| Presence | instruments ±6 dB direct, their room and the place's echo ∓4 dB, the place's own sounds ∓3 dB | response 0.8 (mall) … 1.9 (office) |
| Scale | the room's decay x0.5 … x2 (x0.25 … x4 in the impossible room), its level ±4 dB | response 0.5 (hotel) … 1.6 (impossible) |
| Activity | Direction Update 2: 0 % = only the bed, the default 25 % = the place's own rare events, 100 % = up to x4 as often (never busy); evolution faster or slower | response 0.8 … 1.5 |
| Unreal | each place's own flavour (below) x the Character's Unreal | — |

Unreal flavours: hospital = frequency-shifted dissonance, wow, ghostly shimmer; mall =
the atrium dissolving (grains, wandering image, rising shimmer); school = pitch
instability, tape degradation, chorus and wandering image (*nostalgic, unsettling*);
garage = the concrete answering back an octave down; pool = loud echoes a fifth higher,
long shimmer (*unnatural reflective echoes*); hotel = warped tape, walls that move;
subway = the tunnel bending pitch, echoes a fourth down; supermarket = electrical
(shifted partials, heavy chorus); office = fragments and echoes that climb an octave;
impossible room = all of it.

## Evolution

Three smooth seeded random movements (spectral, spatial, harmonic) and occasional
disturbances (since Direction Update 2: the place's next event brought forward, never closer than 3/4 of its minimum gap). Spectral moves the instruments'
low-pass ±0.8 octave and the bed's level ±20 %; spatial moves the room's decay (x2^±0.3)
and level (±1.5 dB); harmonic adds drift and wow. Rate: a change of direction every ~100 s
(0 %) to ~5 s (100 %), x the Character's speed and the Activity macro. **Freeze** holds it;
**Reset** restarts it from the **Seed** (same seed, same evolution, sample for sample).
The viewfinder shows a row of ticks, lit by how much is changing.

## Scenes A/B

STORE A / STORE B capture every parameter (except Power and the morph itself). With
MORPH on, the processor plays the morph instead of the knobs, through a mirror of all
parameters updated each block (automation works with the editor closed):

- continuous parameters interpolate in their normalised range;
- place, Character, switches and choices change at half-way — the place cross-fades (its
  tail rings, the new ambience fades in over ~2 s), switches fade over 20 ms;
- an instrument on in only one scene fades through its level (to -60 dB) instead of
  switching, so nothing jumps and no note is cut;
- an unstored scene = the knobs as they are.

Scenes are saved with the song (by parameter ID, so later versions can add parameters).
