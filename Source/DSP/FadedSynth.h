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
#include "DSP/Instrument.h"
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
    };
    static constexpr int polyphony = 12;
    static constexpr int slots = polyphony + 4;

    void setSettings(const Settings& s) { settings = s; }
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override { pool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); }
    void allSoundOff() override;
    void render(float* left, float* right, int n) override;
    int activeVoices() const override { return pool.active(); }

private:
    static constexpr int controlPeriod = 16;
    struct Voice : VoiceBase {
        float hz = 0, targetHz = 0;
        std::array<float, 3> phase {}, inc {};
        float pulseWidth = 0.5f, lfoPhase = 0, lfoRate = 0.3f;
        std::array<Wander, 3> oscDrift {};
        Wander cutoffDrift;
        float ladder[4] {};
        float g = 0, gStep = 0;   // filter coefficient, ramped across the control period
        float amp = 0, filterEnv = 0;
        bool attacking = true, filterAttacking = true;
        float velocity = 0, velocityGain = 0, panL = 0.7f, panR = 0.7f;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.amp * v.velocityGain * v.fade; }

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
