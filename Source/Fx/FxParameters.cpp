#include "Fx/FxParameters.h"

namespace liminal::fxp {

namespace {
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
juce::AudioParameterFloatAttributes percent() {
    return juce::AudioParameterFloatAttributes().withLabel("%").withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100)); });
}
void amount(Layout& layout, const juce::String& id, const juce::String& name, float value) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), value, percent()));
}
void gainDb(Layout& layout, const char* id, const char* name) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(minDb, maxDb, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) { return (v > 0.04f ? "+" : "") + juce::String(v, 1); })));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    Layout layout;
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { bypass, 1 }, "Bypass", false));
    amount(layout, mix, "Mix", defaultMix);
    gainDb(layout, input, "Input");
    gainDb(layout, output, "Output");
    amount(layout, intensity, "Intensity", defaultIntensity);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { environment, 1 }, "Room",
        juce::StringArray { "Vacant Hospital", "Empty Shopping Mall", "Abandoned School", "Underground Parking Garage", "Abandoned Indoor Pool",
                            "Empty Hotel", "Deserted Subway Station", "Empty Supermarket", "Endless Office", "Impossible Room" }, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { character, 1 }, "Character", 0, 2, 0,
                                                         juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return juce::String(v + 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { space, 1 }, "Space", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), defaultSpace,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100)) + " %"; })));
    {
        juce::NormalisableRange<float> range(params::minDb, 0.0f, 0.1f);
        range.setSkewForCentre(-18.0f);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { ambience, 1 }, "Room Sound", range, defaultRoomSound,
            juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) {
                return v <= params::minDb + 0.01f ? juce::String("-inf") : juce::String(v, 1);
            })));
    }
    amount(layout, presence, "Presence", 0.5f);
    amount(layout, scale, "Scale", 0.5f);
    amount(layout, activity, "Activity", 0.25f);
    amount(layout, unreal, "Unreal", 0.0f);
    amount(layout, evolution, "Evolution Rate", 0.4f);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { evolutionFreeze, 1 }, "Evolution Freeze", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { evolutionSeed, 1 }, "Evolution Seed", 1, 9999, 1));
    amount(layout, farAway, "Far Away", 0.0f);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { quality, 1 }, "Quality", juce::StringArray { "Eco", "Standard", "High" }, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { vhs, 1 }, "VHS Look", juce::StringArray { "Off", "Light", "Heavy" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { vhsWarble, 1 }, "VHS Tape Warble", false));
    amount(layout, liminality, "Liminality", dsp::lim::macroDefault);
    for (int k = 0; k < dsp::lim::count; ++k) amount(layout, limId(k), juce::String("Liminality ") + dsp::lim::names[k], limDefaults[k]);
    return layout;
}

} // namespace liminal::fxp
