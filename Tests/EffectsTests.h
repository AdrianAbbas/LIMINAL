// Milestone 3: per-instrument effects with environment defaults and Environment Influence.
// Included by LiminalTests.cpp after EnvironmentTests.h.

void setFx(Rig& rig, int instrument, const char* name, float value) {
    auto* p = rig.p->parameters.getParameter(params::fxId(instrument, name));
    p->setValueNotifyingHost(p->convertTo0to1(value));
}

// A rig in `place` with only the player's own effects acting (Influence 0), no ambience, no room.
std::unique_ptr<Rig> fxRig(int place, float influence = 0.0f, float space = 0.0f) {
    auto rig = rigIn(place);
    rig->set(params::ambience, params::minDb);
    rig->set(params::space, space);
    setFx(*rig, 0, params::fx::influence, influence);
    rig->render(0.3);
    return rig;
}

double maxDiff(const Signal& a, const Signal& b) {
    double d = 0;
    for (std::size_t i = 0; i < std::min(a.first.size(), b.first.size()); ++i)
        d = std::max({ d, double(std::abs(a.first[i] - b.first[i])), double(std::abs(a.second[i] - b.second[i])) });
    return d;
}

void testEffects(Suite& s) {
    s.section("effects (Milestone 3)");
    const double rate = 48000.0;
    {
        LiminalProcessor p;
        int found = 0;
        for (int i = 0; i < params::instrumentCount; ++i)
            for (const char* n : params::fx::all) found += p.parameters.getParameter(params::fxId(i, n)) != nullptr ? 1 : 0;
        s.check(found == params::instrumentCount * params::fx::count, "every instrument has its own effects (" + std::to_string(params::fx::count) + " parameters each)",
                std::to_string(found));
    }
    const auto phrase = [&](Rig& rig) { return rig.render(7.0, standardPhrase(rig.time)); };

    // 1. Transparent when nothing asks for an effect.
    {
        auto a = fxRig(1);   // the mall, Influence 0
        auto b = rigIn(1);
        b->set(params::ambience, params::minDb);
        b->set(params::space, 0.0f);
        b->set(params::environmentAmount, 0.0f);
        b->render(0.3);
        s.check(maxDiff(phrase(*a), phrase(*b)) < 1.0e-6, "Influence 0 with no effects: the instrument exactly as it was (= Environment Amount 0)");
        auto c = rigIn(1);
        c->set(params::ambience, params::minDb);
        c->set(params::space, 0.0f);
        for (const char* n : { params::fx::drift, params::fx::tape, params::fx::echo, params::fx::chorus, params::fx::grain }) setFx(*c, 0, n, 1.0f);
        setFx(*c, 0, params::fx::mix, 0.0f);
        c->render(0.3);
        s.check(maxDiff(phrase(*c), phrase(*fxRig(1))) < 1.0e-6, "Effects Mix 0: the dry instrument, even with every effect and the place at full");
    }

    // 2. Every module is audible and does what it says (piano, in the hospital, Influence 0).
    const auto with = [&](std::initializer_list<std::pair<const char*, float>> settings, float space = 0.0f) {
        auto rig = fxRig(0, 0.0f, space);
        for (const auto& [n, v] : settings) setFx(*rig, 0, n, v);
        rig->render(0.1);
        return phrase(*rig);
    };
    const Signal reference = with({});
    const Character refChar = characterOf(reference, rate);
    {
        const auto x = with({ { params::fx::drift, 1.0f }, { params::fx::driftRate, 0.6f } });
        s.metric("drift 100 %: character distance from no effects", characterDistance(characterOf(x, rate), refChar));
        // Pitch moves: a held synth note.
        const auto heldPitch = [&](float drift) {
            auto rig = fxRig(0);
            solo(*rig, params::synthOn);
            rig->set(params::environmentAmount, 1.0f);
            setFx(*rig, 1, params::fx::influence, 0.0f);
            rig->set(params::synthDrift, 0.0f);
            setFx(*rig, 1, params::fx::drift, drift);
            setFx(*rig, 1, params::fx::driftRate, 0.7f);
            rig->render(0.2);
            const auto note = playNote(*rig, 69, 100, 3.0, 0.0);
            double lo = 1e9, hi = -1e9;
            for (double t = 0.3; t + 0.2 < 3.0; t += 0.2) {
                const double c = centsBetween(pitchAt(note, rate, t, t + 0.2), 440.0);
                lo = std::min(lo, c);
                hi = std::max(hi, c);
            }
            return hi - lo;
        };
        const double still = heldPitch(0.0f), moving = heldPitch(1.0f);
        s.check(moving > 6.0 && still < 3.0, "Drift: the pitch wanders around the note", num(still, 1) + " -> " + num(moving, 1) + " cents range");
    }
    {
        const auto x = with({ { params::fx::tape, 1.0f }, { params::fx::tapeTone, 0.0f }, { params::fx::tapeHiss, 1.0f } });
        const Spectrum a(monoOf(reference, rate, 0.0, 4.0), rate), b(monoOf(x, rate, 0.0, 4.0), rate);
        const double loss = (a.bandDb(3000, 12000) - a.bandDb(100, 1000)) - (b.bandDb(3000, 12000) - b.bandDb(100, 1000));
        s.check(loss > 6.0, "Tape: the top end goes (bandwidth reduction)", num(loss) + " dB less 3-12 kHz relative to the body");
        s.metric("tape 100 %: character distance", characterDistance(characterOf(x, rate), refChar));
        auto quiet = fxRig(0);
        setFx(*quiet, 0, params::fx::tape, 1.0f);
        quiet->render(2.0);
        s.check(rms(quiet->render(1.0)) < 1.0e-6, "Tape hiss stops when nothing plays (silence stays silent)");
    }
    {
        const auto x = with({ { params::fx::filterLow, 900.0f }, { params::fx::filterRes, 0.6f } });
        const Spectrum a(monoOf(reference, rate, 0.0, 4.0), rate), b(monoOf(x, rate, 0.0, 4.0), rate);
        const double cut = a.bandDb(2500, 10000) - b.bandDb(2500, 10000);
        s.check(cut > 15.0, "Filter: low-pass at 900 Hz takes the top off", num(cut) + " dB less above 2.5 kHz");
        const auto y = with({ { params::fx::filterHigh, 800.0f } });
        const Spectrum c(monoOf(y, rate, 0.0, 4.0), rate);
        s.check(a.bandDb(60, 300) - c.bandDb(60, 300) > 12.0, "Filter: high-pass at 800 Hz takes the bottom off", num(a.bandDb(60, 300) - c.bandDb(60, 300)) + " dB");
    }
    {
        const auto x = with({ { params::fx::chorus, 1.0f } });
        const double before = correlation(reference, rate, 0.0, 6.6), after = correlation(x, rate, 0.0, 6.6);
        s.check(after < before - 0.1, "Chorus: wider (L/R correlation falls)", num(before, 2) + " -> " + num(after, 2));
    }
    {
        const auto x = with({ { params::fx::echo, 1.0f }, { params::fx::echoTime, 300.0f }, { params::fx::echoFeedback, 0.5f } });
        const double t0 = windowDb(reference, rate, 5.6, 6.6), t1 = windowDb(x, rate, 5.6, 6.6);
        s.check(t1 > t0 + 8.0, "Echo: repeats after the notes stop", num(t0) + " -> " + num(t1) + " dBFS a second after the chord");
        // Tempo sync: 1/4 at the default 120 BPM is 500 ms.
        auto rig = fxRig(0);
        setFx(*rig, 0, params::fx::echo, 1.0f);
        setFx(*rig, 0, params::fx::echoSync, 4.0f);
        setFx(*rig, 0, params::fx::echoFeedback, 0.0f);
        rig->render(0.2);
        const auto hit = rig->render(1.2, { { rig->time + 0.01, on(84, 127) }, { rig->time + 0.04, off(84) } });
        double best = -200, bestAt = 0;
        for (double t = 0.25; t < 1.0; t += 0.005) {
            const double d = windowDb(hit, rate, t, t + 0.01);
            if (d > best) { best = d; bestAt = t; }
        }
        s.check(std::abs(bestAt - 0.51) < 0.03, "Echo sync 1/4 at 120 BPM: the repeat arrives 500 ms later", num(bestAt * 1000.0, 0) + " ms after the note (+10 ms offset)");
    }
    {
        const auto x = with({ { params::fx::grain, 1.0f }, { params::fx::grainSpread, 0.8f } });
        const double d = characterDistance(characterOf(x, rate), refChar);
        s.check(d > 1.0 && finite(x), "Granular: smears and fragments the phrase", num(d, 2));
        auto frozen = fxRig(0);
        setFx(*frozen, 0, params::fx::grain, 1.0f);
        frozen->render(0.1);
        frozen->render(0.8, { { frozen->time + 0.05, on(60, 110) } });
        setFx(*frozen, 0, params::fx::grainFreeze, 1.0f);   // catch it while it sounds
        frozen->render(1.2, { { frozen->time + 0.25, off(60) } });
        const double held = toDb(rms(frozen->render(1.0)));
        auto loose = fxRig(0);
        setFx(*loose, 0, params::fx::grain, 1.0f);
        loose->render(0.1);
        loose->render(0.8, { { loose->time + 0.05, on(60, 110) } });
        loose->render(1.2, { { loose->time + 0.25, off(60) } });
        const double gone = toDb(rms(loose->render(1.0)));
        s.check(held > gone + 6.0, "Granular Freeze: the frozen sound keeps playing after the note", num(gone) + " -> " + num(held) + " dBFS");
    }
    {
        const auto dryRoom = with({ { params::fx::reverb, 0.0f } }, 0.6f), wetRoom = with({ { params::fx::reverb, 1.0f } }, 0.6f);
        const double t0 = windowDb(dryRoom, rate, 5.6, 6.6), t1 = windowDb(wetRoom, rate, 5.6, 6.6);
        s.check(t1 > t0 + 10.0, "Reverb: the place's room for this instrument", num(t0) + " -> " + num(t1) + " dBFS a second after the chord");
    }
    {   // Toggles: a module switched off with its amount at full does nothing at all.
        const auto x = with({ { params::fx::echo, 1.0f }, { params::fx::echoOn, 0.0f }, { params::fx::chorus, 1.0f }, { params::fx::chorusOn, 0.0f },
                              { params::fx::grain, 1.0f }, { params::fx::grainOn, 0.0f }, { params::fx::drift, 1.0f }, { params::fx::driftOn, 0.0f },
                              { params::fx::tape, 1.0f }, { params::fx::tapeOn, 0.0f }, { params::fx::filterLow, 500.0f }, { params::fx::filterOn, 0.0f } });
        s.check(maxDiff(x, reference) < 1.0e-6, "bypassing every module restores the dry instrument exactly");
    }
    {   // Order changes the result (granular before or after the echo).
        const auto a = with({ { params::fx::grain, 1.0f }, { params::fx::echo, 0.8f }, { params::fx::order, 0.0f } });
        const auto b = with({ { params::fx::grain, 1.0f }, { params::fx::echo, 0.8f }, { params::fx::order, 2.0f } });
        s.check(maxDiff(a, b) > 1.0e-3, "Effect Order changes the result (echo into granular vs granular into echo)");
    }

    // 3. Environment Influence and the places' defaults.
    {
        auto none = fxRig(1, 0.0f), half = fxRig(1, 0.5f), full = fxRig(1, 1.0f);
        const auto c0 = characterOf(phrase(*none), rate), c5 = characterOf(phrase(*half), rate), c1 = characterOf(phrase(*full), rate);
        const double whole = characterDistance(c0, c1), part = characterDistance(c0, c5);
        s.check(whole > 2.0, "Influence 0 -> 100 % in the mall changes the piano clearly", num(whole, 2));
        s.check(part > 0.2 * whole && part < whole, "Influence 50 % sits in between", num(part, 2) + " of " + num(whole, 2));
    }
    {
        double nearestPlaces = 1e9, nearestInstruments = 1e9;
        const auto vec = [](const dsp::PlaceEffects& e) {
            return std::vector<double> { e.drift, e.tape, e.chorus, e.grain, e.echo, e.echoMs / 1000.0, e.echoFeedback, e.lowOctaves / 2.0, e.highOctaves / 2.0, e.reverbBias / 2.0 };
        };
        const auto dist = [&](const dsp::PlaceEffects& a, const dsp::PlaceEffects& b) {
            const auto x = vec(a), y = vec(b);
            double d = 0;
            for (std::size_t i = 0; i < x.size(); ++i) d += (x[i] - y[i]) * (x[i] - y[i]);
            return std::sqrt(d);
        };
        for (int i = 0; i < params::instrumentCount; ++i)
            for (int a = 0; a < dsp::placeCount; ++a) {
                for (int b = a + 1; b < dsp::placeCount; ++b) nearestPlaces = std::min(nearestPlaces, dist(dsp::placeEffects(dsp::Place(a), i), dsp::placeEffects(dsp::Place(b), i)));
                for (int j = i + 1; j < params::instrumentCount; ++j)
                    nearestInstruments = std::min(nearestInstruments, dist(dsp::placeEffects(dsp::Place(a), i), dsp::placeEffects(dsp::Place(a), j)));
            }
        s.check(nearestPlaces > 0.15, "every place has its own default effects for every instrument", "closest " + num(nearestPlaces, 2));
        s.check(nearestInstruments > 0.02, "within a place, each instrument gets its own defaults", "closest " + num(nearestInstruments, 3));
    }
    {   // Independent: the piano's effects never touch the synth.
        const auto synthAlone = [&](bool pianoExtreme) {
            auto rig = fxRig(0);
            rig->set(params::synthOn, 1.0f);
            rig->set(params::solo, 2.0f);   // the piano keeps running, muted
            if (pianoExtreme)
                for (const char* n : { params::fx::drift, params::fx::tape, params::fx::echo, params::fx::chorus, params::fx::grain }) setFx(*rig, 0, n, 1.0f);
            rig->render(0.5);   // the muted piano's level has faded to nothing
            return phrase(*rig);
        };
        const double d = maxDiff(synthAlone(false), synthAlone(true));
        s.check(d < 1.0e-6, "each instrument's effects are its own (the piano's leave the synth untouched)", num(d * 1e6, 3) + " e-6");
    }

    // 4. Changing place with heavy effects: smooth, stable, no allocation.
    {
        auto rig = rigIn(0);
        for (int i = 0; i < params::instrumentCount; ++i) {
            setFx(*rig, i, params::fx::echo, 1.0f);
            setFx(*rig, i, params::fx::echoFeedback, 0.95f);
            setFx(*rig, i, params::fx::grain, 0.8f);
            setFx(*rig, i, params::fx::drift, 1.0f);
            setFx(*rig, i, params::fx::tape, 1.0f);
            setFx(*rig, i, params::fx::tapeDropouts, 1.0f);
        }
        rig->set(params::synthOn, 1.0f);
        juce::AudioBuffer<float> buffer(2, 512);
        Signal out;
        allocationProbe::calls = 0;
        std::vector<Event> events;
        for (int i = 0; i < 14; ++i) {
            events.push_back({ 0.3 * i + 0.1, on(55 + (i * 7) % 15, 100) });
            events.push_back({ 0.3 * i + 0.35, off(55 + (i * 7) % 15) });
        }
        for (int block = 0; block < int(20.0 * rate / 512); ++block) {
            juce::MidiBuffer midi;
            const double start = rig->time, end = start + 512.0 / rate;
            for (const auto& e : events)
                if (e.at >= start && e.at < end) midi.addEvent(e.message, int((e.at - start) * rate));
            if (block % 37 == 10 && block < int(6.0 * rate / 512)) setPlace(*rig, (block / 37 + 3) % dsp::placeCount);
            allocationProbe::enabled = true;
            rig->p->processBlock(buffer, midi);
            allocationProbe::enabled = false;
            rig->time = end;
            for (int i = 0; i < 512; ++i) {
                out.first.push_back(buffer.getSample(0, i));
                out.second.push_back(buffer.getSample(1, i));
            }
        }
        double worstStep = 0, peakAll = 0;
        for (std::size_t i = 1; i < out.first.size(); ++i) {
            worstStep = std::max(worstStep, double(std::abs(out.first[i] - out.first[i - 1])));
            peakAll = std::max(peakAll, double(std::abs(out.first[i])));
        }
        s.check(allocationProbe::calls == 0, "changing place with heavy effects never allocates", std::to_string(allocationProbe::calls));
        s.check(finite(out) && peakAll < 1.0, "echo feedback at 95 % on every instrument stays bounded over 20 s", num(peakAll, 2));
        s.check(worstStep < 0.5 * peakAll, "changing place with heavy effects is click-free (largest step under half the peak)", num(worstStep, 3) + " vs " + num(peakAll, 3));
    }
    {   // State: every effect setting comes back.
        LiminalProcessor a;
        a.parameters.getParameter(params::fxId(3, params::fx::echoTime))->setValueNotifyingHost(0.77f);
        a.parameters.getParameter(params::fxId(5, params::fx::grainFreeze))->setValueNotifyingHost(1.0f);
        a.parameters.getParameter(params::fxId(0, params::fx::influence))->setValueNotifyingHost(0.25f);
        juce::MemoryBlock block;
        a.getStateInformation(block);
        LiminalProcessor b;
        b.setStateInformation(block.getData(), int(block.getSize()));
        bool same = true;
        for (int i = 0; i < params::instrumentCount; ++i)
            for (const char* n : params::fx::all)
                same &= std::abs(a.parameters.getParameter(params::fxId(i, n))->getValue() - b.parameters.getParameter(params::fxId(i, n))->getValue()) < 1.0e-6f;
        s.check(same, "every effect setting is saved and restored");
    }
    {   // Cost: all six instruments, every effect on, 16 notes.
        Rig rig(rate);
        for (const char* id : { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn }) rig.set(id, 1.0f);
        for (int i = 0; i < params::instrumentCount; ++i)
            for (const char* n : { params::fx::drift, params::fx::tape, params::fx::echo, params::fx::chorus, params::fx::grain }) setFx(rig, i, n, 0.6f);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int note = 48; note < 64; ++note) midi.addEvent(on(note, 100), 0);
        rig.p->processBlock(buffer, midi);
        juce::MidiBuffer none;
        const auto start = std::chrono::steady_clock::now();
        const int blocks = int(5.0 * rate / 512);
        for (int i = 0; i < blocks; ++i) rig.p->processBlock(buffer, none);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        s.metric("CPU, 16 notes on all six instruments with every effect on, % of one core (report-only)", 100.0 * seconds / 5.0, " %");
    }
}
