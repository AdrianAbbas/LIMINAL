#include "DSP/Woodwind.h"

namespace liminal::dsp {

void Woodwind::setVoice(int voice, bool recorder) {
    voiceNow = voice == woodwind::clarinet ? woodwind::clarinet : woodwind::flute;
    wantRecorder = recorder;
}

void Woodwind::setProfile(const RememberedProfile& p) {
    profile = p;
    treatment.setProfile(p);
}

void Woodwind::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    treatment.prepare(sr);
    treatment.setProfile(profile);
    reset();
}

void Woodwind::reset() {
    for (auto& v : pool.voices) {
        v.note = -1;
        v.fade = 1;
        v.stolen = false;
        v.r.on = false;
    }
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    rng.state = 0x51F15EEDu;
    bend = 1.0f;
    clock = 0;
    treatment.reset();
}

bool Woodwind::recorderRecorded() const { return recordings(woodwind::setOf(woodwind::recorder)) != nullptr; }

int Woodwind::playingVoice() const {
    if (wantRecorder && recorderRecorded()) return woodwind::recorder;
    return wantRecorder ? woodwind::flute : voiceNow;   // (the Recorder voice before it is recorded: the flute)
}

const samples::Instrument* Woodwind::set() const { return recordings(woodwind::setOf(playingVoice())); }

bool Woodwind::recordedNow() const { return set() != nullptr; }

float Woodwind::lowNote() const {
    if (const auto* s = set()) return s->lowRoot - 2.0f;
    return playingVoice() == woodwind::clarinet ? 50.0f : 60.0f;
}
float Woodwind::highNote() const {
    if (const auto* s = set()) return s->highRoot + 2.0f;
    return playingVoice() == woodwind::clarinet ? 89.0f : 96.0f;
}

float Woodwind::standInTrimDb(int voice) {
    // Measured by the `woodwind` tests (the same phrase, K-weighted, recorded vs stand-in, both treated).
    return voice == woodwind::clarinet ? -4.5f : -3.8f;
}

void Woodwind::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.03, [](const Voice& x) { return x.amp * x.gain * x.fade; });
    pool.begin(v, note);
    v.shape = takeShape();
    startVoice(v, note, std::clamp(velocity, 0.0f, 1.0f));
}

void Woodwind::startVoice(Voice& v, int note, float velocity) {
    // Fold into the instrument's range by octaves (a flute does not play an octave below its lowest note).
    float played = float(note);
    const float lo = lowNote(), hi = highNote();
    while (played < lo) played += 12.0f;
    while (played > hi && played - 12.0f >= lo) played -= 12.0f;
    v.played = played;
    v.velocity = velocity;
    v.amp = 0;
    v.age = 0;
    // A woodwind breathes in (the recorded take's own onset, then softer still), and lets go in a breath.
    v.attackCoef = float(1.0 - std::exp(-1.0 / (0.06 * sr)));
    v.releaseCoef = t60(0.9, sr);
    v.gain = 0.4f + 0.6f * std::pow(velocity, 1.3f);
    v.vibPhase = rng.uniform();
    const float side = (pool.counter % 2 == 0 ? 0.18f : -0.18f);
    const auto g = panGains(side);
    v.panL = g.l;
    v.panR = g.r;
    v.recorded = false;
    v.r.on = false;
    if (const auto* s = set()) {
        if (const auto* z = s->find(int(std::round(played)), 0.5f, rng.uniform()); z != nullptr && v.r.start(z, s->rate, sr, played)) {
            v.r.gain = dbGain(-21.0f - z->loudnessDb);   // every note at one loudness
            v.recorded = true;
        }
    }
    if (!v.recorded) {
        v.phase = rng.uniform();
        v.inc = noteHz(played) / float(sr);
        v.chiff = 1.0f;
        v.noise.state = rng.state ^ std::uint32_t(note * 2654435761u) ^ 0xA5A5u;
        if (v.noise.state == 0) v.noise.state = 1;
        v.breath.clear();
        v.tone.z = 0;
        v.breath.set(std::min(0.4f * float(sr), noteHz(played) * 2.0f), 2.5f, sr);
        v.tone.setHz(std::min(0.45f * float(sr), 2600.0f + 1200.0f * velocity), sr);
    }
    control(v, 0.0f);
}

