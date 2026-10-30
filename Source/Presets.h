#pragma once
// FACTORY PRESETS (Milestone 6), in four groups plus the player's own (saved as files):
//   Environments (10)  the place, its first Character, nothing else touched
//   Characters (30)    the place and one of its Characters, with macros/evolution to suit
//   Instruments (26)   six foundational (each instrument on its own) + 20 variations; only
//                      the instruments change, the place stays
//   Scenes (20)        everything: instruments, effects, place, Character, macros
// Values are real parameter values (dB, seconds, %/100, indices), by parameter ID.
#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

namespace liminal::presets {

enum class Category { environment, character, instrument, scene, user };
inline const char* categoryNames[] { "ENVIRONMENTS", "CHARACTERS", "INSTRUMENTS", "SCENES", "USER" };

struct Preset {
    juce::String name;
    Category category;
    int instrument = -1;   // instrument presets: which one (its controls are reset first)
    juce::String description;
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<Preset>& factory();
std::vector<int> inCategory(Category);   // indices into factory()

} // namespace liminal::presets
