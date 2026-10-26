#include "Instruments.h"
#include <algorithm>
#include <cmath>
namespace liminal
{
namespace
{
constexpr double tau = 6.283185307179586;
constexpr Param first[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
double wave(double &phase, double hz, double sr)
{
    phase += hz / sr;
    phase -= std::floor(phase);
    return std::sin(tau * phase);
}
} // namespace
void Instruments::prepare(double rate, const Settings &settings)
{
    sr = std::clamp(rate, 8000., 192000.);
    target = sanitise(settings);
    families = {};
    pedal = {};
    bendTarget = {};
    bendCurrent = {};
    rng = 12345;
    smoothing = float(1 - std::exp(-1 / (sr * .025)));
    for (int i = 0; i < 6; ++i)
    {
        auto &f = families[i];
        f.on = target[first[i]];
        f.level = target[first[i] + 1];
        for (int j = 0; j < 6; ++j)
            f.controls[j] = target[first[i] + 2 + j];
    }
}
void Instruments::setSettings(const Settings &s) noexcept
{
    target = s;
}
float Instruments::noise() noexcept
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return float(rng >> 8) * (2.f / 16777216.f) - 1;
}
void Instruments::noteOn(int ch, int note, float velocity) noexcept
{
    if (velocity <= 0)
    {
        noteOff(ch, note);
        return;
    }
    ch = std::clamp(ch - 1, 0, 15);
    note = std::clamp(note, 0, 127);
    for (int i = 0; i < 6; ++i)
    {
        auto &f = families[i];
        if (target[first[i]] < .5f)
            continue;
        auto *v = &*std::min_element(
            f.voices.begin(), f.voices.end(), [](auto &a, auto &b)
            { return (a.note < 0 ? -1.f : a.envelope) < (b.note < 0 ? -1.f : b.envelope); });
        if (v->note >= 0)
        {
            auto *tail = &*std::min_element(
                f.tails.begin(), f.tails.end(), [](auto &a, auto &b)
                { return (a.note < 0 ? -1.f : a.envelope) < (b.note < 0 ? -1.f : b.envelope); });
            *tail = *v;
            tail->stealing = true;
            tail->control = 0;
        }
        *v = Voice{};
        v->note = note;
        v->channel = ch;
        v->held = true;
        v->velocity = std::clamp(velocity, 0.f, 1.f);
        v->frequency = 440 * std::pow(2., (note - 69) / 12.);
    }
}
void Instruments::noteOff(int ch, int note) noexcept
{
    ch = std::clamp(ch - 1, 0, 15);
    for (auto &f : families)
        for (auto &v : f.voices)
            if (v.note == note && v.channel == ch)
            {
                v.control = 0;
                v.held = false;
                v.sustained = pedal[ch];
            }
}
void Instruments::sustain(int ch, bool down) noexcept
{
    ch = std::clamp(ch - 1, 0, 15);
    pedal[ch] = down;
    if (!down)
        for (auto &f : families)
            for (auto &v : f.voices)
                if (v.channel == ch)
                    v.sustained = false, v.control = 0;
}
void Instruments::bend(int ch, int value) noexcept
{
    bendTarget[std::clamp(ch - 1, 0, 15)] = float(std::clamp(value, 0, 16383) - 8192) / 8192.f * 2;
}
void Instruments::allNotesOff(int ch, bool immediately) noexcept
{
    ch = std::clamp(ch - 1, 0, 15);
    pedal[ch] = false;
    for (auto &f : families)
    {
        for (auto &v : f.voices)
            if (v.channel == ch)
            {
                v.held = v.sustained = false, v.control = 0;
                if (immediately)
                    v = Voice{};
            }
        for (auto &v : f.tails)
            if (v.channel == ch)
            {
                v.held = v.sustained = false, v.control = 0;
                if (immediately)
                    v = Voice{};
            }
    }
}
int Instruments::activeVoices() const noexcept
{
    int n = 0;
    for (auto &f : families)
    {
        for (auto &v : f.voices)
            n += v.note >= 0;
        for (auto &v : f.tails)
            n += v.note >= 0;
    }
    return n;
}
float Instruments::render(Voice &v, int family, const std::array<float, 6> &c) noexcept
{
    if (v.note < 0)
        return 0;
    v.age += 1 / sr;
    bool sounding = (v.held || v.sustained) && !v.stealing;
    if ((v.control++ & 31u) == 0)
    {
        double attack = .004, release = .12;
        if (family == 1)
        {
            attack = .005 + 3 * c[3] * c[3];
            release = .05 + 10 * c[4] * c[4];
        }
        if (family == 3)
        {
            attack = .12 + .6 * c[0];
            release = .2 + 14 * c[5] * c[5];
        }
        if (family == 5)
        {
            attack = .15 + 3 * c[0];
            release = .3 + 25 * c[5] * c[5];
        }
        if (v.stealing)
            release = .006;
        v.envelopeCoefficient = float(1 - std::exp(-1 / (sr * (sounding ? attack : release))));
        double hz = v.frequency * std::pow(2., bendCurrent[v.channel] / 12.);
        v.noiseLevel = 0;
        v.filterCoefficient = 1;
        v.drive = 0;
        v.gain = 1;
        auto mode = [&](int j, double f, double amplitude)
        {
            v.frequencies[j] = f;
            v.amplitudeTargets[j] = f < sr * .45 ? float(amplitude) : 0;
        };
        if (family == 0)
        {
            v.partials = 12;
            double decay = .6 + 9 * c[4];
            for (int j = 0; j < 12; ++j)
            {
                int h = j / 3 + 1;
                double det = (j % 3 - 1) * (1 + 10 * c[1]) + c[0] * std::sin(v.note * 1.17) * 3;
                mode(j,
                     hz * h * std::sqrt(1 + .00018 * c[0] * h * h) * std::pow(2., det / 1200.) *
                         (1 + .0007 * c[0] * std::sin(v.age * 2.1 + v.note)),
                     std::exp(-v.age * (1 + .28 * h) / decay) /
                         std::pow(h, 1.2 + 2 * c[2] * (1 - .7 * v.velocity)) / 3);
            }
            v.noiseLevel = c[3] * float(std::exp(-v.age * 150)) * .14f;
            v.gain = 1 - .65f * c[5];
        }
        else if (family == 1)
        {
            v.partials = 12;
            for (int j = 0; j < 12; ++j)
            {
                int h = j / 2 + 1;
                mode(j,
                     hz * h *
                         (1 + (j % 2 ? 1 : -1) * (.0004 + .008 * c[0]) +
                          .002 * c[2] * std::sin(v.age * .8 + v.note)),
                     .25 / h);
            }
            v.filterCoefficient =
                float(1 - std::exp(-tau * (500 + 9000 * (1 - c[1]) * (.3 + .7 * v.velocity)) / sr));
            v.drive = 1 + 1.5f * c[1];
        }
        else if (family == 2)
        {
            v.partials = 6;
            constexpr double ratios[]{1, 2.71, 4.13, 5.43, 6.79, 8.21};
            for (int j = 0; j < 6; ++j)
                mode(j, hz * ratios[j] * (1 + .006 * c[3] * std::sin(v.age * (.8 + j * .11) + j)),
                     std::exp(-v.age * (1 + j * (1 - c[2])) / (.4 + 12 * c[4])) *
                         std::pow(.25 + .65 * c[1], j) * .5);
            v.gain = float(1 - std::exp(-v.age / (.001 + .035 * (1 - c[0])))) * (1 - .65f * c[5]);
        }
        else if (family == 3)
        {
            v.partials = 12;
            double f1 = 350 + 650 * c[1], f2 = 1100 + 1700 * c[1];
            for (int j = 0; j < 12; ++j)
            {
                int h = j / 2 + 1;
                double f = hz * h * (1 + (j % 2 ? 1 : -1) * c[2] * .006);
                double a =
                    std::exp(-std::pow((f - f1) / 280, 2)) + .7 * std::exp(-std::pow((f - f2) / 500, 2));
                mode(j, f, (a + .08) / h * .25);
            }
            v.noiseLevel = c[0] * .045f;
            v.filterCoefficient = float(1 - std::exp(-tau * (800 + 7000 * (1 - c[4])) / sr));
        }
        else if (family == 4)
        {
            v.partials = 4;
            constexpr double ratios[]{1, 2.756, 5.404, 8.933};
            double drift = 1 + c[1] * .002 * std::sin(v.age * 4 + v.note);
            for (int j = 0; j < 4; ++j)
                mode(j, hz * ratios[j] * drift * (1 + c[0] * .003 * std::sin(v.note)),
                     std::exp(-v.age * (1 + j * 1.3) / (.12 + 4 * c[4])) * std::pow(.2 + .7 * c[3], j) * .6);
            v.noiseLevel = c[2] * .25f * float(std::exp(-v.age * 250));
            v.gain = 1 - .65f * c[5];
        }
        else
        {
            v.partials = 8;
            for (int j = 0; j < 8; ++j)
            {
                double ratio = j == 0 ? .5 : double(j);
                mode(j, hz * ratio * (1 + c[1] * .007 * std::sin(v.age * (.2 + j * .04) + j)),
                     (j == 0 ? c[3] : (.2 + c[2]) / (1 + j)) * .35);
            }
            v.filterCoefficient = float(
                1 - std::exp(-tau * (250 + 2500 * c[2] * (.7 + .3 * std::sin(v.age * (.1 + c[4])))) / sr));
            v.gain = .5f + .5f * c[0];
        }
    }
    v.envelope += v.envelopeCoefficient * ((sounding ? 1.f : 0.f) - v.envelope);
    if (!sounding && v.envelope < .00001f)
    {
        v = Voice{};
        return 0;
    }
    float result = 0;
    for (int j = 0; j < v.partials; ++j)
    {
        v.amplitudes[j] += .03125f * (v.amplitudeTargets[j] - v.amplitudes[j]);
        result += float(wave(v.phase[j], v.frequencies[j], sr)) * v.amplitudes[j];
    }
    if (v.noiseLevel > 0)
        result += noise() * v.noiseLevel;
    v.filter += v.filterCoefficient * (result - v.filter);
    result = v.filter;
    if (v.drive > 0)
        result = std::tanh(result * v.drive) * .6f;
    return result * v.gain * v.envelope * v.velocity * .18f;
}
Stereo Instruments::tick() noexcept
{
    for (int ch = 0; ch < 16; ++ch)
        bendCurrent[ch] += smoothing * (bendTarget[ch] - bendCurrent[ch]);
    Stereo out;
    for (int i = 0; i < 6; ++i)
    {
        auto &f = families[i];
        f.on += smoothing * (target[first[i]] - f.on);
        f.level += smoothing * (target[first[i] + 1] - f.level);
        for (int j = 0; j < 6; ++j)
            f.controls[j] += smoothing * (target[first[i] + 2 + j] - f.controls[j]);
        auto add = [&](Voice &v)
        {
            if (v.note < 0)
                return;
            float s = render(v, i, f.controls) * f.level * f.on;
            float width = i == 1 ? f.controls[5] : (i == 3 ? f.controls[3] : (i == 5 ? f.controls[4] : .35f));
            float pan = std::sin(float(v.note) * 1.7f + float(v.age) * .12f) * width * .5f;
            out.l += s * (1 - pan) * .7071f;
            out.r += s * (1 + pan) * .7071f;
        };
        for (auto &v : f.voices)
            add(v);
        for (auto &v : f.tails)
            add(v);
    }
    return out;
}
} // namespace liminal
