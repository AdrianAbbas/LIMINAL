#include "Parameters.h"

namespace liminal::params {

namespace {
std::unique_ptr<juce::AudioParameterFloat> level(const char* id, const char* name, float defaultDb) {
    juce::NormalisableRange<float> range(minDb, maxDb, 0.1f);
    range.setSkewForCentre(-12.0f);
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, range, defaultDb,
        juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) {
            return v <= minDb + 0.01f ? juce::String("-inf") : juce::String(v, 1);
        }));
}
std::unique_ptr<juce::AudioParameterBool> toggle(const char* id, const char* name, bool on) {
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, on);
}

// A 0..1 amount shown as a percentage.
std::unique_ptr<juce::AudioParameterFloat> amount(const char* id, const char* name, float value) {
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), value,
        juce::AudioParameterFloatAttributes().withLabel("%").withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100)); }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { power, 1 }, "Power", true));
    layout.add(level(master, "Master Level", 0.0f));
    layout.add(level(instrument, "Instrument Level", 0.0f));
    layout.add(level(ambience, "Ambience Level", 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { space, 1 }, "Space", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100)) + " %"; })));
    // Forgotten Piano
    layout.add(toggle(pianoOn, "Piano On", true));
    layout.add(level(pianoLevel, "Piano Level", 0.0f));
    layout.add(amount(pianoAge, "Piano Age", 0.35f));
    layout.add(amount(pianoDetune, "Piano Detune", 0.25f));
    layout.add(amount(pianoSoftness, "Piano Softness", 0.3f));
    layout.add(amount(pianoNoise, "Piano Mechanical Noise", 0.4f));
    layout.add(amount(pianoDecay, "Piano Decay", 0.5f));
    layout.add(amount(pianoDistance, "Piano Distance", 0.0f));
    return layout;
}

} // namespace liminal::params
