#pragma once
// Matched models for what an impulse response can't hold (pedal-capture milestone): a tape
// echo (wow/flutter, repeats that darken, saturation in the loop), a modulation voice
// (chorus/flanger from a measured rate/depth) and an octave-up shifter for shimmer.
// Each is driven by a profile whose fields are exactly what tools/captures/analyze.py measures,
// so a measured unit drops straight in. All buffers are made in prepare(); process is
// real-time safe.
#include <algorithm>
#include <cmath>
#include <vector>

namespace liminal::dsp {

// What analyze.py reports for an echo (report.json -> measurements.echo / tone_modulation).
struct TapeEchoProfile {
    float echoMs = 380.0f;
    float feedbackDb = -6.0f;          // per repeat, 1 kHz octave
    float darkeningDb = -2.5f;         // per repeat: (4 kHz - 500 Hz) tilt change
    float lowCutHz = 90.0f;            // the loop's thinning
    float wowHz = 0.7f, wowCents = 6.0f;
    float flutterHz = 6.0f, flutterCents = 1.5f;
    float drive = 0.3f;                // 0..1: loop saturation
};

class TapeEcho {
public:
    void prepare(double sampleRate, double maxSeconds = 2.0) {
        sr = sampleRate;
        int n = 1;
        while (n < int(maxSeconds * sampleRate) + 8) n <<= 1;
        line.assign(std::size_t(n), 0.0f);
        mask = n - 1;
        configure(profile);
        reset();
    }
    void reset() {
        std::fill(line.begin(), line.end(), 0.0f);
        write = 0;
        lp = hp = hpIn = 0;
        wowPhase = flutterPhase = 0;
    }
    // Not for every sample (solves the darkening filter), but allocation-free.
    void configure(const TapeEchoProfile& p) {
        profile = p;
        gain = std::pow(10.0f, p.feedbackDb / 20.0f);
        // One-pole low-pass whose 4 kHz vs 500 Hz difference equals the measured darkening.
        const auto tilt = [&](double fc) {
            const double a = std::exp(-2.0 * 3.141592653589793 * fc / sr);
            const auto mag = [&](double f) {
                const double w = 2.0 * 3.141592653589793 * f / sr;
                return (1.0 - a) / std::sqrt(1.0 - 2.0 * a * std::cos(w) + a * a);
            };
            return 20.0 * std::log10(mag(4000.0) / mag(500.0));
        };
        double lo = 200.0, hi = sr * 0.45;
        const double want = std::min(-0.01, double(p.darkeningDb));
        for (int i = 0; i < 50; ++i) {
            const double mid = std::sqrt(lo * hi);
            (tilt(mid) < want ? lo : hi) = mid;
        }
        lpA = float(std::exp(-2.0 * 3.141592653589793 * std::sqrt(lo * hi) / sr));
        // 1 kHz loss of that low-pass, made up in the loop gain so feedback stays as measured.
        const double w = 2.0 * 3.141592653589793 * 1000.0 / sr;
        const double m1k = (1.0 - lpA) / std::sqrt(1.0 - 2.0 * lpA * std::cos(w) + double(lpA) * lpA);
        gain /= float(m1k);
        hpA = float(std::exp(-2.0 * 3.141592653589793 * p.lowCutHz / sr));
        // Pitch deviation c cents at rate f <=> delay modulation amplitude c ln2/1200 / (2 pi f) s.
        wowDepth = float(p.wowCents * 0.000577623 / (6.283185307 * std::max(0.05f, p.wowHz)) * sr);
        flutterDepth = float(p.flutterCents * 0.000577623 / (6.283185307 * std::max(0.5f, p.flutterHz)) * sr);
        drive = 1.0f + 3.0f * std::clamp(p.drive, 0.0f, 1.0f);
    }
    // Wet only (the repeats), mono.
    float tick(float in) {
        const double twoPi = 6.283185307179586;
        wowPhase += profile.wowHz / sr;
        flutterPhase += profile.flutterHz / sr;
        wowPhase -= std::floor(wowPhase);
        flutterPhase -= std::floor(flutterPhase);
        const float d = profile.echoMs * 0.001f * float(sr) + wowDepth * float(std::sin(twoPi * wowPhase)) +
                        flutterDepth * float(std::sin(twoPi * flutterPhase));
        const float pos = float(write) - d;
        const int i0 = int(std::floor(pos));
        const float fr = pos - float(i0);
        // Cubic (Catmull-Rom) read.
        const float y0 = line[std::size_t((i0 - 1) & mask)], y1 = line[std::size_t(i0 & mask)];
        const float y2 = line[std::size_t((i0 + 1) & mask)], y3 = line[std::size_t((i0 + 2) & mask)];
        const float rd = y1 + 0.5f * fr * (y2 - y0 + fr * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + fr * (3.0f * (y1 - y2) + y3 - y0)));
        // The loop: input + feedback -> saturation -> darkening -> thinning -> the tape.
        float v = in + gain * rd;
        v = std::tanh(v * drive) / drive;
        lp = (1.0f - lpA) * v + lpA * lp;
        const float h = 0.5f * (1.0f + hpA) * (lp - hpIn) + hpA * hp;
        hpIn = lp;
        hp = h;
        line[std::size_t(write)] = h;
        write = (write + 1) & mask;
        return rd;
    }
    const TapeEchoProfile& current() const { return profile; }

private:
    double sr = 48000.0;
    TapeEchoProfile profile;
    std::vector<float> line;
    int mask = 0, write = 0;
    float gain = 0.5f, lpA = 0.5f, hpA = 0.99f, lp = 0, hp = 0, hpIn = 0, drive = 1.0f;
    float wowDepth = 0, flutterDepth = 0;
    double wowPhase = 0, flutterPhase = 0;
};

