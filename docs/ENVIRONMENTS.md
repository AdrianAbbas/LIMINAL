# Environments (Milestone 2a/2b)

Each place is a complete configuration in `Source/DSP/Environment.cpp` (profile) and
`Source/DSP/Ambience.cpp` (its sounds). The developer's priority: **the environment is
the main source of sound transformation** — changing place must change every instrument
more than any instrument control does. So each place has four layers:

1. **Instrument processing** (`Colour`, scaled by *Environment Amount*): high-/low-pass
   (optionally steep, 24 dB/oct: "behind a wall"), two resonances, a high shelf, tuned comb
   resonators (pipes, rails, concrete) or a moving feed-forward comb (spectral notches),
   pitch wow/flutter/random drift (a swinging delay — a wobble around the pitch, never a
   glide), a detuned "second player" chorus, a single-sideband frequency shift (inharmonic,
   dissonant partials), tape-like saturation, mains tremolo, stereo width, a wandering
   image, and how near the instruments sound (dry level).
2. **Echo**: a tuned stereo two-tap delay with a band-passed feedback loop (the far wall),
   optionally wavering (modulated time), smeared (an allpass in the loop) or pitch-shifted
   on every repeat.
3. **Room** (`Space`, scaled by *Space*): reflection pattern from the geometry (flutter
   between parallel walls / dense irregular concrete / sparse distant surfaces / a tunnel
   receding to one side / an exactly repeating grid; reflections may even *grow*, in the
   impossible room), input diffusion, an 8-line FDN with separate RT60 below and above a
   crossover, slow line modulation, output band limits, tail width; optionally a shimmer
   (the tail pitch-shifted and fed back in).
4. **Ambience**: a bed from place-specific generators and incidental events from
   place-specific event types, each scheduled as a Poisson process above a minimum gap
   with random parameters (so nothing loops); events are partly sent into the room.

