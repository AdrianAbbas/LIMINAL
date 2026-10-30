#pragma once
// An instrument's Distance control: how far from the listener it stands, before any room.
// Farther means quieter direct sound, high frequencies lost to air and obstacles (two
// one-poles, 18 kHz down to about 900 Hz), a few early reflections from nearby surfaces,
// and more of the instrument sent to the room. Smoothed per sample, so automation is
// click-free. At 0 it is transparent.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace liminal::dsp {

class Distance {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        line.assign(2 * (std::size_t(std::ceil(0.02 * sr)) + 2), 0.0f);   // 20 ms per channel
        smooth = float(1.0 - std::exp(-1.0 / (0.03 * sr)));
        const auto at = [&](double ms) { return std::clamp(int(ms * 0.001 * sr), 1, int(line.size() / 2) - 1); };
        taps = { at(7.3), at(13.1), at(9.4), at(16.6) };
        reset();
    }
    void reset() {
        std::fill(line.begin(), line.end(), 0.0f);
        pos = 0;
        l1 = l2 = r1 = r2 = 0;
        amount = target;
        coef = coefFor(amount);
    }
    void setAmount(float a) { target = std::clamp(a, 0.0f, 1.0f); }
    // In place; `send` receives the signal for the room (scaled by distance).
    void process(float* left, float* right, float* sendL, float* sendR, int n) {
        const float coefTarget = coefFor(target);
        const int size = int(line.size()) / 2;
        for (int i = 0; i < n; ++i) {
            amount += smooth * (target - amount);
            coef += smooth * (coefTarget - coef);
            if (amount < 1.0e-5f && target <= 0.0f) {   // transparent at zero
                l1 = l2 = left[i];
                r1 = r2 = right[i];
                sendL[i] += left[i];
                sendR[i] += right[i];
                continue;
            }
            l1 += coef * (left[i] - l1); l2 += coef * (l1 - l2);
            r1 += coef * (right[i] - r1); r2 += coef * (r1 - r2);
            line[std::size_t(pos)] = l2;
            line[std::size_t(size + pos)] = r2;
            const auto tap = [&](int channel, int delay) { return line[std::size_t(channel * size + (pos - delay + size) % size)]; };
            const float er = 0.3f * amount;
            const float direct = 1.0f - 0.55f * amount;
            const float outL = direct * l2 + er * (0.6f * tap(1, taps[0]) + 0.4f * tap(0, taps[1]));
            const float outR = direct * r2 + er * (0.6f * tap(0, taps[2]) + 0.4f * tap(1, taps[3]));
            if (++pos >= size) pos = 0;
            const float send = 1.0f + 1.5f * amount;
            left[i] = outL;
            right[i] = outR;
            sendL[i] += outL * send;
            sendR[i] += outR * send;
        }
    }

private:
    float coefFor(float a) const {
        const double hz = std::min(0.45 * sr, 18000.0 * std::pow(900.0 / 18000.0, double(a)));
        return float(1.0 - std::exp(-6.283185307 * hz / sr));
    }
    double sr = 48000.0;
    std::vector<float> line;
    int pos = 0;
    std::array<int, 4> taps {};
    float l1 = 0, l2 = 0, r1 = 0, r2 = 0, amount = 0, target = 0, coef = 1, smooth = 0.001f;
};

} // namespace liminal::dsp
