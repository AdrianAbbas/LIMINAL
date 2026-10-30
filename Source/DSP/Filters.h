#pragma once
// Small filters shared by the Milestone 1b instruments and the Milestone 2 environments.
// No allocation; coefficients are set at control rate by their owners.
#include <algorithm>
#include <cmath>

namespace liminal::dsp {

// One-pole low-pass (and, by subtraction, high-pass).
struct OnePole {
    float z = 0, c = 1;
    void setHz(float hz, double sr) { c = float(1.0 - std::exp(-6.283185307 * std::clamp(double(hz), 1.0, 0.49 * sr) / sr)); }
    float lp(float x) { return z += c * (x - z); }
    float hp(float x) { return x - lp(x); }
};

// Topology-preserving state-variable filter (Zavalishin): low, band and high outputs.
struct Svf {
    float ic1 = 0, ic2 = 0, g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    float low = 0, band = 0, high = 0;
    void set(float hz, float q, double sr) {
        g = float(std::tan(3.14159265 * std::clamp(double(hz), 5.0, 0.48 * sr) / sr));
        k = 1.0f / std::max(0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick(float x) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2;
        band = v1;
        high = x - k * v1 - v2;
    }
    void clear() { ic1 = ic2 = low = band = high = 0; }
};

// RBJ biquad (transposed direct form II).
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
    float tick(float x) {
        const float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { s1 = s2 = 0; }
    void identity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void normalise(double b0_, double b1_, double b2_, double a0, double a1_, double a2_) {
        b0 = float(b0_ / a0); b1 = float(b1_ / a0); b2 = float(b2_ / a0); a1 = float(a1_ / a0); a2 = float(a2_ / a0);
    }
    void lowPass(double hz, double q, double sr) {
        const double w = 6.283185307 * std::clamp(hz, 10.0, 0.47 * sr) / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
        normalise((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highPass(double hz, double q, double sr) {
        const double w = 6.283185307 * std::clamp(hz, 5.0, 0.47 * sr) / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
        normalise((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak(double hz, double q, double db, double sr) {
        const double A = std::pow(10.0, db / 40), w = 6.283185307 * std::clamp(hz, 10.0, 0.47 * sr) / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
        normalise(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highShelf(double hz, double db, double sr) {
        const double A = std::pow(10.0, db / 40), w = 6.283185307 * std::clamp(hz, 10.0, 0.47 * sr) / sr, c = std::cos(w), s = std::sin(w);
        const double al = s / 2 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * al;
        normalise(A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                  (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// Band-limited sawtooth step correction (PolyBLEP), phase in [0, 1), inc per sample.
inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
inline float blepSaw(float phase, float inc) { return 2.0f * phase - 1.0f - polyBlep(phase, inc); }

} // namespace liminal::dsp

namespace liminal::dsp {
// sin(2 pi p) for p in [0, 1), a refined parabola (error under 0.1 % of full scale): cheap enough to
// run per oscillator per sample.
inline float sinCycle(float p) {
    const float u = 2.0f * p - 1.0f;
    const float y = -4.0f * u * (1.0f - std::abs(u));
    return y * (0.775f + 0.225f * std::abs(y));
}
} // namespace liminal::dsp
