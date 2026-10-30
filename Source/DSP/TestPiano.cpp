#include "DSP/TestPiano.h"
#include <algorithm>
#include <cmath>

namespace liminal::dsp {

namespace {
constexpr float twoPi = 6.283185307f;
constexpr float silent = 1.0e-4f;   // -80 dB: a released voice below this is finished
float perSample(double seconds60, double sr) { return float(std::exp(-6.907755 / std::max(1.0e-4, seconds60 * sr))); }
} // namespace

void TestPiano::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    reset();
}

void TestPiano::reset() {
    for (auto& v : voices) v = Voice {};
    pedal = false;
    bend = 1.0f;
    rng = 0x1234567u;
    counter = 0;
}

float TestPiano::noise() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return float(rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

float TestPiano::level(const Voice& v) const {
    float peak = 0;
    for (int p = 0; p < partials; ++p) peak = std::max(peak, v.amp[p] * (0.65f * v.fast[p] + 0.35f * v.slow[p]));
    return peak * v.fade;
}

int TestPiano::activeVoices() const {
    return int(std::count_if(voices.begin(), voices.end(), [](const Voice& v) { return v.note >= 0; }));
}

void TestPiano::tune(Voice& v) {
    // Stiff-string stretch: f_n = n f0 sqrt(1 + B n^2), B larger for higher notes.
    const float b = 0.00015f * std::pow(2.0f, float(v.note - 21) / 24.0f);
    for (int k = 0; k < partials * 2; ++k) {
        const float hz = v.baseHz * bend * v.ratio[std::size_t(k)];
        const float n = float(k / 2 + 1);
        const float stretched = hz * std::sqrt(1.0f + b * n * n);
        auto& o = v.osc[std::size_t(k)];
        if (stretched >= 0.45f * float(sr)) {
            o.dc = 1;
            o.ds = 0;
            v.amp[std::size_t(k / 2)] = 0;
            continue;
        }
        const float w = twoPi * stretched / float(sr);
        o.dc = std::cos(w);
        o.ds = std::sin(w);
    }
}

void TestPiano::start(Voice& v, int note, float velocity) {
    v = Voice {};
    v.note = note;
    v.held = true;
    v.sustained = false;
    v.velocity = velocity;
    v.started = ++counter;
    v.baseHz = 440.0f * std::pow(2.0f, float(note - 69) / 12.0f);
    const float pan = std::clamp(float(note - 64) / 64.0f, -1.0f, 1.0f) * 0.35f;
    v.panL = std::cos((pan + 1.0f) * 0.25f * 3.14159265f) * 1.41421356f * 0.7071f;
    v.panR = std::sin((pan + 1.0f) * 0.25f * 3.14159265f) * 1.41421356f * 0.7071f;

    // Two strings per partial, detuned by a little under a cent each way (varies per note).
    const float cents = 0.5f + 0.5f * (noise() * 0.5f + 0.5f);
    const float up = std::pow(2.0f, cents / 1200.0f), down = 1.0f / up;
    const float brightness = 0.35f + 0.6f * velocity;
    const float gain = 0.4f * (0.05f + 0.95f * std::pow(velocity, 1.7f));
    const double base = std::clamp(9.0 * std::pow(262.0 / double(v.baseHz), 0.6), 0.6, 22.0);
    // Dampers stop the strings in about a quarter of a second; the top octave and a half
    // of a piano has none.
    const bool damped = note < 89;
    for (int p = 0; p < partials; ++p) {
        const float n = float(p + 1);
        v.ratio[std::size_t(2 * p)] = n * up;
        v.ratio[std::size_t(2 * p + 1)] = n * down;
        v.amp[std::size_t(p)] = gain * std::pow(brightness, n - 1.0f) / std::pow(n, 0.8f);
        const double slowT = base / (1.0 + 0.3 * (n - 1.0));
        v.slowMul[std::size_t(p)] = perSample(slowT, sr);
        v.fastMul[std::size_t(p)] = perSample(slowT / 6.0, sr);
        v.release[std::size_t(p)] = damped ? perSample(0.18 + 0.1 * double(89 - note) / 68.0, sr) : 1.0f;
        v.fast[std::size_t(p)] = 1;
        v.slow[std::size_t(p)] = 1;
        // Each partial starts at its own phase (so chords do not all peak together), the
        // same for both strings: struck together, they begin in phase and then beat.
        const float phase = (noise() * 0.5f + 0.5f) * twoPi;
        for (int s = 0; s < 2; ++s) {
            v.osc[std::size_t(2 * p + s)].c = std::cos(phase);
            v.osc[std::size_t(2 * p + s)].s = std::sin(phase);
        }
    }
    tune(v);
    const float attackSeconds = 0.0025f - 0.0015f * velocity;
    v.attackStep = 1.0f / std::max(1.0f, attackSeconds * float(sr));
    v.hammer = 0.13f * velocity;
    v.hammerMul = perSample(0.012 - 0.006 * velocity, sr);
    const float cutoff = std::min(0.45f * float(sr), 900.0f + 5000.0f * velocity);
    v.hammerCoef = 1.0f - std::exp(-twoPi * cutoff / float(sr));
}

void TestPiano::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) {
        noteOff(note);
        return;
    }
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    // A repeated note restrikes: the old one fades out quickly (as a re-struck string).
    int sounding = 0;
    for (auto& v : voices)
        if (v.note >= 0 && !v.stolen) {
            if (v.note == note) {
                v.stolen = true;
                v.fadeStep = 1.0f / float(0.02 * sr);
            } else {
                ++sounding;
            }
        }
    // Over the polyphony: the quietest note fades out in 5 ms (no click).
    if (sounding >= polyphony) {
        Voice* quietest = nullptr;
        for (auto& v : voices)
            if (v.note >= 0 && !v.stolen && (quietest == nullptr || level(v) < level(*quietest))) quietest = &v;
        if (quietest != nullptr) {
            quietest->stolen = true;
            quietest->fadeStep = 1.0f / float(0.005 * sr);
        }
    }
    Voice* slot = nullptr;
    for (auto& v : voices)
        if (v.note < 0) { slot = &v; break; }
    if (slot == nullptr) // every slot busy, even the fading ones: take the quietest fading one
        for (auto& v : voices)
            if (slot == nullptr || level(v) < level(*slot)) slot = &v;
    start(*slot, note, velocity);
}

