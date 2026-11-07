// Old sessions and presets open exactly as before (2026-11-02, the instrument library).
// Tests/fixtures/sessions-v2.xml was written by the build BEFORE the library existed
// (`LiminalTests --write-session-fixtures Tests/fixtures/sessions-v2.xml`): for the default state, a
// state with all six instruments on and many controls moved, and every factory scene, the saved
// state itself and a fingerprint of a rendered phrase (RMS per 250 ms window, per channel, in dB;
// the synthesised stand-ins and the rooms without captures, so it is pure code, the same on every
// machine). The group `sessions` loads each saved state into today's build and checks that every
// parameter it saved comes back, every slot holds its own instrument, and the phrase renders the
// same. Included by LiminalTests.cpp after ScrollTests.h.

namespace sessiontest {

constexpr double rate = 48000.0;
constexpr int block = 512;
constexpr double seconds = 7.0;
constexpr double window = 0.25;

// A phrase over the whole keyboard (each slot's range), held chords, the pedal.
std::vector<Event> phrase() {
    std::vector<Event> e;
    const int notes[] { 48, 55, 60, 64, 67, 72, 76, 79, 84, 65, 69, 57 };
    double t = 0.1;
    for (int k = 0; k < 12; ++k) {
        e.push_back({ t, juce::MidiMessage::noteOn(1, notes[k], juce::uint8(40 + 7 * k)) });
        e.push_back({ t + 0.35 + 0.1 * (k % 3), juce::MidiMessage::noteOff(1, notes[k]) });
        t += 0.22;
    }
    e.push_back({ 3.0, juce::MidiMessage::controllerEvent(1, 64, 127) });
    for (int n : { 45, 52, 60, 64, 71 }) e.push_back({ 3.05, juce::MidiMessage::noteOn(1, n, juce::uint8(90)) });
    for (int n : { 45, 52, 60, 64, 71 }) e.push_back({ 4.6, juce::MidiMessage::noteOff(1, n) });
    e.push_back({ 5.2, juce::MidiMessage::controllerEvent(1, 64, 0) });
    return e;
}

// Loads `state` into a fresh processor and renders the phrase: dB per window, L then R.
std::vector<float> fingerprint(const juce::MemoryBlock& state) {
    BypassRecordings standIns;
    const bool capturesWere = dsp::CaptureBank::disabled();
    dsp::CaptureBank::disabled() = true;
    std::vector<float> out;
    {
        auto p = std::make_unique<LiminalProcessor>();
        p->setStateInformation(state.getData(), int(state.getSize()));
        p->setRateAndBufferSizeDetails(rate, block);
        p->prepareToPlay(rate, block);
        Rig rig(rate, block);
        rig.p = std::move(p);
        const auto sig = rig.render(seconds, phrase());
        const auto w = std::size_t(window * rate);
        for (const auto* ch : { &sig.first, &sig.second })
            for (std::size_t i = 0; i + w <= ch->size(); i += w) {
                double sum = 0;
                for (std::size_t k = i; k < i + w; ++k) sum += double((*ch)[k]) * (*ch)[k];
                out.push_back(float(toDb(std::sqrt(sum / double(w)))));
            }
    }
    dsp::CaptureBank::disabled() = capturesWere;
    return out;
}

juce::String join(const std::vector<float>& v) {
    juce::StringArray a;
    for (float x : v) a.add(juce::String(x, 3));
    return a.joinIntoString(" ");
}

// The states the fixture holds: name -> state.
std::vector<std::pair<juce::String, juce::MemoryBlock>> states() {
    std::vector<std::pair<juce::String, juce::MemoryBlock>> list;
    const auto save = [&](const juce::String& name, LiminalProcessor& p) {
        juce::MemoryBlock m;
        p.getStateInformation(m);
        list.push_back({ name, m });
    };
    {
        LiminalProcessor p;
        save("default", p);
    }
    {   // all six on, many controls away from their defaults, scenes A/B stored
        LiminalProcessor p;
        const auto set = [&](const juce::String& id, float v) {
            if (auto* q = p.parameters.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v));
        };
        for (const char* key : params::instrumentKeys) set(juce::String(key) + "_on", 1.0f);
        set(params::pianoAge, 0.7f); set(params::synthWarmth, 0.8f); set(params::glassDecay, 0.3f); set(params::choirFormant, 0.7f);
        set(params::boxMechanism, 0.9f); set(params::droneSub, 0.8f); set(params::synthPatch, 2.0f);
        set(params::environment, 5.0f); set(params::character, 1.0f); set(params::liminality, 0.6f);
        for (int i = 0; i < params::instrumentCount; ++i) {
            set(params::fxId(i, params::fx::echo), 0.1f * float(i));
            set(params::limId(i, "detune"), 0.1f + 0.1f * float(i));
            set(params::midiId(i, params::midi::keyLow), float(30 + 3 * i));
        }
        set(params::pianoLevel, -6.0f);
        set(params::droneLevel, -9.0f);
        p.storeScene(0);
        save("all six, moved", p);
    }
    for (const int index : presets::inCategory(presets::Category::scene)) {
        LiminalProcessor p;
        p.applyPreset(presets::factory()[std::size_t(index)]);
        save("scene: " + presets::factory()[std::size_t(index)].name, p);
    }
    return list;
}

