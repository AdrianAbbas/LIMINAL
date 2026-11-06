# TAPEWORN FX

An audio **effect** by **Windward Audio**, built from this repository alongside the instrument
(one repository, two plug-ins, as DUNGEON CRAWLER / LIGHT SEEKER): any sound - guitar, vocals,
synths, a full mix, drums - goes through TAPEWORN's world, everything except the instruments
themselves.

| | TAPEWORN (instrument) | TAPEWORN FX (effect) |
|---|---|---|
| AU | `aumu` / `Lmnl` / `Veil`, "Elsewhere Audio: TAPEWORN" | `aufx` / `Tpfx` / `Wnwd`, "Windward Audio: TAPEWORN FX" |
| VST3 | Instrument / Synth | Fx / Reverb |
| Bundle ID | `com.elsewhereaudio.tapeworn` | `com.windwardaudio.tapewornfx` |
| Bundles | `TAPEWORN.component`, `.vst3`, `.app` | `TAPEWORN FX.component`, `.vst3` |
| Presets | `~/Library/Elsewhere Audio/TAPEWORN/Presets` | `~/Library/Windward Audio/TAPEWORN FX/Presets` (`.tapewornfxpreset`) |
| Latency | 0 | 20 ms (reported; hosts compensate) |

The instrument's codes, names and parameters are unchanged.

## What it does

```
input -> Input -> LIMINALITY (pitch line, Ghost, Warble, Wear, Unevenness, Fade)
      -> the place's processing (its Colour and default effects, sends with Timing)
      -> the place (signature, echo, room with the captured spaces / convolution, room sound)
      -> the room's level match -> Mix with the input (delayed by the latency) -> Output
      -> Bypass (25 ms S-curve to the input, delayed by the same latency)
```

- **The places** - the ten rooms of the instrument (`DSP/Environment`, `DSP/Signature`,
  `DSP/Effects`, `DSP/Captures`), each with its three **Characters**, evolution and the four macros
  (Presence, Scale, Activity, Unreal), Far Away. Changing room cross-fades two complete chains as
  in the instrument; every room and Character is **level-matched** to what comes in.
- **LIMINALITY for audio** (`Fx/FxLiminality`): the instrument's stage works on notes; an effect
  has no notes, so every control acts on the sound itself:
  - **Detune** - a subtle constant pitch shift (flat, like a slightly slow tape; ~28 cents at 100 %,
    the two sides a few cents apart)
  - **Drift** - the pitch wanders away over seconds and comes back
  - **Wow & Flutter** - tape wow (~0.6-0.9 Hz) and flutter (~7-12 Hz)
  - **Warble** - each side wavers on its own; a slow swell and a faint chorus that grow back after
    each attack
  - **Ghost** - faint delayed (55-95 ms), slightly detuned copies
  - **Wear** - tape / VHS degradation: lost highs and lows, soft saturation, hiss and crackle under
    the playing, occasional dropouts (level-matched)
  - **Timing** - the delayed and wet paths (the room and echo sends) smeared and jittered: their
    delay jumps (cross-faded, never glided, so no pitch artefacts) between 0 and ~45 ms
  - **Unevenness** - slow fluctuation of level (about +/-3.5 dB) and tone
  - **Sour Notes** - occasional short pitch sags (30-60 cents down, a quarter to half a second),
    most often right on an attack
  - **Fade** - now and then the sound fades away (and darkens) and returns
  - the **LIMINALITY** macro: 0 % clean, 40 % the amounts as set, 100 % 2.5x them; the place and
    its Character nudge the amounts as in the instrument.

  The pitch controls share one *pitch line* per side: a delay read at a moving position, centred
  on the reported latency, so the shifted sound stays in time with the track on average. At the
  edge of its range it jumps back towards the centre to the point whose recent sound best matches,
  cross-faded over 25 ms (no click, no comb). At every amount 0 it settles exactly on the centre:
  the input delayed by the latency, bit for bit.

## Controls

| Control | Default | |
|---|---|---|
| Mix | 100 % | 0 % = the input untouched (delayed by the latency); 100 % = only TAPEWORN's world, which carries its own near sound of the source (an insert). Lower it for parallel use. |
| Intensity | 70 % | how strongly the place transforms the sound: its processing (Environment Amount) and, with it, its room (Space x Intensity / 70 %) |
| Input, Output | 0 dB | +/-24 dB; Input drives the world (its saturation), Output the result |
| Room | Vacant Hospital | the ten places (the film strip, or the PLACE page) |
| Character | 1 | three per place |
| Space | 45 % | how much of the room |
| Room Sound | -12 dB | the place's own bed and rare events - only while something plays (they fade out a few seconds after the input stops: silence in, silence out) |
| Presence, Scale, Activity, Unreal, Evolution, Far Away | as the instrument | |
| LIMINALITY + its ten controls | 40 %, own amounts | see above |
| Bypass | off | the host's own bypass uses it; a 25 ms S-curve, then the input exactly |
| VHS look, Quality | Light, Standard | |

