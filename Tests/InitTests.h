// INIT, undo, and the PRESETS button at every window size (2026-11-06). Included by LiminalTests.cpp
// after ScrollTests.h (mouseAt). Group `init`.
//   INIT resets everything: after a scene preset and every parameter moved at random, INIT leaves the
//   same state as a new instance (every parameter, compared through getStateInformation), keeps the
//   viewing / listening preferences; the host's "Init" program, the INIT row and RESET do the same;
//   every preset load is undoable (UNDO returns the exact state before, REDO the state after);
//   the PRESETS button and the presets drawer are inside the window and clickable at every size,
//   including window shapes a host may force that are not the editor's own.

namespace inittest {

// Every parameter's value (0..1) by ID, as saved in the song.
std::map<juce::String, double> savedValues(LiminalProcessor& p) {
    juce::MemoryBlock block;
    p.getStateInformation(block);
    std::map<juce::String, double> values;
    if (auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize())))
        for (auto* e : xml->getChildWithTagNameIterator("PARAM")) values[e->getStringAttribute("id")] = e->getDoubleAttribute("value");
    return values;
}

bool preference(const juce::String& id) { return id == params::power || id == params::vhs || id == params::vhsWarble; }

// Parameters whose value differs between two saved states (preferences left out); a few names.
int differences(const std::map<juce::String, double>& a, const std::map<juce::String, double>& b, std::string& names, bool withPreferences = false) {
    int n = 0;
    for (const auto& [id, v] : a) {
        if (!withPreferences && preference(id)) continue;
        const auto it = b.find(id);
        if (it == b.end() || std::abs(it->second - v) > 1.0e-6) {
            if (++n <= 6) names += id.toStdString() + " ";
        }
    }
    if (a.size() != b.size()) ++n;
    return n;
}

void scramble(LiminalProcessor& p, unsigned seed) {
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (auto* param : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param)) {
            if (preference(ranged->getParameterID())) continue;
            ranged->setValueNotifyingHost(unit(random));
        }
}

} // namespace inittest

