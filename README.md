# LIMINAL

A cinematic environmental **instrument** (AU / VST3, macOS; GarageBand and Logic Pro):
playable, unsettling instruments placed inside liminal spaces.

**Status: Milestone 0 skeleton.** One placeholder test instrument, one room-tone
ambience in one corridor, and the first sketch of the "viewfinder" interface. The
real instruments, environments and design come in Milestones 1–7.

- Brief: [`LIMINAL_CLAUDE_BRIEF.md`](LIMINAL_CLAUDE_BRIEF.md), spec:
  [`LIMINAL_DIRECTION_UPDATE_1.md`](LIMINAL_DIRECTION_UPDATE_1.md)
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