**Level match** (`Fx/FxLevelMatch.h`): a calibrated trim per place and Character
(`TapewornFXTests --calibrate-levels`, `tools/levelmatch/fx_levelmatch.py`) plus a slow automatic
make-up (K-weighted loudness of the input against the room's output; since a start or a change of
room the totals so far, then the last ~8 s; within +/-12 dB; only while something plays). It goes
on the near sound and on what the place being heard adds; a place fading out keeps its own level,
so its tail is not turned up or down by the next room. LIMINALITY's own level changes (Fade,
Unevenness) are left alone.

**Mono and stereo**: stereo, mono, and mono in / stereo out.

**Real-time**: everything is allocated in `prepareToPlay`; no locks; ~2.5 % of one core here
(default settings, 48 kHz stereo).

## Presets

26 factory presets by source (VOCALS, GUITAR, KEYS, DRUMS, MIX, ANY) plus your own (YOURS):
Mall at 3 AM, Answering Machine, Fluorescent Corridor, Room 404, Dream State; Hallway Guitar,
Distant Movement, Last Train Home, Forgotten Summer; Hotel Lobby Piano, Night Shift Keys, Infinite
Cubicles, Supermarket Muzak; Empty Pool Drums, Parking Level B3, Tunnel Snare; Worn VHS Mix,
Childhood Tape, Distant Radio, Clean Room; Fading Memory, Something Wrong, Fluorescent Nightmare,
Submerged, Beyond Reality, Unstable. INIT (no preset), RESET, UNDO / REDO; **A / B** compares two
settings (B starts as a copy of A; switching is undoable; A>B copies the one shown). The song keeps
both.

## The editor

The instrument's viewfinder (`UI/Viewfinder`, now shared): the place's photographic or painted
scene with the VHS look, the camcorder overlay, the film strip of places, the Character chips and
the clean place changes; the lights answer the input's attacks. The strip holds MIX, INTENSITY,
INPUT, OUTPUT, SPACE, ROOM SOUND and BYPASS (bypassed, the picture dims); the big LIMINALITY knob
sits on the scene; A / B beside the page buttons PLACE, LIMINALITY and PRESETS. Double-buffered
(the scene is a cached picture, composited on its own thread) and cheap (frame governor).

## Shared code

Both plug-ins compile the same sources for the places (`DSP/Environment`, `Signature`, `Effects`,
`Characters`, `Space`, `Convolver`, `Captures`, `Ambience`, `Liminality`) and the editor's
viewfinder and parts (`UI/Viewfinder`, `UI/Widgets`, `UI/SceneArt`, `UI/PhotoScene`, `UI/Vhs`);
`Brand.h` gives each its name (`TAPEWORN_FX` is defined for the effect's target). The effect's own
files are in `Source/Fx/`.

## Tests

`TapewornFXTests` (Linux CI runs it whole; macOS runs its smoke groups on both slices):
smoke, parameters, latency (Mix 0 and Bypass are the input exactly, delayed by the reported
latency, at 44.1 / 48 / 96 kHz; the host's own bypass too), presets (every one transforms its
source and stays within 4 dB; INIT / UNDO / REDO; A / B; your own), state, mono (mono, mono in /
stereo out), clicks (Bypass, room and Character changes, the pitch line's wraps), levels (all 30
rooms / Characters on guitar, vocal and a full mix), strength (every room and Character, and
LIMINALITY 50 / 100 %, on the three sources), realtime (no allocation, block-size independence,
determinism, CPU) and editor. The three test sources are synthesised in the test
(`--write-sources dir` writes them), so it needs no files. `--render dir` writes the listening
render (see the DEVLOG).

macOS CI: `auval -v aufx Tpfx Wnwd` and `tools/au-harness/TapewornFXAUHarness` (adapted from
DUNGEON CRAWLER FX's effect harness: latency-compensated input / output comparison, host bypass,
Mix 0, Output, Room, presets, state, the editor view) in-process and out-of-process, arm64 and
x86_64, after installing the quarantined package with `install-mac.sh`.
