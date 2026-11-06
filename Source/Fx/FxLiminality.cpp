#include "Fx/FxLiminality.h"
#include <algorithm>
#include <cmath>

namespace liminal::fx {

namespace {
constexpr double twoPi = 6.283185307179586;
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }
float noise(std::uint32_t s, double x) { return dsp::lim::smoothNoise(s, x); }
float rnd(std::uint32_t h) { return dsp::lim::unit(h); }
void allpass(dsp::Biquad& f, double hz, double q, double sr) {
    const double w = twoPi * std::clamp(hz, 20.0, 0.45 * sr) / sr, c = std::cos(w), a = std::sin(w) / (2.0 * q);
    f.normalise(1.0 - a, -2.0 * c, 1.0 + a, 1.0 + a, -2.0 * c, 1.0 - a);
}
} // namespace

//==============================================================================
void PitchLine::prepare(int centreSamples, int rangeSamples, int crossfadeSamples) {
    centre = std::max(4, centreSamples);
    range = std::clamp(rangeSamples, 1, centre - 3);
    fadeLength = std::max(1, crossfadeSamples);
    searchLength = std::max(16, crossfadeSamples / 2);
    int n = 1;
    while (n < 2 * (centre + range) + searchLength + 64) n <<= 1;
    line.assign(std::size_t(n), 0.0f);
    mask = n - 1;
    reset();
}

void PitchLine::reset() {
    std::fill(line.begin(), line.end(), 0.0f);
    write = 0;
    d1 = d2 = double(centre);
    fading = false;
    fadePos = 0;
}

// 4-point Hermite: exact at whole-sample delays.
float PitchLine::read(double delay) const {
    const double p = double(write) - delay;
    const double fl = std::floor(p);
    const auto f = float(p - fl);
    const int i = int(fl);
    const float xm1 = line[std::size_t((i - 1) & mask)], x0 = line[std::size_t(i & mask)], x1 = line[std::size_t((i + 1) & mask)],
                x2 = line[std::size_t((i + 2) & mask)];
    if (f == 0.0f) return x0;
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

float PitchLine::process(float x, double ratio) {
    line[std::size_t(write)] = x;
    const double dev = 1.0 - ratio;
    if (dev == 0.0 && !fading && d1 != double(centre)) {   // unshifted: drift back to the centre, inaudibly (< 1 cent), then exactly
        const double e = double(centre) - d1;
        d1 = std::abs(e) <= 0.0005 ? double(centre) : d1 + std::copysign(0.0005, e);
    }
    d1 += dev;
    if (fading) d2 += dev;
    if (!fading && (d1 > double(centre + range) || d1 < double(centre - range))) {
        // Back towards the centre, cross-faded. Where exactly: the read point near the centre whose
        // recent sound best matches the one being left (the two then add up in phase: no dip, no
        // comb, no click while they cross).
        fading = true;
        const double frac = d1 - std::floor(d1);
        const int from = int(std::floor(d1));
        double best = -1.0e30;
        int pick = centre;
        for (int o = -range / 2; o <= range / 2; o += 2) {
            const int cand = centre + o;
            double xy = 0, yy = 0;
            for (int j = 0; j < searchLength; j += 2) {
                const float a = line[std::size_t((write - from - j) & mask)], b = line[std::size_t((write - cand - j) & mask)];
                xy += double(a) * b;
                yy += double(b) * b;
            }
            const double score = yy > 1.0e-20 ? xy / std::sqrt(yy) : 0.0;
            if (score > best) { best = score; pick = cand; }
        }
        d2 = double(pick) + frac;
        fadePos = 0;
        ++wrapCount;
    }
    float y;
    if (fading) {
        const float t = float(fadePos) / float(fadeLength);
        const float a = std::cos(0.5f * 3.14159265f * t), b = std::sin(0.5f * 3.14159265f * t);
        y = a * read(d1) + b * read(d2);
        if (++fadePos >= fadeLength) {
            fading = false;
            d1 = d2;
        }
    } else {
        y = read(d1);
    }
    write = (write + 1) & mask;
    return y;
}

//==============================================================================
void Jitter::prepare(int maxSamples, int crossfadeSamples) {
    int n = 1;
    while (n < maxSamples + 8) n <<= 1;
    line.assign(std::size_t(n), 0.0f);
    mask = n - 1;
    fadeLength = std::max(1, crossfadeSamples);
    reset();
}

void Jitter::reset() {
    std::fill(line.begin(), line.end(), 0.0f);
    write = now = from = next = fadePos = 0;
    fading = false;
}

float Jitter::process(float x) {
    line[std::size_t(write)] = x;
    if (!fading && next != now) {
        from = now;
        now = std::clamp(next, 0, mask - 4);
        fading = true;
        fadePos = 0;
    }
    float y = line[std::size_t((write - now) & mask)];
    if (fading) {
        const float t = float(fadePos) / float(fadeLength);
        const float a = std::cos(0.5f * 3.14159265f * t), b = std::sin(0.5f * 3.14159265f * t);
        y = a * line[std::size_t((write - from) & mask)] + b * y;
        if (++fadePos >= fadeLength) fading = false;
    }
    write = (write + 1) & mask;
    return y;
}

//==============================================================================
void AudioLiminality::prepare(double sampleRate, int centreSamples) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    centre = centreSamples;
    lim.setAudioInput(true);
    lim.prepare(sr, 0);
    const int range = std::min(centre - 4, int(std::lround(0.016 * sr)));
    pitchL.prepare(centre, range, int(std::lround(0.025 * sr)));
    pitchR.prepare(centre, range, int(std::lround(0.025 * sr)));
    for (auto& j : jitter) j.prepare(int(std::lround(0.05 * sr)), int(std::lround(0.04 * sr)));
    for (std::size_t k = 0; k < smear.size(); ++k) allpass(smear[k], k % 2 == 0 ? 640.0 + 90.0 * double(k) : 2300.0 + 210.0 * double(k), 0.6, sr);
    tiltL.setHz(1200.0f, sr);
    tiltR.setHz(1200.0f, sr);
    reset();
}

