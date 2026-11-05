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
    for (auto& v : rpool.voices) v = RVoice {};   // (the recorded patches' voices too: a reset never leaves one sounding)
    rpool.pedal = false;
    rpool.counter = 0;
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
    float synthGain = 1.0f;
    {
        const NoteShape keep = nextShape;
        const float from = lastHz;
        if (startRecorded(note, velocity, from)) {
            lastHz = noteHz(float(note));
            synthGain = settings.blend;   // the synthesised voice under it, or not at all
            if (synthGain <= 0.001f) { nextShape = {}; return; }
            nextShape = keep;
            lastHz = from;
        }
    }
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
    v.velocityGain = 0.35f * (0.12f + 0.88f * std::pow(velocity, 1.5f)) * synthGain;
    const float side = (id % 2 == 0 ? 1.0f : -1.0f) * (0.1f + 0.5f * settings.width) * (0.5f + 0.5f * rng.uniform());
    const auto g = panGains(side);
    v.panL = g.l;
    v.panR = g.r;
    v.g = 0;
    v.shape = takeShape();
    control(v);
    v.g += v.gStep * float(controlPeriod);   // start the filter where it should be
    v.gStep = 0;
}

void FadedSynth::allSoundOff() {
    pool.allSoundOff(sr);
    rpool.allSoundOff(sr);
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
    const float shaped = shapeCents(v.shape, tick), unstable = wander();
    static constexpr std::array<float, 3> offsets { -0.5f, 0.5f, 0.15f };
    for (std::size_t k = 0; k < 3; ++k) {
        const float c = offsets[k] * spread + (v.oscDrift[k].tick(rng, 0.25f, tick) * st.drift * 7.0f + wowCents) * unstable + shaped;
        v.inc[k] = std::min(0.45f, v.hz * bend * cents(c) / float(sr));
    }
    v.lfoPhase += v.lfoRate * tick;
    v.lfoPhase -= std::floor(v.lfoPhase);
    const float lfo = std::sin(twoPi * v.lfoPhase);
    v.pulseWidth = 0.5f + 0.18f * lfo;

    const float base = 9000.0f * std::pow(500.0f / 9000.0f, st.warmth) * std::exp2(0.8f * v.shape.tone);
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
            for (auto& v : rpool.voices)
                if (v.active()) controlRecorded(v, float(controlPeriod / sr));
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
        for (auto& v : rpool.voices) {
            if (!v.active()) continue;
            const bool releasing = v.releasing();
            if (releasing && !v.a.released) { v.a.release(); v.b.release(); }
            for (int i = 0; i < m; ++i) {
                if (releasing) {
                    v.amp -= releaseCoef * v.amp;
                } else if (v.attacking) {
                    v.amp += attackCoef * (1.3f - v.amp);
                    if (v.amp >= 1.0f) { v.amp = 1.0f; v.attacking = false; }
                }
                const float xa = v.a.tick(), xb = v.b.on ? v.b.tick() : 0.0f;
                float yl = xa * v.aL + xb * v.bL, yr = xa * v.aR + xb * v.bR;
                // Warmth: a little saturation, then the low-pass.
                yl = softClip(v.drive * yl) / v.drive;
                yr = softClip(v.drive * yr) / v.drive;
                float g = v.amp * v.gain;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                dryL[i] += v.lp1.lp(yl) * g;
                dryR[i] += v.lp2.lp(yr) * g;
            }
            if (m == clock) {
                if (!v.stolen && ((releasing && v.amp < 1.0e-4f) || (!v.a.on && !v.b.on))) v.steal(0.03, sr);
                if (v.stolen && v.fade <= 0.0f) v.note = -1;
            }
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

//==============================================================================
// The recorded patches (Milestone 8).
bool FadedSynth::startRecorded(int note, float velocity, float glideFromHz) {
    const auto& st = settings;
    if (st.patch <= 0 || st.patch >= int(std::size(patchSets))) return false;
    const auto* set = recordings(patchSets[st.patch]);
    if (set == nullptr) return false;
    const auto* z = set->find(note, 0.5f, 0.0f);
    if (z == nullptr) return false;
    auto& v = rpool.allocate(note, voices(polyphony), sr, 0.012, [this](const RVoice& x) { return level(x); });
    rpool.begin(v, note);
    v.shape = takeShape();
    v.targetHz = noteHz(float(note));
    v.hz = st.glide && glideFromHz > 0.0f ? glideFromHz : v.targetHz;
    v.velocity = velocity;
    // Every note of every patch at one loudness (each patch evened by its notes' median), then the
    // velocity curve of the synthesised voice.
    v.gain = 0.35f * (0.12f + 0.88f * std::pow(velocity, 1.5f)) * dbGain(-21.0f - z->loudnessDb + recordedTrimDb);
    v.a.start(z, set->rate, sr, float(note), 1.0);
    // Detune: a second copy of the note, apart by up to ~24 cents (as the oscillators), on the other side.
    const float spread = 24.0f * std::pow(st.detune, 1.5f);
    v.b.on = false;
    const auto side = [](float p) { return panGains(p); };
    const float w = 0.1f + 0.5f * st.width;
    if (spread > 0.5f && v.b.start(z, set->rate, sr, float(note), z->loops() ? double(z->loopStart) * 0.5 + 1.0 : 1.0)) {
        const float k = std::sqrt(0.5f);
        v.a.gain = v.b.gain = k;
        const auto ga = side(-w), gb = side(w);
        v.aL = ga.l; v.aR = ga.r; v.bL = gb.l; v.bR = gb.r;
    } else {
        const auto g = side((pool.counter + rpool.counter) % 2 == 0 ? 0.3f * w : -0.3f * w);
        v.aL = v.bL = g.l; v.aR = v.bR = g.r;
    }
    v.drift.start(rng);
    v.amp = 0;
    v.attacking = true;
    v.drive = 1.0f + 2.0f * st.warmth;
    v.lp1.z = v.lp2.z = 0;
    controlRecorded(v, 0.0f);
    return true;
}

void FadedSynth::controlRecorded(RVoice& v, float tick) {
    const auto& st = settings;
    if (st.glide && v.hz != v.targetHz && tick > 0.0f) {
        const float k = 1.0f - std::exp(-tick * 3.0f / std::max(0.005f, st.glideTime));
        v.hz *= std::pow(v.targetHz / v.hz, k);
        if (std::abs(v.hz / v.targetHz - 1.0f) < 1.0e-5f) v.hz = v.targetHz;
    } else if (!st.glide) {
        v.hz = v.targetHz;
    }
    const float spread = 24.0f * std::pow(st.detune, 1.5f);
    const float c = shapeCents(v.shape, tick) + (v.drift.tick(rng, 0.25f, std::max(tick, 1.0e-4f)) * st.drift * 7.0f + wowCents) * wander();
    const float glide = v.hz / v.targetHz;
    v.a.retune(bend * glide * cents(c - 0.5f * spread * (v.b.on ? 1.0f : 0.0f)));
    v.b.retune(bend * glide * cents(c + 0.5f * spread));
    // Warmth darkens, velocity brightens (as the synthesised filter does, without its sweep).
    const float hz = std::clamp(16000.0f * std::pow(900.0f / 16000.0f, st.warmth) * std::exp2(0.8f * v.shape.tone) * (0.6f + 0.6f * v.velocity)
                                * std::pow(v.targetHz / 261.6f, 0.3f), 200.0f, 0.45f * float(sr));
    v.lp1.setHz(hz, sr);
    v.lp2.setHz(hz, sr);
}

} // namespace liminal::dsp
