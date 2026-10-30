#include "DSP/ForgottenPiano.h"

namespace liminal::dsp {

namespace {
// Stiffness (inharmonicity) coefficient B by key, log-interpolated between register
// points (docs/INSTRUMENT_NOTES.md: ~1e-4 in the wound bass, ~4e-4 at middle C, rising
// to ~1e-2 in the top octave).
float inharmonicity(int note) {
    static constexpr std::array<std::pair<float, float>, 5> points { { { 21.0f, 2.5e-4f }, { 40.0f, 1.5e-4f }, { 60.0f, 4.0e-4f }, { 84.0f, 2.5e-3f }, { 108.0f, 1.5e-2f } } };
    const float n = std::clamp(float(note), 21.0f, 108.0f);
    for (std::size_t i = 1; i < points.size(); ++i)
        if (n <= points[i].first) {
            const float t = (n - points[i - 1].first) / (points[i].first - points[i - 1].first);
            return std::exp(std::log(points[i - 1].second) + t * (std::log(points[i].second) - std::log(points[i - 1].second)));
        }
    return points.back().second;
}
constexpr std::array<int, 6> openStrings { 33, 36, 40, 43, 45, 48 };   // A1 C2 E2 G2 A2 C3
} // namespace

void ForgottenPiano::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    for (std::size_t i = 0; i < sympathetic.size(); ++i) {
        auto& comb = sympathetic[i];
        const int period = std::max(2, int(std::lround(sr / double(noteHz(float(openStrings[i]))))));
        comb.line.assign(std::size_t(period), 0.0f);
        comb.feedback = std::pow(t60(4.0, sr), float(period));
    }
    pedalCoef = float(1.0 - std::exp(-1.0 / (0.08 * sr)));
    reset();
}

void ForgottenPiano::reset() {
    for (auto& v : pool.voices) v = Voice {};
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    bend = 1.0f;
    clock = 0;
    for (auto& e : events) e = Event {};
    nextEvent = 0;
    for (auto& comb : sympathetic) {
        std::fill(comb.line.begin(), comb.line.end(), 0.0f);
        comb.pos = 0;
        comb.lp = 0;
    }
    pedalMix = pedalTarget = 0;
}

// A knock rings the resonator at `knock` and adds `rustle` of noise through it.
void ForgottenPiano::addEvent(float hz, float q, float seconds, float knock, float rustle, float brightness, float pan) {
    if (knock + rustle <= 1.0e-6f) return;
    auto& e = events[std::size_t(nextEvent)];
    nextEvent = (nextEvent + 1) % int(events.size());
    const float w = twoPi * std::min(hz, 0.45f * float(sr)) / float(sr);
    const float r = std::exp(-0.5f * w / std::max(0.3f, q));
    e.b1 = 2.0f * r * std::cos(w);
    e.b2 = -r * r;
    // Input scaled so the resonator's peak gain is about one.
    e.amp = rustle * (1.0f - r) * std::sqrt(std::max(1.0e-6f, 1.0f - 2.0f * r * std::cos(2.0f * w) + r * r));
    e.decay = t60(seconds, sr);
    e.y1 = knock * std::sin(w);
    e.y2 = e.lp = 0;
    e.noise.state = 0x9E3779B9u ^ (rng.state * 2654435761u) | 1u;   // its own noise: independent of block size
    e.lpCoef = 1.0f - std::exp(-twoPi * std::min(0.45f * float(sr), brightness) / float(sr));
    const auto g = panGains(pan);
    e.panL = g.l;
    e.panR = g.r;
}

