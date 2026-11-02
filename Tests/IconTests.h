// The twenty instrument icons (2026-11-02, UI/InstrumentIcons): one each, readable small, distinct;
// and the editor following what each slot holds (shelf, tabs, the picker, the controls).
// Included by LiminalTests.cpp after SessionTests.h.

namespace icontest {

const juce::Colour backdrop(0xff141c1a), stroke(0xffe6efe9);

juce::Image iconImage(int instrument, int size) {
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);
    g.fillAll(backdrop);
    g.setColour(stroke);
    ui::drawInstrumentIcon(g, instrument, juce::Rectangle<float>(0, 0, float(size), float(size)).reduced(float(size) * 0.06f));
    return img;
}
// How much of the square is ink (0..1), from the brightness above the backdrop.
double coverage(const juce::Image& img) {
    double sum = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x) sum += std::clamp((img.getPixelAt(x, y).getBrightness() - backdrop.getBrightness()) / 0.8, 0.0, 1.0);
    return sum / double(img.getWidth() * img.getHeight());
}
double difference(const juce::Image& a, const juce::Image& b) {
    double sum = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x) sum += std::abs(a.getPixelAt(x, y).getBrightness() - b.getPixelAt(x, y).getBrightness());
    return sum / double(a.getWidth() * a.getHeight());
}

} // namespace icontest

