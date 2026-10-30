# LIMINAL — Direction Update 1

## Milestone checklist
- [ ] 1 — Six playable musical instrument families — **Partially completed** (engine checkpoint implemented; listening/host acceptance and card artwork outstanding)
- [ ] 2 — **Implemented but not fully tested** — ten procedural models, musical profiles, comparison renders; subjective/DAW review pending
- [ ] 3 — Independent cinematic effects per instrument
- [ ] 4 — Environment artwork and page-based visual redesign
- [ ] 5 — Thirty Characters and expanded scene transitions
- [ ] 6 — Preset library, export, quality modes and production workflow
- [ ] 7 — Listening refinement, screenshots and host validation

## Audit and implementation plan
The existing engine provides reusable seeded ambience, sparse event scheduling,
geometry-derived reflections, an eight-line reverb, output protection and MIDI
sample-offset dispatch. Its shared harmonic MIDI bank is inadequate for the new
six-family requirement. The old parameter IDs, presets and ambience renders must
remain compatible. The current generic editor is retained only as a temporary
technical editor until Milestone 4; it is not the intended final appearance.

Milestone 1 adds a separate fixed-capacity polyphonic instrument bank and injects
its stereo output before the existing room/output chain. Instruments must be
validated dry as well as through the processor. Parameter IDs are appended,
with migration for version-1 saved states. No commercial samples are required.
GarageBand/Logic cannot be launched in this Linux workspace; compilation or a
VST3 smoke test must not be described as verification in those DAWs.

## Milestone 1 — instrument-engine checkpoint

Implemented:
- Separate three-string dispersive/modal piano; paired additive-saw synth;
  inharmonic glass resonator; formant-weighted synthetic choir; struck-tine
  music box with mechanical excitation; sustained harmonic/sub-octave drone.
- Twelve main voices and twelve retirement voices per family, independent
  smoothed enable/level/six controls, velocity, pedal, channel-specific bend,
  sample-offset MIDI routing and explicit panic handling.
- Temporary INSTRUMENTS cards with enable/level/detail selection. The existing
  room/ambience panel remains accessible using ENVIRONMENT. Card artwork/icons
  and the substantial cinematic redesign have not been completed.
- Stereo bank output enters the existing room/output chain; continuous ambience
  remains operational. Version-1 preset/session migration adds default controls;
  version-2 roundtrips the extended parameter set. Existing parameter IDs remain.

Validation performed:
- Release Linux VST3/standalone compilation succeeds.
- All three CTest suites pass: 68 existing DSP checks; six dry-family tests
  covering chords, sustain/release, bend dispatch, voice bounds, panic, layered
  output and several sample rates; processor/state tests including legacy
  migration and the callback allocation guard.
- The built VST3 scans, loads, renders bounded stereo, restores state and opens /
  reopens the editor in the JUCE smoke host under Xvfb.
- Hosted instrument-page screenshot reviewed at 1160 x 820. Other sizes and
  complete pointer/keyboard interaction remain unverified.
- Six 12-second dry 48 kHz stereo WAVs and INDEX.txt generated under
  `build/DEMO_AUDIO/milestone-1/`; RIFF frame/channel/rate metadata verified.
  These are listening deliverables, not a claim of convincing subjective timbre.

Known limitations / next steps:
- Milestone 1 overall is partial: subjective instrument quality, GarageBand / Logic
  use and real host automation are not verified. Card icons/artwork are missing.
- Extremely dense repeated stealing can exhaust retirement slots; a retiring
  voice may then be truncated. Further click analysis/refinement is needed.
- Distance is currently attenuation per family; per-family acoustic coloration
  and independent effect chains have not been implemented.
- The choir/piano are simplified procedural models, not sampled acoustic instruments.
  Dry listening and spectral review must guide their refinement before claiming
  professional quality. Saturation is not oversampled.
- Scene slots persist new values, but old reduced morph behavior is retained;
  full scene/instrument/discrete switching is Milestone 5.
- Mac test release `v0.2.0-milestone1.2` is now published. Its matching
  `install.sh` installs the six-family checkpoint. GarageBand/Logic still
  require user acceptance testing.
- Milestones 2–7: **Not started**. Finish Milestone 1 listening/host acceptance,
  then implement ten genuinely distinct environments before visual polish.

Performance correction: the initial maximum-layer stress case (72 voices) took
11.08 s to render 2 s on this Linux machine. Moving modal, filter and envelope
coefficient calculations to a 32-sample control rate reduced that observation to
2.17 s for the same 2 s. Waveform phases and envelopes still advance per sample;
modal gains interpolate between targets. This remains slower than real time at
maximum layering here; quality/voice-limit work is still required. These are
cloud CPU observations, not Apple Silicon benchmarks.

A three-note piano benchmark rendered 2 s in 0.109 s on the same machine.

## Milestone 1 Mac test release

Published v0.2.0-milestone1.2 after native Mac compilation, all three test suites,
both-architecture verification, VST3 host rendering, signature verification and
Apple auval validation passed. The initial validation attempt could not discover
the newly copied AU; refreshing the CI user's AudioComponentRegistrar and waiting
for discovery resolved it. Download checksums and bundle version 0.2.0 are checked
before handoff. This is a partial Milestone 1 test release, not completion of the
seven-milestone overhaul. Future testable checkpoints must include a matching
published native installer before handing them off for user testing.