void ForgottenPiano::start(Voice& v, int note, float velocity) {
    const auto& st = settings;
    const float age = st.age, det = st.detune, soft = st.softness;
    const float h1 = keyHash(note, 1), h2 = keyHash(note, 2), h3 = keyHash(note, 3), h4 = keyHash(note, 4), h5 = keyHash(note, 5),
                h6 = keyHash(note, 6);

    // Each key is a little out of tune with its neighbours.
    const float f0 = noteHz(float(note)) * cents((2.0f * h1 - 1.0f) * (det * 9.0f + age * 3.0f));
    const float stiffness = inharmonicity(note) * (1.0f + 0.5f * age);

    // Hammer contact time: ~4 ms in the bass to under 1 ms at the top, +/-20 % with
    // dynamics, longer with soft felt. The force pulse low-passes the string's spectrum.
    const float tau = 0.004f * std::exp2(-float(note - 21) / 37.0f) * (1.4f - 0.65f * velocity) * (1.0f + 1.2f * soft);
    const float voicing = 1.0f + (2.0f * h2 - 1.0f) * (0.15f + 0.35f * age);   // uneven hammers
    const float corner = 1.8f / tau * voicing;
    // Felt is a stiffening spring: harder blows add disproportionately more upper partials.
    const float slope = 2.5f - 1.8f * velocity + 0.5f * soft;
    const float strike = 0.12f + 0.02f * (2.0f * h3 - 1.0f);    // strike point, fraction of the string

    // Sustain: long in the bass, short at the top; a duller soundboard with age.
    const float sustainScale = std::exp2((st.decay - 0.5f) * 3.0f) * (1.0f - 0.35f * age) * (1.0f + (2.0f * h4 - 1.0f) * (0.1f + 0.3f * age));
    const float fundamentalT60 = std::clamp(24.0f * std::pow(110.0f / f0, 0.7f), 1.2f, 45.0f) * sustainScale;
    // The treble's two stages are closer together (fewer, stiffer strings, less coupling).
    const float treble = std::clamp(float(note - 60) / 36.0f, 0.0f, 1.0f);
    const float afterShare = std::min(0.7f, 0.3f + 0.2f * soft + 0.1f * age + 0.2f * treble);
    const float promptRatio = 5.0f - 2.5f * treble;
    // Unison mistuning (cents): a well-tuned piano has under a cent; age and Detune widen it.
    // Some keys of an old piano are badly off (false beats).
    float unison = (0.6f + 12.0f * det * det + 3.0f * age) * (0.5f + h5);
    if (h6 < age * 0.2f) unison *= 4.0f;
    if (note < 33) unison *= 0.35f;   // single wound strings: only the two polarisations
    v.hasDamper = note < 89;
    const float damperT60 = (0.12f + 0.35f * std::clamp(float(89 - note) / 68.0f, 0.0f, 1.0f)) * (1.0f + 2.5f * age);

    const float limit = std::min(0.45f * float(sr), 16000.0f);
    std::array<float, maxPartials> amps {}, freqs {};
    int partials = 0;
    float energy = 0;
    for (int i = 0; i < maxPartials; ++i) {
        const float n = float(i + 1);
        const float f = n * f0 * std::sqrt(1.0f + stiffness * n * n);
        if (f > limit) break;
        const float a = std::abs(std::sin(n * pi * strike)) / std::sqrt(1.0f + std::pow(f / corner, 2.0f * slope)) * std::pow(n, -0.35f);
        amps[std::size_t(i)] = a;
        freqs[std::size_t(i)] = f;
        energy += a * a;
        ++partials;
    }
    const float gain = 0.55f * (0.02f + 0.98f * velocity * velocity) / std::sqrt(std::max(1.0e-9f, energy));
    const float pan = std::clamp(float(note - 64) / 64.0f, -1.0f, 1.0f) * 0.35f;
    const auto centre = panGains(pan * 0.6f), wideA = panGains(pan + 0.2f), wideB = panGains(pan - 0.2f);

    v.modes.clear();
    const int beating = std::min(partials, beatingPartials);
    for (int i = 0; i < partials; ++i) {
        const float f = freqs[std::size_t(i)];
        const float a = gain * amps[std::size_t(i)];
        const float t = 1.0f / (1.0f / fundamentalT60 + 0.18f * std::pow(f / 1000.0f, 1.5f));
        // Upper partials already decay fast; their two stages are closer together.
        const float prompt = t / (1.0f + (promptRatio - 1.0f) / std::sqrt(float(i + 1)));
        const auto damped = [&](float seconds) { return v.hasDamper ? t60(1.0 / (1.0 / seconds + 1.0 / damperT60), sr) : t60(seconds, sr); };
        const float w = twoPi * f / float(sr);
        const float phase = twoPi * keyHash(note * 64 + i, 7);
        const float spread = unison * (0.7f + 0.6f * keyHash(note * 64 + i, 11));
        v.modes.set(i, a * (1.0f - afterShare), phase, w, t60(prompt, sr), damped(prompt), centre.l, centre.r);
        if (i < beating) {
            v.modes.set(partials + i, a * afterShare * 0.5f, phase, w * cents(0.5f * spread), t60(t, sr), damped(t), wideA.l, wideA.r);
            v.modes.set(2 * partials + i, a * afterShare * 0.5f, phase, w * cents(-0.5f * spread), t60(t, sr), damped(t), wideB.l, wideB.r);
        } else {
            v.modes.set(partials + i, a * afterShare, phase, w * cents((i % 2 == 0 ? 0.25f : -0.25f) * spread), t60(t, sr), damped(t), wideA.l, wideA.r);
        }
    }
    v.damped = false;
    v.wobble.start(rng);
    v.wobbleCents = 0;
    v.appliedRatio = bend;
    v.modes.retune(bend, false);
    v.attack = 0;
    v.attackStep = 1.0f / std::max(1.0f, tau * float(sr));
    v.level = gain;

    // Key-bed thump and the hammer's knock.
    const float noise = st.noise;
    addEvent(95.0f + 40.0f * h3, 2.5f, 0.05f, 0.035f * noise * std::pow(velocity, 1.5f), 0.004f * noise * velocity, 2000.0f, pan);
    addEvent(1400.0f + 900.0f * velocity, 1.2f, 0.012f, 0.006f * noise * velocity, 0.06f * noise * velocity * (1.0f + 0.5f * age) * (1.3f - 0.6f * soft), 9000.0f, pan);
}

