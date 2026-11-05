// Hung-note stress test (2026-11-05, group `hung`). Included by LiminalTests.cpp after
// LibraryTests.h. The developer heard, in GarageBand, Faded Synth (and sometimes Faded Strings and
// Hallway Pizzicato) "hanging on random notes and glitching" in a played pattern. Every one of the
// twenty instruments, at LIMINALITY 0, 40 and 100 %, gets thousands of random and fast notes driven
// like a host: random block sizes, a playing timeline with cycle jumps, note-ons and note-offs at the
// same sample in both orders, overlapping notes of the same key, velocity-0 note-ons, notes far
// shorter than Liminality's Timing delay, staccato, chords, the sustain pedal going up and down.
// Then everything is let go and the instrument is left in silence: no voice may still be sounding,
// the output must die away, a note played afterwards must be in tune, and nothing may click.

namespace stresstest {

constexpr double rate = 48000.0;

// The recorded group's click detector, outside the first 12 ms of each note (a struck or plucked
// note's own onset is sharp on purpose: what is looked for is a step anywhere else).
int clicksBetweenOnsets(const Signal& x, double sampleRate, const std::vector<double>& onsets, double from = 0.0) {
    int count = 0;
    std::vector<char> skip(x.first.size(), 0);
    for (const double t : onsets)
        for (auto i = std::size_t(std::max(0.0, (t - 0.002) * sampleRate)); i < std::min(skip.size(), std::size_t((t + 0.012) * sampleRate)); ++i) skip[i] = 1;
    for (const auto* ch : { &x.first, &x.second }) {
        const auto& v = *ch;
        std::vector<double> d(v.size(), 0.0);
        for (std::size_t i = 2; i < v.size(); ++i) d[i] = double(v[i]) - 2.0 * v[i - 1] + v[i - 2];
        const auto half = std::size_t(0.005 * sampleRate);
        double sum = 0;
        std::size_t lo = 0, hi = 0;
        for (std::size_t i = std::max(std::size_t(from * sampleRate), half); i + half < v.size(); ++i) {
            while (hi < i + half) { sum += d[hi] * d[hi]; ++hi; }
            while (lo < i - half) { sum -= d[lo] * d[lo]; ++lo; }
            const double local = std::sqrt(std::max(0.0, sum) / double(hi - lo));
            if (!skip[i] && std::abs(d[i]) > 8.0 * local + 1.0e-7 && std::abs(d[i]) > 1.0e-4) {
                if (std::getenv("LIMINAL_PRINT_CLICKS") != nullptr) std::cout << "    click at " << double(i) / sampleRate << " s, step " << d[i] << ", local " << local << "\n";
                ++count;
                i += half;
            }
        }
    }
    return count;
}

// GarageBand-like timeline: playing, advancing block by block, jumping back at a cycle.
struct Head final : juce::AudioPlayHead {
    juce::int64 samples = 0;
    bool playing = true;
    int jitter = 0;   // the reported position wavers by up to this many samples (a host's rounding)
    mutable juce::Random wobble { 7 };
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setTimeInSamples(samples + (jitter > 0 ? juce::int64(wobble.nextInt(2 * jitter + 1) - jitter) : 0));
        info.setBpm(120.0);
        return info;
    }
};

// Renders `seconds` with random block sizes (32..1024) and the head advancing; events at absolute
// times since the rig started (the rig's own clock, rig.time, moves on too).
Signal renderHost(Rig& rig, Head& head, double seconds, const std::vector<Event>& events, juce::Random& random, double cycle = 0.0) {
    Signal out;
    const auto total = std::size_t(seconds * rate);
    out.first.reserve(total);
    out.second.reserve(total);
    juce::AudioBuffer<float> buffer(2, 1024);
    juce::MidiBuffer midi;
    std::size_t done = 0, next = 0;
    // Events sorted by time (the vector may not be).
    std::vector<std::size_t> order(events.size());
    std::iota(order.begin(), order.end(), std::size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return events[a].at < events[b].at; });
    double sinceCycle = 0;
    while (done < total) {
        const int n = int(std::min<std::size_t>(std::size_t(32 + random.nextInt(993)), total - done));
        buffer.setSize(2, n, false, false, true);
        midi.clear();
        const double start = rig.time, end = rig.time + double(n) / rate;
        while (next < order.size() && events[order[next]].at < end) {
            const auto& e = events[order[next]];
            midi.addEvent(e.message, std::clamp(int((e.at - start) * rate), 0, n - 1));
            ++next;
        }
        rig.p->processBlock(buffer, midi);
        for (int i = 0; i < n; ++i) {
            out.first.push_back(buffer.getSample(0, i));
            out.second.push_back(buffer.getSample(1, i));
        }
        done += std::size_t(n);
        rig.time = end;
        head.samples += n;
        sinceCycle += double(n) / rate;
        if (cycle > 0.0 && sinceCycle >= cycle) {   // the host's cycle: the timeline jumps back
            head.samples = juce::int64(rate * 0.5);
            sinceCycle = 0;
        }
    }
    return out;
}

