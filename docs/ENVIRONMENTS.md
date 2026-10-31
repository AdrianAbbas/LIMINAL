# Environments (Milestone 2a/2b, rebuilt for Direction Update 2)

Each place is a configuration in `Source/DSP/Environment.cpp` (profile: room, echo, the
instruments' processing) and `Source/DSP/Ambience.cpp` (its own sounds).

**Direction Update 2 (2026-10-30) replaced the sound design.** A liminal space is *still*:
the room is the sound. So:

- **Ambience = a steady tonal bed + a very quiet room tone + rare, distant events.** No
  risers, whooshes, swells, noise sweeps or "waves" of noise anywhere (the old breathing air,
  lapping water, gusts, reversed swells, groans, passing cars, voices and drive seeks were
  removed). Broadband noise is only a heavily low-passed air/HVAC component, below the hum.
- **Mains hum** is 60 Hz with harmonics (120, 180 … 360 Hz), each beating slowly against a
  near twin (0.02–0.09 Hz). **Fluorescent ballast buzz**: a full-wave shape (120 Hz and its
  harmonics) plus the arc's grit (band-passed noise bursts at every mains zero crossing,
  120 per second), with a tiny random level flicker. A failing tube **stutters**: a few quick
  dark flicks, the buzz dropping out and sputtering back. The same stutter drives the
  picture's failing light (`Ambience::lamp()`, published to the editor as `meters.lamp` /
  `meters.flickers`).
- **Events** come from one scheduler per place: a minimum gap (20–35 s) then an exponential
  wait (mean 50–80 s). At the default **Activity (25 %)** that is at most one event every
  20 s, usually one every 40–90 s; **0 %** = only the bed; **100 %** = up to four times as
  often (still a few per minute at most). A Character changes which events and (within
  0.6–1.6x) how often, never below the minimum gap. Evolution's disturbances can bring the
  next event forward, never closer than 3/4 of the gap.
- **The instrument + the room is the main event.** Each place's identity is in its RT60,
  early reflections (flutter between parallel walls, dense concrete, distinct tiled
  reflections, a tunnel), damping, width and distance. The non-physical processing that
  2b used to push places apart (frequency shifting, moving notches, steep "behind a wall"
  filters, tremolo, chorus washes) is gone from the real places; only the Impossible Room
  keeps a little, by design.
