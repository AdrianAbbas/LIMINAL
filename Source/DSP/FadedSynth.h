#pragma once
// FADED SYNTH (Milestone 1a): a warm, worn polyphonic analog-style synthesizer.
// The reasoning and sources are in docs/INSTRUMENT_NOTES.md.
//
// Per voice: two band-limited sawtooths and a pulse with slow pulse-width modulation, each
// free-running and drifting on its own; a saturating input stage (warmth: even and odd
// harmonics) into a four-pole low-pass ladder with key tracking and its own envelope; an
// analog-style (RC) amplitude envelope. After the voices: a bucket-brigade-style stereo
// chorus whose depth follows Width, and a shared slow "tired electronics" wow.
// Glide is an option, off by default: notes start on their pitch.
//
// Milestone 8: Patch chooses one of the developer's analog-synth recordings (long, held hits of
// every key: Soft Pad by default, Pulse, Square, Cyberpunk, Static Bass; Analog = the synthesised
// voice alone). A recorded note loops its sustain, crosses into its recorded release, takes Attack,
// Release, Warmth (a darker low-pass and a little saturation), Detune (a second, detuned copy, spread
// by Width), Drift and Glide; Analog Blend mixes the synthesised voice under it. Both go through the
// same chorus and wow. Without the recordings every patch plays the synthesised voice.
#include "DSP/Instrument.h"
#include "DSP/Sampler.h"
#include <vector>

namespace liminal::dsp {

class FadedSynth final : public Instrument {
public:
    struct Settings {
        float detune = 0.35f;     // 0..1: oscillator spread, up to 24 cents
        float warmth = 0.5f;      // 0..1: darker filter, more saturation
        float drift = 0.3f;       // 0..1: unstable tuning and filter
        float attack = 0.008f;    // seconds
        float release = 0.6f;     // seconds
        float width = 0.5f;       // 0..1: voice spread and chorus
        bool glide = false;
        float glideTime = 0.12f;  // seconds
        int patch = 1;            // Milestone 8: 0 Analog, 1.. the recorded patches (params::synthPatchSets)
        float blend = 0.0f;       // the synthesised voice under a recorded patch
    };
    static constexpr int polyphony = 12;
    // The recorded patches (params::synthPatchNames), the sample sets they play.
    static constexpr const char* patchSets[] { nullptr, "mlg-softpad-long", "mlg-pulse-long", "mlg-square-long", "mlg-cyberpunk-long", "mlg-staticbass-long" };
    static constexpr int slots = polyphony + 4;

    void setSettings(const Settings& s) { settings = s; }
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override { pool.noteOff(note); rpool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); rpool.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); rpool.allNotesOff(); }
    void allSoundOff() override;
    void render(float* left, float* right, int n) override;
    // The host jumped: the shared wow, the chorus sweep and the voice count (which side a note
    // pans to) start again from the new position, so a replay matches.
    // While notes sound only the randomness is reseeded: the wow and the chorus sweep carry on (a
    // jump in either is a click or a pitch step in a held note).
    void reseed(std::uint32_t s) override {
        rng.state = s | 1u;
        if (activeVoices() > 0) return;
        wow = {};
        wowCents = 0;
        chorusPhase = float(s >> 8) * (1.0f / 16777216.0f);
        pool.counter = rpool.counter = 0;
        lastHz = 0;
    }
    int activeVoices() const override { return pool.active() + rpool.active(); }
    int heldVoices() const override { return pool.held() + rpool.held(); }
    static constexpr float recordedTrimDb = 13.2f;   // level-matched to the stand-in (tests: recorded)

private:
    static constexpr int controlPeriod = 16;
    struct Voice : VoiceBase {
        float hz = 0, targetHz = 0;
        std::array<float, 3> phase {}, inc {};
        float pulseWidth = 0.5f, lfoPhase = 0, lfoRate = 0.3f;
        std::array<Wander, 3> oscDrift {};
        Wander cutoffDrift;
        VoiceShape shape;   // Liminality
        float ladder[4] {};
        float g = 0, gStep = 0;   // filter coefficient, ramped across the control period
        float amp = 0, filterEnv = 0;
        bool attacking = true, filterAttacking = true;
        float velocity = 0, velocityGain = 0, panL = 0.7f, panR = 0.7f;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.amp * v.velocityGain * v.fade; }

    struct RVoice : VoiceBase {   // Milestone 8: a recorded patch's note
        Reader a, b;               // the note, and its detuned copy (Detune)
        float hz = 0, targetHz = 0, amp = 0, gain = 1, velocity = 0;
        bool attacking = true;
        float aL = 0.7f, aR = 0.7f, bL = 0.7f, bR = 0.7f, drive = 1;
        OnePole lp1, lp2;
        Wander drift;
        VoiceShape shape;
    };
    bool startRecorded(int note, float velocity, float glideFromHz);
    void controlRecorded(RVoice&, float tick);
    float level(const RVoice& v) const { return v.amp * v.gain * v.fade; }
    VoicePool<RVoice, slots> rpool;

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f, lastHz = 0;
    int clock = 0;
    Wander wow;
    float wowCents = 0;

    // Chorus: two modulated delay lines (left/right), darkened like a bucket brigade.
    std::vector<float> lineL, lineR;
    int write = 0;
    float chorusPhase = 0, chorusMix = 0.3f, chorusSpread = 0.5f, wetL = 0, wetR = 0;
};

} // namespace liminal::dsp