// Thousands of notes in `seconds` from t0 between keys lo..hi; every note gets its note-off (some at
// the very sample of a note-on), the pedal ends up; returns the time of the last event.
double storm(std::vector<Event>& e, double t0, double seconds, int lo, int hi, juce::Random& random) {
    const auto key = [&] { return lo + random.nextInt(hi - lo + 1); };
    const auto vel = [&] { return 20 + random.nextInt(107); };
    double last = t0;
    const auto add = [&](double at, const juce::MidiMessage& m) { e.push_back({ at, m }); last = std::max(last, at); };
    // 1. A fast pattern: sixteenth-note arpeggios with repeated notes, staccato and legato (the
    //    developer's case), 12 notes a second for a third of the time.
    {
        const int base = std::clamp((lo + hi) / 2 - 7, lo, hi - 12);
        const int shape[] { 0, 4, 7, 12, 7, 4, 0, 0, 3, 7, 10, 7, 12, 12, 7, 3 };
        double t = t0;
        for (int i = 0; t < t0 + seconds / 3.0; ++i, t += 1.0 / 12.0) {
            const int note = base + shape[i % 16];
            const double length = (i / 16) % 3 == 0 ? 0.02 + 0.03 * random.nextDouble()            // staccato
                                : (i / 16) % 3 == 1 ? 1.0 / 12.0                                      // exactly to the next note
                                                    : 1.0 / 12.0 + 0.04 * random.nextDouble();        // legato (overlapping)
            add(t, on(note, vel()));
            add(t + length, off(note));
        }
    }
    // 2. Random notes: very short (shorter than Timing's delay) to long, overlaps of the same key,
    //    on and off at the same sample (both orders), velocity-0 note-ons as note-offs, chords.
    const double from = t0 + seconds / 3.0, to = t0 + seconds;
    for (int i = 0; i < 2600; ++i) {
        const double at = from + (to - from) * random.nextDouble();
        const int note = key();
        const double r = random.nextDouble();
        if (r < 0.25) {                       // very short: 1-15 ms
            add(at, on(note, vel()));
            add(at + 0.001 + 0.014 * random.nextDouble(), off(note));
        } else if (r < 0.40) {                // the same key twice, overlapping (on on off off)
            add(at, on(note, vel()));
            add(at + 0.01 + 0.05 * random.nextDouble(), on(note, vel()));
            add(at + 0.08 + 0.1 * random.nextDouble(), off(note));
            add(at + 0.2 + 0.1 * random.nextDouble(), off(note));
        } else if (r < 0.50) {                // note-off then note-on of the key at one sample (and the reverse)
            add(at, on(note, vel()));
            const double t = at + 0.05 + 0.2 * random.nextDouble();
            if (random.nextBool()) { add(t, off(note)); add(t, on(note, vel())); }
            else { add(t, on(note, vel())); add(t, off(note)); }
            add(t + 0.03 + 0.2 * random.nextDouble(), off(note));
        } else if (r < 0.58) {                // a velocity-0 note-on ends it
            add(at, on(note, vel()));
            add(at + 0.02 + 0.3 * random.nextDouble(), juce::MidiMessage::noteOn(1, note, juce::uint8(0)));
        } else if (r < 0.63) {                // a six-note chord
            for (int k = 0; k < 6; ++k) {
                const int n = std::clamp(note + k * (2 + random.nextInt(4)), lo, hi);
                add(at + 0.002 * random.nextDouble(), on(n, vel()));
                add(at + 0.1 + 1.0 * random.nextDouble(), off(n));
            }
        } else if (r < 0.70) {                // the pedal changes
            add(at, pedal(random.nextBool()));
        } else {                              // ordinary: 30 ms .. 1.5 s
            add(at, on(note, vel()));
            add(at + 0.03 + 1.47 * random.nextDouble() * random.nextDouble(), off(note));
        }
    }
    // The pedal up after everything.
    add(last + 0.01, pedal(false));
    return last;
}

