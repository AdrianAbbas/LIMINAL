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
//
// Milestone 8: with the developer's recordings (his own piano, 88 keys) the voices are recorded:
// the short hits soft / medium / full as three crossfaded velocity layers, handing over (aligned
// on the waveform, level- and tone-matched to the dynamic) to the medium long sweep while the key
// is held, and that hands over to the measured natural decay (resynthesised partials) where the
// recording's damper fell. Each note is tuned to the piano's own stretch curve (overall tuning
// corrected). The modelled voices above remain the stand-in when there are no recordings.
#include "DSP/Instrument.h"
#include "DSP/Sampler.h"
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
    void reseed(std::uint32_t s) override { rng.state = s | 1u; }
    int activeVoices() const override { return pool.active() + rpool.active(); }
    static constexpr float recordedTrimDb = -2.4f;   // level-matched to the stand-in (tests: recorded)

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
        VoiceShape shape;   // Liminality
    };
    struct Event {   // a short mechanical sound: noise through a resonator
        Random noise;
        float amp = 0, decay = 0, b1 = 0, b2 = 0, y1 = 0, y2 = 0, lp = 0, lpCoef = 1, panL = 0.7f, panR = 0.7f;
    };
    void start(Voice&, int note, float velocity);
    void control(Voice&);
    void addEvent(float hz, float q, float seconds, float knock, float rustle, float brightness, float pan);
    float level(const Voice& v) const { return v.level * v.fade; }

    // A recorded note (Milestone 8).
    struct RVoice : VoiceBase {
        Reader shortA, shortB, longNote;   // two velocity layers, the long note
        ModeBank<32> tail;             // the natural decay after the recording
        bool tailOn = false;
        float tailX = 0, tailStep = 0, tailW = 0, tailGain = 0;
        int tailAt = -1;
        int handCountdown = -1, handLength = 1;
        Tilt heldTone, tone;
        float gain = 1, panL = 0.7f, panR = 0.7f;
        float env = 1, dampMul = 1, decayMul = 1;
        float pitch = 60, keyCents = 0, wobbleCents = 0, ratioNow = 1;
        float tailDecayScale = 1, damperT60 = 0.3f;
        float level = 0;
        bool damped = false, hasDamper = true;
        Wander wobble;
        VoiceShape shape;
    };
    bool startRecorded(int note, float velocity, const samples::Instrument& hits);
    void controlRecorded(RVoice&, float tick);
    void renderRecorded(float* l, float* r, float* mono, int m, bool endOfPeriod);
    float level(const RVoice& v) const { return v.level * v.fade; }
    VoicePool<RVoice, slots> rpool;

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
