#include "dsp/Instruments.h"
#include "environments/Profiles.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace liminal;
void check(bool b, const char *text)
{
    if (!b)
        throw std::runtime_error(text);
}
struct Analysis
{
    double sum = 0, low = 0, mid = 0, high = 0;
    float peak = 0, lp1 = 0, lp2 = 0;
    int crossings = 0, frames = 0;
    float last = 0;
    void add(Stereo x)
    {
        check(std::isfinite(x.l) && std::isfinite(x.r), "non-finite environment");
        float mono = (x.l + x.r) * .5f;
        lp1 += .01558f * (mono - lp1);
        lp2 += .23033f * (mono - lp2);
        low += double(lp1) * lp1;
        mid += double(lp2 - lp1) * (lp2 - lp1);
        high += double(mono - lp2) * (mono - lp2);
        sum += double(x.l) * x.l + double(x.r) * x.r;
        peak = std::max({peak, std::abs(x.l), std::abs(x.r)});
        crossings += (mono >= 0) != (last >= 0);
        last = mono;
        ++frames;
    }
    double rms() const { return std::sqrt(sum / (2 * frames)); }
};
void saveWav(const std::string &path, const std::vector<Stereo> &audio, float gain)
{
    std::ofstream f(path, std::ios::binary);
    auto u16 = [&](unsigned n)
    {
        f.put(char(n));
        f.put(char(n >> 8));
    };
    auto u32 = [&](unsigned n)
    {
        u16(n & 65535);
        u16(n >> 16);
    };
    f.write("RIFF", 4);
    u32(36 + unsigned(audio.size() * 4));
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(2);
    u32(48000);
    u32(192000);
    u16(4);
    u16(16);
    f.write("data", 4);
    u32(unsigned(audio.size() * 4));
    for (auto s : audio)
        for (float x : {s.l, s.r})
            u16(unsigned(int(std::clamp(x * gain, -1.f, 1.f) * 32767)) & 65535);
    check(bool(f), "WAV write failed");
}
int main(int argc, char **argv)
{
    try
    {
        bool exportAudio = argc > 1;
        std::ofstream csv, index;
        if (exportAudio)
        {
            std::filesystem::create_directories(argv[1]);
            csv.open(std::string(argv[1]) + "/environment-comparison.csv");
            index.open(std::string(argv[1]) + "/INDEX.md");
            index << "# Ten environment comparison renders\n\n30 seconds, stereo 48 kHz PCM16. Seed 4242, "
                     "master -9 dB.\nEach file is offline RMS-matched toward -28 dBFS, capped at -1 dBFS "
                     "peak; live mixer is unchanged. CSV records the exact matching gain and achieved "
                     "level.\nThe ambience-only and fixed Forgotten Piano phrase use the same live DSP, not "
                     "a separate renderer.\nMetrics support technical comparison; emotional/timbral "
                     "distinction requires listening.\n\n";
            csv << "environment,mode,raw_rms,raw_peak,events,low_energy,mid_energy,high_energy,zero_"
                   "crossings,export_gain,export_rms\n";
        }
        std::array<Analysis, 10> signatures{};
        for (int location = 0; location < 10; ++location)
            for (int musical = 0; musical < 2; ++musical)
            {
                auto p = environmentSettings(location);
                p[seed] = 4242;
                Engine e;
                e.prepare(48000, p);
                Instruments bank;
                bank.prepare(48000, p);
                Analysis meter;
                std::vector<Stereo> audio;
                if (exportAudio)
                    audio.reserve(1440000);
                for (int n = 0; n < 1440000; ++n)
                {
                    if (musical)
                    {
                        int step = n % 192000;
                        for (int j = 0; j < 3; ++j)
                        {
                            if (step == j * 14400)
                                bank.noteOn(1, 48 + j * 7, .8f);
                            if (step == 144000)
                                bank.noteOff(1, 48 + j * 7);
                        }
                    }
                    auto s = e.tick(musical ? bank.tick() : Stereo{});
                    meter.add(s);
                    if (exportAudio)
                        audio.push_back(s);
                }
                check(meter.rms() > 1e-6, "silent location");
                check(meter.peak < .89f, "environment exceeded output ceiling");
                check(e.locationEvents() > 0, "no location events executed");
                if (!musical)
                    signatures[location] = meter;
                float gain = float(std::min(.0398107 / meter.rms(), .89 / double(meter.peak)));
                if (exportAudio)
                {
                    std::string name = "location-" + std::to_string(location + 1) +
                                       (musical ? "-piano.wav" : "-ambience.wav");
                    saveWav(std::string(argv[1]) + "/" + name, audio, gain);
                    index << "- " << name << ": " << environments[location].name << ". "
                          << environments[location].description << "\n";
                    csv << environments[location].name << ',' << musical << ',' << meter.rms() << ','
                        << meter.peak << ',' << e.locationEvents() << ',' << meter.low / meter.frames << ','
                        << meter.mid / meter.frames << ',' << meter.high / meter.frames << ','
                        << meter.crossings << ',' << gain << ',' << meter.rms() * gain << '\n';
                    csv.flush();
                }
                std::cout << environments[location].name << (musical ? " piano" : " ambience") << " RMS "
                          << meter.rms() << " events " << e.locationEvents() << '\n';
            }
        // These check algorithmic effects, not whether environments feel emotionally distinct.
        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < i; ++j)
                check(std::abs(signatures[i].mid - signatures[j].mid) > 1e-8 ||
                          signatures[i].crossings != signatures[j].crossings,
                      "identical rendered location signals");
        for (int location = 0; location < 10; ++location)
        {
            auto quiet = environmentSettings(location);
            for (auto id : {electricalOn, ventilationOn, structureOn, atmosphereOn, distantOn, subOn})
                quiet[id] = 0;
            Engine silent;
            silent.prepare(48000, quiet);
            for (int n = 0; n < 96000; ++n)
            {
                auto v = silent.tick();
                check(v.l == 0 && v.r == 0, "disabled layers leak location audio");
            }
            Locations effect;
            effect.prepare(48000, quiet);
            quiet[environmentInfluence] = 0;
            for (int n = 0; n < 48000; ++n)
            {
                effect.ambience(quiet);
                Stereo input{float(std::sin(n * .03)) * .1f, float(std::sin(n * .04)) * .1f};
                auto out = effect.music(input, quiet);
                check(out.l == input.l && out.r == input.r,
                      "environment influence zero does not bypass musical coloration");
            }
        }
        for (int location = 0; location < 10; ++location)
        {
            auto preset = recommendedEnvironmentSettings(location);
            int enabled = 0;
            for (auto id : {pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn})
                enabled += preset[id] > .5f;
            check(enabled == 2, "recommended instrument collection missing layers");
            check(preset == sanitise(preset), "recommended preset outside parameter limits");
        }
        std::ofstream layerReport;
        if (exportAudio)
        {
            layerReport.open(std::string(argv[1]) + "/layering.csv");
            layerReport << "environment,peak_dbfs,rms_dbfs\n";
        }
        for (int location = 0; location < 10; ++location)
        {
            auto preset = recommendedEnvironmentSettings(location);
            Engine e;
            e.prepare(48000, preset);
            Instruments bank;
            bank.prepare(48000, preset);
            for (int note : {48, 55, 60})
                bank.noteOn(1, note, .8f);
            Analysis recommended;
            for (int n = 0; n < 96000; ++n)
                recommended.add(e.tick(bank.tick()));
            check(recommended.rms() > .0158489, "recommended instruments below -36 dBFS RMS");
            preset = environmentSettings(location);
            for (auto id : {pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn})
                preset[id] = 1;
            e.prepare(48000, preset);
            bank.prepare(48000, preset);
            for (int note = 48; note < 60; ++note)
                bank.noteOn(1, note, 1);
            Analysis layered;
            for (int n = 0; n < 96000; ++n)
                layered.add(e.tick(bank.tick()));
            check(layered.peak <= .89f && layered.rms() < .316228,
                  "new location layering exceeds headroom/loudness limits");
            if (exportAudio)
                layerReport << environments[location].name << ',' << 20 * std::log10(layered.peak) << ','
                            << 20 * std::log10(layered.rms()) << '\n';
        }
        auto p = environmentSettings(4);
        p[seed] = 4242;
        Engine a, b;
        a.prepare(48000, p);
        b.prepare(48000, p);
        for (int n = 0; n < 48000; ++n)
        {
            auto x = a.tick(), y = b.tick();
            check(x.l == y.l && x.r == y.r, "location seed nondeterministic");
        }
        auto count = a.locationEvents();
        p[freeze] = 1;
        a.setSettings(p);
        for (int n = 0; n < 480000; ++n)
            a.tick();
        check(a.locationEvents() == count, "freeze generated new events");
        constexpr Param ons[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
        // Check the balance fix survives every new location, for every instrument family.
        for (int env = 0; env < 10; ++env)
            for (int family = 0; family < 6; ++family)
            {
                p = environmentSettings(env);
                for (auto id : ons)
                    p[id] = 0;
                p[ons[family]] = 1;
                Instruments bank;
                bank.prepare(48000, p);
                auto m = p, amb = p;
                m[ambienceBusLevel] = -60;
                amb[instrumentBusLevel] = -60;
                Engine music, noise;
                music.prepare(48000, m);
                noise.prepare(48000, amb);
                Analysis mm, nm;
                for (int n = 0; n < 288000; ++n)
                {
                    if (n == 0)
                        for (int note : {48, 55, 60})
                            bank.noteOn(1, note, .8f);
                    if (n == 192000)
                        for (int note : {48, 55, 60})
                            bank.noteOff(1, note);
                    mm.add(music.tick(bank.tick()));
                    nm.add(noise.tick());
                }
                check(mm.rms() > nm.rms() * 3.981, "new environment masks the musical instrument by RMS");
            }
        p = environmentSettings(0);
        a.prepare(48000, p);
        Instruments bank;
        bank.prepare(48000, p);
        bank.noteOn(1, 60, .8f);
        Stereo last;
        float jump = 0;
        for (int n = 0; n < 192000; ++n)
        {
            if (n % 9600 == 0)
            {
                p = environmentSettings((n / 9600) % 10, p);
                a.setSettings(p);
            }
            auto x = a.tick(bank.tick());
            jump = std::max({jump, std::abs(x.l - last.l), std::abs(x.r - last.r)});
            last = x;
        }
        check(jump < .25f, "large discontinuity during environment switching");
        std::cout << "Ten locations, 20 comparison renders, deterministic seed/freeze, 60 bus-balance "
                     "comparisons and switching checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ENVIRONMENT FAILURE: " << e.what() << '\n';
        return 1;
    }
}