void Woodwind::control(Voice& v, float tick) {
    v.age += tick;
    float c = shapeCents(v.shape, tick);
    // A flute's soft vibrato, after a moment (a muzak player's); the clarinet plays straight. The
    // recorder (a school recorder) too: straight.
    if (playingVoice() == woodwind::flute) {
        v.vibPhase += tick * 4.9f;
        v.vibPhase -= std::floor(v.vibPhase);
        const float grow = std::clamp((v.age - 0.35f) / 0.7f, 0.0f, 1.0f);
        c += 9.0f * grow * std::sin(twoPi * v.vibPhase);
    }
    const float ratio = bend * cents(c);
    if (v.recorded) v.r.retune(ratio);
    else v.inc = std::min(0.45f, noteHz(v.played) * ratio / float(sr));
}

void Woodwind::render(float* left, float* right, int n) {
    int done = 0;
    while (done < n) {
        if (clock == 0) {
            const float tick = float(controlPeriod / sr);
            for (auto& v : pool.voices)
                if (v.active()) control(v, tick);
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        float bufL[controlPeriod] {}, bufR[controlPeriod] {};
        bool any = false;
        const bool clarinet = playingVoice() == woodwind::clarinet;
        const float trim = dbGain(standInTrimDb(clarinet ? woodwind::clarinet : woodwind::flute));
        for (auto& v : pool.voices) {
            if (!v.active()) continue;
            any = true;
            const bool releasing = v.releasing();
            if (releasing && v.recorded && !v.r.released) v.r.release();
            float peak = 0;
            for (int i = 0; i < m; ++i) {
                if (releasing) v.amp *= v.releaseCoef;
                else v.amp += v.attackCoef * (1.0f - v.amp);
                float y;
                if (v.recorded) {
                    y = v.r.tick();
                } else {
                    // The stand-in: a near-sine (flute) or odd harmonics (clarinet), breath around the second
                    // harmonic, a little chiff at the start.
                    const float p = v.phase;
                    const auto h = [&](float k) { float q = p * k; q -= std::floor(q); return sinCycle(q); };
                    float x;
                    if (clarinet) x = 0.55f * h(1) + 0.3f * h(3) + 0.16f * h(5) + 0.07f * h(7) + 0.03f * h(2);
                    else x = 0.75f * h(1) + 0.16f * h(2) + 0.06f * h(3);
                    v.phase += v.inc;
                    v.phase -= std::floor(v.phase);
                    v.breath.tick(v.noise.bipolar());
                    const float air = (clarinet ? 0.06f : 0.16f) + 0.5f * v.chiff;
                    v.chiff *= 0.9992f;
                    x += air * v.breath.band;
                    y = 0.22f * trim * v.tone.lp(x);
                }
                float g = v.amp * v.gain;
                if (v.stolen) {
                    v.fade = std::max(0.0f, v.fade - v.fadeStep);
                    g *= v.fade;
                }
                y *= g;
                bufL[i] += y * v.panL;
                bufR[i] += y * v.panR;
                peak = std::max(peak, std::abs(y));
            }
            if (m == clock) {   // voices end only at the end of a control period
                const bool silent = v.recorded ? !v.r.on : false;
                if (!v.stolen && ((releasing && v.amp < 1.0e-4f) || silent)) v.steal(0.004, sr);
                if (v.stolen && v.fade <= 0.0f) { v.note = -1; v.r.on = false; }
            }
            (void) peak;
        }
        // The treatment always (it sleeps by itself once its tails have died).
        if (!bypassTreatmentForTests()) treatment.process(bufL, bufR, m, wander());
        (void) any;
        for (int i = 0; i < m; ++i) {
            left[done + i] += bufL[i];
            right[done + i] += bufR[i];
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
