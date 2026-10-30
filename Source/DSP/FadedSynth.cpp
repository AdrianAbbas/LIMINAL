#include "DSP/FadedSynth.h"

namespace liminal::dsp {

namespace {
// PolyBLEP: removes the aliasing of a hard edge (t = phase, dt = phase increment).
float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
float saw(float p, float dt) { return 2.0f * p - 1.0f - blep(p, dt); }
float pulse(float p, float width, float dt) {
    float q = p + 1.0f - width;
    q -= std::floor(q);
    return (p < width ? 1.0f : -1.0f) + blep(p, dt) - blep(q, dt) - (2.0f * width - 1.0f);
}
constexpr float resonance = 0.55f;   // of 4 (self-oscillation): a soft analog peak
} // namespace

void FadedSynth::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    lineL.assign(std::size_t(std::ceil(0.012 * sr)) + 4, 0.0f);
    lineR.assign(lineL.size(), 0.0f);
    reset();
}

void FadedSynth::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    rng.state = 0x51ED270Bu;
    bend = 1.0f;
    lastHz = 0;
    clock = 0;
    wow = {};
    wowCents = 0;
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    write = 0;
    chorusPhase = 0;
    wetL = wetR = 0;
}

void FadedSynth::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.012, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    const std::uint64_t id = pool.counter;
    v.targetHz = noteHz(float(note));
    v.hz = settings.glide && lastHz > 0.0f ? lastHz : v.targetHz;   // Glide is opt-in
    lastHz = v.targetHz;
    // Free-running oscillators: every note starts at its own phases.
    for (auto& p : v.phase) p = rng.uniform();
    for (auto& d : v.oscDrift) d.start(rng);
    v.cutoffDrift.start(rng);
    v.lfoPhase = rng.uniform();
    v.lfoRate = 0.22f + 0.2f * rng.uniform();
    std::fill(std::begin(v.ladder), std::end(v.ladder), 0.0f);
    v.amp = v.filterEnv = 0;
    v.attacking = v.filterAttacking = true;
    v.velocity = velocity;
    v.velocityGain = 0.35f * (0.12f + 0.88f * std::pow(velocity, 1.5f));
    const float side = (id % 2 == 0 ? 1.0f : -1.0f) * (0.1f + 0.5f * settings.width) * (0.5f + 0.5f * rng.uniform());
    const auto g = panGains(side);
    v.panL = g.l;
    v.panR = g.r;
    v.g = 0;
    control(v);
    v.g += v.gStep * float(controlPeriod);   // start the filter where it should be
    v.gStep = 0;
}

void FadedSynth::allSoundOff() {
    pool.allSoundOff(sr);
}

void FadedSynth::control(Voice& v) {
    const auto& st = settings;
    const float tick = float(controlPeriod / sr);
    if (st.glide && v.hz != v.targetHz) {
        const float k = 1.0f - std::exp(-tick * 3.0f / std::max(0.005f, st.glideTime));
        v.hz *= std::pow(v.targetHz / v.hz, k);
        if (std::abs(v.hz / v.targetHz - 1.0f) < 1.0e-5f) v.hz = v.targetHz;
    } else {
        v.hz = v.targetHz;
    }
    const float spread = 24.0f * std::pow(st.detune, 1.5f);
    static constexpr std::array<float, 3> offsets { -0.5f, 0.5f, 0.15f };
    for (std::size_t k = 0; k < 3; ++k) {
        const float c = offsets[k] * spread + v.oscDrift[k].tick(rng, 0.25f, tick) * st.drift * 7.0f + wowCents;
        v.inc[k] = std::min(0.45f, v.hz * bend * cents(c) / float(sr));
    }
    v.lfoPhase += v.lfoRate * tick;
    v.lfoPhase -= std::floor(v.lfoPhase);
    const float lfo = std::sin(twoPi * v.lfoPhase);
    v.pulseWidth = 0.5f + 0.18f * lfo;

    const float base = 9000.0f * std::pow(500.0f / 9000.0f, st.warmth);
    const float track = std::pow(v.hz / 261.6f, 0.6f);
    const float envOctaves = 0.8f + 1.6f * v.velocity;
    const float wander = 1.0f + st.drift * 0.15f * v.cutoffDrift.tick(rng, 0.2f, tick) + 0.04f * lfo;
    const float hz = std::clamp(base * track * std::exp2(envOctaves * v.filterEnv) * wander, 30.0f, 0.2f * float(sr));
    const float x = twoPi * hz / float(sr);
    v.gStep = (x / (1.0f + x) - v.g) / float(controlPeriod);
}

