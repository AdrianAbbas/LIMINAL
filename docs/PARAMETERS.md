# Parameter reference

IDs are stable, version 1. Continuous parameters are automatable; enumerated and boolean parameters are discrete. Values shown are unnormalised.

## Macro mappings

- Presence: source excitation gain and perceived proximity through the direct/reverb field.
- Scale: reflection delay lengths, late-network lengths and RT60. The displayed dimensions remain in metres.
- Activity: structure/distant event density; combines with each layer activity and global Event Density.
- Unreal: quadratic drift depth, dissonant structure-mode ratios, atmosphere pitch deviations, spatial drift and irregular early-reflection modulation. It does not raise the master gain.

Mains uses 0 = 50 Hz, 1 = 60 Hz. Mode uses 0 = Environment, 1 = Instrument. Room materials are 0 Concrete, 1 Tile, 2 Metal, 3 Glass, 4 Carpet, 5 Wood, 6 Mixed. Booleans are 0 off / 1 on. Pan values use -1 left / +1 right.

## Complete registry

| Group | ID | Control | Range | Default | Type |
|---|---|---|---|---|---|
| Global | `active` | Active | 0 … 1 | 1 | Discrete |
| Global | `mode` | Mode | 0 … 1 | 0 | Discrete |
| Global | `follow` | Follow transport | 0 … 1 | 0 | Discrete |
| Macros | `presence` | Presence | 0 … 1 | 0.55 | Continuous |
| Macros | `scale` | Scale | 0 … 1 | 0.45 | Continuous |
| Macros | `activity` | Activity | 0 … 1 | 0.35 | Continuous |
| Macros | `unreal` | Unreal | 0 … 1 | 0.12 | Continuous |
| Performance | `attack` | Attack seconds | 0.01 … 8 | 0.4 | Continuous |
| Performance | `release` | Release seconds | 0.05 … 20 | 3 | Continuous |
| Performance | `velocity` | Velocity sensitivity | 0 … 1 | 0.7 | Continuous |
| Performance | `noiseGate` | Gate noise with notes | 0 … 1 | 1 | Discrete |
| Performance | `tempoSync` | Tempo sync pulse | 0 … 1 | 0 | Discrete |
| Performance | `beatDivision` | Pulse beats | 0.25 … 8 | 2 | Continuous |
| Electrical | `electricalOn` | Electrical enabled | 0 … 1 | 1 | Discrete |
| Electrical | `electricalLevel` | Electrical level | 0 … 1 | 0.45 | Continuous |
| Electrical | `electricalPan` | Electrical position | -1 … 1 | -0.18 | Continuous |
| Electrical | `mains` | Mains 50 or 60 Hz | 0 … 1 | 0 | Discrete |
| Electrical | `harmonics` | Harmonics | 0 … 1 | 0.4 | Continuous |
| Electrical | `buzz` | Buzz | 0 … 1 | 0.18 | Continuous |
| Electrical | `flicker` | Flicker | 0 … 1 | 0.3 | Continuous |
| Electrical | `instability` | Instability | 0 … 1 | 0.2 | Continuous |
| Ventilation | `ventilationOn` | Ventilation enabled | 0 … 1 | 1 | Discrete |
| Ventilation | `ventilationLevel` | Ventilation level | 0 … 1 | 0.45 | Continuous |
| Ventilation | `ventilationPan` | Ventilation position | -1 … 1 | 0.2 | Continuous |
| Ventilation | `airflow` | Airflow | 0 … 1 | 0.6 | Continuous |
| Ventilation | `motor` | Motor level | 0 … 1 | 0.35 | Continuous |
| Ventilation | `fanSpeed` | Fan speed Hz | 0.5 … 15 | 3 | Continuous |
| Ventilation | `rumble` | Rumble | 0 … 1 | 0.4 | Continuous |
| Ventilation | `ventResonance` | Vent resonance | 0 … 1 | 0.3 | Continuous |
| Ventilation | `ventMovement` | Vent movement | 0 … 1 | 0.3 | Continuous |
| Ventilation | `ventWidth` | Vent width | 0 … 1 | 0.6 | Continuous |
| Structure | `structureOn` | Structure enabled | 0 … 1 | 1 | Discrete |
| Structure | `structureLevel` | Structure level | 0 … 1 | 0.4 | Continuous |
| Structure | `structurePan` | Structure position | -1 … 1 | -0.35 | Continuous |
| Structure | `structureActivity` | Structure activity | 0 … 1 | 0.35 | Continuous |
| Structure | `structureMaterial` | Structure material | 0 … 1 | 0.5 | Continuous |
| Structure | `structureResonance` | Structure resonance | 0 … 1 | 0.6 | Continuous |
| Structure | `weight` | Weight | 0 … 1 | 0.5 | Continuous |
| Structure | `decay` | Decay seconds | 0.1 … 8 | 2 | Continuous |
| Structure | `randomness` | Randomness | 0 … 1 | 0.7 | Continuous |
| Structure | `structureDistance` | Structure distance | 0 … 1 | 0.25 | Continuous |
| Atmosphere | `atmosphereOn` | Atmosphere enabled | 0 … 1 | 1 | Discrete |
| Atmosphere | `atmosphereLevel` | Atmosphere level | 0 … 1 | 0.4 | Continuous |
| Atmosphere | `atmospherePan` | Atmosphere position | -1 … 1 | 0 | Continuous |
| Atmosphere | `density` | Density | 0 … 1 | 0.5 | Continuous |
| Atmosphere | `darkness` | Darkness | 0 … 1 | 0.65 | Continuous |
| Atmosphere | `movement` | Movement | 0 … 1 | 0.4 | Continuous |
| Atmosphere | `texture` | Texture | 0 … 1 | 0.3 | Continuous |
| Atmosphere | `subEnergy` | Sub energy | 0 … 1 | 0.2 | Continuous |
| Atmosphere | `spread` | Atmosphere spread | 0 … 1 | 0.8 | Continuous |
| Distant | `distantOn` | Distant enabled | 0 … 1 | 1 | Discrete |
| Distant | `distantLevel` | Distant level | 0 … 1 | 0.35 | Continuous |
| Distant | `distantPan` | Distant position | -1 … 1 | 0.4 | Continuous |
| Distant | `distantActivity` | Distant activity | 0 … 1 | 0.35 | Continuous |
| Distant | `distance` | Distant distance | 0 … 1 | 0.75 | Continuous |
| Distant | `eventRate` | Event rate | 0 … 1 | 0.3 | Continuous |
| Distant | `muffle` | Muffle | 0 … 1 | 0.7 | Continuous |
| Distant | `variation` | Variation | 0 … 1 | 0.6 | Continuous |
| Substructure | `subOn` | Substructure enabled | 0 … 1 | 1 | Discrete |
| Substructure | `subLevel` | Substructure level | 0 … 1 | 0.3 | Continuous |
| Substructure | `subPan` | Substructure position | -1 … 1 | 0 | Continuous |
| Substructure | `subFrequency` | Sub frequency Hz | 24 … 90 | 38 | Continuous |
| Substructure | `depth` | Depth | 0 … 1 | 0.5 | Continuous |
| Substructure | `pulse` | Pulse | 0 … 1 | 0.2 | Continuous |
| Substructure | `subMod` | Sub modulation | 0 … 1 | 0.3 | Continuous |
| Substructure | `subHarmonics` | Sub harmonics | 0 … 1 | 0.3 | Continuous |
| Room | `roomWidth` | Room width m | 3 … 100 | 22 | Continuous |
| Room | `roomLength` | Room length m | 3 … 150 | 40 | Continuous |
| Room | `roomHeight` | Ceiling height m | 2 … 30 | 5 | Continuous |
| Room | `sourceX` | Source X | 0 … 1 | 0.3 | Continuous |
| Room | `sourceY` | Source Y | 0 … 1 | 0.3 | Continuous |
| Room | `listenerX` | Listener X | 0 … 1 | 0.65 | Continuous |
| Room | `listenerY` | Listener Y | 0 … 1 | 0.7 | Continuous |
| Room | `sourceDistance` | Source distance m | 1 … 80 | 12 | Continuous |
| Room | `material` | Room material | 0 … 6 | 0 | Discrete |
| Room | `reverb` | Reverb mix | 0 … 1 | 0.5 | Continuous |
| Evolution | `evolutionRate` | Evolution rate | 0 … 1 | 0.25 | Continuous |
| Evolution | `evolutionDepth` | Evolution depth | 0 … 1 | 0.4 | Continuous |
| Evolution | `eventDensity` | Event density | 0 … 1 | 0.4 | Continuous |
| Evolution | `seed` | Random seed | 1 … 65535 | 1024 | Discrete |
| Evolution | `freeze` | Freeze evolution | 0 … 1 | 0 | Discrete |
| Output | `master` | Master dB | -60 … 6 | -9 | Continuous |
| Output | `stereoWidth` | Stereo width | 0 … 1.5 | 1 | Continuous |
| Output | `lowCut` | Low cut Hz | 15 … 250 | 25 | Continuous |
| Output | `highCut` | High cut Hz | 1000 … 20000 | 15000 | Continuous |
| Output | `mute` | Mute | 0 … 1 | 0 | Discrete |
| Scenes | `morph` | Scene morph | 0 … 1 | 0 | Continuous |

