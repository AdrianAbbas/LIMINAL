# LIMINAL — Claude Code Master Brief

**Read this file first, then `LIMINAL_DIRECTION_UPDATE_1.md` (the full feature spec for Milestones 1–7). Where they conflict, this file wins.**

LIMINAL is a cinematic environmental **instrument** (AU/VST3, macOS, GarageBand and Logic Pro): playable, unsettling instruments placed inside liminal spaces — empty hospitals, malls, pools, hotels, subway stations, offices and impossible rooms. A composer should be able to pick a space, play a fragile piano melody, and immediately hear (and see) it become part of that architecture.

The current repository was started by another AI tool and is poor: the interface looks bad and barely works, the instruments sound terrible, and every environment looks and sounds the same. **Treat it as a rough draft, not a foundation that must be preserved.**

---

## 0. How to work (read first)

**The developer's setup.** Claude Code runs on a Windows PC or in the cloud. The developer's MacBook Air (fanless, Apple Silicon) is only used to test finished builds in GarageBand. All macOS building, signing and testing happens on GitHub Actions. The developer has a limited usage budget, so be efficient.

**Reuse the proven WORLDBUILDER pipeline.** The repository `AdrianAbbas/Worldbuilder` (same developer) contains a working, GarageBand-verified setup. Copy and adapt rather than reinvent:
- `CMakeLists.txt` structure (pinned JUCE 8.x, `AU_SANDBOX_SAFE TRUE`, universal arm64 + x86_64, low deployment target, instrument type);
- `.github/workflows/` (Linux full test suite in parallel shards; macOS universal build, short smoke test, auval and the AudioToolbox host harness on arm64 and x86_64, packaging; ccache; report-only CPU metrics);
- `scripts/install-mac.sh`, `uninstall-mac.sh`, `INSTALL.txt`;
- `tools/au-harness/` (in-process and out-of-process AU host test);
- the sampler engine and sample-preparation pipeline from its Milestone 15 (for later sample use; see Section 3);
- `docs/DEVLOG.md` lessons (GarageBand sandboxing, editor performance on a MacBook Air, determinism, real-time safety).
Read WORLDBUILDER's DEVLOG before starting so solved problems aren't solved again. Use a new AU identity for LIMINAL (e.g. subtype `Lmnl`, manufacturer `Veil`, its own bundle ID).

**Do NOT copy WORLDBUILDER's interface, layout or visual style.** LIMINAL must look like a completely different product (Section 2).

**Working rules (every session):**
- Work milestone by milestone. Split big milestones into sub-milestones (a/b/c) across sessions.
- Push to main after each finished part with `[skip ci]` in the commit message; the **final push of a session has no `[skip ci]`** so CI builds once.
- Keep a progress checklist at the top of `docs/DEVLOG.md`; resume from it after a compaction or usage limit.
- Small test-metric misses after 2 attempts → report-only, note them, move on. Never leave the build broken.
- Don't wait for or poll CI. Don't ask questions when running unattended; record decisions in the DEVLOG.
- When CI fails: read only the last ~100 lines of the failing step; max 2 fix attempts, then stop and explain in plain words.
- Never claim something works because a control exists or auval passed. Say exactly what was tested.
- Keep editor drawing cheap and never able to block the message thread (cached layers, ≤30 fps, skip late frames, auto-reduce animation if frames are slow) — the MacBook Air must never freeze.

---

## 1. Milestone 0 — Audit and rebuild plan (first session)

1. Audit the existing repository: what builds, what works in a host, what is salvageable (DSP pieces, parameter layout, presets, the floor-plan view, Scene A/B, macros).
2. Decide per area: **keep, rework, or replace.** Expect most of the UI and instrument DSP to be replaced. Write the audit and decisions in `docs/DEVLOG.md`.
3. Set up the WORLDBUILDER-derived build/CI/install pipeline and a minimal working skeleton: loads as an instrument in GarageBand, plays a test instrument from MIDI, ambience runs, custom editor appears.
4. Push without `[skip ci]` and **stop** so the developer can confirm it loads in GarageBand.

---

