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
    for (auto& v : rpool.voices) v = RVoice {};
    rpool.pedal = false;
    rpool.counter = 0;
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
    v.shape = takeShape();
    const float h1 = keyHash(note, 1), h2 = keyHash(note, 2), h3 = keyHash(note, 3), h4 = keyHash(note, 4), h5 = keyHash(note, 5),
                h6 = keyHash(note, 6);

    // Each key is a little out of tune with its neighbours.
    const float f0 = noteHz(float(note)) * cents((2.0f * h1 - 1.0f) * (det * 9.0f + age * 3.0f));
    const float stiffness = inharmonicity(note) * (1.0f + 0.5f * age);

    // Hammer contact time: ~4 ms in the bass to under 1 ms at the top, +/-20 % with
    // dynamics, longer with soft felt. The force pulse low-passes the string's spectrum.
    const float tau = 0.004f * std::exp2(-float(note - 21) / 37.0f) * (1.4f - 0.65f * velocity) * (1.0f + 1.2f * soft);
    const float voicing = 1.0f + (2.0f * h2 - 1.0f) * (0.15f + 0.35f * age);   // uneven hammers
    const float corner = 1.8f / tau * voicing * std::exp2(0.9f * v.shape.tone);   // Liminality's Unevenness
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

    // Quality: Eco keeps 16 partials below 9 kHz, High lets them reach 19 kHz.
    const float limit = std::min(0.45f * float(sr), quality == 0 ? 9000.0f : quality == 2 ? 19000.0f : 16000.0f);
    const int partialLimit = quality == 0 ? 16 : maxPartials;
    std::array<float, maxPartials> amps {}, freqs {};
    int partials = 0;
    float energy = 0;
    for (int i = 0; i < partialLimit; ++i) {
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
    v.appliedRatio = bend * cents(shapeCents(v.shape, 0.0f));
    v.modes.retune(v.appliedRatio, false);
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
    if (const auto* hits = recordings("piano"))
        if (startRecorded(note, velocity, *hits)) return;
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.03, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    start(v, note, velocity);
}

void ForgottenPiano::noteOff(int note) { pool.noteOff(note); rpool.noteOff(note); }

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
    rpool.sustain(down);
    pedalTarget = down ? 1.0f : 0.0f;
}

void ForgottenPiano::allNotesOff() {
    pool.allNotesOff();
    rpool.allNotesOff();
    pedalTarget = 0.0f;
}

void ForgottenPiano::allSoundOff() {
    pool.allSoundOff(sr);
    rpool.allSoundOff(sr);
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
            v.appliedRatio = bend * cents(v.wobbleCents + v.shape.now);
            v.modes.retune(v.appliedRatio, v.damped);
        }
    for (auto& v : rpool.voices)
        if (v.active()) controlRecorded(v, 0.0f);
}

void ForgottenPiano::control(Voice& v) {
    const bool damp = v.hasDamper && v.releasing();
    const float tick = float(controlPeriod / sr);
    v.wobbleCents = v.wobble.tick(rng, 0.35f, tick) * (0.3f + 2.5f * settings.age) * wander();
    const float ratio = bend * cents(v.wobbleCents + shapeCents(v.shape, tick));
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
            for (auto& v : rpool.voices)
                if (v.active()) controlRecorded(v, float(controlPeriod / sr));
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
            // Voices end only at the end of a control period, so the block size never matters.
            if (m == clock && ((v.stolen && v.fade <= 0.0f) || (v.level < 2.0e-5f && v.attack >= 1.0f))) v.note = -1;
        }
        renderRecorded(l, r, mono, m, m == clock);
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

//==============================================================================
// The recorded piano (Milestone 8).
namespace {
constexpr float layerCentres[3] { 0.25f, 0.6f, 0.92f };   // where soft / medium / full sound as recorded
constexpr float layerTone[3] { 0.62f, 1.0f, 1.38f };      // the long (medium) note's tone for each dynamic
constexpr float handSeconds = 0.08f;                       // short -> long crossfade
const samples::Zone* heldZone(const samples::Instrument& set, int key, float& semis) {
    // The key's long note, or (a note the recording cut short) the nearest well-held neighbour.
    semis = 0;
    const auto good = [](const samples::Zone* z) { return z != nullptr && (z->tailAt >= 0 || z->length - z->onset > int(0.9f * 44100.0f)); };
    const auto* z = set.onKey(key, 0);
    if (good(z) || key >= 96) return z;
    for (int d : { 1, -1, 2, -2 })
        if (const auto* n = set.onKey(key + d, 0); good(n)) { semis = float(d); return n; }
    return z;
}
} // namespace