// Chorus / flanger voice: a delay swept by a sine LFO, with feedback (a flanger's "regen").
struct ModProfile {
    float rateHz = 0.25f, baseMs = 3.0f, depthMs = 1.5f, feedback = 0.3f, mix = 0.5f;
};

class ModVoice {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        int n = 1;
        while (n < int(0.05 * sampleRate) + 8) n <<= 1;
        line.assign(std::size_t(n), 0.0f);
        mask = n - 1;
        write = 0;
        phase = 0;
        last = 0;
    }
    void configure(const ModProfile& p, double startPhase = 0.0) { prof = p; phase = startPhase; }
    void reset() { std::fill(line.begin(), line.end(), 0.0f); write = 0; last = 0; }
    float tick(float in) {
        phase += prof.rateHz / sr;
        phase -= std::floor(phase);
        const float d = std::max(1.0f, (prof.baseMs + prof.depthMs * float(std::sin(6.283185307179586 * phase))) * 0.001f * float(sr));
        const float pos = float(write) - d;
        const int i0 = int(std::floor(pos));
        const float fr = pos - float(i0);
        const float rd = line[std::size_t(i0 & mask)] * (1.0f - fr) + line[std::size_t((i0 + 1) & mask)] * fr;
        line[std::size_t(write)] = in + prof.feedback * rd;
        write = (write + 1) & mask;
        last = rd;
        return in * (1.0f - prof.mix) + rd * prof.mix;
    }

private:
    double sr = 48000.0, phase = 0;
    ModProfile prof;
    std::vector<float> line;
    int mask = 0, write = 0;
    float last = 0;
};

// Octave-up pitch shifter: two read heads sweeping a delay line at double speed, Hann-crossfaded
// (a classic delay-line shifter; grain 60 ms). Feeds a reverb for shimmer.
class OctaveShifter {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        grain = int(0.06 * sampleRate);
        int n = 1;
        while (n < 2 * grain + 8) n <<= 1;
        line.assign(std::size_t(n), 0.0f);
        mask = n - 1;
        write = 0;
        ramp = 0;
    }
    float tick(float in) {
        line[std::size_t(write)] = in;
        // Each head's delay falls from `grain` to 0 at rate 1 sample per sample (ratio 2).
        float out = 0.0f;
        for (int h = 0; h < 2; ++h) {
            const float r = std::fmod(ramp + 0.5f * float(h), 1.0f);
            const float d = float(grain) * (1.0f - r) + 1.0f;
            const float pos = float(write) - d;
            const int i0 = int(std::floor(pos));
            const float fr = pos - float(i0);
            const float s = line[std::size_t(i0 & mask)] * (1.0f - fr) + line[std::size_t((i0 + 1) & mask)] * fr;
            out += s * (0.5f - 0.5f * float(std::cos(6.283185307179586 * r)));
        }
        ramp += 1.0f / float(grain);
        if (ramp >= 1.0f) ramp -= 1.0f;
        write = (write + 1) & mask;
        return out;
    }

private:
    double sr = 48000.0;
    std::vector<float> line;
    int mask = 0, write = 0, grain = 2880;
    float ramp = 0;
};

}  // namespace liminal::dsp
