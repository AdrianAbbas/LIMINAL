#pragma once
// What every LIMINAL instrument shares (Milestone 1): the MIDI-facing interface, voice
// bookkeeping (restrike, click-free stealing, sustain pedal, panic), a bank of decaying
// sinusoidal modes for the modal instruments, and small helpers.
//
// Real-time rules for everything here: fixed-size storage, no allocation, no locks.
// Modulation is updated on an internal control clock (every `controlPeriod` samples),
// never per render call, so output does not depend on how the host splits its blocks.
#include "DSP/Stereo.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace liminal::dsp {

inline constexpr float twoPi = 6.283185307f;
inline constexpr float pi = 3.14159265f;

// Per-sample multiplier that falls 60 dB in `seconds`.
inline float t60(double seconds, double sr) { return float(std::exp(-6.907755 / std::max(1.0e-4, seconds * sr))); }
inline float noteHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }
inline float cents(float c) { return std::exp2(c / 1200.0f); }

// A fixed hash of (note, salt) in [0, 1): an instrument's per-key character, the same
// every time the key is played (a real piano's unevenness belongs to its keys).
inline float keyHash(int note, int salt) {
    std::uint32_t x = std::uint32_t(note) * 0x9E3779B1u ^ std::uint32_t(salt) * 0x85EBCA77u;
    x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15;
    return float(x >> 8) * (1.0f / 16777216.0f);
}

struct Random {
    std::uint32_t state = 0x1234567u;
    float uniform() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return float(state >> 8) * (1.0f / 16777216.0f); }
    float bipolar() { return 2.0f * uniform() - 1.0f; }
};

// Slow, smooth random wander in [-1, 1] (two one-poles chasing random targets). Used for
// drift and pitch instability: a wobble around the note, never a slide into it.
struct Wander {
    float target = 0, mid = 0, value = 0, coef = 0.001f, hold = 0;
    void start(Random& rng) { value = mid = target = rng.bipolar() * 0.5f; }
    // Called once per control tick; `rateHz` is roughly how often it turns.
    float tick(Random& rng, float rateHz, float tickSeconds) {
        hold -= tickSeconds * rateHz;
        if (hold <= 0.0f) { target = rng.bipolar(); hold += 0.5f + rng.uniform(); }
        coef = std::min(1.0f, tickSeconds * rateHz * 2.5f);
        mid += coef * (target - mid);
        value += coef * (mid - value);
        return value;
    }
};

// The MIDI side of an instrument. Render adds into left/right.
class Instrument {
public:
    virtual ~Instrument() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void noteOn(int note, float velocity) = 0;
    virtual void noteOff(int note) = 0;
    virtual void sustain(bool down) = 0;
    virtual void pitchBend(int value14bit) = 0;   // +/- 2 semitones, immediate
    virtual void allNotesOff() = 0;               // release (dampers down)
    virtual void allSoundOff() = 0;               // short fade of everything
    virtual void render(float* left, float* right, int n) = 0;
    virtual int activeVoices() const = 0;

    static float bendRatio(int value14bit) {
        return std::exp2(2.0f * float(std::clamp(value14bit, 0, 16383) - 8192) / 8192.0f / 12.0f);
    }
};

struct VoiceBase {
    int note = -1;
    bool held = false, sustained = false, stolen = false;
    float fade = 1, fadeStep = 0;
    std::uint64_t started = 0;
    bool active() const { return note >= 0; }
    bool releasing() const { return !held && !sustained; }
    void steal(double seconds, double sr) {
        if (stolen) return;
        stolen = true;
        fadeStep = 1.0f / float(std::max(1.0, seconds * sr));
    }
};

// Voice bookkeeping shared by the instruments. `level(voice)` estimates loudness, so the
// quietest voice is the one stolen.
template <class V, std::size_t Slots>
struct VoicePool {
    std::array<V, Slots> voices {};
    bool pedal = false;
    std::uint64_t counter = 0;

