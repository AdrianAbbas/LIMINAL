#pragma once
// CHARACTERS (Milestone 5): three emotional identities per place. A Character keeps the
// location but changes how it behaves: which incidental sounds happen and how often, the
// bed, the room, the place's processing of the instruments, the default effects, how the
// place evolves, how strongly it answers the Unreal macro, and how it looks.
//
// Also here: each place's *flavour* of the Unreal macro and its response to the other
// three macros, and the Evolution layer (slow, seeded, freezable).
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include <array>
#include <cstdint>

namespace liminal::dsp {

inline constexpr int characterCount = 3;

struct Character {
    const char* name;
    const char* mood;          // one line for the interface
    const char* recommended;   // instruments that suit it
    // Ambience
    float eventRate = 1;                                   // how often events happen (x)
    std::array<float, 5> eventWeight { 1, 1, 1, 1, 1 };    // per schedule of the place (0 = never)
    int extraKind = -1;                                    // one more event kind (Ambience::Kind), -1 none
    float extraMean = 12;                                  // its mean interval, seconds
    float bed = 1, bedTone = 1, hum = 1;                   // bed level, bed brightness, hum/buzz level
    // Room
    float decay = 1, roomBright = 1, wetDb = 0;
    // The place's processing of the instruments
    float lowPass = 1, driftCents = 0, wowCents = 0, chorusMix = 0, shiftMix = 0, wanderPan = 0, tremolo = 0, dryDb = 0;
    // Default effects, added to the place's own (per instrument)
    float fxDrift = 0, fxTape = 0, fxChorus = 0, fxGrain = 0, fxEcho = 0, fxReverb = 0;
    // Evolution and the Unreal macro
    float evolutionSpeed = 1, evolutionDepth = 1, unreal = 1;
    // How it looks: a colour grade (ARGB), light level and fluorescent flicker (0..1)
    std::uint32_t grade = 0;
    float light = 1, flicker = 0;
};

const Character& character(int place, int index);

// What Unreal does in a place (at Unreal 100 %, before the Character's multiplier).
struct UnrealFlavour {
    float shiftMix = 0, wanderPan = 0, wowCents = 0;           // the place's processing
    float drift = 0, tape = 0, grain = 0, chorus = 0;          // the instruments' effects
    float echo = 0, shimmer = 0, echoShift = 0;                // the place's echo and room
};
const UnrealFlavour& unrealFlavour(int place);

// How strongly a place answers Presence, Scale and Activity (1 = the standard amount).
struct MacroResponse { float presence = 1, scale = 1, activity = 1; };
const MacroResponse& macroResponse(int place);

struct Macros { float presence = 0.5f, scale = 0.5f, activity = 0.5f, unreal = 0.0f; };

// The place, slowly alive: three smooth random movements (spectral, spatial, harmonic) and
// occasional disturbances. Deterministic for a seed; can be frozen and reset.
class Evolution {
public:
    void prepare(double sampleRate) { sr = sampleRate; }
    void start(unsigned seed);
    // Once per control period (`seconds` long). `rate` 0..1 (the parameter), `speed` and
    // `depth` from the Character, `activity` from the macro.
    void tick(float seconds, float rate, float speed, float depth, float activity, bool frozen);
    float spectral() const { return spectralNow; }   // -1..1 x depth
    float spatial() const { return spatialNow; }
    float harmonic() const { return harmonicNow; }
    bool takeDisturbance() { const bool d = disturbance; disturbance = false; return d; }
    float movement() const { return movementNow; }   // how much is changing right now (0..1)

private:
    double sr = 48000.0;
    Random rng;
    Wander a, b, c;
    float spectralNow = 0, spatialNow = 0, harmonicNow = 0, movementNow = 0;
    float nextDisturbance = 20;
    bool disturbance = false;
};

} // namespace liminal::dsp
