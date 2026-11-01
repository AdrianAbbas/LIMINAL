#pragma once
// LIMINALITY (2026-11-01): the shared stage that makes every instrument unstable, slightly
// detuned and half-remembered - music from an old tape, a neglected piano in an empty building,
// a memory that is slightly wrong. One per instrument, after the instrument's voices and before
// Distance, the effects and the environment. Two halves:
//
//   NOTE side (through Instrument's Liminality hook, so any instrument, synthesised or sampled,
//   gets it by applying a per-voice pitch offset and tone):
//     Detune      each note slightly out of tune (mostly per key, like a neglected piano, a little
//                 per strike), up to ~30 cents at 100 %
//     Sour Notes  a small chance that a note comes out clearly off (35-60 cents: microtonal,
//                 never a wrong key)
//     Unevenness  some notes unexpectedly quieter, duller or brighter (worn keys and hammers)
//     Timing      notes held back unevenly (0 to ~60 ms at 100 %; see the note on "early")
//     Drift       a slow wander of the whole instrument's pitch that drifts away and comes back
//     Wow & Flutter  tape: slow wow (~0.6-0.9 Hz) plus faster flutter (~7-12 Hz)
//     Warble      (pitch part) each sustained voice wavers on its own, seasick, growing in after
//                 the attack
//   AUDIO side (on the instrument's stereo output):
//     Ghost       a second, slightly detuned (a constant few cents) and delayed (~55-95 ms) copy
//     Warble      (volume part) a slow uneven swell plus a faint chorus copy on sustained notes
//     Fade        held notes slowly lose definition: the highs close in, a slight blur
//     Wear        lost highs and lows, soft saturation, faint hiss and crackle under the playing,
//                 an occasional tiny dropout
//
// Rules: pitch moves only through these controls (no glides or scoops: every movement is a wobble
// around the note); at every amount 0 the stage is bypassed exactly; all randomness comes from
// hashes of (seed, instrument, the stage's clock), the clock being the host's timeline while it
// plays, so a render or a replay of the same song gives the same notes; fixed storage (prepare),
// no allocation or locks in the audio path; parameter changes are smoothed.
//
// "Early": a plug-in cannot play a note before it arrives without adding latency to everything,
// so Timing only holds notes back; with an average lateness some land before it (early relative
// to the others) and some after.
#include "DSP/Instrument.h"
#include "DSP/Filters.h"
#include <array>
#include <cstdint>
#include <vector>

namespace liminal::dsp {

namespace lim {
enum Control { detune, drift, wow, warble, ghost, wear, timing, uneven, sour, fade, count };
inline constexpr const char* keys[count] { "detune", "drift", "wow", "warble", "ghost", "wear", "timing", "uneven", "sour", "fade" };
inline constexpr const char* names[count] { "Detune", "Drift", "Wow & Flutter", "Warble", "Ghost", "Wear", "Timing", "Unevenness", "Sour Notes", "Fade" };
// Each instrument's own amounts by default (with the LIMINALITY macro at its default, 40 %, these
// are what is heard before the room's nudge).
inline constexpr float defaults[count] { 0.40f, 0.35f, 0.40f, 0.35f, 0.35f, 0.40f, 0.35f, 0.40f, 0.35f, 0.35f };
inline constexpr float macroDefault = 0.4f;   // the macro's default; 100 % is 2.5x the amounts
inline constexpr float maxEffective = 1.5f;   // past 1 (deeply unsettling) towards broken
} // namespace lim
using LiminalAmounts = std::array<float, lim::count>;

// How each place (and its Character) nudges the instruments' liminality: a multiplier per control
// (1 = unchanged). School: more wow and tape wear; hotel: wear and fade; hospital: cold detune;
// impossible room: sour notes and ghost; pool: slow warble; Something Wrong, Room 404, Beyond
// Reality (and the other unsettling Characters): more sour notes and drift.
LiminalAmounts liminalNudge(int place, int character);
// The amounts the stage uses: own amount x macro (0 clean, 0.4 as set, 1 = 2.5x) x the room's
// nudge (scaled by Environment Amount), each clamped to [0, maxEffective].
LiminalAmounts liminalEffective(const LiminalAmounts& own, float macro, int place, int character, float environmentAmount);

class Liminality {
public:
    void prepare(double sampleRate, int instrumentIndex);
    void reset();   // audio state, pending notes and smoothing (jumps to the current amounts)
    void setAmounts(const LiminalAmounts& effective) { target = effective; }
    void setSeed(std::uint32_t s) { seed = s; }
    // The timeline jumped (the host relocated): the modulators continue from the new position
    // (they are pure functions of it) and the audio side's noise is reseeded from it.
    void relocate(std::uint64_t clock);

