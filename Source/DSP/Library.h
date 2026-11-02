#pragma once
// THE INSTRUMENT LIBRARY (2026-11-02). LIMINAL has six slots (piano, synth, glass, choir, music box,
// drone: the parameter keys) and twenty instruments; each slot can hold any of them. The first six
// instruments are the original classes (ForgottenPiano ... HollowDrone); this class is the other
// fourteen. Everything after the voices - Liminality, Distance, the slot's effects, the rooms' per-slot
// shaping, Characters, the VHS warble - belongs to the slot, so it works on every instrument the same
// way (docs/DEVLOG.md, "The instrument library").
//
// Every instrument here plays the developer's recordings where it has them (the sample bank, as the
// originals since Milestage 8) and keeps a synthesised stand-in that plays without them (a build without
// SAMPLES_TOKEN, or while the bank loads): a plucked string (Old Guitar, Hallway Pizzicato), modal bars
// and bells (Toy Piano, Elevator Vibraphone, Mall Chimes, Bowed Glass), a two-operator tine (Lounge
// E-Piano), drawbars (Warped Organ) and analog / formant voices (Faded Strings, Solo Voice, Hummed Memory,
// Cassette Strings, Arcade Lead, Basement Bass). Six controls per slot, the same six for all fourteen,
// each read by the instrument in its own way: Tone, Age, Softness, Decay, Width and one of its own
// (Colour: "Other Room", "Section", "Col Legno", "Broken", "Bell", "Tremolo", "PA", "Vowel", "Memory",
// "Tape Wear", "Grit", "Floor", "Wow", "Bow"). 0.5 everywhere is the instrument as designed.
//
// Real-time rules as everywhere: fixed storage (the plucked string's line lives in the voice), no
// allocation after prepare(), modulation on a 32-sample control clock.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "DSP/Sampler.h"
#include <vector>

namespace liminal::dsp {

namespace library {
inline constexpr int originals = 6;   // the original six instruments come first (slot i's own = i)
inline constexpr int total = 20;
enum Kind { oldGuitar, fadedStrings, hallwayPizzicato, toyPiano, loungeEPiano, elevatorVibraphone, mallChimes, soloVoice, hummedMemory,
            cassetteStrings, arcadeLead, basementBass, warpedOrgan, bowedGlass, kinds };
struct Info {
    const char* name;         // the instrument (product name, no brands)
    const char* shortName;    // tabs, the shelf
    const char* colour;       // what its sixth control is called
    const char* description;  // the picker's one line
    const char* sets;         // the recordings it plays ("" = synthesised only)
};
// Index 0..19 over the whole library (0..5 the originals).
const Info& info(int instrument);
inline bool isOriginal(int instrument) { return instrument >= 0 && instrument < originals; }
} // namespace library

class Library final : public Instrument {
public:
    struct Settings {
        float tone = 0.5f;       // darker .. brighter
        float age = 0.5f;        // how worn: the instrument's own way of having aged
        float softness = 0.5f;   // a softer, slower attack
        float decay = 0.5f;      // how long notes ring / release
        float width = 0.5f;      // stereo spread (and ensemble width)
        float colour = 0.5f;     // the instrument's own control (library::info().colour)
    };
    static constexpr int polyphony = 12;
    static constexpr int slots = polyphony + 4;

    void setKind(int kind);   // 0..13 (library::Kind). Only while silent (the processor swaps instances).
    int kind() const { return kindNow; }
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
    void reseed(std::uint32_t s) override {
        rng.state = s | 1u;
        wow = {};
        wowCents = 0;
        lfoPhase = float(s >> 8) * (1.0f / 16777216.0f);
        pool.counter = 0;
    }
    int activeVoices() const override { return pool.active(); }
    // Whether a note now would play recordings (false: the stand-in).
    bool recordedNow() const;
    // Level trims (dB): the recordings matched to the stand-in, the stand-ins to the piano (tests: library).
    static float trimDb(int kind, bool recorded);

private:
    static constexpr int controlPeriod = 32;
    static constexpr int lineSize = 4096;   // the plucked string's delay (E1 at 96 kHz)
    enum class Engine { recorded, pluck, modal, tine, analog };
    struct Voice : VoiceBase {
        Engine engine = Engine::analog;
        float velocity = 0, gain = 1, amp = 0, age = 0, hz = 440;
        bool attacking = true;
        float attackCoef = 0, releaseCoef = 0, heldCoef = 1;   // per-sample envelope coefficients
        VoiceShape shape;
        Wander wobble;
        float panL = 0.7f, panR = 0.7f;
        float scoop = 0;            // cents at the start that glide to 0 (Hummed Memory, Solo Voice)
        float sag = 0, sagRate = 0; // a dying note (Warped Organ): cents and level it loses
        // Recorded: up to three readers (a second set or articulation, an ensemble copy).
        std::array<Reader, 3> r {};
        std::array<float, 3> rL {}, rR {}, rCents {};
        float mix = 0;              // vowel / patch crossfade (equal power), set at control rate
        // Modal (bars, bells, drawbars, glass).
        ModeBank<8> modes;
        // Tine (two-operator FM: the e-piano).
        float pc = 0, pm = 0, pt = 0, incC = 0, incM = 0, incT = 0, index = 0, indexDecay = 1, bell = 0, bellDecay = 1;
        // Analog / formant.
        std::array<float, 3> ph {}, inc {}, det {};
        Svf filter, formant[3];
        float formantGain[3] {};
        float vibPhase = 0, vibRate = 5.2f;
        Random noise;
        float burst = 0;            // a short noise (key click, hammer thunk, bow catch)
        // Plucked string.
        std::array<float, lineSize> line {};
        int write = 0;
        float delay = 100, delayTarget = 100, loss = 0.99f, bright = 0.2f, z1 = 0, z2 = 0;
        // Tone (per voice, darker with Tone / softer velocity).
        OnePole lpL, lpR;
        float tone = 1;
        bool stereo = false;
    };
    void startVoice(Voice&, int note, float velocity);
    bool startRecorded(Voice&, int note, float velocity);
    void startSynth(Voice&, int note, float velocity);
    void control(Voice&, float tick);
    void renderVoice(Voice&, float* l, float* r, int m, const float* trem);
    float level(const Voice& v) const { return v.amp * v.gain * v.fade; }

    double sr = 48000.0;
    Settings settings;
    int kindNow = 0;
    VoicePool<Voice, slots> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
    Wander wow;
    float wowCents = 0, lfoPhase = 0, flutterPhase = 0;
    // After the voices: the instrument's own colour (a room behind a wall, a PA horn, a floor, a
    // console's converter, a hallway's echo, a vibraphone's motor).
    Biquad postL[2], postR[2];
    std::vector<float> echoL, echoR;
    int echoWrite = 0;
    float echoLp[2] {}, crushOut[2] {};
    void updatePost();
};

} // namespace liminal::dsp
