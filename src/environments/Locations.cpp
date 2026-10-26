#include "Locations.h"
#include <algorithm>
#include <cmath>
namespace liminal
{
namespace
{
constexpr double tau = 6.283185307179586;
double osc(double &phase, double hz, double sr)
{
    phase += hz / sr;
    phase -= std::floor(phase);
    return std::sin(tau * phase);
}
float low(float x, float &state, float c)
{
    state += c * (x - state);
    return state;
}
} // namespace
float Locations::random(State &s) noexcept
{
    s.rng ^= s.rng << 13;
    s.rng ^= s.rng >> 17;
    s.rng ^= s.rng << 5;
    return float(s.rng >> 8) / 16777216.f;
}
void Locations::prepare(double rate, const Settings &p)
{
    sr = rate;
    smooth = float(1 - std::exp(-1 / (sr * .045)));
    delay.resize(std::size_t(sr * 2.1) + 8);
    reset(unsigned(p[seed]));
    int selected = int(p[environmentModel]);
    if (selected >= 0 && selected < 10)
        weights[selected] = 1;
}
void Locations::reset(unsigned seed)
{
    states = {};
    weights = {};
    for (int i = 0; i < 10; ++i)
    {
        auto &s = states[i];
        s.rng = std::max(1u, seed + unsigned(i) * 0x9e3779b9u);
        s.wait = .7 + i * .17;
        s.cycleWait = 17 + i * 1.3;
        s.eventLoss = float(std::exp(-1 / (sr * (.3 + i * .14))));
        s.airCoefficient = float(1 - std::exp(-tau * environments[i].airCutoff / sr));
        s.musicCoefficient = float(1 - std::exp(-tau * environments[i].colourHz / sr));
    }
    std::fill(delay.begin(), delay.end(), Stereo{});
    position = 0;
    clock = 0;
    samples = eventCount = 0;
}
Stereo Locations::read(double offset) const noexcept
{
    double p = double(position) - std::clamp(offset, 1., double(delay.size() - 2));
    if (p < 0)
        p += delay.size();
    auto a = std::size_t(p), b = (a + 1) % delay.size();
    float f = float(p - a);
    return {delay[a].l + f * (delay[b].l - delay[a].l), delay[a].r + f * (delay[b].r - delay[a].r)};
}
Stereo Locations::ambience(const Settings &p) noexcept
{
    Stereo out;
    int selected = int(p[environmentModel]);
    if (p[freeze] < .5f)
        clock += (.2 + 1.8 * p[evolutionRate]) / sr;
    ++samples;
    for (int i = 0; i < 10; ++i)
    {
        weights[i] += smooth * ((i == selected ? 1.f : 0.f) - weights[i]);
        if (weights[i] < 1e-6f)
            continue;
        auto &s = states[i];
        auto &profile = environments[i];
        float n = random(s) * 2 - 1, n2 = random(s) * 2 - 1;
        float airflow = low(n, s.low[0], s.airCoefficient) - low(n, s.low[1], s.airCoefficient * .09f);
        float diffuse = low(n2, s.low[2], s.airCoefficient * .7f);
        float intensity = p[activity] * p[eventDensity];
        if (p[freeze] < .5f)
        {
            s.wait -= 1 / sr;
            s.cycleWait -= 1 / sr;
            s.age += 1 / sr;
            if (s.wait <= 0)
            {
                s.wait =
                    .6 - std::log(std::max(.0001f, random(s))) / (.015 + intensity * (i == 4 ? .65 : .2));
                if (intensity > .0001f)
                {
                    s.event = .35f + .35f * random(s);
                    s.age = 0;
                    s.eventHz = 200 + 1000 * random(s);
                    s.pan = random(s) * 1.6f - .8f;
                    ++eventCount;
                }
            }
            if (s.cycleWait <= 0)
            {
                s.cycleWait = 17 + random(s) * 29;
                s.cycleTarget = s.cycleTarget > .5f ? 0 : 1;
            }
        }
        s.cycle += .00002f * (s.cycleTarget - s.cycle);
        s.event *= s.eventLoss;
        s.eventSmooth += .003f * (s.event - s.eventSmooth);
        float evolving = 1 + std::sin(clock * (.017 + .003 * i)) * p[evolutionDepth] * .2f;
        float motor = float(osc(
            s.phase[0],
            profile.motorHz * (.6 + .13 * p[fanSpeed]) * (1 + .002 * p[unreal] * std::sin(clock * .13)), sr));
        float electric = float(osc(s.phase[1], p[mains] > .5f ? 60 : 50, sr));
        float pressure = float(osc(s.phase[2], 23 + i * 2.1, sr));
        float event = 0;
        switch (i)
        {
        case 0:
            event = float(osc(s.phase[3], 880, sr)) * .65f + float(osc(s.phase[4], 1100, sr)) * .2f;
            event *= float(std::exp(-s.age * 4));
            break;
        case 1:
            event = float(osc(s.phase[3], s.eventHz * .18, sr)) +
                    .4f * float(osc(s.phase[4], s.eventHz * .47, sr));
            motor *= .7f + .3f * float(std::sin(clock * .39));
            break;
        case 2:
            event = float(osc(s.phase[3], s.eventHz * .6 * (1 + .008 * std::sin(s.age * 13)), sr)) +
                    .5f * float(osc(s.phase[4], s.eventHz * 1.41, sr));
            break;
        case 3:
            event = float(osc(s.phase[3], 42 + s.eventHz * .025, sr)) * .8f + airflow * .6f;
            pressure *= 2;
            break;
        case 4:
            event = float(osc(s.phase[3], 380 + 1800 / (1 + s.age * 95), sr)) +
                    .35f * float(osc(s.phase[4], 1170 + 2000 / (1 + s.age * 70), sr));
            motor *= .15f;
            airflow *= .35f;
            break;
        case 5:
            event = float(osc(s.phase[3], 60 + std::min(2., s.age) * 45, sr)) * .5f + diffuse * .8f;
            electric *= .2f;
            break;
        case 6:
            event = float(osc(s.phase[3], 330 + s.eventHz * .4 + 25 * std::sin(s.age * .8), sr)) +
                    .5f * float(osc(s.phase[4], s.eventHz * 1.93, sr));
            pressure *= 1.5f;
            break;
        case 7:
            event = airflow * .8f + float(osc(s.phase[3], 360, sr)) * .3f;
            motor *= .15f + .85f * s.cycle;
            electric *= .5f + .5f * s.cycle;
            break;
        case 8:
            event = float(osc(s.phase[3], 1700 + s.eventHz, sr)) * .5f + diffuse * .3f;
            motor *= .65f + .35f * float(std::sin(clock * .23));
            break;
        default:
            event = float(osc(s.phase[3], s.eventHz * (1 + .08 * std::sin(clock * .17)), sr)) +
                    .7f * float(osc(s.phase[4], s.eventHz * 1.4142, sr));
            pressure *= 1 + float(std::sin(clock * .07));
            break;
        }
        float steady = (airflow * p[ventilationLevel] * p[ventilationOn] * .28f +
                        diffuse * p[atmosphereLevel] * p[atmosphereOn] * .16f +
                        motor * p[motor] * p[ventilationLevel] * p[ventilationOn] * .045f +
                        electric * p[buzz] * p[electricalLevel] * p[electricalOn] * .04f +
                        pressure * p[subLevel] * p[subOn] * .025f) *
                       evolving;
        float foreground = event * s.eventSmooth *
                           (p[structureLevel] * p[structureOn] + p[distantLevel] * p[distantOn]) * .11f;
        out.l += weights[i] * (steady + foreground * (1 - s.pan) * .707f);
        out.r += weights[i] * (steady + foreground * (1 + s.pan) * .707f +
                               diffuse * p[atmosphereLevel] * p[atmosphereOn] * .03f);
    }
    return out;
}
Stereo Locations::music(Stereo input, const Settings &p) noexcept
{
    Stereo output;
    float sum = 0;
    Stereo feedback;
    for (int i = 0; i < 10; ++i)
    {
        if (weights[i] < 1e-6f)
            continue;
        auto &s = states[i];
        auto &e = environments[i];
        sum += weights[i];
        double flutterSamples =
            sr * (.009 + e.flutter * (.002 + .006 * p[unreal]) * std::sin(clock * (.7 + i * .07)));
        auto warped = read(flutterSamples);
        auto echo = read(sr * e.echoSeconds * (.7 + p[scale]));
        float warp = e.flutter * (.15f + .55f * p[unreal]);
        Stereo coloured{low(input.l * (1 - warp) + warped.l * warp, s.musicalLow[0], s.musicCoefficient),
                        low(input.r * (1 - warp) + warped.r * warp, s.musicalLow[1], s.musicCoefficient)};
        output.l += weights[i] * (coloured.l + echo.r * e.echoAmount) * e.musicGain;
        output.r += weights[i] * (coloured.r + echo.l * e.echoAmount) * e.musicGain;
        feedback.l += weights[i] * echo.r * .23f;
        feedback.r += weights[i] * echo.l * .23f;
    }
    delay[position] = {input.l + feedback.l, input.r + feedback.r};
    if (++position == delay.size())
        position = 0;
    float influence = p[environmentInfluence];
    sum = std::clamp(sum, 0.f, 1.f);
    return {input.l * (1 - influence * sum) + output.l * influence,
            input.r * (1 - influence * sum) + output.r * influence};
}
} // namespace liminal