struct Outcome {
    int voices = 0;          // still sounding after the silence
    int instrumentVoices = 0;
    double tailDb = 0;       // the last second of the silence
    int clicks = 0;          // in the storm and the release
    double cents = 0;        // a note afterwards against the same note in a fresh rig
    bool finiteOut = true;
};

// A played pattern as GarageBand plays one (the developer's case): 44.1 kHz, blocks of 1024, the
// timeline playing; two bars of sixteenths (repeated notes, staccato and full-length, the next
// note-on at the sample of the last note-off), then a rest of 0.6 s, again and again. At the end of
// every rest no voice may still be held by a key, and the level must have fallen.
struct Gaps { int heldInGap = 0, gaps = 0; double worstGapDb = -200, playedDb = 0; int clicks = 0; };
Gaps patternWithRests(int which, float amount, int cycles, double sampleRate = 44100.0, int block = 1024, int jitter = 0, Signal* keep = nullptr) {
    Gaps g;
    std::unique_ptr<Rig> rig;
    {
        auto r = std::make_unique<Rig>(sampleRate, block);
        rig.swap(r);
    }
    const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    const int slot = which < dsp::library::originals ? which : 1;
    solo(*rig, ons[slot]);
    rig->set(params::slotTypeId(slot).toRawUTF8(), float(which));
    rig->set(params::liminality, amount);
    rig->render(0.2);
    Head head;
    head.samples = 0;
    head.jitter = jitter;
    rig->p->setPlayHead(&head);
    const int shape[] { 0, 0, 7, 12, 3, 3, 7, 10, 0, 12, 7, 3, 0, 0, 5, 7 };
    const int roots[] { 57, 53, 60, 55 };
    double played = 0;
    int playedN = 0;
    for (int c = 0; c < cycles; ++c) {
        std::vector<Event> e;
        const double t0 = rig->time;
        const double step = 0.125;
        for (int i = 0; i < 32; ++i) {
            const int note = roots[(c + i / 16) % 4] + shape[i % 16] - (which == 17 ? 24 : 0);
            const double length = c % 3 == 0 ? 0.06 : step;   // staccato, or exactly to the next note
            e.push_back({ t0 + i * step, on(note, 80 + (i % 4 == 0 ? 30 : 0)) });
            e.push_back({ t0 + i * step + length, off(note) });
        }
        if (c % 4 == 1) {   // with the pedal over the second bar
            e.push_back({ t0 + 2.0, pedal(true) });
            e.push_back({ t0 + 3.98, pedal(false) });
        }
        // Block by block, as the host: the head moves with the audio.
        Signal x;
        const auto total = std::size_t((4.0 + 0.6) * sampleRate);
        juce::AudioBuffer<float> buffer(2, block);
        juce::MidiBuffer midi;
        std::size_t done = 0;
        while (done < total) {
            const int n = int(std::min<std::size_t>(std::size_t(block), total - done));
            buffer.setSize(2, n, false, false, true);
            midi.clear();
            const double start = rig->time, end = rig->time + double(n) / sampleRate;
            for (const auto& ev : e)
                if (ev.at >= start && ev.at < end) midi.addEvent(ev.message, std::clamp(int((ev.at - start) * sampleRate), 0, n - 1));
            rig->p->processBlock(buffer, midi);
            for (int i = 0; i < n; ++i) { x.first.push_back(buffer.getSample(0, i)); x.second.push_back(buffer.getSample(1, i)); }
            done += std::size_t(n);
            rig->time = end;
            head.samples += n;
        }
        ++g.gaps;
        g.heldInGap += rig->p->meters.held.load() != 0;
        played += std::pow(10.0, windowDb(x, sampleRate, 0.0, 4.0) / 10.0);
        ++playedN;
        g.worstGapDb = std::max(g.worstGapDb, windowDb(x, sampleRate, 4.45, 4.6));
        std::vector<double> onsets;
        for (const auto& ev : e)
            if (ev.message.isNoteOn()) onsets.push_back(ev.at - t0);
        if (amount > 0.0f)   // Liminality's Timing holds notes back by up to ~90 ms: their onsets move with it
            for (std::size_t k = 0, n = onsets.size(); k < n; ++k)
                for (double late = 0.01; late < 0.1; late += 0.01) onsets.push_back(onsets[k] + late);
        g.clicks += clicksBetweenOnsets(x, sampleRate, onsets);
        if (keep != nullptr) {
            keep->first.insert(keep->first.end(), x.first.begin(), x.first.end());
            keep->second.insert(keep->second.end(), x.second.begin(), x.second.end());
        }
    }
    rig->p->setPlayHead(nullptr);
    g.playedDb = 10.0 * std::log10(std::max(1e-30, played / std::max(1, playedN)));
    return g;
}

} // namespace stresstest

