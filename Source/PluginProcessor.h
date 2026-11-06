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
#include "DSP/Library.h"
#include "DSP/TapeWarble.h"
#include "DSP/Liminality.h"
#include "DSP/LevelMatch.h"
#include "Parameters.h"
#include "Presets.h"
#include "Brand.h"
#include "SceneMeters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>

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
    // (The viewfinder's part, SceneMeters, is shared with TAPEWORN FX.)
    struct Meters : SceneMeters {
        std::atomic<int> voices { 0 };
        std::atomic<int> held { 0 };    // voices a key or the pedal still holds (hung-note tests)
    } meters;

    // Scenes A/B (Milestone 5). Storing is done on the message thread; the audio thread
    // reads the stored values (one atomic per parameter).
    void storeScene(int which);              // the current settings become scene A (0) or B (1)
    bool sceneStored(int which) const { return stored[std::size_t(which & 1)].load(); }
    void requestEvolutionReset() { evolutionResetRequest.store(true); }
    std::atomic<float> uiScale { 1.0f };   // the editor's size (saved with the song)
    // The VHS look (2026-10-31): how hard the picture's tape is glitching right now (0..1), set by
    // the editor each frame; the optional audio warble (parameter vhs_warble) follows it.
    std::atomic<float> vhsWarble { 0.0f };

    // Presets (Milestone 6). Message thread only.
    void applyPreset(const presets::Preset&);
    void resetToDefaults();
    // INIT (2026-11-06): no preset - every parameter of the sound at its default, as in a new
    // instance (each slot its own instrument, LIMINALITY at its default, the default place and
    // Character, the default effects); the evolution starts again. The viewing / listening
    // preferences (POWER, the VHS look and its warble) and the song's stored scenes A/B are kept.
    void initPreset();
    static constexpr const char* initName = "INIT";
    // Every preset load (INIT, factory, your own, a host program) can be undone and redone.
    bool undoPreset();
    bool redoPreset();
    bool canUndoPreset() const { return presetUndo.canUndo(); }
    bool canRedoPreset() const { return presetUndo.canRedo(); }
    juce::String undoPresetName() const { return presetUndo.getUndoDescription(); }   // what UNDO goes back from
    juce::String redoPresetName() const { return presetUndo.getRedoDescription(); }
    juce::String presetName() const { return currentPresetName; }
    static juce::File userPresetFolder();          // Elsewhere Audio/TAPEWORN/Presets
    static juce::File legacyUserPresetFolder();    // Veil Audio/LIMINAL/Presets (read: carried over)
    static void carryOverLegacyPresets();
    juce::File saveUserPreset(const juce::String& name);   // returns the file (or {} on failure)
    bool loadUserPreset(const juce::File&);
    juce::Array<juce::File> userPresets() const;
    const dsp::EnvironmentChain& placeNow() const { return environments.current(); }   // audio-thread state (tests)
    const dsp::Liminality& liminalityOf(int instrument) const { return slots[std::size_t(instrument)].lim; }   // (tests)
    // The instrument library (2026-11-02): which of the twenty a slot is playing now (audio thread's view; tests).
    int playingIn(int slot) const { return slots[std::size_t(slot)].playing; }
    bool slotRecorded(int slot) const;   // whether the slot's instrument plays recordings now (tests)

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
    // What a slot can play (2026-11-02): an instance of each original instrument and three of the library
    // (the one playing and up to two fading out after changes), preallocated (~1 MB a slot).
    struct Kit {
        dsp::ForgottenPiano piano;
        dsp::FadedSynth synth;
        dsp::GlassMemories glass;
        dsp::EmptyChoir choir;
        dsp::LostMusicBox box;
        dsp::HollowDrone drone;
        std::array<dsp::Library, 3> library;
        dsp::Instrument* original(int k) {
            switch (k) {
            case 0: return &piano;
            case 1: return &synth;
            case 2: return &glass;
            case 3: return &choir;
            case 4: return &box;
            default: return &drone;
            }
        }
        template <class F> void forEach(F f) {
            for (int k = 0; k < 6; ++k) f(*original(k));
            for (auto& l : library) f(l);
        }
    };
    // One playable instrument with its on/off, level and distance (before the room).
    struct Slot {
        dsp::Instrument* instrument = nullptr;   // what the slot plays now
        // What it played before a change: rendered until its notes have faded, then its own tail (an
        // echo, a chorus) for a moment more, faded out (never cut).
        struct Fading { dsp::Instrument* instrument = nullptr; int tail = -1, tailLength = 1; };
        std::array<Fading, 2> fading {};
        bool isFading(const dsp::Instrument* in) const { return std::any_of(fading.begin(), fading.end(), [&](const Fading& f) { return f.instrument == in; }); }
        int fadingVoices() const { int n = 0; for (const auto& f : fading) if (f.instrument != nullptr) n += f.instrument->activeVoices(); return n; }
        int playing = -1;                        // which of the twenty (params::libraryNames)
        std::atomic<float>* type = nullptr;
        std::array<std::atomic<float>*, params::lib::count> libParams {};
        std::unique_ptr<Kit> kit = std::make_unique<Kit>();
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* distance = nullptr;   // optional
        dsp::Distance stage;
        dsp::Liminality lim;                                            // Liminality (2026-11-01)
        std::array<std::atomic<float>*, dsp::lim::count> limParams {};
        dsp::EffectRack rack;                                           // Milestone 3
        dsp::EffectSettings settings;
        std::array<std::atomic<float>*, params::fx::count> fx {};      // in params::fx::all order
        std::atomic<float>* keyLow = nullptr;
        std::atomic<float>* keyHigh = nullptr;
        std::atomic<float>* velocityVolume = nullptr;
        std::atomic<float>* velocityFilter = nullptr;
        Smooth gain;
        Smooth trim;    // the room's level match for the instrument it plays (DSP/LevelMatch.h), slower
        Smooth share;   // how much of it the place's signature takes (smoothed: no jumps when its effects change)
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
    float levelTrim(const Slot&, int place, int character) const;
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

    std::array<Slot, instrumentCount> slots;
    // The original six instruments' own controls (shared by every slot that holds that instrument).
    std::atomic<float>* pianoSettings[5] {};
    std::atomic<float>* synthSettings[10] {};
    std::atomic<float>* glassSettings[5] {};
    std::atomic<float>* choirSettings[6] {};
    std::atomic<float>* boxSettings[5] {};
    std::atomic<float>* droneSettings[7] {};
    void selectInstrument(Slot&, int which);   // the slot's type changed: swap (the old one fades out)
    double preparedRate = 48000.0;
    std::atomic<float>* soloParam = nullptr;
    dsp::CaptureBank captures;   // the developer's effect captures (built in prepareToPlay)
    dsp::Environments environments;
    std::atomic<float>* environmentChoice = nullptr;
    std::atomic<float>* environmentAmount = nullptr;
    std::atomic<float>* farAway = nullptr;
    std::atomic<float>* warbleOn = nullptr;
    std::atomic<float>* liminalityMacro = nullptr;
    std::uint64_t limClock = 0;   // Liminality's timeline: the host's position while it plays
    bool hostWasPlaying = false;
    static constexpr double relocateTolerance = 0.1;   // seconds of disagreement that are only a host's rounding
    void updateLiminality(int place, int character);
    dsp::TapeWarble warble;
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
    // A preset load as one undoable step: every parameter's value (0..1) and the preset's name.
    struct Snapshot { std::vector<float> values; juce::String name; };
    Snapshot snapshot() const;
    void restore(const Snapshot&);
    void undoable(const juce::String& name, const std::function<void()>& load);
    void applyPresetNow(const presets::Preset&);
    juce::UndoManager presetUndo { 0, 64 };
    Smooth presenceDry, presenceRoom;
    Smooth powerGain, masterGain, instrumentGain, ambienceGain, spaceMix;
    float scratchL[chunk] {}, scratchR[chunk] {}, sendL[chunk] {}, sendR[chunk] {};
    float ambienceGains[chunk] {}, spaces[chunk] {}, mixL[chunk] {}, mixR[chunk] {}, dryL[chunk] {}, dryR[chunk] {}, ambiencePower[chunk] {};
    float slotL[chunk] {}, slotR[chunk] {}, slotSendL[chunk] {}, slotSendR[chunk] {}, slotEchoL[chunk] {}, slotEchoR[chunk] {};
    float echoInL[chunk] {}, echoInR[chunk] {};
    double instrumentSum = 0, ambienceSum = 0, outputSum = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalProcessor)
};

} // namespace liminal