Numbers are design choices *(estimate)* guided by typical acoustics: RT60s of long hard
corridors (~2–3 s), large atria (5–8 s), narrow school corridors (~1–1.5 s), concrete car
parks (~3–4 s, bass-heavy), tiled pools (4–6 s, bright), carpeted hotel rooms (~0.5–1 s,
highs absorbed) and tiled stations (3–5 s); flutter-echo spacing = 2 x wall distance /
343 m/s. 2b deliberately pushed every place further from the others (see "How the places
were spread apart").

| | Instruments | Echo | Room | Bed | Events |
|---|---|---|---|---|---|
| **Hospital** | HP 300, steep LP 2.1 kHz, +11 dB ring @ 1.9 kHz Q5 ("cold"), drift ±12 c + wow 5 c, width 2.4 | 241/367 ms ping-pong, fb 0.42 | RT 2.4/1.5 s, flutter 11 ms x12 | air, 60 Hz hum, stuttering fluorescent buzz | monitor beeps, vent rattles, metal clanks |
| **Mall** | HP 50, LP 4.8 kHz, shelf -12 dB > 2 kHz, +3 dB @ 200 Hz, chorus 9 c 70 %, width 1.6, -6 dB | — | RT 7.5/3.8 s, size 2.4, pre 70 ms, diffuse | low roar, ventilation, escalator motor + step clacks | steel groans, clanks, thuds |
| **School** | HP 420, LP 3 kHz, +8 dB @ 1.05 kHz, wow 9 c, flutter 4.5 c, chorus 12 c, tape drive 60 %, **mono** | 41/42 ms slap | RT 1.5/0.6 s, flutter 6.5 ms x14, narrow | air, whistling ducts, 50 Hz ballast hum + buzz | creaks, pipe rushes, cooling ticks, a far door |
| **Garage** | HP 32, steep LP 2.4 kHz, +10 dB @ 165 Hz, +4 dB @ 330, combs 118/157 Hz, width 0.55, -10 dB | 101/107 ms, dark | RT 4.2/1.3 s (x-over 600 Hz), dense strong ER x16 | deep rumble, 26 Hz fan, 38 Hz sub | cars passing (doppler), double thuds, clanks, a gate |
| **Pool** | HP 220, shelf +12 dB > 3.5 kHz, +3 dB @ 3.2 kHz, width 1.6 | 173/291 ms ping-pong, fb 0.5, bright | RT 4.6/4.4 s, HF to 15 kHz | air, lapping water, low pump | drips (leaks), plops, ladder pings |
| **Hotel** | HP 70, **steep LP 700 Hz** (through the wall), +4 dB @ 240, wow 6 c + flutter 2.5 c, tape drive 45 %, width 0.75 | — | RT 1.1/0.3 s, sparse soft ER, HF cut 1.8 kHz | low air, faint hum | the lift (relay clunk, motor spin-up, sheave whine), murmuring voices, muffled doors, soft creaks |
| **Subway** | HP 40, **+11 dB @ 72 Hz**, +6 dB @ 2.6 kHz, metallic combs 412/587 Hz fb 0.86, drift ±16 c + 0.09 Hz wow, width 0.4 | **520/610 ms tunnel echo**, fb 0.52, wavering | RT 4.5/2.2 s, *tunnel* ER (ever later, drifting to one side) | tunnel air, rumble, 31 Hz sub, rectifier hum (360 Hz) | singing rails + far rumble passing, tunnel gusts, clanks, relay clicks |
| **Supermarket** | **HP 480**, LP 5.2 kHz, +4 dB @ 1.7 kHz, chorus 6 c 40 %, **100 Hz tremolo 35 %** | 63/71 ms, bright | RT 1.8/1.4 s, *grid* ER (exactly even) | air, fluorescent hum + buzz, cold-case fan tone, **compressor that cycles on/off** with relay clunks | till beeps, cart rattles, ticks, relay clunks |
| **Office** | HP 300, LP 3.8 kHz, -10 dB scoop @ 750 Hz, **moving notch comb** 640 Hz, chorus 10 c 55 %, width 1.8 | 330/445 ms, fb 0.5, wavering 5 ms, smeared | RT 3.4/3.0 s (unnaturally even), *grid* ER x16, diffusion 1.0 | HVAC air, duct, 120 Hz transformer hum, buzz, 9.4 kHz coil whine, fans | drive seeks, electronic chirps, ticks, far doors |
| **Impossible** | HP 30, +9 dB @ 90 Hz, +6 dB @ 1.48 kHz Q6, **frequency shift -17 Hz (L) / +17 Hz (R) 45 %**, combs 233/330 Hz sweeping, drift, **wandering image** | 410/590 ms, **each repeat a tritone higher** | RT 11/8 s, reflections that **grow**, **octave-up shimmer** | air, a cluster of inharmonic tones whose balance never settles | reversed swells, phantom beating tones, ticks, thuds |

Scene colour cast per place (until Milestone 4 paints each place): none (hospital green),
amber (mall), yellowed (school), sodium orange (garage), cyan (pool), warm faded (hotel),
sickly green (subway), cold white (supermarket), pale grey-green (office), violet (impossible).

## How the places were spread apart (2b)

The test's "character distance" (unchanged from 2a, so the numbers compare) combines the
octave tone profile (3 dB RMS = 1), the tail one second after the chord (8 dB = 1), L/R
correlation (0.25 = 1) and level flicker (1.5 dB = 1). Ten places only stay apart if each
has its own combination, so they were placed deliberately:

| | Low end | Top end | Tail | Width (L/R corr.) |
|---|---|---|---|---|
| Hospital | thin | dark | short (-37 dB) | medium (0.50) |
| Mall | full | dark | long (-13) | very wide (-0.02) |
| School | thin mids | mid | dry (-63) | mono (0.87) |
| Garage | booming | very dark | medium (-30) | narrow (0.62) |
| Pool | thin | bright | long (-18) | wide (0.16) |
| Hotel | full | muffled | dry (-65) | mono (0.98) |
| Subway | sub-heavy | mid | long (-19) | medium (0.46) |
| Supermarket | thin | mid | short (-44) | narrow (0.82) |
| Office | thin | mid | medium (-27) | wide (0.24) |
| Impossible | full | bright | endless (-7) | negative (-0.19) |

## How it is verified (tests group `environments`)

- 30 s of ambience per place, same seed: level (-38 to -44 dBFS for all ten), never
  dropping out, events counted, level movement, octave profile, and the strongest
  repetition of the envelope at 2–12 s (all under 0.55 — no loops).
- Every pair of places: different ambience spectra (closest pair pool / impossible 3.1 dB
  RMS across octaves).
- The same piano phrase in each place: level within 6 dB (loudness is not the
  difference), and the character distance between every pair.
- **Place vs controls, for every instrument:** each instrument's closest pair of places is
  compared with its strongest control swept end to end (in the hospital). Enforced for the
  piano; measured and reported for the other five (see DEVLOG 2b).
- Switching places 16 times while playing: no allocation, no clicks; the old room's tail
  rings on; block-size independence (within -80 dB); Environment Amount 0 is transparent.