void testHungNotes(Suite& s) {
    s.section("hung notes (2026-11-05): thousands of fast notes on every instrument, then silence");
    using namespace stresstest;
    const bool ready = recordingsReady();
    if (!ready) s.report(false, "recordings present (the instruments play their stand-ins here)", samples::Bank::get().message().toStdString());
    int hung = 0, loud = 0, clicky = 0, wandering = 0;
    double worstTail = -200, worstCents = 0;
    std::string worstTailName, worstCentsName;
    const char* onlyWhich = std::getenv("LIMINAL_ONLY_INSTRUMENT");   // (development: one instrument)
    for (int which = 0; which < dsp::library::total; ++which) {
        if (onlyWhich != nullptr && std::atoi(onlyWhich) != which) continue;
        const std::string name = dsp::library::info(which).name;
        for (const float amount : { 0.0f, 0.4f, 1.0f }) {
            auto rig = librarytest::rigFor(which);
            rig->set(params::liminality, amount);
            if (which == 4) rig->set(params::boxMechanism, 0.0f);   // (the music box's clockwork ticks on purpose: Mechanism)
            rig->render(0.1);
            Head head;
            rig->p->setPlayHead(&head);
            juce::Random random(juce::int64(1000 + which * 17 + int(amount * 10)));
            const int lo = which == 17 ? 28 : 40, hi = which == 17 ? 64 : 84;   // (the bass lower)
            std::vector<Event> events;
            const double t0 = rig->time + 0.05;
            const double last = storm(events, t0, 12.0, lo, hi, random);
            const double startAt = rig->time;
            const auto played = renderHost(*rig, head, last - rig->time + 0.02, events, random, 4.0);
            // Silence: the longest releases here are a few seconds (chimes, bowed glass, the choir).
            const auto quiet = renderHost(*rig, head, 9.0, {}, random);
            if (std::getenv("LIMINAL_WRITE_STORM") != nullptr && amount == 0.0f) writeAudio(played, juce::File::getCurrentWorkingDirectory().getChildFile("probe-storm.wav"));
            head.playing = false;
            Outcome o;
            o.voices = rig->p->meters.voices.load();
            o.tailDb = windowDb(quiet, rate, 8.0, 9.0);
            o.finiteOut = finite(played) && finite(quiet);
            if (std::getenv("LIMINAL_PRINT_EVENTS") != nullptr && amount == 0.0f)
                for (const auto& ev : events) std::cout << "    event " << ev.at - startAt << " " << ev.message.getDescription() << "\n";
            std::vector<double> onsets;
            for (const auto& ev : events)
                if (ev.message.isNoteOn())
                    for (double late = 0.0; late <= (amount > 0.0f ? 0.1 : 0.0); late += 0.01) onsets.push_back(ev.at - startAt + late);
            o.clicks = clicksBetweenOnsets(played, rate, onsets, 0.05) + clicksIn(quiet, rate, 0.0);
            rig->p->setPlayHead(nullptr);
            const std::string tag = name + " at LIMINALITY " + std::to_string(int(amount * 100)) + " %";
            s.check(o.voices == 0, tag + ": no voice still sounding 9 s after the storm", std::to_string(o.voices) + " voices");
            s.check(o.finiteOut, tag + ": output finite and within +/-1");
            hung += o.voices != 0;
            if (o.tailDb > worstTail) { worstTail = o.tailDb; worstTailName = tag; }
            if (o.tailDb > -90.0) ++loud;
            s.check(o.tailDb < -90.0, tag + ": silence after the storm (last second)", juce::String(o.tailDb, 1).toStdString() + " dBFS");
            s.check(o.clicks == 0, tag + ": no clicks through the storm and the release", std::to_string(o.clicks) + " clicks");
            clicky += o.clicks != 0;
            // No wandering: a note afterwards in tune with the same note in a fresh rig (LIMINALITY 0
            // only: above it, the note's own detune and the timeline's drift differ on purpose).
            if (amount == 0.0f && which != 2 && which != 12 && which != 19) {   // (glass, chimes, bowed glass: inharmonic)
                const int note = which == 17 ? 45 : 69;
                const auto after = playNote(*rig, note, 100, 1.2, 0.6);
                auto fresh = librarytest::rigFor(which);
                fresh->set(params::liminality, 0.0f);
                fresh->render(0.1);
                const auto ref = playNote(*fresh, note, 100, 1.2, 0.6);
                const double fa = pitchAt(after, rate, 0.4, 1.1), fr = pitchAt(ref, rate, 0.4, 1.1);
                const double c = fa > 0 && fr > 0 ? centsBetween(fa, fr) : 0.0;
                s.check(std::abs(c) < 5.0, tag + ": a note after the storm is in tune with a fresh instance", juce::String(c, 2).toStdString() + " cents");
                wandering += std::abs(c) >= 5.0;
                if (std::abs(c) > std::abs(worstCents)) { worstCents = c; worstCentsName = tag; }
            }
        }
    }
    // The played pattern with rests, every instrument at LIMINALITY 0, 40 and 100 %, 16 times (75 s).
    int heldGaps = 0;
    for (int which = 0; which < dsp::library::total; ++which)
        if (onlyWhich == nullptr || std::atoi(onlyWhich) == which) for (const float amount : { 0.0f, 0.4f, 1.0f }) {
            const auto g = patternWithRests(which, amount, 16);
            const std::string tag = std::string(dsp::library::info(which).name) + " at LIMINALITY " + std::to_string(int(amount * 100)) + " %";
            s.check(g.heldInGap == 0, tag + ": a pattern with rests (44.1 kHz, 1024 blocks, timeline playing): no key-held voice in any rest",
                    std::to_string(g.heldInGap) + " of " + std::to_string(g.gaps) + " rests");
            s.check(g.clicks == 0, tag + ": the pattern does not click", std::to_string(g.clicks) + " clicks");
            std::cout << "  " << tag << ": played " << juce::String(g.playedDb, 1) << " dB, loudest rest end " << juce::String(g.worstGapDb, 1) << " dB\n";
            heldGaps += g.heldInGap;
        }
    s.metric("rests with a key-held voice", heldGaps);
    s.metric("instruments x LIMINALITY with a voice left", hung);
    s.metric("instruments x LIMINALITY not silent after 9 s", loud);
    s.metric("instruments x LIMINALITY with clicks", clicky);
    s.metric("notes out of tune after a storm (LIMINALITY 0)", wandering);
    std::cout << "  loudest tail: " << worstTailName << " " << juce::String(worstTail, 1) << " dBFS; worst pitch after a storm: "
              << worstCentsName << " " << juce::String(worstCents, 2) << " cents\n";
}