bool ForgottenPiano::startRecorded(int note, float velocity, const samples::Instrument& hits) {
    const auto& st = settings;
    const auto* longSet = recordings("piano-long");
    // Keys beyond the 88: the nearest recorded key, shifted (a real piano has none there).
    const int key = std::clamp(note, 21, 108);
    const samples::Zone* layer[3] {};
    for (int i = 0; i < 3; ++i) layer[i] = hits.onKey(key, i);
    if (layer[1] == nullptr) return false;
    for (int i = 0; i < 3; ++i) if (layer[i] == nullptr) layer[i] = layer[1];

    auto& v = rpool.allocate(note, voices(polyphony), sr, 0.03, [this](const RVoice& x) { return level(x); });
    rpool.begin(v, note);
    v.shape = takeShape();
    const float age = st.age, det = st.detune, soft = st.softness;
    const float h1 = keyHash(note, 1), h2 = keyHash(note, 2), h4 = keyHash(note, 4);
    // Tuning: the piano's own stretch curve (overall tuning corrected), plus this old piano's
    // key-to-key detune (Detune, Age) as in the model.
    const float stretch = hits.hasStretch ? hits.stretchCents[std::size_t(key)] : 0.0f;
    v.keyCents = stretch + (2.0f * h1 - 1.0f) * (det * 9.0f + age * 3.0f);
    v.pitch = float(note);

    // Velocity: the two layers either side, crossfaded; softer felt plays a little softer layer.
    const float vpos = std::clamp(velocity * (1.0f - 0.3f * soft), 0.0f, 1.0f);
    const auto mix = layerMix(vpos, layerCentres, 3);
    // Each layer at its own recorded loudness, kept in order (soft under medium under full, a
    // few dB apart even where the recording was uneven), so loudness rises with velocity.
    const float lMed = layer[1]->loudnessDb;
    const float target[3] { std::min(layer[0]->loudnessDb, lMed - 5.0f), lMed, std::max(layer[2]->loudnessDb, lMed + 3.0f) };
    float extraDb = vpos < layerCentres[0] ? 24.0f * std::log10(std::max(vpos, 0.03f) / layerCentres[0]) : 0.0f;
    extraDb += (2.0f * h2 - 1.0f) * 2.0f * age + recordedTrimDb;   // uneven voicing
    const float g0 = dbGain(extraDb);
    const float gainA = g0 * mix.wa * dbGain(target[mix.a] - layer[mix.a]->loudnessDb);
    const float gainB = g0 * mix.wb * dbGain(target[mix.b] - layer[mix.b]->loudnessDb);
    const auto* za = layer[mix.a];
    const auto* zb = layer[mix.b];
    // The two strikes are partly in phase: keep the crossfade's level where an uncorrelated mix
    // would be (their measured correlation, liminal.py).
    float norm = 1.0f;
    if (za != zb && mix.wb > 0.0f && mix.wa > 0.0f) {
        const float pa = gainA * dbGain(za->rms300Db), pb = gainB * dbGain(zb->rms300Db);
        const float want = pa * pa + pb * pb;
        const float got = want + 2.0f * za->corrUp * pa * pb;
        norm = std::sqrt(want / std::max(0.05f * want, got));
    }
    const double startA = std::max(1, za->onset - 48), startB = std::max(1, zb->onset - 48);
    const float pitch = v.pitch + (float(note - key));
    v.shortA.start(za, hits.rate, sr, pitch, startA);
    v.shortA.gain = gainA * norm;
    v.shortB.on = false;
    if (mix.wb > 0.0f && zb != za) {
        v.shortB.start(zb, hits.rate, sr, pitch, startB);
        v.shortB.gain = gainB * norm;
    }
    const auto* dom = mix.wb > mix.wa ? zb : za;
    const double domStart = dom == za ? startA : startB;

    // The long note, aligned to the short one at the handover, and as loud and as bright as the
    // dynamic being played.
    v.longNote.on = false;
    v.handCountdown = -1;
    v.tailOn = false;
    v.tailAt = -1;
    if (longSet != nullptr && dom->hand > 0) {
        float shift = 0;
        const auto* lz = dom->longKey >= 0 ? longSet->onKey(dom->longKey, 0) : heldZone(*longSet, key, shift);
        if (lz != nullptr) {
            const double since = double(dom->hand - domStart);
            const double longAt = double(dom->longPos) - since;
            if (v.longNote.start(lz, longSet->rate, sr, pitch, std::max(1.0, longAt), 0.0f)) {
                const auto amp = [](const samples::Zone* z, float g) { return g * dbGain(z->handDb); };
                const float a = amp(za, gainA), b = v.shortB.on ? amp(zb, gainB) : 0.0f;
                v.longNote.gain = std::sqrt(a * a + b * b) / dbGain(dom->longDb);
                v.handLength = std::max(16, int(handSeconds * sr));
                // Hand over before either layer's recorded damper (or a noise of the key after it).
                double first = since;
                if (v.shortB.on && zb->hand > 0) first = std::min(first, double(zb->hand - startB));
                if (za->hand > 0) first = std::min(first, double(za->hand - startA));
                v.handCountdown = std::max(0, int(first / double(v.shortA.inc)) - v.handLength / 2);
                v.tailAt = lz->tailCount > 0 ? lz->tailAt : -1;
            }
        }
    }
    const float wa2 = mix.wa * mix.wa, wb2 = mix.wb * mix.wb;
    const float f0 = noteHz(float(note));
    v.heldTone.clear();
    v.heldTone.set(std::clamp(3.0f * f0, 300.0f, 4500.0f), (wa2 * layerTone[mix.a] + wb2 * layerTone[mix.b]) / std::max(1.0e-6f, wa2 + wb2), sr);
    // Age (a duller soundboard), soft felt, and Liminality's Unevenness.
    v.tone.clear();
    v.tone.set(std::clamp(4.0f * f0, 500.0f, 5000.0f), std::clamp((1.0f - 0.35f * age) * (1.0f - 0.3f * soft) * std::exp2(0.8f * v.shape.tone), 0.25f, 1.8f), sr);

    // Decay: the recordings' own decay; shorter below the middle of Decay, the tail's longer above.
    const float scale = std::exp2((st.decay - 0.5f) * 3.0f) * (1.0f - 0.35f * age) * (1.0f + (2.0f * h4 - 1.0f) * (0.1f + 0.3f * age));
    v.tailDecayScale = std::clamp(scale / 0.88f, 0.25f, 3.0f);
    v.decayMul = scale < 0.88f ? t60(3.0 * double(scale) / std::max(0.02, 0.9 - double(scale)), sr) : 1.0f;
    v.hasDamper = note < 89;
    v.damperT60 = (0.12f + 0.35f * std::clamp(float(89 - note) / 68.0f, 0.0f, 1.0f)) * (1.0f + 2.5f * age);
    v.dampMul = 1.0f;
    v.damped = false;
    v.env = 1.0f;
    const float pan = std::clamp(float(note - 64) / 64.0f, -1.0f, 1.0f) * 0.35f;
    const auto g = panGains(pan);
    v.panL = g.l;
    v.panR = g.r;
    v.wobble.start(rng);
    v.wobbleCents = 0;
    v.level = std::max(gainA, gainB) * dbGain(dom->peakDb);
    controlRecorded(v, 0.0f);

    // The recordings carry the hammer and key noise; the modelled thump only adds a little.
    const float noise = st.noise * 0.35f;
    addEvent(95.0f + 40.0f * keyHash(note, 3), 2.5f, 0.05f, 0.035f * noise * std::pow(velocity, 1.5f), 0.004f * noise * velocity, 2000.0f, pan);
    return true;
}

