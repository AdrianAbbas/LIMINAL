#include "DSP/RoomTone.h"
#include <algorithm>
#include <cmath>

namespace liminal::dsp {

namespace {
constexpr double twoPi = 6.283185307179586;
// Levels (linear) chosen so the bed measures about -41 dBFS RMS at the output by default.
constexpr float airLevel = 0.066f, humLevel = 0.0048f, buzzLevel = 0.0021f;
} // namespace

float RoomTone::Pink::next(float w) {
    // Paul Kellet's refined pink-noise filter.
    b0 = 0.99886f * b0 + w * 0.0555179f;
    b1 = 0.99332f * b1 + w * 0.0750759f;
    b2 = 0.96900f * b2 + w * 0.1538520f;
    b3 = 0.86650f * b3 + w * 0.3104856f;
    b4 = 0.55000f * b4 + w * 0.5329522f;
    b5 = -0.7616f * b5 - w * 0.0168980f;
    const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
    b6 = w * 0.115926f;
    return out * 0.11f;
}

void RoomTone::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    lpCoef = float(1.0 - std::exp(-twoPi * 1400.0 / sr));
    breathCoef = float(1.0 - std::exp(-1.0 / (1.8 * sr)));
    buzzCoef = float(1.0 - std::exp(-1.0 / (0.004 * sr)));
    humStep = twoPi * 60.0 / sr;
    reset();
}

void RoomTone::reset() {
    rng = 0x9e3779b9u;
    pinkL = {};
    pinkR = {};
    lpL = lpR = 0;
    breath = breathTarget = 1;
    breathCountdown = 0;
    humPhase = 0;
    buzz = buzzTarget = 1;
    buzzCountdown = int(sr * 3);
}

float RoomTone::random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return float(rng >> 8) * (1.0f / 16777216.0f);
}

Stereo RoomTone::tick() {
    // The air breathes: a new target loudness every 2-6 s, approached slowly.
    if (--breathCountdown <= 0) {
        breathTarget = 0.8f + 0.4f * random();
        breathCountdown = int(sr * (2.0 + 4.0 * random()));
    }
    breath += breathCoef * (breathTarget - breath);
    lpL += lpCoef * (pinkL.next(white()) - lpL);
    lpR += lpCoef * (pinkR.next(white()) - lpR);

    // A tired fluorescent tube: mostly steady, sometimes it stutters for a moment.
    if (--buzzCountdown <= 0) {
        const bool stutter = buzzTarget >= 1.0f && random() < 0.35f;
        buzzTarget = stutter ? 0.15f + 0.3f * random() : 1.0f;
        buzzCountdown = stutter ? int(sr * (0.03 + 0.12 * random())) : int(sr * (1.5 + 6.0 * random()));
    }
    buzz += buzzCoef * (buzzTarget - buzz);

    humPhase += humStep;
    if (humPhase > twoPi) humPhase -= twoPi;
    const float hum = humLevel * (float(std::sin(humPhase)) + 0.5f * float(std::sin(2.0 * humPhase)) + 0.25f * float(std::sin(3.0 * humPhase)));
    // The buzz is the tube's 120 Hz with its harsh upper harmonics, kept very quiet.
    const float b = buzzLevel * buzz * (float(std::sin(2.0 * humPhase)) * 0.5f + float(std::sin(6.0 * humPhase)) * 0.35f
                                        + float(std::sin(10.0 * humPhase)) * 0.25f + float(std::sin(14.0 * humPhase)) * 0.15f);
    return { airLevel * breath * lpL + hum + b * 1.1f, airLevel * breath * lpR + hum * 0.9f + b * 0.8f };
}

} // namespace liminal::dsp
