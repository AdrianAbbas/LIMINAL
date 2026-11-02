// Liminality (2026-11-01): the shared stage that makes every instrument unstable, detuned and
// half-remembered. Each control is measured doing its job (pitch deviation, wobble rates, ghost
// delay and detune, dropouts, timing spread, ...) and checked fully clean at 0; replays are
// deterministic; parameter jumps never click; the CPU with all six instruments at 100 %.
// Included by LiminalTests.cpp after the other groups.

namespace limtest {

using dsp::lim::Control;

// A stand-in instrument that records what the stage hands it.
struct Probe final : dsp::Instrument {
    struct On { int note; float velocity; dsp::NoteShape shape; std::uint64_t at; };
    std::vector<On> ons;
    std::vector<std::pair<int, std::uint64_t>> offs;
    std::uint64_t now = 0;
    Probe() { ons.reserve(8192); offs.reserve(8192); }
    void prepare(double) override {}
    void reset() override {}
    void noteOn(int n, float v) override { ons.push_back({ n, v, nextShape, now }); nextShape = {}; }
    void noteOff(int n) override { offs.push_back({ n, now }); }
    void sustain(bool) override {}
    void pitchBend(int) override {}
    void allNotesOff() override {}
    void allSoundOff() override {}
    void render(float*, float*, int) override {}
    int activeVoices() const override { return 0; }
    // The hook as a voice uses it.
    dsp::VoiceShape take(const dsp::NoteShape& s) { nextShape = s; return takeShape(); }
    float voiceCents(dsp::VoiceShape& v, float tick) const { return shapeCents(v, tick); }
};

dsp::LiminalAmounts only(int control, float e) {
    dsp::LiminalAmounts a {};
    a[std::size_t(control)] = e;
    return a;
}

// One stage driven like the processor drives it: control every 64 samples, held-back notes
// fired at their sample, audio processed in 64-sample steps.
struct Stage {
    dsp::Liminality lim;
    Probe probe;
    std::uint64_t t = 0;
    double rate;
    explicit Stage(const dsp::LiminalAmounts& a, double sampleRate = 48000.0, int instrument = 0) : rate(sampleRate) {
        lim.setAmounts(a);
        lim.prepare(sampleRate, instrument);
    }
    // Processes `x` (stereo, same length) in place; `onTick` runs at each control tick.
    template <class F>
    void run(std::vector<float>& l, std::vector<float>& r, F onTick) {
        for (std::size_t i = 0; i < l.size(); i += 64) {
            const int n = int(std::min<std::size_t>(64, l.size() - i));
            probe.now = t;
            onTick(t);
            lim.control(probe, t);
            lim.fireDue(probe, t);
            // events due inside this step fire at their sample (as the processor does)
            int done = 0;
            while (done < n) {
                const auto due = lim.nextDue();
                int m = n - done;
                if (due > t && due - t < std::uint64_t(m)) m = int(due - t);
                lim.process(l.data() + i + std::size_t(done), r.data() + i + std::size_t(done), m);
                t += std::uint64_t(m);
                done += m;
                probe.now = t;
                lim.fireDue(probe, t);
            }
        }
    }
    void run(std::vector<float>& l, std::vector<float>& r) { run(l, r, [](std::uint64_t) {}); }
    // The pitch the stage hands the instrument, one value per control tick, for `seconds`.
    std::vector<double> pitchTrace(double seconds) {
        std::vector<double> trace;
        const auto ticks = std::size_t(seconds * rate / 64.0);
        for (std::size_t k = 0; k < ticks; ++k) {
            lim.control(probe, t);
            trace.push_back(lim.pitchNow());
            t += 64;
        }
        return trace;
    }
};

std::vector<float> sine(double hz, double seconds, double amp = 0.5, double rate = 48000.0) {
    std::vector<float> x(std::size_t(seconds * rate));
    for (std::size_t i = 0; i < x.size(); ++i) x[i] = float(amp * std::sin(2.0 * 3.141592653589793 * hz * double(i) / rate));
    return x;
}
double rmsOf(const std::vector<float>& x, std::size_t a, std::size_t b) {
    double s = 0;
    b = std::min(b, x.size());
    for (std::size_t i = a; i < b; ++i) s += double(x[i]) * x[i];
    return b > a ? std::sqrt(s / double(b - a)) : 0.0;
}
// Power spectrum of a trace (sampled at `rate`) summed over [lo, hi] Hz, as a share of [0.02, top].
double traceBand(const std::vector<double>& trace, double rate, double lo, double hi, double top) {
    std::vector<float> x(trace.size());
    const double mean = std::accumulate(trace.begin(), trace.end(), 0.0) / double(std::max<std::size_t>(1, trace.size()));
    for (std::size_t i = 0; i < trace.size(); ++i) x[i] = float(trace[i] - mean);
    const Spectrum sp(x, rate, 1 << 16);
    double band = 0, all = 0;
    for (std::size_t i = 1; i < sp.mag.size(); ++i) {
        const double f = double(i) * sp.binHz, p = sp.mag[i] * sp.mag[i];
        if (f >= 0.02 && f <= top) all += p;
        if (f >= lo && f <= hi) band += p;
    }
    return all > 0 ? band / all : 0.0;
}
// The strongest rate in a trace between lo and hi Hz.
double traceRate(const std::vector<double>& trace, double rate, double lo, double hi) {
    std::vector<float> x(trace.size());
    const double mean = std::accumulate(trace.begin(), trace.end(), 0.0) / double(std::max<std::size_t>(1, trace.size()));
    for (std::size_t i = 0; i < trace.size(); ++i) x[i] = float(trace[i] - mean);
    const Spectrum sp(x, rate, 1 << 16);
    std::size_t best = 0;
    for (std::size_t i = 1; i < sp.mag.size(); ++i) {
        const double f = double(i) * sp.binHz;
        if (f >= lo && f <= hi && (best == 0 || sp.mag[i] > sp.mag[best])) best = i;
    }
    return double(best) * sp.binHz;
}
double peakAbs(const std::vector<double>& v) {
    double m = 0;
    for (const double x : v) m = std::max(m, std::abs(x));
    return m;
}
double stdOf(const std::vector<double>& v) {
    if (v.empty()) return 0;
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / double(v.size());
    double s = 0;
    for (const double x : v) s += (x - mean) * (x - mean);
    return std::sqrt(s / double(v.size()));
}
// Clicks: samples whose second difference stands far out of its surroundings (> 8x the RMS of
// the second difference over +/- 5 ms). A steady tone, hiss or a slow wobble never trips it.
int clicks(const std::vector<float>& x, double rate, std::size_t from = 0) {
    std::vector<double> d(x.size(), 0.0);
    for (std::size_t i = 2; i < x.size(); ++i) d[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    const auto half = std::size_t(0.005 * rate);
    int count = 0;
    double sum = 0;
    std::size_t lo = 0, hi = 0;
    for (std::size_t i = std::max(from, half); i + half < x.size(); ++i) {
        while (hi < i + half) { sum += d[hi] * d[hi]; ++hi; }
        while (lo < i - half) { sum -= d[lo] * d[lo]; ++lo; }
        const double local = std::sqrt(std::max(0.0, sum) / double(hi - lo));
        if (std::abs(d[i]) > 8.0 * local + 1.0e-7) { ++count; i += half; }
    }
    return count;
}

void setLim(Rig& rig, int instrument, int control, float v) {
    auto* p = rig.p->parameters.getParameter(params::limId(instrument, dsp::lim::keys[control]));
    p->setValueNotifyingHost(p->convertTo0to1(v));
}
void allInstruments(Rig& rig) {
    for (const char* key : params::instrumentKeys) rig.set((juce::String(key) + "_on").toRawUTF8(), 1.0f);
}
void allLim(Rig& rig, float v) {
    for (int i = 0; i < params::instrumentCount; ++i)
        for (int k = 0; k < dsp::lim::count; ++k) setLim(rig, i, k, v);
}
// A dry rig: no ambience, no room, no environment processing (and so no room nudge).
std::unique_ptr<Rig> dryRig(double rate = 48000.0, int block = 512) {
    auto rig = std::make_unique<Rig>(rate, block);
    rig->set(params::ambience, params::minDb);
    rig->set(params::space, 0.0f);
    rig->set(params::environmentAmount, 0.0f);
    rig->p->prepareToPlay(rate, block);
    return rig;
}
// A fragile piano phrase (also the listening render's).
std::vector<Event> fragilePhrase(double t) {
    std::vector<Event> e;
    const int melody[] { 76, 72, 69, 71, 72, 69, 64, 67, 69 };
    const double at[] { 0.0, 0.6, 1.2, 1.8, 2.4, 3.3, 3.9, 4.5, 5.1 };
    for (int i = 0; i < 9; ++i) {
        e.push_back({ t + at[i], on(melody[i], 62 + (i % 3) * 8) });
        e.push_back({ t + at[i] + (i == 8 ? 3.4 : 0.55), off(melody[i]) });
    }
    for (int n : { 45, 52, 57 }) {
        e.push_back({ t + 0.0, on(n, 50) });
        e.push_back({ t + 2.9, off(n) });
    }
    for (int n : { 41, 48, 57, 60 }) {
        e.push_back({ t + 3.3, on(n, 52) });
        e.push_back({ t + 8.5, off(n) });
    }
    return e;
}

// Renders with each event at its exact sample (the rig's own render rounds per block, which can move
// a note by one sample between block sizes - and a note's randomness follows its exact position).
Signal renderExact(Rig& rig, double seconds, const std::vector<Event>& events, int block) {
    const double rate = rig.rate;
    const auto start = std::int64_t(std::llround(rig.time * rate));
    const auto total = std::int64_t(seconds * rate);
    Signal out;
    juce::AudioBuffer<float> buffer(2, block);
    juce::MidiBuffer midi;
    for (std::int64_t done = 0; done < total; done += block) {
        const int n = int(std::min<std::int64_t>(block, total - done));
        buffer.setSize(2, n, false, false, true);
        midi.clear();
        for (const auto& e : events) {
            const auto at = std::int64_t(std::llround(e.at * rate)) - start;
            if (at >= done && at < done + n) midi.addEvent(e.message, int(at - done));
        }
        rig.p->processBlock(buffer, midi);
        for (int i = 0; i < n; ++i) {
            out.first.push_back(buffer.getSample(0, i));
            out.second.push_back(buffer.getSample(1, i));
        }
    }
    rig.time += double(total) / rate;
    return out;
}

// A host's playhead for the replay test.
struct Head final : juce::AudioPlayHead {
    juce::int64 samples = 0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setTimeInSamples(samples);
        info.setBpm(100.0);
        return info;
    }
};

} // namespace limtest