// The icon sheet for docs/snapshots: every icon at 64, 32, 20 and 16 px with its name, on the viewfinder's glass.
juce::Image iconSheet() {
    using namespace icontest;
    const int cellW = 186, cellH = 118, cols = 5;
    juce::Image sheet(juce::Image::RGB, cellW * cols, cellH * 4 + 34, true);
    juce::Graphics g(sheet);
    g.fillAll(backdrop);
    g.setColour(stroke.withAlpha(0.55f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("LIMINAL  -  THE TWENTY INSTRUMENTS  (64 / 32 / 20 / 16 PX)", 12, 8, cellW * cols - 24, 18, juce::Justification::centredLeft);
    for (int i = 0; i < params::libraryCount; ++i) {
        const int cx = (i % cols) * cellW, cy = 34 + (i / cols) * cellH;
        g.setColour(stroke.withAlpha(0.12f));
        g.drawRect(cx + 4, cy + 2, cellW - 8, cellH - 6, 1);
        g.setColour(stroke);
        ui::drawInstrumentIcon(g, i, juce::Rectangle<float>(float(cx + 12), float(cy + 10), 64.0f, 64.0f));
        ui::drawInstrumentIcon(g, i, juce::Rectangle<float>(float(cx + 88), float(cy + 10), 32.0f, 32.0f));
        ui::drawInstrumentIcon(g, i, juce::Rectangle<float>(float(cx + 128), float(cy + 16), 20.0f, 20.0f));
        ui::drawInstrumentIcon(g, i, juce::Rectangle<float>(float(cx + 156), float(cy + 18), 16.0f, 16.0f));
        g.setColour(stroke.withAlpha(0.85f));
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(juce::String(params::libraryNames[i]).toUpperCase(), cx + 12, cy + 80, cellW - 20, 14, juce::Justification::centredLeft);
        g.setColour(stroke.withAlpha(0.45f));
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(juce::String(i < 6 ? "ORIGINAL" : "NEW") + "  -  " + dsp::library::info(i).shortName, cx + 12, cy + 95, cellW - 20, 12, juce::Justification::centredLeft);
    }
    return sheet;
}

void testIcons(Suite& s) {
    s.section("icons (2026-11-02): one line icon per instrument, readable small, distinct");
    using namespace icontest;
    {
        int sparse = 0, heavy = 0;
        double nearest32 = 1e9, nearest16 = 1e9;
        std::string pair32, pair16;
        std::vector<juce::Image> at32, at16;
        for (int i = 0; i < params::libraryCount; ++i) {
            at32.push_back(iconImage(i, 32));
            at16.push_back(iconImage(i, 16));
            const double c = coverage(at16.back());
            if (c < 0.06) ++sparse;
            if (c > 0.55) ++heavy;
            std::cout << "  [ink at 16 px] " << params::libraryNames[i] << ": " << num(c * 100.0, 0) << " %\n";
        }
        for (int a = 0; a < params::libraryCount; ++a)
            for (int b = a + 1; b < params::libraryCount; ++b) {
                const double d32 = difference(at32[std::size_t(a)], at32[std::size_t(b)]), d16 = difference(at16[std::size_t(a)], at16[std::size_t(b)]);
                if (d32 < nearest32) { nearest32 = d32; pair32 = std::string(params::libraryNames[a]) + " / " + params::libraryNames[b]; }
                if (d16 < nearest16) { nearest16 = d16; pair16 = std::string(params::libraryNames[a]) + " / " + params::libraryNames[b]; }
            }
        s.check(sparse == 0 && heavy == 0, "every icon is readable at 16 px (ink between 6 and 55 % of the square: neither faint nor a blot)",
                std::to_string(sparse) + " too faint, " + std::to_string(heavy) + " too heavy");
        s.check(nearest32 > 0.06, "every icon differs from every other at 32 px (mean brightness difference over 6 %)", pair32 + ": " + num(nearest32 * 100.0, 1) + " %");
        s.check(nearest16 > 0.06, "and at 16 px", pair16 + ": " + num(nearest16 * 100.0, 1) + " %");
        const auto again = iconImage(13, 32);
        s.check(difference(again, at32[13]) == 0.0, "icons are drawn the same every time (paths, no state)");
    }
    // The editor follows what each slot holds.
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) { s.check(false, "the editor opens"); return; }
    lm->showInstruments(true);
    auto& drawer = lm->instrumentDrawer();
    drawer.focus(1);
    drawer.showPicker(true);
    auto& picker = drawer.instrumentPicker();
    s.check(picker.isVisible() && picker.slotShown() == 1, "CHOOSE opens the instrument picker for the slot being edited");
    // Every one of the twenty is reachable in the picker (it scrolls) and loads with a click.
    int unreachable = 0, wrong = 0;
    for (int i = 0; i < params::libraryCount; ++i) {
        drawer.showPicker(true);
        int presses = 0;
        while (!picker.list().rowVisible(i) && presses < 60) {
            picker.keyPressed(juce::KeyPress(i > picker.list().firstVisible() ? juce::KeyPress::downKey : juce::KeyPress::upKey));
            ++presses;
        }
        if (!picker.list().rowVisible(i)) { ++unreachable; continue; }
        const auto at = picker.list().rowBounds(i).getCentre();
        picker.mouseDown(scrolltest::mouseAt(picker, at));
        picker.mouseUp(scrolltest::mouseAt(picker, at));
        drawer.refresh();
        if (std::lround(p.parameters.getRawParameterValue(params::slotTypeId(1))->load()) != i) ++wrong;
    }
    s.check(unreachable == 0 && wrong == 0, "all twenty instruments can be scrolled to in the picker and loaded with a click", std::to_string(unreachable) + " unreachable, " + std::to_string(wrong) + " wrong");
    s.check(!picker.isVisible(), "choosing closes the picker");
    // The page's controls follow the instrument: the vibraphone's sixth control is its Tremolo.
    {
        auto* type = p.parameters.getParameter(params::slotTypeId(1));
        type->setValueNotifyingHost(type->convertTo0to1(11.0f));
        drawer.refresh();
        const auto labels = drawer.controlLabels();
        s.check(labels.contains("TREMOLO") && labels.contains("TONE") && !labels.contains("WARMTH"), "a slot's SOUND page shows the loaded instrument's controls (Elevator Vibraphone: ... TREMOLO)",
                labels.joinIntoString(", ").toStdString());
        type->setValueNotifyingHost(type->convertTo0to1(0.0f));   // the piano in the synth's slot: the piano's own controls, no Distance (the slot has none)
        drawer.refresh();
        const auto piano = drawer.controlLabels();
        s.check(piano.contains("AGE") && piano.contains("MECH. NOISE") && !piano.contains("DISTANCE"), "an original instrument in another slot shows its own controls (Forgotten Piano in the synth's slot)",
                piano.joinIntoString(", ").toStdString());
        type->setValueNotifyingHost(type->convertTo0to1(1.0f));
        drawer.refresh();
        const auto synth = drawer.controlLabels();
        s.check(synth.contains("WARMTH") && synth.contains("PATCH"), "back to its own instrument: the synth's controls again", synth.joinIntoString(", ").toStdString());
    }
    // The picker is cheap to draw (a list of paths), and the shelf repaints only when something changes.
    {
        drawer.showPicker(true);
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 10; ++k) (void) picker.createComponentSnapshot(picker.getLocalBounds(), true, 2.0f);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 10.0;
        s.metric("instrument picker paint (2x, software)", ms, " ms");
        s.check(ms < 25.0, "the picker paints in under 25 ms at 2x (software renderer)", num(ms, 1) + " ms");
        drawer.showPicker(false);
        lm->view().tick();
        lm->view().tick();
        lm->view().tick();
        s.check(!lm->view().instrumentShelf().refresh(), "the shelf does not redraw when nothing changed");
    }
    lm->showInstruments(false);
    p.editorBeingDeleted(editor.get());
}

