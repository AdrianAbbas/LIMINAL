#pragma once
// The product's name as people see it. 2026-11-06: LIMINAL became TAPEWORN; 2026-11-07: TAPEWORN
// became VACANCY and TAPEWORN FX became VACANCY FX, both by Windward Audio.
// Internal names (the namespace, classes, CMake targets, the state's XML tags, parameter IDs, the AU's
// type / subtype / manufacturer codes) stay as they were, so songs and presets made with LIMINAL,
// TAPEWORN and TAPEWORN FX load unchanged.
#include <juce_core/juce_core.h>
#include <initializer_list>

namespace liminal::brand {

inline constexpr const char* company = "Windward Audio";

// One folder of presets an earlier name saved (read, never written): carried over, and loadable.
struct LegacyPresets {
    const char* company;
    const char* product;
    const char* extension;
    const char* tag;
};

// VACANCY FX (TAPEWORN FX until 2026-11-07), the effect built from the same sources: its own name,
// preset files and folders (TAPEWORN_FX is defined for its target only).
#if TAPEWORN_FX
inline constexpr const char* product = "VACANCY FX";
inline constexpr const char* presetExtension = ".vacancyfxpreset";
inline constexpr const char* presetTag = "VACANCY_FX_PRESET";
inline constexpr LegacyPresets legacyPresets[] = {
    { "Windward Audio", "TAPEWORN FX", ".tapewornfxpreset", "TAPEWORN_FX_PRESET" },
};
#else
inline constexpr const char* product = "VACANCY";
inline constexpr const char* presetExtension = ".vacancypreset";
inline constexpr const char* presetTag = "VACANCY_PRESET";
inline constexpr LegacyPresets legacyPresets[] = {
    { "Elsewhere Audio", "TAPEWORN", ".tapewornpreset", "TAPEWORN_PRESET" },   // TAPEWORN (2026-11-06)
    { "Veil Audio", "LIMINAL", ".liminalpreset", "LIMINAL_PRESET" },            // LIMINAL (before)
};
#endif

// ~/Library/Windward Audio/VACANCY (juce's userApplicationDataDirectory is ~/Library on macOS).
inline juce::File appDataFolder() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(company).getChildFile(product);
}
inline juce::File presetFolder() { return appDataFolder().getChildFile("Presets"); }
inline juce::File legacyPresetFolder(const LegacyPresets& l) {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(l.company).getChildFile(l.product).getChildFile("Presets");
}

// A preset file this product reads: its own tag or one an earlier name wrote.
inline bool isPresetTag(const juce::String& tag) {
    if (tag == presetTag) return true;
    for (const auto& l : legacyPresets)
        if (tag == l.tag) return true;
    return false;
}

// "*.vacancypreset;*.tapewornpreset;..." for listing the preset folder.
inline juce::String presetWildcard() {
    juce::String w = juce::String("*") + presetExtension;
    for (const auto& l : legacyPresets) w << ";*" << l.extension;
    return w;
}

// The presets saved under the earlier names are copied over once, newest name first (a preset
// already in the new folder under that name is never replaced; the old files are left in place).
inline void carryOverLegacyPresets() {
    const auto folder = presetFolder();
    for (const auto& l : legacyPresets) {
        const auto old = legacyPresetFolder(l);
        if (!old.isDirectory()) continue;
        for (const auto& f : old.findChildFiles(juce::File::findFiles, false, juce::String("*") + l.extension)) {
            const auto target = folder.getChildFile(f.getFileNameWithoutExtension() + presetExtension);
            if (target.existsAsFile() || !folder.createDirectory()) continue;
            f.copyFileTo(target);
        }
    }
}

} // namespace liminal::brand