Release: https://github.com/AdrianAbbas/LIMINAL/releases/tag/v0.2.0-milestone1.2
Native validation: https://github.com/AdrianAbbas/LIMINAL/actions/runs/40272030404

## Instrument/ambience balance correction — 0.2.1

Audited the model/velocity/envelope/family/master chain. The musical bank was
conservatively scaled and its entire output was then attenuated by architectural
source distance/reverb. Presence was already applied only to the ambience input;
an isolated-instrument regression now proves it does not attenuate music.

Added independent always-visible Instrument Level and Ambience Level controls,
family reference calibration, family-layer power compensation, a foreground
musical direct path plus architectural send, quieter per-preset ambience defaults
and a -1 dBFS bounded protection ceiling. Master default remains -9 dB.
State schema 3 migrates version-1/2 sessions to the new bus defaults.

Four local suites pass: existing ambience, instruments, gain staging and state.
The gain suite compares six instruments against all six ambience layers using
a fixed phrase in default/Mall/Hospital/Industrial configurations (24 cases).
Final local musical RMS: -31.94 to -25.41 dBFS; foreground advantage: 21.33 to
31.39 dB; worst normal mixed peak: -8.18 dBFS. Maximum 72-voice layered chord:
-4.51 dBFS peak, -18.20 dBFS RMS. Independent fader calibration and Presence
isolation are checked. CSV: docs/validation/gain-staging-0.2.1.csv.

These verify rendered balance/headroom, not subjective timbre or universal MIDI
performance. GarageBand listening verification remains with the user. A native
Mac test installer is being generated before continuing Milestone 2.
See docs/GAIN_STAGING.md for the signal-path audit and exact regression criteria.

## Milestone 2 — environment audio overhaul

Implemented ten separate procedural source/event configurations: medical tones,
escalator drives, bending pipes, concrete impacts, gliding water drops, elevator
ramps, rail modes, random long compressor cycles, electronic activity and
inharmonic disturbances. Shaped airflow, machinery and sparse foreground events
replace the old bed as the primary identity. Each model has geometry/material,
common musical filtering/modulated delay/echo, a palette and actual optional
recommended layered instrument presets. Old sounds remain explicitly legacy.

Sources/musical profiles crossfade; location selection retains room tails.
Influence bypass and layer mutes work. Delay buffers are preallocated and bounded.
State schema 4 migrates older projects to legacy model -1. An active-location
allocation guard exercises hospital events and musical effects in processBlock.

Validation: all five local suites passed, followed by targeted environment/state
checks after adding recommended-preset/layering tests. Twenty 30-second WAVs
were rendered from live DSP at 48 kHz with seed 4242 and a matching RMS target;
headers/frame counts verified. Tests cover ten ambience and piano renders,
60 family/location foreground-balance comparisons, source events, deterministic
seed/freeze, all-layer muting, influence bypass, switching bounds, state
migration, recommended-preset output and maximum 72-voice layering.
Maximum-layer location peaks measured -3.07 to -4.61 dBFS, RMS -15.14 to -18.55
dBFS. The built VST3 loads/renders/restores state and opens/reopens its editor.

Evidence: build/DEMO_AUDIO/milestone-2/ (INDEX.md, WAVs, comparison CSV and
layering.csv). Native test release is being built from this checkpoint; the
separately published balance-fix installer already passed Mac tests/AU validation.

Status: **Implemented but not fully tested**. No claim of emotional distinction,
realism, absence of perceived repetition or GarageBand/Logic acceptance is made
without listening/host review. Approximate acoustics and variable-delay pitch
movement are documented. Per-instrument effects remain Milestone 3; photographic/
vector environment artwork and animation remain Milestone 4. Milestones 3–7
are not claimed complete by the new source models or technical palette changes.

## Published balance and environment test installers

- v0.2.1-balance.1: native Mac build, four suites, AU validation, package/archive
  integrity and both CPU architectures verified. Native render balance remains
  21.33–31.39 dB musical foreground advantage in the representative gain suite.
- v0.3.0-environments.1: native Mac build, all five suites and AU validation
  passed. Installer/ZIP SHA256, all three bundle versions (0.3.0), Intel/Apple
  Silicon slices and the 20 native thirty-second comparison WAVs were downloaded
  and verified. Native maximum layered peak across locations: -3.0737 dBFS.
- Native CSV evidence is recorded under docs/validation/ for gain, comparison
  spectra/events and layering. No subjective listening claim or GarageBand/Logic
  acceptance claim is inferred from those passing checks.

Install/release: https://github.com/AdrianAbbas/LIMINAL/releases/tag/v0.3.0-environments.1
Comparison audio: release asset LIMINAL-environment-comparisons.zip
Native validation: https://github.com/AdrianAbbas/LIMINAL/actions/runs/40278298083

Milestone 2 remains **Implemented but not fully tested** pending user listening
and DAW review. The loudness correction is verified by rendered stems/headroom
regressions; its perception in the user's GarageBand session remains feedback
to collect. Both updated builds are published, not source-only checkpoints.
