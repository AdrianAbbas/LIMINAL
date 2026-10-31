#pragma once
// The VHS look's optional audio side (2026-10-31, off by default): while the picture's tape
// struggles through a heavy tracking glitch, the sound warbles faintly with it - a slow and a
// quicker pitch wobble (at most ~25 cents), a slight level dip and a little lost top end.
//
// The editor decides when the tape glitches (the picture is the event) and publishes how hard
// (0..1, LiminalProcessor::vhsWarble); this follows it smoothly. With the window closed nothing
// glitches, so nothing warbles. Switched on, the sound runs through a 4 ms delay line (constant,
// so the wobble never needs to cross-fade with the dry sound); switching it on or off cross-fades
// over 30 ms. Real-time safe: the buffers are made in prepare().
#include <cmath>
#include <vector>

namespace liminal::dsp {

class TapeWarble {
public:
    void prepare(double sampleRate) {
        rate = sampleRate;
        int n = 1;
        while (n < int(sampleRate * 0.02) + 8) n <<= 1;
        mask = n - 1;
        for (auto& b : lines) b.assign(std::size_t(n), 0.0f);
        write = 0;
        mix = depth = 0;
        phase1 = phase2 = 0;
        low[0] = low[1] = 0;
        mixStep = float(1.0 - std::exp(-1.0 / (0.03 * sampleRate)));
        depthStep = float(1.0 - std::exp(-1.0 / (0.06 * sampleRate)));
    }

    // `on`: the parameter; `target`: how hard the tape is glitching now (0..1).
    void process(float* left, float* right, int samples, bool on, float target) {
        if (lines[0].empty() || (!on && mix < 1.0e-5f)) {
            mix = 0;
            return;
        }
        target = on ? std::fmin(1.0f, std::fmax(0.0f, target)) : 0.0f;
        const float onTarget = on ? 1.0f : 0.0f;
        const double twoPi = 6.283185307179586;
        for (int i = 0; i < samples; ++i) {
            mix += mixStep * (onTarget - mix);
            depth += depthStep * (target - depth);
            phase1 += 5.3 / rate;
            phase2 += 0.83 / rate;
            if (phase1 >= 1.0) phase1 -= 1.0;
            if (phase2 >= 1.0) phase2 -= 1.0;
            const float delayMs = 4.0f + depth * float(0.45 * std::sin(twoPi * phase1) + 0.25 * std::sin(twoPi * phase2));
            const float d = delayMs * 0.001f * float(rate);
            const int whole = int(d);
            const float frac = d - float(whole);
            // A little top end lost while it struggles (about 20 kHz down to 6 kHz).
            const float lp = depth < 1.0e-3f ? 1.0f : 1.0f - std::exp(-float(twoPi) * (20000.0f - 14000.0f * depth) / float(rate));
            const float gain = 1.0f - 0.18f * depth;
            float* io[2] { left, right };
            for (int c = 0; c < 2; ++c) {
                if (io[c] == nullptr) continue;
                auto& line = lines[std::size_t(c)];
                const float dry = io[c][i];
                line[std::size_t(write)] = dry;
                const float a = line[std::size_t((write - whole) & mask)], b = line[std::size_t((write - whole - 1) & mask)];
                low[c] += lp * ((a + frac * (b - a)) - low[c]);
                io[c][i] = dry + mix * (low[c] * gain - dry);
            }
            write = (write + 1) & mask;
        }
    }

private:
    double rate = 48000, phase1 = 0, phase2 = 0;
    std::vector<float> lines[2];
    int mask = 0, write = 0;
    float mix = 0, depth = 0, mixStep = 0, depthStep = 0, low[2] {};
};

} // namespace liminal::dsp
