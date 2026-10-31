# Effect captures (pedal-capture milestone)

No pedal brand names appear in the product or the UI. In this document the five units are
referred to by their role: **dark hall** (reverb), **shimmer** (reverb / shimmer),
**tape echo** (tape-delay emulator), **modulated drive** (modulated drive / filter), **flanger**.
(The developer's message names the physical units; the captures will live in the private
Samples repository, never in this public one.)

## Part 1 — inventory (2026-10-31, `AdrianAbbas/Samples` @ `476bfe1`, full history checked)

**No effect captures were found.** The private Samples repository has a single branch
(`main`), five commits, and 25 files; none is a pedal capture, no file is named
`01-pedal-capture-sequence-48k.wav`, and nothing was ever added and later deleted.
No other repository on the developer's account (LIMINAL, Worldbuilder, Dungeon-Crawler,
Dark-Fantasy-Plugin, elsewhere-audio-site, STA130-HW) holds them either.

What *is* in Samples (none of it matches the capture plan):

| Files | What they are (from `notes.txt` and names) | Capture plan match |
|---|---|---|
| `Soft/Medium/Hard 1 (no pick).mp3` | classical guitar, 15-note sweep E2…A#5, three dynamics | none — dry instrument, not reamped through a pedal |
| `Soft/Medium/Hard Hits.mp3`, `Varying Sweep…mp3`, `Wood tapping…mp3` | kalimba | none |
| `Open sustain 1.mp3`, `pizzicato 1/2.mp3`, `hitting bow wood….mp3` | violin | none |
| `Room tone.mp3` | room tone | none (could serve as a noise-floor reference for the recording chain only) |
| ten `* synth * hits sweep*.wav` | Minilogue patches, "dry, line input, **no pedals**" | none |

The kit's **dry guitar phrases** (the source that was to be reamped) are not there either,
so the reamped-phrase comparison in Part 3 has neither side.

## What the pipeline expects (drop the captures here, in Samples)

```
effects/<unit>/<patch>[-static]/01-pedal-capture-sequence-48k.wav   # the kit sequence through the patch
effects/<unit>/<patch>[-static]/phrase-<n>.wav                       # dry phrases reamped through it
effects/dry/phrase-<n>.wav                                           # the same phrases dry
effects/dry/01-pedal-capture-sequence-48k.wav                        # loopback (interface only, no pedal) - strongly recommended
```

`<unit>` is free text (`dark-hall`, `shimmer`, `tape-echo`, `mod-drive`, `flanger`); the tools
read folder names, not brand names. Any layout works as long as each take's file name says
sequence vs phrase; the inventory step lists anything it can't classify.

A **loopback take** of the sequence (interface out → in, no pedal) lets the tools remove the
interface's own response and latency exactly; without it they assume a flat chain and align
on the sync clicks only.

## Part 2 — quality check (tools ready, no real take checked)

`tools/captures/analyze.py` (per take) and `run_all.py` (a whole folder). Per take: clock drift
from the start/end sync clicks (the kit's own spacing, echoes of the clicks ignored); every sweep
segment (sweep + 20 s tail) checked for clicks, dropped buffers and hiss bursts on the raw signal
**and** on the residual left after predicting the segment from the other sweep's response (a
glitch under a loud sweep is invisible in the raw signal, obvious in the residual); noise floor
from the quietest 0.5 s of the tails. Sweep choice: both clean -> averaged; one clean -> that one;
neither -> the less damaged with clicks <= 2 ms repaired (AR interpolation) and the take goes on
the re-record list. Short clicks in the level tone are repaired.

Validated on synthetic takes with known damage (`docs/captures-selftest.txt`): a 64-sample
dropped buffer and a click in sweep 1, a click in the tone, a hiss burst in the quiet sweep's
tail, 30 ppm drift, -84 dBFS floor - all found, no false alarms on clean segments (including a
modulated echo, and a noiseless digital render), drift 30.3 ppm, floor -84.1 dBFS.

## Part 3 — IRs, measurements, engine, models

**Tools (validated, synthetic):** Farina deconvolution (regularised, tapered band edges, harmonic
IRs kept), aligned to the sync clicks, cut where the response meets the noise floor (after the
*last* block above it, so an echo's silent gaps don't end it). Measurements: 1/3-octave EQ;
RT60 per octave (Schroeder, noise-compensated, past the direct sound) - 4.0/3.0/1.6 s hall came
back 3.7-3.9/2.98/1.62 s; echo time (detrended envelope autocorrelation) - 380.03 ms for 380;
feedback at 1 kHz and darkening (1/6-octave 4 kHz vs 500 Hz) per repeat, from the loud **and**
the quiet sweep; wow/flutter (FM) and AM from the tone - 0.704 Hz / 6.1 cents and 6.0 Hz /
1.43 cents for 0.7/6 and 6/1.5; THD from the harmonic IRs at -12 and -24 dBFS; level
dependence (quiet vs loud sweep per octave; 0 dB on a linear hall); noise-burst levels.
Reamped-phrase comparison (`analyze.compare`): level, 1/3-octave spectral and 50 ms envelope
difference - 0.36 dB envelope on the synthetic hall.