void AudioLiminality::reset() {
    lim.reset();
    pitchL.reset();
    pitchR.reset();
    for (auto& j : jitter) j.reset();
    for (auto& f : smear) f.clear();
    smearMix = smearTarget = 0;
    centsL = centsR = stepL = stepR = 0;
    sagStage = SagStage::idle;
    sag = sagDepth = sagRate = 0;
    sinceSag = sinceOnset = 100;
    sagHold = sagCount = 0;
    levelGain = 1;
    levelStep = tilt = tiltStep = 0;
    tiltL.z = tiltR.z = 0;
    fadeStage = FadeStage::idle;
    fadeGain = 1;
    fadeDepth = 1;
    fadeSpeed = fadeHold = 0;
    fadeLpNow = 1;
    fadeLpStep = 0;
    fl1 = fl2 = fr1 = fr2 = 0;
    fadeCount = 0;
    untilFade = 3.0f + 6.0f * rnd(salt(60));   // the first fade comes within the first ten seconds
    quiet = true;
}

void AudioLiminality::relocate(std::uint64_t clock) {
    lim.relocate(clock, false);
    clockNow = clock;
    untilFade = 3.0f + 6.0f * rnd(salt(61) ^ std::uint32_t(clock));
}

void AudioLiminality::onset(float strength) {
    lim.onset();
    sinceOnset = 0;
    // Sour Notes: this attack may come out sour.
    const float es = lim.amounts()[dsp::lim::sour];
    if (es <= 0.0f || sagStage != SagStage::idle || sinceSag < 0.6f) return;
    const std::uint32_t h = dsp::lim::hash(salt(31), std::uint32_t(clockNow / 64));
    const float chance = std::min(0.6f, 0.45f * std::pow(es, 1.1f)) * std::clamp(0.5f + strength, 0.5f, 1.0f);
    if (rnd(h) < chance) {
        sagStage = SagStage::down;
        sagDepth = -(30.0f + 22.0f * std::min(es, 1.0f) + 8.0f * rnd(dsp::lim::hash(h, 1)));
        sagRate = sagDepth / 0.05f;   // cents per second, down in 50 ms
        sagHold = int((0.12f + 0.23f * rnd(dsp::lim::hash(h, 2))) * float(sr));
        sinceSag = 0;
        ++sagCount;
    }
}

