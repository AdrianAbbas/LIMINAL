#pragma once
// The Milestone 0 test instrument: a small struck-string model, enough to prove that MIDI
// reaches the plug-in in a host and that notes, chords, the sustain pedal and pitch bend
// behave. It is NOT the Forgotten Piano (Milestone 1a replaces it).
//
// Each note is eight stretched partials (stiff-string inharmonicity), each sounded by two
// slightly detuned "strings" (beating), with a two-stage decay (prompt sound, aftersound)
// that is shorter for higher partials and higher notes, plus a short felt-hammer noise.
// Real-time safe: fixed voice pool, no allocation, no locks.
#include "DSP/Stereo.h"
#include <array>
#include <cstdint>

namespace liminal::dsp {

class TestPiano {
public:
    static constexpr int polyphony = 16;              // sounding notes
    static constexpr int slots = polyphony + 6;       // + room for stolen notes to fade out

    void prepare(double sampleRate);
    void reset();

    // MIDI (channel is ignored for now: every channel plays the instrument).
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void sustain(bool down);
    void pitchBend(int value14bit);                   // +/- 2 semitones
    void allNotesOff();                               // release (dampers down)
    void allSoundOff();                               // short fade of everything

    // Adds `n` samples to left/right.
    void render(float* left, float* right, int n);

    int activeVoices() const;

private:
    static constexpr int partials = 8;
    struct Osc { float c = 1, s = 0, dc = 1, ds = 0; };
    struct Voice {
        int note = -1;
        bool held = false, sustained = false, stolen = false;
        float velocity = 0, panL = 0.7f, panR = 0.7f;
        std::array<Osc, partials * 2> osc {};
        std::array<float, partials> amp {}, fast {}, slow {}, fastMul {}, slowMul {}, release {};
        std::array<float, partials * 2> ratio {};     // frequency / note frequency per oscillator
        float attack = 0, attackStep = 0, fade = 1, fadeStep = 0;
        float hammer = 0, hammerMul = 0, hammerLp = 0, hammerCoef = 0;
        float baseHz = 0;
        std::uint64_t started = 0;
        int renorm = 0;
    };

    void start(Voice&, int note, float velocity);
    void tune(Voice&);
    void releaseVoice(Voice&);
    float level(const Voice&) const;
    float noise();

    double sr = 48000.0;
    std::array<Voice, slots> voices {};
    float bend = 1.0f;
    bool pedal = false;
    std::uint32_t rng = 0x1234567u;
    std::uint64_t counter = 0;
};

} // namespace liminal::dsp
