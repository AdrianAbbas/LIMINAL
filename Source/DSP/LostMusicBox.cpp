#include "DSP/LostMusicBox.h"

namespace liminal::dsp {

namespace {
// Clamped-free beam bending modes (Euler-Bernoulli: 1, 6.267, 17.55), plus the faint
// harmonics a large tooth swing adds. Strength, and how much faster each one dies.
struct Mode { float ratio, strength, decayPower; };
constexpr Mode modes[] {
    { 1.0f, 1.0f, 0.0f }, { 2.0f, 0.07f, 0.9f }, { 3.0f, 0.025f, 1.0f },
    { 6.267f, 0.32f, 0.6f }, { 17.55f, 0.12f, 0.7f },
};
constexpr int modeCount = 5;
} // namespace

void LostMusicBox::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    tickDecay = t60(0.012, sr);
    reset();
}

void LostMusicBox::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    for (auto& v : rpool.voices) v = RVoice {};
    rpool.pedal = false;
    rpool.counter = 0;
    rng = {};
    rng.state = 0x510E527Fu;
    tickRng.state = 0x9B05688Cu;
    bend = 1.0f;
    clock = 0;
    motor.start(rng);
    flutter.start(rng);
    motorRatio = 1.0f;
    tickEnv = 0;
    tickCountdown = 0;
    busy = 0;
    tickTone.clear();
    tickTone.set(2600.0f, 3.0f, sr);
}

void LostMusicBox::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    if (startRecorded(note, velocity)) return;
    const auto& st = settings;
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.02, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);

    v.shape = takeShape();
    // Each tooth's wear is its own and the same every time it is plucked.
    const float wear = st.wear;
    const float mistune = (keyHash(note, 61) * 2.0f - 1.0f) * wear * 28.0f;   // cents
    const float dull = keyHash(note, 62) < 0.25f * wear ? 0.35f : 1.0f;        // a few dead teeth
    const float uneven = 1.0f - wear * 0.55f * keyHash(note, 63);
    const float ringVar = 1.0f + wear * (keyHash(note, 64) - 0.5f) * 1.2f;
    const float f1 = noteHz(float(note)) * cents(mistune);
    const float ring = 0.5f * std::pow(12.0f, st.decay) * std::clamp(std::pow(880.0f / f1, 0.45f), 0.35f, 2.5f) * ringVar * dull;
    const float gain = 0.3f * (0.08f + 0.92f * std::pow(velocity, 1.6f)) * uneven;
    const float bright = std::clamp(st.brightness * 0.8f + velocity * 0.3f + 0.3f * v.shape.tone, 0.0f, 1.0f);
    const float pan = std::clamp(float(note - 79) / 30.0f, -1.0f, 1.0f) * 0.4f;   // low teeth left, high right
    const auto g = panGains(pan);
    const float limit = 0.45f * float(sr);

    v.modes.clear();
    for (int k = 0; k < modeCount; ++k) {
        const auto& md = modes[k];
        const float stretch = k >= 3 ? 1.0f + wear * 0.03f * (keyHash(note, 70 + k) - 0.5f) : 1.0f;
        const float f = f1 * md.ratio * stretch;
        float a = md.strength * (k == 0 ? 1.0f : k < 3 ? 0.5f + 1.5f * bright : 0.12f + 2.0f * bright * bright);
        if (f >= limit) a = 0.0f;
        const float t = ring * std::pow(md.ratio, -md.decayPower);
        const float spread = (k % 2 == 0 ? 1.0f : -1.0f) * 0.15f;
        const auto side = panGains(pan + spread);
        v.modes.set(k, a * gain, k == 0 ? 0.0f : twoPi * rng.uniform(), twoPi * f / float(sr), t60(t, sr), t60(t, sr),
                    k == 0 ? g.l : side.l, k == 0 ? g.r : side.r);
    }
    // A bent tooth: the fundamental as a doublet that beats slowly.
    const float beat = wear * (0.4f + 2.5f * keyHash(note, 65));
    v.modes.set(modeCount, 0.35f * wear * gain, 0.0f, twoPi * (f1 + beat) / float(sr), t60(ring * 0.8f, sr), t60(ring * 0.8f, sr), g.l, g.r);
    v.ratio = cents(shapeCents(v.shape, 0.0f));
    v.modes.retune(bend * motorRatio * v.ratio, false);
    // The pin's click: a short noise burst through a small resonance, brighter with Brightness.
    v.click = st.mechanism * 1.2f * gain * (0.5f + bright);
    v.clickDecay = t60(0.015, sr);
    v.clickTone.clear();
    v.clickTone.set(std::min(0.4f * float(sr), 2500.0f + 4000.0f * bright + 0.5f * f1), 2.5f, sr);
    v.noise.state = rng.state ^ 0x1F83D9ABu;
    v.noise.uniform();
    v.level = gain;
    busy = 1.0f;
}

