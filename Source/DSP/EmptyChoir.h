#pragma once
// EMPTY CHOIR (Milestone 1b): a wordless vocal pad by source-filter (formant) synthesis.
// Per note, four slightly detuned "singers" (band-limited glottal-like sawtooths with a
// spectral tilt) and breath noise go through four formant band-passes per ear. The
// formants morph between the vowels oo - oh - ah - eh - ee (Formant), and each note's
// vowel wanders a little on its own so a held chord never sounds frozen. A gentle,
// late-starting vibrato wobbles around the pitch; nothing slides into it.
// Sources: docs/INSTRUMENT_NOTES.md (formant tables).
#include "DSP/Filters.h"
#include "DSP/Instrument.h"

namespace liminal::dsp {

class EmptyChoir final : public Instrument {
public:
    struct Settings {
        float breath = 0.3f;     // 0..1: air in the voice
        float formant = 0.3f;    // 0..1: oo .. oh .. ah .. eh .. ee
        float detune = 0.35f;    // 0..1: spread between the singers
        float width = 0.6f;      // 0..1: singers spread across the stereo field
        float darkness = 0.4f;   // 0..1: darker, softer upper formants
        float release = 2.5f;    // seconds
    };
    static constexpr int polyphony = 8;
    static constexpr int slots = polyphony + 4;
    static constexpr int singers = 4;
    static constexpr int formants = 4;

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
        std::array<float, singers> phase {}, inc {}, offset {}, panL {}, panR {};
        std::array<Wander, singers> drift {};
        Wander vowel;
        Svf formL[formants], formR[formants];
        float formGain[formants] {};
        OnePole tiltL, tiltR, darkL, darkR;
        float amp = 0, velocity = 0, gain = 0, age = 0, vibPhase = 0, vibRate = 5;
        Random noise;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.amp * v.gain * v.fade; }

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
};

} // namespace liminal::dsp
