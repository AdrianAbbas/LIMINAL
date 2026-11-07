#pragma once
// VACANCY FX's factory presets (2026-11-06): VACANCY's places for audio, each made for a kind of
// source (it works on any). A preset sets every parameter of the sound (the rest at its default);
// Bypass and the VHS look (viewing preferences) are kept.
#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

namespace liminal::fx::presets {

enum class Source { vocals, guitar, keys, drums, mix, any, user };
inline constexpr int sourceCount = 7;
inline constexpr const char* sourceNames[sourceCount] { "VOCALS", "GUITAR", "KEYS", "DRUMS", "MIX", "ANY", "YOURS" };

struct Preset {
    juce::String name;
    Source source;
    juce::String description;
    std::vector<std::pair<juce::String, float>> values;   // parameter ID -> value (in the parameter's own units)
};

const std::vector<Preset>& factory();
std::vector<int> inSource(Source);   // indices into factory()

} // namespace liminal::fx::presets
