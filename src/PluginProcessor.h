#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Engine.h"
#include "presets/Factory.h"
class LiminalProcessor final : public juce::AudioProcessor
{
  public:
    LiminalProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LIMINAL"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    liminal::Settings settings() const;
    void loadSettings(const liminal::Settings &);
    void loadFactory(int);
    void captureScene(bool b);
    void disableScenes() { scenesEnabled.store(false); }
    juce::Result savePreset(const juce::File &);
    juce::Result loadPreset(const juce::File &);
    juce::String presetName() const;
    void resetEvolution() { resetRequested.store(true); }
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> meterL{0}, meterR{0};

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    bool restoreState(const void *, int);
    liminal::Engine engine;
    std::array<std::atomic<float> *, liminal::count> raw{};
    // Scene slots use atomics: the audio callback never waits for UI/state I/O.
    std::array<std::atomic<float>, liminal::count> sceneA{}, sceneB{};
    std::atomic<bool> scenesEnabled{false}, haveScene{false}, resetRequested{false};
    juce::String currentPreset = "Initial";
    mutable juce::CriticalSection metadataLock;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalProcessor)
};
