#pragma once
// FORGOTTEN PIANO (Milestone 1a): an old, slightly out-of-tune upright, modelled modally.
// The acoustics behind every number are in docs/INSTRUMENT_NOTES.md.
//
// Each note is up to 32 stiff-string partials (f_n = n f0 sqrt(1 + B n^2), B by register).
// Each partial is the coupled unison's normal modes: a "prompt" mode that decays fast and
// two slow "aftersound" modes a little apart (Weinreich's two-stage decay with beating).
// The hammer's contact time shapes the spectrum (shorter when struck harder and higher up),
// the strike point combs it, felt dampers stop it on release (none in the top octave and a
// half), and the sustain pedal lifts them and lets a bank of open strings ring along.
// Mechanical noise: key-bed thump, hammer knock, damper and pedal sounds.
// Age makes it a forgotten piano: wider unisons, keys out of tune with each other, uneven
// voicing and decay, worn dampers, a duller soundboard, and a slight pitch wobble.
#include "DSP/Instrument.h"
#include <vector>

namespace liminal::dsp {

class ForgottenPiano final : public Instrument {
public:
    struct Settings {
        float age = 0.35f;        // 0..1
        float detune = 0.25f;     // 0..1: unison spread and key-to-key tuning
        float softness = 0.3f;    // 0..1: hammer felt (soft = longer contact, darker)
        float noise = 0.4f;       // 0..1: mechanical noise
        float decay = 0.5f;       // 0..1: 0.35x .. 2.8x the natural sustain
    };
    static constexpr int polyphony = 20;
    static constexpr int slots = polyphony + 6;
    static constexpr int maxPartials = 32;
    static constexpr int beatingPartials = 12;   // partials with the second aftersound mode

    void setSettings(const Settings& s) { settings = s; }
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override;
    void sustain(bool down) override;
    void pitchBend(int value14bit) override;
    void allNotesOff() override;
    void allSoundOff() override;
    void render(float* left, float* right, int n) override;
    int activeVoices() const override { return pool.active(); }

private:
    static constexpr int controlPeriod = 128;
    struct Voice : VoiceBase {
        // P prompt modes, then P aftersound modes, then up to 12 second aftersound modes.
        ModeBank<2 * maxPartials + beatingPartials> modes;
        bool damped = false, hasDamper = true;
        float attack = 0, attackStep = 0;
        float wobbleCents = 0, appliedRatio = 1;
        float level = 0;
        Wander wobble;
    };
    struct Event {   // a short mechanical sound: noise through a resonator
        Random noise;
        float amp = 0, decay = 0, b1 = 0, b2 = 0, y1 = 0, y2 = 0, lp = 0, lpCoef = 1, panL = 0.7f, panR = 0.7f;
    };
    void start(Voice&, int note, float velocity);
    void control(Voice&);
    void addEvent(float hz, float q, float seconds, float knock, float rustle, float brightness, float pan);
    float level(const Voice& v) const { return v.level * v.fade; }

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;

    std::array<Event, 16> events {};
    int nextEvent = 0;

    // Open strings ringing in sympathy while the pedal is down.
    struct Comb { std::vector<float> line; int pos = 0; float lp = 0, feedback = 0.99f; };
    std::array<Comb, 6> sympathetic;
    float pedalMix = 0, pedalTarget = 0, pedalCoef = 0;
};

} // namespace liminal::dsp
