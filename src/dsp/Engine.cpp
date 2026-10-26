#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <numeric>
namespace liminal
{
namespace
{
constexpr double pi = 3.14159265358979323846, tau = 2 * pi;
float lp(float input, float &state, float coefficient)
{
    state += coefficient * (input - state);
    return state;
}
void pan(Stereo &out, float v, float position, float width = 0, float side = 0)
{
    auto angle = (std::clamp(position, -1.f, 1.f) + 1) * float(pi * .25);
    out.l += v * std::cos(angle) + side * width * .35f;
    out.r += v * std::sin(angle) - side * width * .35f;
}
} // namespace
float Engine::Delay::read(double delay) const noexcept
{
    double pos = double(write) - std::clamp(delay, 1., double(data.size() - 2));
    if (pos < 0)
        pos += double(data.size());
    auto a = std::size_t(pos);
    auto b = (a + 1) % data.size();
    float f = float(pos - double(a));
    return data[a] + f * (data[b] - data[a]);
}
void Engine::Delay::push(float x) noexcept
{
    data[write] = x;
    if (++write == data.size())
        write = 0;
}
void Engine::Delay::clear()
{
    std::fill(data.begin(), data.end(), 0);
    write = 0;
}
float Engine::random() noexcept
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return float(rng >> 8) * (1.f / 16777216.f);
}
double Engine::osc(int i, double hz) noexcept
{
    phases[std::size_t(i)] += std::clamp(hz, 0., sr * .4) / sr;
    phases[std::size_t(i)] -= std::floor(phases[std::size_t(i)]);
    return std::sin(tau * phases[std::size_t(i)]);
}
void Engine::prepare(double rate, const Settings &initial)
{
    sr = std::clamp(rate, 8000., 192000.);
    target = p = sanitise(initial);
    smooth = float(1 - std::exp(-1 / (sr * .035)));
    for (auto &d : late)
        d.data.resize(std::size_t(sr * .5) + 8);
    earlyL.data.resize(std::size_t(sr * 2) + 8);
    earlyR.data.resize(std::size_t(sr * 2) + 8);
    reset(std::uint32_t(p[seed]));
    coefficients();
}
void Engine::reset(std::uint32_t s)
{
    transportGain = 1;
    rng = lastSeed = std::max(1u, s);
    sampleCounter = voiceAge = structureCount = distantCount = 0;
    voices = {};
    modes = {};
    phases = {};
    colour = {};
    outLP = {};
    hpPrevIn = {};
    hpPrevOut = {};
    distanceLP = {};
    damped = {};
    evolutionClock = 0;
    evolution = evolutionTarget = 0;
    structureWait = .2;
    distantWait = .7;
    flickerWait = .4;
    flickerGain = flickerTarget = 1;
    distantEnv = 0;
    distantPhase = 0;
    for (auto &d : late)
        d.clear();
    earlyL.clear();
    earlyR.clear();
}
void Engine::setSettings(const Settings &s)
{
    target = sanitise(s);
    if (std::uint32_t(target[seed]) != lastSeed)
        reset(std::uint32_t(target[seed]));
}
void Engine::setTransport(bool isPlaying, double tempo) noexcept
{
    playing = isPlaying;
    if (sampleCounter == 0)
        transportGain = (target[follow] > .5f && !playing) ? 0.f : 1.f;
    bpm = std::isfinite(tempo) ? std::clamp(tempo, 20., 400.) : 120.;
}
void Engine::noteOn(int note, float vel) noexcept
{
    if (vel <= 0)
    {
        noteOff(note);
        return;
    }
    auto it = std::find_if(voices.begin(), voices.end(), [](const Voice &v) { return v.note < 0; });
    if (it == voices.end())
        it = std::min_element(voices.begin(), voices.end(),
                              [](const Voice &a, const Voice &b) { return a.env < b.env; });
    // Keep amplitude and phase on steals to avoid an instantaneous discontinuity.
    it->note = std::clamp(note, 0, 127);
    it->held = true;
    it->velocity = std::clamp(vel, 0.f, 1.f);
    it->age = ++voiceAge;
}
void Engine::noteOff(int note) noexcept
{
    for (auto &v : voices)
        if (v.note == note)
            v.held = false;
}
void Engine::allNotesOff(bool immediately) noexcept
{
    for (auto &v : voices)
    {
        v.held = false;
        if (immediately)
        {
            v.env = 0;
            v.note = -1;
        }
    }
}
void Engine::coefficients() noexcept
{
    auto coefficient = [this](double hz)
    { return float(1 - std::exp(-tau * std::clamp(hz, 1., sr * .45) / sr)); };
    lpAir = coefficient(180 + 1800 * p[airflow] + 500 * p[ventMovement] * evolution);
    lpDark = coefficient(100 + 3500 * (1 - p[darkness]));
    lpMuffle = coefficient(100 + 1400 * (1 - p[muffle]));
    lpOutput = coefficient(p[highCut]);
    hpOutput = float(std::exp(-tau * p[lowCut] / sr));
    double dx = (p[sourceX] - p[listenerX]) * p[roomWidth], dy = (p[sourceY] - p[listenerY]) * p[roomLength];
    double geometric = std::sqrt(dx * dx + dy * dy);
    double dist = .5 * (geometric + p[sourceDistance]);
    lpDistance = coefficient(16000 / (1 + dist * .12));
    // Approximate material absorption; not a wave or ray-tracing simulation.
    constexpr float absorption[]{.08f, .04f, .035f, .06f, .5f, .22f, .16f};
    constexpr float darknessByMaterial[]{.23f, .12f, .08f, .1f, .85f, .65f, .45f};
    int m = std::clamp(int(target[material]), 0, 6);
    double w = p[roomWidth], l = p[roomLength], h = p[roomHeight], size = .5 + 2 * p[scale];
    rt60 = std::clamp(.161 * w * l * h / (2 * (w * l + w * h + l * h) * absorption[m]) * size, .25, 12.);
    damping = coefficient(13000 * (1 - darknessByMaterial[m]) / (1 + rt60 * .12));
    const double paths[]{
        2 * w, 2 * l, 2 * h, 2 * std::hypot(w, l), 2 * std::hypot(w, h), 2 * std::hypot(l, h)};
    for (std::size_t i = 0; i < 6; ++i)
        erTimes[i] = std::clamp((paths[i] + dist) * size / 343. * sr, 1., sr * 1.95);
    constexpr double lengths[]{.0297, .0371, .0411, .0437, .0531, .0617, .0719, .0797};
    for (std::size_t i = 0; i < 8; ++i)
    {
        lateTimes[i] = sr * lengths[i] * std::clamp((w + l + h) / 55. * size, .5, 4.);
        feedback[i] = float(std::pow(.001, lateTimes[i] / sr / rt60));
    }
}
Stereo Engine::room(Stereo in) noexcept
{
    float left = lp(in.l, distanceLP[0], lpDistance), right = lp(in.r, distanceLP[1], lpDistance);
    earlyL.push(left);
    earlyR.push(right);
    Stereo er;
    for (std::size_t i = 0; i < 6; ++i)
    {
        er.l += earlyL.read(erTimes[i] * (1 + .015 * p[unreal] * evolution)) * (.12f / (1 + float(i) * .25f));
        er.r += earlyR.read(erTimes[(i + 2) % 6] * (1 - .015 * p[unreal] * evolution)) *
                (.12f / (1 + float(i) * .25f));
    }
    std::array<float, 8> taps{};
    float sum = 0;
    for (std::size_t i = 0; i < 8; ++i)
    {
        taps[i] = lp(late[i].read(lateTimes[i]), damped[i], damping);
        sum += taps[i];
    }
    for (std::size_t i = 0; i < 8; ++i)
        late[i].push(((i % 2 ? right : left) + .3f * (er.l + er.r)) * .16f +
                     (.25f * sum - taps[i]) * feedback[i]);
    float wetL = taps[0] - taps[2] + taps[4] - taps[6], wetR = taps[1] - taps[3] + taps[5] - taps[7];
    float distanceGain = 1 / (1 + p[sourceDistance] * .06f);
    float mix = p[reverb];
    float directMid = (left + right) * .5f;
    float directSide = (left - right) * .5f / (1 + p[sourceDistance] * .04f);
    float balance = std::clamp((p[sourceX] - p[listenerX]) * .8f, -.6f, .6f);
    return {(directMid + directSide) * (1 - balance) * distanceGain * (1 - .7f * mix) +
                mix * (er.l + wetL * .6f),
            (directMid - directSide) * (1 + balance) * distanceGain * (1 - .7f * mix) +
                mix * (er.r + wetR * .6f)};
}
Stereo Engine::tick(Stereo instrument) noexcept
{
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!specs[i].discrete)
            p[i] += smooth * (target[i] - p[i]);
        else if (i != active && i != mute && i != mode && i != electricalOn && i != ventilationOn &&
                 i != structureOn && i != atmosphereOn && i != distantOn && i != subOn)
            p[i] = target[i];
        else
            p[i] += smooth * (target[i] - p[i]);
        if (std::abs(target[i] - p[i]) < .0001f * (1 + std::abs(target[i])))
            p[i] = target[i];
    }
    if ((sampleCounter++ & 31u) == 0)
        coefficients();
    double dt = 1 / sr;
    float n = noise(), n2 = noise();
    if (target[freeze] < .5f)
    {
        evolutionClock -= dt;
        if (evolutionClock <= 0)
        {
            evolutionTarget = noise();
            evolutionClock = 1 + (1 - p[evolutionRate]) * 25;
        }
        evolution += float(dt / (.2 + (1 - p[evolutionRate]) * 5)) * (evolutionTarget - evolution);
        structureWait -= dt;
        distantWait -= dt;
        flickerWait -= dt;
        if (flickerWait <= 0)
        {
            flickerTarget = random() < .4f ? 1 - p[flicker] * (.2f + .8f * random()) : 1;
            flickerWait = .03 + random() * (2.8 - 2 * p[flicker]);
        }
        if (structureWait <= 0)
        {
            float rate = p[structureActivity] * p[activity] * p[eventDensity];
            structureWait = .3 + (-std::log(std::max(.0001f, random()))) / (0.02 + rate * 1.8);
            if (rate > .0001f)
            {
                ++structureCount;
                for (std::size_t i = 0; i < modes.size(); ++i)
                {
                    auto &m = modes[i];
                    m.freq = (60 + 450 * p[structureMaterial]) * (1 + double(i) * (1.45 + p[unreal] * .31)) *
                             (1 - .6 * p[weight]) * (1 + p[randomness] * noise() * .15);
                    m.amplitude = (.1f + .2f * random()) / (1 + float(i));
                    m.phase = 0;
                }
            }
        }
        if (distantWait <= 0)
        {
            float rate = p[distantActivity] * p[eventRate] * p[activity] * p[eventDensity];
            distantWait = 1 + (-std::log(std::max(.0001f, random()))) / (.008 + rate * .9);
            if (rate > .0001f)
            {
                ++distantCount;
                distantEnv = .25f;
                distantFrequency = 45 + random() * 140 * (.5 + p[variation]);
            }
        }
    }
    flickerGain += float(dt / .012) * (flickerTarget - flickerGain);
    float evo = evolution * p[evolutionDepth], un = p[unreal] * p[unreal];
    double drift = 1 + (evo * .004 + p[instability] * .002) * un;
    double humHz = target[mains] > .5 ? 60 : 50;
    double hum = osc(0, humHz * drift), hum2 = std::sin(tau * phases[0] * 2),
           hum3 = std::sin(tau * phases[0] * 3);
    float tonal = 0, gate = 0;
    for (auto &v : voices)
        if (v.note >= 0)
        {
            float dest = v.held ? 1.f : 0.f;
            float c = float(1 - std::exp(-dt / (v.held ? p[attack] : p[release])));
            v.env += c * (dest - v.env);
            if (!v.held && v.env < .00001f)
            {
                v.note = -1;
                continue;
            }
            double hz = 440 * std::pow(2., (v.note - 69) / 12.);
            v.phase += hz * drift / sr;
            v.phase -= std::floor(v.phase);
            tonal += v.env * (1 - p[velocity] + p[velocity] * v.velocity) *
                     float(std::sin(tau * v.phase) +
                           (hz * 2 < sr * .45 ? p[harmonics] * .22 * std::sin(tau * v.phase * 2) : 0));
            gate = std::max(gate, v.env);
        }
    float instrumentMix = p[mode];
    tonal = tonal * .2f;
    float pitched = float(hum + .3 * p[harmonics] * hum2 + .17 * p[harmonics] * hum3) * (1 - instrumentMix) +
                    tonal * instrumentMix;
    float noiseGain = (1 - instrumentMix) + instrumentMix * ((1 - p[noiseGate]) + p[noiseGate] * gate);
    Stereo dry;
    float crack = std::max(0.f, 1 - flickerGain) * lp(n, colour[5], .2f) * .15f;
    float electrical =
        (pitched * .2f + lp(n, colour[0], .35f) * p[buzz] * .12f * noiseGain + crack * noiseGain) *
        flickerGain;
    pan(dry, electrical * p[electricalLevel] * p[electricalOn], p[electricalPan]);
    double fan = osc(1, p[fanSpeed] * (1 + .04 * p[ventMovement] * evo));
    float air = lp(n, colour[1], lpAir);
    float low = lp(n2, colour[2], .004f);
    float vent = (air * p[airflow] * (.7f + .3f * float(fan)) +
                  float(osc(2, p[fanSpeed] * 12)) * p[motor] * .09f * (1 - instrumentMix) +
                  tonal * p[motor] * .3f * instrumentMix + low * p[rumble] * .7f);
    vent *= .45f * (1 + p[ventResonance] * float(fan) * .4f);
    pan(dry, vent * p[ventilationLevel] * p[ventilationOn] * noiseGain, p[ventilationPan], p[ventWidth],
        air * .2f * p[ventilationLevel] * p[ventilationOn] * noiseGain);
    float structural = 0;
    for (auto &m : modes)
    {
        m.phase += std::min(m.freq, sr * .4) / sr;
        m.phase -= std::floor(m.phase);
        m.amplitude *= float(std::exp(-dt / (p[decay] * (.2 + .8 * p[structureResonance]))));
        structural += m.amplitude * float(std::sin(tau * m.phase));
    }
    pan(dry, structural * p[structureLevel] * p[structureOn] * noiseGain / (1 + 4 * p[structureDistance]),
        p[structurePan]);
    float bed = lp(n2, colour[3], lpDark) * (.2f + .6f * p[density]) * (1 + p[movement] * evo * .4f);
    bed += float(osc(3, 30 + 18 * p[texture] + un * evo * 2)) * p[subEnergy] * .06f;
    bed += float(osc(4, 90 + 47 * p[texture] + un * evo * 7)) * p[texture] * .025f * (1 - instrumentMix) +
           tonal * p[texture] * .05f * instrumentMix;
    pan(dry, bed * .6f * p[atmosphereLevel] * p[atmosphereOn] * noiseGain, p[atmospherePan] + un * evo * .2f,
        p[spread], air * .18f * p[atmosphereLevel] * p[atmosphereOn] * noiseGain);
    distantPhase += distantFrequency / sr;
    distantPhase -= std::floor(distantPhase);
    distantEnv *= float(std::exp(-dt / 1.2));
    float distantSignal =
        lp(distantEnv * (float(std::sin(tau * distantPhase)) + .3f * n), colour[4], lpMuffle);
    pan(dry, distantSignal * p[distantLevel] * p[distantOn] * noiseGain / (1 + 5 * p[distance]),
        p[distantPan]);
    double pulseHz = target[tempoSync] > .5 ? bpm / 60. / p[beatDivision] : .03 + .2 * p[subMod];
    float pulseOsc = float(osc(5, pulseHz));
    float sub = float(osc(6, p[subFrequency] * (1 + evo * p[subMod] * .015)) +
                      .15 * p[subHarmonics] * std::sin(tau * phases[6] * 2));
    sub = (sub * (1 - instrumentMix) + tonal * instrumentMix) * (.75f + .25f * p[pulse] * pulseOsc) *
          p[depth] * .2f;
    pan(dry, sub * p[subLevel] * p[subOn], p[subPan]);
    // Smooth the transport gate as well as the activation switch.
    float transportTarget = (target[follow] > .5f && !playing) ? 0.f : 1.f;
    transportGain += smooth * (transportTarget - transportGain);
    float inputGain = p[active] * transportGain * (.3f + 1.4f * p[presence]);
    auto busGain = [](float db) { return db <= -59.9f ? 0.f : std::pow(10.f, db / 20.f); };
    float ambienceGain = busGain(p[ambienceBusLevel]);
    float musicalGain = 6.f * busGain(p[instrumentBusLevel]) * p[active] * transportGain;
    dry.l *= inputGain * ambienceGain;
    dry.r *= inputGain * ambienceGain;
    instrument.l *= musicalGain;
    instrument.r *= musicalGain;
    // Preserve a foreground musical direct path. Architecture processes the send,
    // rather than attenuating the entire instrument through the source-distance model.
    auto out = room({dry.l + instrument.l * .25f, dry.r + instrument.r * .25f});
    out.l += instrument.l * .75f;
    out.r += instrument.r * .75f;
    float mid = (out.l + out.r) * .5f, side = (out.l - out.r) * .5f * p[stereoWidth];
    out = {mid + side, mid - side};
    float gain = std::pow(10.f, p[master] / 20.f) * (1 - p[mute]);
    float *channels[]{&out.l, &out.r};
    for (int c = 0; c < 2; ++c)
    {
        float x = lp(*channels[c], outLP[std::size_t(c)], lpOutput);
        float y = x - hpPrevIn[std::size_t(c)] + hpOutput * hpPrevOut[std::size_t(c)];
        hpPrevIn[std::size_t(c)] = x;
        hpPrevOut[std::size_t(c)] = y;
        // Bounded soft protection. Normal gain staging stays well below this knee.
        *channels[c] = .89f * std::tanh(y * gain / .89f);
        if (std::abs(hpPrevOut[std::size_t(c)]) < 1e-25f)
            hpPrevOut[std::size_t(c)] = 0;
    }
    return out;
}
} // namespace liminal
