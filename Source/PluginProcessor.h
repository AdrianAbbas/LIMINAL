#pragma once
#include "DSP/RoomTone.h"
#include "DSP/Space.h"
#include "DSP/Distance.h"
#include "DSP/FadedSynth.h"
#include "DSP/ForgottenPiano.h"
#include "DSP/GlassMemories.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace liminal {

class LiminalProcessor final : public juce::AudioProcessor {
public:
    LiminalProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Corridor (test)"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;

    // Read by the editor (message thread); written once per block by the audio thread.
    struct Meters {
        std::atomic<float> instrument { 0 }, ambience { 0 }, output { 0 }; // RMS, linear
        std::atomic<int> voices { 0 };
    } meters;

    static constexpr int stateVersion = 1;

private:
    struct Smooth {
        float current = 0, target = 0, coef = 0.01f;
        void reset(float v) { current = target = v; }
        float next() { return current += coef * (target - current); }
    };
    // One playable instrument with its on/off, level and distance (before the room).
    struct Slot {
        dsp::Instrument* instrument = nullptr;
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* distance = nullptr;   // optional
        dsp::Distance stage;
        Smooth gain;
        bool running = false;
        bool enabled() const { return on->load() > 0.5f; }
        float targetGain() const { return enabled() ? params::levelGain(level->load()) : 0.0f; }
    };
    static constexpr int instrumentCount = 3;
    void handleMidi(const juce::MidiMessage&);
    void updateSettings();
    void renderSegment(float* left, float* right, int n);
    static constexpr int chunk = 256;

    std::atomic<float>* power = nullptr;
    std::atomic<float>* master = nullptr;
    std::atomic<float>* instrumentLevel = nullptr;
    std::atomic<float>* ambienceLevel = nullptr;
    std::atomic<float>* space = nullptr;

    dsp::ForgottenPiano piano;
    std::array<Slot, instrumentCount> slots;
    std::atomic<float>* pianoSettings[5] {};
    dsp::FadedSynth synth;
    std::atomic<float>* synthSettings[8] {};
    dsp::GlassMemories glass;
    std::atomic<float>* glassSettings[5] {};
    dsp::RoomTone roomTone;
    dsp::Space room;
    Smooth powerGain, masterGain, instrumentGain, ambienceGain, spaceMix;
    float scratchL[chunk] {}, scratchR[chunk] {}, sendL[chunk] {}, sendR[chunk] {};
    float slotL[chunk] {}, slotR[chunk] {}, slotSendL[chunk] {}, slotSendR[chunk] {};
    double instrumentSum = 0, ambienceSum = 0, outputSum = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalProcessor)
};

} // namespace liminal
