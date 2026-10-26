# LIMINAL

A local, procedural cinematic environmental instrument built with C++17 and JUCE 8.0.6. Six synthesized layers feed approximate architectural early reflections and an eight-line feedback reverb. No samples, cloud APIs, or model downloads are used.

This is an initial implementation, not a production-certified release. See [validation](docs/VALIDATION.md) for evidence and limitations.

## Play

Load LIMINAL as an instrument in a VST3 host (or AU on macOS). Start with a factory preset, enable **ACTIVE**, and use Environment mode for continuous ambience. Enable **Follow host** to gate new input with playback; reverb tails continue. Instrument mode provides eight MIDI voices, attack/release, velocity sensitivity and optionally gated noise. MIDI channels share the voice pool; sustain pedal and MPE are not implemented.

The four main macros are Presence, Scale, Activity and Unreal. Select a panel in the right dropdown for the complete parameter set. Drag **S** and **L** to position source and listener. All displayed controls connect to parameters. Save/Load writes portable `.liminal` presets (versioned JUCE XML binary, without external assets). Loading occurs on the UI thread. State and A/B slots persist in host sessions; random evolution restarts from its seed on restoration rather than resuming a sample-exact running tail.

**Capture A**, edit with **Live controls**, then **Capture B**. Morph interpolates continuous environmental controls from Electrical through Event Density, including room geometry and evolution amount. Enable switches, mains choice, room material, seed, freeze, performance/global settings and output controls remain live. Capture activates morph; Live controls exits it. Factory/preset changes disable morph. This is a reduced morph system, not two independent running engines or an arbitrary state crossfade.

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
