# Effect captures (pedal-capture milestone)

No pedal brand or model names appear in the product, the UI, the code or these docs: the units
are named by role. The captures stay in the private Samples repository; nothing derived from
them is committed here (CI fetches and prepares them with the `SAMPLES_TOKEN` secret).

## Part 1 — inventory (Samples @ `7e4234d`, "Effects Patches", 2026-10-31)

| File | What it is | Capture plan |
|---|---|---|
| `01-pedal-capture-sequence-48k.wav` | the capture kit itself, dry (left channel; right silent) | used as the reference: its own sweeps, clicks and layout |
| `Notes.wav` | the dry guitar phrases (236 s, stereo) | dry originals for the reamp comparisons (alignment confirms: every reamp lines up within 1-6 ms) |
| `Ergosphere tape delay 48k.wav` / `… phrases.wav` | **tape echo** patch (mono) | sequence + reamped phrases; no static take |
| `Patch 1- … Shimmer 48k.wav`, `… 48k 0 rate.wav` / `… phrases.wav`, `… phrases 0 rate.wav` | **shimmer** patch, modulated and static ("0 rate") | sequence x2 + phrases x2 |
| `Patch 2- Abyssal Reverb and Delay 48k.wav` / `… phrases.wav` | **dark reverb + delay** patch (stereo) | sequence + phrases (the phrases file is the same 236 s then ~136 s of digital silence) |
| `Wobbly chorusmodulator +reverb 48k.wav` / `… phrases.wav` | **chorus modulator into a reverb** (mono) | sequence + phrases; no static take |
| `.DS_Store` | macOS folder file | ignored |

Unclear / assumed (recorded instead of asking):
- **Every file is 44.1 kHz** although named "48k" (the kit too). The tools resample to 48 kHz and
  deconvolve against the kit file's own sweeps, so the mismatch costs nothing.
- **Which unit made which file** is not stated. By name and behaviour: the shimmer patch = the
  shimmer unit; "Abyssal Reverb and Delay" = the dark reverb (+ delay) unit; "Ergosphere tape
  delay" = the tape-echo unit (it has a clear 270 ms repeat - but its name could also be a patch
  of the dark reverb unit, since "Patch 1/Patch 2" suggests two patches of one unit); "Wobbly
  chorusmodulator +reverb" = the modulator unit into a reverb.
- **No flanger capture** was uploaded: the Mall/Supermarket width uses the chorus modulator's
  measured modulation instead. **No static take** for the tape echo or the modulator.
- The kit layout (measured from the kit file): tone 2-5 s, clicks 7.0/7.5/8.0 s, sweeps 11.5 s
  and 46.5 s, impulse 81.5 s, bursts 92.0/98.0/104.2 s (50/200/1000 ms), quiet sweep 111.25 s,
  end clicks 146.25/146.75/147.25 s.

## Part 2 — quality check

