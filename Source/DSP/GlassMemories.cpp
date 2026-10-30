#include "DSP/GlassMemories.h"

namespace liminal::dsp {

namespace {
// (n,0) modes of a wine glass relative to (2,0); the last is extrapolated.
constexpr std::array<float, GlassMemories::modeCount> ratios { 1.0f, 1.86f, 3.25f, 4.95f, 7.0f, 9.35f };
constexpr std::array<float, GlassMemories::modeCount> strengths { 1.0f, 0.7f, 0.45f, 0.32f, 0.2f, 0.13f };
constexpr std::array<float, 3> singHarmonics { 1.0f, 0.22f, 0.08f };
} // namespace

void GlassMemories::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    reset();
}

void GlassMemories::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    rng.state = 0x6A09E667u;
    bend = 1.0f;
    clock = 0;
}

void GlassMemories::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const auto& st = settings;
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.04, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);

    const float frag = st.fragility, bright = st.brightness;
    // Pitch instability: each strike a little off (fixed for the note), then a slow wander.
    const float f1 = noteHz(float(note)) * cents(rng.bipolar() * st.detune * 15.0f);
    // Strike: a raised-cosine pulse, 4 ms (soft mallet) down to 0.16 ms (hard tick).
    const float hardness = std::clamp(0.1f + 0.5f * bright + 0.45f * velocity, 0.0f, 1.0f);
    const float strike = 0.004f * std::pow(0.04f, hardness);
    const float tilt = 0.6f + 0.6f * bright;
    const float ring = 1.2f * std::pow(20.0f / 1.2f, st.decay) * std::clamp(std::pow(880.0f / f1, 0.3f), 0.5f, 2.0f);
    const float gain = 0.26f * (0.04f + 0.96f * std::pow(velocity, 1.8f));
    const float pan = std::clamp(float(note - 72) / 48.0f, -1.0f, 1.0f) * 0.3f;
    const auto leftSide = panGains(pan - 0.25f), rightSide = panGains(pan + 0.25f), middle = panGains(pan);
    const float limit = 0.45f * float(sr);

    v.struck.clear();
    float energy = 0;
    std::array<float, modeCount> amps {};
    for (int k = 0; k < modeCount; ++k) {
        const float f = f1 * ratios[std::size_t(k)];
        const float h = 1.0f / std::sqrt(1.0f + std::pow(f * strike * 1.5f, 4.0f));
        const float irregular = 1.0f + frag * 0.3f * rng.bipolar();   // where the strike lands
        amps[std::size_t(k)] = f < limit ? strengths[std::size_t(k)] * std::pow(tilt, float(k)) * h * irregular : 0.0f;
        energy += amps[std::size_t(k)] * amps[std::size_t(k)];
    }
    const float norm = gain / std::sqrt(std::max(1.0e-9f, energy));
    for (int k = 0; k < modeCount; ++k) {
        const float ratio = ratios[std::size_t(k)] * (1.0f + frag * 0.006f * float(k) * rng.bipolar());   // no two glasses alike
        const float f = f1 * ratio;
        const float t = ring * std::pow(ratios[std::size_t(k)], -0.8f) * (k == 0 ? 1.0f + st.resonance : 1.0f);
        const float damped = std::max(0.4f, 0.4f * t);   // a finger rests on the rim
        // The doublet: two nearly equal modes, a fraction of a hertz apart.
        const float split = (0.08f + 1.4f * frag * frag) * std::sqrt(f / 1000.0f);
        const float a = amps[std::size_t(k)] * norm;
        const float phase = twoPi * rng.uniform();
        v.struck.set(2 * k, 0.55f * a, phase, twoPi * (f - 0.5f * split) / float(sr), t60(t, sr), t60(damped, sr), leftSide.l, leftSide.r);
        v.struck.set(2 * k + 1, 0.45f * a, phase, twoPi * (f + 0.5f * split) / float(sr), t60(t, sr), t60(damped, sr), rightSide.l, rightSide.r);
    }
    // Fragility: two short, brittle partials that belong to no clean mode.
    const float crack = 0.25f * frag * frag * norm;
    const float crackT = 0.25f + 0.4f * (1.0f - frag);
    v.struck.set(2 * modeCount, crack, 0.0f, twoPi * f1 * (2.4f + 0.3f * keyHash(note, 21)) / float(sr), t60(crackT, sr), t60(crackT * 0.5f, sr), middle.l, middle.r);
    v.struck.set(2 * modeCount + 1, crack * 0.7f, 0.0f, twoPi * f1 * (5.6f + 0.6f * keyHash(note, 22)) / float(sr), t60(crackT * 0.6f, sr),
                 t60(crackT * 0.3f, sr), middle.l, middle.r);

    // The rubbed rim (Resonance): the lowest mode and its harmonics, which do not decay
    // while the key is held; their level is the `sing` envelope.
    v.singing.clear();
    for (int h = 0; h < 3; ++h) {
        const float f = f1 * float(h + 1);
        const float a = f < limit ? singHarmonics[std::size_t(h)] : 0.0f;
        const float split = (0.08f + 1.4f * frag * frag) * std::sqrt(f / 1000.0f);
        v.singing.set(2 * h, 0.55f * a, 0.0f, twoPi * (f - 0.5f * split) / float(sr), 1.0f, 1.0f, leftSide.l, leftSide.r);
        v.singing.set(2 * h + 1, 0.45f * a, 0.0f, twoPi * (f + 0.5f * split) / float(sr), 1.0f, 1.0f, rightSide.l, rightSide.r);
    }
    v.sing = 0;
    v.singTarget = st.resonance * 0.45f * gain * (0.4f + 0.6f * velocity);
    v.singRise = 1.0f - std::exp(-1.0f / (0.35f * float(sr)));
    v.singFall = 1.0f - t60(0.8, sr);

    v.attack = 0;
    v.attackStep = 1.0f / std::max(1.0f, strike * float(sr));
    v.tick = 0.6f * bright * (0.3f + frag) * velocity * gain;
    v.tickDecay = t60(0.004, sr);
    v.tickPrev = 0;
    v.noise.state = rng.state ^ 0xA5A5A5A5u;
    v.noise.uniform();
    v.wobble.start(rng);
    v.flutter.start(rng);
    v.flutterNow = 1;
    v.damped = false;
    v.wobbleRatio = 1.0f;
    v.struck.retune(bend, false);
    v.singing.retune(bend, false);
    v.level = gain;
}

