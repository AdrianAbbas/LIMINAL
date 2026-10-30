#pragma once
#include "Parameters.h"
#include <array>
namespace liminal
{
struct FactoryPreset
{
    const char *name;
    const char *category;
    const char *description;
    Settings values;
};
const std::array<FactoryPreset, 15> &factoryPresets();
} // namespace liminal