void LostMusicBox::control(Voice& v) {
    v.ratio = cents(shapeCents(v.shape, float(controlPeriod / sr)));
    v.modes.retune(bend * motorRatio * v.ratio, false);
    v.level = v.modes.peak();
}

void LostMusicBox::render(float* left, float* right, int n) {
    const auto& st = settings;
    int done = 0;
    while (done < n) {
        if (clock == 0) {
            const float tick = float(controlPeriod / sr);
            // The spring motor: a slow uneven wander and a faster, smaller flutter.
            const float c = wander() * st.drift * (18.0f * motor.tick(rng, 0.18f, tick) + 4.0f * flutter.tick(rng, 2.5f, tick));
            motorRatio = cents(c);
            bool any = false;
            for (auto& v : pool.voices)
                if (v.active()) { control(v); any = true; }
            for (auto& v : rpool.voices)
                if (v.active()) {
                    const float ratio = bend * motorRatio * cents(v.cents + shapeCents(v.shape, tick));
                    v.a.retune(ratio);
                    v.b.retune(ratio);
                    v.top.retune(ratio);
                    float lv = std::max(v.a.on ? v.a.gain : 0.0f, v.b.on ? v.b.gain : 0.0f);
                    if (v.top.on) lv = std::max(lv, v.top.gain);
                    v.level = lv * v.env;
                    any = true;
                }
            busy = any ? 1.0f : std::max(0.0f, busy - tick * 0.5f);
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        float* l = left + done;
        float* r = right + done;
        for (auto& v : pool.voices) {
            if (!v.active()) continue;
            for (int i = 0; i < m; ++i) {
                Stereo o = v.modes.tick();
                if (v.click > 1.0e-7f) {
                    v.clickTone.tick(v.noise.bipolar());
                    const float c = v.clickTone.band * v.click;
                    o.l += c;
                    o.r += c;
                    v.click *= v.clickDecay;
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
            if (m == clock && ((v.stolen && v.fade <= 0.0f) || (v.level < 2.0e-5f && v.click < 1.0e-6f))) v.note = -1;
        }
        for (auto& v : rpool.voices) {
            if (!v.active()) continue;
            for (int i = 0; i < m; ++i) {
                if (v.delay > 0) { --v.delay; continue; }
                float x = v.tone.tick(v.a.tick() + v.b.tick() + v.top.tick());
                v.env *= v.decayMul;
                x *= v.env;
                if (v.click > 1.0e-7f) {
                    v.clickTone.tick(v.noise.bipolar());
                    x += v.clickTone.band * v.click;
                    v.click *= v.clickDecay;
                }
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    x *= v.fade;
                }
                l[i] += x * v.panL;
                r[i] += x * v.panR;
            }
            if (m == clock) {
                const bool silent = !v.a.on && !v.b.on && !v.top.on;
                if (!v.stolen && v.delay == 0 && (silent || v.level < 2.0e-5f)) v.steal(0.05, sr);
                if (v.stolen && v.fade <= 0.0f) v.note = -1;
            }
        }
        // The clockwork ticks while the box is playing (Mechanism), irregularly.
        if (st.mechanism > 0.0f && busy > 0.0f) {
            for (int i = 0; i < m; ++i) {
                tickCountdown -= 1.0f;
                if (tickCountdown <= 0.0f) {
                    tickEnv = st.mechanism * 0.004f * (0.5f + tickRng.uniform()) * busy;
                    tickCountdown = float(sr) * (0.09f + 0.25f * tickRng.uniform());
                }
                if (tickEnv > 1.0e-7f) {
                    tickTone.tick(tickRng.bipolar());
                    const float t = tickTone.band * tickEnv;
                    l[i] += 0.8f * t;
                    r[i] += 0.6f * t;
                    tickEnv *= tickDecay;
                }
            }
        }
        clock -= m;
        done += m;
    }
}

//==============================================================================
// The recorded box (Milestone 8): the kalimba, the piano's top.
namespace {
constexpr float tineCentres[3] { 0.22f, 0.58f, 0.9f };
constexpr float tineDb = -20.0f;
} // namespace

bool LostMusicBox::startRecorded(int note, float velocity) {
    const auto* kalimba = recordings("kalimba");
    if (kalimba == nullptr) return false;
    const auto* piano = recordings("piano-long");
    const auto& st = settings;
    auto& v = rpool.allocate(note, voices(polyphony), sr, 0.02, [this](const RVoice& x) { return level(x); });
    rpool.begin(v, note);
    v.shape = takeShape();
    const float wear = st.wear;
    v.cents = (keyHash(note, 61) * 2.0f - 1.0f) * wear * 28.0f;          // each tooth's wear, as in the model
    const bool dull = keyHash(note, 62) < 0.25f * wear;
    const float uneven = 1.0f - wear * 0.55f * keyHash(note, 63);
    // The kalimba plays where it was recorded (C4-E6) and a fifth beyond; the piano's top takes
    // over above, and sits under the high notes (Brightness).
    const float kalShare = std::clamp((96.0f - float(note)) / 6.0f, 0.0f, 1.0f);
    const float topShare = piano == nullptr ? 0.0f
                         : std::clamp(std::max(1.0f - kalShare, (float(note) - 70.0f) / 26.0f * (0.25f + 0.5f * st.brightness)), 0.0f, 1.0f);
    const float wk = std::sqrt(std::max(0.0f, 1.0f - topShare * topShare)) * (piano == nullptr ? 1.0f : 1.0f);
    // The recorded dynamics carry most of the change: a gentler curve on top.
    const float gain = (0.3f + 0.7f * velocity) * uneven * dbGain(recordedTrimDb);
    const auto mix = layerMix(velocity, tineCentres, kalimba->layers);
    const float pitch = float(note);
    v.a.on = v.b.on = v.top.on = false;
    if (wk > 0.0f) {
        const auto* za = kalimba->find(note, (tineCentres[mix.a] + 0.0f), 0.0f);
        const auto* zb = kalimba->find(note, tineCentres[mix.b], 0.0f);
        // The layer's own dynamics are kept (prepare.py set them apart); within a layer, notes evened.
        const auto lv = [&](const samples::Zone* z) { return dbGain(tineDb + (kalimba->medianDb(z->layer) - kalimba->medianDb(1)) - z->loudnessDb); };
        // Two strikes of one tine are partly in phase: the crossfade keeps the level an
        // uncorrelated mix would have (their measured correlation, liminal.py).
        float norm = 1.0f;
        if (za != nullptr && zb != nullptr && za != zb && mix.wa > 0.0f && mix.wb > 0.0f) {
            const float pa = mix.wa * lv(za) * dbGain(za->rms300Db), pb = mix.wb * lv(zb) * dbGain(zb->rms300Db);
            const float want = pa * pa + pb * pb, got = want + 2.0f * za->corrUp * pa * pb;
            norm = std::sqrt(want / std::max(0.05f * want, got));
        }
        if (za != nullptr && v.a.start(za, kalimba->rate, sr, pitch, 1.0)) v.a.gain = gain * wk * mix.wa * lv(za) * norm;
        if (zb != nullptr && zb != za && mix.wb > 0.0f && v.b.start(zb, kalimba->rate, sr, pitch, 1.0)) v.b.gain = gain * wk * mix.wb * lv(zb) * norm;
    }
    if (topShare > 0.0f) {
        const int key = std::clamp(note, 21, 108);
        if (const auto* z = piano->onKey(key, 0); z != nullptr && v.top.start(z, piano->rate, sr, pitch, double(std::max(1, z->onset - 32))))
            v.top.gain = gain * topShare * dbGain(tineDb - 6.0f - z->loudnessDb);
    }
    if (!v.a.on && !v.b.on && !v.top.on) { v.note = -1; return false; }
    // Decay: the recordings ring as they rang; Decay below its top shortens them (dull teeth more).
    const float f1 = noteHz(float(note));
    const float ring = 0.5f * std::pow(12.0f, st.decay) * std::clamp(std::pow(880.0f / f1, 0.45f), 0.35f, 2.5f) * 1.8f * (dull ? 0.35f : 1.0f)
                       * (1.0f + wear * (keyHash(note, 64) - 0.5f) * 1.2f);
    v.decayMul = st.decay > 0.97f && !dull ? 1.0f : t60(std::max(0.08f, ring), sr);
    v.env = 1.0f;
    const float bright = std::clamp(st.brightness * 0.8f + velocity * 0.3f + 0.3f * v.shape.tone, 0.0f, 1.0f);
    v.tone.clear();
    v.tone.set(std::clamp(2.0f * f1, 900.0f, 5000.0f), 0.45f + 1.1f * bright, sr);
    const float pan = std::clamp(float(note - 79) / 30.0f, -1.0f, 1.0f) * 0.4f;
    const auto g = panGains(pan);
    v.panL = g.l;
    v.panR = g.r;
    // The worn mechanism: each note a little late (up to ~14 ms), the same tooth roughly the same.
    v.delay = int(st.mechanism * (0.004f + 0.010f * (0.6f * keyHash(note, 66) + 0.4f * rng.uniform())) * float(sr));
    // The pin's click, softer than the model's (the recordings have their own touch).
    v.click = st.mechanism * 0.5f * gain * 0.3f * (0.5f + bright);
    v.clickDecay = t60(0.012, sr);
    v.clickTone.clear();
    v.clickTone.set(std::min(0.4f * float(sr), 2500.0f + 4000.0f * bright + 0.5f * f1), 2.5f, sr);
    v.noise.state = rng.state ^ 0x1F83D9ABu;
    v.noise.uniform();
    v.level = gain;
    busy = 1.0f;
    return true;
}

} // namespace liminal::dsp
