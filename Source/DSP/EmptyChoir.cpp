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
    remembered.setProfile(remembered::choir());
    remembered.prepare(sr);
    reset();
}

void EmptyChoir::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    for (auto& v : rpool.voices) v = RVoice {};
    rpool.pedal = false;
    rpool.counter = 0;
    humChanged = false;
    rng = {};
    rng.state = 0x3C6EF372u;
    bend = 1.0f;
    clock = 0;
    remembered.reset();
}

void EmptyChoir::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    // The recordings cover the voice's range; beyond it they fade into the formant choir.
    float share = 0.0f;
    if (recordings("voice-ooh") != nullptr) {
        const float below = std::clamp((float(note) - 40.0f) / 7.0f, 0.0f, 1.0f), above = std::clamp((86.0f - float(note)) / 7.0f, 0.0f, 1.0f);
        share = std::min(below, above);
        const NoteShape keep = nextShape;
        if (share > 0.0f && !startRecorded(note, velocity, share)) share = 0.0f;
        nextShape = keep;   // the formant voice of the same note gets the same shape
        if (share >= 1.0f) { nextShape = {}; return; }
    }
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.08, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.gain = 0.2f * (0.25f + 0.75f * std::pow(v.velocity, 1.5f)) * std::sqrt(std::max(0.0f, 1.0f - share * share));
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
    v.shape = takeShape();
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
    const float unstable = wander();   // Liminality: no movement of its own at Drift 0
    const float vib = vibDepth * std::sin(twoPi * v.vibPhase) * unstable;
    const float base = noteHz(float(v.note)) * bend * cents(shapeCents(v.shape, tick));
    for (int k = 0; k < singers; ++k) {
        const auto i = std::size_t(k);
        const float c = v.offset[i] * (2.0f + 22.0f * st.detune) + v.drift[i].tick(rng, 0.25f, tick) * (1.5f + 4.0f * st.detune) * unstable + vib;
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
    const float tilt = 900.0f * std::pow(3.0f, v.velocity) * (1.5f - 1.3f * st.darkness) * std::exp2(0.8f * v.shape.tone);
    v.tiltL.setHz(tilt, sr);
    v.tiltR.setHz(tilt, sr);
    const float dark = 12000.0f * std::pow(700.0f / 12000.0f, st.darkness) * std::exp2(0.5f * v.shape.tone);
    v.darkL.setHz(dark, sr);
    v.darkR.setHz(dark, sr);
}