    // Every control tick (64 samples, on the processor's grid). `clock`: the stage's timeline
    // position. Hands the instrument its pitch (Drift, Wow & Flutter, Warble depth, wander gate).
    void control(Instrument&, std::uint64_t clock);

    // Notes. `now`: the processor's sample counter (the timing queue runs on it); `clock`: the
    // timeline (the randomness is a function of it). Note-ons may be held back (Timing); a note-off
    // follows its note by the same delay.
    void noteOn(Instrument&, int note, float velocity, std::uint64_t now, std::uint64_t clock);
    void noteOff(Instrument&, int note, std::uint64_t now);
    void flush(Instrument&);   // panic: forget held-back note-ons, send held-back note-offs now
    std::uint64_t nextDue() const;   // when the next held-back event is due (UINT64_MAX: none)
    void fireDue(Instrument&, std::uint64_t now);

    // The audio side, in place, at most 64 samples (never across a control tick).
    void process(float* left, float* right, int n);

    // For tests and the interface.
    struct Decision { float cents = 0, tone = 0, velocityScale = 1; int delay = 0; bool sour = false; };
    Decision lastDecision() const { return last; }
    float pitchNow() const { return pitch.cents; }
    bool audioBypassed() const { return bypassed; }
    const LiminalAmounts& amounts() const { return now; }
    int dropouts() const { return dropoutCount; }

private:
    float modulators(double seconds) const;   // Drift + Wow & Flutter at a timeline position, cents
    std::uint32_t salt(std::uint32_t k) const;
    void startNote(Instrument&, int note, float velocity, const NoteShape&);
    float read(const std::vector<float>& line, float delay) const;
    void resetFilters();

    double sr = 48000.0;
    int instrument = 0;
    std::uint32_t seed = 1;
    LiminalAmounts target {}, now {};
    LiminalPitch pitch;
    Decision last;
    float relocateOffset = 0, relocateDecay = 0.9f;
    std::uint64_t lastClock = 0;
    bool clockKnown = false;

    // Held-back events.
    struct Pending { std::uint64_t due; int note; float velocity; NoteShape shape; bool on; };
    std::array<Pending, 64> queue {};
    int queued = 0;
    std::array<std::uint32_t, 128> noteDelay {};   // each note's latest delay (its note-off follows)
    std::array<std::uint64_t, 128> lastDue {};     // a key's events never overtake each other

    // Audio side.
    bool bypassed = true, lineLive = false;
    std::vector<float> lineL, lineR;   // the dry instrument's history (Ghost, Warble's chorus)
    int lineMask = 0, write = 0;
    float ghostDelay = 0, ghostWindow = 0, ghostPos = 0, ghostStep = 0, ghostGain = 0, ghostTarget = 0;
    OnePole ghostToneL, ghostToneR;
    std::array<float, 513> window {};   // sin^2 over half a turn
    float warbleAmp = 0, warbleAmpTarget = 0, warbleChorus = 0, warbleChorusTarget = 0;
    float warbleGain = 1, warbleGainStep = 0, chorusDelay = 0, chorusDelayStep = 0;
    float sinceNote = 10;   // seconds since the last note-on (Warble and Fade grow on held notes)
    float sustainWarble = 0, sustainFade = 0;
    float fadeMix = 0, fadeMixTarget = 0, blurMix = 0, blurMixTarget = 0;
    float fadeCoef = 1, fadeCoefStep = 0, fadeOctaves = 0;
    float fadeL1 = 0, fadeL2 = 0, fadeR1 = 0, fadeR2 = 0;
    struct Allpass { std::vector<float> line; int pos = 0; };
    std::array<Allpass, 4> blur;
    float wearMix = 0, wearMixTarget = 0, drive = 1, driveTarget = 1;
    float lowCoef = 1, highCoef = 0;
    float wl1 = 0, wl2 = 0, wr1 = 0, wr2 = 0, hl1 = 0, hl2 = 0, hr1 = 0, hr2 = 0;
    float envelope = 0, envAttack = 0, envRelease = 0;
    float wearGain = 1, wearGainTarget = 1, wearIn = 0, wearOut = 0, wearEnvCoef = 0;   // Wear's level match
    float outGain = 1, outGainTarget = 1;   // Unevenness' make-up (its quiet notes stay quieter than the rest)
    float hissLevel = 0, hissZ1 = 0, hissZ2 = 0, hissH1 = 0, hissH2 = 0;
    float crackleRate = 0, crackleLevel = 0, crackle = 0, crackleZ = 0;
    float dropGain = 1, dropTarget = 1, dropCoefDown = 0, dropCoefUp = 0;
    int dropLeft = 0, dropoutCount = 0;
    Random noise;
    float smooth = 0.01f;   // per-sample smoothing of the mixes (~20 ms)
};

} // namespace liminal::dsp