void AudioLiminality::control(std::uint64_t clock) {
    clockNow = clock;
    lim.controlAudio(clock);
    const auto& e = lim.amounts();
    const double t = double(clock) / sr;
    const float tick = float(64.0 / sr);
    using namespace dsp;
    // Ramps that have arrived end exactly (the stage can then be bypassed exactly).
    if (fadeStage == FadeStage::idle && std::abs(fadeGain - 1.0f) < 1.0e-4f) fadeGain = 1.0f;
    if (e[lim::uneven] <= 0.0f) {
        if (std::abs(levelGain - 1.0f) < 1.0e-4f) levelGain = 1.0f;
        if (std::abs(tilt) < 1.0e-5f) tilt = 0.0f;
    }

    // Sour Notes on sustained sound too (no attack for a while): now and then a sag.
    sinceSag += tick;
    sinceOnset += tick;
    const float es = e[lim::sour];
    if (es > 0.0f && sagStage == SagStage::idle && sinceSag > 1.0f && sinceOnset > 0.8f) {
        const std::uint32_t h = lim::hash(salt(32), std::uint32_t(clock / 64));
        if (rnd(h) < 0.25f * std::pow(es, 1.3f) * tick) {
            sagStage = SagStage::down;
            sagDepth = -(30.0f + 22.0f * std::min(es, 1.0f) + 8.0f * rnd(lim::hash(h, 1)));
            sagRate = sagDepth / 0.05f;
            sagHold = int((0.12f + 0.23f * rnd(lim::hash(h, 2))) * float(sr));
            sinceSag = 0;
            ++sagCount;
        }
    }
    switch (sagStage) {
    case SagStage::down:
        sag += sagRate * tick;
        if (sag <= sagDepth) { sag = sagDepth; sagStage = SagStage::hold; }
        break;
    case SagStage::hold:
        sagHold -= 64;
        if (sagHold <= 0) { sagStage = SagStage::up; sagRate = -sagDepth / 0.18f; }
        break;
    case SagStage::up:
        sag += sagRate * tick;
        if (sag >= 0.0f) { sag = 0; sagStage = SagStage::idle; }
        break;
    case SagStage::idle: break;
    }

    // The pitch, per side.
    const float ed = e[lim::detune], ew = e[lim::warble];
    const float detune = ed > 0.0f ? -28.0f * ed * (0.8f + 0.2f * noise(salt(1), t / 7.0)) : 0.0f;
    const float common = lim.modulatorsAt(t) + sag
                         + (vhsWarble > 0.0f ? 18.0f * vhsWarble * float(std::sin(std::fmod(twoPi * 0.9 * t, twoPi))) : 0.0f);
    float wl = 0, wr = 0;
    if (ew > 0.0f) {
        wl = 14.0f * ew * (0.7f * noise(salt(2), t / 1.6) + 0.3f * noise(salt(3), t / 0.55));
        wr = 14.0f * ew * (0.7f * noise(salt(4), t / 1.7) + 0.3f * noise(salt(5), t / 0.6));
    }
    const float nextL = detune * 1.12f + common + wl, nextR = detune * 0.88f + common + wr;
    stepL = (nextL - centsL) / 64.0f;
    stepR = (nextR - centsR) / 64.0f;

    // Unevenness: slow level and tone fluctuation.
    const float eu = e[lim::uneven];
    const float level = eu > 0.0f ? dbGain(3.5f * std::min(eu, 1.3f) * noise(salt(40), t / 2.9)) : 1.0f;
    levelStep = (level - levelGain) / 64.0f;
    const float tiltTarget = eu > 0.0f ? 0.55f * std::min(eu, 1.3f) * noise(salt(41), t / 3.7) : 0.0f;
    tiltStep = (tiltTarget - tilt) / 64.0f;

    // Fade: now and then the sound fades away (darker as it goes) and returns.
    const float ef = e[lim::fade];
    float g = fadeGain;
    switch (fadeStage) {
    case FadeStage::idle:
        g = 1.0f;
        if (ef > 0.0f) {
            untilFade -= tick;
            if (untilFade <= 0.0f) {
                const std::uint32_t h = lim::hash(salt(62), std::uint32_t(clock / 64));
                fadeStage = FadeStage::down;
                fadeDepth = dbGain(-(10.0f + 20.0f * std::min(ef, 1.2f)));
                fadeSpeed = 1.0f / (0.7f + 0.9f * rnd(lim::hash(h, 1)));
                fadeHold = 0.3f + 1.0f * rnd(lim::hash(h, 2));
                untilFade = (5.0f + 20.0f * rnd(lim::hash(h, 3))) / (0.35f + ef);
                fadeLpNow = 0;
                ++fadeCount;
            }
        }
        break;
    case FadeStage::down:
        fadeLpNow = std::min(1.0f, fadeLpNow + fadeSpeed * tick);   // (the phase, 0..1)
        g = 1.0f - (1.0f - fadeDepth) * (0.5f - 0.5f * std::cos(3.14159265f * fadeLpNow));
        if (fadeLpNow >= 1.0f) fadeStage = FadeStage::hold;
        break;
    case FadeStage::hold:
        g = fadeDepth;
        fadeHold -= tick;
        if (fadeHold <= 0.0f) {
            fadeStage = FadeStage::up;
            fadeSpeed = 1.0f / (0.9f + 1.1f * rnd(lim::hash(salt(63), std::uint32_t(clock / 64))));
            fadeLpNow = 0;
        }
        break;
    case FadeStage::up:
        fadeLpNow = std::min(1.0f, fadeLpNow + fadeSpeed * tick);
        g = fadeDepth + (1.0f - fadeDepth) * (0.5f - 0.5f * std::cos(3.14159265f * fadeLpNow));
        if (fadeLpNow >= 1.0f) { fadeStage = FadeStage::idle; g = 1.0f; }
        break;
    }
    fadeLpStep = (g - fadeGain) / 64.0f;   // (the gain's per-sample step; the darkening follows the gain)

    // Timing: the sends' delays jump now and then (each at its own moment), with a little diffusion.
    const float et = e[lim::timing];
    smearTarget = 0.6f * std::min(et, 1.0f);
    for (std::size_t k = 0; k < jitter.size(); ++k) {
        const double period = 0.25 + 0.65 * double(rnd(salt(70 + std::uint32_t(k))));
        const auto slot = std::uint32_t(t / period);
        const float r = rnd(lim::hash(salt(80 + std::uint32_t(k)), slot));
        jitter[k].setTarget(et > 0.0f ? int(0.045f * float(sr) * std::min(et, 1.3f) * std::pow(r, 1.3f)) : 0);
    }

    const bool pitchStill = std::abs(nextL) < 1.0e-5f && std::abs(centsL) < 1.0e-5f && std::abs(nextR) < 1.0e-5f && std::abs(centsR) < 1.0e-5f;
    quiet = pitchStill && pitchL.atCentre() && pitchR.atCentre() && lim.audioBypassed() && sagStage == SagStage::idle && fadeStage == FadeStage::idle
            && fadeGain == 1.0f && level == 1.0f && levelGain == 1.0f && tiltTarget == 0.0f && tilt == 0.0f;
    if (pitchStill) { centsL = centsR = stepL = stepR = 0; }
}

