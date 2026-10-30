#include "dsp/Instruments.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace liminal;
void require(bool b, const char *message)
{
    if (!b)
        throw std::runtime_error(message);
}
double render(Instruments &bank, int samples)
{
    double energy = 0;
    for (int n = 0; n < samples; ++n)
    {
        auto s = bank.tick();
        require(std::isfinite(s.l) && std::isfinite(s.r) && std::abs(s.l) < 2 && std::abs(s.r) < 2,
                "unstable instrument");
        energy += s.l * s.l + s.r * s.r;
    }
    return energy / samples;
}
int main()
{
    try
    {
        constexpr Param ons[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
        double signatures[6]{};
        for (int i = 0; i < 6; ++i)
        {
            auto p = defaults();
            for (auto id : ons)
                p[id] = 0;
            p[ons[i]] = 1;
            Instruments b;
            b.prepare(48000, p);
            require(render(b, 100) == 0, "idle instrument not silent");
            for (int note : {48, 55, 60})
                b.noteOn(1, note, .8f);
            signatures[i] = render(b, 48000 * 2);
            require(signatures[i] > 1e-8, "silent MIDI family");
            b.sustain(1, true);
            for (int note : {48, 55, 60})
                b.noteOff(1, note);
            render(b, 24000);
            require(b.activeVoices() == 3, "sustain lost notes");
            b.bend(1, 16383);
            render(b, 24000);
            b.sustain(1, false);
            render(b, 48000 * 50);
            require(b.activeVoices() == 0, "stuck release");
            for (int note = 30; note < 90; ++note)
            {
                b.noteOn(1, note, .7f);
                render(b, 64);
            }
            require(b.activeVoices() <= 24, "voice capacity exceeded");
            b.allNotesOff(1, true);
            require(render(b, 100) == 0, "all sound off not silent");
            std::cout << specs[ons[i]].group << " chord energy " << signatures[i] << " passed\n";
        }
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < i; ++j)
                require(std::abs(signatures[i] - signatures[j]) > 1e-9, "identical family render signatures");
        for (double rate : {44100., 88200., 96000.})
        {
            auto p = defaults();
            for (auto id : ons)
                p[id] = 1;
            Instruments b;
            b.prepare(rate, p);
            for (int n = 48; n < 60; ++n)
                b.noteOn(1, n, .7f);
            render(b, int(rate));
            b.allNotesOff(1, true);
            require(render(b, 64) == 0, "layered panic failure");
        }
        std::cout
            << "Six dry families, chords, pedal, bend, stealing, release, layering and sample rates passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