void GlassMemories::pitchBend(int value14bit) {
    bend = bendRatio(value14bit);
    for (auto& v : pool.voices)
        if (v.active()) {
            v.struck.retune(bend * v.wobbleRatio, v.damped);
            v.singing.retune(bend * v.wobbleRatio, false);
        }
}

void GlassMemories::control(Voice& v) {
    const float tick = float(controlPeriod / sr);
    const float wander = v.wobble.tick(rng, 0.4f, tick) * settings.detune * 6.0f;
    v.flutterNow = 1.0f + 0.12f * v.flutter.tick(rng, 3.0f, tick);   // the rubbed rim's stick-slip unevenness
    v.wobbleRatio = cents(wander);
    v.damped = v.releasing();
    v.struck.retune(bend * v.wobbleRatio, v.damped);
    v.singing.retune(bend * v.wobbleRatio, false);
    v.level = v.struck.peak();
}

void GlassMemories::render(float* left, float* right, int n) {
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
            const float target = releasing ? 0.0f : v.singTarget;
            const bool singing = v.sing > 1.0e-7f || target > 0.0f;
            for (int i = 0; i < m; ++i) {
                Stereo o = v.struck.tick();
                if (v.attack < 1.0f) {
                    v.attack = std::min(1.0f, v.attack + v.attackStep);
                    const float g = 0.5f - 0.5f * std::cos(pi * v.attack);
                    o.l *= g;
                    o.r *= g;
                }
                if (singing) {
                    v.sing += (releasing ? v.singFall : v.singRise) * (target - v.sing);
                    const Stereo s = v.singing.tick();
                    const float g = v.sing * v.flutterNow;
                    o.l += s.l * g;
                    o.r += s.r * g;
                }
                if (v.tick > 1.0e-7f) {   // the brittle tick: a few ms of high-passed noise
                    const float x = v.noise.bipolar();
                    const float hp = x - v.tickPrev;
                    v.tickPrev = x;
                    o.l += hp * v.tick;
                    o.r += hp * v.tick;
                    v.tick *= v.tickDecay;
                }
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    o.l *= v.fade;
                    o.r *= v.fade;
                }
                l[i] += o.l;
                r[i] += o.r;
            }
            // Voices end only at the end of a control period, so the block size never matters.
            if (m == clock && ((v.stolen && v.fade <= 0.0f) || (v.level < 2.0e-5f && v.sing < 2.0e-5f && releasing && v.attack >= 1.0f))) v.note = -1;
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
