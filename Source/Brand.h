#pragma once
// The product's name as people see it (2026-11-06: LIMINAL became TAPEWORN, by Elsewhere Audio).
// Internal names (the namespace, classes, CMake target, the state's XML tag, parameter IDs, the AU's
// type / subtype / manufacturer codes) stay as they were, so songs and presets made with LIMINAL load
// unchanged.
#include <juce_core/juce_core.h>

namespace liminal::brand {

// TAPEWORN FX (2026-11-06), the effect built from the same sources: its own name, company, preset
// files and folders (TAPEWORN_FX is defined for its target only).
#if TAPEWORN_FX
inline constexpr const char* product = "TAPEWORN FX";
inline constexpr const char* company = "Windward Audio";
inline constexpr const char* presetExtension = ".tapewornfxpreset";
inline constexpr const char* presetTag = "TAPEWORN_FX_PRESET";
#else
inline constexpr const char* product = "TAPEWORN";
inline constexpr const char* company = "Elsewhere Audio";
inline constexpr const char* presetExtension = ".tapewornpreset";
inline constexpr const char* presetTag = "TAPEWORN_PRESET";
#endif
// What LIMINAL used (read, never written): its presets are carried over.
inline constexpr const char* legacyPresetExtension = ".liminalpreset";
inline constexpr const char* legacyPresetTag = "LIMINAL_PRESET";

// ~/Library/Application Support/Elsewhere Audio/TAPEWORN (and the folder LIMINAL used).
inline juce::File appDataFolder() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(company).getChildFile(product);
}
inline juce::File legacyAppDataFolder() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Veil Audio").getChildFile("LIMINAL");
}

} // namespace liminal::brand
