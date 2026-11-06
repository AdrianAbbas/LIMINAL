#pragma once
// WOODWIND (2026-11-06): a soft flute or clarinet played along with an instrument, as an optional
// blend on its own control. It is never heard clean: a built-in remembered treatment (DSP/Remembered.h:
// distance, tape and age, blur) is always on it, the way a woodwind on an old lobby or elevator tape is
// half-heard. Two instruments carry one (docs/DEVLOG.md, "Real woodwinds"):
//   Elevator Vibraphone  + flute     (muzak: vibes doubled by a soft flute, from a ceiling speaker)
//   Lounge E-Piano       + clarinet  (a hotel lobby's lounge record: a soft clarinet across the lobby)
//
// Recordings: the sets `ww-flute` and `ww-clarinet` (the free CC0 library VSCO 2 Community Edition,
// soft sustained takes, prepared by tools/sample-prep/woodwinds.py: looped, release tails, each note
// at its measured pitch) and, when the developer has recorded it, `alto-recorder` (the Recorder voice:
// a school recorder; until it exists the Recorder voice plays the flute). Without the recordings a
// synthesised stand-in plays (a few harmonics and breath: a flute's near-sine, a clarinet's odd
// harmonics), level-matched to the recordings (tests: woodwind).
//
// It is an Instrument of its own so the Liminality hooks reach it unchanged: the owner hands it each
// note's shape and the instrument-wide Liminality pitch. Real-time rules as everywhere: fixed storage
// set in prepare(), no allocation, modulation on a 32-sample control clock.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "DSP/Remembered.h"
#include "DSP/Sampler.h"

namespace liminal::dsp {

namespace woodwind {
enum Voice { none = -1, flute = 0, clarinet = 1, recorder = 2 };
inline const char* setOf(int voice) { return voice == clarinet ? "ww-clarinet" : voice == recorder ? "alto-recorder" : "ww-flute"; }
inline const char* nameOf(int voice) { return voice == clarinet ? "clarinet" : voice == recorder ? "recorder" : "flute"; }
// The treatment each one is heard through.
inline RememberedProfile muzakFlute() {   // a ceiling speaker in an elevator: small, far, an old tape
    RememberedProfile p;
    p.lowCut = 260; p.highCut = 3000; p.wall = 0.25f; p.deEss = 0.6f; p.soften = 0.12f; p.saturation = 0.3f;
    p.wow = 6.0f; p.drift = 7.0f; p.blur = 0.5f; p.reflections = 0.6f; p.preDelayMs = 20; p.spacing = 1.5f;
    p.direct = 0.65f; p.hissDb = -54; p.makeupDb = 0.0f;
    return p;
}
inline RememberedProfile lobbyClarinet() {   // across a hotel lobby, from the lounge's old record
    RememberedProfile p;
    p.lowCut = 150; p.highCut = 3300; p.wall = 0.15f; p.deEss = 0.5f; p.soften = 0.1f; p.saturation = 0.25f;
    p.wow = 5.0f; p.drift = 6.0f; p.blur = 0.55f; p.reflections = 0.75f; p.preDelayMs = 28; p.spacing = 2.0f;
    p.direct = 0.6f; p.hissDb = -56; p.makeupDb = 0.0f;
    return p;
}
} // namespace woodwind

class Woodwind final : public Instrument {
public:
    static constexpr int polyphony = 8;
    // `voice`: woodwind::flute / clarinet (the instrument's own); `recorder`: the developer's alto
    // recorder instead, when recorded (else the flute). Only while silent (or the next note on).
    void setVoice(int voice, bool recorder);
    int voice() const { return voiceNow; }
    // The treatment (its make-up is set here so the treated layer is as loud as the dry one).
    void setProfile(const RememberedProfile& p);
    void prepare(double sampleRate) override;
    void reset() override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override { pool.noteOff(note); }
    void sustain(bool down) override { pool.sustain(down); }
    void pitchBend(int value14bit) override { bend = bendRatio(value14bit); }
    void setBendRatio(float ratio) { bend = ratio; }
    void allNotesOff() override { pool.allNotesOff(); }
    void allSoundOff() override { pool.allSoundOff(sr); }
    // Adds into left/right (the treatment included).
    void render(float* left, float* right, int n) override;
    void reseed(std::uint32_t s) override { rng.state = s | 1u; }
    int activeVoices() const override { return pool.active(); }
    int heldVoices() const override { return pool.held(); }
    bool recordedNow() const;          // a note now plays recordings (false: the stand-in)
    bool recorderRecorded() const;     // the developer's alto recorder exists
    // Level trims (dB): the stand-in matched to the recordings (tests: woodwind).
    static float standInTrimDb(int voice);
    // Tests: without the treatment (a cents measurement on the bare notes).
    static bool& bypassTreatmentForTests() { static bool off = false; return off; }
    // The pitch range of what plays now (MIDI): notes outside fold in by octaves.
    float lowNote() const;
    float highNote() const;

private:
    static constexpr int controlPeriod = 32;
    struct Voice : VoiceBase {
        bool recorded = false;
        Reader r;
        float played = 60;          // the (folded) pitch it plays
        float velocity = 0, gain = 1, amp = 0, age = 0;
        float attackCoef = 0, releaseCoef = 0;
        VoiceShape shape;
        float vibPhase = 0;
        float panL = 0.7f, panR = 0.7f;
        // Stand-in.
        float phase = 0, inc = 0, chiff = 0;
        Random noise;
        Svf breath;
        OnePole tone;
    };
    void startVoice(Voice& v, int note, float velocity);
    void control(Voice& v, float tick);
    const samples::Instrument* set() const;
    int playingVoice() const;   // flute / clarinet / recorder as it sounds now

    double sr = 48000.0;
    int voiceNow = woodwind::flute;
    bool wantRecorder = false;
    VoicePool<Voice, polyphony + 2> pool;
    Random rng;
    float bend = 1.0f;
    int clock = 0;
    Remembered treatment;
    RememberedProfile profile = woodwind::muzakFlute();
};

} // namespace liminal::dsp
