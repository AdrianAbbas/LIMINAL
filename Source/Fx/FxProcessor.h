#pragma once
// TAPEWORN FX (2026-11-06): TAPEWORN's world for any audio. The instrument's places - their rooms
// with the captured spaces and convolution, their signatures, Characters, evolution, the macros,
// the cross-fade between places - and its LIMINALITY stage, adapted for audio (Fx/FxLiminality),
// on whatever comes in: guitar, vocals, synths, drums, a whole mix. Everything except the
// instruments themselves, from the same code (DSP/Environment, DSP/Effects, DSP/Signature,
// DSP/Captures, DSP/Liminality; the editor's UI/Viewfinder and UI/Widgets).
//
//   input -> Input level -> LIMINALITY (pitch line, Ghost, Warble, Wear, Unevenness, Fade)
//         -> the place's processing (EffectRack: Colour, its default effects; sends with Timing)
//         -> the place (EnvironmentChain: signature, echo, room, captures, room sound)
//         -> the room's level match -> Mix with the input (delayed by the latency) -> Output
//         -> Bypass (a 25 ms cross-fade to the input, delayed by the same latency)
//
// Latency: the LIMINALITY pitch line reads a delay line around a fixed centre (20 ms), so the
// shifted sound stays in time with the track on average; the dry share and Bypass are delayed by
// exactly the same, and that is what the plug-in reports (getLatencySamples). Mono and stereo, in
// = out. Real-time safe: everything is allocated in prepareToPlay.
#include "DSP/Effects.h"
#include "DSP/Environment.h"
#include "Fx/FxLiminality.h"
#include "Fx/FxParameters.h"
#include "Fx/FxPresets.h"
#include "Brand.h"
#include "SceneMeters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <functional>
#include <vector>

namespace liminal::fx {

class FxProcessor final : public juce::AudioProcessor {
public:
    FxProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    // Host programs: "Init" and the factory presets.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    SceneMeters meters;   // (instrument: the input's level, ambience: the room sound's, output)
    std::atomic<float> uiScale { 1.0f };
    std::atomic<float> vhsWarble { 0.0f };   // the picture's tape struggling (the optional audio warble follows)

    static constexpr double latencySeconds = 0.02;
    static int latencyFor(double sampleRate) { return int(std::lround(latencySeconds * sampleRate)); }

