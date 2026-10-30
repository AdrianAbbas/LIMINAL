#include "DSP/HollowDrone.h"

namespace liminal::dsp {

void HollowDrone::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    reset();
}

void HollowDrone::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    rng.state = 0xA54FF53Au;
    bend = 1.0f;
    clock = 0;
}

void HollowDrone::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.15, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.level = 0.11f * (0.3f + 0.7f * std::pow(v.velocity, 1.3f));
    v.amp = 0;
    for (int k = 0; k < oscillators; ++k) {
        const auto i = std::size_t(k);
        v.phase[i] = rng.uniform();
        v.panPhase[i] = rng.uniform();
        v.drift[i].start(rng);
    }
    v.subPhase = v.sub2Phase = 0;
    v.cutoffDrift.start(rng);
    v.sweep = rng.uniform();
    v.filterL.clear();
    v.filterR.clear();
    control(v);
}

void HollowDrone::control(Voice& v) {
    const auto& st = settings;
    const float tick = float(controlPeriod / sr);
    const float base = noteHz(float(v.note)) * bend;
    // Density: 2..7 stacked oscillators, the last one fading in fractionally.
    const float stack = 2.0f + 5.0f * st.density;
    const float spreadCents = 4.0f + 14.0f * st.density;
    const float moveRate = 0.03f + 0.25f * st.movement;
    for (int k = 0; k < oscillators; ++k) {
        const auto i = std::size_t(k);
        float ratio = 1.0f, g = 0.0f, c = 0.0f;
        if (k < 7) {
            g = std::clamp(stack - float(k), 0.0f, 1.0f) * 1.6f / std::sqrt(stack);   // fuller, not much louder
            c = (k == 0 ? 0.0f : (k % 2 == 1 ? 1.0f : -1.0f) * float((k + 1) / 2) / 3.0f) * spreadCents;
        } else {
            ratio = k == 7 ? 1.5f : 2.0f;   // a fifth and an octave above, with Harmonics
            g = std::clamp(st.harmonics * 1.6f - (k == 7 ? 0.5f : 0.2f), 0.0f, 1.0f) * 0.5f;
        }
        c += v.drift[i].tick(rng, 0.08f + 0.2f * st.instability, tick) * (1.0f + 22.0f * st.instability);
        v.inc[i] = std::min(0.45f, base * ratio * cents(c) / float(sr));
        v.gain[i] = g;
        // Each oscillator circles slowly through the stereo field (Movement).
        v.panPhase[i] += tick * moveRate * (0.6f + 0.15f * float(k));
        if (v.panPhase[i] >= 1.0f) v.panPhase[i] -= 1.0f;
        const auto p = panGains((0.25f + 0.75f * st.movement) * std::sin(twoPi * v.panPhase[i]) * (k == 0 ? 0.3f : 1.0f));
        v.gl[i] = p.l;
        v.gr[i] = p.r;
    }
    v.subInc = std::min(0.45f, 0.5f * base / float(sr));
    v.sub2Inc = std::min(0.45f, 0.25f * base / float(sr));
    // The resonant low-pass: tracks the note, opens with Harmonics, breathes with Movement.
    v.sweep += tick * (0.02f + 0.1f * st.movement);
    if (v.sweep >= 1.0f) v.sweep -= 1.0f;
    const float octaves = st.movement * 1.2f * std::sin(twoPi * v.sweep) + v.cutoffDrift.tick(rng, 0.1f, tick) * st.instability * 0.7f;
    const float cutoff = std::clamp(base * (1.5f + 14.0f * st.harmonics * st.harmonics) * std::exp2(octaves) + 120.0f, 60.0f, 16000.0f);
    const float q = 1.2f + 3.0f * st.harmonics;
    v.filterL.set(cutoff, q, sr);
    v.filterR.set(cutoff * 1.06f, q, sr);
}

void HollowDrone::render(float* left, float* right, int n) {
    const auto& st = settings;
    const float attackCoef = 1.0f - std::exp(-1.0f / (std::max(0.002f, st.attack) * 0.3f * float(sr)));
    const float releaseCoef = 1.0f - std::exp(-1.0f / (std::max(0.02f, st.release) * 0.22f * float(sr)));
    // Harmonics morphs each oscillator from a soft, nearly sinusoidal shape to a sawtooth.
    const float bright = 0.15f + 0.85f * st.harmonics;
    const float subGain = st.sub * 0.9f, sub2Gain = std::max(0.0f, st.sub - 0.5f) * 0.9f;
    int done = 0;
    while (done < n) {
        if (clock == 0) {
            for (auto& v : pool.voices)
                if (v.active()) control(v);
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        float* l = left + done;
        float* r = right + done;
        for (auto& v : pool.voices) {
            if (!v.active()) continue;
            const bool releasing = v.releasing();
            for (int i = 0; i < m; ++i) {
                v.amp += releasing ? releaseCoef * (0.0f - v.amp) : attackCoef * (1.02f - v.amp);
                if (!releasing) v.amp = std::min(v.amp, 1.0f);
                float sl = 0, sr_ = 0;
                for (int k = 0; k < oscillators; ++k) {
                    const auto q = std::size_t(k);
                    if (v.gain[q] <= 0.0f) continue;
                    const float ph = v.phase[q];
                    const float saw = blepSaw(ph, v.inc[q]);
                    const float soft = sinCycle(ph);
                    const float s = (soft + bright * (saw - soft)) * v.gain[q];
                    v.phase[q] = ph + v.inc[q] >= 1.0f ? ph + v.inc[q] - 1.0f : ph + v.inc[q];
                    sl += s * v.gl[q];
                    sr_ += s * v.gr[q];
                }
                v.filterL.tick(sl);
                v.filterR.tick(sr_);
                v.subPhase += v.subInc;
                if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
                v.sub2Phase += v.sub2Inc;
                if (v.sub2Phase >= 1.0f) v.sub2Phase -= 1.0f;
                const float sub = subGain * sinCycle(v.subPhase) + sub2Gain * sinCycle(v.sub2Phase);
                float g = v.amp * v.level;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                l[i] += (v.filterL.low + sub) * g;
                r[i] += (v.filterR.low + sub) * g;
            }
            // Voices end only at the end of a control period, so the block size never matters.
            if (m == clock && ((v.stolen && v.fade <= 0.0f) || (releasing && v.amp < 1.0e-4f))) v.note = -1;
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
