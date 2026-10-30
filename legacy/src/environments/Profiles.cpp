#include "Profiles.h"
#include <algorithm>
namespace liminal
{
const std::array<EnvironmentProfile, 10> environments{
    {{"Vacant Hospital", "Clinical ventilation, equipment tones and remote metallic impacts.",
      "Forgotten Piano / Empty Choir", 7, 68, 3, 1, 350, 96, 2300, .137f, .17f, .18f, 1.18f, 0xff54786c},
     {"Empty Shopping Mall", "Escalator drives and diffuse movement in a broad atrium.",
      "Forgotten Piano / Hollow Drone", 55, 90, 14, 3, 650, 43, 4200, .37f, .29f, .13f, 1.05f, 0xff887a66},
     {"Abandoned School", "Duct bands, resonant pipes and creaking classroom mechanisms.",
      "Forgotten Piano / Lost Music Box", 6, 82, 3.2f, 5, 580, 72, 1900, .19f, .24f, .4f, 1.22f, 0xff958658},
     {"Underground Parking Garage", "Deep fan blades, hollow concrete impacts and low structural pressure.",
      "Hollow Drone / Faded Synth", 65, 95, 3, 0, 120, 29, 1600, .11f, .31f, .08f, 1.28f, 0xff626b72},
     {"Abandoned Indoor Pool", "Gliding water drops and bright tiled reflections over quiet air.",
      "Glass Memories / Forgotten Piano", 30, 50, 11, 1, 1600, 51, 10000, .283f, .4f, .04f, .98f, 0xff397e99},
     {"Empty Hotel", "Carpet-muted room tone, elevator mechanisms and distant door-like impacts.",
      "Forgotten Piano / Empty Choir", 9, 64, 3, 4, 240, 58, 1300, .23f, .14f, .32f, 1.38f, 0xff976a50},
     {"Deserted Subway Station", "Tunnel airflow, bending rail modes and directional machinery.",
      "Glass Memories / Hollow Drone", 8, 145, 6, 2, 800, 36, 3200, .49f, .42f, .28f, 1.12f, 0xff4b7276},
     {"Empty Supermarket", "Irregular refrigeration cycles, compressor harmonics and switching transients.",
      "Lost Music Box / Faded Synth", 43, 70, 5, 3, 420, 59, 2700, .079f, .2f, .5f, 1.12f, 0xff9c9b74},
     {"Endless Office", "Transformer banks, thin electronic activity and modulated repetitive reflections.",
      "Faded Synth / Empty Choir", 5, 115, 2.8f, 4, 960, 119, 2900, .213f, .32f, .65f, 1.16f, 0xff6c8271},
     {"Impossible Room", "Inharmonic pressure fields, irregular modal excitation and warped echoes.",
      "Glass Memories / Hollow Drone", 72, 110, 27, 2, 1250, 47, 6200, .61f, .45f, 1.f, 1.05f, 0xff766381}}};
Settings environmentSettings(int index, const Settings &current)
{
    auto p = current;
    index = std::clamp(index, 0, 9);
    auto &e = environments[std::size_t(index)];
    p[environmentModel] = float(index);
    p[roomWidth] = e.width;
    p[roomLength] = e.length;
    p[roomHeight] = e.height;
    p[material] = float(e.material);
    p[sourceDistance] = index == 4 ? 8 : 12;
    p[reverb] = index == 4 ? .7f : (index == 5 ? .35f : .55f);
    p[scale] = index == 1 || index == 9 ? .8f : .45f;
    p[eventDensity] = .5f;
    p[activity] = .5f;
    p[evolutionDepth] = index == 9 ? .8f : .4f;
    p[evolutionRate] = index == 7 ? .15f : .3f;
    p[ambienceBusLevel] = index == 3 || index == 6 ? -15.f : -12.f;
    p[environmentInfluence] = 1;
    p[electricalOn] = p[ventilationOn] = p[structureOn] = p[atmosphereOn] = p[distantOn] = p[subOn] = 1;
    p[unreal] = index == 9 ? .65f : .1f;
    return sanitise(p);
}
Settings recommendedEnvironmentSettings(int index, const Settings &current)
{
    index = std::clamp(index, 0, 9);
    auto p = environmentSettings(index, current);
    const auto &e = environments[index];
    constexpr Param first[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
    constexpr int pairs[10][2]{{0, 3}, {0, 5}, {0, 4}, {5, 1}, {2, 0},
                               {0, 3}, {2, 5}, {4, 1}, {1, 3}, {2, 5}};
    constexpr float piano[10][6]{{.5f, .12f, .7f, .1f, .3f, .4f},   {.3f, .2f, .6f, .1f, .7f, .45f},
                                 {.8f, .5f, .6f, .25f, .5f, .35f},  {.5f, .1f, .7f, .25f, .4f, .65f},
                                 {.2f, .08f, .2f, .15f, .8f, .3f},  {.7f, .2f, .9f, .1f, .35f, .5f},
                                 {.6f, .3f, .55f, .2f, .55f, .5f},  {.65f, .3f, .5f, .08f, .45f, .3f},
                                 {.45f, .25f, .7f, .05f, .6f, .5f}, {.9f, .9f, .35f, .15f, .9f, .8f}};
    float brightness = std::clamp(e.colourHz / 10000.f, .15f, .95f);
    float controls[6][6]{};
    for (int j = 0; j < 6; ++j)
        controls[0][j] = piano[index][j];
    const float synth[]{.12f + .45f * e.flutter, 1 - brightness,      .1f + .6f * e.flutter, .2f,
                        .3f + .5f * p[reverb],   .5f + .4f * p[scale]};
    const float glass[]{1 - brightness,        brightness, .4f + .4f * p[scale], .1f + .6f * e.flutter,
                        .3f + .6f * p[reverb], .3f};
    const float choir[]{.25f,           .3f + .3f * brightness, .15f + .4f * e.flutter, .5f + .4f * p[scale],
                        1 - brightness, .25f + .5f * p[reverb]};
    const float box[]{
        .2f + .6f * e.flutter, .05f + .6f * e.flutter, .2f, brightness, .2f + .6f * p[reverb], .3f};
    const float drone[]{.35f + .4f * p[reverb],  .1f + .8f * e.flutter,   .2f + .6f * brightness,
                        index == 3 ? .65f : .3f, .25f + .65f * e.flutter, .3f + .6f * p[reverb]};
    const float *others[]{synth, glass, choir, box, drone};
    for (int family = 1; family < 6; ++family)
        for (int j = 0; j < 6; ++j)
            controls[family][j] = others[family - 1][j];
    for (int family = 0; family < 6; ++family)
    {
        p[first[family]] = family == pairs[index][0] || family == pairs[index][1] ? 1.f : 0.f;
        p[first[family] + 1] = family == pairs[index][0] ? .65f : .3f;
        for (int j = 0; j < 6; ++j)
            p[first[family] + 2 + j] = controls[family][j];
    }
    return sanitise(p);
}
} // namespace liminal
