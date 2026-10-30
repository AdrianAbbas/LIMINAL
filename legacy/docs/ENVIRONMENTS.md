# Milestone 2: ten core location models

Choose from **CORE ENVIRONMENTS** at the top of the preset menu. Previous factory
configurations remain under LEGACY FACTORY PRESETS; legacy sessions restore with
model -1 and retain their original engine. New instances start in Vacant Hospital.
Previous/next cycles core locations while a core model is selected. Switching a
location preserves the selected instrument families and levels, master and bus
settings except the location's calibrated ambience default. It sets architectural
geometry, material, event/evolution defaults and the common musical processing.

| Location | Procedural identity | Musical processing |
|---|---|---|
| Vacant Hospital | Narrow-band airflow, electrical infrastructure, short dual equipment tones and distant metallic excitation | Clinical low-pass tone, small unstable delay, corridor reflections |
| Empty Shopping Mall | Escalator drive harmonics, broad ventilation, irregular structural clanks | Broad glass atrium, diffuse tail, cross-channel echoes |
| Abandoned School | Duct bands, slowly bending pipe modes, creaking mechanical excitation | Warm/dark wood corridor, flutter and short echoes |
| Underground Parking Garage | Low fan blades, pressure oscillator, hollow filtered impacts | Concrete early reflections, low-mid coloration, darkened instrument path |
| Abandoned Indoor Pool | Quiet air and motor, gliding synthesized water drops, tiled modal echoes | Bright bandwidth, distinct echoes, long tiled reflections |
| Empty Hotel | Quiet filtered room tone, slow elevator frequency ramps, low muffled impacts | Carpet absorption, soft dark coloration, mild unstable delay |
| Deserted Subway Station | Tunnel airflow, low mechanical pressure, independently oscillating rail-like modes | Metal coloration, long cross-channel tunnel echo, pitch movement |
| Empty Supermarket | Random 17–46 second compressor duty cycles, harmonic motor/electrical sources, switching/rattling events | Thin commercial-room coloration and gentle chorus-like delay |
| Endless Office | Transformer motor banks, spectral airflow bands, sparse electronic activity | Carpeted repeated space, stronger modulated delay and filtered synth tone |
| Impossible Room | Inharmonic low pressure and stochastic modal disturbance | Very long metal room, inharmonic motion and time-varying delay/pitch-warped echoes |

These are original synthesized approximations, not recordings, exact physical
simulations or intelligible speech. Each has independent seeded random state,
minimum event spacing and different source/event distributions; parameters are
more than level/reverb changes. Noise is spectrally shaped, mixed with machinery
and sparse events. Twelve percent of the original bed remains underneath the new
models to preserve existing layer controls and evolution character. Each existing
layer enable/level controls its corresponding contribution; disabling all six
silences ambience. Compressor transitions and profile changes are smoothed.

Music passes through a common location-specific colour/modulated-delay/echo stage
before the independent musical gain stage and architectural send. **Environment
Influence** (Room panel) bypasses that musical coloration at zero without muting
ambience; it does not bypass the separately configurable room. This common chain
is Milestone 2 functionality. Per-instrument independent effects, configurable
chains and per-instrument influence are still Milestone 3 work. The Impossible
Room uses variable-delay pitch movement, not a fixed-interval pitch shifter.

Model changes crossfade source/colour outputs over approximately 45 ms. Delay
memory is allocated in prepare, feedback is bounded at 0.23, and room tails are
retained on selection rather than resetting the whole engine. A newly selected
preset interpolates geometry using the existing room smoothing. An explicit
Reset Evolution still clears tails as previously documented.

Models have distinct palette metadata and the technical room view changes colour.
This is not the full environment artwork/animation redesign requested in Milestone
4. Recommended combinations appear in the descriptions. The separate ENVIRONMENT + RECOMMENDED INSTRUMENTS menu loads actual per-family synthesis settings and two calibrated musical layers. Ordinary location selection preserves the user's patch.

## Comparisons and acceptance evidence

`environment_tests <output-folder>` generates 20 WAVs: 30 seconds of each location
without MIDI, then the same C3/G3/D4 phrase through each location. All start with
seed 4242 and master -9 dB. Stereo 48 kHz PCM16 exports are matched toward -28 dBFS
RMS with a -1 dBFS peak cap; actual applied gain/achieved RMS is in the CSV. This
post-render matching is for fair listening comparisons and does not change live
bus levels. The reference renderer calls the same Engine/Instruments classes as
the plugin; it is not a separate approximation.

The CSV records raw peak/RMS, event count, low/mid/high band energy and zero
crossings. Tests establish source activation, differing rendered signals,
determinism, freeze, input muting, influence bypass, numerical stability, profile
switching bounds and preserved foreground balance for every family in every
location (60 comparisons). Metrics are supporting evidence only. Emotional
identity, absence of perceived repetition, realism and musical usefulness still
require the user's listening review. These must not be marked subjectively
validated from the metrics alone.

Schema 4 appends `environmentModel` (-1 legacy, 0–9 in the table order) and
`environmentInfluence` (0–1). Earlier states migrate missing values to legacy
mode with influence 1. Selecting a core location applies its room/event defaults;
automating only the model ID changes sources/colour processing while preserving
separately automated geometry. Seed/time initialization determines repeatability;
changing rooms mid-stream does not restart its seed or existing acoustic tails.
