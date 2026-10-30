#include "DSP/Space.h"
#include "DSP/Instrument.h"
#include <algorithm>
#include <cmath>

namespace liminal::dsp {

namespace {
constexpr double lengthsMs[] { 37.1, 43.7, 51.3, 59.9, 67.3, 73.1, 83.9, 97.3 };
constexpr double diffuserMs[] { 4.7, 3.6, 12.7, 9.3 };
} // namespace

void Space::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    const auto samples = [&](double ms) { return std::size_t(std::ceil(ms * 0.001 * sr)) + 4; };
    for (int i = 0; i < lines; ++i) delays[std::size_t(i)].data.assign(samples(lengthsMs[i] * maxSize + 4.0), 0.0f);
    for (int i = 0; i < diffusers; ++i) diffuse[std::size_t(i)].data.assign(samples(diffuserMs[i] * std::sqrt(maxSize)), 0.0f);
    early.assign(samples(maxPreDelayMs + maxReflectionMs), 0.0f);
    configure(shape, 1);
    reset();
}

void Space::configure(const Shape& s, unsigned seed) {
    shape = s;
    const float size = std::clamp(s.size, 0.3f, maxSize);
    for (int i = 0; i < lines; ++i) {
        auto& d = delays[std::size_t(i)];
        d.length = std::clamp(int(lengthsMs[i] * size * 0.001 * sr), 8, int(d.data.size()) - 4);
        d.rate = 0.13f + 0.07f * float(i);
        d.phase = float(i) / float(lines);
    }
    for (int i = 0; i < diffusers; ++i) {
        auto& a = diffuse[std::size_t(i)];
        a.length = std::clamp(int(diffuserMs[i] * std::sqrt(size) * 0.001 * sr), 2, int(a.data.size()) - 1);
    }
    updateDecay();
    diffuseCoef = 0.75f * std::clamp(s.diffusion, 0.0f, 1.0f);
    crossover = float(1.0 - std::exp(-6.283185307 * s.crossoverHz / sr));
    modSamples = float(std::clamp(s.modulation, 0.0f, 1.0f) * 0.0012 * sr);   // up to 1.2 ms
    const int historyMax = int(early.size()) - 2;
    preDelay = std::clamp(int(s.preDelayMs * 0.001 * sr), 1, historyMax);

    // The first reflections: each environment's surfaces, as delays and stereo gains.
    Random rng;
    rng.state = 0x2545F491u ^ (seed * 0x9E3779B9u);
    taps = std::clamp(s.reflections, 0, maxTaps);
    float level = s.reflectionLevel;
    for (int k = 0; k < taps; ++k) {
        float ms = 0;
        float pan = 0;
        switch (s.pattern) {
        case Pattern::flutter:   // parallel walls: evenly spaced, alternating sides
            ms = s.firstMs + s.spacingMs * float(k) * (1.0f + 0.04f * rng.bipolar());
            pan = (k % 2 == 0 ? -0.7f : 0.7f);
            break;
        case Pattern::dense:     // many hard surfaces close by: irregular, packed
            ms = s.firstMs + s.spacingMs * float(k) * (0.4f + 1.2f * rng.uniform());
            pan = rng.bipolar() * 0.9f;
            break;
        case Pattern::sparse:    // a few big, far surfaces
            ms = s.firstMs + s.spacingMs * float(k) * (0.7f + 0.6f * rng.uniform());
            pan = rng.bipolar() * 0.8f;
            break;
        case Pattern::tunnel:    // a tube: ever later, ever further to one side (the tunnel mouth)
            ms = s.firstMs + s.spacingMs * std::pow(float(k), 1.15f) * (0.95f + 0.1f * rng.uniform());
            pan = k % 4 == 3 ? 0.5f : -0.2f - 0.7f * float(k) / float(std::max(1, s.reflections - 1));
            break;
        case Pattern::grid:      // a repeating structure: exactly even, alternating sides
            ms = s.firstMs + s.spacingMs * float(k);
            pan = (k % 2 == 0 ? -0.5f : 0.5f);
            break;
        }
        const auto g = panGains(pan);
        tapDelay[std::size_t(k)] = std::clamp(preDelay + int(ms * 0.001 * sr), 1, historyMax);
        tapL[std::size_t(k)] = level * g.l;
        tapR[std::size_t(k)] = level * g.r;
        level *= s.reflectionDecay;
    }
    lowCutHz = s.lowCutHz;
    highCutHz = s.highCutHz;
    lowCutL.setHz(lowCutHz, sr);
    lowCutR.setHz(lowCutHz, sr);
    highCutL.setHz(highCutHz, sr);
    highCutR.setHz(highCutHz, sr);
}

