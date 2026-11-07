// Real woodwinds (2026-11-06): the listening render. Included by LiminalTests.cpp after LibraryRender.h.
//   LiminalTests --render-woodwinds f.flac
// Each changed instrument (Elevator Vibraphone + flute, Lounge E-Piano + clarinet) on one phrase (a
// melody, then held chords), BEFORE (its woodwind at 0: the instrument exactly as it was) and AFTER (its
// woodwind at the factory presets' 60 %), dry and in three rooms (hotel, mall, school), at the default
// LIMINALITY (40 %); then AFTER dry on the synthesised stand-in (a build without the samples). A cue
// sheet (f.txt) gives the time of every take.

int renderWoodwinds(const juce::String& path) {
    const double rate = 48000.0;
    recordingsReady();
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    const auto phrase = [](double t0) {
        std::vector<Event> e;
        const int melody[] { 64, 67, 72, 71, 69, 67, 65, 64, 62, 64 };
        const double len[] { 0.5, 0.5, 1.0, 0.5, 0.5, 0.5, 0.5, 1.0, 0.5, 1.5 };
        double t = t0;
        for (int i = 0; i < 10; ++i) {
            e.push_back({ t, on(melody[i], 84) });
            e.push_back({ t + len[i] * 0.92, off(melody[i]) });
            t += len[i];
        }
        const int chords[][3] { { 60, 64, 67 }, { 57, 60, 64 }, { 53, 57, 60 } };
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < 3; ++k) {
                e.push_back({ t + 0.3 + 1.6 * c, on(chords[c][k], 76) });
                e.push_back({ t + 0.3 + 1.6 * c + 1.5, off(chords[c][k]) });
            }
        return e;
    };
    struct Take { const char* what; int place; };
    const Take takes[] { { "dry", -1 }, { "hotel", int(dsp::Place::hotel) }, { "mall", int(dsp::Place::mall) }, { "school", int(dsp::Place::school) } };
    Signal out;
    const bool ww = dsp::recordings("ww-flute") != nullptr;
    juce::String cues = "VACANCY - real woodwinds: each changed instrument BEFORE (its woodwind at 0 %: exactly as it was)\n"
                        "and AFTER (its woodwind at 60 %, the factory presets' amount), dry (no room, no ambience) and in\n"
                        "the hotel, the mall and the school (default Character, ambience at -14 dB), LIMINALITY 40 % (default).\n"
                        "Phrase: E4 G4 C5 B4 A4 G4 F4 E4 D4 E4, then the chords C, Am, F held.\n"
                        "Woodwind samples: " + juce::String(ww ? "loaded (the CC0 flute and clarinet)" : "MISSING: every take plays the stand-in") + "\n"
                        + samples::Bank::get().message() + "\n\n";
    const auto add = [&](int which, float amount, int place, bool standIn, const juce::String& label) {
        BypassRecordings bypass(standIn);
        std::unique_ptr<Rig> rig;
        {
            AtLiminalityDefault keep;
            if (place < 0) rig = limtest::dryRig();
            else {
                rig = rigIn(place);
                rig->set(params::ambience, -14.0f);
            }
        }
        const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
        for (int i = 0; i < params::instrumentCount; ++i) rig->set(ons[i], i == 1 ? 1.0f : 0.0f);
        rig->set(params::slotTypeId(1).toRawUTF8(), float(which));
        rig->set(params::libId(1, params::wind::amount).toRawUTF8(), amount);
        rig->render(0.4);
        const double at = double(out.first.size()) / rate;
        const auto x = rig->render(13.0, phrase(rig->time + 0.1));
        out.first.insert(out.first.end(), x.first.begin(), x.first.end());
        out.second.insert(out.second.end(), x.second.begin(), x.second.end());
        out.first.insert(out.first.end(), std::size_t(0.8 * rate), 0.0f);
        out.second.insert(out.second.end(), std::size_t(0.8 * rate), 0.0f);
        cues << juce::String(int(at) / 60) << ":" << juce::String(int(at) % 60).paddedLeft('0', 2) << "  " << label << "\n";
    };
    for (const auto& c : woodwindtest::changed) {
        for (const auto& take : takes) {
            add(c.which, 0.0f, take.place, false, juce::String(c.name) + " - BEFORE (no " + c.blend + ") - " + take.what);
            add(c.which, 0.6f, take.place, false, juce::String(c.name) + " - AFTER (+ " + c.blend + " 60 %) - " + take.what + (ww ? "" : "  (stand-in)"));
        }
        add(c.which, 0.6f, -1, true, juce::String(c.name) + " - AFTER (+ " + c.blend + " 60 %) - dry, the synthesised stand-in (a build without the samples)");
    }
    double peak = 0;
    for (const auto* ch : { &out.first, &out.second })
        for (float v : *ch) peak = std::max(peak, double(std::abs(v)));
    cues << "\npeak " << juce::String(toDb(peak), 1) << " dBFS\n";
    if (!writeAudio(out, file)) return 1;
    file.withFileExtension("txt").replaceWithText(cues);
    std::cout << cues;
    return 0;
}

