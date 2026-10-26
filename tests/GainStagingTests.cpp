#include "dsp/Instruments.h"
#include "presets/Factory.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace liminal;
struct Meter
{
    double squares = 0;
    float peak = 0;
    int frames = 0;
    void add(Stereo s)
    {
        if (!std::isfinite(s.l) || !std::isfinite(s.r))
            throw std::runtime_error("nonfinite");
        squares += double(s.l) * s.l + double(s.r) * s.r;
        peak = std::max({peak, std::abs(s.l), std::abs(s.r)});
        ++frames;
    }
    double rms() const { return std::sqrt(squares / (frames * 2)); }
};
double db(double x)
{
    return 20 * std::log10(std::max(x, 1e-12));
}
void check(bool b, const char *message)
{
    if (!b)
        throw std::runtime_error(message);
}
constexpr Param ons[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
void phrase(Instruments &b, int n)
{
    for (int j = 0; j < 3; ++j)
    {
        if (n == j * 9600)
            b.noteOn(1, 48 + j * 7, .8f);
        if (n == 144000)
            b.noteOff(1, 48 + j * 7);
        if (n == 192000)
            b.noteOn(1, 51 + j * 7, .8f);
        if (n == 288000)
            b.noteOff(1, 51 + j * 7);
    }
}
int main(int argc, char **argv)
{
    try
    {
        std::ofstream file;
        if (argc > 1)
        {
            std::filesystem::create_directories(argv[1]);
            file.open(std::string(argv[1]) + "/gain-staging.csv");
        }
        std::ostream &report = file.is_open() ? file : std::cout;
        report << "preset,instrument,instrument_rms_dbfs,ambience_rms_dbfs,balance_db,mix_peak_dbfs,mix_rms_"
                  "dbfs\n";
        for (int preset : {-1, 0, 3, 9})
            for (int family = 0; family < 6; ++family)
            {
                auto p = preset < 0 ? defaults() : factoryPresets()[std::size_t(preset)].values;
                for (auto id : ons)
                    p[id] = 0;
                p[ons[family]] = 1;
                Instruments bank;
                bank.prepare(48000, p);
                Engine music, ambience, mix;
                auto m = p, a = p;
                m[ambienceBusLevel] = -60;
                a[instrumentBusLevel] = -60;
                music.prepare(48000, m);
                ambience.prepare(48000, a);
                mix.prepare(48000, p);
                Meter mm, am, xm;
                for (int n = 0; n < 384000; ++n)
                {
                    phrase(bank, n);
                    auto s = bank.tick();
                    mm.add(music.tick(s));
                    am.add(ambience.tick());
                    xm.add(mix.tick(s));
                }
                double ratio = db(mm.rms() / am.rms());
                report << preset << ',' << specs[ons[family]].group << ',' << db(mm.rms()) << ','
                       << db(am.rms()) << ',' << ratio << ',' << db(xm.peak) << ',' << db(xm.rms()) << '\n';
                report.flush();
                check(db(mm.rms()) > -36, "default musical phrase below -36 dBFS RMS");
                check(ratio >= 12, "instrument less than 12 dB above ambience");
                check(xm.peak < .708f, "default mixed phrase lacks 3 dB peak headroom");
            }
        auto p = defaults();
        p[ambienceBusLevel] = -60;
        Engine low, high;
        p[presence] = 0;
        low.prepare(48000, p);
        p[presence] = 1;
        high.prepare(48000, p);
        Instruments b;
        b.prepare(48000, p);
        double delta = 0;
        for (int n = 0; n < 96000; ++n)
        {
            if (n == 0)
                b.noteOn(1, 60, .8f);
            auto s = b.tick();
            auto l = low.tick(s), h = high.tick(s);
            delta += std::abs(l.l - h.l) + std::abs(l.r - h.r);
        }
        check(delta < 1e-6, "Presence changes isolated musical level");
        p = defaults();
        p[ambienceBusLevel] = -60;
        Engine unity, minusSix;
        p[master] = -20;
        unity.prepare(48000, p);
        p[instrumentBusLevel] = -6;
        minusSix.prepare(48000, p);
        b.prepare(48000, p);
        Meter u, d;
        for (int n = 0; n < 96000; ++n)
        {
            if (n == 0)
                b.noteOn(1, 60, .8f);
            auto s = b.tick();
            u.add(unity.tick(s));
            d.add(minusSix.tick(s));
        }
        check(std::abs(db(u.rms() / d.rms()) - 6) < .3, "Instrument Level calibration is not 6 dB");
        p = defaults();
        p[master] = -20;
        Engine ambienceUnity, ambienceDown;
        ambienceUnity.prepare(48000, p);
        p[ambienceBusLevel] -= 6;
        ambienceDown.prepare(48000, p);
        Meter au, ad;
        for (int n = 0; n < 96000; ++n)
        {
            au.add(ambienceUnity.tick());
            ad.add(ambienceDown.tick());
        }
        check(std::abs(db(au.rms() / ad.rms()) - 6) < .3, "Ambience Level calibration is not 6 dB");
        p = defaults();
        for (auto id : ons)
            p[id] = 1;
        Engine engine;
        engine.prepare(48000, p);
        b.prepare(48000, p);
        for (int note = 48; note < 60; ++note)
            b.noteOn(1, note, 1);
        Meter layered;
        for (int n = 0; n < 144000; ++n)
            layered.add(engine.tick(b.tick()));
        check(layered.peak <= .89f, "layered peak exceeds safety ceiling");
        check(db(layered.rms()) < -10, "layered output excessive sustained loudness");
        std::cout << "Layered max peak " << db(layered.peak) << " dBFS, RMS " << db(layered.rms())
                  << " dBFS\n24 instrument/ambience comparisons, Presence isolation and layering passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "GAIN FAILURE: " << e.what() << '\n';
        return 1;
    }
}
