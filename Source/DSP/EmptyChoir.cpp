#include "DSP/EmptyChoir.h"

namespace liminal::dsp {

namespace {
// Alto-ish vowel formants (Hz, bandwidth Hz, level dB), after the usual formant tables
// (Peterson & Barney; the Csound manual's vowel table). See docs/INSTRUMENT_NOTES.md.
struct Vowel { float f[4], bw[4], db[4]; };
constexpr Vowel vowels[] {
    { { 350, 600, 2400, 3300 }, { 60, 80, 120, 150 }, { 0, -10, -28, -32 } },   // oo
    { { 450, 800, 2830, 3500 }, { 70, 80, 100, 130 }, { 0, -6, -24, -30 } },    // oh
    { { 800, 1150, 2900, 3900 }, { 80, 90, 120, 130 }, { 0, -4, -20, -30 } },   // ah
    { { 400, 1600, 2700, 3300 }, { 60, 80, 120, 150 }, { 0, -10, -12, -20 } },  // eh
    { { 300, 2000, 2900, 3500 }, { 50, 100, 120, 130 }, { 0, -10, -10, -16 } }, // ee
};
constexpr int vowelCount = 5;
} // namespace

void EmptyChoir::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    reset();
}

void EmptyChoir::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    rng.state = 0x3C6EF372u;
    bend = 1.0f;
    clock = 0;
}

void EmptyChoir::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    auto& v = pool.allocate(note, polyphony, sr, 0.08, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.gain = 0.2f * (0.25f + 0.75f * std::pow(v.velocity, 1.5f));
    v.amp = 0;
    v.age = 0;
    // The singers: fixed offsets spread symmetrically (Detune), each drifting on its own.
    constexpr float spread[singers] { -1.0f, -0.33f, 0.33f, 1.0f };
    for (int k = 0; k < singers; ++k) {
        v.phase[std::size_t(k)] = rng.uniform();
        v.offset[std::size_t(k)] = spread[k] * (0.15f + 0.85f * keyHash(note, 40 + k));
        v.drift[std::size_t(k)].start(rng);
    }
    v.vowel.start(rng);
    v.vibPhase = rng.uniform();
    v.vibRate = 4.6f + 0.8f * rng.uniform();
    v.noise.state = rng.state ^ 0x5bd1e995u;
    v.noise.uniform();
    for (int f = 0; f < formants; ++f) { v.formL[f].clear(); v.formR[f].clear(); }
    v.tiltL.z = v.tiltR.z = v.darkL.z = v.darkR.z = 0;
    control(v);
}

void EmptyChoir::control(Voice& v) {
    const auto& st = settings;
    const float tick = float(controlPeriod / sr);
    v.age += tick;
    // Vibrato: starts after about half a second, about 5 cents deep, around the pitch.
    v.vibPhase += v.vibRate * tick;
    if (v.vibPhase >= 1.0f) v.vibPhase -= 1.0f;
    const float vibDepth = 5.0f * std::clamp((v.age - 0.5f) / 1.5f, 0.0f, 1.0f);
    const float vib = vibDepth * std::sin(twoPi * v.vibPhase);
    const float base = noteHz(float(v.note)) * bend;
    for (int k = 0; k < singers; ++k) {
        const auto i = std::size_t(k);
        const float c = v.offset[i] * (2.0f + 22.0f * st.detune) + v.drift[i].tick(rng, 0.25f, tick) * (1.5f + 4.0f * st.detune) + vib;
        v.inc[i] = std::min(0.45f, base * cents(c) / float(sr));
        const auto g = panGains((k % 2 == 0 ? -1.0f : 1.0f) * st.width * (0.5f + 0.5f * float(k / 2)));
        v.panL[i] = g.l;
        v.panR[i] = g.r;
    }
    // Vowel position, wandering about a tenth of the way to the next vowel.
    const float pos = std::clamp(st.formant + 0.07f * v.vowel.tick(rng, 0.12f, tick), 0.0f, 1.0f) * float(vowelCount - 1);
    const int a = std::min(int(pos), vowelCount - 2);
    const float t = pos - float(a);
    for (int f = 0; f < formants; ++f) {
        const float hz = vowels[a].f[f] + t * (vowels[a + 1].f[f] - vowels[a].f[f]);
        const float bw = vowels[a].bw[f] + t * (vowels[a + 1].bw[f] - vowels[a].bw[f]);
        float db = vowels[a].db[f] + t * (vowels[a + 1].db[f] - vowels[a].db[f]);
        db -= st.darkness * 10.0f * float(f);   // darkness softens the upper formants
        v.formL[f].set(hz, hz / bw, sr);
        v.formR[f].set(hz * 1.02f, hz / bw, sr);   // a slightly different throat on each side
        v.formGain[f] = std::pow(10.0f, db / 20.0f);
    }
    // Spectral tilt of the source (brighter with velocity) and the darkness low-pass.
    const float tilt = 900.0f * std::pow(3.0f, v.velocity) * (1.5f - 1.3f * st.darkness);
    v.tiltL.setHz(tilt, sr);
    v.tiltR.setHz(tilt, sr);
    const float dark = 12000.0f * std::pow(700.0f / 12000.0f, st.darkness);
    v.darkL.setHz(dark, sr);
    v.darkR.setHz(dark, sr);
}

void EmptyChoir::render(float* left, float* right, int n) {
    const auto& st = settings;
    const float attackCoef = 1.0f - std::exp(-1.0f / (0.35f * float(sr)));
    const float releaseCoef = 1.0f - std::exp(-1.0f / (std::max(0.02f, st.release) * 0.22f * float(sr)));
    const float breath = st.breath;
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
                for (int k = 0; k < singers; ++k) {
                    const auto q = std::size_t(k);
                    const float s = blepSaw(v.phase[q], v.inc[q]);
                    v.phase[q] += v.inc[q];
                    if (v.phase[q] >= 1.0f) v.phase[q] -= 1.0f;
                    sl += s * v.panL[q];
                    sr_ += s * v.panR[q];
                }
                // Breath: noise, a little stronger in the open part of the first singer's cycle.
                const float air = breath * 1.6f * (0.6f + 0.4f * v.phase[0]);
                sl = v.tiltL.lp(sl) * (1.0f - 0.5f * breath) + air * v.noise.bipolar();
                sr_ = v.tiltR.lp(sr_) * (1.0f - 0.5f * breath) + air * v.noise.bipolar();
                float ol = 0, or_ = 0;
                for (int f = 0; f < formants; ++f) {
                    v.formL[f].tick(sl);
                    v.formR[f].tick(sr_);
                    ol += v.formL[f].band * v.formL[f].k * v.formGain[f];
                    or_ += v.formR[f].band * v.formR[f].k * v.formGain[f];
                }
                float g = v.amp * v.gain;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                l[i] += v.darkL.lp(ol) * g;
                r[i] += v.darkR.lp(or_) * g;
            }
            // Voices end only at the end of a control period, so the block size never matters.
            if (m == clock && ((v.stolen && v.fade <= 0.0f) || (releasing && v.amp < 1.0e-4f))) v.note = -1;
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