`tools/captures/run_all.py` (QC + analysis of every take), `prepare.py` (the plug-in's assets).
Glitches: clicks and dropped buffers on the raw signal **and** on the residual after predicting
each sweep from the other sweep's response; hiss bursts in the tails. The detector was tuned on
the kit file itself (a perfect loopback must report nothing - it now doesn't) and on synthetic
takes with known damage (`docs/captures-selftest.txt`, 30/30).

| Patch | Clock drift | Noise floor (recording) | QC | IR made from |
|---|---|---|---|---|
| tape echo | 0.0 ppm | -79.3 dBFS (low hum) | sweep1: clean, sweep2: clean, sweep_quiet: clean, tone + pre-roll: clean | average of both sweeps |
| shimmer (static take for the IR) | -3.4 ppm | -69.5 dBFS | sweep1: clean, sweep2: clean, sweep_quiet: 1 event(s) (dropout), tone + pre-roll: 1 event(s) (click) | static take: average of both sweeps |
| dark reverb + delay | -7.3 ppm | -67.0 dBFS | sweep1: 1 event(s) (click), sweep2: clean, sweep_quiet: 2 event(s) (click), tone + pre-roll: clean | sweep 2 only (sweep 1 damaged) |
| chorus modulator + reverb | 0.0 ppm | -81.1 dBFS | sweep1: clean, sweep2: clean, sweep_quiet: clean, tone + pre-roll: clean | average of both sweeps |

- **dark reverb + delay:** a 3-sample click in sweep 1 at 21.08 s (seen in the raw signal *and*
  the residual: a real recording glitch) -> sweep 2 used alone; two small clicks in the quiet sweep
  (112.92 s, 119.74 s) -> its level-dependence numbers are unreliable.
- **shimmer (modulated take):** a dropped buffer (16+ identical samples) at 126.60 s in the quiet
  sweep's tail; the "tone" events are the shimmer's own repeats of the sync clicks, not glitches.
  The IR comes from the clean static take anyway.
- Noise floors: the tails carry a low hum (the broadband floor sits ~10 dB above every octave
  band from 125 Hz up). Handled by truncating each IR where it meets **its own** noise floor
  (the deconvolved response's last 2 s) and noise-compensating the decay fits.
- **Re-record:** nothing is unusable. Worth re-recording if convenient: the dark reverb + delay
  take (both its sweep 1 and its quiet sweep have clicks - one clean loud sweep is all the IR
  has), and static ("minimum modulation") takes of the tape echo and the chorus modulator. A
  longer level tone (10 s+) would let the tools resolve modulation slower than ~1.3 Hz.

## Part 3 — measurements, IRs, engine, models

Measured (48 kHz IRs, wet + dry as captured):

| Patch | RT60 125 / 500 / 1k / 4k / 8k Hz (s) | Echoes | Modulation (wet tail of the tone) | THD -12 / -24 dBFS |
|---|---|---|---|---|
| tape echo | 4.532 / 7.192 / 5.888 / 2.309 / 1.644 | 270.0 ms, first repeat -19.3 dB re direct, then a diffuse tail | wow 3.99 Hz / 1.89 c, flutter 16.829 Hz / 3.153 c | 1.574 / 1.101 % |
| shimmer (static) | 10.045 / 10.996 / 10.183 / 8.438 / 6.865 | none (a reverb) | wow 1.175 Hz / 7.736 c, flutter 10.429 Hz / 8.295 c | 27.768 / 27.813 % |
| dark reverb + delay | 7.605 / 7.284 / 5.892 / 2.168 / 1.659 | none (a reverb) | wow 3.985 Hz / 10.517 c, flutter 16.719 Hz / 12.792 c | 1.502 / 2.403 % |
| chorus mod + reverb | 7.244 / 6.886 / 5.812 / 2.038 / 1.049 | none (a reverb) | wow 2.848 Hz / 21.088 c, flutter 7.004 Hz / 14.661 c | 2.192 / 0.948 % |

- The shimmer's "THD" (25-32 %) is its octave: a sweep puts octave-up energy where a 2nd
  harmonic would be (-11.5 dB re the linear response).
- Level dependence (quiet vs loud sweep, per octave): tape echo and chorus mod within +/-0.5 dB to
  2 kHz (linear); shimmer -1.5 to -2.4 dB below 2 kHz (slight expansion); dark reverb + delay
  unreliable (its quiet sweep has clicks).
- Noise bursts (50 ms / 200 ms / 1 s at the kit's level): first-40 ms and end levels within
  ~1 dB for every patch - no compression or gating at the kit's level.
- Modulation was measured on the 1.4-1.8 s wet-only tail after the level tone, so rates below
  ~1.3 Hz cannot be seen; for the reverbs the "FM" is partly the tail's own decorrelation.

**Tape-echo profile** (the plug-in's `TapeEcho`, from the measurements): echo 270.0 ms;
feedback -2.75 dB per repeat and darkening -4.76 dB per repeat (4 kHz vs
500 Hz) - from the decay of the smeared repeats per band, since only the first repeat stands
clear of the tail; wow 3.99 Hz / 1.89 cents, flutter 16.829 Hz / 3.15 cents.
**Modulation profile** (`ModVoice`): 2.848 Hz, 21.088 cents (= 0.681 ms of delay swing).

**Engine** (`Source/DSP/Convolver`): zero-latency non-uniform partitioned convolution (reported
0, tested); shared spectra (both room chains play one IR's partitions by pointer); real-time
Quality cap (Eco 2 s, Standard 6 s, High 8 s) and decay reshaping (Character decay x Scale) by
per-partition gains, skipping silent partitions and stages; `Source/DSP/Captures` builds the IRs
for the host rate in prepareToPlay (resampled, energy-kept, decay-shaped per room).

### Emulation vs the reamped phrases (`tools/captures/compare_models.py`)

Dry `Notes.wav` through the emulation, aligned to the real reamp, level-matched, then compared
(1/3-octave spectral RMS difference; 50 ms envelope RMS difference):

| Reamped take | Emulation | Spectral | Envelope | Reamp level vs kit level |
|---|---|---|---|---|
| `Ergosphere tape delay phrases.wav` | captured IR | 1.11 dB | 3.01 dB | +7.34 dB |
| `Ergosphere tape delay phrases.wav` | tape model (measured profile) | 3.68 dB | 3.5 dB | -2.93 dB |
| `Patch 1- Shimmer phrases 0 rate.wav` | captured IR | 3.99 dB | 3.11 dB | +13.92 dB |
| `Patch 1- Shimmer phrases.wav` | captured IR | 4.22 dB | 3.37 dB | +13.19 dB |
| `Patch 2- Abyssal Reverb and Delay phrases.wav` | captured IR | 4.48 dB | 3.44 dB | +10.21 dB |
| `Wobbly chorusmodulator +reverb phrases.wav` | captured IR | 2.15 dB | 3.73 dB | +4.92 dB |

Reading it: the captured tape-echo IR is closest (1.1 dB) - an IR holds the patch's EQ and its
smeared repeats; the tape *model* alone (dry + measured echo, no diffuse tail) is further off
(3.7 dB) but adds what the IR can't: moving wow/flutter and repeats that keep darkening. The
shimmer IR misses the octave above ~6 kHz by 6-14 dB (the octave is not linear; the plug-in adds
it with the octave shifter). The dark reverb + delay IR is 4-6 dB short at 1.5-4 kHz and 5-11 dB
too strong below 80 Hz against its phrases: the phrases were played hotter (+10 dB) and this
patch is level-dependent (and its quiet sweep is damaged, so that can't be modelled here).
Envelope differences of ~3-3.7 dB everywhere come from the modulated tails (and, for the
reverbs, their randomness), which a fixed IR can't follow sample by sample. The reamps were
recorded 5-14 dB hotter than the kit; the comparison removes that level difference.

## Part 4 — which capture went where, and why

| Room | Capture | How | Why |
|---|---|---|---|
| Empty Shopping Mall | chorus mod + reverb | IR as the atrium's late field (room late x0.4) + the measured modulation as a 30 % width on the wet | its ~6.3 s tail fits the mall's 6-8 s; slow, subtle movement in a huge space (stands in for the flanger, not uploaded) |
| Abandoned School | tape echo | measured profile, faint (-14 dB), 6 dB less feedback, 6 kHz | nostalgia: worn, darkening, wobbling repeats in a short hallway |
| Underground Parking Garage | dark reverb + delay | IR shortened to ~3 s (room late x0.3) | the garage's dark concrete tail |
| Abandoned Indoor Pool | shimmer (static take) | IR shortened to ~4.5 s (room late x0.6) + its octave, feed-forward, 18 dB under the tail | the tiled shimmer, kept still (Direction Update 2: alive, not moving) |
| Empty Hotel | tape echo | measured profile, very faint (-20 dB), 8 dB less feedback, 2.4 kHz | nostalgia through the wall |
| Deserted Subway Station | tape echo | measured profile at its own feedback, -12 dB, 9 kHz | repeats receding down the tunnel, darker each time (the dub patch was not uploaded) |
| Empty Supermarket | (chorus mod's modulation) | 25 % width on the wet | subtle width (stands in for the flanger) |
| Impossible Room | dark reverb + delay | IR as captured, ~6.6 s (room late x0.4) | a tail too big for the room |
| Vacant Hospital, Endless Office | none | - | their short, hard / dead rooms stay as they were (bit-exact, tested) |

Kept: each room's reflections and signature (only part of the algorithmic *late* tail is handed
over), Direction Update 2's rules (the `rules` group passes: steady hums, rare events, no risers
or noise waves), and the level: every room's output for the standard phrase is within
+/-0.8 dB of before (solved per room from three renders, then corrected once). The captures follow
the room's controls: Character decay and Scale reshape the captured tail's RT60 and the tape's
repeats; Character wet level moves the tape; Space scales the captured tail like the room.

## Part 5 — listening render

`LiminalTests --render-captures build/test-logs/captures-rooms.wav` (CI: the Linux job's
test-logs artifact) writes `-before.wav`, `-after.wav`, `-ab.wav` (each room before, then after)
and `-cues.txt`. CPU and plugin size: see the DEVLOG section of 2026-11-01.