void testWaveringTimeline(Suite& s) {
    s.section("hung notes: a host timeline that wavers by a sample or two");
    using namespace stresstest;
    recordingsReady();
    // A host whose reported position wavers by a sample or two (GarageBand's timeline comes from its
    // beat position) is still playing straight on: the sound must be what it is with an exact one.
    for (int which = 0; which < dsp::library::total; ++which) {
        Signal exact, wavering;
        patternWithRests(which, 0.4f, 6, 44100.0, 1024, 0, &exact);
        const auto g = patternWithRests(which, 0.4f, 6, 44100.0, 1024, 2, &wavering);
        double diff = 0, ref = 0;
        for (std::size_t i = 0; i < exact.first.size(); ++i) {
            const double d = double(exact.first[i]) - wavering.first[i], e = double(exact.second[i]) - wavering.second[i];
            diff += d * d + e * e;
            ref += double(exact.first[i]) * exact.first[i] + double(exact.second[i]) * exact.second[i];
        }
        const double rel = 10.0 * std::log10(std::max(1e-30, diff) / std::max(1e-30, ref));
        const std::string name = dsp::library::info(which).name;
        s.check(rel < -60.0, name + ": a host timeline wavering by +/-2 samples changes nothing (difference to an exact timeline)",
                juce::String(rel, 1).toStdString() + " dB");
        s.check(g.clicks == 0, name + ": no clicks with the wavering timeline", std::to_string(g.clicks) + " clicks");
    }
}