juce::File fixtureFile(const char* name = "sessions-v2.xml") { return juce::File(LIMINAL_SOURCE_DIR).getChildFile("Tests").getChildFile("fixtures").getChildFile(name); }

} // namespace sessiontest

// Writes the fixture (run once, by the build before a change that must not alter old sessions).
int writeSessionFixtures(const juce::String& path) {
    using namespace sessiontest;
    juce::XmlElement root("LIMINAL_SESSION_FIXTURES");
    root.setAttribute("comment", "States saved by LIMINAL before the instrument library (2026-11-02) and fingerprints of a phrase rendered from them "
                                 "(dB per 250 ms window, left then right; stand-ins, rooms without captures). Tests/SessionTests.h.");
    root.setAttribute("window", window);
    for (const auto& [name, state] : states()) {
        auto* e = root.createNewChildElement("SESSION");
        e->setAttribute("name", name);
        juce::MemoryOutputStream packed;
        {
            juce::GZIPCompressorOutputStream zip(packed, 9);
            zip.write(state.getData(), state.getSize());
        }
        e->setAttribute("state", packed.getMemoryBlock().toBase64Encoding());   // the saved state, gzipped
        e->setAttribute("fingerprint", join(fingerprint(state)));
        std::cout << "fixture: " << name << "\n";
    }
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    return root.writeTo(file) ? 0 : 1;
}

// Re-takes the fingerprints of the fixture's states with today's build, keeping the states (written by
// the build before the library) as they are: for a patch that changes the instruments' sound on purpose
// (2026-11-05: the remembered treatment, the hung-note fixes, Distant Radio). The loading checks
// (every parameter back, every slot its own instrument) still run on the old states.
int refreshSessionFingerprints() {
    using namespace sessiontest;
    auto xml = juce::XmlDocument::parse(fixtureFile());
    if (xml == nullptr) return 1;
    for (auto* e : xml->getChildWithTagNameIterator("SESSION")) {
        juce::MemoryBlock packed, state;
        packed.fromBase64Encoding(e->getStringAttribute("state"));
        {
            juce::MemoryInputStream in(packed, false);
            juce::GZIPDecompressorInputStream unzip(in);
            unzip.readIntoMemoryBlock(state);
        }
        e->setAttribute("fingerprint", join(fingerprint(state)));
        std::cout << "fingerprint: " << e->getStringAttribute("name") << "\n";
    }
    xml->setAttribute("comment", "States saved by LIMINAL before the instrument library (2026-11-02); fingerprints of a phrase rendered from them "
                                 "(dB per 250 ms window, left then right; stand-ins, rooms without captures), re-taken 2026-11-05 after the patch "
                                 "notes changed the instruments' sound on purpose. Tests/SessionTests.h.");
    return xml->writeTo(fixtureFile()) ? 0 : 1;
}

void testSessionFixture(Suite& s, const char* file, const std::string& who);