void testLiminality(Suite& s) {
    using namespace limtest;
    namespace lim = dsp::lim;
    s.section("liminality (2026-11-01): parameters");
    const double rate = 48000.0;
    {
        LiminalProcessor p;
        auto* macro = p.parameters.getParameter(params::liminality);
        s.check(macro != nullptr && std::abs(macro->convertFrom0to1(macro->getDefaultValue()) - 0.4f) < 1.0e-3f, "the LIMINALITY macro exists, default 40 %");
        int found = 0, inRange = 0;
        for (int i = 0; i < params::instrumentCount; ++i)
            for (int k = 0; k < lim::count; ++k)
                if (auto* q = p.parameters.getParameter(params::limId(i, lim::keys[k]))) {
                    ++found;
                    const float d = q->convertFrom0to1(q->getDefaultValue());
                    inRange += d >= 0.35f - 1e-4f && d <= 0.45f + 1e-4f ? 1 : 0;
                }
        s.check(found == params::instrumentCount * lim::count, "every instrument has the ten Liminality controls", std::to_string(found));
        s.check(inRange == found, "defaults are noticeably liminal but musical (35-45 %)", std::to_string(inRange) + " of " + std::to_string(found));
    }

    s.section("liminality: clean at 0");
    {   // the stage alone: all amounts 0 -> the audio is untouched and nothing is decided
        Stage st(dsp::LiminalAmounts {});
        auto l = sine(440, 2.0), r = sine(660, 2.0);
        const auto l0 = l, r0 = r;
        int decided = 0;
        st.run(l, r, [&](std::uint64_t t) {
            if (t % 4800 == 0) {
                st.lim.noteOn(st.probe, 40 + int(t / 4800) % 40, 0.7f, t, t);
                const auto d = st.lim.lastDecision();
                decided += (d.cents != 0 || d.tone != 0 || d.velocityScale != 1 || d.delay != 0) ? 1 : 0;
            }
        });
        bool shapes = true;
        for (const auto& o : st.probe.ons) shapes = shapes && o.shape.cents == 0.0f && o.shape.tone == 0.0f && o.velocity == 0.7f && o.at % 4800 == 0;
        const auto p = st.probe.liminalPitch();
        s.check(l == l0 && r == r0 && st.lim.audioBypassed(), "every control at 0: the audio passes bit-identical (stage bypassed)");
        s.check(decided == 0 && shapes && p.cents == 0.0f && p.warble == 0.0f && p.wander == 0.0f,
                "every control at 0: notes on time, in tune, unchanged; no pitch movement (the instruments' own wander off too)");
    }
    {   // the whole plug-in: macro 0, and sliders 0 at macro 100 %, give the same clean output
        auto a = dryRig(), b = dryRig();
        allInstruments(*a);
        allInstruments(*b);
        a->set(params::liminality, 0.0f);
        b->set(params::liminality, 1.0f);
        allLim(*b, 0.0f);
        a->render(0.2);
        b->render(0.2);
        const auto x = a->render(9.0, fragilePhrase(a->time)), y = b->render(9.0, fragilePhrase(b->time));
        bool bypassed = true;
        for (int i = 0; i < params::instrumentCount; ++i) bypassed = bypassed && a->p->liminalityOf(i).audioBypassed() && a->p->liminalityOf(i).pitchNow() == 0.0f;
        s.check(x == y && bypassed, "LIMINALITY 0 % = every slider at 0: identical output, all six stages bypassed");
        // each control alone audibly changes the sound
        std::string quiet;
        for (int k = 0; k < lim::count; ++k) {
            auto c = dryRig();
            allInstruments(*c);
            c->set(params::liminality, 1.0f);
            allLim(*c, 0.0f);
            for (int i = 0; i < params::instrumentCount; ++i) setLim(*c, i, k, 0.4f);
            c->render(0.2);
            const auto z = c->render(9.0, fragilePhrase(c->time));
            double diff = 0, sig = 0;
            for (std::size_t i = 0; i < z.first.size(); ++i) {
                diff += double(z.first[i] - x.first[i]) * (z.first[i] - x.first[i]);
                sig += double(x.first[i]) * x.first[i];
            }
            const double change = 10.0 * std::log10(diff / std::max(1e-20, sig));
            s.metric(std::string("change from clean, ") + lim::keys[k] + " alone at 100 % macro", change, " dB");
            if (change < -40.0) quiet += std::string(lim::keys[k]) + " ";
        }
        s.check(quiet.empty(), "each control alone audibly changes all six instruments (difference above -40 dB)", quiet.empty() ? "all ten" : quiet);
    }
    {   // LIMINALITY 0: nothing moves with time (the same note played later is the same sound,
        // sample for sample: no drift, wow or wander of its own); with Drift up it moves.
        const auto noteAt = [&](float macro, float drift, double wait) {
            auto rig = dryRig();
            rig->set(params::liminality, macro);
            allLim(*rig, 0.0f);
            setLim(*rig, 0, lim::drift, drift);
            rig->render(0.5 + wait);
            return rig->render(4.0, { { rig->time + 0.0, on(69, 90) } });
        };
        const auto x = noteAt(0.0f, 0.4f, 0.0), y = noteAt(0.0f, 0.4f, 3.7);
        double worst = 0;
        for (std::size_t i = 0; i < x.first.size(); ++i) worst = std::max(worst, double(std::abs(x.first[i] - y.first[i])));
        s.check(worst < 1.0e-6, "the piano at LIMINALITY 0: the same note now or 3.7 s later is identical (perfectly stable)", "max difference " + num(worst, 8));
        // the same note started at different moments: its pitch differs with Drift up, not at 0
        const auto pitchOf = [&](const Signal& sig) {
            std::vector<double> track;
            for (double t = 0.3; t + 0.2 < 1.4; t += 0.1) track.push_back(1200.0 * std::log2(frequency(monoOf(sig, rate, t, t + 0.18), rate) / 440.0));
            return std::accumulate(track.begin(), track.end(), 0.0) / double(track.size());
        };
        std::vector<double> still, moving;
        for (const double wait : { 0.0, 4.0, 9.0, 15.0, 22.0 }) {
            still.push_back(pitchOf(noteAt(0.0f, 0.4f, wait)));
            moving.push_back(pitchOf(noteAt(1.0f, 0.4f, wait)));
        }
        s.check(stdOf(still) < 0.05 && stdOf(moving) > 2.0, "... and with Drift up the same note comes out at a different pitch each time",
                num(stdOf(still), 2) + " -> " + num(stdOf(moving), 2) + " cents between five starts");
    }

    s.section("liminality: each control does its job");
    // Detune: per note, mostly per key; a few to ~30 cents at 100 %.
    {
        for (const float e : { 0.4f, 1.0f }) {
            Stage st(only(lim::detune, e));
            std::vector<double> c, sameKey;
            for (int i = 0; i < 600; ++i) {
                st.lim.control(st.probe, st.t);
                const int note = 36 + (i * 7) % 61;
                st.lim.noteOn(st.probe, note, 0.8f, st.t, st.t);
                c.push_back(st.lim.lastDecision().cents);
                if (note == 60) sameKey.push_back(c.back());
                st.t += 9600;
            }
            std::vector<double> absC;
            for (const double x : c) absC.push_back(std::abs(x));
            const double mean = std::accumulate(absC.begin(), absC.end(), 0.0) / double(absC.size());
            const double maxC = peakAbs(c);
            s.metric("detune at " + num(e) + ": mean |cents|", mean);
            s.metric("detune at " + num(e) + ": max |cents|", maxC);
            s.check(maxC <= 30.0 * e + 0.01 && mean > 2.5 * e && mean < 14.0 * e, "Detune " + num(e * 100) + " %: each note off by a few cents, at most " + num(30 * e) + " (stays near the note)",
                    "mean " + num(mean, 1) + ", max " + num(maxC, 1));
            if (e == 1.0f) s.check(stdOf(sameKey) < 0.6 * stdOf(c), "Detune: mostly each key's own (a neglected piano), a little per strike",
                                   "same key " + num(stdOf(sameKey), 1) + " vs all " + num(stdOf(c), 1));
        }
        // the instruments honour the hook: a +50 cent shape comes out 50 cents sharp (their synthesised
        // voices; the recorded ones are checked the same way in the recorded group, over a long held
        // stretch, since a sung note wanders on its own within this short window)
        std::string wrong;
        BypassRecordings standIns;
        dsp::ForgottenPiano piano;
        dsp::FadedSynth synth;
        dsp::GlassMemories glass;
        dsp::EmptyChoir choir;
        dsp::LostMusicBox box;
        dsp::HollowDrone drone;
        dsp::Instrument* all[] { &piano, &synth, &glass, &choir, &box, &drone };
        const char* names[] { "piano", "synth", "glass", "choir", "box", "drone" };
        for (int i = 0; i < 6; ++i) {
            const auto play = [&](float cents) {
                all[i]->prepare(rate);
                all[i]->setLiminalPitch({ 0.0f, 0.0f, 0.0f });   // no wander of its own: a clean measure
                all[i]->shapeNextNote({ cents, 0.0f, 7u });
                all[i]->noteOn(69, 0.8f);
                std::vector<float> l(std::size_t(rate * 1.5), 0.0f), r(l.size(), 0.0f);
                for (std::size_t k = 0; k < l.size(); k += 256) all[i]->render(l.data() + k, r.data() + k, int(std::min<std::size_t>(256, l.size() - k)));
                std::vector<float> m(l.begin() + std::ptrdiff_t(rate * 0.6), l.begin() + std::ptrdiff_t(rate * 1.4));
                for (std::size_t k = 0; k < m.size(); ++k) m[k] += r[k + std::size_t(rate * 0.6)];
                return Spectrum(m, rate, 1 << 17).peakNear(440.0 * std::exp2(cents / 1200.0), 40).first;
            };
            const double shift = 1200.0 * std::log2(play(50.0f) / play(0.0f));
            if (std::abs(shift - 50.0) > 3.0) wrong += std::string(names[i]) + " " + num(shift, 1) + " ";
        }
        s.check(wrong.empty(), "all six instruments apply a note's Liminality pitch (+50 cents -> +50 +/- 3)", wrong.empty() ? "all six" : wrong);
    }
    // Sour notes: a small chance, clearly off (microtonal), never a wrong key.
    {
        for (const float e : { 0.4f, 1.0f }) {
            Stage st(only(lim::sour, e));
            int sour = 0, outside = 0;
            const int n = 3000;
            for (int i = 0; i < n; ++i) {
                st.lim.control(st.probe, st.t);
                st.lim.noteOn(st.probe, 40 + i % 50, 0.8f, st.t, st.t);
                const auto d = st.lim.lastDecision();
                if (d.sour) {
                    ++sour;
                    outside += (std::abs(d.cents) < 32.0f || std::abs(d.cents) > 62.0f) ? 1 : 0;
                } else {
                    outside += d.cents != 0.0f ? 1 : 0;
                }
                st.t += 4800;
            }
            const double share = double(sour) / n, expected = std::min(0.35, 0.16 * std::pow(double(e), 1.3));
            s.metric("sour notes at " + num(e) + ": share", share);
            s.check(std::abs(share - expected) < 0.25 * expected + 0.01 && outside == 0,
                    "Sour Notes " + num(e * 100) + " %: about " + num(expected * 100, 1) + " % of notes clearly off (32-62 cents), the rest in tune",
                    num(share * 100, 1) + " %, " + std::to_string(outside) + " outside");
        }
        Stage st([] { dsp::LiminalAmounts a {}; a.fill(lim::maxEffective); return a; }());
        double worst = 0;
        for (int i = 0; i < 4000; ++i) {
            st.lim.control(st.probe, st.t);
            st.lim.noteOn(st.probe, 30 + i % 70, 0.8f, st.t, st.t);
            worst = std::max(worst, double(std::abs(st.lim.lastDecision().cents)));
            st.t += 4800;
        }
        s.check(worst <= 85.0, "everything at the extreme: no note further than 85 cents from its key (never a wrong key)", num(worst, 1) + " cents");
    }
    // Unevenness: some notes quieter, duller or brighter.
    {
        Stage st(only(lim::uneven, 1.0f));
        std::vector<double> db, tone;
        for (int i = 0; i < 600; ++i) {
            st.lim.control(st.probe, st.t);
            st.lim.noteOn(st.probe, 40 + (i * 5) % 48, 0.8f, st.t, st.t);
            const auto d = st.lim.lastDecision();
            db.push_back(1.6 * toDb(d.velocityScale));
            tone.push_back(d.tone);
            st.t += 4800;
        }
        const double quietest = *std::min_element(db.begin(), db.end());
        s.check(quietest < -6.0 && *std::max_element(db.begin(), db.end()) <= 0.0 && stdOf(db) > 1.0,
                "Unevenness 100 %: some notes clearly quieter (worn keys), never louder", "quietest " + num(quietest, 1) + " dB, spread " + num(stdOf(db), 1) + " dB");
        s.check(stdOf(tone) > 0.25 && *std::min_element(tone.begin(), tone.end()) < -0.5 && *std::max_element(tone.begin(), tone.end()) > 0.5,
                "Unevenness 100 %: some notes duller, some brighter", "tone spread " + num(stdOf(tone), 2));
        // heard: the same note at the same velocity, repeated on the piano, varies in level
        auto rig = dryRig();
        rig->set(params::liminality, 1.0f);
        allLim(*rig, 0.0f);
        setLim(*rig, 0, lim::uneven, 0.4f);
        rig->render(0.2);
        std::vector<Event> ev;
        for (int i = 0; i < 16; ++i) {
            ev.push_back({ rig->time + 0.4 * i, on(48 + (i * 5) % 24, 90) });
            ev.push_back({ rig->time + 0.4 * i + 0.3, off(48 + (i * 5) % 24) });
        }
        const double t0 = rig->time;
        const auto x = rig->render(6.6, ev);
        std::vector<double> levels;
        for (int i = 0; i < 16; ++i) levels.push_back(windowDb(x, rate, 0.4 * i + 0.01, 0.4 * i + 0.11));
        auto clean = dryRig();
        clean->set(params::liminality, 0.0f);
        clean->render(0.2);
        std::vector<Event> ev2;
        for (const auto& e : ev) ev2.push_back({ e.at - t0 + clean->time, e.message });
        const auto y = clean->render(6.6, ev2);
        std::vector<double> diffs;
        for (int i = 0; i < 16; ++i) diffs.push_back(levels[std::size_t(i)] - windowDb(y, rate, 0.4 * i + 0.01, 0.4 * i + 0.11));
        s.check(stdOf(diffs) > 1.0 && *std::min_element(diffs.begin(), diffs.end()) < -2.5, "Unevenness heard on the piano: notes come out unexpectedly quieter",
                "spread " + num(stdOf(diffs), 1) + " dB, quietest " + num(*std::min_element(diffs.begin(), diffs.end()), 1) + " dB");
    }
    // Timing: notes held back unevenly; note-offs follow their notes (durations kept).
    {
        for (const float e : { 0.4f, 1.0f }) {
            Stage st(only(lim::timing, e));
            std::vector<float> l(std::size_t(rate * 120), 0.0f), r(l.size(), 0.0f);
            std::vector<std::pair<int, std::uint64_t>> sent;
            int next = 0;
            st.run(l, r, [&](std::uint64_t t) {
                if (t >= std::uint64_t(next) * 9600 && next < 500) {
                    const int note = 40 + (next * 7) % 40;
                    st.lim.noteOn(st.probe, note, 0.8f, t, t);
                    sent.push_back({ note, t });
                    ++next;
                }
                // each note is released 2432 samples (38 ticks) after it was played
                for (const auto& sn : sent)
                    if (sn.second + 2432 == t) st.lim.noteOff(st.probe, sn.first, t);
            });
            std::vector<double> delays;
            int badDuration = 0;
            for (std::size_t i = 0; i < st.probe.ons.size() && i < sent.size(); ++i) {
                const auto& o = st.probe.ons[i];
                std::uint64_t at = 0;
                for (const auto& sn : sent)
                    if (sn.first == o.note && sn.second <= o.at && o.at - sn.second < std::uint64_t(rate)) at = sn.second;
                delays.push_back(1000.0 * double(o.at - at) / rate);
                for (const auto& f : st.probe.offs)
                    if (f.first == o.note && f.second >= o.at && f.second - o.at < std::uint64_t(rate * 0.2)) {
                        badDuration += f.second - o.at != 2432 ? 1 : 0;
                        break;
                    }
            }
            const double maxMs = *std::max_element(delays.begin(), delays.end());
            s.metric("timing at " + num(e) + ": spread (std)", stdOf(delays), " ms");
            s.check(st.probe.ons.size() == 500 && maxMs <= 60.0 * e + 0.1 && stdOf(delays) > 3.0 * e,
                    "Timing " + num(e * 100) + " %: notes land unevenly, 0-" + num(60 * e) + " ms late", "spread " + num(stdOf(delays), 1) + " ms, latest " + num(maxMs, 1) + " ms");
            s.check(badDuration == 0 && st.probe.offs.size() == st.probe.ons.size(), "Timing: each note-off follows its note by the same delay (lengths kept)",
                    std::to_string(badDuration) + " changed");
        }
    }
    // Drift: a slow wander that drifts away and comes back.
    {
        Stage st(only(lim::drift, 1.0f));
        const auto trace = st.pitchTrace(120.0);
        const double tickRate = rate / 64.0;
        const double slow = traceBand(trace, tickRate, 0.0, 0.5, 20.0);
        s.check(peakAbs(trace) > 8.0 && peakAbs(trace) < 30.0 && stdOf(trace) > 3.0, "Drift 100 %: the pitch wanders away by up to ~20 cents",
                "peak " + num(peakAbs(trace), 1) + ", spread " + num(stdOf(trace), 1) + " cents");
        double mean = std::accumulate(trace.begin(), trace.end(), 0.0) / double(trace.size());
        s.check(slow > 0.9 && std::abs(mean) < 0.5 * stdOf(trace) + 1.0, "Drift: slow (90 % of its movement under 0.5 Hz) and it comes back (centred)",
                num(slow * 100, 1) + " % slow, mean " + num(mean, 1) + " cents");
    }
    // Wow & Flutter: slow wow plus faster flutter.
    {
        Stage st(only(lim::wow, 1.0f));
        const auto trace = st.pitchTrace(60.0);
        const double tickRate = rate / 64.0;
        const double wowRate = traceRate(trace, tickRate, 0.2, 3.0), flutterRate = traceRate(trace, tickRate, 4.0, 20.0);
        const double flutterShare = traceBand(trace, tickRate, 5.0, 14.0, 40.0);
        s.metric("wow rate", wowRate, " Hz");
        s.metric("flutter rate", flutterRate, " Hz");
        s.check(wowRate > 0.5 && wowRate < 0.9 && flutterRate > 6.0 && flutterRate < 13.0 && flutterShare > 0.02,
                "Wow & Flutter 100 %: a slow wow (0.5-0.9 Hz) and a faster flutter (6-13 Hz)", "wow " + num(wowRate, 2) + " Hz, flutter " + num(flutterRate, 1) + " Hz");
        s.check(peakAbs(trace) > 8.0 && peakAbs(trace) < 20.0, "Wow & Flutter 100 %: about +/-12 cents of tape wobble", num(peakAbs(trace), 1) + " cents");
    }
    // Warble: each sustained voice wavers on its own, growing in after the attack; plus a swell.
    {
        Stage st(only(lim::warble, 1.0f));
        st.lim.control(st.probe, 0);
        st.probe.setLiminalPitch(st.probe.liminalPitch());
        auto voice = st.probe.take({ 0.0f, 0.0f, 12345u });
        auto other = st.probe.take({ 0.0f, 0.0f, 999u });
        std::vector<double> a, b, early;
        const float tick = float(64.0 / rate);
        for (int k = 0; k < int(5.0 * rate / 64.0); ++k) {
            a.push_back(st.probe.voiceCents(voice, k == 0 ? 0.0f : tick));
            b.push_back(st.probe.voiceCents(other, k == 0 ? 0.0f : tick));
            if (k * 64 < int(0.1 * rate)) early.push_back(a.back());
        }
        std::vector<double> late(a.begin() + std::ptrdiff_t(2.0 * rate / 64.0), a.end()), lateB(b.begin() + std::ptrdiff_t(2.0 * rate / 64.0), b.end());
        const double r = traceRate(late, rate / 64.0, 0.1, 3.0);
        s.check(peakAbs(early) < 0.5 && peakAbs(late) > 8.0, "Warble 100 %: the attack is clean, the held note then wavers (no scoop)",
                "first 100 ms " + num(peakAbs(early), 2) + ", later " + num(peakAbs(late), 1) + " cents");
        double diff = 0;
        for (std::size_t i = 0; i < late.size(); ++i) diff = std::max(diff, std::abs(late[i] - lateB[i]));
        s.check(r > 0.25 && r < 1.0 && diff > 6.0, "Warble: slow and seasick (0.25-1 Hz), each voice on its own", num(r, 2) + " Hz, voices up to " + num(diff, 1) + " cents apart");
        // the swell on the audio side, while a note is held
        Stage sw(only(lim::warble, 1.0f));
        auto l = sine(440, 6.0), rr = sine(440, 6.0);
        sw.run(l, rr, [&](std::uint64_t t) { if (t == 0) sw.lim.noteOn(sw.probe, 69, 0.8f, t, t); });
        std::vector<double> env;
        for (std::size_t i = std::size_t(2.0 * rate); i + 2400 < l.size(); i += 2400) env.push_back(toDb(rmsOf(l, i, i + 2400)));
        const double swing = *std::max_element(env.begin(), env.end()) - *std::min_element(env.begin(), env.end());
        s.check(swing > 1.5 && swing < 12.0, "Warble 100 %: a slow volume wavering on the held note", num(swing, 1) + " dB swing");
    }
    // Ghost: a second, slightly detuned and delayed copy.
    {
        Stage st(only(lim::ghost, 1.0f));
        std::vector<float> l(std::size_t(rate * 1.0), 0.0f), r(l.size(), 0.0f);
        const std::size_t at = std::size_t(0.3 * rate);
        l[at] = r[at] = 1.0f;
        st.run(l, r);
        std::size_t best = at + 1;
        for (std::size_t i = at + 48; i < at + std::size_t(0.25 * rate); ++i)
            if (std::abs(l[i]) > std::abs(l[best])) best = i;
        double energy = 0;
        for (std::size_t i = at + 48; i < at + std::size_t(0.25 * rate); ++i) energy += double(l[i]) * l[i];
        const double delayMs = 1000.0 * double(best - at) / rate;
        s.check(delayMs > 50.0 && delayMs < 150.0 && toDb(std::sqrt(energy)) > -18.0, "Ghost 100 %: a delayed copy 50-150 ms after the note",
                num(delayMs, 1) + " ms, " + num(toDb(std::sqrt(energy)), 1) + " dB");
        Stage tone(only(lim::ghost, 1.0f));
        auto x = sine(440, 10.0), y = sine(440, 10.0);
        tone.run(x, y);
        std::vector<float> tail(x.begin() + std::ptrdiff_t(rate * 1.0), x.end());
        const Spectrum sp(tail, rate, 1 << 19);
        double bestHz = 0, bestMag = 0;
        for (std::size_t i = 1; i < sp.mag.size(); ++i) {
            const double f = double(i) * sp.binHz, c = 1200.0 * std::log2(f / 440.0);
            if (std::abs(c) > 3.0 && std::abs(c) < 30.0 && sp.mag[i] > bestMag) { bestMag = sp.mag[i]; bestHz = f; }
        }
        const double ghostCents = 1200.0 * std::log2(bestHz / 440.0);
        s.check(std::abs(ghostCents) > 8.0 && std::abs(ghostCents) < 18.0, "Ghost 100 %: the copy is slightly detuned (~13 cents), like two instruments not quite matching",
                num(ghostCents, 1) + " cents");
    }
    // Wear: lost highs and lows, saturation, hiss and crackle under the playing, dropouts.
    {
        const auto through = [&](float e, double hz, double amp, double seconds = 2.0) {
            Stage st(only(lim::wear, e));
            auto l = sine(hz, seconds, amp), r = sine(hz, seconds, amp);
            st.run(l, r);
            return l;
        };
        const auto gainAt = [&](double hz) { const auto y = through(1.0f, hz, 0.1); return toDb(rmsOf(y, std::size_t(rate), y.size()) / (0.1 / std::sqrt(2.0))); };
        const double hi = gainAt(12000), lo = gainAt(40), mid = gainAt(1000);
        s.check(hi < -8.0 && lo < -4.0 && std::abs(mid) < 2.0, "Wear 100 %: the highs and lows are lost, the middle stays",
                "12 kHz " + num(hi, 1) + ", 40 Hz " + num(lo, 1) + ", 1 kHz " + num(mid, 1) + " dB");
        const auto loud = through(1.0f, 250, 0.8), clean = through(0.0f, 250, 0.8);
        const auto harmonic = [&](const std::vector<float>& y) {
            std::vector<float> t(y.begin() + std::ptrdiff_t(rate), y.end());
            const Spectrum sp(t, rate, 1 << 17);
            return toDb(sp.peakNear(750, 20).second / sp.peakNear(250, 20).second);
        };
        s.check(harmonic(loud) > -45.0 && harmonic(clean) < -80.0, "Wear 100 %: soft saturation on loud notes", "3rd harmonic " + num(harmonic(loud), 1) + " dB (clean " + num(harmonic(clean), 1) + ")");
        // hiss under the playing, gone in the silence after
        Stage st(only(lim::wear, 1.0f));
        auto l = sine(300, 8.0, 0.3), r = sine(300, 8.0, 0.3);
        for (std::size_t i = std::size_t(2.0 * rate); i < l.size(); ++i) l[i] = r[i] = 0.0f;
        st.run(l, r);
        std::vector<float> playing(l.begin() + std::ptrdiff_t(0.8 * rate), l.begin() + std::ptrdiff_t(1.8 * rate));
        const double hiss = Spectrum(playing, rate).bandDb(8000, 14000) - Spectrum(through(0.0f, 300, 0.3), rate).bandDb(8000, 14000);
        const double after = toDb(rmsOf(l, std::size_t(7.0 * rate), l.size()));
        s.check(hiss > 20.0 && after < -110.0, "Wear 100 %: faint hiss under the playing, silence once it stops",
                "hiss +" + num(hiss, 0) + " dB in 8-14 kHz; after: " + num(after, 0) + " dB");
        // dropouts: occasional tiny dips
        for (const float e : { 0.4f, 1.0f }) {
            Stage d(only(lim::wear, e));
            auto x = sine(1000, 120.0, 0.3), y = sine(1000, 120.0, 0.3);
            d.run(x, y);
            int dips = 0;
            double deepest = 0;
            bool in = false;
            const double ref = toDb(rmsOf(x, std::size_t(rate), std::size_t(2 * rate)));
            for (std::size_t i = std::size_t(rate); i + 240 < x.size(); i += 240) {
                const double lvl = toDb(rmsOf(x, i, i + 240)) - ref;
                deepest = std::min(deepest, lvl);
                if (lvl < -3.0 && !in) { ++dips; in = true; }
                if (lvl > -1.0) in = false;
            }
            s.metric("dropouts in 2 minutes at wear " + num(e), double(d.lim.dropouts()));
            const double expected = 0.25 * std::pow(e, 1.5) * 120.0;
            s.check(dips >= int(0.4 * expected) && dips <= int(2.0 * expected) + 2 && deepest < -6.0,
                    "Wear " + num(e * 100) + " %: an occasional tiny dropout (about " + num(expected, 0) + " in 2 minutes)",
                    std::to_string(dips) + " dips, deepest " + num(deepest, 1) + " dB");
        }
    }
    // Fade: held notes slowly lose their highs; a new note is clear again.
    {
        Stage st(only(lim::fade, 1.0f));
        auto l = sine(6000, 8.0, 0.2), r = sine(6000, 8.0, 0.2);
        st.run(l, r, [&](std::uint64_t t) {
            if (t == 0 || t == std::uint64_t(6.0 * rate) / 64 * 64) st.lim.noteOn(st.probe, 60, 0.8f, t, t);
        });
        const double start = toDb(rmsOf(l, std::size_t(0.05 * rate), std::size_t(0.25 * rate))), held = toDb(rmsOf(l, std::size_t(5.5 * rate), std::size_t(5.9 * rate)));
        const double again = toDb(rmsOf(l, std::size_t(6.15 * rate), std::size_t(6.3 * rate)));
        s.check(start - held > 8.0 && again - held > 5.0, "Fade 100 %: a held note dissolves (its highs fade), the next note is clear again",
                "6 kHz: " + num(start, 1) + " -> " + num(held, 1) + " dB, new note " + num(again, 1) + " dB");
    }

    s.section("liminality: rooms and Characters nudge it");
    {
        using dsp::liminalNudge;
        const auto n = [&](int place, int character, int k) { return liminalNudge(place, character)[std::size_t(k)]; };
        const bool school = n(2, 0, lim::wow) > 1.3f && n(2, 0, lim::wear) > 1.3f;
        const bool hotel = n(5, 0, lim::wear) > 1.2f && n(5, 0, lim::fade) > 1.3f;
        const bool hospital = n(0, 0, lim::detune) > 1.3f;
        const bool impossible = n(9, 0, lim::sour) > 1.5f && n(9, 0, lim::ghost) > 1.3f;
        const bool pool = n(4, 0, lim::warble) > 1.4f;
        s.check(school && hotel && hospital && impossible && pool,
                "School: wow + wear; Hotel: wear + fade; Hospital: detune; Impossible Room: sour + ghost; Pool: warble");
        const bool wrong = n(0, 2, lim::sour) > 1.5f * n(0, 0, lim::sour) && n(0, 2, lim::drift) > 1.4f * n(0, 0, lim::drift);
        const bool r404 = n(5, 2, lim::sour) > 1.5f * n(5, 0, lim::sour) && n(5, 2, lim::drift) > 1.4f * n(5, 0, lim::drift);
        const bool beyond = n(9, 2, lim::sour) > 1.5f * n(9, 0, lim::sour) && n(9, 2, lim::drift) > 1.4f * n(9, 0, lim::drift);
        s.check(wrong && r404 && beyond, "Something Wrong, Room 404, Beyond Reality: more sour notes and drift");
        dsp::LiminalAmounts own {};
        own.fill(0.4f);
        const auto dry = dsp::liminalEffective(own, 0.4f, 9, 2, 0.0f), wet = dsp::liminalEffective(own, 0.4f, 9, 2, 1.0f);
        const auto zero = dsp::liminalEffective(own, 0.0f, 9, 2, 1.0f), full = dsp::liminalEffective(own, 1.0f, 0, 0, 0.0f);
        s.check(dry[lim::sour] == 0.4f && wet[lim::sour] > 0.9f && zero[lim::sour] == 0.0f && std::abs(full[lim::detune] - 1.0f) < 1e-5f,
                "Environment Amount 0: no nudge; LIMINALITY 0 %: clean in every room; 100 %: 2.5x (deeply unsettling)");
    }

    s.section("liminality: musical (the same loudness, near the note)");
    {   // it changes the character, not the level: the fragile piano phrase at 0 / 40 / 100 %
        const auto level = [&](int place, float macro) {
            std::unique_ptr<Rig> rig;
            if (place < 0) {
                rig = dryRig();
            } else {
                rig = rigIn(place);
                rig->set(params::ambience, params::minDb);
            }
            rig->set(params::liminality, macro);
            rig->render(0.3);
            return toDb(rms(rig->render(11.0, fragilePhrase(rig->time + 0.2))));
        };
        const double clean = level(-1, 0.0f), mid = level(-1, 0.4f) - clean, full = level(-1, 1.0f) - clean;
        s.check(std::abs(mid) < 1.5 && std::abs(full) < 1.5, "the dry piano phrase keeps its loudness at LIMINALITY 40 % and 100 % (+/- 1.5 dB)",
                num(mid, 2) + " / " + num(full, 2) + " dB");
        std::string rooms;
        bool within = true;
        for (const int place : { 2, 5, 9 }) {
            const double base = level(place, 0.0f), at40 = level(place, 0.4f) - base, at100 = level(place, 1.0f) - base;
            rooms += std::string(placeNames[place]) + " " + num(at40, 1) + "/" + num(at100, 1) + " ";
            within = within && std::abs(at40) < 3.0 && std::abs(at100) < 3.0;
        }
        s.report(within, "in the school, hotel and impossible room too (+/- 3 dB at 40 / 100 %)", rooms + "dB");
    }

    s.section("liminality: determinism, clicks, real time, CPU");
    {   // two instances, all six instruments, macro 100 %
        auto a = dryRig(), b = dryRig();
        for (auto* r : { a.get(), b.get() }) {
            allInstruments(*r);
            r->set(params::liminality, 1.0f);
            renderExact(*r, 0.1, {}, 512);
        }
        const auto x = renderExact(*a, 9.0, fragilePhrase(a->time), 512), y = renderExact(*b, 9.0, fragilePhrase(b->time), 512);
        s.check(x == y, "LIMINALITY 100 %, all six instruments: two instances render identical output");
        auto c = dryRig(rate, 97);
        allInstruments(*c);
        c->set(params::liminality, 1.0f);
        renderExact(*c, 0.1, {}, 97);
        const auto z = renderExact(*c, 9.0, fragilePhrase(c->time), 97);
        double worst = 0;
        for (std::size_t i = 0; i < std::min(x.first.size(), z.first.size()); ++i) worst = std::max(worst, double(std::abs(x.first[i] - z.first[i])));
        s.check(worst < 1.0e-5, "... and the host's block size changes nothing (512 vs 97)", "max difference " + num(worst, 8));
    }
    {   // a replay of the same passage of the song (the host's timeline) gives the same notes
        const auto pass = [&](Rig& rig, Head& head, juce::int64 from, double seconds, bool withNotes) {
            head.samples = from;
            std::vector<float> out;
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            const auto events = fragilePhrase(0.05);
            for (double t = 0; t < seconds; t += 512.0 / rate) {
                buffer.clear();
                midi.clear();
                if (withNotes)
                    for (const auto& e : events)
                        if (e.at >= t && e.at < t + 512.0 / rate) midi.addEvent(e.message, int((e.at - t) * rate));
                rig.p->processBlock(buffer, midi);
                for (int i = 0; i < 512; ++i) out.push_back(buffer.getSample(0, i));
                head.samples += 512;
            }
            return out;
        };
        auto rig = dryRig();
        Head head;
        rig->p->setPlayHead(&head);
        allInstruments(*rig);
        rig->set(params::liminality, 1.0f);
        rig->render(0.1);
        pass(*rig, head, 0, 4.0, false);
        const auto first = pass(*rig, head, 480000, 9.5, true);
        pass(*rig, head, 96000, 12.0, false);   // somewhere else in the song, in silence (tails gone)
        const auto second = pass(*rig, head, 480000, 9.5, true);
        double diff = 0, sig = 0;
        for (std::size_t i = 0; i < first.size(); ++i) {
            diff += double(first[i] - second[i]) * (first[i] - second[i]);
            sig += double(first[i]) * first[i];
        }
        const double db = 10.0 * std::log10(diff / std::max(1e-20, sig) + 1e-30);
        s.check(db < -60.0, "playing the same passage again (host timeline, six instruments): the same detuned notes, drift and wow", num(db, 1) + " dB difference");
        rig->p->setPlayHead(nullptr);
    }
    {   // clicks: every control jumped abruptly while a tone sounds (the stage alone)
        const auto jumpy = [&](bool withWear) {
            Stage st(dsp::LiminalAmounts {});
            auto l = sine(220, 12.0, 0.3), r = sine(330, 12.0, 0.3);
            std::uint32_t h = 12345;
            st.run(l, r, [&](std::uint64_t t) {
                if (t % 6400 == 0) {   // every 133 ms: every amount jumps to a new random value
                    dsp::LiminalAmounts a {};
                    for (int k = 0; k < lim::count; ++k) {
                        h = h * 1664525u + 1013904223u;
                        a[std::size_t(k)] = (k == lim::wear && !withWear) ? 1.0f : float(h >> 8) / 16777216.0f * lim::maxEffective;
                    }
                    st.lim.setAmounts(a);
                }
                if (t % 9600 == 0) st.lim.noteOn(st.probe, 60, 0.8f, t, t);
            });
            return clicks(l, rate, std::size_t(0.2 * rate)) + clicks(r, rate, std::size_t(0.2 * rate));
        };
        const auto steadyWear = [&] {   // the same, wear held at 1.0 (its crackle is the only tick)
            Stage st(only(lim::wear, 1.0f));
            auto l = sine(220, 12.0, 0.3), r = sine(330, 12.0, 0.3);
            st.run(l, r, [&](std::uint64_t t) { if (t % 9600 == 0) st.lim.noteOn(st.probe, 60, 0.8f, t, t); });
            return clicks(l, rate, std::size_t(0.2 * rate)) + clicks(r, rate, std::size_t(0.2 * rate));
        };
        const int jumps = jumpy(false), crackles = steadyWear();
        s.check(jumps <= crackles + 2, "every control jumping every 133 ms: no clicks beyond Wear's own faint crackle",
                std::to_string(jumps) + " vs " + std::to_string(crackles) + " with Wear steady");
        // the whole plug-in: the macro swept up and down while a piano chord rings
        auto rig = dryRig();
        allInstruments(*rig);
        allLim(*rig, 0.4f);
        for (int i = 0; i < params::instrumentCount; ++i) setLim(*rig, i, lim::wear, 0.0f);
        rig->render(0.1);
        std::vector<Event> chord { { rig->time + 0.01, on(57, 70) }, { rig->time + 0.01, on(64, 70) }, { rig->time + 0.01, pedal(true) } };
        rig->render(0.3, chord);
        std::vector<float> outL;
        for (int k = 0; k < 40; ++k) {
            rig->set(params::liminality, k % 2 == 0 ? 1.0f : 0.0f);
            const auto x = rig->render(0.1);
            outL.insert(outL.end(), x.first.begin(), x.first.end());
        }
        s.check(clicks(outL, rate, 0) == 0, "LIMINALITY jumped 0 <-> 100 % every 100 ms over a held chord (six instruments): no clicks",
                std::to_string(clicks(outL, rate, 0)) + " found");
    }
    {   // real time and CPU: all six instruments at 100 %
        const auto measure = [&](float macro, std::size_t& allocations) {
            auto rig = dryRig();
            allInstruments(*rig);
            rig->set(params::ambience, 0.0f);
            rig->set(params::space, 0.35f);
            rig->set(params::environmentAmount, 1.0f);
            rig->set(params::liminality, macro);
            rig->render(0.5);
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            for (int i = 0; i < 16; ++i) midi.addEvent(on(36 + 3 * i, 90), 10 + i);
            midi.addEvent(pedal(true), 30);
            allocationProbe::calls = 0;
            allocationProbe::enabled = true;
            rig->p->processBlock(buffer, midi);
            juce::MidiBuffer none;
            const auto start = std::chrono::steady_clock::now();
            const int blocks = int(10.0 * rate / 512.0);
            for (int i = 0; i < blocks; ++i) {
                if (i % 40 == 20) {   // notes keep coming (held back by Timing, through the queue)
                    juce::MidiBuffer more;
                    more.addEvent(on(60 + i % 12, 80), 5);
                    more.addEvent(off(60 + (i + 6) % 12), 200);
                    rig->p->processBlock(buffer, more);
                } else {
                    rig->p->processBlock(buffer, none);
                }
            }
            allocationProbe::enabled = false;
            allocations = allocationProbe::calls;
            return 100.0 * std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / 10.0;
        };
        std::size_t a0 = 0, a1 = 0;
        const double clean = measure(0.0f, a0), full = measure(1.0f, a1);
        s.check(a1 == 0, "LIMINALITY 100 %, six instruments, notes through the timing queue: processBlock never allocates", std::to_string(a1) + " allocations");
        s.metric("CPU, six instruments x 16 notes, LIMINALITY 0 %", clean, " % of one core");
        s.metric("CPU, six instruments x 16 notes, LIMINALITY 100 %", full, " % of one core");
        s.check(full - clean < 6.0, "Liminality is light: all six stages at 100 % add under 6 % of one core", num(clean, 1) + " -> " + num(full, 1) + " %");
    }
}

