# LIMINAL — Development log

Read `LIMINAL_CLAUDE_BRIEF.md` first (it wins over everything else), then
`LIMINAL_DIRECTION_UPDATE_1.md`. Resume from the checklist below.

## Progress checklist

- [x] **0 — Audit, WORLDBUILDER-derived pipeline, skeleton** (2026-10-30) — confirmed by the developer in GarageBand: loads, plays, layout looks good
- [ ] **1a — Forgotten Piano, Faded Synth, Glass Memories** (in progress, 2026-10-30)
  - [x] Research and `docs/INSTRUMENT_NOTES.md` (piano section)
  - [x] Shared instrument layer (`DSP/Instrument.h`), Distance stage, processor routing, Forgotten Piano + tests
  - [ ] Faded Synth + tests
  - [ ] Glass Memories + tests
  - [ ] Instruments drawer in the editor, DEVLOG handoff, final push without `[skip ci]`
- [ ] 1b — Empty Choir, Lost Music Box, Hollow Drone
- [ ] 2a — Five environments
- [ ] 2b — Five environments
- [ ] 3 — Per-instrument effects with environment influence
- [ ] 4a — Viewfinder shell + 3 environments (prototype, then stop for approval)
- [ ] 4b — Remaining environments' art
- [ ] 5 — Characters, evolution, Scene A/B, macros
- [ ] 6 — Presets, MIDI performance, WAV export, quality modes, state
- [ ] 7 — Final sound/visual polish and validation
- [ ] 8 — Samples from `AdrianAbbas/Samples`

### Requests from the developer for later milestones

- **Milestone 4 — more realism in the scenes** (asked 2026-10-30, after the M0 check):
  interactive lights — fluorescent tubes that flicker, buzz visually, and brighten or
  dim in response to the notes being played and to the ambience events; light spilling
  onto walls and floors; reflections on tiles, water and glossy floors; maybe lights
  reacting to the mouse. More depth and atmosphere overall. (Within the editor's frame
  budget: the MacBook Air must never freeze. The audio thread already publishes meter
  levels; note-on/ambience-event counters for the lights can be published the same way.)

---

## Milestone 0 — audit, pipeline, skeleton (2026-10-30)

### Working conditions

| | |
|---|---|
| Machine | Linux cloud container (Ubuntu 24.04, x86_64). No macOS, Xcode, GarageBand or Logic. |
| Verified here | Linux build (VST3, Standalone, tests) with JUCE 8.0.16: **64 checks pass, 0 fail** (`scripts/run-tests-parallel.sh`). Editor rendered to an image at 2x and inspected by eye. A 17 s phrase rendered to WAV and measured (peak -8.5 dBFS, RMS -30 dBFS); **not listened to** (no audio output here). |
| Not verified here | Everything macOS: the AU build, `auval`, the adapted AU host harness (`tools/au-harness/LiminalAUHarness.mm`, edited but **never compiled** here), the install script with the new identity, GarageBand. The first macOS CI run is the first test of all of these; the developer's GarageBand check is the real one. |

### 1. Audit of the existing repository (now in `legacy/`)

What was there: a JUCE 8.0.6 / C++17 plug-in (~106 KB of source in `src/`), seven
test executables, three workflows (Linux/Windows/macOS build, a tagged `.pkg`
release, an `install.sh` publisher) and long docs.

**What builds.** With JUCE 8.0.16 (the WORLDBUILDER pin) the plug-in and its
test suites compile, and all five CTest suites pass (dsp, instruments,
gain_staging, environments, state). Only `host_smoke` fails to compile
(`createEditor()` is private in 8.0.16). The old CI also built it.

**What works in a host.** Unknown. The old DEVLOG itself says GarageBand/Logic
were never verified; only a JUCE VST3 smoke host under Xvfb was run. There was
no auval, no AU host harness, no quarantine handling, `AU_SANDBOX_SAFE` was not
set, and the release was an unsigned `.pkg` installing system-wide into
`/Library/Audio/Plug-Ins` — exactly the setup WORLDBUILDER/VEIL found to fail
silently in GarageBand.

**Area by area — decision:**

