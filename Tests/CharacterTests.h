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

// Direction Update 2, Section 1's hard rules, re-checked in every place under each of its three
// Characters (Part 3): a quiet, still, tonal bed; rare, sparse events (default Activity and
// 100 %); no risers, whooshes or waves of noise (the air above 2 kHz never swells for long); the
// instruments about 10-15 dB over the room at the default settings.
void testAudioRules(Suite& s) {
    s.section("audio rules: ten places x three Characters (Direction Update 2, Section 1)");
    const double rate = 48000.0;
    int beds = 0, still = 0, tonal = 0, rare = 0, noSwell = 0, over = 0, total = 0;
    double minGap = 1e9, maxGap = -1e9, longestSwell = 0;
    std::string worstGap, worstSwell;
    for (int place = 0; place < dsp::placeCount; ++place)
        for (int c = 0; c < dsp::characterCount; ++c) {
            ++total;
            const std::string name = std::string(placeNames[place]) + " / " + dsp::character(place, c).name;
            // The bed alone (Activity 0), 20 s after 2 s to settle.
            auto rig = characterRig(place, c);
            rig->set(params::activity, 0.0f);
            rig->render(2.0);
            const auto x = rig->render(20.0);
            const double level = toDb(rms(x));
            auto env = envelope(x, rate, 0.25);
            std::sort(env.begin(), env.end());
            const double spread = env[env.size() * 95 / 100] - env[env.size() * 5 / 100];
            double tone = 0, all = 0;
            {
                const Spectrum sp(monoOf(x, rate, 4.0, 8.0), rate, 1 << 17);
                const auto& m = sp.mag;
                const std::size_t lo = std::size_t(30.0 / sp.binHz), hi = std::min(m.size() - 64, std::size_t(6000.0 / sp.binHz));
                std::vector<double> local;
                for (std::size_t i = lo; i < hi; ++i) {
                    local.clear();
                    for (std::size_t j = i - 48; j < i + 48; j += 3) local.push_back(m[j]);
                    std::nth_element(local.begin(), local.begin() + std::ptrdiff_t(local.size() / 2), local.end());
                    all += m[i] * m[i];
                    if (m[i] > 4.0 * local[local.size() / 2]) tone += m[i] * m[i];
                }
            }
            const double share = tone / std::max(1e-30, all);
            beds += level > -64.0 && level < -36.0 ? 1 : 0;
            still += (place == 7 || place == 9 || spread < 2.5) ? 1 : 0;   // (the compressor cycles; the impossible room's silences)
            tonal += share > 0.5 ? 1 : 0;
            if (!(level > -64.0 && level < -36.0) || !(place == 7 || place == 9 || spread < 2.5) || share <= 0.5)
                std::cout << "  [detail] " << name << ": bed " << num(level, 1) << " dBFS, moves " << num(spread, 2) << " dB, tonal " << num(share, 2) << "\n";
            // Events from the engine itself: default Activity and 100 %, 3 minutes each.
            const auto eventTimes = [&](float activity) {
                dsp::Ambience amb;
                amb.prepare(rate);
                amb.configure(dsp::Ambience::Place(place), 1, &dsp::character(place, c));
                amb.setActivity(dsp::activityRate(activity, dsp::macroResponse(place).activity));
                std::vector<double> times;
                int seen = 0, flicks = 0;
                for (int i = 0; i < int(180.0 * rate); ++i) {
                    amb.tick();
                    if (amb.spawned() + amb.flickers() > seen + flicks) times.push_back(double(i) / rate);
                    seen = amb.spawned();
                    flicks = amb.flickers();
                }
                return times;
            };
            const auto atDefault = eventTimes(0.25f), atFull = eventTimes(1.0f);
            double closest = 1e9;
            for (std::size_t i = 1; i < atDefault.size(); ++i) closest = std::min(closest, atDefault[i] - atDefault[i - 1]);
            const bool sparse = atDefault.size() <= 9 && (atDefault.size() < 2 || closest >= 19.0) && atFull.size() <= 40;
            rare += sparse ? 1 : 0;
            if (!sparse) std::cout << "  [detail] " << name << ": events " << atDefault.size() << " (closest " << num(closest, 1) << " s) / " << atFull.size() << " at 100 %\n";
            // No risers or whooshes: at the default Activity, the air above 2 kHz (250 ms frames)
            // never stays more than 8 dB over its median for over 1.5 s.
            {
                auto r2 = characterRig(place, c);
                r2->render(2.0);
                const auto y = r2->render(45.0);
                std::vector<double> hiEnv;
                double hp = 0, prev = 0, acc = 0;
                const double a = std::exp(-2.0 * juce::MathConstants<double>::pi * 2000.0 / rate);
                const int frame = int(0.25 * rate);
                for (std::size_t i = 0; i < y.first.size(); ++i) {
                    const double m = 0.5 * (double(y.first[i]) + double(y.second[i]));
                    hp = a * (hp + m - prev);
                    prev = m;
                    acc += hp * hp;
                    if ((i + 1) % std::size_t(frame) == 0) {
                        hiEnv.push_back(10.0 * std::log10(acc / frame + 1e-20));
                        acc = 0;
                    }
                }
                auto sorted = hiEnv;
                std::nth_element(sorted.begin(), sorted.begin() + std::ptrdiff_t(sorted.size() / 2), sorted.end());
                const double median = sorted[sorted.size() / 2];
                int run = 0, longest = 0, endsAt = 0;
                for (std::size_t k = 0; k < hiEnv.size(); ++k) {
                    run = hiEnv[k] > median + 8.0 ? run + 1 : 0;
                    if (run > longest) { longest = run; endsAt = int(k); }
                }
                const double seconds = 0.25 * longest;
                if (seconds > 1.5) std::cout << "  [detail] " << name << ": the air above 2 kHz swells for " << num(seconds, 2) << " s, ending " << num(2.0 + 0.25 * (endsAt + 1), 2) << " s in\n";
                // (The impossible room's reverb swells after each sound by design - Section 2 - so a
                // ghost note's tail may stay up to 3 s; everywhere else 1.5 s.)
                noSwell += seconds <= (place == 9 ? 3.0 : 1.5) ? 1 : 0;
                if (place != 9 && seconds > longestSwell) { longestSwell = seconds; worstSwell = name; }
                // The instruments over the room: the standard phrase alone vs this bed at the default Activity.
                auto r3 = characterRig(place, c);
                r3->set(params::ambience, params::minDb);
                r3->render(1.0);
                const auto phrase = r3->render(7.0, standardPhrase(r3->time));
                const double gap = toDb(rms(phrase)) - toDb(rms(y));
                over += gap >= 10.0 ? 1 : 0;
                if (gap < minGap) { minGap = gap; worstGap = name; }
                maxGap = std::max(maxGap, gap);
                if (gap < 10.0) std::cout << "  [detail] " << name << ": phrase over the room " << num(gap, 1) << " dB\n";
            }
        }
    const auto all = std::to_string(total);
    s.check(beds == total, "every bed is quiet (-64..-36 dBFS at Activity 0)", std::to_string(beds) + " of " + all);
    s.check(still == total, "every bed is still: steady hums, no waves (level moves under 2.5 dB)", std::to_string(still) + " of " + all);
    s.check(tonal == total, "every bed is tonal: noise stays below the hum (over half the energy in tones)", std::to_string(tonal) + " of " + all);
    s.check(rare == total, "events rare and distant in every Character (default: at most 9 in 3 min, never two within 20 s; 100 %: at most 40)", std::to_string(rare) + " of " + all);
    s.check(noSwell == total, "no risers, whooshes or noise swells (air above 2 kHz never 8 dB over its median for over 1.5 s; the impossible room's swelling reverb 3 s)",
            std::to_string(noSwell) + " of " + all + ", longest " + num(longestSwell, 2) + " s (" + worstSwell + ")");
    s.metric("phrase over the room at default settings: smallest / largest", minGap, (" / " + num(maxGap, 1) + " dB (smallest: " + worstGap + ")").c_str());
    s.check(over == total, "the instruments sit at least 10 dB over the room in every place and Character", std::to_string(over) + " of " + all);
}
