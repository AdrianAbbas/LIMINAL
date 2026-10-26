#pragma once
#include <array>
#include <cstddef>
namespace liminal
{
// Append new IDs; never reorder or reuse existing IDs in released sessions.
#define LIMINAL_PARAMETERS(X)                                                                                \
    X(active, "Active", 0, 1, 1, 1, "Global")                                                                \
    X(mode, "Mode", 0, 1, 0, 1, "Global")                                                                    \
    X(follow, "Follow transport", 0, 1, 0, 1, "Global")                                                      \
    X(presence, "Presence", 0, 1, .55f, 0, "Macros")                                                         \
    X(scale, "Scale", 0, 1, .45f, 0, "Macros")                                                               \
    X(activity, "Activity", 0, 1, .35f, 0, "Macros")                                                         \
    X(unreal, "Unreal", 0, 1, .12f, 0, "Macros")                                                             \
    X(attack, "Attack seconds", .01f, 8, .4f, 0, "Performance")                                              \
    X(release, "Release seconds", .05f, 20, 3, 0, "Performance")                                             \
    X(velocity, "Velocity sensitivity", 0, 1, .7f, 0, "Performance")                                         \
    X(noiseGate, "Gate noise with notes", 0, 1, 1, 1, "Performance")                                         \
    X(tempoSync, "Tempo sync pulse", 0, 1, 0, 1, "Performance")                                              \
    X(beatDivision, "Pulse beats", .25f, 8, 2, 0, "Performance")                                             \
    X(electricalOn, "Electrical enabled", 0, 1, 1, 1, "Electrical")                                          \
    X(electricalLevel, "Electrical level", 0, 1, .45f, 0, "Electrical")                                      \
    X(electricalPan, "Electrical position", -1, 1, -.18f, 0, "Electrical")                                   \
    X(mains, "Mains 50 or 60 Hz", 0, 1, 0, 1, "Electrical")                                                  \
    X(harmonics, "Harmonics", 0, 1, .4f, 0, "Electrical")                                                    \
    X(buzz, "Buzz", 0, 1, .18f, 0, "Electrical")                                                             \
    X(flicker, "Flicker", 0, 1, .3f, 0, "Electrical")                                                        \
    X(instability, "Instability", 0, 1, .2f, 0, "Electrical")                                                \
    X(ventilationOn, "Ventilation enabled", 0, 1, 1, 1, "Ventilation")                                       \
    X(ventilationLevel, "Ventilation level", 0, 1, .45f, 0, "Ventilation")                                   \
    X(ventilationPan, "Ventilation position", -1, 1, .2f, 0, "Ventilation")                                  \
    X(airflow, "Airflow", 0, 1, .6f, 0, "Ventilation")                                                       \
    X(motor, "Motor level", 0, 1, .35f, 0, "Ventilation")                                                    \
    X(fanSpeed, "Fan speed Hz", .5f, 15, 3, 0, "Ventilation")                                                \
    X(rumble, "Rumble", 0, 1, .4f, 0, "Ventilation")                                                         \
    X(ventResonance, "Vent resonance", 0, 1, .3f, 0, "Ventilation")                                          \
    X(ventMovement, "Vent movement", 0, 1, .3f, 0, "Ventilation")                                            \
    X(ventWidth, "Vent width", 0, 1, .6f, 0, "Ventilation")                                                  \
    X(structureOn, "Structure enabled", 0, 1, 1, 1, "Structure")                                             \
    X(structureLevel, "Structure level", 0, 1, .4f, 0, "Structure")                                          \
    X(structurePan, "Structure position", -1, 1, -.35f, 0, "Structure")                                      \
    X(structureActivity, "Structure activity", 0, 1, .35f, 0, "Structure")                                   \
    X(structureMaterial, "Structure material", 0, 1, .5f, 0, "Structure")                                    \
    X(structureResonance, "Structure resonance", 0, 1, .6f, 0, "Structure")                                  \
    X(weight, "Weight", 0, 1, .5f, 0, "Structure")                                                           \
    X(decay, "Decay seconds", .1f, 8, 2, 0, "Structure")                                                     \
    X(randomness, "Randomness", 0, 1, .7f, 0, "Structure")                                                   \
    X(structureDistance, "Structure distance", 0, 1, .25f, 0, "Structure")                                   \
    X(atmosphereOn, "Atmosphere enabled", 0, 1, 1, 1, "Atmosphere")                                          \
    X(atmosphereLevel, "Atmosphere level", 0, 1, .4f, 0, "Atmosphere")                                       \
    X(atmospherePan, "Atmosphere position", -1, 1, 0, 0, "Atmosphere")                                       \
    X(density, "Density", 0, 1, .5f, 0, "Atmosphere")                                                        \
    X(darkness, "Darkness", 0, 1, .65f, 0, "Atmosphere")                                                     \
    X(movement, "Movement", 0, 1, .4f, 0, "Atmosphere")                                                      \
    X(texture, "Texture", 0, 1, .3f, 0, "Atmosphere")                                                        \
    X(subEnergy, "Sub energy", 0, 1, .2f, 0, "Atmosphere")                                                   \
    X(spread, "Atmosphere spread", 0, 1, .8f, 0, "Atmosphere")                                               \
    X(distantOn, "Distant enabled", 0, 1, 1, 1, "Distant")                                                   \
    X(distantLevel, "Distant level", 0, 1, .35f, 0, "Distant")                                               \
    X(distantPan, "Distant position", -1, 1, .4f, 0, "Distant")                                              \
    X(distantActivity, "Distant activity", 0, 1, .35f, 0, "Distant")                                         \
    X(distance, "Distant distance", 0, 1, .75f, 0, "Distant")                                                \
    X(eventRate, "Event rate", 0, 1, .3f, 0, "Distant")                                                      \
    X(muffle, "Muffle", 0, 1, .7f, 0, "Distant")                                                             \
    X(variation, "Variation", 0, 1, .6f, 0, "Distant")                                                       \
    X(subOn, "Substructure enabled", 0, 1, 1, 1, "Substructure")                                             \
    X(subLevel, "Substructure level", 0, 1, .3f, 0, "Substructure")                                          \
    X(subPan, "Substructure position", -1, 1, 0, 0, "Substructure")                                          \
    X(subFrequency, "Sub frequency Hz", 24, 90, 38, 0, "Substructure")                                       \
    X(depth, "Depth", 0, 1, .5f, 0, "Substructure")                                                          \
    X(pulse, "Pulse", 0, 1, .2f, 0, "Substructure")                                                          \
    X(subMod, "Sub modulation", 0, 1, .3f, 0, "Substructure")                                                \
    X(subHarmonics, "Sub harmonics", 0, 1, .3f, 0, "Substructure")                                           \
    X(roomWidth, "Room width m", 3, 100, 22, 0, "Room")                                                      \
    X(roomLength, "Room length m", 3, 150, 40, 0, "Room")                                                    \
    X(roomHeight, "Ceiling height m", 2, 30, 5, 0, "Room")                                                   \
    X(sourceX, "Source X", 0, 1, .3f, 0, "Room")                                                             \
    X(sourceY, "Source Y", 0, 1, .3f, 0, "Room")                                                             \
    X(listenerX, "Listener X", 0, 1, .65f, 0, "Room")                                                        \
    X(listenerY, "Listener Y", 0, 1, .7f, 0, "Room")                                                         \
    X(sourceDistance, "Source distance m", 1, 80, 12, 0, "Room")                                             \
    X(material, "Room material", 0, 6, 0, 1, "Room")                                                         \
    X(reverb, "Reverb mix", 0, 1, .5f, 0, "Room")                                                            \
    X(evolutionRate, "Evolution rate", 0, 1, .25f, 0, "Evolution")                                           \
    X(evolutionDepth, "Evolution depth", 0, 1, .4f, 0, "Evolution")                                          \
    X(eventDensity, "Event density", 0, 1, .4f, 0, "Evolution")                                              \
    X(seed, "Random seed", 1, 65535, 1024, 1, "Evolution")                                                   \
    X(freeze, "Freeze evolution", 0, 1, 0, 1, "Evolution")                                                   \
    X(master, "Master dB", -60, 6, -9, 0, "Output")                                                          \
    X(stereoWidth, "Stereo width", 0, 1.5f, 1, 0, "Output")                                                  \
    X(lowCut, "Low cut Hz", 15, 250, 25, 0, "Output")                                                        \
    X(highCut, "High cut Hz", 1000, 20000, 15000, 0, "Output")                                               \
    X(mute, "Mute", 0, 1, 0, 1, "Output")                                                                    \
    X(morph, "Scene morph", 0, 1, 0, 0, "Scenes")                                                            \
    X(pianoOn, "Forgotten Piano enabled", 0, 1, 1, 1, "Forgotten Piano")                                     \
    X(pianoLevel, "Forgotten Piano level", 0, 1, .65f, 0, "Forgotten Piano")                                 \
    X(piano0, "Age", 0, 1, .35f, 0, "Forgotten Piano")                                                       \
    X(piano1, "Detune", 0, 1, .35f, 0, "Forgotten Piano")                                                    \
    X(piano2, "Softness", 0, 1, .35f, 0, "Forgotten Piano")                                                  \
    X(piano3, "Mechanical Noise", 0, 1, .35f, 0, "Forgotten Piano")                                          \
    X(piano4, "Decay", 0, 1, .35f, 0, "Forgotten Piano")                                                     \
    X(piano5, "Distance", 0, 1, .35f, 0, "Forgotten Piano")                                                  \
    X(synthOn, "Faded Synth enabled", 0, 1, 0, 1, "Faded Synth")                                             \
    X(synthLevel, "Faded Synth level", 0, 1, .65f, 0, "Faded Synth")                                         \
    X(synth0, "Detune", 0, 1, .35f, 0, "Faded Synth")                                                        \
    X(synth1, "Warmth", 0, 1, .35f, 0, "Faded Synth")                                                        \
    X(synth2, "Drift", 0, 1, .35f, 0, "Faded Synth")                                                         \
    X(synth3, "Attack", 0, 1, .35f, 0, "Faded Synth")                                                        \
    X(synth4, "Release", 0, 1, .35f, 0, "Faded Synth")                                                       \
    X(synth5, "Width", 0, 1, .35f, 0, "Faded Synth")                                                         \
    X(glassOn, "Glass Memories enabled", 0, 1, 0, 1, "Glass Memories")                                       \
    X(glassLevel, "Glass Memories level", 0, 1, .65f, 0, "Glass Memories")                                   \
    X(glass0, "Fragility", 0, 1, .35f, 0, "Glass Memories")                                                  \
    X(glass1, "Brightness", 0, 1, .35f, 0, "Glass Memories")                                                 \
    X(glass2, "Resonance", 0, 1, .35f, 0, "Glass Memories")                                                  \
    X(glass3, "Detune", 0, 1, .35f, 0, "Glass Memories")                                                     \
    X(glass4, "Decay", 0, 1, .35f, 0, "Glass Memories")                                                      \
    X(glass5, "Distance", 0, 1, .35f, 0, "Glass Memories")                                                   \
    X(choirOn, "Empty Choir enabled", 0, 1, 0, 1, "Empty Choir")                                             \
    X(choirLevel, "Empty Choir level", 0, 1, .65f, 0, "Empty Choir")                                         \
    X(choir0, "Breath", 0, 1, .35f, 0, "Empty Choir")                                                        \
    X(choir1, "Formant", 0, 1, .35f, 0, "Empty Choir")                                                       \
    X(choir2, "Detune", 0, 1, .35f, 0, "Empty Choir")                                                        \
    X(choir3, "Width", 0, 1, .35f, 0, "Empty Choir")                                                         \
    X(choir4, "Darkness", 0, 1, .35f, 0, "Empty Choir")                                                      \
    X(choir5, "Release", 0, 1, .35f, 0, "Empty Choir")                                                       \
    X(boxOn, "Lost Music Box enabled", 0, 1, 0, 1, "Lost Music Box")                                         \
    X(boxLevel, "Lost Music Box level", 0, 1, .65f, 0, "Lost Music Box")                                     \
    X(box0, "Wear", 0, 1, .35f, 0, "Lost Music Box")                                                         \
    X(box1, "Pitch Drift", 0, 1, .35f, 0, "Lost Music Box")                                                  \
    X(box2, "Mechanism", 0, 1, .35f, 0, "Lost Music Box")                                                    \
    X(box3, "Brightness", 0, 1, .35f, 0, "Lost Music Box")                                                   \
    X(box4, "Decay", 0, 1, .35f, 0, "Lost Music Box")                                                        \
    X(box5, "Distance", 0, 1, .35f, 0, "Lost Music Box")                                                     \
    X(droneOn, "Hollow Drone enabled", 0, 1, 0, 1, "Hollow Drone")                                           \
    X(droneLevel, "Hollow Drone level", 0, 1, .65f, 0, "Hollow Drone")                                       \
    X(drone0, "Density", 0, 1, .35f, 0, "Hollow Drone")                                                      \
    X(drone1, "Instability", 0, 1, .35f, 0, "Hollow Drone")                                                  \
    X(drone2, "Harmonics", 0, 1, .35f, 0, "Hollow Drone")                                                    \
    X(drone3, "Sub", 0, 1, .35f, 0, "Hollow Drone")                                                          \
    X(drone4, "Movement", 0, 1, .35f, 0, "Hollow Drone")                                                     \
    X(drone5, "Release", 0, 1, .35f, 0, "Hollow Drone")                                                      \
    X(instrumentBusLevel, "Instrument Level dB", -60, 6, 0, 0, "Output")                                     \
    X(ambienceBusLevel, "Ambience Level dB", -60, 6, -12, 0, "Output")

enum Param : std::size_t
{
#define ENUM(id, ...) id,
    LIMINAL_PARAMETERS(ENUM)
#undef ENUM
        count
};
struct Spec
{
    const char *id;
    const char *name;
    float min, max, initial;
    bool discrete;
    const char *group;
};
extern const std::array<Spec, count> specs;
using Settings = std::array<float, count>;
Settings defaults();
Settings sanitise(Settings);
} // namespace liminal
