#pragma once
// Each place's signature (2026-10-31, "ten truly distinct rooms"): one effect on the instruments
// that no other place uses, so any two rooms can be told apart blind. The research behind each
// one is in docs/ENVIRONMENT_RESEARCH.md; the numbers per place are in Signature.cpp.
//
//   hospital     flutter echo between the corridor's parallel walls (a buzzing train of repeats
//                every 2 x width / c, high-passed: the cold "zing"), thin low end
//   mall         the distant PA: the instrument heard over ceiling loudspeakers in another wing
//                (band-limited, small-speaker resonances, transformer grit, several speakers'
//                late arrivals, most of it sent into the atrium's tail)
//   school       nostalgic tape wobble (wow + flutter) and steel lockers ringing in sympathy
//   garage       slap-back: discrete, dark repeats off pillars and the far wall, no feedback,
//                and the low ceiling's boom (its first axial mode)
//   pool         the underwater layer: half of the sound heard below the surface (steep
//                low-pass, slow wobble in pitch and level), blendable
//   hotel        through the wall: the instrument from the next room (mass-air-mass resonance,
//                mass-law low-pass, a faint coincidence ring)
//   subway       receding tunnel echoes: repeats travelling away down the tube, each later one
//                darker, quieter, further to one side, drifting flat, rails ringing in them
//   supermarket  the ceiling-speaker grid: the instrument from several small speakers at
//                different distances and positions (comb filtering, gloss) and the polished floor
//   office       hum-infected: the ballast's 120 Hz buzz imprinted on the instrument (amplitude
//                modulation by the rectified mains), and dead, dry treatment
//   impossible   wrong acoustics: reversed swells before the sound, pitch-shifted reflections,
//                and now and then the sound freezes in the air
//
// Each Character varies its place's signature audibly (e.g. pool Submerged pushes the underwater
// layer). The stage runs on the instruments' dry bus inside each environment chain, so it cross-
// fades with the place. Buffers are allocated in prepare(); configure() never allocates.
#include "DSP/Environment.h"
#include "DSP/Filters.h"
#include <array>
#include <vector>

namespace liminal::dsp {

class Signature {
public:
    // What the place's Character does to the signature (one row per Character, see Signature.cpp).
    struct Settings {
        float depth = 1;      // how much of the signature (0..1+)
        float spread = 1;     // a geometry factor: corridor width, tap spacing, distance (x)
        float tone = 1;       // brightness of the signature's own part (x its corner frequencies)
        float extra = 0;      // a per-place twist (lockers, boom, comb, freeze rate ... 0..1)
        int voices = 0;       // a count where the place needs one (speakers, taps); 0 = the default
        bool mirror = false;  // the other way round (subway: the echoes come from the other tunnel)
    };
    static Settings settingsFor(Place, int character);

    void prepare(double sampleRate);
    void configure(Place, int character, unsigned seed);   // then reset()
    void reset();
    // The ballast's light (1 = steady) for the office's hum: a stutter of the tube deepens it.
    void setLamp(float l) { lamp = l; }
    // in: the instruments' dry bus. out: what the place makes of it (replaces the dry sound);
    // send: extra signal for the room (the mall's PA feeds the atrium). n <= 1024.
    void process(const float* inL, const float* inR, float* outL, float* outR, float* sendL, float* sendR, int n);
    Place place() const { return where; }

private:
    float read(const std::vector<float>& line, float delay) const;   // fractional, behind `write`
    double sr = 48000.0;
    Place where = Place::hospital;
    Settings s;
    float lamp = 1;
    unsigned rngState = 1;
    float random01();

    // Shared building blocks (each place uses the ones it needs).
    std::vector<float> lineL, lineR;    // up to 1.6 s
    int write = 0;
    std::vector<float> combL, combR;    // short feedback lines (flutter, rails): up to 60 ms
    int combWrite = 0;
    float combDL = 1, combDR = 1, combFb = 0;
    std::array<Biquad, 16> bq {};       // per-place filters
    std::array<OnePole, 16> op {};
    std::array<Svf, 8> res {};          // resonators (lockers, boom)
    std::array<float, 8> resGain {};
    struct Tap { float ms = 0, gain = 0, pan = 0, cents = 0, modHz = 0, phase = 0; OnePole lpL, lpR; };
    std::array<Tap, 10> taps {};
    int tapCount = 0;
    float phaseA = 0, phaseB = 0, phaseC = 0, wander = 0, wanderTarget = 0;
    int wanderClock = 0;
    float dryGain = 1, wetGain = 1, sendGain = 0, blend = 0, makeup = 1;
    // Impossible room: the reversed swell and the freeze.
    std::vector<float> revL, revR;      // two blocks
    int revPos = 0, revBlock = 1;
    PitchShift shiftA, shiftB;
    std::vector<float> freezeL, freezeR;
    int freezeLength = 1, freezePos = 0, freezeClock = 0, freezeNext = 0;
    float freezeEnv = 0, freezeTarget = 0, freezeHeld = 0;
    float level = 0;   // the input's recent level (the freeze waits for something to hold)
    float stutter = 0; int stutterClock = 0, stutterNext = 0;   // the office's own ballast stutter
    std::array<std::vector<float>, 4> ap;
    std::array<int, 4> apPos {};
};

} // namespace liminal::dsp