void EmptyChoir::render(float* left, float* right, int n) {
    const auto& st = settings;
    const float attackCoef = 1.0f - std::exp(-1.0f / (0.35f * float(sr)));
    const float releaseCoef = 1.0f - std::exp(-1.0f / (std::max(0.02f, st.release) * 0.22f * float(sr)));
    const float breath = st.breath;
    const float recordedAttack = attackCoef;   // the same swell as the formant choir
    int done = 0;
    while (done < n) {
        if (clock == 0) {
            for (auto& v : pool.voices)
                if (v.active()) control(v);
            for (auto& v : rpool.voices)
                if (v.active()) controlRecorded(v, float(controlPeriod / sr));
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        // The voices into the choir's own buffer, then the remembered treatment, then out.
        float l[controlPeriod] {}, r[controlPeriod] {};
        if (humChanged) { updateHum(); humChanged = false; }
        for (auto& v : rpool.voices) {
            if (!v.active()) continue;
            const bool releasing = v.releasing();
            const float coef = releasing ? releaseCoef : recordedAttack;
            const float target = releasing ? 0.0f : 1.0f;
            for (int i = 0; i < m; ++i) {
                v.amp += coef * (target - v.amp);
                float sl = 0, sr_ = 0;
                for (int k = 0; k < readers; ++k) {
                    auto& rd = v.r[std::size_t(k)];
                    if (!rd.on) continue;
                    const float x = rd.tick();
                    sl += x * v.panL[std::size_t(k)];
                    sr_ += x * v.panR[std::size_t(k)];
                }
                float g = v.amp * v.gain;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                l[i] += v.distL.lp(v.darkL.lp(sl)) * g;
                r[i] += v.distR.lp(v.darkR.lp(sr_)) * g;
            }
            if (m == clock) {
                bool any = false;
                for (const auto& rd : v.r) any = any || rd.on;
                if (!v.stolen && ((releasing && v.amp < 1.0e-4f) || !any)) v.steal(0.05, sr);
                if (v.stolen && v.fade <= 0.0f) { v.note = -1; humChanged = true; }
            }
        }
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
        remembered.process(l, r, m, wander());
        for (int i = 0; i < m; ++i) {
            left[done + i] += l[i];
            right[done + i] += r[i];
        }
        clock -= m;
        done += m;
    }
}

//==============================================================================
// The recorded choir (Milestone 8).
namespace {
constexpr const char* sweepSets[3] { "voice-mm", "voice-ooh", "voice-aah" };
constexpr const char* stackSets[3] { "choir-mm", "choir-ooh", "choir-aah" };
constexpr float referenceDb = -20.0f;   // every take levelled to this loudness, then the voice's gain
constexpr float singerCents[3] { 7.0f, -9.0f, 4.0f };
constexpr float singerWaitMs[3] { 35.0f, 62.0f, 90.0f };
constexpr float singerPan[3] { -0.75f, 0.75f, -0.3f };
constexpr float singerDb = -4.5f;      // the distant singers
constexpr float ghostDb = -6.0f;       // at Breath 1
constexpr float humDb = -6.0f;
} // namespace

bool EmptyChoir::startRecorded(int note, float velocity, float share) {
    const auto& st = settings;
    const float p = std::clamp(st.formant * 3.0f, 0.0f, 2.0f);
    const int va = std::min(1, int(p));
    const float t = p - float(va);
    const float wv[2] { std::cos(1.5707963f * t), std::sin(1.5707963f * t) };
    // mmm was sung only in the chest voice: above it, its share goes to ooh.
    const auto sweep = [&](int vowel) { return recordings(sweepSets[(vowel == 0 && note > 72) ? 1 : vowel]); };
    const auto* lead = sweep(va);
    if (lead == nullptr) return false;
    auto& v = rpool.allocate(note, voices(polyphony), sr, 0.08, [this](const RVoice& x) { return level(x); });
    rpool.begin(v, note);
    v.shape = takeShape();
    for (auto& rd : v.r) rd.on = false;
    const float vel = std::clamp(velocity, 0.0f, 1.0f);
    const auto level = [](const samples::Zone* z) { return dbGain(referenceDb - z->loudnessDb); };
    const auto pan = [&](int k, float where) {
        const auto g = panGains(where);
        v.panL[std::size_t(k)] = g.l;
        v.panR[std::size_t(k)] = g.r;
    };
    // The lead: the two vowels either side of Formant (the soft takes).
    for (int k = 0; k < 2; ++k) {
        const auto* set = sweep(va + k);
        if (set == nullptr || wv[k] <= 0.001f) continue;
        const auto* z = set->find(note, 0.2f, 0.0f);
        if (z == nullptr || !v.r[std::size_t(k)].start(z, set->rate, sr, float(note), 1.0)) continue;
        v.r[std::size_t(k)].gain = wv[k] * level(z);
        v.cents[std::size_t(k)] = 0;
        pan(k, 0.0f);
    }
    // Three more singers, further away: a different take of the nearest choir stack each (else the
    // sweep again, from elsewhere in its sustain), later, detuned, spread across the room.
    const int vowel = int(std::lround(p));
    const auto* stack = recordings(stackSets[vowel]);
    const int distant = quality == 0 ? 1 : 3;   // Eco: one distant singer
    for (int k = 0; k < distant; ++k) {
        auto& rd = v.r[std::size_t(2 + k)];
        const samples::Zone* z = nullptr;
        const samples::Instrument* set = nullptr;
        if (stack != nullptr) {
            z = stack->find(note, 0.5f, (float(k) + 0.5f) / 3.0f);
            set = stack;
            if (z != nullptr && std::abs(z->root - float(note)) > 5.01f) z = nullptr;
        }
        double from = 1.0;
        if (z == nullptr) {
            set = sweep(vowel);
            z = set != nullptr ? set->find(note, 0.8f, 0.0f) : nullptr;
            if (z != nullptr && z->loops()) from = double(z->loopStart) + double(z->loopEnd - z->loopStart) * (0.25 + 0.25 * k);
        }
        if (z == nullptr || !rd.start(z, set->rate, sr, float(note), from)) continue;
        rd.gain = dbGain(singerDb) * level(z);
        rd.wait = int(singerWaitMs[k] * (0.5f + st.detune) * 0.001f * float(sr));
        v.cents[std::size_t(2 + k)] = singerCents[k] * (0.3f + 1.5f * st.detune);
        pan(2 + k, singerPan[k] * st.width);
    }
    // The ghost: the falsetto ooh an octave above, very faint, coming in slowly.
    if (const auto* ooh = recordings("choir-ooh"); ooh != nullptr && st.breath > 0.01f) {
        const auto* z = ooh->find(note + 12, 0.5f, 0.5f);
        auto& rd = v.r[5];
        if (z != nullptr && std::abs(z->root - float(note + 12)) <= 7.01f && rd.start(z, ooh->rate, sr, float(note + 12), 1.0, 0.0f)) {
            rd.gain = dbGain(ghostDb) * st.breath * level(z);
            rd.fadeTo(1.0f, 1.6f * float(sr));
            v.cents[5] = -5.0f;
            pan(5, 0.45f * st.width);
        }
    }
    // The hum, an octave or two under: only the lowest note held keeps it (updateHum).
    if (const auto* hum = recordings("voice-hum")) {
        int under = note - 12;
        while (under > 50) under -= 12;
        auto& rd = v.r[6];
        if (under >= 33) {
            const auto* z = hum->find(under, 0.5f, 0.0f);
            if (z != nullptr && rd.start(z, hum->rate, sr, float(under), 1.0, 0.0f)) {
                rd.gain = dbGain(humDb) * level(z);
                v.cents[6] = 0;
                pan(6, 0.0f);
            }
        }
    }
    // Lead + three singers (+ hum) summed by power: the voice's level is the lead's.
    v.gain = 0.2f * (0.25f + 0.75f * std::pow(vel, 1.5f)) * share * dbGain(recordedTrimDb) / std::sqrt(1.0f + float(distant) * dbGain(2.0f * singerDb));
    v.amp = 0;
    // Faded: darker (Darkness, Liminality's Unevenness) and a little distant.
    const float dark = 12000.0f * std::pow(700.0f / 12000.0f, 0.75f * st.darkness) * std::exp2(0.5f * v.shape.tone);
    v.darkL.setHz(dark, sr);
    v.darkR.setHz(dark * 1.04f, sr);
    v.distL.setHz(7000.0f, sr);
    v.distR.setHz(6600.0f, sr);
    v.darkL.z = v.darkR.z = v.distL.z = v.distR.z = 0;
    controlRecorded(v, 0.0f);
    humChanged = true;
    return true;
}

void EmptyChoir::controlRecorded(RVoice& v, float tick) {
    const float base = shapeCents(v.shape, tick);
    for (int k = 0; k < readers; ++k)
        if (v.r[std::size_t(k)].on) v.r[std::size_t(k)].retune(bend * cents(base + v.cents[std::size_t(k)]));
}

// The hum belongs to the lowest note still held (or sustained); it moves over in 0.8 s.
void EmptyChoir::updateHum() {
    int lowest = 128;
    for (const auto& v : rpool.voices)
        if (v.active() && !v.stolen && !v.releasing() && v.r[6].on) lowest = std::min(lowest, v.note);
    for (auto& v : rpool.voices)
        if (v.active() && v.r[6].on) v.r[6].fadeTo(v.note == lowest && !v.releasing() ? 1.0f : 0.0f, 0.8f * float(sr));
}

} // namespace liminal::dsp
