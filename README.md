# LIMINAL

A local, procedural cinematic environmental instrument built with C++17 and JUCE 8.0.6. Six synthesized layers feed approximate architectural early reflections and an eight-line feedback reverb. No samples, cloud APIs, or model downloads are used.

This is an initial implementation, not a production-certified release. See [validation](docs/VALIDATION.md) for evidence and limitations.

## Direction Update 1 — development branch

The seven-milestone overhaul is tracked in [DEVLOG](DEVLOG.md), following the [updated specification](docs/DIRECTION_UPDATE_1.md). The first source checkpoint adds six separate MIDI instrument families, layered enables/levels, sustain, pitch bend, temporary instrument cards and version-1 state migration. See [instrument controls and limitations](docs/INSTRUMENTS.md). The Milestone 1 test build is published by the tagged Mac release workflow. Its updated `install.sh` installs v0.2.0-milestone1.2 once that build passes.

Generate six dry listening examples with `build/instrument_demo build/DEMO_AUDIO/milestone-1`. These use the live synthesis code without room effects. An index records notes, velocity and settings. Listening review remains outstanding.

## Play

Load LIMINAL as an instrument in a VST3 host (or AU on macOS). Start with a factory preset, enable **ACTIVE**, and use Environment mode for continuous ambience. Enable **Follow host** to gate new input with playback; reverb tails continue. The development source provides twelve MIDI voices per enabled family, velocity, sustain pedal and pitch bend, alongside optional note-gated ambience. MPE is not implemented. The downloadable v0.1.0-preview.3 still uses the older eight-voice bank.

The four main macros are Presence, Scale, Activity and Unreal. Select a panel in the right dropdown for the complete parameter set. Drag **S** and **L** to position source and listener. All displayed controls connect to parameters. Save/Load writes portable `.liminal` presets (versioned JUCE XML binary, without external assets). Loading occurs on the UI thread. State and A/B slots persist in host sessions; random evolution restarts from its seed on restoration rather than resuming a sample-exact running tail.

**Capture A**, edit with **Live controls**, then **Capture B**. Morph interpolates continuous environmental controls from Electrical through Event Density, including room geometry and evolution amount. Enable switches, mains choice, room material, seed, freeze, performance/global settings and output controls remain live. Capture activates morph; Live controls exits it. Factory/preset changes disable morph. This is a reduced morph system, not two independent running engines or an arbitrary state crossfade.

## Download for Mac

Download [install.sh](https://github.com/AdrianAbbas/LIMINAL/raw/refs/heads/main/install.sh), type `bash ` in Terminal, drag the downloaded file into Terminal, and press Return. It downloads and verifies the prebuilt universal Mac release, then installs the AU, VST3 and standalone app in your user folders. No Xcode, build tools or administrator password is needed. It preserves existing installations as dated backups.

Use the macOS installer (`LIMINAL-macOS-universal.pkg`) from [Releases](https://github.com/AdrianAbbas/LIMINAL/releases). The release workflow builds it on GitHub's Mac runners and publishes it only after compilation and automated checks pass. A release asset is available only once that workflow succeeds. You do not need Xcode or build tools to install a release.

The installer includes the standalone app, AU for Logic/GarageBand, and VST3 for other compatible hosts, for Intel and Apple Silicon on macOS 11+. Restart your DAW after installing. Early previews are not Developer ID signed or notarized; macOS may require your explicit approval in Privacy & Security. See the included READ-ME.

Combined JUCE-based preview binaries are distributed under AGPLv3, with the full corresponding LIMINAL/JUCE source included in the release. Original LIMINAL code remains MIT licensed. [Distribution details](packaging/MAC-README.txt).

## Build

See [BUILDING](docs/BUILDING.md). CMake defaults to universal Intel (`x86_64`) and Apple Silicon (`arm64`) binaries on macOS 11+, including **VST3, AU and standalone**. Windows x64 and Linux VST3/standalone are configured too. macOS and Windows require native platform validation before release; a Linux build cannot establish compatibility with those hosts.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

JUCE is fetched at the immutable 8.0.6 commit, or provided using `-DJUCE_PATH=/path/to/JUCE`. Dependency-free DSP development works with `-DLIMINAL_BUILD_PLUGIN=OFF`.

[Parameter reference](docs/PARAMETERS.md) · [Architecture](docs/ARCHITECTURE.md) · [Validation](docs/VALIDATION.md)

## Licensing

Original LIMINAL code is MIT licensed. JUCE is separately licensed under AGPLv3 or JUCE's commercial terms. The MIT license does not remove obligations on a distributed JUCE application. Review JUCE's applicable terms and bundled third-party notices before distribution; commercial closed-source distribution generally needs an appropriate JUCE license. No signing, notarization or commercial certification is included.