## Factory environments

All presets store the complete registry; values are generated in `src/presets/Factory.cpp`.

| Name | Intent | Dimensions (m) | Material | Unreal | Seed |
|---|---|---|---|---|---|
| Empty Shopping Mall | Wide tiled atrium; soft ventilation and receding electrical light. | 45 x 70 x 12 | 1 | 0 | 1024 |
| Abandoned School Hallway | Narrow hard corridor with intermittent pipe resonance. | 6 x 65 x 3 | 1 | 0.02 | 2021 |
| Underground Parking Garage | Heavy concrete, slow fans and deep structural pressure. | 55 x 85 x 3 | 0 | 0.04 | 3018 |
| Vacant Hospital | Quiet clinical air, remote impacts and bright empty reflections. | 12 x 40 x 3 | 1 | 0.06 | 4015 |
| Indoor Swimming Pool | Tall tiled chamber with a diffuse bright acoustic tail. | 28 x 45 x 10 | 1 | 0.08 | 5012 |
| Endless Office Corridor | Carpeted repeating space, subdued machinery and spectral drift. | 5 x 110 x 3 | 4 | 0.1 | 6009 |
| Fluorescent Classroom | Close electrical harmonics and irregular fluorescent flicker. | 10 x 14 x 3 | 6 | 0.12 | 7006 |
| Empty Supermarket | Broad retail floor; motors beneath a glassy reflection field. | 42 x 65 x 6 | 3 | 0.14 | 8003 |
| Concrete Stairwell | Vertical concrete void with exposed structural ringing. | 5 x 9 x 22 | 0 | 0.16 | 9000 |
| Derelict Industrial Plant | Dense metal resonances, heavy motors and distant machinery. | 38 x 60 x 16 | 2 | 0.18 | 9997 |
| Subway Maintenance Tunnel | Long tunnel rumble with muffled remote activity. | 6 x 140 x 5 | 0 | 0.2 | 10994 |
| Deserted Hotel Lobby | Warm wood surfaces and a gentle, quiet atmospheric bed. | 26 x 38 x 8 | 5 | 0.22 | 11991 |
| Empty Airport Terminal | Enormous glass volume with distant movement and long tails. | 85 x 140 x 24 | 3 | 0.24 | 12988 |
| Impossible Room | Unstable resonances and spatial drift in an impossible volume. | 65 x 95 x 28 | 2 | 0.75 | 13985 |
| The Backrooms | Oppressive fluorescent hum, carpet absorption and slow unreality. | 32 x 90 x 3 | 4 | 0.9 | 14982 |

## Direction Update 1 additions

State version 2 appends the six family enable/level pairs and 36 model controls.
See [INSTRUMENTS.md](INSTRUMENTS.md) for mappings. Prefixes are `piano`, `synth`,
`glass`, `choir`, `box`, `drone`; enable IDs end in `On`, level IDs in `Level`,
and the six controls use suffixes `0` through `5` in the documented table order.
Old IDs and factory ambience values remain unchanged.
