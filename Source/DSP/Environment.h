#pragma once
// ENVIRONMENTS (Milestone 2a/2b): each place is a complete sound-design configuration, not a
// reverb preset. It has
//   - its own ambience (DSP/Ambience: bed generators and incidental events),
//   - its own room (DSP/Space: size, two-band decay, reflection pattern, diffusion, band),
//   - its own echoes (a tuned multi-tap delay: the corridor's far wall, the pool's tiles),
//   - and its own *instrument processing* (Colour): filtering, resonances (peaks and tuned
//     comb resonators or moving notches), pitch instability, chorus/detune, frequency
//     shifting, tape-like saturation, electrical tremolo, stereo width and movement, and
//     how far away the instruments sound. This is what makes every instrument change with
//     the place, more than any instrument control does.
// `Environment Amount` scales the instrument processing (0 = untouched, as Milestone 1);
// `Space` scales the room, as before.
//
// Changing environment cross-fades two complete chains: the old one stops receiving the
// instruments (its tail rings out naturally) and its ambience fades over two seconds
// while the new one fades in. Buffers are allocated in prepare(); switching never
// allocates.
#include "DSP/Ambience.h"
#include "DSP/Filters.h"
#include "DSP/Space.h"
#include <array>
#include <vector>

namespace liminal::dsp {

// The order is the saved parameter index: append only.
enum class Place { hospital, mall, school, garage, pool, hotel, subway, supermarket, office, impossible };
inline constexpr int placeCount = 10;

struct Profile {
    const char* name;      // shown in the interface
    const char* location;  // the camcorder caption's second half
    Space::Shape room;
    struct Colour {
        float highPassHz = 10, lowPassHz = 22000, lowPassQ = 0.707f;
        bool steep = false;                           // low-pass twice (24 dB/oct): behind a wall
        float peakHz = 1000, peakQ = 1, peakDb = 0;
        float peak2Hz = 300, peak2Q = 1, peak2Db = 0;
        float shelfHz = 4000, shelfDb = 0;
        // Tuned comb resonators (pipes, rails, metal) or, with `notch`, a moving
        // feed-forward comb (spectral filtering). Two per side at combHz and comb2Hz.
        float combHz = 0, comb2Hz = 0, combFeedback = 0, combMix = 0, combDamp = 0.3f;
        float combSweepHz = 0, combSweep = 0;        // slow movement of the comb (0..1 of its delay)
        bool notch = false;
        float shiftHz = 0, shiftMix = 0;              // frequency shift: inharmonic, dissonant partials
        float wowHz = 0.3f, wowCents = 0;            // slow pitch wobble
        float flutterHz = 6.0f, flutterCents = 0;     // fast pitch wobble (tape, old machinery)
        float driftCents = 0;                         // random, unsteady pitch
        float chorusHz = 0.5f, chorusCents = 0, chorusMix = 0;   // detuned doubling / wash
        float drive = 0;                              // 0..1 tape-like saturation
        float tremoloHz = 0, tremoloDepth = 0;        // electrical amplitude modulation
        float width = 1;                              // stereo width of the instruments
        float wanderPan = 0;                          // 0..1: the image drifts unpredictably side to side
        float dryDb = 0;                              // how present (near) the instruments are
    } colour;
    struct Echo {
        float leftMs = 250, rightMs = 370, feedback = 0, level = 0, lowPassHz = 4000, highPassHz = 150;
        bool pingPong = true;
        float modHz = 0, modMs = 0;    // the delay time wavers (tape, unstable machinery)
        float diffuse = 0;             // 0..0.7: each repeat smeared (an allpass in the loop)
        float shiftSemis = 0;          // each repeat shifted in pitch (nonphysical)
    } echo;
    float wetDb = 0;        // room level (at a given Space)
    float ambienceTrim = 1; // the bed's overall level
    float shimmerSemis = 0, shimmerFeedback = 0;   // pitch-shifted room output fed back into the room
};

const Profile& profile(Place);

// A granular pitch shifter: two read heads sweeping a short delay with crossfading windows.
class PitchShift {
public:
    void prepare(double sampleRate);
    void reset();
    void setSemitones(float semis);
    float tick(float x);

private:
    std::vector<float> line;
    int write = 0;
    float phase = 0, step = 0, window = 1;
};

// A single-sideband frequency shifter (Hilbert pair of allpass chains): every partial moves
// by the same number of hertz, so harmonics stop being harmonic.
class FrequencyShift {
public:
    void reset();
    float tick(float x, float cosine, float sine);

private:
    struct Section { float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    std::array<Section, 4> re {}, im {};
    float imDelay = 0;
    bool ready = false;
};

// The instrument processing of one place, for one stereo signal.
class Colour {
public:
    void prepare(double sampleRate);
    void configure(const Profile::Colour&, unsigned seed);
    void reset();
    void setAmount(float a) { amountTarget = std::clamp(a, 0.0f, 1.0f); }
    void snapAmount() { amount = amountTarget; updateFilters(); }
    void process(float* left, float* right, int n);

private:
    void updateFilters();
    double sr = 48000.0;
    Profile::Colour c;
    float amount = 0, amountTarget = 0, amountCoef = 0.001f, filtersAt = -1;
    Biquad hpL, hpR, peakL, peakR, peak2L, peak2R, shelfL, shelfR, lpL, lpR, lp2L, lp2R;
    std::vector<float> lineL, lineR;
    int write = 0, clock = 0;
    float wowPhase = 0, flutterPhase = 0, chorusPhase = 0, tremoloPhase = 0, sweepPhase = 0, shiftPhase = 0;
    // The comb resonators: one line per side, two read points.
    std::vector<float> combL, combR;
    int combWrite = 0;
    float combD1 = 1, combD2 = 1;
    OnePole combLpL, combLpR;
    FrequencyShift shiftL, shiftR;
    Random rng;
    Wander drift, pan;
    float driftNow = 0, panNow = 0;
    bool bypassed = false;
};

// A tuned stereo echo: two taps (one per side), feedback (crossed for ping-pong) through
// a band-pass in the loop.
class Echo {
public:
    void prepare(double sampleRate);
    void configure(const Profile::Echo&);
    void reset();
    Stereo tick(Stereo in, float amount);

private:
    double sr = 48000.0;
    Profile::Echo e;
    std::vector<float> lineL, lineR;
    int write = 0, delayL = 1, delayR = 1;
    OnePole lpL, lpR, hpL, hpR;
    float modPhase = 0, modSamples = 0, apL = 0, apR = 0;
    std::vector<float> apLineL, apLineR;
    int apWrite = 0, apLength = 1;
    PitchShift shiftL, shiftR;
};

// One place, complete: what the processor renders for it.
class EnvironmentChain {
public:
    void prepare(double sampleRate);
    void configure(Place, unsigned seed);   // resets everything
    Place place() const { return current; }
    // Adds into out (instrument dry + echo + room + ambience); `instGain`/`ambGain`
    // fade this chain's inputs (used for cross-fades between places).
    void process(const float* instL, const float* instR, const float* sendL, const float* sendR, const float* ambienceGain,
                 const float* space, float* outL, float* outR, float* ambienceOut, int n);
    void setAmount(float a) { dryColour.setAmount(a); sendColour.setAmount(a); amountTarget = a; }
    void snapAmount() { dryColour.snapAmount(); sendColour.snapAmount(); amount = amountTarget; }
    // Fading: 1 = this is the current place; 0 = fading out.
    void setLive(bool on) { liveTarget = on ? 1.0f : 0.0f; }
    void snapLive() { live = ambienceFade = liveTarget; }
    // A quick fade of everything (before it is reused for another place).
    void kill() { killing = true; }
    bool killed() const { return killing && outFade <= 0.0f; }
    bool silent() const { return liveTarget == 0.0f && ambienceFade < 1.0e-4f && quietFor > quietLimit; }

private:
    double sr = 48000.0;
    Place current = Place::hospital;
    const Profile* p = nullptr;
    Colour dryColour, sendColour;
    Echo echo;
    Space room;
    Ambience ambience;
    PitchShift shimmer;
    float shimmerIn = 0;
    float dryGain = 1, wetGain = 1, amount = 1, amountTarget = 1, amountCoef = 0.001f;
    float live = 1, liveTarget = 1, liveCoef = 0.001f, ambienceFade = 1, ambienceCoef = 0.0001f;
    int quietFor = 0, quietLimit = 48000;
    bool killing = false;
    float outFade = 1, killStep = 0.001f;
    std::vector<float> bufL, bufR, sbufL, sbufR;
};

// Two chains and the cross-fade between them.
class Environments {
public:
    void prepare(double sampleRate, Place initial);
    void select(Place);         // called once per block with the parameter's value
    void setAmount(float a);
    void process(const float* instL, const float* instR, const float* sendL, const float* sendR, const float* ambienceGain,
                 const float* space, float* outL, float* outR, float* ambienceOut, int n);
    Place place() const { return chains[std::size_t(active)].place(); }
    bool fading() const { return !chains[std::size_t(1 - active)].silent(); }

private:
    void start(Place);
    std::array<EnvironmentChain, 2> chains;
    std::array<bool, 2> running { true, false };
    int active = 0;
    int pending = -1;   // a place waiting for the other chain to finish its quick fade
    float amount = 1;
    unsigned changes = 0;
};

} // namespace liminal::dsp