void ForgottenPiano::controlRecorded(RVoice& v, float tick) {
    const bool damp = v.hasDamper && v.releasing();
    v.wobbleCents = v.wobble.tick(rng, 0.35f, std::max(tick, 1.0e-4f)) * (0.3f + 2.5f * settings.age) * wander();
    const float ratio = bend * cents(v.keyCents + v.wobbleCents + shapeCents(v.shape, tick));
    v.ratioNow = ratio;
    v.shortA.retune(ratio);
    v.shortB.retune(ratio);
    v.longNote.retune(ratio);
    if (damp && !v.damped) {
        v.damped = true;
        v.dampMul = t60(v.damperT60, sr);
        // Let go before the handover: the short note goes on (its own recorded damper follows),
        // the long one is not needed.
        if (v.handCountdown > 0) { v.handCountdown = -1; v.longNote.on = false; }
        if (!v.stolen)
            addEvent(380.0f + 200.0f * keyHash(v.note, 8), 4.0f, 0.03f, 0.012f * settings.noise * (1.0f + settings.age),
                     0.002f * settings.noise * (1.0f + settings.age), 3000.0f, 0.0f);
    } else if (!damp && v.damped) {   // pedal down again while it was being damped
        v.damped = false;
        v.dampMul = 1.0f;
    }
    if (v.tailOn) v.tail.retune(ratio, v.damped);
    // The natural decay takes over where the long recording's damper fell.
    if (!v.tailOn && v.tailAt >= 0 && v.longNote.on && v.longNote.pos >= double(v.tailAt) && v.longNote.zone != nullptr) {
        const auto& z = *v.longNote.zone;
        v.tail.clear();
        const float f0 = noteHz(z.root + v.longNote.semis);
        const float g = v.longNote.gain;
        for (int k = 0; k < z.tailCount; ++k) {
            const auto& md = z.tail[std::size_t(k)];
            const float radians = twoPi * f0 * md.ratio / float(sr);
            if (radians >= 0.95f * pi) continue;
            const double t = double(md.t60) * double(v.tailDecayScale);
            const double d = v.hasDamper ? 1.0 / (1.0 / t + 1.0 / double(v.damperT60)) : t;
            const float side = (k % 2 == 0 ? 1.0f : -1.0f) * 0.12f;
            v.tail.set(k, md.amp * g, twoPi * keyHash(v.note * 64 + k, 17), radians, t60(t, sr), t60(d, sr), 1.0f + side, 1.0f - side);
        }
        v.tail.retune(ratio, v.damped);
        v.tailOn = true;
        v.tailX = 0;
        v.tailStep = 1.0f / float(std::max(64, int(double(z.tailFade) * sr / 44100.0)));
        v.longNote.fadeTo(0.0f, 1.0f / v.tailStep);
    }
    float lvl = std::max(v.shortA.on ? v.shortA.gain : 0.0f, v.shortB.on ? v.shortB.gain : 0.0f);
    if (v.longNote.on) lvl = std::max(lvl, v.longNote.gain * v.longNote.w);
    if (v.tailOn) lvl = std::max(lvl, v.tail.peak());
    v.level = lvl * v.env;
}

