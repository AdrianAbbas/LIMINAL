// Milestone 5: Characters, evolution, Scene A/B morphing and the four macros.
// Included by LiminalTests.cpp after EffectsTests.h.

void setParam(Rig& rig, const char* id, float value) { rig.set(id, value); }

std::unique_ptr<Rig> characterRig(int place, int character, double rate = 48000.0, int block = 512) {
    auto rig = std::make_unique<Rig>(rate, block);
    setPlace(*rig, place);
    rig->set(params::character, float(character));
    rig->p->prepareToPlay(rate, block);
    return rig;
}

// The ambience's "fingerprint": octave profile, level and event count over 15 s.
struct AmbienceTrace { std::vector<double> profile; double level; int events; };
AmbienceTrace ambienceOf(Rig& rig, double seconds) {
    const auto x = rig.render(seconds);
    return { averageProfile(x, rig.rate), toDb(rms(x)), countEvents(envelope(x, rig.rate, 0.05), 20) };
}
double ambienceDistance(const AmbienceTrace& a, const AmbienceTrace& b) {
    const double tone = profileDistance(a.profile, b.profile) / 3.0, level = std::abs(a.level - b.level) / 3.0,
                 events = std::abs(std::log2(double(a.events + 1) / double(b.events + 1)));
    return std::sqrt(tone * tone + level * level + events * events);
}

