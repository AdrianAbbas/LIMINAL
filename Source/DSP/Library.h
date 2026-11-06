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
// E-Piano, Distant Radio), drawbars (Warped Organ) and analog / formant voices (Faded Strings, Solo Voice,
// Cassette Strings, Arcade Lead, Basement Bass). Six controls per slot, the same six for all fourteen,
// each read by the instrument in its own way: Tone, Age, Softness, Decay, Width and one of its own
// (Colour: "Other Room", "Section", "Col Legno", "Broken", "Bell", "Tremolo", "PA", "Vowel", "Station",
// "Tape Wear", "Grit", "Floor", "Wow", "Bow"). 0.5 everywhere is the instrument as designed.
//
// Real-time rules as everywhere: fixed storage (the plucked string's line lives in the voice), no
// allocation after prepare(), modulation on a 32-sample control clock.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "DSP/Remembered.h"
#include "DSP/Sampler.h"
#include "DSP/Woodwind.h"
#include <vector>

namespace liminal::dsp {

namespace library {
inline constexpr int originals = 6;   // the original six instruments come first (slot i's own = i)
inline constexpr int total = 20;
enum Kind { oldGuitar, fadedStrings, hallwayPizzicato, toyPiano, loungeEPiano, elevatorVibraphone, mallChimes, soloVoice, distantRadio,
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
// How much of the room an instrument sends by default, against the piano's 1 (2026-11-05: the voices,
// the violin and the arcade lead remembered in their place, so more of the room).
inline float roomSend(int instrument) {
    switch (instrument) {
    case 3: return 1.5f;                // Empty Choir
    case 5: return 1.15f;               // Hollow Drone (its voice layer)
    case originals + 1: return 1.25f;   // Faded Strings
    case originals + 2: return 1.25f;   // Hallway Pizzicato
    case originals + 7: return 1.8f;    // Solo Voice
    case originals + 8: return 1.4f;    // Distant Radio (in another room)
    case originals + 10: return 2.0f;   // Arcade Lead (across the mall: mostly the mall)
    default: return 1.0f;
    }
}
// 2026-11-06: the woodwind each instrument can blend in (woodwind::none: none), and what its control is called.
inline int windOf(int instrument) {
    switch (instrument) {
    case originals + 5: return woodwind::flute;      // Elevator Vibraphone: muzak's flute
    case originals + 4: return woodwind::clarinet;   // Lounge E-Piano: the lobby's clarinet
    default: return woodwind::none;
    }
}
inline const char* windLabel(int instrument) {
    switch (windOf(instrument)) {
    case woodwind::flute: return "MUZAK FLUTE";
    case woodwind::clarinet: return "LOBBY CLARINET";
    default: return "";
    }
}
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
        // 2026-11-06: the woodwind blend (DSP/Woodwind.h), on the instruments that have one
        // (library::windOf): 0 = the instrument alone, exactly as before.
        float wind = 0.0f;
        bool windRecorder = false;   // the Recorder voice (the developer's alto recorder) instead
    };
    static constexpr int polyphony = 12;
    static constexpr int slots = polyphony + 4;

    void setKind(int kind);   // 0..13 (library::Kind). Only while silent (the processor swaps instances).
    int kind() const { return kindNow; }
    void setSettings(const Settings& s) { settings = s; }
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override { pool.noteOff(note); wind.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); wind.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void allNotesOff() override { pool.allNotesOff(); wind.allNotesOff(); }
    void allSoundOff() override { pool.allSoundOff(sr); wind.allSoundOff(); }
    void render(float* left, float* right, int n) override;
    void reseed(std::uint32_t s) override {   // (while notes sound, only the randomness: see FadedSynth)
        rng.state = s | 1u;
        wind.reseed(s ^ 0x77u);
        if (activeVoices() > 0) return;
        wow = {};
        wowCents = 0;
        lfoPhase = float(s >> 8) * (1.0f / 16777216.0f);
        pool.counter = 0;
    }
    int activeVoices() const override { return pool.active() + wind.activeVoices(); }
    int heldVoices() const override { return pool.held() + wind.heldVoices(); }
    // The woodwind layer (tests).
    const Woodwind& woodwindLayer() const { return wind; }
    bool windRecordedNow() const { return wind.recordedNow(); }
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
        float scoop = 0;            // cents at the start that glide to 0 (Solo Voice: a legato glide from the last note)
        float scoopTime = 0.18f;    // seconds of that glide
        float sag = 0, sagRate = 0; // a dying note (Warped Organ): cents and level it loses
        // Recorded: up to three readers (a second set or articulation, an ensemble copy).
        std::array<Reader, 3> r {};
        std::array<float, 3> rL {}, rR {}, rCents {};
        float mix = 0;              // vowel / patch crossfade (equal power), set at control rate
        bool oneTake = false;       // Solo Voice's lowest notes: one take for both vowels (no crossfade)
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
    // 2026-11-05: the voice, the violin and the arcade lead remembered, not recorded (DSP/Remembered.h).
    Remembered remembered;
    bool treated = false;
    void updateTreatment();
    // Distant Radio (2026-11-05, in Hummed Memory's place): the station's slow fading and its static,
    // both following the playing (they die with it).
    float radioLevel = 0, radioLevelCoef = 0, radioFadePhase = 0, staticZ = 0, crackle = 0;
    Random radioNoise;
    // 2026-11-06: the woodwind blend. The instrument gives way a little as it comes in (the two together
    // as loud as the instrument alone); both gains glide per sample (no steps when the control moves).
    Woodwind wind;
    float windNow = 0, windStep = 0, windTarget = 0;
    int windKey = -1;
    void updateWind();
public:
    static float windMatchDb(int kind);   // the layer's level against the instrument's (tests: woodwind)
};

} // namespace liminal::dsp