void FadedSynth::render(float* left, float* right, int n) {
    const auto& st = settings;
    int done = 0;
    float attackCoef = 0, decayCoef = 0, releaseCoef = 0, filterDecay = 0, drive = 1, bias = 0, biasOut = 0;
    const auto coefficients = [&] {
        const double s = sr;
        attackCoef = float(1.0 - std::pow(0.3 / 1.3, 1.0 / (std::max(0.001, double(st.attack)) * s)));   // RC toward 1.3: reaches 1 in `attack`
        decayCoef = 1.0f - t60(1.6, s);
        releaseCoef = 1.0f - t60(std::max(0.01f, st.release), s);
        filterDecay = 1.0f - t60(1.2, s);
        drive = 1.0f + 3.0f * st.warmth;
        bias = 0.2f * st.warmth;   // asymmetry: even harmonics
        biasOut = softClip(bias);
    };
    coefficients();
    const float sustainLevel = 0.8f, filterSustain = 0.15f;
    const int size = int(lineL.size());
    while (done < n) {
        if (clock == 0) {
            wowCents = wow.tick(rng, 0.6f, float(controlPeriod / sr)) * st.drift * st.drift * 6.0f;
            for (auto& v : pool.voices)
                if (v.active()) control(v);
            chorusMix = 0.12f + 0.38f * st.width;
            chorusSpread = st.width;
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        float dryL[controlPeriod] {}, dryR[controlPeriod] {};
        for (auto& v : pool.voices) {
            if (!v.active()) continue;
            const bool releasing = v.releasing();
            for (int i = 0; i < m; ++i) {
                const float osc = 0.5f * saw(v.phase[0], v.inc[0]) + 0.5f * saw(v.phase[1], v.inc[1]) + 0.35f * pulse(v.phase[2], v.pulseWidth, v.inc[2]);
                for (std::size_t k = 0; k < 3; ++k) {
                    v.phase[k] += v.inc[k];
                    if (v.phase[k] >= 1.0f) v.phase[k] -= 1.0f;
                }
                if (releasing) {
                    v.amp -= releaseCoef * v.amp;
                    v.filterEnv -= releaseCoef * v.filterEnv;
                } else {
                    if (v.attacking) {
                        v.amp += attackCoef * (1.3f - v.amp);
                        if (v.amp >= 1.0f) { v.amp = 1.0f; v.attacking = false; }
                    } else {
                        v.amp += decayCoef * (sustainLevel - v.amp);
                    }
                    if (v.filterAttacking) {
                        v.filterEnv += attackCoef * (1.3f - v.filterEnv);
                        if (v.filterEnv >= 1.0f) { v.filterEnv = 1.0f; v.filterAttacking = false; }
                    } else {
                        v.filterEnv += filterDecay * (filterSustain - v.filterEnv);
                    }
                }
                // Warm input stage, then the four-pole ladder with gentle resonance.
                const float sat = softClip(drive * 0.55f * osc + bias) - biasOut;
                v.g += v.gStep;
                const float u = sat - resonance * v.ladder[3];
                v.ladder[0] += v.g * (u - v.ladder[0]);
                v.ladder[1] += v.g * (v.ladder[0] - v.ladder[1]);
                v.ladder[2] += v.g * (v.ladder[1] - v.ladder[2]);
                v.ladder[3] += v.g * (v.ladder[2] - v.ladder[3]);
                float out = v.ladder[3] * (1.0f + 0.5f * resonance) * v.amp * v.velocityGain;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    out *= v.fade;
                }
                dryL[i] += out * v.panL;
                dryR[i] += out * v.panR;
            }
            // Voices end only at the end of a control period, so the block size never matters.
            if (m == clock && ((v.stolen && v.fade <= 0.0f) || (releasing && v.amp < 1.0e-4f))) v.note = -1;
        }
        // Chorus: delays sweep 3.5 +/- 1.8 ms at 0.55 Hz; the right side's sweep is
        // offset by up to half a cycle with Width.
        for (int i = 0; i < m; ++i) {
            chorusPhase += float(0.55 / sr);
            if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
            const float sweepL = std::sin(twoPi * chorusPhase), sweepR = std::sin(twoPi * chorusPhase + pi * chorusSpread);
            lineL[std::size_t(write)] = dryL[i];
            lineR[std::size_t(write)] = dryR[i];
            const auto read = [&](const std::vector<float>& line, float sweep) {
                const float delay = float((0.0035 + 0.0018 * double(sweep)) * sr);
                float pos = float(write) - delay;
                if (pos < 0) pos += float(size);
                const int a = int(pos);
                const float f = pos - float(a);
                return line[std::size_t(a)] + f * (line[std::size_t((a + 1) % size)] - line[std::size_t(a)]);
            };
            wetL += 0.45f * (read(lineL, sweepL) - wetL);   // bucket-brigade darkness
            wetR += 0.45f * (read(lineR, sweepR) - wetR);
            if (++write >= size) write = 0;
            left[done + i] += dryL[i] * (1.0f - 0.5f * chorusMix) + wetR * chorusMix;
            right[done + i] += dryR[i] * (1.0f - 0.5f * chorusMix) + wetL * chorusMix;
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
