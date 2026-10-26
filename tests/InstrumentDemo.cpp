#include "dsp/Instruments.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace liminal;
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: instrument_demo output-directory\n";
        return 1;
    }
    std::filesystem::create_directories(argv[1]);
    constexpr Param ids[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
    std::ofstream index(std::string(argv[1]) + "/INDEX.txt");
    index << "Original synthesis; 48 kHz stereo PCM16, dry, seed 12345.\nAll controls 0.35, family level "
             "0.65.\nSame C-minor notes 48/55/60/63 at velocity .8; no room, effects, normalisation or "
             "limiter.\nListening evaluation has not been performed.\n";
    for (int family = 0; family < 6; ++family)
    {
        auto p = defaults();
        for (auto id : ids)
            p[id] = 0;
        p[ids[family]] = 1;
        Instruments bank;
        bank.prepare(48000, p);
        std::string name = "instrument-" + std::to_string(family + 1) + ".wav";
        index << name << " : " << specs[ids[family]].group << '\n';
        std::ofstream out(std::string(argv[1]) + "/" + name, std::ios::binary);
        auto u16 = [&](unsigned v)
        {
            out.put(char(v));
            out.put(char(v >> 8));
        };
        auto u32 = [&](unsigned v)
        {
            u16(v & 65535);
            u16(v >> 16);
        };
        constexpr int frames = 48000 * 12;
        out.write("RIFF", 4);
        u32(36 + frames * 4);
        out.write("WAVEfmt ", 8);
        u32(16);
        u16(1);
        u16(2);
        u32(48000);
        u32(192000);
        u16(4);
        u16(16);
        out.write("data", 4);
        u32(frames * 4);
        constexpr int notes[]{48, 55, 60, 63};
        for (int n = 0; n < frames; ++n)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (n == j * 24000)
                    bank.noteOn(1, notes[j], .8f);
                if (n == j * 24000 + 20000)
                    bank.noteOff(1, notes[j]);
                if (n == 96000)
                    bank.noteOn(1, notes[j], .8f);
                if (n == 192000)
                    bank.noteOff(1, notes[j]);
            }
            auto s = bank.tick();
            for (float value : {s.l, s.r})
                u16(unsigned(int(std::clamp(value, -1.f, 1.f) * 32767)) & 65535);
        }
        if (!out)
        {
            std::cerr << "Render write failed\n";
            return 1;
        }
        std::cout << name << '\n';
    }
    return 0;
}
