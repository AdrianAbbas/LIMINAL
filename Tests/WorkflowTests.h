// Milestone 6: presets, MIDI details, quality modes, ambience export, state.
// Included by LiminalTests.cpp after CharacterTests.h.

double spectralCentroid(const Signal& x, double rate, double from, double to) {
    const Spectrum sp(monoOf(x, rate, from, to), rate);
    double num = 0, den = 0;
    for (double f = 60; f < 12000; f *= 1.06) {
        const double e = std::pow(10.0, sp.bandDb(f, f * 1.06) / 10.0);
        num += e * f;
        den += e;
    }
    return num / std::max(1e-20, den);
}

void testWorkflow(Suite& s) {
    s.section("presets, MIDI, quality, export, state (Milestone 6)");
    const double rate = 48000.0;
    using presets::Category;
    {
        const auto count = [](Category c) { return int(presets::inCategory(c).size()); };
        s.check(count(Category::environment) >= 10 && count(Category::character) >= 30 && count(Category::instrument) >= 26 && count(Category::scene) >= 20,
                "factory presets: 10 places, 30 Characters, 6 + 20 instruments, 20 scenes",
                std::to_string(count(Category::environment)) + " / " + std::to_string(count(Category::character)) + " / " + std::to_string(count(Category::instrument)) + " / "
                    + std::to_string(count(Category::scene)));
        LiminalProcessor p;
        int unknown = 0, outOfRange = 0;
        for (const auto& preset : presets::factory())
            for (const auto& [id, value] : preset.values) {
                auto* param = p.parameters.getParameter(id);
                if (param == nullptr) { ++unknown; std::cout << "  unknown parameter " << id << " in " << preset.name << "\n"; continue; }
                const auto range = param->getNormalisableRange();
                if (value < range.start - 1e-4f || value > range.end + 1e-4f) { ++outOfRange; std::cout << "  out of range " << id << " in " << preset.name << "\n"; }
            }
        s.check(unknown == 0 && outOfRange == 0, "every preset value names a real parameter and is in its range");
    }
    {   // Every preset loads, plays, and (place, Character and scene presets) the place is audible without MIDI,
        // as the AU harness checks in GarageBand-like conditions (over -60 dBFS one second in: the beds are quiet now).
        int loaded = 0, audible = 0, margin = 0, playing = 0, total = 0;
        double quietest = 0;
        std::string quietestName;
        for (const auto& preset : presets::factory()) {
            ++total;
            Rig rig(rate);
            rig.p->applyPreset(preset);
            rig.render(1.0);
            const auto bed = rig.render(1.0);
            const auto note = rig.render(2.0, { { rig.time + 0.05, on(60, 100) }, { rig.time + 0.05, on(64, 100) }, { rig.time + 0.05, on(67, 100) },
                                                { rig.time + 1.5, off(60) }, { rig.time + 1.5, off(64) }, { rig.time + 1.5, off(67) } });
            const bool ok = finite(bed) && finite(note) && peak(note) < 1.0f && rig.p->presetName() == preset.name;
            loaded += ok ? 1 : 0;
            playing += rms(note) > rms(bed) * 1.41 ? 1 : 0;
            if (preset.category != Category::instrument) {
                const double level = toDb(rms(bed));
                audible += level > -60.0 ? 1 : 0;
                margin += level > -57.0 ? 1 : 0;
                if (quietestName.empty() || level < quietest) { quietest = level; quietestName = preset.name.toStdString(); }
            } else {
                ++audible;
                ++margin;
            }
        }
        s.check(loaded == total, "every factory preset loads and plays cleanly", std::to_string(loaded) + " of " + std::to_string(total));
        s.check(playing == total, "every preset plays a MIDI chord clearly over its ambience (3 dB or more)", std::to_string(playing) + " of " + std::to_string(total));
        s.check(audible == total, "every place, Character and scene preset keeps the place audible (over -60 dBFS, the AU harness's gate)", "quietest " + quietestName + " " + num(quietest) + " dBFS");
        s.report(margin == total, "and with 3 dB to spare (over -57 dBFS)", std::to_string(margin) + " of " + std::to_string(total));
    }
    {   // What each group changes.
        Rig rig(rate);
        rig.p->applyPreset(presets::factory()[std::size_t(presets::inCategory(Category::scene)[1])]);   // The Last Patient
        const float drone = rig.p->parameters.getRawParameterValue(params::droneOn)->load();
        rig.p->applyPreset(presets::factory()[std::size_t(presets::inCategory(Category::environment)[4])]);   // the pool
        s.check(rig.p->parameters.getRawParameterValue(params::droneOn)->load() == drone && std::lround(rig.p->parameters.getRawParameterValue(params::environment)->load()) == 4,
                "a place preset changes the place and keeps the instruments");
        rig.p->applyPreset(presets::factory()[std::size_t(presets::inCategory(Category::instrument)[2])]);   // glass
        s.check(std::lround(rig.p->parameters.getRawParameterValue(params::environment)->load()) == 4 && rig.p->parameters.getRawParameterValue(params::glassOn)->load() > 0.5f
                    && rig.p->parameters.getRawParameterValue(params::droneOn)->load() < 0.5f,
                "an instrument preset changes the instruments and keeps the place");
        LiminalProcessor p;
        s.check(p.getNumPrograms() == 21 && p.getProgramName(0) == "Init" && p.getProgramName(1) == "FORGOTTEN CHILDHOOD", "the host sees Init and the twenty scenes as programs");
        p.setCurrentProgram(2);
        s.check(p.getCurrentProgram() == 2 && p.presetName() == "THE LAST PATIENT" && p.parameters.getRawParameterValue(params::droneOn)->load() > 0.5f, "choosing a host program loads that scene");
        p.setCurrentProgram(0);
        s.check(p.parameters.getRawParameterValue(params::droneOn)->load() < 0.5f && p.parameters.getRawParameterValue(params::pianoOn)->load() > 0.5f, "Init returns to the defaults");
    }
    {   // User presets: save, list, load.
        LiminalProcessor a;
        a.applyPreset(presets::factory()[std::size_t(presets::inCategory(Category::scene)[8])]);
        a.parameters.getParameter(params::presence)->setValueNotifyingHost(0.83f);
        const auto name = "test preset " + juce::String(juce::Time::currentTimeMillis());
        const auto file = a.saveUserPreset(name);
        LiminalProcessor b;
        const bool listed = b.userPresets().contains(file);
        const bool loaded = b.loadUserPreset(file);
        bool same = true;
        for (auto* param : a.getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param);
            if (ranged->getParameterID() == params::power) continue;
            same &= std::abs(ranged->getValue() - b.parameters.getParameter(ranged->getParameterID())->getValue()) < 1.0e-4f;
        }
        s.check(file.existsAsFile() && listed && loaded && same, "a user preset is saved, listed and loads every setting back", file.getFullPathName().toStdString());
        file.deleteFile();
    }

    // MIDI details.
    {
        auto range = [&](int low) {
            Rig rig(rate);
            dry(rig);
            rig.set(params::pianoOn, 1.0f);
            rig.p->parameters.getParameter(params::midiId(0, params::midi::keyLow))->setValueNotifyingHost(rig.p->parameters.getParameter(params::midiId(0, params::midi::keyLow))->convertTo0to1(float(low)));
            rig.render(0.1);
            return rms(playNote(rig, 48, 100, 0.5, 0.1));
        };
        s.check(range(0) > 1e-3 && range(60) < 1e-6, "a note below an instrument's lowest key does not play it");
        const auto level = [&](float velocityVolume, int velocity) {
            Rig rig(rate);
            dry(rig);
            rig.p->parameters.getParameter(params::midiId(0, params::midi::velocityVolume))->setValueNotifyingHost(velocityVolume);
            rig.render(0.1);
            return toDb(rms(playNote(rig, 60, velocity, 0.5, 0.0)));
        };
        const double spreadOwn = level(1.0f, 120) - level(1.0f, 30), spreadFlat = level(0.0f, 120) - level(0.0f, 30);
        s.check(spreadOwn > 10.0 && std::abs(spreadFlat) < 1.0, "Velocity to Volume: 100 % = the instrument's own response, 0 % = every note alike",
                num(spreadOwn) + " -> " + num(spreadFlat) + " dB between velocity 30 and 120");
        const auto brightness = [&](float velocityFilter, int velocity) {
            Rig rig(rate);
            dry(rig);
            rig.p->parameters.getParameter(params::midiId(0, params::midi::velocityFilter))->setValueNotifyingHost(velocityFilter);
            rig.p->parameters.getParameter(params::midiId(0, params::midi::velocityVolume))->setValueNotifyingHost(0.0f);
            rig.render(0.1);
            return spectralCentroid(playNote(rig, 72, velocity, 0.8, 0.0), rate, 0.05, 0.6);
        };
        const double off = brightness(0.0f, 40) / brightness(0.0f, 120), onRatio = brightness(1.0f, 40) / brightness(1.0f, 120);
        s.check(onRatio < 0.9 * off, "Velocity to Filter: soft notes darker (centroid over 10 % lower)", "soft/loud centroid " + num(off, 2) + " -> " + num(onRatio, 2));
        s.report(onRatio < 0.8 * off, "Velocity to Filter: centroid over 20 % lower", num(onRatio / off, 2));
    }

    // Quality: sustained chords stay stable in every mode; CPU follows the mode; switching is silent.
    {
        std::array<double, 3> cpu {};
        bool stable = true;
        for (int q = 0; q < 3; ++q) {
            Rig rig(rate);
            rig.set(params::quality, float(q));
            for (const char* id : { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn }) rig.set(id, 1.0f);
            std::vector<Event> chord { { 0.05, pedal(true) } };
            for (int note = 48; note < 64; ++note) chord.push_back({ 0.05, on(note, 100) });
            const auto x = rig.render(10.0, chord);
            stable &= finite(x) && peak(x) < 1.0f && rms(x) > 1e-3;
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer none;
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < int(5.0 * rate / 512); ++i) rig.p->processBlock(buffer, none);
            cpu[std::size_t(q)] = 100.0 * std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / 5.0;
        }
        s.check(stable, "a 16-note pedalled chord on all six instruments stays stable for 10 s in Eco, Standard and High");
        s.metric("CPU, 16 held notes on all six, Eco (report-only)", cpu[0], " %");
        s.metric("CPU, 16 held notes on all six, Standard (report-only)", cpu[1], " %");
        s.metric("CPU, 16 held notes on all six, High (report-only)", cpu[2], " %");
        s.check(cpu[0] < cpu[1] * 0.9, "Eco costs clearly less than Standard", num(cpu[0], 1) + " vs " + num(cpu[1], 1) + " %");
        Rig rig(rate);
        rig.render(0.2, { { 0.05, on(60, 100) }, { 0.05, on(64, 100) } });
        Signal out = rig.render(0.5);
        rig.set(params::quality, 0.0f);
        const auto after = rig.render(1.0);
        out.first.insert(out.first.end(), after.first.begin(), after.first.end());
        double worst = 0, top = 0;
        for (std::size_t i = 1; i < out.first.size(); ++i) {
            worst = std::max(worst, double(std::abs(out.first[i] - out.first[i - 1])));
            top = std::max(top, double(std::abs(out.first[i])));
        }
        s.check(worst < 0.25 * top, "switching quality while notes ring is click-free", num(worst, 4) + " vs " + num(top, 3));
    }

    // Export: the same engine, offline, deterministic, any rate.
    {
        LiminalProcessor p;
        p.applyPreset(presets::factory()[std::size_t(presets::inCategory(Category::scene)[3])]);   // Dream of an Empty Pool
        juce::MemoryBlock state;
        p.getStateInformation(state);
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("liminal-export-test");
        folder.createDirectory();
        exporter::Options o;
        o.seconds = 6.0;
        const auto a = folder.getChildFile("a.wav"), b = folder.getChildFile("b.wav"), c = folder.getChildFile("c.wav"), d = folder.getChildFile("d.wav");
        const bool ok = exporter::renderAmbience(state, o, a) && exporter::renderAmbience(state, o, b);
        o.sampleRate = 96000.0;
        const bool ok96 = exporter::renderAmbience(state, o, c);
        o.sampleRate = 48000.0;
        o.room = false;
        const bool okDry = exporter::renderAmbience(state, o, d);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const auto read = [&](const juce::File& f) {
            std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f));
            Signal x;
            if (r == nullptr) return std::make_pair(x, 0.0);
            juce::AudioBuffer<float> buf(2, int(r->lengthInSamples));
            r->read(&buf, 0, int(r->lengthInSamples), 0, true, true);
            for (int i = 0; i < buf.getNumSamples(); ++i) {
                x.first.push_back(buf.getSample(0, i));
                x.second.push_back(buf.getSample(1, i));
            }
            return std::make_pair(x, r->sampleRate);
        };
        const auto [xa, ra] = read(a);
        const auto [xb, rb] = read(b);
        const auto [xc, rc] = read(c);
        const auto [xd, rd] = read(d);
        s.check(ok && xa.first.size() == std::size_t(6.0 * 48000) && ra == 48000.0, "exports a 6 s stereo WAV of the place's ambience", std::to_string(xa.first.size()) + " frames");
        s.check(toDb(rms(xa)) > -60.0 && toDb(rms(xa)) < -25.0, "the exported ambience is the place's bed (audible, not clipped)", num(toDb(rms(xa))) + " dBFS");
        s.check(maxDiff(xa, xb) == 0.0, "the export is deterministic: the same file twice, sample for sample");
        s.check(ok96 && rc == 96000.0 && xc.first.size() == std::size_t(6.0 * 96000), "exports at 96 kHz too");
        s.check(okDry && maxDiff(xa, xd) > 1e-4, "the dry stem (no room) differs from the stem with the room");
        {   // The export is the live engine: live with instruments off and the same state = the file.
            Rig live(rate);
            live.p->setStateInformation(state.getData(), int(state.getSize()));
            for (const char* key : params::instrumentKeys) live.set((juce::String(key) + "_on").toRawUTF8(), 0.0f);
            live.p->prepareToPlay(rate, 512);
            const auto y = live.render(6.0);
            s.check(maxDiff(xa, y) < 1.0e-5, "the export equals live playback of the same state (24-bit rounding aside)", num(maxDiff(xa, y) * 1e6, 2) + " e-6");
        }
        folder.deleteRecursively();
    }

    // State: duplicating, reloading, presets with scenes, old versions.
    {
        Rig a(rate);
        a.p->applyPreset(presets::factory()[std::size_t(presets::inCategory(Category::scene)[9])]);
        a.p->storeScene(0);
        a.set(params::unreal, 0.7f);
        a.p->storeScene(1);
        a.set(params::sceneOn, 1.0f);
        a.set(params::sceneMorph, 0.4f);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Rig b(rate), c(rate);
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        c.p->setStateInformation(state.getData(), int(state.getSize()));
        b.p->prepareToPlay(rate, 512);
        c.p->prepareToPlay(rate, 512);
        const auto x = b.render(3.0, { { 0.2, on(60) }, { 1.0, off(60) } }), y = c.render(3.0, { { 0.2, on(60) }, { 1.0, off(60) } });
        s.check(maxDiff(x, y) == 0.0, "a duplicated instance (same state) plays identically");
        juce::MemoryBlock again;
        b.p->getStateInformation(again);
        s.check(again == state, "saving a reopened project gives back the same state, scenes included (nothing changes on reload)");
        // A Milestone 0-4 project (state version 1, no scenes) still loads.
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        xml->setAttribute("stateVersion", 1);
        xml->deleteAllChildElementsWithTagName("SCENES");
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary(*xml, old);
        LiminalProcessor d;
        d.setStateInformation(old.getData(), int(old.getSize()));
        s.check(!d.sceneStored(0) && std::lround(d.parameters.getRawParameterValue(params::environment)->load()) == 5, "an older project without scenes loads (settings kept, no scenes)");
    }
    {   // Loading presets while playing never allocates on the audio thread.
        Rig rig(rate);
        juce::AudioBuffer<float> buffer(2, 512);
        allocationProbe::calls = 0;
        int index = 0;
        for (const auto& preset : presets::factory()) {
            if (index++ % 3 != 0) continue;
            rig.p->applyPreset(preset);   // message thread, between blocks
            juce::MidiBuffer midi;
            midi.addEvent(on(60 + index % 12, 90), 0);
            for (int i = 0; i < 4; ++i) {
                allocationProbe::enabled = true;
                rig.p->processBlock(buffer, midi);
                allocationProbe::enabled = false;
                midi.clear();
            }
        }
        s.check(allocationProbe::calls == 0, "loading presets between blocks: processBlock never allocates (no memory growth)", std::to_string(allocationProbe::calls));
    }
}
