# LIMINAL

A cinematic environmental **instrument** (AU / VST3, macOS; GarageBand and Logic Pro):
playable, unsettling instruments placed inside liminal spaces.

**Status: Milestones 0–6 done (7 and 8 to come).** Six playable instruments — Forgotten
Piano, Faded Synth, Glass Memories, Empty Choir, Lost Music Box, Hollow Drone
([`docs/INSTRUMENT_NOTES.md`](docs/INSTRUMENT_NOTES.md)) — in ten places that each reshape
every instrument and have their own room and sounds ([`docs/ENVIRONMENTS.md`](docs/ENVIRONMENTS.md)),
each with three Characters, evolution, four macros and Scene A/B morphing
([`docs/CHARACTERS.md`](docs/CHARACTERS.md)); per-instrument effects with the place's defaults
([`docs/EFFECTS.md`](docs/EFFECTS.md)); 86 factory presets, ambience export to WAV,
Eco/Standard/High quality. The interface is the "viewfinder": every place as its own scene,
graded per Character, lights that answer the music ([`docs/snapshots/`](docs/snapshots/)).

**Direction Update 2 (in progress):** the places are *still* now — steady hums and room tone,
rare distant events, the room's acoustics carrying the instrument. The hospital, the pool and
the office are photographic: scripted Blender/Cycles scenes rendered as separate light passes
and composited live, so the lights flicker with the hum and swell with the notes
([`tools/scenes/`](tools/scenes/README.md)); the other seven are still painted.

- Brief: [`LIMINAL_CLAUDE_BRIEF.md`](LIMINAL_CLAUDE_BRIEF.md), spec:
  [`LIMINAL_DIRECTION_UPDATE_1.md`](LIMINAL_DIRECTION_UPDATE_1.md),
  [`LIMINAL_DIRECTION_UPDATE_2.md`](LIMINAL_DIRECTION_UPDATE_2.md) (overrides the others)
- Progress, audit and decisions: [`docs/DEVLOG.md`](docs/DEVLOG.md)
- The earlier draft is kept, unbuilt, in [`legacy/`](legacy/)

## Install (macOS)

Download `LIMINAL-macOS-Universal` from the latest successful **Native build**
Actions run, unzip it, and follow `INSTALL.txt` (run `bash install-mac.sh`).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # add -DJUCE_PATH=... to use a local JUCE 8.0.16
cmake --build build --parallel 4
bash scripts/run-tests-parallel.sh build/LiminalTests_artefacts/Release/LiminalTests
```

Licence: AGPL-3.0 (see `LICENSE`).