void AudioLiminality::process(float* left, float* right, int n) {
    // The pitch line (always: it is the latency); unshifted it is an exact integer delay.
    for (int i = 0; i < n; ++i) {
        centsL += stepL;
        centsR += stepR;
        const double rl = centsL == 0.0f ? 1.0 : std::exp2(double(centsL) / 1200.0);
        const double rr = centsR == 0.0f ? 1.0 : std::exp2(double(centsR) / 1200.0);
        left[i] = pitchL.process(left[i], rl);
        right[i] = pitchR.process(right[i], rr);
    }
    if (quiet) return;
    lim.process(left, right, n);   // Ghost, Warble's swell and chorus, Wear, dropouts
    const float sinceTop = 1.0f - fadeDepth;
    for (int i = 0; i < n; ++i) {
        levelGain += levelStep;
        tilt += tiltStep;
        fadeGain += fadeLpStep;
        float l = left[i], r = right[i];
        if (tilt != 0.0f) {   // Unevenness' tone: the highs against the lows
            const float lo = tiltL.lp(l), ro = tiltR.lp(r);
            l = lo * (1.0f - 0.5f * tilt) + (l - lo) * (1.0f + tilt);
            r = ro * (1.0f - 0.5f * tilt) + (r - ro) * (1.0f + tilt);
        }
        if (fadeGain < 1.0f) {   // Fade: quieter and darker, together
            const float close = sinceTop > 0.0f ? std::clamp((1.0f - fadeGain) / sinceTop, 0.0f, 1.0f) : 0.0f;
            const float hz = 18000.0f * std::exp2(-3.5f * close);
            const float c = float(1.0 - std::exp(-twoPi * double(hz) / sr));
            fl1 += c * (l - fl1);
            fl2 += c * (fl1 - fl2);
            fr1 += c * (r - fr1);
            fr2 += c * (fr1 - fr2);
            l = (l + close * (fl2 - l)) * fadeGain;
            r = (r + close * (fr2 - r)) * fadeGain;
        } else {
            fl1 = fl2 = l;
            fr1 = fr2 = r;
        }
        left[i] = l * levelGain;
        right[i] = r * levelGain;
    }
}

void AudioLiminality::processSends(float* roomL, float* roomR, float* echoL, float* echoR, int n) {
    const bool idle = std::all_of(jitter.begin(), jitter.end(), [](const Jitter& j) { return j.idle(); });
    if (idle && smearTarget == 0.0f && smearMix < 1.0e-6f) {
        smearMix = 0;
        return;
    }
    float* channels[4] { roomL, roomR, echoL, echoR };
    const float k = float(1.0 - std::exp(-1.0 / (0.03 * sr)));
    for (int i = 0; i < n; ++i) {
        smearMix += k * (smearTarget - smearMix);
        for (std::size_t c = 0; c < 4; ++c) {
            float y = jitter[c].process(channels[c][i]);
            if (smearMix > 1.0e-6f) {
                const float a = smear[2 * c + 1].tick(smear[2 * c].tick(y));
                y += smearMix * (a - y);
            }
            channels[c][i] = y;
        }
    }
}

} // namespace liminal::fx
