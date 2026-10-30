#pragma once
// GLASS MEMORIES (Milestone 1a): a struck (and, with Resonance, rubbed) glass. Modal
// synthesis with measured wine-glass mode ratios; the sources are in
// docs/INSTRUMENT_NOTES.md.
//
// Each note is the glass's (n,0) bending modes, n = 2..7, at 1 : 1.86 : 3.25 : 4.95 : 7.0
// : 9.35 of the played pitch. A real glass is never perfectly round, so every mode is a
// doublet a fraction of a hertz apart, which beats (more with Fragility). The strike is a
// raised-cosine pulse whose length sets which modes speak (soft mallet to hard tick).
// Resonance adds the rubbed-rim sound while a key is held: the lowest mode and its
// harmonics, swelling in slowly, as a finger on a wet rim excites them; it also lengthens
// the fundamental's ring. Fragility adds strike-to-strike irregularity, wider doublets, a
// brittle tick and two short "crack" partials. Detune is pitch instability: a random offset
// per strike and a slow wander, never a slide into the note.
#include "DSP/Instrument.h"

namespace liminal::dsp {

class GlassMemories final : public Instrument {
public:
    struct Settings {
        float fragility = 0.4f;
        float brightness = 0.5f;
        float resonance = 0.3f;
        float detune = 0.2f;
        float decay = 0.6f;    // 0..1: 1.2 s .. 20 s ring of the lowest mode (around A5)
    };
    static constexpr int polyphony = 16;
    static constexpr int slots = polyphony + 6;
    static constexpr int modeCount = 6;

    void setSettings(const Settings& s) { settings = s; }
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override { pool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); }
    void pitchBend(int value14bit) override;
    void allNotesOff() override { pool.allNotesOff(); }
    void allSoundOff() override { pool.allSoundOff(sr); }
    void render(float* left, float* right, int n) override;
    int activeVoices() const override { return pool.active(); }

private:
    static constexpr int controlPeriod = 128;
    struct Voice : VoiceBase {
        ModeBank<2 * modeCount + 2> struck;   // doublets + two crack partials
        ModeBank<6> singing;                  // rubbed rim: three harmonics, doublets
        bool damped = false;
        float attack = 0, attackStep = 0;
        float sing = 0, singTarget = 0, singRise = 0, singFall = 0;
        float tick = 0, tickDecay = 0, tickPrev = 0;
        Random noise;
        Wander wobble, flutter;
        float flutterNow = 1, wobbleRatio = 1, level = 0;
    };
    void control(Voice&);
    float level(const Voice& v) const { return (v.level + v.sing) * v.fade; }

    double sr = 48000.0;
    Settings settings;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
};

} // namespace liminal::dsp
