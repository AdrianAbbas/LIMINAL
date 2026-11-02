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

juce::File fixtureFile() { return juce::File(LIMINAL_SOURCE_DIR).getChildFile("Tests").getChildFile("fixtures").getChildFile("sessions-v2.xml"); }

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

void testSessions(Suite& s) {
    s.section("old sessions and presets open exactly as before (2026-11-02)");
    using namespace sessiontest;
    auto xml = juce::XmlDocument::parse(fixtureFile());
    if (!s.check(xml != nullptr, "the fixture of sessions saved before the library is readable", fixtureFile().getFullPathName().toStdString())) return;
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
            for (int slot = 0; slot < params::instrumentCount; ++slot)
                if (std::lround(p.parameters.getRawParameterValue(params::slotTypeId(slot))->load()) != slot) ++slotsWrong;
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
    s.check(sessions >= 22, "the fixture holds the default, a moved state and every factory scene", std::to_string(sessions) + " sessions");
    s.check(paramsWrong == 0, "every parameter an old session saved comes back with its value", std::to_string(paramsWrong) + " changed");
    s.check(slotsWrong == 0, "in an old session every slot holds its own instrument (piano, synth, glass, choir, music box, drone)", std::to_string(slotsWrong) + " slots changed");
    s.check(fingerprintsOff == 0, "old sessions render as before (every 250 ms window within 0.05 dB)", "worst " + num(worst, 3) + " dB" + (worstName.empty() ? "" : " (" + worstName + ")"));
}