// docs/snapshots for the library (2026-11-02): the icon sheet (PNG: thin lines), the INSTRUMENTS page,
// the picker open, the main view with new instruments in the slots, the presets with their scrollbar.
int writeLibrarySnapshots(const juce::String& path) {
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    folder.createDirectory();
    const auto save = [&](const juce::Image& image, const juce::String& name) {
        auto file = folder.getChildFile(name + ".jpg");
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::JPEGImageFormat jpeg;
        jpeg.setQuality(0.86f);
        jpeg.writeImageToStream(image, out);
    };
    {
        auto file = folder.getChildFile("instrument-icons.png");
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(iconSheet(), out);
    }
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) return 1;
    auto& view = lm->view();
    p.parameters.getParameter(params::glassOn)->setValueNotifyingHost(1.0f);
    p.parameters.getParameter(params::vhs)->setValueNotifyingHost(0.0f);
    view.placeSelector().choose(0);
    view.characterSelector().choose(0);
    view.animate();
    lm->showInstruments(true);
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "page-instruments");
    lm->instrumentDrawer().focus(1);
    lm->instrumentDrawer().showPicker(true);
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "page-instruments-picker");
    lm->instrumentDrawer().showPicker(false);
    // New instruments in the slots: an old guitar, faded strings, chimes, a voice, the vibraphone, a hum.
    const int hold[] { 6, 7, 12, 13, 11, 14 };
    for (int i = 0; i < params::instrumentCount; ++i) {
        auto* t = p.parameters.getParameter(params::slotTypeId(i));
        t->setValueNotifyingHost(t->convertTo0to1(float(hold[i])));
        p.parameters.getParameter(juce::String(params::instrumentKeys[i]) + "_on")->setValueNotifyingHost(i % 2 == 0 ? 1.0f : 0.0f);
    }
    lm->instrumentDrawer().focus(4);
    view.tick();
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "page-instruments-library");
    lm->showInstruments(false);
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "shelf-library");
    lm->showPresets(true);
    lm->presetDrawer().showCategory(2);
    for (int k = 0; k < 6; ++k) lm->presetDrawer().keyPressed(juce::KeyPress(juce::KeyPress::pageDownKey));
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "page-presets");
    lm->showPresets(false);
    p.editorBeingDeleted(editor.get());
    std::cout << "wrote library snapshots to " << folder.getFullPathName() << "\n";
    return 0;
}
