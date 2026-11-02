// The preset browser's scrolling (2026-11-02): every preset reachable by the wheel / trackpad, the
// scrollbar and the keys, at every window size, and nothing (a repaint, the editor's timer, loading a
// preset) jumping the list back up. Included by LiminalTests.cpp after FrameTests.h.

namespace scrolltest {

juce::MouseEvent mouseAt(juce::Component& c, juce::Point<int> p, bool dragged = false) {
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto t = juce::Time::getCurrentTime();
    return juce::MouseEvent(source, p.toFloat(), {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, t, p.toFloat(), t, 1, dragged);
}
juce::MouseWheelDetails wheelOf(float deltaY, bool smooth = true, bool inertial = false) {
    juce::MouseWheelDetails w;
    w.deltaX = 0.0f;
    w.deltaY = deltaY;
    w.isReversed = false;
    w.isSmooth = smooth;
    w.isInertial = inertial;
    return w;
}

// A trackpad flick: small deltas, the zero-delta events a gesture begins and ends with, then momentum.
template <class Send>
void trackpadFlick(Send send, float sign) {
    send(wheelOf(0.0f));
    for (int k = 0; k < 12; ++k) send(wheelOf(sign * 0.012f * float(1 + k % 3)));
    send(wheelOf(0.0f));
    for (int k = 0; k < 20; ++k) send(wheelOf(sign * 0.02f * std::exp(-0.15f * float(k)), true, true));
    send(wheelOf(0.0f, true, true));
}

} // namespace scrolltest

void testScroll(Suite& s) {
    s.section("scrolling lists (2026-11-02): presets reachable by wheel, bar and keys");
    using namespace scrolltest;
    {   // The list's arithmetic.
        ui::ScrollList list;
        list.setGeometry({ 16, 60, 368, 220 }, 22, 33);
        s.check(list.maxOffset() == 33 * 22 - 220, "content height is rows x row height; the end puts the last row at the bottom", num(list.maxOffset(), 0) + " px");
        int moved = 0;
        for (int k = 0; k < 50; ++k) moved += list.wheel(wheelOf(0.0f)) ? 1 : 0;
        s.check(moved == 0 && list.position() == 0.0f, "a wheel event with no vertical movement (a trackpad gesture's start / end) never moves the list");
        list.setOffset(200.0f);
        for (int k = 0; k < 50; ++k) list.wheel(wheelOf(0.0f, true, true));
        s.check(list.position() == 200.0f, "nor scrolls it back up from the middle (the old bug: deltaY == 0 counted as 'up')");
        for (int k = 0; k < 400; ++k) list.wheel(wheelOf(-0.01f));
        s.check(list.position() == list.maxOffset() && list.rowVisible(32), "a stream of small trackpad deltas reaches the end; the last row is fully visible");
        const auto last = list.rowBounds(32);
        s.check(list.rowAt(last.getCentre()) == 32, "the last row is under the mouse where it is drawn");
        list.setOffset(0);
        s.check(list.mouseDown(list.thumbArea().getCentre()) && list.draggingThumb(), "the scrollbar's thumb can be grabbed");
        list.mouseDrag({ list.thumbArea().getCentreX(), 2000 });
        list.mouseUp();
        s.check(list.position() == list.maxOffset() && list.thumbArea().getBottom() == list.barArea().getBottom(), "dragging the thumb to the bottom reaches the last row (the thumb sits at the bar's end)");
        list.setOffset(0);
        int pages = 0;
        while (list.position() < list.maxOffset() && pages < 100) { list.mouseDown({ list.barArea().getCentreX(), list.barArea().getBottom() - 2 }); list.mouseUp(); ++pages; }
        s.check(list.position() == list.maxOffset(), "clicking the track below the thumb pages down to the end", std::to_string(pages) + " pages");
        list.key(juce::KeyPress(juce::KeyPress::homeKey));
        s.check(list.position() == 0.0f, "Home goes to the top");
        int keys = 0;
        while (list.position() < list.maxOffset() && keys < 100) { list.key(juce::KeyPress(juce::KeyPress::downKey)); ++keys; }
        s.check(list.position() == list.maxOffset() && keys == 33 - 10, "the down arrow moves one row at a time to the last", std::to_string(keys) + " presses");
        list.key(juce::KeyPress(juce::KeyPress::homeKey));
        list.key(juce::KeyPress(juce::KeyPress::endKey));
        s.check(list.rowVisible(32), "End shows the last row");
        list.key(juce::KeyPress(juce::KeyPress::pageUpKey));
        s.check(list.position() < list.maxOffset() && list.position() > 0.0f, "Page Up moves back a page");
        list.setRows(5);
        s.check(!list.scrollable() && list.position() == 0.0f && list.rowVisible(4), "a short list does not scroll (and the offset follows the shorter content)");
        list.setRows(40);
        list.ensureVisible(39);
        s.check(list.rowVisible(39), "ensureVisible() shows a chosen row");
    }
    // The real browser in the editor, at 85 / 100 / 150 %: every group, every row reached and clicked.
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) { s.check(false, "the editor opens"); return; }
    auto& drawer = lm->presetDrawer();
    for (const float scale : { 0.85f, 1.0f, 1.5f }) {
        editor->setSize(juce::roundToInt(LiminalEditor::defaultWidth * scale), juce::roundToInt(LiminalEditor::defaultHeight * scale));
        lm->showPresets(true);
        const auto tag = juce::String(juce::roundToInt(scale * 100)).toStdString() + " %";
        int unreachable = 0, wrongLoads = 0, jumps = 0, offscreen = 0, rowsChecked = 0;
        for (int category = 0; category < 4; ++category) {
            drawer.showCategory(category);
            const int total = drawer.rows();
            const auto& list = drawer.list();
            // The whole list is inside the window at this size.
            const auto view = editor->getLocalArea(&drawer, list.viewArea());
            if (!editor->getLocalBounds().contains(view)) ++offscreen;
            // Down by trackpad flicks until it stops moving; repaint and tick the editor between flicks.
            float before = -1;
            int flicks = 0;
            while (list.position() != before && flicks < 200) {
                before = list.position();
                trackpadFlick([&](const juce::MouseWheelDetails& w) { drawer.mouseWheelMove(mouseAt(drawer, list.viewArea().getCentre()), w); }, -1.0f);
                const float now = list.position();
                lm->view().tick();
                (void) editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
                if (list.position() != now) ++jumps;
                ++flicks;
            }
            if (!list.rowVisible(total - 1)) ++unreachable;
            // Click every visible row at the bottom (the last ones), as the mouse would.
            for (int row = std::max(0, total - 3); row < total; ++row) {
                const auto at = list.rowBounds(row).getCentre();
                if (drawer.rowAt(at) != row) { ++unreachable; continue; }
                const float kept = list.position();
                drawer.mouseDown(mouseAt(drawer, at));
                drawer.mouseUp(mouseAt(drawer, at));
                lm->view().tick();
                if (list.position() != kept) ++jumps;   // loading a preset must not move the list
                if (drawer.rowAt(at) != row) ++jumps;
                const auto scenes = presets::inCategory(presets::Category(category));
                if (p.presetName() != presets::factory()[std::size_t(scenes[std::size_t(row)])].name) ++wrongLoads;
                // On screen too: the row's centre maps into the window.
                if (!editor->getLocalBounds().contains(editor->getLocalPoint(&drawer, at))) ++offscreen;
                ++rowsChecked;
            }
            // Every row: reached by the keys (Home, then down) and by dragging the bar.
            drawer.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
            int seen = 0;
            for (int row = 0; row < total; ++row) {
                int presses = 0;
                while (!list.rowVisible(row) && presses < 200) { drawer.keyPressed(juce::KeyPress(juce::KeyPress::downKey)); ++presses; }
                seen += list.rowVisible(row) ? 1 : 0;
            }
            if (seen != total) ++unreachable;
            if (list.scrollable()) {
                drawer.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
                const auto thumb = list.thumbArea().getCentre();
                drawer.mouseDown(mouseAt(drawer, thumb));
                drawer.mouseDrag(mouseAt(drawer, { thumb.x, drawer.getHeight() + 400 }, true));
                drawer.mouseUp(mouseAt(drawer, { thumb.x, drawer.getHeight() + 400 }, true));
                if (!list.rowVisible(total - 1)) ++unreachable;
            }
        }
        s.check(unreachable == 0, "every preset in every group reachable by the trackpad, the keys and the scrollbar at " + tag, std::to_string(unreachable) + " not reached");
        s.check(wrongLoads == 0 && rowsChecked >= 12, "clicking each of the last rows loads that very preset at " + tag, std::to_string(rowsChecked) + " rows clicked, " + std::to_string(wrongLoads) + " wrong");
        s.check(jumps == 0, "the list never jumps back (repaints, the editor's timer, loading a preset) at " + tag, std::to_string(jumps) + " jumps");
        s.check(offscreen == 0, "the list and its last rows are inside the window at " + tag);
        // Another tab starts at its top; coming back to a list is a fresh list (the only reset).
        drawer.showCategory(3);
        s.check(drawer.list().position() == 0.0f, "a new tab starts at the top of its list");
        lm->showPresets(false);
    }
    editor->setSize(LiminalEditor::defaultWidth, LiminalEditor::defaultHeight);
    p.editorBeingDeleted(editor.get());
}