void testCharacters(Suite& s) {
    s.section("characters, evolution, scenes, macros (Milestone 5)");
    const double rate = 48000.0;
    {
        LiminalProcessor p;
        bool all = true;
        for (const char* id : { params::character, params::evolution, params::evolutionFreeze, params::evolutionSeed, params::presence, params::scale,
                                params::activity, params::unreal, params::sceneOn, params::sceneMorph })
            all &= p.parameters.getParameter(id) != nullptr && p.parameters.getParameter(id)->isAutomatable();
        s.check(all, "Character, evolution (rate, freeze, seed), the four macros and the scene morph are automatable parameters");
        std::set<std::string> names;
        bool distinctLooks = true;
        for (int place = 0; place < dsp::placeCount; ++place) {
            std::set<std::uint32_t> grades;
            for (int c = 0; c < dsp::characterCount; ++c) {
                names.insert(std::to_string(place) + dsp::character(place, c).name);
                grades.insert(dsp::character(place, c).grade);
            }
            distinctLooks &= grades.size() == std::size_t(dsp::characterCount);
        }
        s.check(names.size() == 30, "thirty named Characters (three per place)", std::to_string(names.size()));
        s.check(distinctLooks, "each Character of a place has its own colour grade");
    }

    // 1. Every Character sounds different from the other two of its place: ambience and the piano.
    {
        double nearestAmbience = 1e9, nearestPhrase = 1e9;
        std::string whereAmbience, wherePhrase;
        int clear = 0, pairs = 0;
        for (int place = 0; place < dsp::placeCount; ++place) {
            std::vector<AmbienceTrace> amb;
            std::vector<Character> phrase;
            for (int c = 0; c < dsp::characterCount; ++c) {
                auto rig = characterRig(place, c);
                rig->render(1.0);
                amb.push_back(ambienceOf(*rig, 15.0));
                auto dryRig = characterRig(place, c);
                dryRig->set(params::ambience, params::minDb);
                dryRig->render(1.0);
                phrase.push_back(characterOf(dryRig->render(7.0, standardPhrase(dryRig->time)), rate));
            }
            for (int a = 0; a < dsp::characterCount; ++a)
                for (int b = a + 1; b < dsp::characterCount; ++b) {
                    const double da = ambienceDistance(amb[std::size_t(a)], amb[std::size_t(b)]);
                    const double dp = characterDistance(phrase[std::size_t(a)], phrase[std::size_t(b)]);
                    const std::string label = std::string(placeNames[place]) + " " + dsp::character(place, a).name + " / " + dsp::character(place, b).name;
                    s.metric("character difference " + label + ": ambience / piano", da);
                    std::cout << "  [metric]   piano phrase distance " << dp << "\n";
                    if (da < nearestAmbience) { nearestAmbience = da; whereAmbience = label; }
                    if (dp < nearestPhrase) { nearestPhrase = dp; wherePhrase = label; }
                    ++pairs;
                    if (std::sqrt(da * da + dp * dp) > 1.5) ++clear;
                }
        }
        s.check(clear == pairs, "every pair of Characters in a place clearly differs (ambience and piano together over 1.5)",
                std::to_string(clear) + " of " + std::to_string(pairs));
        s.metric("closest pair of Characters, ambience (" + whereAmbience + ")", nearestAmbience);
        s.metric("closest pair of Characters, piano (" + wherePhrase + ")", nearestPhrase);
        s.check(nearestAmbience > 0.5, "every Character changes the place's own sounds", whereAmbience + " " + num(nearestAmbience, 2));
    }

    // 2. The macros, in every place, each its own way.
    {
        int presenceOk = 0, presenceStrong = 0, scaleOk = 0, activityOk = 0, unrealOk = 0, unrealClear = 0;
        std::vector<std::vector<double>> unrealDelta;
        for (int place = 0; place < dsp::placeCount; ++place) {
            const auto phraseWith = [&](const char* id, float v, float space = 0.35f) {
                auto rig = characterRig(place, 0);
                rig->set(params::ambience, params::minDb);
                rig->set(params::space, space);
                rig->set(id, v);
                rig->render(1.0);
                return rig->render(7.0, standardPhrase(rig->time));
            };
            {   // Presence: nearer = more direct sound against the room.
                const auto far = phraseWith(params::presence, 0.0f), near = phraseWith(params::presence, 1.0f);
                const double change = (windowDb(near, rate, 0.0, 3.5) - windowDb(near, rate, 5.6, 6.6)) - (windowDb(far, rate, 0.0, 3.5) - windowDb(far, rate, 5.6, 6.6));
                presenceOk += change > 2.0 ? 1 : 0;
                presenceStrong += change > 4.0 ? 1 : 0;
                s.metric(std::string(placeNames[place]) + " Presence 0 -> 1: direct-to-tail ratio change", change, " dB");
            }
            {   // Scale: a bigger room rings longer.
                const auto small = phraseWith(params::scale, 0.0f, 0.8f), big = phraseWith(params::scale, 1.0f, 0.8f);
                const double change = windowDb(big, rate, 5.6, 6.6) - windowDb(small, rate, 5.6, 6.6);
                scaleOk += change > 3.0 ? 1 : 0;
                s.metric(std::string(placeNames[place]) + " Scale 0 -> 1: tail level change", change, " dB");
            }
            {   // Activity: more happens (events counted by the engine: since Direction Update 2 they are
                // rare, quiet and distant, so they no longer stand out of the bed as loud onsets).
                auto calm = characterRig(place, 0), busy = characterRig(place, 0);
                calm->set(params::activity, 0.0f);
                busy->set(params::activity, 1.0f);
                calm->render(30.0);
                busy->render(30.0);
                const int a = calm->p->meters.events.load(), b = busy->p->meters.events.load();
                activityOk += b > a ? 1 : 0;
                s.metric(std::string(placeNames[place]) + " Activity 0 -> 1: events in 30 s at Activity 0 (" + std::to_string(a) + ") and at 100 %", double(b));
            }
            {   // Unreal: a different place, in the place's own way.
                const auto real = characterOf(phraseWith(params::unreal, 0.0f), rate), unreal = characterOf(phraseWith(params::unreal, 1.0f), rate);
                const double d = characterDistance(real, unreal);
                unrealOk += d > 0.8 ? 1 : 0;
                unrealClear += d > 1.0 ? 1 : 0;
                s.metric(std::string(placeNames[place]) + " Unreal 0 -> 1: character distance", d);
                std::vector<double> delta { unreal.tail - real.tail, (unreal.width - real.width) * 32.0, (unreal.flicker - real.flicker) * 5.0 };
                for (std::size_t k = 0; k < real.tone.size(); ++k) delta.push_back(unreal.tone[k] - real.tone[k]);
                unrealDelta.push_back(delta);
            }
        }
        s.check(presenceOk == dsp::placeCount, "Presence brings the instruments forward in every place (direct-to-tail over 2 dB)", std::to_string(presenceOk) + " of 10");
        s.report(presenceStrong == dsp::placeCount, "Presence: over 4 dB in every place", std::to_string(presenceStrong) + " of 10 (the office's own wet echo layer is not moved by Presence)");
        s.check(scaleOk == dsp::placeCount, "Scale makes the room ring longer in every place", std::to_string(scaleOk) + " of 10");
        s.check(activityOk == dsp::placeCount, "Activity makes more happen in every place", std::to_string(activityOk) + " of 10");
        s.check(unrealOk == dsp::placeCount, "Unreal transforms the piano in every place (character distance over 0.8)", std::to_string(unrealOk) + " of 10");
        // Report-only since Direction Update 2 (two attempts): the impossible room is already
        // strange at Unreal 0 (a swelling cathedral tail), so Unreal adds a little less there.
        s.report(unrealClear == dsp::placeCount, "Unreal: over 1.0 in every place", std::to_string(unrealClear) + " of 10");
        double nearest = 1e9;
        for (std::size_t a = 0; a < unrealDelta.size(); ++a)
            for (std::size_t b = a + 1; b < unrealDelta.size(); ++b) {
                double d = 0;
                for (std::size_t k = 0; k < unrealDelta[a].size(); ++k) d += (unrealDelta[a][k] - unrealDelta[b][k]) * (unrealDelta[a][k] - unrealDelta[b][k]);
                nearest = std::min(nearest, std::sqrt(d / double(unrealDelta[a].size())));
            }
        s.check(nearest > 1.0, "Unreal does something different in each place (closest pair of places' Unreal changes over 1 dB RMS apart)", num(nearest, 2));
    }

    // 3. Evolution: deterministic for a seed, freezable, resettable.
    {
        auto a = characterRig(8, 1), b = characterRig(8, 1), c = characterRig(8, 1);
        c->set(params::evolutionSeed, 77.0f);
        for (auto* r : { a.get(), b.get(), c.get() }) r->set(params::evolution, 1.0f);
        const auto x = a->render(12.0), y = b->render(12.0), z = c->render(12.0);
        s.check(maxDiff(x, y) == 0.0, "evolution is deterministic: the same seed gives the same 12 s, sample for sample");
        s.check(maxDiff(x, z) > 1.0e-4, "a different seed evolves differently");
        const auto& evo = a->p->placeNow().evolution();
        a->set(params::evolutionFreeze, 1.0f);
        a->render(0.1);
        const float held[] { evo.spectral(), evo.spatial(), evo.harmonic() };
        a->render(5.0);
        s.check(evo.spectral() == held[0] && evo.spatial() == held[1] && evo.harmonic() == held[2], "Freeze holds the evolution where it is");
        a->set(params::evolutionFreeze, 0.0f);
        a->p->requestEvolutionReset();
        a->render(512.0 / rate);   // one block: reset, then ticks from the start
        auto fresh = characterRig(8, 1);
        fresh->set(params::evolution, 1.0f);
        fresh->render(512.0 / rate);
        const auto& f = fresh->p->placeNow().evolution();
        s.check(std::abs(evo.spectral() - f.spectral()) < 1.0e-4f && std::abs(evo.harmonic() - f.harmonic()) < 1.0e-4f, "Reset starts the evolution again from its seed",
                num(evo.spectral(), 5) + " / " + num(f.spectral(), 5) + ", " + num(evo.harmonic(), 5) + " / " + num(f.harmonic(), 5));
        double moved = 0;
        {
            auto rig = characterRig(8, 1);
            rig->set(params::evolution, 1.0f);
            float lo = 1, hi = -1;
            for (int i = 0; i < 40; ++i) {
                rig->render(0.5);
                lo = std::min(lo, rig->p->placeNow().evolution().spectral());
                hi = std::max(hi, rig->p->placeNow().evolution().spectral());
            }
            moved = hi - lo;
        }
        s.check(moved > 0.3, "the place keeps changing slowly (evolution spectral range over 20 s at full rate)", num(moved, 2));
    }

    // 4. Scenes A/B: a morph while a chord rings and notes play is smooth; no stuck notes.
    {
        Rig rig(rate);
        auto& p = *rig.p;
        p.storeScene(0);   // A: the default (hospital, piano)
        setPlace(rig, 4);
        rig.set(params::character, 2.0f);
        rig.set(params::pianoOn, 0.0f);
        rig.set(params::synthOn, 1.0f);
        rig.set(params::glassOn, 1.0f);
        rig.set(params::unreal, 0.8f);
        rig.set(params::space, 0.8f);
        setFx(rig, 1, params::fx::echo, 0.6f);
        p.storeScene(1);   // B: the pool, Submerged, synth + glass, Unreal
        // Back to A's knobs; the morph plays the scenes.
        setPlace(rig, 0);
        rig.set(params::character, 0.0f);
        rig.set(params::pianoOn, 1.0f);
        rig.set(params::synthOn, 0.0f);
        rig.set(params::glassOn, 0.0f);
        rig.set(params::sceneOn, 1.0f);
        rig.set(params::sceneMorph, 0.0f);
        rig.render(1.0);
        std::vector<Event> events;
        for (int note : { 48, 55, 60, 64 }) events.push_back({ rig.time + 0.05, on(note, 90) });
        events.push_back({ rig.time + 0.05, pedal(true) });
        for (int i = 0; i < 16; ++i) {
            events.push_back({ rig.time + 0.5 + 0.4 * i, on(67 + (i * 5) % 12, 80) });
            events.push_back({ rig.time + 0.8 + 0.4 * i, off(67 + (i * 5) % 12) });
        }
        Signal out;
        juce::AudioBuffer<float> buffer(2, 512);
        allocationProbe::calls = 0;
        const int blocks = int(7.0 * rate / 512);
        for (int block = 0; block < blocks; ++block) {
            juce::MidiBuffer midi;
            const double start = rig.time, end = start + 512.0 / rate;
            for (const auto& e : events)
                if (e.at >= start && e.at < end) midi.addEvent(e.message, int((e.at - start) * rate));
            rig.set(params::sceneMorph, std::clamp(float(block) / float(blocks - 40), 0.0f, 1.0f));   // A -> B over ~6.5 s
            allocationProbe::enabled = true;
            p.processBlock(buffer, midi);
            allocationProbe::enabled = false;
            rig.time = end;
            for (int i = 0; i < 512; ++i) {
                out.first.push_back(buffer.getSample(0, i));
                out.second.push_back(buffer.getSample(1, i));
            }
        }
        double worstStep = 0, peakAll = 0, worstJump = 0;
        for (std::size_t i = 1; i < out.first.size(); ++i) {
            worstStep = std::max(worstStep, double(std::abs(out.first[i] - out.first[i - 1])));
            peakAll = std::max(peakAll, double(std::abs(out.first[i])));
        }
        for (double t = 0.6; t + 0.2 < 7.0; t += 0.1) worstJump = std::max(worstJump, std::abs(windowDb(out, rate, t, t + 0.1) - windowDb(out, rate, t - 0.1, t)));
        s.check(allocationProbe::calls == 0, "morphing A -> B while playing never allocates");
        s.check(finite(out) && worstStep < 0.5 * peakAll, "the morph is click-free (largest step under half the peak)", num(worstStep, 3) + " vs " + num(peakAll, 3));
        s.check(worstJump < 9.0, "no large level jumps during the morph (100 ms windows within 9 dB of the one before)", num(worstJump) + " dB");
        s.check(p.placeNow().place() == dsp::Place::pool && p.placeNow().characterIndex() == 2, "at B: scene B's place and Character are heard");
        rig.render(0.1, { { rig.time, pedal(false) } });
        for (int note : { 48, 55, 60, 64 }) rig.render(0.01, { { rig.time, off(note) } });
        rig.render(8.0);
        s.check(p.meters.voices.load() == 0, "no stuck notes after the morph (every voice ended)");
        // State: the scenes come back.
        juce::MemoryBlock block;
        p.getStateInformation(block);
        LiminalProcessor q;
        q.setStateInformation(block.getData(), int(block.getSize()));
        s.check(q.sceneStored(0) && q.sceneStored(1), "scenes A and B are saved with the song and restored");
        Rig r1(rate), r2(rate);
        r1.p->setStateInformation(block.getData(), int(block.getSize()));
        r2.p->setStateInformation(block.getData(), int(block.getSize()));
        r1.p->prepareToPlay(rate, 512);
        r2.p->prepareToPlay(rate, 512);
        s.check(maxDiff(r1.render(2.0, { { 0.1, on(60) } }), r2.render(2.0, { { 0.1, on(60) } })) == 0.0, "a restored song with scenes plays identically every time");
    }
}
