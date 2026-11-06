# TAPEWORN and TAPEWORN FX

A cinematic environmental **instrument** by **Elsewhere Audio** (AU / VST3, macOS; GarageBand and
Logic Pro): playable, unsettling instruments placed inside liminal spaces.

**TAPEWORN FX** (2026-11-06), by **Windward Audio**: the audio **effect** built from the same
repository - any sound (guitar, vocals, synths, drums, a whole mix) through TAPEWORN's ten places
(their rooms with the captured spaces, signatures, Characters, the cross-fade between places) and
its LIMINALITY stage adapted for audio (Detune, Drift, Wow & Flutter, Warble, Ghost, Wear, Timing,
Unevenness, Sour Notes, Fade), with Mix, Intensity, Input, Output and a click-free Bypass, the same
viewfinder, 26 presets, A/B and undo. AU `aufx Tpfx Wnwd`, VST3 effect; level-matched in every
room; 20 ms reported latency. See [`docs/TAPEWORN_FX.md`](docs/TAPEWORN_FX.md).

*TAPEWORN was called LIMINAL until 2026-11-06. Songs and presets made with LIMINAL open in TAPEWORN
unchanged (same Audio Unit codes, same parameters); the installer removes the old copy.*

**Status: Milestones 0–6 and 8 done (7 to come), plus the instrument library.** Twenty
instruments in six slots (each slot plays any of them, with its own Liminality, effects and room):
Forgotten Piano, Faded Synth, Glass Memories, Empty Choir, Lost Music Box, Hollow Drone, Old Guitar,
Faded Strings, Hallway Pizzicato, Toy Piano, Lounge E-Piano, Elevator Vibraphone, Mall Chimes, Solo
Voice, Distant Radio, Cassette Strings, Arcade Lead, Basement Bass, Warped Organ, Bowed Glass — most
playing the developer's own recordings (prepared in CI from a private repository; synthesised
stand-ins without them), level-matched in every place
([`docs/INSTRUMENT_NOTES.md`](docs/INSTRUMENT_NOTES.md)); Elevator Vibraphone and Lounge E-Piano can
blend in a soft, distant flute or clarinet (samples from the free CC0 library VSCO 2 Community
Edition by Versilian Studios, http://vis.versilstudios.net/vsco-community.html, fetched in CI from
its official source: [`docs/LICENSES/`](docs/LICENSES/README.md)) — in ten places that each reshape
every instrument and have their own room and sounds ([`docs/ENVIRONMENTS.md`](docs/ENVIRONMENTS.md)),
each with three Characters, evolution, four macros and Scene A/B morphing
([`docs/CHARACTERS.md`](docs/CHARACTERS.md)); per-instrument effects with the place's defaults
([`docs/EFFECTS.md`](docs/EFFECTS.md)); 116 factory presets plus INIT (no preset), undoable
preset loads, ambience export to WAV,
Eco/Standard/High quality. The interface is the "viewfinder": every place as its own scene,
graded per Character, lights that answer the music ([`docs/snapshots/`](docs/snapshots/)).

**Direction Update 2 (in progress):** the places are *still* now — steady hums and room tone,
rare distant events, the room's acoustics carrying the instrument. The hospital, the pool and
the office are photographic: scripted Blender/Cycles scenes rendered as separate light passes
and composited live, so the lights flicker with the hum and swell with the notes
([`tools/scenes/`](tools/scenes/README.md)); the other seven are still painted.

- Brief (written under the old name): [`LIMINAL_CLAUDE_BRIEF.md`](LIMINAL_CLAUDE_BRIEF.md), spec:
  [`LIMINAL_DIRECTION_UPDATE_1.md`](LIMINAL_DIRECTION_UPDATE_1.md),
  [`LIMINAL_DIRECTION_UPDATE_2.md`](LIMINAL_DIRECTION_UPDATE_2.md) (overrides the others)
- Progress, audit and decisions: [`docs/DEVLOG.md`](docs/DEVLOG.md)
- The earlier draft is kept, unbuilt, in [`legacy/`](legacy/)

## Install (macOS)

Download `TAPEWORN-macOS-Universal` from the latest successful **Native build**
Actions run, unzip it, and follow `INSTALL.txt` (run `bash install-mac.sh`): it installs both
plug-ins, TAPEWORN and TAPEWORN FX, side by side.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # add -DJUCE_PATH=... to use a local JUCE 8.0.16
cmake --build build --parallel 4
bash scripts/run-tests-parallel.sh build/LiminalTests_artefacts/Release/LiminalTests
build/TapewornFXTests_artefacts/Release/TapewornFXTests          # TAPEWORN FX's suite
```

Licence: AGPL-3.0 (see `LICENSE`).
