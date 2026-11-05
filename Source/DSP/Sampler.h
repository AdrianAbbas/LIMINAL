#pragma once
// Milestone 8: what the recorded instruments share. A Reader plays one recorded note (a Zone of
// the sample bank, Source/Samples/SampleBank): a cubic read at any rate, its sustain loop while
// the note is held, its recorded release tail when let go, and a weight that moves along an
// equal-power curve (crossfades between velocity layers, the piano's short->long handover, a
// vowel or a patch turned to). Real-time safe: no allocation, nothing but arithmetic.
//
// Each instrument keeps its synthesis as the stand-in: it asks `recordings()` at every note-on
// (nullptr while the bank is loading, missing or bypassed by a test) and plays its synthesised
// voice when there are none, so a build without the recordings sounds as before.
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include "Samples/SampleBank.h"

namespace liminal::dsp {

inline float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }

// An instrument's recorded set, or nullptr (not loaded / missing / bypassed).
inline const samples::Instrument* recordings(const char* id) { return samples::Bank::get().instrument(id); }

// sin(pi/2 x) for x in [0, 1] (equal-power fade-in curve; cos is the same mirrored).
inline float fadeIn(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * (1.5707963f - 0.5707963f * x * x);   // exact at 0, 1/2 within 1 %, 1
}

struct Reader {
    const samples::Zone* zone = nullptr;
    double pos = 0, tailPos = -1;
    float base = 1;        // rate ratio: recording rate / output rate
    float semis = 0;       // the key's pitch relative to the recording's root (semitones)
    float inc = 1;
    float gain = 1;        // fixed (levels, layers)
    float x = 1, xStep = 0, xTarget = 1;   // the weight's position on its equal-power curve
    float w = 1;           // fadeIn(x)
    float outRate = 48000;
    float tailMix = 0, tailStep = 0;
    int wait = 0;          // samples of silence before it speaks
    float ramp = 1, rampStep = 0;   // a start mid-recording or after a wait ramps in (4 ms): never a step
    bool on = false, released = false;
    bool holdLoop = false;   // let go before its sustain: it keeps looping, the instrument's envelope ends it

