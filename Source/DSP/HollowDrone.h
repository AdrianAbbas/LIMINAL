#pragma once
// HOLLOW DRONE (Milestone 1b): a sustained, slowly evolving instrument for low notes,
// chords and pads. Per note: a stack of free-running oscillators (Density: 2 to 7) that
// morph from soft near-sines to bright sawtooths (Harmonics, which also brings in a fifth
// and an octave), a sub-octave and a two-octave-down sine (Sub), each oscillator drifting
// on its own (Instability), all through a resonant low-pass per ear whose cutoff and the
// oscillators' stereo positions move slowly (Movement). Long attack and release.
//
// Milestone 8: with the developer's recordings a note is four held recordings blended into one
// hollow, breathing drone: his voice's drones (the hummed / 'ooo' G2, the 'aah' A3; the nearest
// within a fifth), the violin's bowed open strings (an octave or two up: air, more with
// Harmonics), the analog synth's soft pad on the note, and its static-y bass an octave below
// (Sub). Density brings the voice and the violin forward, Movement breathes between the voice
// and the pad and circles them through the stereo field, Instability lets each drift on its own,
// Harmonics opens the low-pass. The oscillators are the stand-in.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "DSP/Sampler.h"

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
    void noteOff(int note) override { pool.noteOff(note); rpool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); rpool.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); rpool.allNotesOff(); }
    void allSoundOff() override { pool.allSoundOff(sr); rpool.allSoundOff(sr); }
    void render(float* left, float* right, int n) override;
    void reseed(std::uint32_t s) override { rng.state = s | 1u; }
    int activeVoices() const override { return pool.active() + rpool.active(); }
    static constexpr float recordedTrimDb = 15.1f;   // level-matched to the stand-in (tests: recorded)

private:
    static constexpr int controlPeriod = 32;
    struct Voice : VoiceBase {
        std::array<float, oscillators> phase {}, inc {}, gain {}, gl {}, gr {}, panPhase {};
        std::array<Wander, oscillators> drift {};
        float subPhase = 0, subInc = 0, sub2Phase = 0, sub2Inc = 0;
        Svf filterL, filterR;
        Wander cutoffDrift;
        VoiceShape shape;   // Liminality
        float amp = 0, velocity = 0, level = 0, sweep = 0;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.amp * v.level * v.fade; }

    static constexpr int layers = 4;   // voice, violin, pad, bass
    struct RVoice : VoiceBase {        // Milestone 8: a recorded drone
        std::array<Reader, layers> r {};
        std::array<float, layers> base {}, offset {}, panPhase {}, gl {}, gr {};
        std::array<Wander, layers> drift {};
        OnePole lpL, lpR;
        float amp = 0, gain = 1, velocity = 0, breath = 0;
        VoiceShape shape;
    };
    bool startRecorded(int note, float velocity);
    void controlRecorded(RVoice&, float tick);
    float level(const RVoice& v) const { return v.amp * v.gain * v.fade; }
    VoicePool<RVoice, slots> rpool;

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
};

} // namespace liminal::dsp
