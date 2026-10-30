#include "DSP/Space.h"
#include <algorithm>
#include <cmath>

namespace liminal::dsp {

namespace {
constexpr double lengthsMs[] { 37.1, 43.7, 51.3, 59.9, 67.3, 73.1, 83.9, 97.3 };
constexpr double rt60 = 2.4, preDelayMs = 18.0, dampingHz = 4200.0;
} // namespace

void Space::prepare(double sampleRate) {
    const double sr = std::clamp(sampleRate, 8000.0, 384000.0);
    for (int i = 0; i < lines; ++i) {
        auto& d = delays[std::size_t(i)];
        d.length = std::max(1, int(lengthsMs[i] * 0.001 * sr));
        d.data.assign(std::size_t(d.length), 0.0f);
        d.gain = float(std::pow(10.0, -3.0 * double(d.length) / (rt60 * sr)));
    }
    preLength = std::max(1, int(preDelayMs * 0.001 * sr));
    pre.assign(std::size_t(preLength), 0.0f);
    damping = float(1.0 - std::exp(-6.283185307179586 * dampingHz / sr));
    reset();
}

void Space::reset() {
    for (auto& d : delays) {
        std::fill(d.data.begin(), d.data.end(), 0.0f);
        d.write = 0;
        d.lp = 0;
    }
    std::fill(pre.begin(), pre.end(), 0.0f);
    preWrite = 0;
}

Stereo Space::tick(Stereo in) {
    if (pre.empty()) return {};
    const float input = pre[std::size_t(preWrite)];
    pre[std::size_t(preWrite)] = 0.5f * (in.l + in.r);
    if (++preWrite >= preLength) preWrite = 0;

    std::array<float, lines> out {};
    float sum = 0;
    for (int i = 0; i < lines; ++i) {
        auto& d = delays[std::size_t(i)];
        auto& y = out[std::size_t(i)];
        y = d.data[std::size_t(d.write)];
        d.lp += damping * (y - d.lp);
        y = d.lp * d.gain;
        sum += y;
    }
    // Householder reflection: each line gets its own output minus 2/N of the sum.
    const float mix = sum * (2.0f / float(lines));
    for (int i = 0; i < lines; ++i) {
        auto& d = delays[std::size_t(i)];
        d.data[std::size_t(d.write)] = out[std::size_t(i)] - mix + ((i & 1) ? -input : input);
        if (++d.write >= d.length) d.write = 0;
    }
    return { 0.35f * (out[0] - out[2] + out[4] - out[6]), 0.35f * (out[1] - out[3] + out[5] - out[7]) };
}

} // namespace liminal::dsp