void TestPiano::releaseVoice(Voice& v) {
    v.held = false;
    v.sustained = pedal;
}

void TestPiano::noteOff(int note) {
    for (auto& v : voices)
        if (v.note == note && v.held && !v.stolen) releaseVoice(v);
}

void TestPiano::sustain(bool down) {
    pedal = down;
    if (!down)
        for (auto& v : voices) v.sustained = false;
}

void TestPiano::pitchBend(int value14bit) {
    const float semis = 2.0f * float(std::clamp(value14bit, 0, 16383) - 8192) / 8192.0f;
    const float next = std::pow(2.0f, semis / 12.0f);
    if (std::abs(next - bend) < 1.0e-6f) return;
    bend = next;
    for (auto& v : voices)
        if (v.note >= 0) tune(v);
}

void TestPiano::allNotesOff() {
    pedal = false;
    for (auto& v : voices) {
        v.held = false;
        v.sustained = false;
    }
}

void TestPiano::allSoundOff() {
    allNotesOff();
    for (auto& v : voices)
        if (v.note >= 0 && !v.stolen) {
            v.stolen = true;
            v.fadeStep = 1.0f / float(0.01 * sr);
        }
}

void TestPiano::render(float* left, float* right, int n) {
    for (auto& v : voices) {
        if (v.note < 0) continue;
        const bool damping = !v.held && !v.sustained;
        for (int i = 0; i < n; ++i) {
            float out = 0;
            for (int p = 0; p < partials; ++p) {
                auto& fast = v.fast[std::size_t(p)];
                auto& slow = v.slow[std::size_t(p)];
                fast *= v.fastMul[std::size_t(p)];
                slow *= v.slowMul[std::size_t(p)];
                if (damping) {
                    fast *= v.release[std::size_t(p)];
                    slow *= v.release[std::size_t(p)];
                }
                auto& a = v.osc[std::size_t(2 * p)];
                auto& b = v.osc[std::size_t(2 * p + 1)];
                const float ac = a.c * a.dc - a.s * a.ds, as = a.c * a.ds + a.s * a.dc;
                const float bc = b.c * b.dc - b.s * b.ds, bs = b.c * b.ds + b.s * b.dc;
                a.c = ac; a.s = as; b.c = bc; b.s = bs;
                out += v.amp[std::size_t(p)] * (0.65f * fast + 0.35f * slow) * 0.5f * (as + bs);
            }
            // Felt hammer: a short, velocity-coloured thump of filtered noise.
            v.hammerLp += v.hammerCoef * (noise() - v.hammerLp);
            out += v.hammer * v.hammerLp;
            v.hammer *= v.hammerMul;
            if (v.attack < 1.0f) {
                v.attack = std::min(1.0f, v.attack + v.attackStep);
                out *= v.attack;
            }
            if (v.stolen) {
                v.fade = std::max(0.0f, v.fade - v.fadeStep);
                out *= v.fade;
            }
            left[i] += out * v.panL;
            right[i] += out * v.panR;
        }
        // Keep the oscillators on the unit circle (rounding drifts over minutes).
        for (auto& o : v.osc) {
            const float g = (3.0f - (o.c * o.c + o.s * o.s)) * 0.5f;
            o.c *= g;
            o.s *= g;
        }
        if ((v.stolen && v.fade <= 0.0f) || (level(v) < silent * 0.16f && v.hammer < 1.0e-6f && v.attack >= 1.0f))
            v.note = -1;
    }
}

} // namespace liminal::dsp