void testSessions(Suite& s) {
    s.section("old sessions and presets open exactly as before (2026-11-02; LIMINAL's and TAPEWORN's, in VACANCY)");
    // Saved before the instrument library (fingerprints re-taken when the sound changed on purpose).
    testSessionFixture(s, "sessions-v2.xml", "saved before the library");
    // Saved by LIMINAL 0.4 just before the rename: VACANCY must load them unchanged (the state's tag,
    // the parameter IDs), and sound exactly the same (the rename changes no sound: fingerprints as written).
    testSessionFixture(s, "sessions-liminal.xml", "saved by LIMINAL (before the rename to TAPEWORN)");
    // Saved by TAPEWORN just before the rename to VACANCY (2026-11-07): the same, through the build of
    // that day (`LiminalTests --write-session-fixtures`, run by TAPEWORN itself).
    testSessionFixture(s, "sessions-tapeworn.xml", "saved by TAPEWORN (before the rename to VACANCY)");
    // A preset saved with TAPEWORN and one saved with LIMINAL (each in its own folder, with its own tag
    // and extension) are carried over to VACANCY's folder and load.
    for (int which = 0; which < 2; ++which) {
        using namespace sessiontest;
        const auto& from = brand::legacyPresets[which];
        const std::string oldName = from.product;
        LiminalProcessor p;
        const auto old = LiminalProcessor::legacyUserPresetFolder(which);
        const bool hadOld = old.isDirectory();
        old.createDirectory();
        const juce::String name = "ZZ CARRYOVER TEST " + juce::String(from.product);
        const auto legacy = old.getChildFile(name + from.extension);
        const auto carried = LiminalProcessor::userPresetFolder().getChildFile(name + brand::presetExtension);
        carried.deleteFile();
        {   // exactly as LIMINAL 0.4 / TAPEWORN wrote one (PluginProcessor.cpp, saveUserPreset, before each rename)
            juce::XmlElement xml(from.tag);
            xml.setAttribute("name", name);
            xml.setAttribute("stateVersion", 2);
            const std::pair<const char*, double> values[] { { params::environment, 6.0 }, { params::liminality, 0.75 }, { "synth_type", 18.0 }, { params::synthOn, 1.0 } };
            for (const auto& [id, v] : values) {
                auto* e = xml.createNewChildElement("PARAM");
                e->setAttribute("id", id);
                e->setAttribute("value", v);
            }
            xml.writeTo(legacy);
        }
        const auto list = p.userPresets();
        bool listed = false;
        for (const auto& f : list) listed |= f == carried;
        s.check(listed && carried.existsAsFile() && legacy.existsAsFile(), "a preset saved with " + oldName + " is carried over to VACANCY's folder (the old file left in place)",
                carried.getFullPathName().toStdString());
        s.check(carried.getFullPathName().contains("Windward Audio") && carried.getFullPathName().contains("VACANCY"), "VACANCY's presets live in Windward Audio/VACANCY/Presets",
                carried.getFullPathName().toStdString());
        const bool loaded = p.loadUserPreset(carried);
        s.check(loaded && std::lround(p.parameters.getRawParameterValue(params::environment)->load()) == 6 && std::abs(p.parameters.getRawParameterValue(params::liminality)->load() - 0.75f) < 1.0e-5f
                    && std::lround(p.parameters.getRawParameterValue("synth_type")->load()) == 18 && p.presetName() == name,
                oldName + "'s preset loads in VACANCY with its values");
        const bool loadedInPlace = p.loadUserPreset(legacy);   // (an old file loads where it is, too)
        s.check(loadedInPlace, oldName + "'s preset file loads from its old folder too", legacy.getFullPathName().toStdString());
        if (which == 0) {
            const auto saved = p.saveUserPreset("ZZ SAVED TEST");
            auto back = juce::XmlDocument::parse(saved);
            s.check(saved.getFileExtension() == ".vacancypreset" && back != nullptr && back->hasTagName("VACANCY_PRESET"), "new presets are saved as VACANCY presets",
                    saved.getFullPathName().toStdString());
            saved.deleteFile();
        }
        carried.deleteFile();
        legacy.deleteFile();
        if (!hadOld) {
            old.deleteRecursively();
            if (old.getParentDirectory().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0) old.getParentDirectory().deleteRecursively();
        }
    }
}