// The instrument pages with the woodwind controls (docs/snapshots/woodwind-*.jpg), and a check that they
// are laid out inside the drawer, one under another, none overlapping (also run by the `woodwind` group).
struct WoodwindPages { int shown = 0, overlapping = 0, outside = 0; juce::StringArray labels[2]; };
WoodwindPages woodwindPages(const juce::File* folder) {
    WoodwindPages r;
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) return r;
    auto& view = lm->view();
    p.parameters.getParameter(params::vhs)->setValueNotifyingHost(0.0f);
    view.placeSelector().choose(int(dsp::Place::hotel));
    view.characterSelector().choose(0);
    const struct { int slot, which; const char* file; } pages[] { { 2, 11, "woodwind-vibraphone-flute" }, { 1, 10, "woodwind-epiano-clarinet" } };
    for (int k = 0; k < 2; ++k) {
        const auto& pg = pages[k];
        auto* t = p.parameters.getParameter(params::slotTypeId(pg.slot));
        t->setValueNotifyingHost(t->convertTo0to1(float(pg.which)));
        p.parameters.getParameter(juce::String(params::instrumentKeys[pg.slot]) + "_on")->setValueNotifyingHost(1.0f);
        auto* w = p.parameters.getParameter(params::libId(pg.slot, params::wind::amount));
        w->setValueNotifyingHost(0.6f);
        lm->showInstruments(true);
        auto& drawer = lm->instrumentDrawer();
        drawer.refresh();
        drawer.focus(pg.slot);
        view.tick();
        view.animate();
        r.labels[k] = drawer.controlLabels();
        std::vector<juce::Rectangle<int>> boxes;
        for (auto* c : drawer.getChildren())
            if (c->isVisible() && dynamic_cast<LevelControl*>(c) != nullptr) {
                ++r.shown;
                const auto b = c->getBounds();
                if (!drawer.getLocalBounds().contains(b)) ++r.outside;
                for (const auto& o : boxes) if (o.intersects(b)) ++r.overlapping;
                boxes.push_back(b);
            }
        if (folder != nullptr) {
            auto file = folder->getChildFile(juce::String(pg.file) + ".jpg");
            file.deleteFile();
            juce::FileOutputStream out(file);
            juce::JPEGImageFormat jpeg;
            jpeg.setQuality(0.86f);
            jpeg.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), out);
        }
    }
    lm->showInstruments(false);
    p.editorBeingDeleted(editor.get());
    return r;
}
int writeWoodwindSnapshots(const juce::String& path) {
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    folder.createDirectory();
    const auto r = woodwindPages(&folder);
    std::cout << "vibraphone page: " << r.labels[0].joinIntoString(", ") << "\ne-piano page: " << r.labels[1].joinIntoString(", ")
              << "\noverlapping " << r.overlapping << ", outside " << r.outside << "\n";
    return 0;
}