| Area | What it is | Decision |
|---|---|---|
| Build / CI / install | JUCE 8.0.6, deployment 11.0, `.pkg` to `/Library`, no auval/harness | **Replace** with the WORLDBUILDER pipeline (done) |
| AU identity | `Lmnl`/`Lm01`, "Liminal Audio" | **Replace**: `aumu` / `Lmnl` / `Veil`, "Veil Audio", `audio.veil.liminal` (brief). Old GarageBand projects using the old build will not find it; acceptable (never released as working). |
| Parameter layout | ~150 flat X-macro parameters, many generic ("Electrical buzz", "Vent movement") | **Replace** with a structured layout grown milestone by milestone. Keep the rule: IDs append-only, never renamed. No migration of old sessions (new identity). |
| Instruments (`Instruments.cpp`, 316 lines for six families) | A shared voice struct; each "family" is a few `sin()` partials with different envelopes. Not modal/formant/analog models; the brief says they sound terrible. | **Replace** (Milestone 1a/1b) with one proper model per instrument. Keep two ideas: retirement slots for stolen voices, explicit panic. |
| Ambience engine (`Engine.cpp`) | Six seeded layers (electrical, ventilation, structure, atmosphere, distant, sub), event scheduler, geometry-derived early reflections, 8-line FDN, output protection | **Rework** in Milestone 2: the event scheduler, seeded RNG and geometry→early-reflection mapping are worth porting; the layers themselves are the generic "electrical noise and ventilation" the direction update rejects. |
| Environments (`Locations.cpp`, `Profiles.cpp`) | Ten thin profiles (213 + 89 lines) mostly re-weighting the same layers | **Replace** (Milestone 2); the names and the brief's ten environments match. |
| Presets (`Factory.cpp`) | 15 room presets (dimensions + descriptions) | **Keep as reference data**: the room dimensions/descriptions are a useful starting table for Milestone 2/6. |
| Scene A/B, macros | Snapshot arrays + linear morph of continuous params; macros Presence/Scale/Activity/Unreal | **Rework** in Milestone 5 (the concept is right; the implementation morphs raw arrays without smoothing). |
| Floor-plan view | Not present (only Source/Listener X/Y parameters) | Nothing to salvage; Milestone 4 decides if it returns. |
| Editor (`PluginEditor.cpp`, 436 lines) | Generic sliders on panels, cards with text | **Replace** (done for M0 by the viewfinder skeleton; Milestone 4 is the real design). |
| Tests | Useful checks (gain staging, allocation guard, migration) | **Replace** with one JUCE console test app with `--only` groups (WORLDBUILDER style); port individual checks when their features return. |
| Docs/validation CSVs | Measurements of the old engine | Kept in `legacy/docs` for reference only. |

`legacy/` is **not built**. It stays until Milestone 2 has taken what it needs,
then it can be deleted (git history keeps it anyway).

### 2. Pipeline (copied from WORLDBUILDER and adapted)

- `CMakeLists.txt`: JUCE 8.0.16 pinned (`ea77444`), C++20, universal
  arm64+x86_64, macOS 10.15, `IS_SYNTH`, `kAudioUnitType_MusicDevice`,
  `AU_SANDBOX_SAFE TRUE`, AU/VST3/Standalone, test app linked against the plug-in
  target.
- `.github/workflows/build.yml`: macOS job (JUCE cache, ccache, universal build,
  smoke tests on arm64 and on x86_64 under Rosetta, bundle/Info.plist gate,
  ad-hoc signing + packaging, AU harness build, quarantined-download install with
  `install-mac.sh --ci`, auval + harness in-process and out-of-process on both
  architectures, annotations, upload of `LIMINAL-macOS-Universal.zip` only if
  every check passed). Linux job: full suite in parallel shards, editor snapshot,
  a listening render, logs uploaded. Tags `v*` publish a GitHub Release. Commits
  with `[skip ci]` do not run.
- `scripts/`: `install-mac.sh`, `uninstall-mac.sh`, `package-mac.sh`,
  `ci-check-bundles.sh` (checks `aumu`/`Lmnl`/`Veil`, `LIMINALAUFactory`,
  `sandboxSafe`), `ci-user-install-test.sh`, `build-au-harness.sh`,
  `ci-annotate.sh`, `ci-crash-summary.py`, `ci-fetch-juce.sh`,
  `ci-developer-id.sh` (optional, untested), `diagnose-mac.sh`, `register-au.sh`,
  `run-tests-parallel.sh` (new shards). Samples handling removed until Milestone 8.
- `tools/au-harness/LiminalAUHarness.mm`: WORLDBUILDER's harness renamed. Its
  WORLDBUILDER-only checks (KEYS state injection, bundled samples) were replaced
  by one LIMINAL check: with Ambience Level at minimum, a MIDI note must be
  audible (> -40 dBFS) and 20 dB over the silence. It still checks: default
  output audible without MIDI, Master Level minimum, Power off/on, MIDI
  accepted, factory presets, state restore in a fresh instance, sample rates,
  mono, and the editor view (out-of-process).
