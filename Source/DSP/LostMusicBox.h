#pragma once
// LOST MUSIC BOX (Milestone 1b): a worn comb plucked by a pin. Modal synthesis of a
// clamped-free steel tooth (bending modes at 1 : 6.27 : 17.55 of the fundamental, plus a
// faint 2nd and 3rd harmonic from the tooth's large swing), a pin click at the pluck and
// the clockwork's own ticking. Wear gives each tooth a fixed mistuning, an uneven level
// and an uneven ring (some teeth are dull); Pitch Drift is the spring motor running
// unevenly: every note wanders together, slowly, around the pitch. Teeth have no dampers:
// a note rings to its natural end, key-up or not. Sources: docs/INSTRUMENT_NOTES.md.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"

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
    void noteOff(int note) override { pool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); }
    void allSoundOff() override { pool.allSoundOff(sr); }
    void render(float* left, float* right, int n) override;
    int activeVoices() const override { return pool.active(); }

private:
    static constexpr int controlPeriod = 64;
    struct Voice : VoiceBase {
        ModeBank<8> modes;
        float click = 0, clickDecay = 0, level = 0, ratio = 1;
        Svf clickTone;
        Random noise;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.level * v.fade; }

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
