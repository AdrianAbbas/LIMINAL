# Per-instrument effects (Milestone 3)

Code: `Source/DSP/Effects.h/.cpp` (`EffectRack`, `placeEffects`). Parameters:
`<instrument>_fx_<name>` (32 per instrument, listed in `Source/Parameters.h`, `params::fx`).

## Signal path (per instrument)

```
instrument -> Distance -> Drift -> Tape -> Filter -> Chorus -> Granular -> Echo   (Standard order)
                       -> the place's processing (filters, resonances, pitch, width, nearness)
                       -> Effects Mix with the dry instrument
                       -> sends: the place's room (Reverb module) and the place's own echo
```

Orders: **Standard** (as above), **Texture first** (Granular, Drift, Tape, Filter, Chorus,
Echo), **Echo into texture** (Drift, Tape, Filter, Chorus, Echo, Granular). The place's
processing always comes last: the place shapes everything the instrument makes.

## Modules

| Module | What it does | Controls |
|---|---|---|
| Drift | slow random pitch wander (up to ±40 cents x Wander), the player's part detunes L against R | amount, rate (0.05–2 Hz) |
| Tape | wow (0.55 Hz) + flutter (6.8 Hz), bandwidth loss (18 kHz → ~1.6 kHz), saturation, hiss that follows the music (silence stays silent), dropouts | amount, tone, hiss, dropouts |
| Reverb | how much of this instrument goes into the **place's room** (its acoustic profile) | room send |
| Echo | stereo delay 20–1500 ms or synced to the host tempo (1/16 … 1 bar), feedback through a band-pass (tone), the player's echo ping-pongs | amount, time, sync, feedback, tone |
| Chorus | three modulated voices, 9–20 ms | amount, rate |
| Filter | resonant low-pass (SVF) + high-pass | amount (wet), low-pass, high-pass, resonance |
| Granular | eight-grain cloud from the last 2 s: size 30–300 ms, spread = position scatter + pitch scatter, above 50 % some grains an octave away; Freeze stops recording | amount, size, spread, freeze |

## Two layers

Every module's effective setting = the **player's own** setting + the **place's default
for this instrument** x **Environment Influence** x the global Environment Amount. The
place's processing (from Milestone 2) is scaled the same way. So:

- Influence 0 and the player's amounts at 0: the instrument exactly as it was (tested
  bit-exact against Environment Amount 0).
- A module's switch bypasses both layers. Every switch off = the dry instrument (bit-exact).
- Effects Mix 0 = the dry instrument (also the place's echo send).
- The echo's time/feedback are the player's when they turn the echo up, else the place's.
- The drawer's last line shows what the current place adds to the instrument shown.

## Place defaults (piano; other instruments adjusted: synth more chorus/tape, glass more echo and room, choir more granular and less chorus, music box more tape and drift, drone more granular, less echo)

| Place | Drift | Tape | Chorus | Granular | Echo | Filter | Room |
|---|---|---|---|---|---|---|---|
| Hospital | 25 | – | – | – | 12 @ 420 ms | LP -1 oct, HP +0.8 oct (cold) | -30 % (short) |
| Mall | 20 (gentle detune) | 10 | 30 | 8 (long grains) | – | LP -1.2 oct (soft highs) | +80 % (long, diffuse) |
| School | 30 | 60 (worn cassette) | 15 | – | 10 @ 180 ms | LP -0.8, HP +0.5 | -20 % |
| Garage | – | 10 | – | – | 20 @ 300 ms | LP -1.5 oct | +30 % |
| Pool | – | – (minimal) | – | – | **35 @ 340 ms, fb 45 %** | HP +0.4 | +60 % (bright tiles) |
| Hotel | 10 | 50 | – | – | – | LP -0.5 | -50 % (intimate) |
| Subway | 40 | – | – | 10 | 30 @ 650 ms, fb 50 % | – | +40 % |
| Supermarket | 15 | 30 | **45** | – | – | HP +0.6 (thin) | -60 % |
| Office | – | – | 35 | 20 | 25 @ 500 ms, fb 55 % | LP -0.7 | +40 % |
| Impossible | **60** | – | 20 | **50** | 20 @ 777 ms, fb 60 % | – | +80 % |

## Smoothness and safety

Settings glide on a 32-sample control clock (~25 ms); filter cutoffs glide in octaves;
echo time glides (100 ms); a change of place cross-fades the place's processing over
60 ms. Echo feedback is soft-clipped in the loop (tested: 95 % feedback on all six for 20 s
stays under 0 dBFS). No allocation after `prepareToPlay` (tested while switching places).
