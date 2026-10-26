#include "Factory.h"
namespace liminal
{
const std::array<FactoryPreset, 15> &factoryPresets()
{
    static const auto presets = []
    {
        std::array<FactoryPreset, 15> result{};
        const char *names[]{
            "Empty Shopping Mall",       "Abandoned School Hallway",  "Underground Parking Garage",
            "Vacant Hospital",           "Indoor Swimming Pool",      "Endless Office Corridor",
            "Fluorescent Classroom",     "Empty Supermarket",         "Concrete Stairwell",
            "Derelict Industrial Plant", "Subway Maintenance Tunnel", "Deserted Hotel Lobby",
            "Empty Airport Terminal",    "Impossible Room",           "The Backrooms"};
        const char *descriptions[]{"Wide tiled atrium; soft ventilation and receding electrical light.",
                                   "Narrow hard corridor with intermittent pipe resonance.",
                                   "Heavy concrete, slow fans and deep structural pressure.",
                                   "Quiet clinical air, remote impacts and bright empty reflections.",
                                   "Tall tiled chamber with a diffuse bright acoustic tail.",
                                   "Carpeted repeating space, subdued machinery and spectral drift.",
                                   "Close electrical harmonics and irregular fluorescent flicker.",
                                   "Broad retail floor; motors beneath a glassy reflection field.",
                                   "Vertical concrete void with exposed structural ringing.",
                                   "Dense metal resonances, heavy motors and distant machinery.",
                                   "Long tunnel rumble with muffled remote activity.",
                                   "Warm wood surfaces and a gentle, quiet atmospheric bed.",
                                   "Enormous glass volume with distant movement and long tails.",
                                   "Unstable resonances and spatial drift in an impossible volume.",
                                   "Oppressive fluorescent hum, carpet absorption and slow unreality."};
        const float widths[]{45, 6, 55, 12, 28, 5, 10, 42, 5, 38, 6, 26, 85, 65, 32};
        const float lengths[]{70, 65, 85, 40, 45, 110, 14, 65, 9, 60, 140, 38, 140, 95, 90};
        const float heights[]{12, 3, 3, 3, 10, 3, 3, 6, 22, 16, 5, 8, 24, 28, 3};
        const int materials[]{1, 1, 0, 1, 1, 4, 6, 3, 0, 2, 0, 5, 3, 2, 4};
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            auto p = defaults();
            p[roomWidth] = widths[i];
            p[roomLength] = lengths[i];
            p[roomHeight] = heights[i];
            p[material] = float(materials[i]);
            p[seed] = float(1024 + i * 997);
            p[sourceDistance] = 3 + float(i % 5) * 8;
            p[scale] = .25f + .045f * float(i);
            p[darkness] = .3f + .045f * float(i % 12);
            p[electricalLevel] = .2f + .08f * float(i % 7);
            p[ventilationLevel] = .25f + .07f * float((i + 2) % 6);
            p[activity] = .15f + .1f * float(i % 6);
            p[structureMaterial] = float(i % 7) / 6;
            p[structureActivity] = .2f + .1f * float(i % 7);
            p[fanSpeed] = 1 + float(i % 9);
            p[subFrequency] = 27 + float(i % 6) * 6;
            p[reverb] = .3f + .04f * float(i % 12);
            p[unreal] = i >= 13 ? .75f + .15f * float(i - 13) : .02f * float(i);
            p[flicker] = .1f + .06f * float(i % 9);
            p[distantActivity] = .15f + .1f * float((i + 3) % 7);
            p[eventDensity] = .2f + .07f * float(i % 9);
            p[texture] = .12f + .07f * float(i % 8);
            p[mains] = float(i % 2);
            p[decay] = .6f + .5f * float(i % 9);
            result[i] = {names[i],
                         i >= 13 ? "Unreal" : (i == 2 || i == 9 || i == 10 ? "Industrial" : "Architectural"),
                         descriptions[i], sanitise(p)};
        }
        return result;
    }();
    return presets;
}
} // namespace liminal