void testSessionFixture(Suite& s, const char* file, const std::string& who) {
    using namespace sessiontest;
    auto xml = juce::XmlDocument::parse(fixtureFile(file));
    if (!s.check(xml != nullptr, "the fixture of sessions " + who + " is readable", fixtureFile(file).getFullPathName().toStdString())) return;
    int sessions = 0, paramsWrong = 0, slotsWrong = 0, fingerprintsOff = 0;
    double worst = 0;
    std::string worstName;
    for (auto* e : xml->getChildWithTagNameIterator("SESSION")) {
        ++sessions;
        juce::MemoryBlock packed, state;
        packed.fromBase64Encoding(e->getStringAttribute("state"));
        {
            juce::MemoryInputStream in(packed, false);
            juce::GZIPDecompressorInputStream unzip(in);
            unzip.readIntoMemoryBlock(state);
        }
        // Every parameter the old state saved comes back with its value; the slots hold their own instruments.
        {
            LiminalProcessor p;
            p.setStateInformation(state.getData(), int(state.getSize()));
            std::unique_ptr<juce::XmlElement> saved(juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize())));
            for (auto* param : saved->getChildWithTagNameIterator("PARAM")) {
                const auto id = param->getStringAttribute("id");
                auto* now = p.parameters.getRawParameterValue(id);
                if (now == nullptr || std::abs(now->load() - float(param->getDoubleAttribute("value"))) > 1.0e-5f) {
                    if (++paramsWrong < 5) std::cout << "  " << e->getStringAttribute("name") << ": " << id << " changed\n";
                }
            }
            for (int slot = 0; slot < params::instrumentCount; ++slot) {
                long expected = slot;   // (a session from before the library: its own instrument)
                for (auto* param : saved->getChildWithTagNameIterator("PARAM"))
                    if (param->getStringAttribute("id") == params::slotTypeId(slot)) expected = std::lround(param->getDoubleAttribute("value"));
                if (std::lround(p.parameters.getRawParameterValue(params::slotTypeId(slot))->load()) != expected) ++slotsWrong;
            }
        }
        // The same sound.
        const auto now = fingerprint(state);
        const auto then = juce::StringArray::fromTokens(e->getStringAttribute("fingerprint"), " ", "");
        double off = then.size() == int(now.size()) ? 0.0 : 99.0;
        for (std::size_t i = 0; i < now.size() && int(i) < then.size(); ++i) {
            const double a = then[int(i)].getDoubleValue(), b = now[i];
            if (a < -100.0 && b < -100.0) continue;   // silence either way
            off = std::max(off, std::abs(a - b));
        }
        if (off > 0.05) ++fingerprintsOff;
        if (off > worst) { worst = off; worstName = e->getStringAttribute("name").toStdString(); }
    }
    const std::string tag = " (" + who + ")";
    s.check(sessions >= 22, "the fixture holds the default, a moved state and every factory scene" + tag, std::to_string(sessions) + " sessions");
    s.check(paramsWrong == 0, "every parameter an old session saved comes back with its value" + tag, std::to_string(paramsWrong) + " changed");
    s.check(slotsWrong == 0, "in an old session every slot holds the instrument it saved" + tag, std::to_string(slotsWrong) + " slots changed");
    s.check(fingerprintsOff == 0, "old sessions render as before (every 250 ms window within 0.05 dB)" + tag, "worst " + num(worst, 3) + " dB" + (worstName.empty() ? "" : " (" + worstName + ")"));
}

// The AU harness's LIMINAL song (tools/au-harness/liminal-session.state): the raw state LIMINAL 0.4 saved
// for "all six, moved" (Tests/fixtures/sessions-liminal.xml), as an AU host keeps it in a song.
// (2026-11-07: also TAPEWORN's, tools/au-harness/tapeworn-session.state, from sessions-tapeworn.xml.)
int writeLiminalAuState(const juce::String& path, const char* fixture = "sessions-liminal.xml") {
    using namespace sessiontest;
    auto xml = juce::XmlDocument::parse(fixtureFile(fixture));
    if (xml == nullptr) return 1;
    for (auto* e : xml->getChildWithTagNameIterator("SESSION")) {
        if (e->getStringAttribute("name") != "all six, moved") continue;
        juce::MemoryBlock packed, state;
        packed.fromBase64Encoding(e->getStringAttribute("state"));
        juce::MemoryInputStream in(packed, false);
        juce::GZIPDecompressorInputStream unzip(in);
        unzip.readIntoMemoryBlock(state);
        return juce::File::getCurrentWorkingDirectory().getChildFile(path).replaceWithData(state.getData(), state.getSize()) ? 0 : 1;
    }
    return 1;
}
