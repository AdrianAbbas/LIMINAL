// Old sessions of the effect open exactly as before (2026-11-07: TAPEWORN FX became VACANCY FX).
// Tests/fixtures/sessions-tapeworn-fx.xml was written by TAPEWORN FX itself, the build just before the
// rename (`TapewornFXTests --write-session-fixtures Tests/fixtures/sessions-tapeworn-fx.xml`): the
// default state, a moved state with A / B, and every factory preset, each with the saved state and a
// fingerprint of the test mix rendered through it (RMS per 250 ms window, per channel, in dB; rooms
// without captures, so it is pure code, the same on every machine). Group `sessions` loads each into
// today's build: every parameter comes back, the preset name and A / B, and the same sound.
// Included by FxTests.cpp after the Rig and the materials.

namespace fxsession {

constexpr double window = 0.25;

std::vector<float> fingerprint(const juce::MemoryBlock& state) {
    const bool capturesWere = dsp::CaptureBank::disabled();
    dsp::CaptureBank::disabled() = true;
    std::vector<float> out;
    {
        Rig rig;
        rig.p->setStateInformation(state.getData(), int(state.getSize()));
        rig.prepare();
        const auto sig = rig.run(materials()[2].signal, 1.0);
        const auto w = std::size_t(window * rig.rate);
        for (const auto* ch : { &sig.first, &sig.second })
            for (std::size_t i = 0; i + w <= ch->size(); i += w) {
                double sum = 0;
                for (std::size_t k = i; k < i + w; ++k) sum += double((*ch)[k]) * (*ch)[k];
                out.push_back(float(20.0 * std::log10(std::max(std::sqrt(sum / double(w)), 1.0e-9))));
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

std::vector<std::pair<juce::String, juce::MemoryBlock>> states() {
    std::vector<std::pair<juce::String, juce::MemoryBlock>> list;
    const auto save = [&](const juce::String& name, FxProcessor& p) {
        juce::MemoryBlock m;
        p.getStateInformation(m);
        list.push_back({ name, m });
    };
    {
        FxProcessor p;
        save("default", p);
    }
    {   // many controls moved, A / B with a different preset on each
        FxProcessor p;
        const auto set = [&](const juce::String& id, float v) {
            if (auto* q = p.parameters.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v));
        };
        p.applyPreset(fx::presets::factory()[2]);
        p.showAB(1);
        p.applyPreset(fx::presets::factory()[9]);
        set(fxp::environment, 6.0f);
        set(fxp::character, 2.0f);
        set(fxp::liminality, 0.8f);
        set(fxp::mix, 0.7f);
        save("moved, A / B", p);
    }
    for (std::size_t i = 0; i < fx::presets::factory().size(); ++i) {
        FxProcessor p;
        p.applyPreset(fx::presets::factory()[i]);
        save("preset: " + fx::presets::factory()[i].name, p);
    }
    return list;
}

juce::File fixtureFile() { return juce::File(LIMINAL_SOURCE_DIR).getChildFile("Tests").getChildFile("fixtures").getChildFile("sessions-tapeworn-fx.xml"); }

juce::MemoryBlock unpack(const juce::XmlElement& e) {
    juce::MemoryBlock packed, state;
    packed.fromBase64Encoding(e.getStringAttribute("state"));
    juce::MemoryInputStream in(packed, false);
    juce::GZIPDecompressorInputStream unzip(in);
    unzip.readIntoMemoryBlock(state);
    return state;
}

} // namespace fxsession

int writeSessionFixtures(const juce::String& path) {
    using namespace fxsession;
    juce::XmlElement root("TAPEWORN_FX_SESSION_FIXTURES");
    root.setAttribute("comment", "States saved by TAPEWORN FX just before it was renamed VACANCY FX (2026-11-07), and fingerprints of the test mix rendered "
                                 "through them (dB per 250 ms window, left then right; rooms without captures). Tests/FxSessionTests.h, group sessions.");
    root.setAttribute("window", window);
    for (const auto& [name, state] : states()) {
        auto* e = root.createNewChildElement("SESSION");
        e->setAttribute("name", name);
        juce::MemoryOutputStream packed;
        {
            juce::GZIPCompressorOutputStream zip(packed, 9);
            zip.write(state.getData(), state.getSize());
        }
        e->setAttribute("state", packed.getMemoryBlock().toBase64Encoding());
        e->setAttribute("fingerprint", join(fingerprint(state)));
        std::cout << "fixture: " << name << "\n";
    }
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    return root.writeTo(file) ? 0 : 1;
}

// The AU harness's TAPEWORN FX song (tools/au-harness/tapeworn-fx-session.state): the raw state of
// "moved, A / B" from the fixture, as an AU host keeps it in a song.
int writeAuState(const juce::String& path) {
    using namespace fxsession;
    auto xml = juce::XmlDocument::parse(fixtureFile());
    if (xml == nullptr) return 1;
    for (auto* e : xml->getChildWithTagNameIterator("SESSION")) {
        if (e->getStringAttribute("name") != "moved, A / B") continue;
        const auto state = unpack(*e);
        return juce::File::getCurrentWorkingDirectory().getChildFile(path).replaceWithData(state.getData(), state.getSize()) ? 0 : 1;
    }
    return 1;
}

void testSessions(Suite& s) {
    using namespace fxsession;
    s.section("sessions (TAPEWORN FX's, in VACANCY FX)");
    auto xml = juce::XmlDocument::parse(fixtureFile());
    if (!s.check(xml != nullptr, "the fixture of sessions saved by TAPEWORN FX is readable", fixtureFile().getFullPathName().toStdString())) return;
    int sessions = 0, paramsWrong = 0, extrasWrong = 0, fingerprintsOff = 0;
    double worst = 0;
    std::string worstName;
    for (auto* e : xml->getChildWithTagNameIterator("SESSION")) {
        ++sessions;
        const auto state = unpack(*e);
        {
            FxProcessor p;
            p.setStateInformation(state.getData(), int(state.getSize()));
            std::unique_ptr<juce::XmlElement> saved(juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize())));
            if (saved == nullptr) { ++paramsWrong; continue; }
            for (auto* param : saved->getChildWithTagNameIterator("PARAM")) {
                const auto id = param->getStringAttribute("id");
                auto* now = p.parameters.getRawParameterValue(id);
                if (now == nullptr || std::abs(now->load() - float(param->getDoubleAttribute("value"))) > 1.0e-5f) {
                    if (++paramsWrong < 5) std::cout << "  " << e->getStringAttribute("name") << ": " << id << " changed\n";
                }
            }
            if (p.presetName() != saved->getStringAttribute("preset") || p.abShown() != saved->getIntAttribute("ab")) ++extrasWrong;
            // the state written again is the state read (nothing renamed inside it)
            juce::MemoryBlock again;
            p.getStateInformation(again);
            std::unique_ptr<juce::XmlElement> back(juce::AudioProcessor::getXmlFromBinary(again.getData(), int(again.getSize())));
            if (back == nullptr || !back->isEquivalentTo(saved.get(), false)) ++extrasWrong;
        }
        const auto now = fingerprint(state);
        const auto then = juce::StringArray::fromTokens(e->getStringAttribute("fingerprint"), " ", "");
        double off = then.size() == int(now.size()) ? 0.0 : 99.0;
        for (std::size_t i = 0; i < now.size() && int(i) < then.size(); ++i) {
            const double a = then[int(i)].getDoubleValue(), b = now[i];
            if (a < -100.0 && b < -100.0) continue;
            off = std::max(off, std::abs(a - b));
        }
        if (off > 0.05) ++fingerprintsOff;
        if (off > worst) { worst = off; worstName = e->getStringAttribute("name").toStdString(); }
    }
    s.check(sessions >= 3 + 20, "the fixture holds the default, a moved state and every factory preset", std::to_string(sessions) + " sessions");
    s.check(paramsWrong == 0, "every parameter a TAPEWORN FX session saved comes back with its value", std::to_string(paramsWrong) + " changed");
    s.check(extrasWrong == 0, "the preset's name and A / B come back, and the state saved again is identical", std::to_string(extrasWrong) + " differ");
    s.check(fingerprintsOff == 0, "TAPEWORN FX sessions render as before (every 250 ms window within 0.05 dB)",
            "worst " + std::to_string(worst) + " dB" + (worstName.empty() ? "" : " (" + worstName + ")"));
}
