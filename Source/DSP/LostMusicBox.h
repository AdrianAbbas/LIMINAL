#pragma once
// LOST MUSIC BOX (Milestone 1b): a worn comb plucked by a pin. Modal synthesis of a
// clamped-free steel tooth (bending modes at 1 : 6.27 : 17.55 of the fundamental, plus a
// faint 2nd and 3rd harmonic from the tooth's large swing), a pin click at the pluck and
// the clockwork's own ticking. Wear gives each tooth a fixed mistuning, an uneven level
// and an uneven ring (some teeth are dull); Pitch Drift is the spring motor running
// unevenly: every note wanders together, slowly, around the pitch. Teeth have no dampers:
// a note rings to its natural end, key-up or not. Sources: docs/INSTRUMENT_NOTES.md.
//
// Milestone 8: with the developer's recordings the tooth is his kalimba (three crossfaded
// dynamics; small, bright tines), with the top of his piano under the high notes as a second,
// glassy layer (more of it with Brightness, all of it above the kalimba's range), each note a
// little late or early as a worn mechanism plays it (Mechanism), the pin's soft click and the
// clockwork as before. The modal tooth is the stand-in.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "DSP/Sampler.h"

namespace liminal::dsp {

class LostMusicBox final : public Instrument {
public:
    struct Settings {
        float wear = 0.4f;
        float drift = 0.3f;        // Pitch Drift
        float mechanism = 0.4f;    // clicks and clockwork
        float brightness = 0.5f;
        float decay = 0.5f;        // 0..1: 0.5 s .. 6 s ring around A5
    };
    static constexpr int polyphony = 20;
    static constexpr int slots = polyphony + 6;

    void setSettings(const Settings& s) { settings = s; }
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override { pool.noteOff(note); rpool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); rpool.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); rpool.allNotesOff(); }
    void allSoundOff() override { pool.allSoundOff(sr); rpool.allSoundOff(sr); }
    void render(float* left, float* right, int n) override;
    // The host jumped: the spring motor and the clockwork start again from the new position.
    void reseed(std::uint32_t s) override {
        rng.state = s | 1u;
        tickRng.state = (s * 2654435761u) | 1u;
        motor = {};
        flutter = {};
        tickCountdown = 0;
    }
    int activeVoices() const override { return pool.active() + rpool.active(); }
    static constexpr float recordedTrimDb = 1.0f;   // level-matched to the stand-in (tests: recorded)

private:
    static constexpr int controlPeriod = 64;
    struct Voice : VoiceBase {
        ModeBank<8> modes;
        float click = 0, clickDecay = 0, level = 0, ratio = 1;
        VoiceShape shape;   // Liminality
        Svf clickTone;
        Random noise;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.level * v.fade; }

    struct RVoice : VoiceBase {   // Milestone 8: a recorded tine
        Reader a, b, top;          // two kalimba dynamics, the piano's top
        Tilt tone;
        float panL = 0.7f, panR = 0.7f, env = 1, decayMul = 1, cents = 0, level = 0;
        float click = 0, clickDecay = 0;
        int delay = 0;             // the worn mechanism plays it late
        Svf clickTone;
        Random noise;
        VoiceShape shape;
    };
    bool startRecorded(int note, float velocity);
    float level(const RVoice& v) const { return v.level * v.fade; }
    VoicePool<RVoice, slots> rpool;

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng, tickRng;
    float bend = 1.0f;
    int clock = 0;
    Wander motor, flutter;
    float motorRatio = 1.0f;
    // The clockwork: soft ticks while the box plays.
    float tickEnv = 0, tickDecay = 0, tickCountdown = 0, busy = 0;
    Svf tickTone;
};

} // namespace liminal::dsp