void Space::updateDecay() {
    for (auto& d : delays) {
        const double seconds = double(d.length) / sr;
        d.gLow = float(std::pow(10.0, -3.0 * seconds / std::max(0.1f, shape.rt60Low * decayScale)));
        d.gHigh = float(std::pow(10.0, -3.0 * seconds / std::max(0.05f, shape.rt60High * decayScale)));
    }
}

void Space::setDecayScale(float scale) {
    scale = std::clamp(scale, 0.25f, 4.0f);
    if (std::abs(scale - decayScale) < 0.005f * decayScale) return;
    decayScale = scale;
    updateDecay();
}

void Space::reset() {
    for (auto& d : delays) {
        std::fill(d.data.begin(), d.data.end(), 0.0f);
        d.write = 0;
        d.lp = 0;
    }
    for (auto& a : diffuse) {
        std::fill(a.data.begin(), a.data.end(), 0.0f);
        a.write = 0;
    }
    std::fill(early.begin(), early.end(), 0.0f);
    earlyWrite = 0;
    lowCutL.z = lowCutR.z = highCutL.z = highCutR.z = 0;
    modNow = lean ? 0.0f : modSamples;
}

Stereo Space::tick(Stereo in) {
    if (early.empty()) return {};
    const int historySize = int(early.size());
    early[std::size_t(earlyWrite)] = 0.5f * (in.l + in.r);
    const auto history = [&](int delay) { return early[std::size_t((earlyWrite - delay + historySize) % historySize)]; };
    float erL = 0, erR = 0;
    const int used = lean ? std::min(taps, 8) : taps;
    for (int k = 0; k < used; ++k) {
        const float x = history(tapDelay[std::size_t(k)]);
        erL += x * tapL[std::size_t(k)];
        erR += x * tapR[std::size_t(k)];
    }
    // The tail's input: the pre-delayed signal plus the reflections, diffused.
    float x = history(preDelay) + 0.5f * (erL + erR);
    if (++earlyWrite >= historySize) earlyWrite = 0;
    for (auto& a : diffuse) {
        const float delayed = a.data[std::size_t(a.write)];
        const float v = x + diffuseCoef * delayed;
        a.data[std::size_t(a.write)] = v;
        if (++a.write >= a.length) a.write = 0;
        x = delayed - diffuseCoef * v;
    }

    std::array<float, lines> out {};
    float sum = 0;
    modNow += 0.0002f * ((lean ? 0.0f : modSamples) - modNow);   // modulation glides in and out (no jump)
    for (int i = 0; i < lines; ++i) {
        auto& d = delays[std::size_t(i)];
        const int size = int(d.data.size());
        float y;
        if (modNow > 0.001f) {   // read a little behind, wobbling slowly
            d.phase += d.rate / float(sr);
            if (d.phase >= 1.0f) d.phase -= 1.0f;
            const float back = float(d.length) - modNow * (1.0f + sinCycle(d.phase));
            float pos = float(d.write) - back;
            while (pos < 0.0f) pos += float(size);
            const int a = int(pos);
            const float f = pos - float(a);
            const float y0 = d.data[std::size_t(a % size)], y1 = d.data[std::size_t((a + 1) % size)];
            y = y0 + f * (y1 - y0);
        } else {
            y = d.data[std::size_t((d.write - d.length + size) % size)];
        }
        d.lp += crossover * (y - d.lp);
        y = d.lp * d.gLow + (y - d.lp) * d.gHigh;   // two-band decay
        out[std::size_t(i)] = y;
        sum += y;
    }
    const float mix = sum * (2.0f / float(lines));
    for (int i = 0; i < lines; ++i) {
        auto& d = delays[std::size_t(i)];
        const int size = int(d.data.size());
        d.data[std::size_t(d.write)] = out[std::size_t(i)] - mix + ((i & 1) ? -x : x);
        if (++d.write >= size) d.write = 0;
    }
    const float a = 0.35f * (out[0] - out[2] + out[4] - out[6]);
    const float b = 0.35f * (out[1] - out[3] + out[5] - out[7]);
    const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * shape.width;
    float l = (mid + side) * shape.lateLevel + erL, r = (mid - side) * shape.lateLevel + erR;
    l = highCutL.lp(lowCutL.hp(l));
    r = highCutR.lp(lowCutR.hp(r));
    return { l, r };
}

} // namespace liminal::dsp
