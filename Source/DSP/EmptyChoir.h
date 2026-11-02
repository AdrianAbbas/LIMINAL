#pragma once
// EMPTY CHOIR (Milestone 1b): a wordless vocal pad by source-filter (formant) synthesis.
// Per note, four slightly detuned "singers" (band-limited glottal-like sawtooths with a
// spectral tilt) and breath noise go through four formant band-passes per ear. The
// formants morph between the vowels oo - oh - ah - eh - ee (Formant), and each note's
// vowel wanders a little on its own so a held chord never sounds frozen. A gentle,
// late-starting vibrato wobbles around the pitch; nothing slides into it.
// Sources: docs/INSTRUMENT_NOTES.md (formant tables).
//
// Milestone 8: with the developer's recordings, a choir remembered in an empty building: his
// soft voice sweeps (mmm - ooh - aah along Formant; the falsetto at the top) as the lead, the
// 5-take choir stacks as three more distant singers (later, quieter, darker, spread by Width,
// apart by Detune), the falsetto ooh stack an octave above as a faint ghost (Breath), and the
// G2 hum drone underneath the lowest note held. Outside the voice's range the recordings fade
// into the formant choir (the stand-in) over a fifth.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "DSP/Sampler.h"

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
    void noteOff(int note) override { pool.noteOff(note); rpool.noteOff(note); humChanged = true; }
    void sustain(bool down) override { pool.sustain(down); rpool.sustain(down); humChanged = true; }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); rpool.allNotesOff(); humChanged = true; }
    void allSoundOff() override { pool.allSoundOff(sr); rpool.allSoundOff(sr); }
    void render(float* left, float* right, int n) override;
    void reseed(std::uint32_t s) override { rng.state = s | 1u; }
    int activeVoices() const override { return pool.active() + rpool.active(); }
    static constexpr float recordedTrimDb = 9.8f;   // level-matched to the stand-in (tests: recorded)

private:
    static constexpr int controlPeriod = 32;
    struct Voice : VoiceBase {
        std::array<float, singers> phase {}, inc {}, offset {}, panL {}, panR {};
        std::array<Wander, singers> drift {};
        Wander vowel;
        VoiceShape shape;   // Liminality
        Svf formL[formants], formR[formants];
        float formGain[formants] {};
        OnePole tiltL, tiltR, darkL, darkR;
        float amp = 0, velocity = 0, gain = 0, age = 0, vibPhase = 0, vibRate = 5;
        Random noise;
    };
    void control(Voice&);
    float level(const Voice& v) const { return v.amp * v.gain * v.fade; }

    // A recorded note (Milestone 8): the lead's two vowels, three distant singers, the ghost, the hum.
    static constexpr int readers = 7;
    struct RVoice : VoiceBase {
        std::array<Reader, readers> r {};
        std::array<float, readers> panL {}, panR {}, cents {};
        OnePole darkL, darkR, distL, distR;
        float amp = 0, gain = 1, breathPhase = 0, breathRate = 0.1f;
        VoiceShape shape;
    };
    bool startRecorded(int note, float velocity, float share);
    void controlRecorded(RVoice&, float tick);
    void updateHum();
    float level(const RVoice& v) const { return v.amp * v.gain * v.fade; }
    VoicePool<RVoice, slots> rpool;
    bool humChanged = false;

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
};

} // namespace liminal::dsp
