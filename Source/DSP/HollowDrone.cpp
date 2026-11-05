#include "DSP/HollowDrone.h"

namespace liminal::dsp {

void HollowDrone::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    voiceLayer.setProfile(remembered::droneVoice());
    voiceLayer.prepare(sr);
    reset();
}

void HollowDrone::reset() {
    voiceLayer.reset();
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
    if (startRecorded(note, std::clamp(velocity, 0.0f, 1.0f))) return;
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
    v.shape = takeShape();
    control(v);
}

void HollowDrone::control(Voice& v) {
    const auto& st = settings;
    const float tick = float(controlPeriod / sr);
    const float base = noteHz(float(v.note)) * bend * cents(shapeCents(v.shape, tick));
    const float unstable = wander();   // Liminality: no movement of its own at Drift 0
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
        c += v.drift[i].tick(rng, 0.08f + 0.2f * st.instability, tick) * (1.0f + 22.0f * st.instability) * unstable;
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
    const float octaves = st.movement * 1.2f * std::sin(twoPi * v.sweep) + v.cutoffDrift.tick(rng, 0.1f, tick) * st.instability * 0.7f + 0.6f * v.shape.tone;
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
            for (auto& v : rpool.voices)
                if (v.active()) controlRecorded(v, float(controlPeriod / sr));
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        float* l = left + done;
        float* r = right + done;
        float vl[controlPeriod] {}, vr[controlPeriod] {};   // the voice layer, treated on its own
        for (auto& v : rpool.voices) {
            if (!v.active()) continue;
            const bool releasing = v.releasing();
            for (int i = 0; i < m; ++i) {
                v.amp += releasing ? releaseCoef * (0.0f - v.amp) : attackCoef * (1.02f - v.amp);
                if (!releasing) v.amp = std::min(v.amp, 1.0f);
                float sl = 0, sr_ = 0, voL = 0, voR = 0;
                for (int k = 0; k < layers; ++k) {
                    auto& rd = v.r[std::size_t(k)];
                    if (!rd.on) continue;
                    const float x = rd.tick();
                    (k == 0 ? voL : sl) += x * v.gl[std::size_t(k)];
                    (k == 0 ? voR : sr_) += x * v.gr[std::size_t(k)];
                }
                float g = v.amp * v.gain;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                // The violin, pad and bass through the drone's low-pass as before; the voice layer goes to
                // its own remembered treatment below (its own, darker low-pass among it).
                l[i] += v.lpL.lp(sl) * g;
                r[i] += v.lpR.lp(sr_) * g;
                vl[i] += voL * g;
                vr[i] += voR * g;
            }
            if (m == clock) {
                bool any = false;
                for (const auto& rd : v.r) any = any || rd.on;
                if (!v.stolen && ((releasing && v.amp < 1.0e-4f) || !any)) v.steal(0.05, sr);
                if (v.stolen && v.fade <= 0.0f) v.note = -1;
            }
        }
        voiceLayer.process(vl, vr, m, wander());
        for (int i = 0; i < m; ++i) {
            l[i] += vl[i];
            r[i] += vr[i];
        }
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

//==============================================================================
// The recorded drone (Milestone 8).
namespace {
constexpr float droneDb = -20.0f;
// The pitch to play a layer at: the note moved by octaves into [lo, hi] (a drone's octave doubling).
float into(int note, int lo, int hi) {
    int n = note;
    while (n < lo) n += 12;
    while (n > hi) n -= 12;
    return float(n);
}
} // namespace

bool HollowDrone::startRecorded(int note, float velocity) {
    const auto* pad = recordings("mlg-softpad-long");
    if (pad == nullptr) return false;
    const auto& st = settings;
    auto& v = rpool.allocate(note, voices(polyphony), sr, 0.15, [this](const RVoice& x) { return level(x); });
    rpool.begin(v, note);
    v.shape = takeShape();
    v.velocity = velocity;
    for (auto& rd : v.r) rd.on = false;
    const auto level = [](const samples::Zone* z) { return dbGain(droneDb - z->loudnessDb); };
    // 0: the voice's nearest drone within a fifth (the hum and the 'ooo' on G2, the 'aah' on A3).
    {
        const samples::Zone* best = nullptr;
        const samples::Instrument* from = nullptr;
        for (const char* id : { "voice-drone", "voice-hum" })
            if (const auto* set = recordings(id))
                for (const auto& z : set->zones)
                    if (std::abs(z.root - float(note)) <= 7.01f && (best == nullptr || std::abs(z.root - float(note)) < std::abs(best->root - float(note)))) { best = &z; from = set; }
        if (best != nullptr && v.r[0].start(best, from->rate, sr, float(note), 1.0)) v.base[0] = level(best) * (0.5f + 0.9f * st.density);
    }
    // 1: the violin's open strings, an octave or two up (into G3..E5).
    if (const auto* violin = quality == 0 ? nullptr : recordings("violin-sustain")) {   // Eco: no violin
        const float pitch = into(note + 12, 52, 79);
        if (const auto* z = violin->find(int(pitch), 0.5f, keyHash(note, 81)); z != nullptr && v.r[1].start(z, violin->rate, sr, pitch, 1.0))
            v.base[1] = level(z) * (0.15f + 0.55f * st.harmonics) * (0.6f + 0.8f * st.density);
    }
    // 2: the soft pad on the note (its 37 keys; beyond them by octaves).
    {
        const float pitch = into(note, int(pad->lowRoot) - 2, int(pad->highRoot) + 2);
        if (const auto* z = pad->find(int(pitch), 0.5f, 0.0f); z != nullptr && v.r[2].start(z, pad->rate, sr, pitch, 1.0)) v.base[2] = level(z);
    }
    // 3: the static-y bass an octave below (Sub).
    if (const auto* bass = recordings("mlg-staticbass-long"); bass != nullptr && st.sub > 0.01f) {
        const float pitch = into(note - 12, int(bass->lowRoot), int(bass->highRoot));
        if (const auto* z = bass->find(int(pitch), 0.5f, 0.0f); z != nullptr && v.r[3].start(z, bass->rate, sr, pitch, 1.0))
            v.base[3] = level(z) * st.sub * 0.8f;
    }
    float power = 0;
    for (int k = 0; k < layers; ++k) {
        const auto i = std::size_t(k);
        if (!v.r[i].on) v.base[i] = 0;
        // base = level(z) x the layer's weight; every layer levelled to droneDb, so the weights alone set the mix.
        const float weight = v.r[i].on ? v.base[i] / level(v.r[i].zone) : 0.0f;
        power += weight * weight;
        v.offset[i] = (2.0f * keyHash(note * 4 + k, 82) - 1.0f) * (2.0f + 10.0f * st.instability);
        v.panPhase[i] = keyHash(note * 4 + k, 83);
        v.drift[i].start(rng);
    }
    // The blend at the loudness of one layer: the stand-in's level for the same note.
    v.gain = 0.11f * (0.3f + 0.7f * std::pow(velocity, 1.3f)) * dbGain(recordedTrimDb) / std::max(0.3f, std::sqrt(power));
    v.amp = 0;
    v.breath = keyHash(note, 84);
    v.lpL.z = v.lpR.z = 0;
    controlRecorded(v, 0.0f);
    return true;
}

void HollowDrone::controlRecorded(RVoice& v, float tick) {
    const auto& st = settings;
    const float unstable = wander();
    const float shaped = shapeCents(v.shape, tick);
    // Movement: the voice and the pad breathe against each other, slowly; everything circles.
    const float moveRate = 0.03f + 0.25f * st.movement;
    v.breath += tick * (0.02f + 0.08f * st.movement);
    v.breath -= std::floor(v.breath);
    const float b = st.movement * 0.6f * std::sin(twoPi * v.breath);
    const float tilt[layers] { 1.0f + b, 1.0f + 0.5f * b, 1.0f - b, 1.0f };
    for (int k = 0; k < layers; ++k) {
        const auto i = std::size_t(k);
        auto& rd = v.r[i];
        if (!rd.on) continue;
        const float c = v.offset[i] + v.drift[i].tick(rng, 0.08f + 0.2f * st.instability, std::max(tick, 1.0e-4f)) * (1.0f + 14.0f * st.instability) * unstable + shaped;
        rd.retune(bend * cents(c));
        rd.gain = v.base[i] * std::max(0.0f, tilt[k]);
        v.panPhase[i] += tick * moveRate * (0.6f + 0.15f * float(k));
        v.panPhase[i] -= std::floor(v.panPhase[i]);
        const auto p = panGains((0.25f + 0.75f * st.movement) * std::sin(twoPi * v.panPhase[i]) * (k == 3 ? 0.2f : 0.8f));
        v.gl[i] = p.l;
        v.gr[i] = p.r;
    }
    const float base = noteHz(float(v.note));
    const float cutoff = std::clamp(base * (3.0f + 24.0f * st.harmonics * st.harmonics) * std::exp2(0.6f * v.shape.tone) + 300.0f, 120.0f, 16000.0f);
    v.lpL.setHz(cutoff, sr);
    v.lpR.setHz(cutoff * 1.06f, sr);
}

} // namespace liminal::dsp