void testInit(Suite& s) {
    using namespace inittest;
    s.section("init: INIT (no preset) resets everything, undoable; PRESETS reachable at every size");
    LiminalProcessor fresh;
    fresh.prepareToPlay(48000.0, 512);
    const auto clean = savedValues(fresh);
    s.check(fresh.presetName() == LiminalProcessor::initName, "a new instance starts at INIT");
    {
        LiminalProcessor p;
        p.prepareToPlay(48000.0, 512);
        const auto scenes = presets::inCategory(presets::Category::scene);
        p.applyPreset(presets::factory()[std::size_t(scenes.back())]);
        scramble(p, 7);
        p.parameters.getParameter(params::vhs)->setValueNotifyingHost(0.0f);   // the preferences, moved
        p.parameters.getParameter(params::power)->setValueNotifyingHost(0.0f);
        const auto before = savedValues(p);
        std::string names;
        s.check(differences(before, clean, names) > 100, "(the scrambled state differs from a new instance in most parameters)", std::to_string(differences(before, clean, names)));
        p.initPreset();
        const auto after = savedValues(p);
        names.clear();
        const int diff = differences(after, clean, names);
        s.check(diff == 0, "INIT: every parameter as in a new instance (state compared: " + std::to_string(clean.size()) + " parameters)", diff == 0 ? std::string("identical") : std::to_string(diff) + " differ: " + names);
        // Each slot its own instrument, LIMINALITY neutral (its default), the default place, effects at their defaults.
        int slotsWrong = 0;
        for (int slot = 0; slot < params::instrumentCount; ++slot)
            if (std::lround(p.parameters.getRawParameterValue(params::slotTypeId(slot))->load()) != slot) ++slotsWrong;
        s.check(slotsWrong == 0, "INIT: every slot holds its default instrument");
        s.check(std::abs(p.parameters.getRawParameterValue(params::liminality)->load() - dsp::lim::macroDefault) < 1.0e-6f, "INIT: LIMINALITY at its neutral default (" + num(dsp::lim::macroDefault * 100.0, 0) + " %)");
        s.check(std::lround(p.parameters.getRawParameterValue(params::environment)->load()) == std::lround(fresh.parameters.getRawParameterValue(params::environment)->load())
                    && std::lround(p.parameters.getRawParameterValue(params::character)->load()) == std::lround(fresh.parameters.getRawParameterValue(params::character)->load()),
                "INIT: the default place and Character");
        s.check(p.presetName() == LiminalProcessor::initName, "INIT: the preset's name is INIT");
        s.check(std::lround(p.parameters.getRawParameterValue(params::vhs)->load()) == 0 && p.parameters.getRawParameterValue(params::power)->load() < 0.5f,
                "INIT keeps the viewing / listening preferences (POWER, VHS look)");
        // Undo: exactly the state before; redo: INIT again.
        s.check(p.canUndoPreset(), "INIT can be undone");
        p.undoPreset();
        names.clear();
        const int undone = differences(savedValues(p), before, names, true);
        s.check(undone == 0 && p.presetName() != LiminalProcessor::initName, "UNDO after INIT: every parameter back as it was", undone == 0 ? std::string("identical") : std::to_string(undone) + " differ: " + names);
        s.check(p.canRedoPreset(), "the INIT that was undone can be redone");
        p.redoPreset();
        names.clear();
        s.check(differences(savedValues(p), clean, names) == 0 && p.presetName() == LiminalProcessor::initName, "REDO: INIT again", names);
        // A factory preset load is undoable too.
        const auto atInit = savedValues(p);
        p.applyPreset(presets::factory()[std::size_t(scenes.front())]);
        p.undoPreset();
        names.clear();
        s.check(differences(savedValues(p), atInit, names, true) == 0 && p.presetName() == LiminalProcessor::initName, "UNDO after a factory preset: the state before it", names);
        // INIT on a clean state changes nothing (and adds nothing to undo).
        LiminalProcessor q;
        q.initPreset();
        s.check(!q.canUndoPreset(), "INIT on a new instance: nothing to undo");
    }
    {   // The host's program 0 ("Init") is the same INIT.
        LiminalProcessor p;
        p.prepareToPlay(48000.0, 512);
        p.setCurrentProgram(3);
        scramble(p, 11);
        p.setCurrentProgram(0);
        std::string names;
        s.check(p.getProgramName(0) == "Init" && differences(savedValues(p), clean, names) == 0, "the host's Init program: the same clean state", names);
    }
    {   // Sound: after INIT the processor plays as a new instance does (the same phrase, the same output).
        Rig a, b;
        a.set(params::liminality, dsp::lim::macroDefault);
        b.p->applyPreset(presets::factory()[std::size_t(presets::inCategory(presets::Category::scene).back())]);
        scramble(*b.p, 5);
        b.render(0.2);
        b.p->initPreset();
        std::vector<Event> events;
        for (int k = 0; k < 8; ++k) events.push_back({ 0.5 + 0.4 * k, on(60 + (k * 5) % 12, 90) });
        for (int k = 0; k < 8; ++k) events.push_back({ 0.8 + 0.4 * k, off(60 + (k * 5) % 12) });
        for (auto& e : events) e.at += a.time;
        auto ya = a.render(5.0, events);
        for (auto& e : events) e.at += b.time - a.time + 5.0;
        auto yb = b.render(5.0, events);
        const double la = windowDb(ya, 48000.0, 0.5, 4.5), lb = windowDb(yb, 48000.0, 0.5, 4.5);
        s.check(std::abs(la - lb) < 1.5 && std::isfinite(lb), "after INIT a phrase sounds at a new instance's level (no leftover of the scene or the scrambled controls)", num(la, 1) + " vs " + num(lb, 1) + " dB");
    }
    // The editor: the INIT row, RESET, UNDO / REDO; the PRESETS button at every window size.
    {
        LiminalProcessor p;
        p.prepareToPlay(48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
        auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
        if (lm == nullptr) { s.check(false, "the editor opens"); return; }
        auto& drawer = lm->presetDrawer();
        lm->showPresets(true);
        for (int category = 0; category < 5; ++category) {
            drawer.showCategory(category);
            if (category < 4) drawer.load(std::min(2, drawer.rows() - 1));
            for (int k = 0; k < 5; ++k) drawer.keyPressed(juce::KeyPress(juce::KeyPress::pageDownKey));   // scrolled away from the top
            scramble(p, unsigned(20 + category));
            const auto at = drawer.initRowArea().getCentre();
            drawer.mouseDown(scrolltest::mouseAt(drawer, at));
            drawer.mouseUp(scrolltest::mouseAt(drawer, at));
            std::string names;
            const char* tab[] { "PLACES", "CHARACTERS", "INSTRUMENTS", "SCENES", "USER" };
            s.check(differences(savedValues(p), clean, names) == 0 && p.presetName() == LiminalProcessor::initName,
                    std::string("the INIT row at the top of ") + tab[category] + " (list scrolled down) loads the clean default", names);
        }
        scramble(p, 99);
        drawer.resetButton().onClick();
        std::string names;
        s.check(differences(savedValues(p), clean, names) == 0, "RESET beside the preset's name: the clean default", names);
        s.check(drawer.resetButton().isVisible() && drawer.resetButton().getX() > 60 && drawer.resetButton().getRight() < drawer.undoButton().getX(),
                "RESET sits right after the name, before UNDO / REDO");
        drawer.undoButton().onClick();
        s.check(differences(savedValues(p), clean, names) > 100, "UNDO (button): the scrambled state is back");
        lm->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));
        names.clear();
        s.check(differences(savedValues(p), clean, names) == 0, "Cmd+Shift+Z: redo (INIT again)", names);
        lm->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0));
        s.check(differences(savedValues(p), clean, names) > 100, "Cmd+Z: undo");
        // Every window size a host may use, its own shapes and others.
        editor->setVisible(true);   // (as a host shows it: a click reaches only visible components)
        const std::pair<int, int> sizes[] { { 816, 612 }, { 960, 720 }, { 1200, 900 }, { 1440, 1080 }, { 1000, 600 }, { 1400, 720 }, { 700, 900 },
                                            { 960, 500 }, { 640, 480 }, { 1920, 1080 }, { 600, 1000 } };
        int hidden = 0, notClickable = 0, cut = 0;
        std::string worst;
        for (const auto& [w, h] : sizes) {
            editor->setSize(w, h);
            lm->showPresets(false);
            auto& button = lm->presetsToggle();
            const auto bounds = editor->getLocalArea(&button, button.getLocalBounds());
            const auto centre = bounds.getCentre();
            if (!editor->getLocalBounds().contains(bounds)) { ++hidden; worst += std::to_string(w) + "x" + std::to_string(h) + " "; }
            if (editor->getComponentAt(centre) != &button) {
                ++notClickable;
                if (auto* c = editor->getComponentAt(centre)) std::cout << "  (at " << w << "x" << h << " the PRESETS button's centre reaches " << typeid(*c).name() << ")\n";
            }
            lm->showPresets(true);
            const auto panel = editor->getLocalArea(&drawer, drawer.getLocalBounds());
            if (!editor->getLocalBounds().contains(panel)) ++cut;
            const auto init = editor->getLocalArea(&drawer, drawer.initRowArea());
            if (editor->getComponentAt(init.getCentre()) != &drawer) ++notClickable;
        }
        s.check(hidden == 0, "the PRESETS button is inside the window at every size (" + std::to_string(std::size(sizes)) + " sizes, other shapes included)", worst);
        s.check(notClickable == 0, "the PRESETS button and the INIT row are what a click there reaches, at every size", std::to_string(notClickable) + " not");
        s.check(cut == 0, "the presets drawer is whole inside the window at every size");
        p.editorBeingDeleted(editor.get());
    }
}