void ForgottenPiano::renderRecorded(float* l, float* r, float* mono, int m, bool endOfPeriod) {
    for (auto& v : rpool.voices) {
        if (!v.active()) continue;
        for (int i = 0; i < m; ++i) {
            if (v.handCountdown >= 0 && v.handCountdown-- == 0) {
                v.shortA.fadeTo(0.0f, float(v.handLength));
                v.shortB.fadeTo(0.0f, float(v.handLength));
                v.longNote.fadeTo(1.0f, float(v.handLength));
            }
            float x = v.shortA.tick() + v.shortB.tick();
            if (v.longNote.on) x += v.heldTone.tick(v.longNote.tick());
            if (v.tailOn) {
                const Stereo t = v.tail.tick();
                if (v.tailX < 1.0f) {
                    v.tailX = std::min(1.0f, v.tailX + v.tailStep);
                    v.tailW = fadeIn(v.tailX);
                }
                x += 0.5f * (t.l + t.r) * v.tailW;
            }
            x = v.tone.tick(x);
            v.env *= v.decayMul * v.dampMul;
            float g = v.env;
            if (v.stolen) {
                v.fade = std::max(0.0f, v.fade - v.fadeStep);
                g *= v.fade;
            }
            x *= g;
            l[i] += x * v.panL;
            r[i] += x * v.panR;
            mono[i] += 2.0f * x;
        }
        if (endOfPeriod) {
            const bool silent = !v.shortA.on && !v.shortB.on && !v.longNote.on && (!v.tailOn || v.tail.peak() < 2.0e-5f);
            // Nearly silent: a 50 ms fade, then the slot is free (never a step, however small).
            if (!v.stolen && (silent || v.level < 2.0e-5f)) v.steal(0.05, sr);
            if (v.stolen && v.fade <= 0.0f) v.note = -1;
        }
    }
}

} // namespace liminal::dsp