- **Far Away** (new parameter `far_away`, 0–100 %, default 0): the instruments heard from
  elsewhere in the building. Mall: **over the PA in another wing** (320 Hz–3.9 kHz, a small
  speaker's resonance and slight distortion; the atrium's long reverb). Hotel: **through
  the wall from the next room** (steep low-pass at 480 Hz, the wall's resonance at 165 Hz).
  Elsewhere: from another room (650 Hz low-pass, 170 Hz resonance).
- Levels: beds about 13–16 dB under a mezzo-forte phrase, quieter where the place is meant
  to be near-silent (pool, hotel, impossible room).

| | Bed (steady) | Acoustics (instrument + room) | Rare events |
|---|---|---|---|
| **Hospital** | 60 Hz hum + ballast buzz; faint HVAC hiss far below; a ~1.4 kHz machine tone, extremely quiet, present ~60 % of the time | RT60 1.5 s low / 1.8 s high (bright-ish, cold), 16 flutter reflections 8.5 ms apart (parallel walls), thin low end (HP 280 Hz, -5 dB at 380 Hz), +4.5 dB at 2.7 kHz; the corridor's far end answers (186/241 ms) | three soft monitor beeps two corridors away; a ballast tick; a far door latch; the tube stutters |
| **Mall** | big-building HVAC drone (38 Hz + harmonics), escalator motor (74 Hz) with its steps' soft periodicity (1.65 Hz), a fountain trickling far off (tiny drops, steady) | RT60 7.5/5.2 s, 75 ms pre-delay, diffuse, dark highs; instruments far (-6.5 dB) and softened. **Far Away = the PA in another wing** | a two- or three-note PA chime (band-limited); a shutter rattling far away |
| **School** | tube hum and buzz; the wall clock ticking once a second (tick/tock); radiator hum (96 Hz) | RT60 1.4/1.2 s, metallic early reflections (16 at 4.6 ms: the lockers), boxy mids (+7 dB at 720 Hz), a faint sheet-metal ring (combs 1160/1630 Hz), nearly mono; a short slap off the opposite wall | radiator ping; a locker settling; a far, muffled bell stroke (rare); the tube stutters |
| **Garage** | ventilation fan (43 Hz + harmonics, blade-rate wobble), lamp buzz, faint electrical hum | RT60 3.4/1.9 s (crossover 800 Hz), 16 dense strong concrete reflections, boomy low mids (+7 dB at 180 Hz), dark top; slap-back off the pillars (63/78 ms) | a car door on another level; the exit barrier clanking very far off; a drip on concrete; the lamp stutters |
| **Pool** | almost silent: a filtration pump (58 Hz), distant ventilation. **No water sounds in the bed** — the water is still | very bright, long, diffuse tiles: RT60 3.8 s low / 4.4 s high, distinct flutter reflections between tiled walls, the far wall (151/233 ms), and a **surface shimmer**: three slow unrelated movements (0.13/0.21/0.34 Hz, about ±0.6 dB) in the tail's level and image, like light on water | a single drip into the water every 30–90 s (a soft plink with a long tiled tail); very rarely one faint lap at the edge |
| **Hotel** | very quiet room tone; an ice machine / HVAC hum down the hall (118 Hz) | carpet and soft furnishings: RT60 0.9/0.4 s, highs rolled off (3.2 kHz), close and narrow. **Far Away = through the wall from the next room** | the lift's chime two floors down (sometimes the lower note too); a door closing far down the hall |
| **Subway** | tunnel airflow (very low, filtered), the substation transformer (120 Hz family), the rails humming faintly (212 Hz) | tiled platform into tunnels: RT60 3.6/2.6 s, reflections running away down the tube to one side, metallic colour (combs 412/587 Hz), +7 dB bass at 95 Hz; the long echo down the tunnel (430/520 ms) | a train somewhere in the tunnels that never arrives (9–15 s, low, very quiet); a rail ticking; the tube stutters |
| **Supermarket** | refrigeration compressor (49 Hz + harmonics) that **cycles itself on and off** every few minutes with a soft start and stop; fluorescent hum and buzz | large, reflective, glossy floor: RT60 2.5/2.9 s (slightly bright), a regular ceiling-grid pattern, wide; shelving's short repeat | a freezer door's seal; a trolley settling (two or three clinks); the tubes stutter |
| **Office** | **the fluorescent hum-buzz is the room** (Backrooms Level 0: slightly too loud, about -40 dBFS); a coil whine almost too high to hear (11.8 kHz); HVAC far below. No white noise | carpet, ceiling tiles, partitions: RT60 0.5/0.35 s, damped highs (4.2 kHz), a little boxy (+5 dB at 480 Hz) | almost none: the ballast stutters (the hum drops out with the picture's flickering panel); very rarely a tick |
| **Impossible** | a room tone that is *too* quiet; a hum whose pitch drifts slowly (±7 %) as no real hum can; now and then **total silence** for 1.5–4 s (never in the first 25 s) | a small room's reflections (2 ms, 3.2 ms apart) with a cathedral's tail (RT60 9/7.5 s); the tail **swells after the note** (the room hears it late, growing over ~1.4 s); reflections subtly pitch-shifted (+0.4 semitone echoes, a +0.3 semitone shimmer); a faint frequency-shifted copy; an image that will not stay still | a reflection of a note nobody played (only in the room); a knock from a direction with no wall (one side, no room) |

Numbers are design choices *(estimate)* guided by typical acoustics: long hard corridors
(1.5–2 s), large atria (6–8 s), school corridors (1.2–1.6 s), concrete car parks (2.5–3.5 s,
bass-heavy), tiled pools (3–5 s, bright), carpeted hotel corridors (0.5–0.8 s), tiled
stations (2.5–4 s), supermarkets (1.5–2.5 s), open-plan offices (0.4–0.7 s); flutter-echo
spacing = 2 x wall distance / 343 m/s.

## Signatures and research corrections (2026-10-31)

The table above is the Direction Update 2 design. Since 2026-10-31 each place also has **one signature effect no
other place uses** (`Source/DSP/Signature.cpp`), and some acoustics were corrected from the research in
`docs/ENVIRONMENT_RESEARCH.md` (its last section has the full table): hospital RT60 1.0/1.15 s with 14 ms flutter
spacing and a flutter-echo signature; mall 5.5/3.8 s with the distant PA as a signature (the direct sound almost
gone); school without flutter pattern, tape wobble + ringing lockers; garage slap-back + 69 Hz boom (the chain's own
63/78 ms echo removed); pool 4.8/3.6 s, first reflection 41 ms, the underwater layer; hotel 0.6/0.35 s, through the
wall; subway receding tunnel echoes (its combs and 430/520 ms echo moved into the signature); supermarket 1.8/1.3 s,
the ceiling-speaker grid (its short repeat removed); office the 120 Hz hum infection; impossible room 7.5/9.5 s (the
highs outlast the lows), reversed swells, wrong-pitch reflections and rare freezes. Tests: group `distinct`.

## How it is verified (tests group `environments`)

- **The bed alone** (Activity 0, 30 s per place, same seed): quiet (-60 to -38 dBFS; measured
  -52 to -39.5), **still** (the 250 ms level moves less than 2.5 dB between its 5th and 95th
  percentile; the supermarket's compressor cycling and the impossible room's silences are
  exempt), **tonal** (over half the energy from 30 Hz to 6 kHz sits in narrow spectral peaks:
  noise stays below the hum; measured 0.91–1.00).
- **Events**, counted by the ambience engine itself over 3 minutes: default Activity 1–9 and
  never two within 20 s; Activity 100 % more but at most 40; Activity 0 none.
- Every pair of places has a different bed spectrum (over 2.5 dB RMS across octaves).
- The same piano phrase in each place: level within 6 dB, and every pair clearly different
  (character distance over 1.5; every instrument over 1.0).
- **Place vs controls** (2b's request): now report-only. The closest pair of places is below
  each instrument's strongest control since the places lost their non-physical processing
  (DEVLOG, Direction Update 2).
- Switching places 16 times while playing: no allocation, no clicks; the old room's tail
  rings on; block-size independence; Environment Amount 0 is transparent.