- `packaging/INSTALL.txt`: rewritten for LIMINAL.
- WORLDBUILDER lessons applied: quarantine removal + ad-hoc re-sign in the
  installer; in-process and out-of-process harness routes; `--deep` signing
  before packaging; install per user (not `/Library`); editor frame governor
  (`Source/UI/FrameGovernor.h`, from the GarageBand freeze fix); levels so the
  ambience is a bed, not the focus.

### 3. Skeleton

| | |
|---|---|
| Parameters | Power, Master Level, Instrument Level, Ambience Level (all -60..+6 dB, minimum = silence), Space. Stable IDs. |
| Test instrument (`DSP/TestPiano`) | 16-note struck-string placeholder: 8 stretched partials x 2 detuned strings, two-stage decay, felt-hammer noise, dampers (none above F6), sustain pedal, pitch bend ±2 (immediate, no glide), click-free stealing (5 ms fade, 6 spare slots), All Notes/Sound Off. **Not** the Forgotten Piano. |
| Ambience (`DSP/RoomTone`) | Breathing pink-noise air, 60 Hz mains hum, a quiet stuttering fluorescent buzz. About -41 dBFS. |
| Room (`DSP/Space`) | 8-line FDN, RT60 ≈ 2.4 s, damped, 18 ms pre-delay. Instrument fully, ambience at 40 %. |
| Output | Smoothed gains, soft safety limiter above -3 dBFS, exact silence when Power is off, stereo or mono. |
| Editor | 920 x 580. The whole window is a procedural vacant-hospital corridor (perspective geometry, ceiling panels with halation, floor reflections, fog, cold grade, vignette, grain), rendered once per size/scale into an image. Viewfinder overlay: REC dot, time stamp, battery-style INST/AMB meters, frame corners; one failing ceiling light flickers. Frosted strip with Power and four sliders (double-click resets). Each tick repaints only the regions that changed; the frame governor spaces frames out and stops the flicker if frames stay slow. Snapshot (1x): `docs/snapshots/m0-editor.png`; CI uploads the 2x one with the Linux test logs. |

**Measured (Linux, this container):** ambience bed -40.6 dBFS, quietest second
-41.5 dBFS (the AU harness gates at -45); a mezzo-forte melody 12.6 dB over it
(brief: 10-15 dB); C4 at velocity 100 -25.5 dBFS RMS;
velocity 30→120 spans 17.8 dB; A5 measures 882 Hz (string stretch, +4 cents);
full bend 990.7 Hz (+2 semitones); 300-note storm: no clicks, no stuck notes;
`processBlock` allocates nothing; CPU 2.3 % of one core for 16 notes + ambience
+ room (report-only); editor open + first 2x paint about 100 ms (software
renderer).

### Decisions (recorded instead of asking)

- Old code moved to `legacy/` rather than deleted, so Milestone 2 can port the
  event scheduler / reflection mapping.
- Source layout follows WORLDBUILDER (`Source/`, `Tests/`, `scripts/`, `tools/`).
- Fonts: the system sans-serif for now (no font files to license). Milestone 4
  picks an OFL signage-like font and records its licence.
- Fixed editor size for now (simplest for GarageBand); resizing comes with
  Milestone 4.
- Grain is baked into the scene image (static); animated grain/VHS shimmer are
  Milestone 4 and must stay within the frame budget.
- Version 0.4.0 (the old code reached 0.3.0).

### Known limitations

- The test instrument is a placeholder; it will sound like a simple electric
  piano / music-box hybrid, not a convincing piano.
- One environment only; Space is one fixed room.
- Nothing here has run on macOS yet (see Working conditions).

### For the developer's GarageBand check

Download **LIMINAL-macOS-Universal** from the Actions run of the final Milestone 0
commit (only produced when every macOS check passed), unzip, run
`bash install-mac.sh` from the unzipped folder, then in GarageBand: new Software
Instrument track > Plug-ins > Instrument slot > AU Instruments > Veil Audio >
LIMINAL. Expected: the corridor window appears; a quiet room tone plays at once;
notes from a keyboard or Musical Typing play the test instrument with a room
tail; the sliders and Power work. If an older LIMINAL `.pkg` was ever installed,
remove `/Library/Audio/Plug-Ins/Components/LIMINAL.component` first (the
installer warns about it).
