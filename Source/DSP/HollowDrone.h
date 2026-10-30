#pragma once
// HOLLOW DRONE (Milestone 1b): a sustained, slowly evolving instrument for low notes,
// chords and pads. Per note: a stack of free-running oscillators (Density: 2 to 7) that
// morph from soft near-sines to bright sawtooths (Harmonics, which also brings in a fifth
// and an octave), a sub-octave and a two-octave-down sine (Sub), each oscillator drifting
// on its own (Instability), all through a resonant low-pass per ear whose cutoff and the
// oscillators' stereo positions move slowly (Movement). Long attack and release.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"

namespace liminal::dsp {

class HollowDrone final : public Instrument {
public:
    struct Settings {
        float density = 0.5f;
        float instability = 0.35f;
        float harmonics = 0.4f;
        float sub = 0.4f;
        float movement = 0.4f;
        float attack = 1.2f;     // seconds
        float release = 4.0f;    // seconds
    };
    static constexpr int polyphony = 8;
    static constexpr int slots = polyphony + 4;
    static constexpr int oscillators = 9;   // up to 7 stacked + fifth + octave

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
    static constexpr int controlPeriod = 32;
    struct Voice : VoiceBase {
        std::array<float, oscillators> phase {}, inc {}, gain {}, gl {}, gr {}, panPhase {};
        std::array<Wander, oscillators> drift {};
        float subPhase = 0, subInc = 0, sub2Phase = 0, sub2Inc = 0;
        Svf filterL, filterR;
        Wander cutoffDrift;
        float amp = 0, velocity = 0, level = 0, sweep = 0;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.amp * v.level * v.fade; }

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
};

} // namespace liminal::dsp