// The renamed editor (2026-11-06) for docs/snapshots: the main view with the VACANCY wordmark (2026-11-07), the
// presets drawer with INIT / RESET / UNDO, and two window shapes a host may give (the view fitted).
int writeRenameSnapshots(const juce::String& path) {
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
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) return 1;
    auto& view = lm->view();
    p.parameters.getParameter(params::vhs)->setValueNotifyingHost(0.0f);
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "vacancy-main");
    // A scene preset loaded, then the drawer: NOW shows it, RESET beside it, UNDO lit.
    p.applyPreset(presets::factory()[std::size_t(presets::inCategory(presets::Category::scene)[2])]);
    lm->showPresets(true);
    lm->presetDrawer().showCategory(3);
    view.tick();
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "vacancy-presets");
    lm->presetDrawer().loadInit();
    view.tick();
    view.animate();
    save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), "vacancy-presets-init");
    for (const auto& [w, h, name] : { std::tuple { 1400, 720, "vacancy-window-wide" }, std::tuple { 700, 900, "vacancy-window-tall" } }) {
        editor->setSize(w, h);
        view.animate();
        save(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), name);
    }
    lm->showPresets(false);
    p.editorBeingDeleted(editor.get());
    std::cout << "wrote the renamed editor's snapshots to " << folder.getFullPathName() << "\n";
    return 0;
}
