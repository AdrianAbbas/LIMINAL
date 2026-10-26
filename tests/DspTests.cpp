#include "dsp/Engine.h"
#include "presets/Factory.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace liminal;
int checks = 0;
void check(bool ok, const std::string &name)
{
    ++checks;
    if (!ok)
        throw std::runtime_error(name);
}
double render(Engine &e, int n, float *maximum = nullptr)
{
    double energy = 0;
    float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        auto s = e.tick();
        if (!std::isfinite(s.l) || !std::isfinite(s.r) || std::abs(s.l) > 1.001f || std::abs(s.r) > 1.001f)
            throw std::runtime_error("nonfinite/unbounded output");
        energy += double(s.l) * s.l + double(s.r) * s.r;
        peak = std::max({peak, std::abs(s.l), std::abs(s.r)});
    }
    if (maximum)
        *maximum = peak;
    return energy / (2 * n);
}
void wav(const std::string &path, const std::vector<float> &audio, int sr)
{
    std::ofstream f(path, std::ios::binary);
    auto u16 = [&](unsigned v)
    {
        f.put(char(v));
        f.put(char(v >> 8));
    };
    auto u32 = [&](unsigned v)
    {
        u16(v & 65535);
        u16(v >> 16);
    };
    f.write("RIFF", 4);
    u32(36 + unsigned(audio.size() * 2));
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(2);
    u32(unsigned(sr));
    u32(unsigned(sr * 4));
    u16(4);
    u16(16);
    f.write("data", 4);
    u32(unsigned(audio.size() * 2));
    for (float v : audio)
        u16(unsigned(int(std::clamp(v, -1.f, 1.f) * 32767)) & 65535);
}
int main(int argc, char **argv)
{
    try
    {
        auto start = std::chrono::steady_clock::now();
        auto invalid = defaults();
        invalid[presence] = std::numeric_limits<float>::quiet_NaN();
        invalid[roomWidth] = 999;
        auto valid = sanitise(invalid);
        check(valid[presence] == specs[presence].initial && valid[roomWidth] == 100,
              "parameter sanitisation");
        for (double sr : {44100., 48000., 88200., 96000.})
            for (int block : {64, 128, 256, 512, 1024})
            {
                Engine e;
                e.prepare(sr);
                double energy = 0;
                int n = int(sr);
                for (int pos = 0; pos < n; pos += block)
                    energy += render(e, std::min(block, n - pos));
                check(energy > 1e-8, "sample rate / block output");
            }
        Engine a, b;
        a.prepare(48000);
        b.prepare(48000);
        bool equal = true;
        for (int i = 0; i < 96000; ++i)
        {
            auto x = a.tick(), y = b.tick();
            equal &= x.l == y.l && x.r == y.r;
        }
        check(equal, "seed repeatability");
        auto p = defaults();
        p[seed] = 321;
        b.prepare(48000, p);
        check(render(b, 48000) > 0, "alternate seed");
        constexpr Param enables[]{electricalOn, ventilationOn, structureOn, atmosphereOn, distantOn, subOn};
        for (auto layer : enables)
        {
            p = defaults();
            for (auto id : enables)
                p[id] = id == layer ? 1.f : 0.f;
            p[activity] = p[eventDensity] = p[structureActivity] = p[distantActivity] = p[eventRate] = 1;
            p[reverb] = 0;
            a.prepare(48000, p);
            check(render(a, 48000 * 4) > 1e-10, "isolated layer " + std::string(specs[layer].id));
        }
        p = defaults();
        for (auto id : enables)
            p[id] = 0;
        a.prepare(48000, p);
        check(render(a, 48000) == 0, "all layers disabled silence");
        p = defaults();
        p[follow] = 1;
        a.prepare(48000, p);
        a.setTransport(false, 120);
        render(a, 48000);
        check(render(a, 48000) == 0, "transport gate settles");
        p[follow] = 0;
        a.prepare(48000, p);
        a.setTransport(false, 120);
        check(render(a, 48000) > 1e-8, "independent transport");
        p = defaults();
        a.prepare(48000, p);
        p[roomLength] = 140;
        b.prepare(48000, p);
        double delta = 0;
        for (int i = 0; i < 96000; ++i)
        {
            auto x = a.tick(), y = b.tick();
            delta += std::abs(x.l - y.l) + std::abs(x.r - y.r);
        }
        check(delta > 1, "geometry changes rendered acoustics");
        p = defaults();
        a.prepare(48000, p);
        p[material] = 4;
        b.prepare(48000, p);
        delta = 0;
        for (int i = 0; i < 96000; ++i)
        {
            auto x = a.tick(), y = b.tick();
            delta += std::abs(x.l - y.l) + std::abs(x.r - y.r);
        }
        check(delta > 1, "material changes rendered acoustics");
        p = defaults();
        p[mute] = 1;
        a.prepare(48000, p);
        check(render(a, 48000) == 0, "muted silence");
        p = defaults();
        p[active] = 0;
        a.prepare(48000, p);
        check(render(a, 48000) == 0, "inactive silence");
        p = defaults();
        p[mode] = 1;
        a.prepare(48000, p);
        check(render(a, 48000) < 1e-14, "instrument gated silence");
        a.noteOn(48, .8f);
        check(render(a, 48000) > 1e-8, "MIDI note audio");
        a.noteOff(48);
        render(a, 48000 * 40);
        check(render(a, 48000) < 1e-10, "MIDI release");
        p = defaults();
        a.prepare(48000, p);
        render(a, 48000);
        p[mute] = 1;
        a.setSettings(p);
        render(a, 48000 * 2);
        check(render(a, 48000) < 1e-15, "mute smoothing settles");
        p = defaults();
        a.prepare(48000, p);
        auto near = a.reflectionSeconds(), hard = a.decaySeconds();
        p[roomWidth] = 90;
        p[roomLength] = 120;
        a.prepare(48000, p);
        check(a.reflectionSeconds() > near, "geometry reflection timing");
        p[material] = 4;
        a.prepare(48000, p);
        check(a.decaySeconds() < hard, "carpet absorption RT60");
        p = defaults();
        p[reverb] = 0;
        p[sourceDistance] = 1;
        a.prepare(48000, p);
        auto close = render(a, 96000);
        p[sourceDistance] = 80;
        b.prepare(48000, p);
        check(render(b, 96000) < close, "distance attenuation");
        p = defaults();
        p[activity] = p[structureActivity] = p[distantActivity] = p[eventRate] = p[eventDensity] = 1;
        a.prepare(48000, p);
        render(a, 48000 * 30);
        check(a.structureEvents() > 0 && a.distantEvents() > 0, "events triggered");
        auto events = a.structureEvents() + a.distantEvents();
        p[freeze] = 1;
        a.setSettings(p);
        render(a, 48000 * 5);
        check(events == a.structureEvents() + a.distantEvents(), "freeze stops events");
        check(events < 150, "event spacing bound");
        p = defaults();
        for (std::size_t i = 0; i < count; ++i)
            p[i] = specs[i].max;
        p[mute] = 0;
        p[mode] = 0;
        p[freeze] = 0;
        a.prepare(96000, p);
        float maxPeak = 0;
        render(a, 96000 * 15, &maxPeak);
        check(maxPeak <= 1, "extreme parameters bounded");
        a.prepare(48000);
        render(a, 48000 * 120);
        check(true, "two minute stability render");
        double last = -1;
        int distinct = 0, presetIndex = 0;
        for (const auto &preset : factoryPresets())
        {
            a.prepare(48000, preset.values);
            auto energy = render(a, 48000 * 2);
            check(energy > 1e-10, std::string("factory ") + preset.name);
            if (std::abs(energy - last) > 1e-9)
                ++distinct;
            // Frozen 0.1 reference energies, 48 kHz, first two seconds, stereo.
            // Allow 2% for platform floating-point differences; intentional DSP changes
            // require reviewing the resulting audio before updating these baselines.
            double reference =
                presetIndex == 0
                    ? 6.20047493653e-5
                    : (presetIndex == 9 ? 8.0524456745e-5 : (presetIndex == 14 ? 9.8280357815e-6 : 0));
            reference *= std::pow(10., preset.values[ambienceBusLevel] / 10.);
            if (reference > 0)
                check(std::abs(energy - reference) < reference * .02, "preset render energy regression");
            ++presetIndex;
            last = energy;
        }
        check(distinct == 15, "distinct preset energy signatures");
        // Exercise simultaneous automation, transitions and polyphonic voice stealing.
        a.prepare(48000);
        for (int step = 0; step < 60; ++step)
        {
            p = defaults();
            for (std::size_t i = 0; i < count; ++i)
                if (!specs[i].discrete)
                    p[i] = step % 2 ? specs[i].min : specs[i].max;
            p[mode] = float(step % 2);
            p[mute] = 0;
            a.setSettings(p);
            a.noteOn(36 + step % 48, .8f);
            render(a, 4096);
        }
        check(true, "automation and voice stealing stability");
        if (argc > 1)
        {
            for (int j : {0, 9, 14})
            {
                a.prepare(48000, factoryPresets()[std::size_t(j)].values);
                std::vector<float> audio;
                audio.reserve(48000 * 2 * 10);
                for (int i = 0; i < 48000 * 10; ++i)
                {
                    auto s = a.tick();
                    audio.push_back(s.l);
                    audio.push_back(s.r);
                }
                wav(std::string(argv[1]) + "/preset-" + std::to_string(j + 1) + ".wav", audio, 48000);
            }
        }
        a.prepare(48000);
        auto t = std::chrono::steady_clock::now();
        render(a, 48000 * 10);
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
        std::cout << checks << " DSP checks passed. 10 s offline render: " << seconds
                  << " s (single instance, this machine). Total "
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                  << " s\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