## 2. Visual direction — distinct from WORLDBUILDER

WORLDBUILDER is a pixel-art fantasy/sci-fi dashboard full of panels. LIMINAL must feel like the opposite: **quiet, photographic, cinematic, uncanny.**

**Concept: "the viewfinder."** The environment *is* the interface.
- The **whole window is the environment scene** — a full-bleed, atmospheric image of the space (hospital corridor, empty atrium, tiled pool…), with lighting, haze, fluorescent flicker and subtle parallax.
- Controls float **minimally** over it on thin translucent strips/frosted glass, like an old camcorder or film-camera viewfinder overlay: a small date/time stamp, a REC dot, battery-style meters, frame lines. Hide detail panels until needed (slide-in drawers for Instruments, Environment, Effects, Settings).
- **Analog photography treatment:** film grain, slight vignetting, colour grading per environment and Character (cold green hospital, sodium-orange garage, sun-faded pool blue, warm faded hotel), soft halation around lights, occasional VHS-style tracking shimmer when "Unreal" is high.
- **Typography:** clean, quiet, slightly institutional (signage-like) — readable, never decorative clutter. Use OFL/CC0 fonts with licences verified, or draw an original one.
- **Instrument selection** as physical objects or photographs (an old upright piano, a cassette synth, a glass, a music box) rather than generic cards.
- Lots of negative space is fine *as long as it's the scene*, not empty black.

**Environment art (important).** There are no copyrighted photos available and no runtime AI images.
- Build each scene as **layered procedural 2.5D art**: perspective geometry (corridors, pillars, tiles, ceiling light grids, aisles, cubicles), lighting and fog, then the photographic treatment above. Aim for "dreamlike photograph," not "vector illustration."
- Add an **asset pipeline**: if `assets/scenes/<environment>/` contains the developer's own photographs (PNG/JPG layers: `background`, `midground`, `lights`, `haze`), use them instead of the procedural scene, with the same grading, grain and animation. (The developer can photograph real empty spaces; their own photos are safe to ship.)
- Every environment and Character must look clearly different at a glance. Snapshot every environment into `docs/snapshots/` and check by eye.

---

## 3. Sound quality — lessons from WORLDBUILDER

- **Instruments:** build each with the right technique (Forgotten Piano: multi-string/modal hammer model or samples; Glass Memories and Music Box: modal synthesis with measured mode ratios; Empty Choir: formant synthesis with vowel morphing and breath; Faded Synth and Hollow Drone: rich multi-oscillator analog-style synthesis with drift). Research the acoustics first and record sources in `docs/INSTRUMENT_NOTES.md`.
- **No forced pitch scoops or glides.** Detune and drift are intentional controls, but any slide into pitch must be an option (default off).
- **Levels:** ambience is a bed, not the focus. At default, instruments sit clearly above the ambience (~10–15 dB). Provide an Ambience master level.
- **Samples (later milestone):** the developer has a private repository `AdrianAbbas/Samples` with their own recordings (piano, guitar, kalimba, violin, banjo, voice, more coming). Plan for a sampled **Forgotten Piano** from the developer's piano recordings, wine-glass taps for Glass Memories, kalimba/music-box material, and voice for Empty Choir. Reuse WORLDBUILDER's sampler engine and fetch-with-secret CI setup; never commit samples to a public repo.

---

## 4. Milestone order

0. Audit + pipeline + skeleton (stop for GarageBand check).
1. Instrument engine — the six instruments (split: 1a Piano, Synth, Glass; 1b Choir, Music Box, Drone).
2. Environment overhaul — ten distinct environments (split: 2a five, 2b five).
3. Per-instrument effects with environment influence.
4. Visual redesign per Section 2 (split: 4a viewfinder shell + 3 environments as a **prototype, then stop for approval**; 4b the rest).
5. Characters, evolution, Scene A/B, macros.
6. Presets, MIDI performance, WAV export, quality modes, state.
7. Final sound/visual polish and validation.
8. Samples: replace or layer instruments with the developer's recordings.

Detailed requirements for 1–7 are in `LIMINAL_DIRECTION_UPDATE_1.md`.
