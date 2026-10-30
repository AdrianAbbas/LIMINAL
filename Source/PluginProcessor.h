#pragma once
#include "DSP/Environment.h"
#include "DSP/Distance.h"
#include "DSP/Effects.h"
#include "DSP/FadedSynth.h"
#include "DSP/ForgottenPiano.h"
#include "DSP/GlassMemories.h"
#include "DSP/EmptyChoir.h"
#include "DSP/LostMusicBox.h"
#include "DSP/HollowDrone.h"
#include "Parameters.h"
#include "Presets.h"
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
    double getTailLengthSeconds() const override { return 8.0; }

    // Host programs: "Init" and the twenty factory scenes (GarageBand's preset menu).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;

    // Read by the editor (message thread); written once per block by the audio thread.
    struct Meters {
        std::atomic<float> instrument { 0 }, ambience { 0 }, output { 0 }; // RMS, linear
        std::atomic<int> voices { 0 };
        std::atomic<int> place { 0 };   // the environment being heard
        std::atomic<int> character { 0 };
        std::atomic<float> activity { 0 };   // evolution movement + events (0..1), for the interface
        std::atomic<int> events { 0 };      // the place's incidental sounds so far (the lights answer them)
        std::atomic<int> notes { 0 };       // note-ons so far (the lights answer them)
        std::atomic<float> velocity { 0 };  // the latest note-on's velocity (0..1): how much the lights swell
        std::atomic<float> lamp { 1 };      // the failing tube's lowest brightness since the editor took it (1 = steady)
        std::atomic<int> flickers { 0 };    // its stutters so far: the picture's tube flickers with the hum
    } meters;

    // Scenes A/B (Milestone 5). Storing is done on the message thread; the audio thread
    // reads the stored values (one atomic per parameter).
    void storeScene(int which);              // the current settings become scene A (0) or B (1)
    bool sceneStored(int which) const { return stored[std::size_t(which & 1)].load(); }
    void requestEvolutionReset() { evolutionResetRequest.store(true); }
    std::atomic<float> uiScale { 1.0f };   // the editor's size (saved with the song)

    // Presets (Milestone 6). Message thread only.
    void applyPreset(const presets::Preset&);
    void resetToDefaults();
    juce::String presetName() const { return currentPresetName; }
    static juce::File userPresetFolder();
    juce::File saveUserPreset(const juce::String& name);   // returns the file (or {} on failure)
    bool loadUserPreset(const juce::File&);
    juce::Array<juce::File> userPresets() const;
    const dsp::EnvironmentChain& placeNow() const { return environments.current(); }   // audio-thread state (tests)

    static constexpr int stateVersion = 2;   // 2: scenes A/B saved alongside the parameters

private:
    struct Smooth {
        float current = 0, target = 0, coef = 0.01f;
        void reset(float v) { current = target = v; }
        float next() {
            current += coef * (target - current);
            if (std::abs(target - current) < 1.0e-6f) current = target;
            return current;
        }
    };
    // One playable instrument with its on/off, level and distance (before the room).
    struct Slot {
        dsp::Instrument* instrument = nullptr;
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* distance = nullptr;   // optional
        dsp::Distance stage;
        dsp::EffectRack rack;                                           // Milestone 3
        dsp::EffectSettings settings;
        std::array<std::atomic<float>*, params::fx::count> fx {};      // in params::fx::all order
        std::atomic<float>* keyLow = nullptr;
        std::atomic<float>* keyHigh = nullptr;
        std::atomic<float>* velocityVolume = nullptr;
        std::atomic<float>* velocityFilter = nullptr;
        Smooth gain;
        bool running = false;
        bool muted = false;   // another instrument is soloed
        bool enabled() const { return on->load() > 0.5f; }
        float targetGain() const { return enabled() && !muted ? params::levelGain(level->load()) : 0.0f; }
    };
    static constexpr int instrumentCount = params::instrumentCount;
    void updateSolo();
    void handleMidi(const juce::MidiMessage&);
    void updateSettings();
    dsp::EffectSettings effectSettings(const Slot&) const;
    double hostBpm = 120.0;
    void renderSegment(float* left, float* right, int n);
    void controlTick();
    static constexpr int controlPeriod = 64;
    std::uint64_t samplePosition = 0;
    dsp::EnvironmentChain::Live liveNow;
    dsp::EffectLive effectLive;
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
    dsp::EmptyChoir choir;
    std::atomic<float>* choirSettings[6] {};
    dsp::LostMusicBox box;
    std::atomic<float>* boxSettings[5] {};
    dsp::HollowDrone drone;
    std::atomic<float>* droneSettings[7] {};
    std::atomic<float>* soloParam = nullptr;
    dsp::Environments environments;
    std::atomic<float>* environmentChoice = nullptr;
    std::atomic<float>* environmentAmount = nullptr;
    std::atomic<float>* farAway = nullptr;
    dsp::Place selectedPlace() const;
    int selectedCharacter() const;
    // Every parameter the DSP reads goes through this mirror: the live value, or, while Scene
    // Morph is on, the morph between the stored scenes.
    std::atomic<float>* mirrorOf(const juce::String& id);
    void updateMirror();
    struct Mirrored { juce::RangedAudioParameter* parameter; std::atomic<float>* raw; bool discrete; int role; int partner; };
    std::vector<Mirrored> mirrored;
    std::unique_ptr<std::atomic<float>[]> mirror;
    std::array<std::unique_ptr<std::atomic<float>[]>, 2> scenes;
    std::array<std::atomic<bool>, 2> stored { false, false };
    std::atomic<float>* sceneOn = nullptr;
    std::atomic<float>* sceneMorph = nullptr;
    std::atomic<float>* characterChoice = nullptr;
    std::atomic<float>* evolutionRate = nullptr;
    std::atomic<float>* evolutionFreeze = nullptr;
    std::atomic<float>* evolutionSeed = nullptr;
    std::atomic<float>* macroParams[4] {};
    std::atomic<float>* qualityChoice = nullptr;
    int qualityNow = -1;
    void applyQuality();
    std::atomic<bool> evolutionResetRequest { false };
    float lastSeed = -1;
    int currentProgram = 0;
    juce::String currentPresetName { "INIT" };
    void setParameter(const juce::String& id, float value);
    Smooth presenceDry, presenceRoom;
    Smooth powerGain, masterGain, instrumentGain, ambienceGain, spaceMix;
    float scratchL[chunk] {}, scratchR[chunk] {}, sendL[chunk] {}, sendR[chunk] {};
    float ambienceGains[chunk] {}, spaces[chunk] {}, mixL[chunk] {}, mixR[chunk] {}, ambiencePower[chunk] {};
    float slotL[chunk] {}, slotR[chunk] {}, slotSendL[chunk] {}, slotSendR[chunk] {}, slotEchoL[chunk] {}, slotEchoR[chunk] {};
    float echoInL[chunk] {}, echoInR[chunk] {};
    double instrumentSum = 0, ambienceSum = 0, outputSum = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalProcessor)
};

} // namespace liminal