    // Finds the slot for a new note. A ringing copy of the same note fades over
    // `restrikeSeconds`; over `polyphony` sounding notes the quietest fades in 5 ms.
    template <class Level>
    V& allocate(int note, int polyphony, double sr, double restrikeSeconds, Level level) {
        int sounding = 0;
        for (auto& v : voices)
            if (v.active() && !v.stolen) {
                if (v.note == note) v.steal(restrikeSeconds, sr);
                else ++sounding;
            }
        if (sounding >= polyphony) {
            V* quietest = nullptr;
            for (auto& v : voices)
                if (v.active() && !v.stolen && (quietest == nullptr || level(v) < level(*quietest))) quietest = &v;
            if (quietest != nullptr) quietest->steal(0.005, sr);
        }
        for (auto& v : voices)
            if (!v.active()) return v;
        V* slot = &voices[0];   // every slot busy, even the fading ones: the quietest fading one
        for (auto& v : voices)
            if (level(v) * v.fade < level(*slot) * slot->fade) slot = &v;
        return *slot;
    }
    void begin(V& v, int note) {
        v.note = note;
        v.held = true;
        v.sustained = false;
        v.stolen = false;
        v.fade = 1.0f;
        v.fadeStep = 0.0f;
        v.started = ++counter;
    }
    void noteOff(int note) {
        for (auto& v : voices)
            if (v.note == note && v.held && !v.stolen) {
                v.held = false;
                v.sustained = pedal;
            }
    }
    void sustain(bool down) {
        pedal = down;
        if (!down)
            for (auto& v : voices) v.sustained = false;
    }
    void allNotesOff() {
        pedal = false;
        for (auto& v : voices) v.held = v.sustained = false;
    }
    void allSoundOff(double sr) {
        allNotesOff();
        for (auto& v : voices)
            if (v.active()) v.steal(0.01, sr);
    }
    int active() const { return int(std::count_if(voices.begin(), voices.end(), [](const V& v) { return v.active(); })); }
};

// A bank of exponentially decaying sinusoids, each a complex phasor rotated (and shrunk)
// once per sample. Arrays are padded to a multiple of 8 and summed in 8 lanes, so the
// compiler can vectorise without reordering floating-point sums.
template <int Max>
struct ModeBank {
    static constexpr int size = (Max + 7) / 8 * 8;
    int count = 0;   // modes in use, rounded up to a multiple of 8 by render
    alignas(32) float c[size] {}, s[size] {}, dc[size] {}, ds[size] {}, gl[size] {}, gr[size] {};
    float w[size] {}, rHeld[size] {}, rDamped[size] {};

    void clear() {
        count = 0;
        std::fill(std::begin(c), std::end(c), 0.0f);
        std::fill(std::begin(s), std::end(s), 0.0f);
        std::fill(std::begin(dc), std::end(dc), 0.0f);
        std::fill(std::begin(ds), std::end(ds), 0.0f);
    }
    // Mode k: amplitude, start phase, radians per sample, held and damped decay per sample,
    // left/right gains.
    void set(int k, float amp, float phase, float radians, float held, float damped, float left, float right) {
        c[k] = amp * std::cos(phase);
        s[k] = amp * std::sin(phase);
        w[k] = radians;
        rHeld[k] = held;
        rDamped[k] = damped;
        gl[k] = left;
        gr[k] = right;
        count = std::max(count, k + 1);
    }
    // Re-derives the rotation of every mode for a pitch ratio and damper state. Modes
    // that would pass Nyquist are silenced.
    void retune(float ratio, bool damped) {
        for (int k = 0; k < count; ++k) {
            const float radians = w[k] * ratio;
            const float r = damped ? rDamped[k] : rHeld[k];
            if (radians >= 0.95f * pi) { c[k] = s[k] = 0; dc[k] = 1; ds[k] = 0; continue; }
            dc[k] = r * std::cos(radians);
            ds[k] = r * std::sin(radians);
        }
    }
    Stereo tick() {
        float accL[8] {}, accR[8] {};
        const int n = (count + 7) / 8 * 8;
        for (int k = 0; k < n; k += 8)
            for (int j = 0; j < 8; ++j) {
                const int q = k + j;
                const float nc = c[q] * dc[q] - s[q] * ds[q];
                const float ns = c[q] * ds[q] + s[q] * dc[q];
                c[q] = nc;
                s[q] = ns;
                accL[j] += ns * gl[q];
                accR[j] += ns * gr[q];
            }
        float l = 0, r = 0;
        for (int j = 0; j < 8; ++j) { l += accL[j]; r += accR[j]; }
        return { l, r };
    }
    // Loudest mode's amplitude (for stealing and for ending a voice).
    float peak() const {
        float m = 0;
        for (int k = 0; k < count; ++k) m = std::max(m, c[k] * c[k] + s[k] * s[k]);
        return std::sqrt(m);
    }
};

// Constant-power pan, p in [-1, 1].
inline Stereo panGains(float p) {
    const float a = (std::clamp(p, -1.0f, 1.0f) + 1.0f) * 0.25f * pi;
    return { std::cos(a) * 1.41421356f * 0.70710678f, std::sin(a) * 1.41421356f * 0.70710678f };
}

// Fast tanh (Pade 3/2, clamped): smooth saturation without std::tanh's cost.
inline float softClip(float x) {
    x = std::clamp(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

} // namespace liminal::dsp