// The listening render: the same fragile piano phrase at LIMINALITY 0 %, 40 % and 100 %, in the dry
// room and in three rooms (school, hotel, impossible room), with a cue sheet beside it.
int renderLiminality(const juce::String& path) {
    using namespace limtest;
    const double rate = 48000.0;
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    struct Room { const char* name; int place; };
    const Room rooms[] { { "dry", -1 }, { "school", 2 }, { "hotel", 5 }, { "impossible room", 9 } };
    std::vector<float> outL, outR;
    juce::String cues = "LIMINAL - Liminality listening render: one fragile piano phrase at LIMINALITY 0 %, 40 %, 100 %\n"
                        "(dry: no room, no ambience; rooms: their default Character, ambience at -12 dB)\n\n";
    for (const auto& room : rooms)
        for (const float macro : { 0.0f, 0.4f, 1.0f }) {
            std::unique_ptr<Rig> rig;
            if (room.place < 0) {
                rig = dryRig();
            } else {
                rig = rigIn(room.place);
                rig->set(params::ambience, -12.0f);
            }
            rig->set(params::liminality, macro);
            rig->render(0.3);
            const double at = double(outL.size()) / rate;
            const auto x = rig->render(11.0, fragilePhrase(rig->time + 0.2));
            outL.insert(outL.end(), x.first.begin(), x.first.end());
            outR.insert(outR.end(), x.second.begin(), x.second.end());
            outL.insert(outL.end(), std::size_t(rate * 1.0), 0.0f);
            outR.insert(outR.end(), std::size_t(rate * 1.0), 0.0f);
            const int m = int(at) / 60, sec = int(at) % 60;
            cues << juce::String(m) << ":" << juce::String(sec).paddedLeft('0', 2) << "  " << room.name << ", LIMINALITY " << juce::roundToInt(macro * 100) << " %\n";
        }
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(new juce::FileOutputStream(file));
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), rate, 2, 24, {}, 0));
    if (writer == nullptr) return 1;
    stream.release();
    const float* channels[] { outL.data(), outR.data() };
    writer->writeFromFloatArrays(channels, 2, int(outL.size()));
    writer.reset();
    file.withFileExtension("txt").replaceWithText(cues);
    std::cout << cues << "wrote " << file.getFullPathName() << "\n";
    return 0;
}