    static float read(const int16_t* d, double p) {
        const auto i = int(p);
        const float t = float(p - double(i));
        const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return (((c3 * t + c2) * t + c1) * t + x0) * (1.0f / 32768.0f);
    }
    // Starts `z` (of a set recorded at `rate`) for a key whose pitch is `pitch` (MIDI, fractional),
    // at sample `from` of the recording.
    bool start(const samples::Zone* z, float rate, double sampleRate, float pitch, double from = 0, float weight = 1) {
        zone = z;
        if (z == nullptr || z->length < 16) return on = false;
        base = float(rate / sampleRate);
        semis = pitch - z->root;
        inc = base * std::exp2(semis / 12.0f);
        pos = std::clamp(from, 1.0, double(z->length - 3));
        tailPos = -1;
        tailMix = 0;
        gain = 1;
        x = xTarget = std::clamp(weight, 0.0f, 1.0f);
        w = fadeIn(x);
        xStep = 0;
        outRate = float(sampleRate);
        wait = 0;
        released = false;
        holdLoop = false;
        rampStep = 1.0f / std::max(1.0f, 0.004f * outRate);
        // (only a full-weight start well past the recording's onset: a note's own attack, and a layer that
        // fades in by its weight, are left exactly as they were)
        ramp = (pos > double(std::max(2048, z->onset + 2048)) && x >= 0.999f) ? 0.0f : 1.0f;
        return on = true;
    }
    // Pitch: `ratio` multiplies the key's rate (bend, Liminality, drift); `extraSemis` moves it.
    void retune(float ratio, float extraSemis = 0.0f) { inc = base * std::exp2((semis + extraSemis) / 12.0f) * ratio; }
    // Moves the weight to `target` over `samples` (equal power along the way).
    void fadeTo(float target, float samples) {
        xTarget = std::clamp(target, 0.0f, 1.0f);
        xStep = (xTarget - x) / std::max(1.0f, samples);
    }
    // Key up: out of the loop into the recorded release tail (40 ms crossfade) if there is one.
    // A note let go before it reached its sustain (a staccato note still in its attack) does not jump
    // to the tail: the tail was recorded at the full sustain level, so a short note bloomed up to
    // 8 dB louder after its note-off and rang on (2026-11-05: "hangs on random notes" in fast
    // patterns). It goes on looping instead, and the instrument's release envelope ends it.
    void release() {
        if (!on || released) return;
        released = true;
        const auto& z = *zone;
        if (z.loops() && pos < double(z.loopStart)) { holdLoop = true; return; }
        // No recorded tail: it goes on looping too (the samples before the loop's end were blended with
        // its start when it was prepared, so playing on past the end is a step).
        if (z.loops() && z.release <= 0) { holdLoop = true; return; }
        if (z.loops() && z.release > 0 && pos < double(z.release)) {
            tailPos = double(z.release);
            tailMix = 0;
            tailStep = 1.0f / (0.04f * outRate);
        }
    }
    float tick() {
        if (!on) return 0.0f;
        if (wait > 0) {
            if (--wait == 0) ramp = 0.0f;
            return 0.0f;
        }
        const auto& z = *zone;
        float y = read(z.data, pos);
        if (ramp < 1.0f) { y *= ramp; ramp = std::min(1.0f, ramp + rampStep); }
        bool moved = false;
        if (tailPos >= 0.0) {
            y = y * (1.0f - tailMix) + read(z.data, tailPos) * tailMix;
            tailPos += double(inc);
            tailMix += tailStep;
            if (tailMix >= 1.0f || tailPos >= double(z.length - 3)) {   // the tail carries on from where it is
                pos = tailPos;                                         // (it used to step on a second sample here)
                tailPos = -1;
                moved = true;
            }
        }
        if (!moved) pos += double(inc);
        // While the release tail cross-fades in, the loop keeps looping under it (2026-11-05: a released
        // note's head ran on past the loop's end into the unblended samples: a click after every
        // pedal-up and note-off that fell near the end of a loop).
        if (z.loops() && (!released || holdLoop || tailPos >= 0.0) && pos >= double(z.loopEnd)) pos -= double(z.loopEnd - z.loopStart);
        if (xStep != 0.0f) {
            x += xStep;
            if ((xStep > 0.0f && x >= xTarget) || (xStep < 0.0f && x <= xTarget)) { x = xTarget; xStep = 0; }
            w = fadeIn(x);
            if (x <= 0.0f) on = false;
        }
        if (pos >= double(z.length - 3)) on = false;
        // The last 1024 samples of a recording fade out (a note trimmed where it fell 50 dB never
        // ends in a step).
        const double rem = double(z.length - 3) - pos;
        if (rem < 1024.0 && ((released && !holdLoop) || !z.loops())) y *= float(std::max(0.0, rem) * (1.0 / 1024.0));
        return y * gain * w;
    }
    // Moves on `n` samples without reading (cheap; a layer at zero weight keeps its place).
    void skip(int n) {
        if (!on) return;
        if (wait > 0) { const int w = std::min(wait, n); wait -= w; n -= w; if (wait == 0) ramp = 0.0f; }
        if (n <= 0) return;
        ramp = std::min(1.0f, ramp + rampStep * float(n));
        const auto& z = *zone;
        bool moved = false;
        if (tailPos >= 0.0) {   // a release tail cross-fading in moves on too
            tailPos += double(inc) * double(n);
            tailMix += tailStep * float(n);
            if (tailMix >= 1.0f || tailPos >= double(z.length - 3)) { pos = tailPos; tailPos = -1; moved = true; }
        }
        if (!moved) pos += double(inc) * double(n);
        if (xStep != 0.0f) {
            x += xStep * float(n);
            if ((xStep > 0.0f && x >= xTarget) || (xStep < 0.0f && x <= xTarget)) { x = xTarget; xStep = 0; }
            w = fadeIn(x);
            if (x <= 0.0f) on = false;
        }
        if (z.loops() && (!released || holdLoop || tailPos >= 0.0)) while (pos >= double(z.loopEnd)) pos -= double(z.loopEnd - z.loopStart);
        if (pos >= double(z.length - 3)) on = false;
    }
    // Samples left before the recording ends (at the current rate), or a large number while looping.
    double samplesLeft() const {
        if (!on || zone == nullptr) return 0;
        if (zone->loops() && (!released || holdLoop)) return 1.0e9;
        return (double(zone->length - 3) - pos) / std::max(1.0e-6, double(inc));
    }
};

// A gentle tone control per voice: y = lp + tilt * (x - lp). tilt < 1 darker, > 1 brighter.
struct Tilt {
    OnePole lp;
    float tilt = 1;
    void set(float cornerHz, float amount, double sr) { lp.setHz(cornerHz, sr); tilt = amount; }
    float tick(float x) { const float l = lp.lp(x); return l + tilt * (x - l); }
    void clear() { lp.z = 0; }
};

// Velocity layers crossfaded (three recorded dynamics, e.g. soft / medium / full): the two layers
// either side of a velocity and their equal-power weights. `centres` are where each layer sounds
// as recorded.
struct LayerMix { int a = 0, b = 0; float wa = 1, wb = 0; };
inline LayerMix layerMix(float velocity, const float* centres, int layers) {
    LayerMix m;
    if (layers <= 1 || velocity <= centres[0]) { m.a = m.b = 0; return m; }
    if (velocity >= centres[layers - 1]) { m.a = m.b = layers - 1; return m; }
    for (int i = 0; i + 1 < layers; ++i)
        if (velocity <= centres[i + 1]) {
            const float t = (velocity - centres[i]) / (centres[i + 1] - centres[i]);
            m.a = i;
            m.b = i + 1;
            m.wa = std::cos(1.5707963f * t);
            m.wb = std::sin(1.5707963f * t);
            return m;
        }
    return m;
}

} // namespace liminal::dsp