**Real-time convolver** (`Source/DSP/Convolver.h/.cpp`, tests group `convolver`): non-uniform
partitioned, **zero latency** (128-tap direct-form head + frequency-domain stages of 128, 512,
2048, 8192, each starting at twice its partition so its FFTs and multiply-adds are spread over a
whole partition period; stage *s* does its FFT and inverse FFT in different blocks from every
other stage). Reports 0 samples latency, and an impulse returns the IR from sample 0. Matches
direct convolution to -134 dB with random host blocks of 1-1024. Never allocates on the audio
thread. Quality caps the IR: Eco 1.5 s, Standard 3.5 s, High 8 s. CPU (this container, one
core): stereo 3.5 s IR = **2.8-2.95 % average**, costliest point of the stage cycle ~21 % of a
128-sample block (the 16k FFT slot), ~1440 ops/sample. Not measured on a Mac here; the Mac build
uses Apple's FFT (vDSP) through JUCE, so expect less.

**Matched models** (`Source/DSP/CaptureModels.h`), each driven by a profile with exactly the
fields the tools measure:
- `TapeEcho` - echo time, feedback per repeat (1 kHz), darkening per repeat (a one-pole solved
  for the measured 4 kHz/500 Hz tilt, its 1 kHz loss made up), low cut, wow and flutter
  (rate + cents, as delay modulation), saturation in the loop. **Closed loop:** the kit rendered
  through the plugin's model and analysed gives back 380.03 ms, -6.14 dB/repeat, -2.27 dB
  darkening (quiet sweep), wow 0.70 Hz / 6.1 cents, flutter 6.0 Hz / 1.4 cents.
- `ModVoice` - chorus/flanger from rate, base delay, depth, regeneration.
- `OctaveShifter` - +12 for shimmer (880 Hz out of 440 Hz, 40 dB above the original).

None of this is routed into a room yet: there is no captured response or measured profile to
put there, and invented ones would be exactly the "bland and fake" the milestone is replacing.

## Part 4 plan — which capture goes where (once the captures exist)

| Room | Capture (role) | Engine | Why |
|---|---|---|---|
| Abandoned School, Empty Hotel | tape echo, worn patch | TapeEcho profile (+ its IR for the head/tone) | nostalgia: darkening repeats and slow wow, short times inside the rooms' small RT60 |
| Deserted Subway Station | tape echo, dub patch | TapeEcho, longer time, higher feedback | the receding tunnel echoes - each repeat further and darker |
| Impossible Room, Underground Parking Garage (tail) | dark hall | convolution | a captured huge, dark tail; for the Impossible Room, the "too big for the room" mismatch |
| Abandoned Indoor Pool | shimmer | convolution of the static take + OctaveShifter in the tail at a very low level | tiled shimmer, kept still (no audible movement), per Direction Update 2 |
| Empty Shopping Mall, Empty Supermarket | flanger, subtle | ModVoice from the measured rate/depth, as width | slow, barely-there widening |
| Hospital, Office | (none planned) | - | keep their own short, hard / dead rooms |

Rules kept: steady hums, rare events, no risers or noise waves; each room level-matched before
and after (the `rules` and `distinct` test groups stay green).

## Status of the milestone's parts

| Part | Status |
|---|---|
| 1 Inventory | done: nothing to inventory (above) |
| 2 Quality check | tools done and validated on synthetic takes; **no real take checked** |
| 3 IRs, measurements | tools done and validated; **no IR or measurement of the real units exists** |
| 3 Engine + models | convolver and models built and tested in the plugin's test suite; closed loop with the tools |
| 3 Comparison with reamped phrases | tool done; **blocked**: no phrases |
| 4 Rooms | **blocked**: plan above, nothing changed |
| 5 Listening render | **blocked**: before = after, so no render was made |

## Next session (after the upload)

```
python3 -I tools/captures/run_all.py <Samples>/effects build/captures --kit <kit file>
```
then read `build/captures/summary.md` (re-record list, measurements), convert the chosen IRs and
profiles into the plugin (fetched with the CI secret like the planned sampler - the captures stay
private), route per the table above, re-run `rules`/`distinct`, render before/after.
