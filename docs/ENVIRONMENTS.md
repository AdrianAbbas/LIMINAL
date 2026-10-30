# Environments (Milestone 2a)

Each place is a complete configuration in `Source/DSP/Environment.cpp` (profile) and
`Source/DSP/Ambience.cpp` (its sounds). The developer's priority: **the environment is
the main source of sound transformation** — changing place must change every instrument
more than any instrument control does. So each place has four layers:

1. **Instrument processing** (`Colour`, scaled by *Environment Amount*): high-/low-pass,
   a resonance, a high shelf, pitch wow/flutter/random drift (as a swinging delay — a
   wobble around the pitch, never a glide), a detuned "second player" chorus, tape-like
   saturation, stereo width, and how near the instruments sound (dry level).
2. **Echo**: a tuned stereo two-tap delay with a band-passed feedback loop (the far wall).
3. **Room** (`Space`, scaled by *Space*): reflection pattern from the geometry (flutter
   between parallel walls / dense irregular concrete / sparse distant surfaces), input
   diffusion, an 8-line FDN with separate RT60 below and above a crossover, slow line
   modulation, output band limits, tail width.
4. **Ambience**: a bed from place-specific generators and incidental events from
   place-specific event types, each scheduled as a Poisson process above a minimum gap
   with random parameters (so nothing loops); events are partly sent into the room.

Numbers are design choices *(estimate)* guided by typical acoustics: RT60s of long hard
corridors (~2–3 s), large atria (5–8 s), narrow school corridors (~1–1.5 s), concrete car
parks (~3–4 s, bass-heavy) and tiled pools (4–6 s, bright, flutter echoes) are in the
ranges usually quoted for such spaces; flutter-echo spacing = 2 x wall distance / 343 m/s
(2.4 m corridor ≈ 14 ms; 1.1 m hallway half-width ≈ 6.5 ms).

| | Hospital | Mall | School | Garage | Pool |
|---|---|---|---|---|---|
| **Instruments** | HP 110 Hz, LP 3.2 kHz, +6 dB @ 2.4 kHz Q3 ("cold"), drift ±9 c + wow 4 c, width 1.2, -4 dB | HP 60, LP 7 kHz, shelf -7 dB > 3 kHz, chorus 7 c 0.37 Hz 60 %, width 1.45, -8 dB ("washed out") | HP 210, LP 4.3 kHz, +4 dB @ 1.1 kHz, wow 7 c 0.55 Hz, flutter 3.5 c 6.5 Hz, chorus 11 c 40 %, tape drive 50 %, width 0.5 | HP 35, +9 dB @ 170 Hz, LP 2.5 kHz Q0.9, -4 dB | HP 90, shelf +6 dB > 5 kHz, +2 dB @ 3.2 kHz, width 1.45, -4 dB |
| **Echo** | 241/367 ms ping-pong, fb 0.38, 300–2200 Hz | — | 37/44 ms slap, fb 0.3, dark | 96/113 ms, fb 0.18, dark | 173/291 ms ping-pong, fb 0.45, bright |
| **Room** | RT 2.4/1.5 s (x-over 1.8 kHz), flutter 11 ms x12, pre 22 ms | RT 6.8/3.6 s, size 2.4, pre 60 ms, sparse far ER, diffuse, modulated | RT 1.5/0.6 s, size 0.55, flutter 6.5 ms x14, narrow | RT 3.6/1.4 s (x-over 700 Hz), dense strong ER x16, unmodulated, HF cut 3.8 kHz | RT 4.6/4.2 s (bright), sparse strong ER, modulated, HF to 14 kHz |
| **Bed** | air, 60 Hz hum, stuttering fluorescent buzz | low roar (brown noise), far ventilation, escalator motor + step clacks | air, whistling duct modes (340/690 Hz), 50 Hz ballast hum + buzz | deep rumble, 26 Hz fan with beating, 38 Hz structural sub, faint hum | air, water lapping in slow irregular waves, low pump |
| **Events** | monitor beeps (1–3 tones), vent rattles, metal clanks | steel groans (narrow resonances, slowly bending), distant clanks, distant thuds | creaks (stick-slip), pipe water rushes with a knock, cooling ticks, a far door | cars passing on another level (engine + tyres, panning, doppler), expansion-joint double thuds, clanks, a gate rattle | drips (bubble "plinks", sometimes a leak dripping for a while), bigger plops, ladder pings |

The scene's colour cast per place (2a only; Milestone 4 paints each place): none
(hospital green), warm amber (mall), yellowed (school), sodium orange (garage), cyan (pool).

## How it is verified (tests group `environments`)

- 30 s of ambience per place, same seed: level (-40 to -42 dBFS for all five), never
  dropping out, events counted, level movement, octave profile, and the strongest
  repetition of the envelope at 2–12 s (all under 0.25 — no loops).
- Every pair of places: different ambience spectra (closest pair 4.2 dB RMS across octaves).
- The same piano phrase in each place: level within 6 dB (loudness is not the
  difference), and a "character distance" (tone, tail, width, flicker) between every pair.
- Changing place vs each piano control swept 0 → 1 (in the hospital).
- Switching places 16 times while playing: no allocation, no clicks; the old room's tail
  rings on; block-size independence (within -80 dB); Environment Amount 0 is transparent.