    // Presets, A/B and undo (message thread).
    static constexpr const char* initName = "INIT";
    void initPreset();
    void applyPreset(const presets::Preset&);
    bool undo();
    bool redo();
    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }
    juce::String presetName() const { return currentPresetName; }
    static juce::File userPresetFolder();   // Windward Audio/TAPEWORN FX/Presets
    juce::File saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::File&);
    juce::Array<juce::File> userPresets() const;
    // A/B: two complete settings to compare. The one not shown keeps what it held; switching is
    // undoable. copyAB() copies the shown one into the other.
    int abShown() const { return ab; }
    void showAB(int which);
    void copyAB();

    // For tests.
    const AudioLiminality& liminalityStage() const { return lim; }
    const dsp::EnvironmentChain& placeNow() const { return environments.current(); }
    float levelTrimDb() const { return trimDbNow; }      // the room's calibrated trim (Fx/FxLevelMatch.h)
    float levelMatchDb() const { return juce::Decibels::gainToDecibels(matchGain); }   // the automatic make-up on top of it
    static inline bool automaticLevel = true;   // (off only while calibrating the table)

    static constexpr int stateVersion = 1;

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
    struct Snapshot { std::vector<float> values; juce::String name; };
    Snapshot snapshot() const;
    void restore(const Snapshot&);
    void undoable(const juce::String& name, const std::function<void()>& change);
    void resetToDefaults();
    void setParameter(const juce::String& id, float value);
    std::atomic<float>* raw(const juce::String& id) { return parameters.getRawParameterValue(id); }

    void controlTick();
    void renderSegment(const float* inL, const float* inR, float* outL, float* outR, int n, bool mono);
    void updateBlockSettings();
    dsp::Place selectedPlace() const;
    int selectedCharacter() const;

    double rate = 48000.0;
    int latency = 960;
    static constexpr int controlPeriod = 64;
    static constexpr int chunk = 64;
    std::uint64_t samplePosition = 0, limClock = 0;
    bool hostWasPlaying = false;

    juce::AudioProcessorParameter* bypassParameter = nullptr;
    std::atomic<float>* bypassOn = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* intensityParam = nullptr;
    std::atomic<float>* placeParam = nullptr;
    std::atomic<float>* characterParam = nullptr;
    std::atomic<float>* spaceParam = nullptr;
    std::atomic<float>* roomSoundParam = nullptr;
    std::atomic<float>* macroParams[4] {};
    std::atomic<float>* evolutionRate = nullptr;
    std::atomic<float>* evolutionFreeze = nullptr;
    std::atomic<float>* evolutionSeed = nullptr;
    std::atomic<float>* farAway = nullptr;
    std::atomic<float>* qualityParam = nullptr;
    std::atomic<float>* warbleOn = nullptr;
    std::atomic<float>* liminalityMacro = nullptr;
    std::array<std::atomic<float>*, dsp::lim::count> limParams {};

    dsp::CaptureBank captures;
    dsp::Environments environments;
    dsp::EffectRack rack;
    dsp::EffectSettings rackSettings;
    dsp::EffectLive effectLive;   // (glided towards its target every control tick)
    float liveGlide = 0.004f;
    dsp::EnvironmentChain::Live liveNow;
    AudioLiminality lim;
    int qualityNow = -1;
    float lastSeed = -1;
    float trimDbNow = 0;

    // The dry share and Bypass: the input delayed by the latency.
    std::vector<float> dryLineL, dryLineR;
    int dryMask = 0, dryWrite = 0;

    // Onsets (Warble and Sour Notes start from them; the lights answer them) and the input's
    // presence (the room sound plays only while something comes in).
    float envFast = 0, envSlow = 0, fastCoef = 0, slowCoef = 0, releaseCoef = 0, sinceOnset = 1;
    float presence = 0, presenceUp = 0, presenceDown = 0;

    // The level match's automatic part: the input's and the room's output loudness (K-weighted, since
    // a start, then over ~8 s), the make-up between them (within +/-12 dB), held while nothing comes in.
    std::array<dsp::Biquad, 8> kWeight {};   // ITU-R BS.1770's K-weighting (shelf, high-pass) per signal and side
    float matchGainCoef = 0, matchGainCoefFast = 0, matchGainCoefStart = 0;
    float matchCoef = 0;
    double matchIn = 0, matchOut = 0, matchSumIn = 0, matchSumOut = 0;
    float matchGain = 1, matchTarget = 1;
    std::uint64_t matchSamples = 0;
    bool levelOpen = false, levelStarting = true, levelChanged = false;
    float levelReference = 1;   // (taken on the control grid)
    float bypassPos = 0, bypassStep = 0.001f;
    int matchPlace = -1, matchCharacter = -1;
    float weigh(int which, float x) { return kWeight[std::size_t(2 * which + 1)].tick(kWeight[std::size_t(2 * which)].tick(x)); }
    Smooth inGain, outGain, mixGain, bypassGain, spaceMix, trim, roomSound, presenceDry, presenceRoom, share;
    float bufL[chunk] {}, bufR[chunk] {}, sendL[chunk] {}, sendR[chunk] {}, echoL[chunk] {}, echoR[chunk] {};
    float madeUp[chunk] {}, roomGains[chunk] {};
    float trimNow = 1;
    float limiterGain = 1, limiterAttack = 0.1f, limiterRelease = 0.0002f, limiterCeiling = 0.84f;   // (-1.5 dBFS)
    float dryL[chunk] {}, dryR[chunk] {}, mixL[chunk] {}, mixR[chunk] {}, ambGains[chunk] {}, spaces[chunk] {}, ambPower[chunk] {};
    double inSum = 0, ambSum = 0, outSum = 0;

    int currentProgram = 0;
    juce::String currentPresetName { initName };
    juce::UndoManager undoManager { 0, 64 };
    int ab = 0;
    std::array<Snapshot, 2> abStore;
    std::array<bool, 2> abStored { false, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxProcessor)
};

} // namespace liminal::fx