// Probe (not a test): the processor's time per block over three minutes of a pattern at the product
// defaults (LIMINALITY 40 %, the default place, ambience), 44.1 kHz, blocks of 512.
int probeCpuPattern(int which) {
    using namespace stresstest;
    recordingsReady();
    const double sr = 44100.0;
    const int block = 512;
    std::unique_ptr<Rig> rig;
    {
        AtLiminalityDefault keep;
        rig = std::make_unique<Rig>(sr, block);
    }
    const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    const int slot = which < dsp::library::originals ? which : 1;
    for (int i = 0; i < params::instrumentCount; ++i) rig->set(ons[i], i == slot ? 1.0f : 0.0f);
    rig->set(params::slotTypeId(slot).toRawUTF8(), float(which));
    rig->render(0.5);
    const int shape[] { 0, 0, 7, 12, 3, 3, 7, 10, 0, 12, 7, 3, 0, 0, 5, 7 };
    std::vector<Event> e;
    for (int i = 0; i < 8 * 180; ++i) {
        const int note = 57 + shape[i % 16] + (i / 64 % 2) * 5;
        e.push_back({ rig->time + 0.1 + i * 0.125, on(note, 90) });
        e.push_back({ rig->time + 0.1 + i * 0.125 + 0.11, off(note) });
    }
    juce::AudioBuffer<float> buffer(2, block);
    juce::MidiBuffer midi;
    const double budget = double(block) / sr;
    double worst = 0, sum = 0;
    int blocks = 0;
    std::size_t next = 0;
    std::sort(e.begin(), e.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
    for (int minute = 0; minute < 6; ++minute) {
        double w = 0, s = 0;
        int n = 0, maxVoices = 0;
        for (int b = 0; b < int(30.0 * sr / block); ++b) {
            midi.clear();
            const double start = rig->time, end = start + budget;
            while (next < e.size() && e[next].at < end) { midi.addEvent(e[next].message, std::clamp(int((e[next].at - start) * sr), 0, block - 1)); ++next; }
            const auto t0 = std::chrono::steady_clock::now();
            rig->p->processBlock(buffer, midi);
            const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            rig->time = end;
            w = std::max(w, took);
            s += took;
            ++n;
            maxVoices = std::max(maxVoices, rig->p->meters.voices.load());
        }
        worst = std::max(worst, w);
        sum += s;
        blocks += n;
        std::cout << "  " << (minute + 1) * 30 << " s: mean " << juce::String(100.0 * s / n / budget, 1) << " % of real time, worst block "
                  << juce::String(100.0 * w / budget, 1) << " %, voices up to " << maxVoices << "\n";
    }
    std::cout << params::libraryNames[which] << ": mean " << juce::String(100.0 * sum / blocks / budget, 1) << " %, worst " << juce::String(100.0 * worst / budget, 1) << " %\n";
    return 0;
}

int probeClickPattern(int which) {
    recordingsReady();
    Signal x;
    const auto g = stresstest::patternWithRests(which, 0.0f, 3, 44100.0, 1024, 0, &x);
    std::cout << g.clicks << " clicks\n";
    writeAudio(x, juce::File::getCurrentWorkingDirectory().getChildFile("probe-pattern.wav"));
    return 0;
}
