#pragma once
#include "Parameters.h"
namespace liminal
{
struct EnvironmentProfile
{
    const char *name;
    const char *description;
    const char *recommended;
    float width, length, height;
    int material;
    float airCutoff, motorHz, colourHz, echoSeconds, echoAmount, flutter, musicGain;
    unsigned colour;
};
extern const std::array<EnvironmentProfile, 10> environments;
Settings recommendedEnvironmentSettings(int index, const Settings &current = defaults());
Settings environmentSettings(int index, const Settings &current = defaults());
} // namespace liminal
