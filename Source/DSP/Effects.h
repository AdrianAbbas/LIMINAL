#pragma once
// PER-INSTRUMENT EFFECTS (Milestone 3). Every instrument runs through its own rack:
//
//   instrument -> Distance -> [ Drift, Tape, Filter, Chorus, Granular, Echo (in the chosen
//                               order) -> the place's processing (Colour) ] -> mix with dry
//                          -> sends: the place's room (Reverb) and the place's echo
//
// Two layers decide how much each effect does:
//   - the player's own settings (amounts, details, toggles), and
//   - the place's defaults for this instrument (`placeEffects`), scaled by the instrument's
//     Environment Influence (and the global Environment Amount).
// The place's processing (Colour: filtering, resonances, pitch behaviour, width, nearness)
// is scaled the same way. At Influence 0 with the player's amounts at 0 the instrument is
// exactly as it was (only its Reverb send to the room, as before). A toggle bypasses its
// module completely, both layers; Effects Mix 0 is the dry instrument.
//
// All buffers are allocated in prepare(); process() never allocates. Modulation runs on an
// internal 32-sample clock, so output does not depend on the host's block size.
#include "DSP/Environment.h"
#include <array>
#include <vector>

namespace liminal::dsp {

// The player's settings for one instrument's rack (read from the parameters each block).
struct EffectSettings {
    float mix = 1, influence = 1;
    int order = 0;   // 0 standard, 1 texture first, 2 echo into texture
    bool driftOn = true;   float drift = 0, driftRate = 0.3f;
    bool tapeOn = true;    float tape = 0, tapeTone = 0.5f, tapeHiss = 0.3f, tapeDropouts = 0;
    bool reverbOn = true;  float reverb = 0.5f;
    bool echoOn = true;    float echo = 0, echoMs = 375, echoFeedback = 0.35f, echoTone = 0.5f; int echoSync = 0;
    bool chorusOn = true;  float chorus = 0, chorusRate = 0.3f;
    bool filterOn = true;  float filter = 1, filterLow = 20000, filterHigh = 20, filterRes = 0.1f;
    bool grainOn = true;   float grain = 0, grainSize = 0.4f, grainSpread = 0.5f; bool grainFreeze = false;
};

// What a place does to one instrument by default (at Influence 100 %).
struct PlaceEffects {
    float drift = 0, tape = 0, chorus = 0, grain = 0, grainSize = 0.4f;
    float echo = 0, echoMs = 400, echoFeedback = 0.3f;
    float lowOctaves = 0, highOctaves = 0;   // the filter: low-pass down / high-pass up, in octaves
    float reverbBias = 0;                    // -1..+1: less or more of this instrument in the room
};
PlaceEffects placeEffects(Place, int instrument, int character = 0);

// Live additions from the macros and evolution (Milestone 5), applied like the place's
// layer (scaled by Influence and Environment Amount).
struct EffectLive {
    float drift = 0, tape = 0, grain = 0, chorus = 0;   // Unreal's share of the effects
    Colour::Live colour;                                  // Unreal and evolution on the place's processing
};

class EffectRack {
public:
    void prepare(double sampleRate, int instrumentIndex);
    void reset();   // also jumps every smoothed setting to its target
    // Once per block. `environmentAmount` is the global Environment Amount; `bpm` the host's.
    void setSettings(const EffectSettings&, float environmentAmount, double bpm, const EffectLive& = {});
    void setPlace(Place, int character = 0);   // cross-fades the place's processing over 60 ms on a change
    // In place. Adds the room send (scaled by `sendFactor`, from Distance) and the place's
    // echo send.
    void process(float* left, float* right, float* roomL, float* roomR, float* echoL, float* echoR, float sendFactor, int n);
    // What the effects are doing now (for tests and the interface).
    struct Effective { float drift, tape, chorus, grain, echo, echoMs, echoFeedback, lowPass, highPass, reverb, colour; };
    Effective effective() const { return now; }

private:
    enum Module { drift, tape, filter, chorus, grain, echo };
    void control();   // one control tick: smoothing, coefficients, modulators
    void moduleTick(Module, float& l, float& r);
    float readLine(const std::vector<float>& line, int write, float delay) const;

    double sr = 48000.0;
    int instrument = 0;
    EffectSettings user;
    float envAmount = 1;
    double bpm = 120.0;
    Place place = Place::hospital;
    int characterNow = 0;
    EffectLive liveNow;
    bool placeKnown = false;
    Effective target {}, now {};
    float smoothCoef = 0.01f;
    int clock = 0;
    Random rng;

    // The place's processing, two copies for a cross-fade.
    std::array<Colour, 2> colours;
    int activeColour = 0;
    float colourFade = 1, colourFadeStep = 0.001f;
    std::vector<float> fadeL, fadeR, dryL, dryR;

    // Drift
    std::vector<float> driftLineL, driftLineR;
    int driftWrite = 0;
    Wander driftWanderL, driftWanderR;
    float driftNowL = 0, driftNowR = 0;
    // Tape
    std::vector<float> tapeLineL, tapeLineR;
    int tapeWrite = 0;
    float wowPhase = 0, flutterPhase = 0;
    OnePole tapeLp1L, tapeLp1R, tapeLp2L, tapeLp2R, hissTone, hissToneR;
    float follower = 0, followerRelease = 0.0001f, dropGain = 1, dropTarget = 1;
    int dropCountdown = 0;
    Random noise;
    // Filter
    Svf lowL, lowR;
    Biquad highL, highR;
    float filterLowAt = -1, filterHighAt = -1, filterResAt = -1;
    bool filterActive = false;
    // Chorus
    std::vector<float> chorusLineL, chorusLineR;
    int chorusWrite = 0;
    float chorusPhase = 0;
    // Granular
    struct Grain { bool active = false; float position = 0, step = 1, panL = 0.7f, panR = 0.7f; int age = 0, length = 1; };
    std::vector<float> grainLine;
    int grainWrite = 0, grainCountdown = 0;
    std::array<Grain, 8> grains {};
    float placeGrainSize = 0, chorusStereo = 0, echoStereo = 0;
    // Echo
    std::vector<float> echoLineL, echoLineR;
    int echoWrite = 0;
    float echoDelay = 1, echoDelayTarget = 1, echoDelayCoef = 0.0005f;
    OnePole echoToneL, echoToneR, echoLowL, echoLowR;
};

} // namespace liminal::dsp