void ForgottenPiano::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    auto& v = pool.allocate(note, polyphony, sr, 0.03, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    start(v, note, velocity);
}

void ForgottenPiano::noteOff(int note) { pool.noteOff(note); }

void ForgottenPiano::sustain(bool down) {
    if (down != pool.pedal) {
        const float noise = settings.noise;
        if (down) {
            addEvent(75.0f, 2.0f, 0.06f, 0.02f * noise, 0.004f * noise, 1500.0f, -0.1f);
            addEvent(900.0f, 4.0f, 0.03f, 0.002f * noise * (1.0f + 2.0f * settings.age), 0.01f * noise * (1.0f + 2.0f * settings.age), 6000.0f, 0.1f);
        } else {
            addEvent(110.0f, 2.0f, 0.05f, 0.015f * noise, 0.003f * noise, 2500.0f, 0.0f);
        }
    }
    pool.sustain(down);
    pedalTarget = down ? 1.0f : 0.0f;
}

void ForgottenPiano::allNotesOff() {
    pool.allNotesOff();
    pedalTarget = 0.0f;
}

void ForgottenPiano::allSoundOff() {
    pool.allSoundOff(sr);
    pedalTarget = pedalMix = 0.0f;
    for (auto& e : events) e = Event {};
    for (auto& comb : sympathetic) {
        std::fill(comb.line.begin(), comb.line.end(), 0.0f);
        comb.lp = 0;
    }
}

void ForgottenPiano::pitchBend(int value14bit) {
    bend = bendRatio(value14bit);
    for (auto& v : pool.voices)
        if (v.active()) {
            v.appliedRatio = bend * cents(v.wobbleCents);
            v.modes.retune(v.appliedRatio, v.damped);
        }
}

void ForgottenPiano::control(Voice& v) {
    const bool damp = v.hasDamper && v.releasing();
    v.wobbleCents = v.wobble.tick(rng, 0.35f, float(controlPeriod / sr)) * (0.3f + 2.5f * settings.age);
    const float ratio = bend * cents(v.wobbleCents);
    if (damp != v.damped || std::abs(ratio - v.appliedRatio) > 2.0e-6f) {
        if (damp && !v.damped && !v.stolen)
            addEvent(380.0f + 200.0f * keyHash(v.note, 8), 4.0f, 0.03f, 0.03f * settings.noise * (1.0f + settings.age),
                     0.004f * settings.noise * (1.0f + settings.age), 3000.0f, 0.0f);
        v.damped = damp;
        v.appliedRatio = ratio;
        v.modes.retune(ratio, damp);
    }
    v.level = v.modes.peak();
}

void ForgottenPiano::render(float* left, float* right, int n) {
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
        float mono[controlPeriod] {};
        for (auto& v : pool.voices) {
            if (!v.active()) continue;
            for (int i = 0; i < m; ++i) {
                const Stereo o = v.modes.tick();
                float g = 1.0f;
                if (v.attack < 1.0f) {
                    v.attack = std::min(1.0f, v.attack + v.attackStep);
                    g = 0.5f - 0.5f * std::cos(pi * v.attack);
                }
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                l[i] += o.l * g;
                r[i] += o.r * g;
                mono[i] += (o.l + o.r) * g;
            }
            if ((v.stolen && v.fade <= 0.0f) || (v.level < 2.0e-5f && v.attack >= 1.0f)) v.note = -1;
        }
        for (auto& e : events) {
            if (e.amp < 1.0e-7f && std::abs(e.y1) + std::abs(e.y2) < 1.0e-7f) continue;
            for (int i = 0; i < m; ++i) {
                e.lp += e.lpCoef * (e.noise.bipolar() - e.lp);
                const float y = e.b1 * e.y1 + e.b2 * e.y2 + e.lp * e.amp;
                e.amp *= e.decay;
                e.y2 = e.y1;
                e.y1 = y;
                l[i] += y * e.panL;
                r[i] += y * e.panR;
            }
        }
        // The open strings: driven while the pedal is down, quickly damped when it lifts.
        for (int i = 0; i < m; ++i) {
            pedalMix += pedalCoef * (pedalTarget - pedalMix);
            const float in = mono[i] * 0.02f * pedalMix;
            float sl = 0, sr2 = 0;
            for (std::size_t c = 0; c < sympathetic.size(); ++c) {
                auto& comb = sympathetic[c];
                const float y = comb.line[std::size_t(comb.pos)];
                comb.lp += 0.35f * (y - comb.lp);
                const float fb = 0.5f + (comb.feedback - 0.5f) * pedalMix;
                comb.line[std::size_t(comb.pos)] = in + comb.lp * fb;
                if (++comb.pos >= int(comb.line.size())) comb.pos = 0;
                (c % 2 == 0 ? sl : sr2) += y;
            }
            l[i] += 0.5f * sl;
            r[i] += 0.5f * sr2;
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
