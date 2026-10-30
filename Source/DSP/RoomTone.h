#pragma once
// The Milestone 0 ambience: the "room tone" of an empty corridor at night. Air (soft,
// slowly breathing pink noise, decorrelated left/right), the mains hum of the lights and
// a faint fluorescent buzz that flickers now and then. A bed, not a feature: it sits
// 10-15 dB under the test instrument. Milestone 2 replaces it with ten environments.
// Deterministic (fixed seeds) and real-time safe.
#include "DSP/Stereo.h"
#include <cstdint>

namespace liminal::dsp {

class RoomTone {
public:
    void prepare(double sampleRate);
    void reset();
    Stereo tick();

private:
    struct Pink { float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0; float next(float white); };
    float random();       // 0..1
    float white() { return random() * 2.0f - 1.0f; }

    double sr = 48000.0;
    std::uint32_t rng = 0x9e3779b9u;
    Pink pinkL, pinkR;
    float lpL = 0, lpR = 0, lpCoef = 0.1f;
    float breath = 1, breathTarget = 1, breathCoef = 0.0001f;
    int breathCountdown = 0;
    double humPhase = 0, humStep = 0;
    float buzz = 1, buzzTarget = 1, buzzCoef = 0.01f;
    int buzzCountdown = 0;
};

} // namespace liminal::dsp
