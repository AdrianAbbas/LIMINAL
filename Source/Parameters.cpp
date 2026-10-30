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
// A time in seconds, shown as ms below one second.
std::unique_ptr<juce::AudioParameterFloat> seconds(const char* id, const char* name, float lo, float hi, float centre, float value) {
    juce::NormalisableRange<float> range(lo, hi, 0.0001f);
    range.setSkewForCentre(centre);
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, range, value,
        juce::AudioParameterFloatAttributes().withLabel("s").withStringFromValueFunction([](float v, int) {
            return v < 1.0f ? juce::String(juce::roundToInt(v * 1000.0f)) + " ms" : juce::String(v, 2) + " s";
        }));
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
    // Faded Synth
    layout.add(toggle(synthOn, "Synth On", false));
    layout.add(level(synthLevel, "Synth Level", 0.0f));
    layout.add(amount(synthDetune, "Synth Detune", 0.35f));
    layout.add(amount(synthWarmth, "Synth Warmth", 0.5f));
    layout.add(amount(synthDrift, "Synth Drift", 0.3f));
    layout.add(seconds(synthAttack, "Synth Attack", 0.001f, 4.0f, 0.15f, 0.008f));
    layout.add(seconds(synthRelease, "Synth Release", 0.01f, 6.0f, 0.6f, 0.6f));
    layout.add(amount(synthWidth, "Synth Width", 0.5f));
    layout.add(toggle(synthGlide, "Synth Glide", false));
    layout.add(seconds(synthGlideTime, "Synth Glide Time", 0.01f, 2.0f, 0.2f, 0.12f));
    // Glass Memories
    layout.add(toggle(glassOn, "Glass On", false));
    layout.add(level(glassLevel, "Glass Level", 0.0f));
    layout.add(amount(glassFragility, "Glass Fragility", 0.4f));
    layout.add(amount(glassBrightness, "Glass Brightness", 0.5f));
    layout.add(amount(glassResonance, "Glass Resonance", 0.3f));
    layout.add(amount(glassDetune, "Glass Detune", 0.2f));
    layout.add(amount(glassDecay, "Glass Decay", 0.6f));
    layout.add(amount(glassDistance, "Glass Distance", 0.0f));
    // Empty Choir
    layout.add(toggle(choirOn, "Choir On", false));
    layout.add(level(choirLevel, "Choir Level", 0.0f));
    layout.add(amount(choirBreath, "Choir Breath", 0.3f));
    layout.add(amount(choirFormant, "Choir Formant", 0.3f));
    layout.add(amount(choirDetune, "Choir Detune", 0.35f));
    layout.add(amount(choirWidth, "Choir Width", 0.6f));
    layout.add(amount(choirDarkness, "Choir Darkness", 0.4f));
    layout.add(seconds(choirRelease, "Choir Release", 0.05f, 10.0f, 1.5f, 2.5f));
    // Lost Music Box
    layout.add(toggle(boxOn, "Music Box On", false));
    layout.add(level(boxLevel, "Music Box Level", 0.0f));
    layout.add(amount(boxWear, "Music Box Wear", 0.4f));
    layout.add(amount(boxDrift, "Music Box Pitch Drift", 0.3f));
    layout.add(amount(boxMechanism, "Music Box Mechanism", 0.4f));
    layout.add(amount(boxBrightness, "Music Box Brightness", 0.5f));
    layout.add(amount(boxDecay, "Music Box Decay", 0.5f));
    layout.add(amount(boxDistance, "Music Box Distance", 0.0f));
    // Hollow Drone
    layout.add(toggle(droneOn, "Drone On", false));
    layout.add(level(droneLevel, "Drone Level", 0.0f));
    layout.add(amount(droneDensity, "Drone Density", 0.5f));
    layout.add(amount(droneInstability, "Drone Instability", 0.35f));
    layout.add(amount(droneHarmonics, "Drone Harmonics", 0.4f));
    layout.add(amount(droneSub, "Drone Sub", 0.4f));
    layout.add(amount(droneMovement, "Drone Movement", 0.4f));
    layout.add(seconds(droneRelease, "Drone Release", 0.05f, 20.0f, 3.0f, 4.0f));
    layout.add(seconds(droneAttack, "Drone Attack", 0.005f, 12.0f, 1.0f, 1.2f));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { solo, 1 }, "Solo", 0, instrumentCount, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) {
            const char* names[] { "Off", "Piano", "Synth", "Glass", "Choir", "Music Box", "Drone" };
            return juce::String(names[std::clamp(v, 0, instrumentCount)]);
        })));
    // Environment. Append only: the saved value is the index, so existing choices keep
    // their meaning (2a had the first five; 2b appended the rest).
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { environment, 1 }, "Environment",
        juce::StringArray { "Vacant Hospital", "Empty Shopping Mall", "Abandoned School", "Underground Parking Garage", "Abandoned Indoor Pool",
                            "Empty Hotel", "Deserted Subway Station", "Empty Supermarket", "Endless Office", "Impossible Room" }, 0));
    layout.add(amount(environmentAmount, "Environment Amount", 1.0f));
    // Milestone 5 (appended after the effects, below).
    // Per-instrument effects (Milestone 3). Amounts are the player's own, added on top of
    // what the place brings (scaled by Environment Influence); toggles bypass a module.
    for (int i = 0; i < instrumentCount; ++i) {
        const juce::String who = instrumentLabels[i];
        const auto id = [&](const char* n) { return fxId(i, n); };
        const auto pct = [&](const char* n, const juce::String& name, float v) {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { id(n), 1 }, who + " " + name, juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), v,
                juce::AudioParameterFloatAttributes().withLabel("%").withStringFromValueFunction([](float x, int) { return juce::String(juce::roundToInt(x * 100)); })));
        };
        const auto onOff = [&](const char* n, const juce::String& name, bool v) {
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id(n), 1 }, who + " " + name, v));
        };
        const auto hz = [&](const char* n, const juce::String& name, float lo, float hi, float v) {
            juce::NormalisableRange<float> range(lo, hi, 0.1f);
            range.setSkewForCentre(std::sqrt(lo * hi));
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { id(n), 1 }, who + " " + name, range, v,
                juce::AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction([](float x, int) {
                    return x >= 1000.0f ? juce::String(x / 1000.0f, 1) + " k" : juce::String(juce::roundToInt(x));
                })));
        };
        pct(fx::mix, "Effects Mix", 1.0f);
        pct(fx::influence, "Environment Influence", 1.0f);
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id(fx::order), 1 }, who + " Effect Order",
                                                                juce::StringArray(fx::orderNames, 3), 0));
        onOff(fx::driftOn, "Drift On", true);
        pct(fx::drift, "Drift", 0.0f);
        pct(fx::driftRate, "Drift Rate", 0.3f);
        onOff(fx::tapeOn, "Tape On", true);
        pct(fx::tape, "Tape", 0.0f);
        pct(fx::tapeTone, "Tape Tone", 0.5f);
        pct(fx::tapeHiss, "Tape Hiss", 0.3f);
        pct(fx::tapeDropouts, "Tape Dropouts", 0.0f);
        onOff(fx::reverbOn, "Reverb On", true);
        pct(fx::reverb, "Reverb", 0.5f);
        onOff(fx::echoOn, "Echo On", true);
        pct(fx::echo, "Echo", 0.0f);
        {
            juce::NormalisableRange<float> range(20.0f, 1500.0f, 1.0f);
            range.setSkewForCentre(300.0f);
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id(fx::echoTime), 1 }, who + " Echo Time", range, 375.0f,
                                                                   juce::AudioParameterFloatAttributes().withLabel("ms")));
        }
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id(fx::echoSync), 1 }, who + " Echo Sync",
                                                                juce::StringArray(fx::syncNames, 8), 0));
        pct(fx::echoFeedback, "Echo Feedback", 0.35f);
        pct(fx::echoTone, "Echo Tone", 0.5f);
        onOff(fx::chorusOn, "Chorus On", true);
        pct(fx::chorus, "Chorus", 0.0f);
        pct(fx::chorusRate, "Chorus Rate", 0.3f);
        onOff(fx::filterOn, "Filter On", true);
        pct(fx::filter, "Filter", 1.0f);
        hz(fx::filterLow, "Low-pass", 200.0f, 20000.0f, 20000.0f);
        hz(fx::filterHigh, "High-pass", 20.0f, 2000.0f, 20.0f);
        pct(fx::filterRes, "Resonance", 0.1f);
        onOff(fx::grainOn, "Granular On", true);
        pct(fx::grain, "Granular", 0.0f);
        pct(fx::grainSize, "Grain Size", 0.4f);
        pct(fx::grainSpread, "Grain Spread", 0.5f);
        onOff(fx::grainFreeze, "Granular Freeze", false);
    }
    // Milestone 5: the Character of the place, evolution, the four macros, scenes A/B.
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { character, 1 }, "Character", 0, 2, 0,
                                                         juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return juce::String(v + 1); })));
    layout.add(amount(evolution, "Evolution Rate", 0.4f));
    layout.add(toggle(evolutionFreeze, "Evolution Freeze", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { evolutionSeed, 1 }, "Evolution Seed", 1, 9999, 1));
    layout.add(amount(presence, "Presence", 0.5f));
    layout.add(amount(scale, "Scale", 0.5f));
    layout.add(amount(activity, "Activity", 0.5f));
    layout.add(amount(unreal, "Unreal", 0.0f));
    layout.add(toggle(sceneOn, "Scene Morph On", false));
    layout.add(amount(sceneMorph, "Scene Morph", 0.0f));
    return layout;
}

} // namespace liminal::params
