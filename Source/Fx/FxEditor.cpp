#include "Fx/FxEditor.h"
#include <cmath>

namespace liminal::fx {

namespace {
using ui::style::ink;
using ui::style::font;
void setValue(juce::AudioProcessorValueTreeState& state, const juce::String& id, float value) {
    if (auto* p = state.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    }
}
int valueOf(juce::AudioProcessorValueTreeState& state, const juce::String& id) { return int(std::lround(state.getRawParameterValue(id)->load())); }
} // namespace

void FxPage::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setFont(font(9.5f));
    g.setColour(ink.withAlpha(0.45f));
    g.drawText(heading, area.reduced(16, 10).removeFromTop(14), juce::Justification::centredLeft);
}

//==============================================================================
FxPlaceDrawer::FxPlaceDrawer(juce::AudioProcessorValueTreeState& s) : FxPage("PLACE  -  WHERE THE SOUND IS HEARD"), state(s) {
    const std::pair<const char*, const char*> ids[] { { fxp::space, "SPACE" }, { fxp::ambience, "ROOM SOUND" }, { fxp::farAway, "FAR AWAY" },
                                                      { fxp::presence, "PRESENCE" }, { fxp::scale, "SCALE" }, { fxp::activity, "ACTIVITY" },
                                                      { fxp::unreal, "UNREAL" }, { fxp::evolution, "EVOLUTION" } };
    for (std::size_t i = 0; i < controls.size(); ++i) {
        controls[i] = std::make_unique<LevelControl>(state, ids[i].first, ids[i].second);
        addAndMakeVisible(*controls[i]);
    }
    controls[1]->slider.setTooltip("ROOM SOUND: the place's own hum and rare events, heard only while something plays (they fade out a few seconds after)");
    controls[0]->slider.setTooltip("SPACE: how much of the room is heard (with Intensity)");
    const char* names[] { "VHS OFF", "LIGHT", "HEAVY" };
    for (int i = 0; i < 3; ++i) {
        vhs[std::size_t(i)].setButtonText(names[i]);
        vhs[std::size_t(i)].setClickingTogglesState(false);
        vhs[std::size_t(i)].onClick = [this, i] { setValue(state, fxp::vhs, float(i)); repaint(); };
        addAndMakeVisible(vhs[std::size_t(i)]);
    }
    refresh();
}

juce::Rectangle<int> FxPlaceDrawer::characterBox(int character) const {
    const int w = (getWidth() - 32 - 2 * 6) / dsp::characterCount;
    return { 16 + character * (w + 6), row(dsp::placeCount - 1).getBottom() + 22, w, 22 };
}

bool FxPlaceDrawer::refresh() {
    const int now = valueOf(state, fxp::environment), ch = valueOf(state, fxp::character);
    const int v = valueOf(state, fxp::vhs);
    for (int i = 0; i < 3; ++i) vhs[std::size_t(i)].setToggleState(i == v, juce::dontSendNotification);
    if (now == selected && ch == selectedCharacter) return false;
    selected = now;
    selectedCharacter = ch;
    repaint();
    return true;
}

void FxPlaceDrawer::choose(int place) {
    setValue(state, fxp::environment, float(place));
    refresh();
    if (onChoose) onChoose();
}

void FxPlaceDrawer::chooseCharacter(int character) {
    setValue(state, fxp::character, float(character));
    refresh();
    if (onChoose) onChoose();
}

void FxPlaceDrawer::paint(juce::Graphics& g) {
    FxPage::paint(g);
    for (int p = 0; p < dsp::placeCount; ++p) {
        const auto r = row(p).toFloat();
        if (p == selected || p == hover) {
            g.setColour(ink.withAlpha(p == selected ? 0.14f : 0.07f));
            g.fillRoundedRectangle(r, 3.0f);
        }
        g.setColour(ink.withAlpha(p == selected ? 1.0f : 0.75f));
        g.setFont(font(10.5f, p == selected));
        g.drawText(placeInfo[p].shortName, r.reduced(8, 0), juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(0.45f));
        g.setFont(font(9.0f));
        g.drawText(juce::String(placeInfo[p].summary).toUpperCase(), r.reduced(8, 0), juce::Justification::centredRight);
    }
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.0f));
    g.drawText("CHARACTER", 16, characterBox(0).getY() - 15, 200, 12, juce::Justification::centredLeft);
    const int place = std::clamp(selected, 0, dsp::placeCount - 1);
    for (int c = 0; c < dsp::characterCount; ++c) {
        const auto r = characterBox(c).toFloat();
        g.setColour(ink.withAlpha(c == selectedCharacter ? 0.18f : c == hoverCharacter ? 0.09f : 0.04f));
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(ink.withAlpha(c == selectedCharacter ? 0.55f : 0.2f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        g.setColour(ink.withAlpha(c == selectedCharacter ? 1.0f : 0.7f));
        g.setFont(font(9.5f, c == selectedCharacter));
        g.drawFittedText(dsp::character(place, c).name, r.toNearestInt().reduced(4, 0), juce::Justification::centred, 1, 0.7f);
    }
    g.setColour(ink.withAlpha(0.55f));
    g.setFont(font(9.0f));
    g.drawFittedText(juce::String(dsp::character(place, std::clamp(selectedCharacter, 0, 2)).mood).toUpperCase(),
                     characterBox(0).withRight(characterBox(2).getRight()).translated(0, 26).withHeight(14), juce::Justification::centredLeft, 1, 0.8f);
}

void FxPlaceDrawer::resized() {
    auto area = juce::Rectangle<int>(16, characterBox(0).getBottom() + 26, getWidth() - 32, getHeight() - characterBox(0).getBottom() - 26 - 40);
    const int half = (area.getWidth() - 14) / 2;
    const int rowsNeeded = int(controls.size() + 1) / 2;
    const int h = std::max(30, area.getHeight() / rowsNeeded);
    for (std::size_t i = 0; i < controls.size(); ++i) {
        const int col = int(i % 2), line = int(i / 2);
        controls[i]->setBounds(area.getX() + col * (half + 14), area.getY() + line * h, half, 34);
    }
    auto v = juce::Rectangle<int>(16, getHeight() - 34, getWidth() - 32, 22);
    const int w = v.getWidth() / 3;
    for (auto& b : vhs) b.setBounds(v.removeFromLeft(w).reduced(2, 0));
}

void FxPlaceDrawer::mouseMove(const juce::MouseEvent& e) {
    int h = -1, hc = -1;
    for (int p = 0; p < dsp::placeCount; ++p)
        if (row(p).contains(e.getPosition())) h = p;
    for (int c = 0; c < dsp::characterCount; ++c)
        if (characterBox(c).contains(e.getPosition())) hc = c;
    setMouseCursor(h >= 0 || hc >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover || hc != hoverCharacter) {
        hover = h;
        hoverCharacter = hc;
        repaint();
    }
}

void FxPlaceDrawer::mouseExit(const juce::MouseEvent&) {
    hover = hoverCharacter = -1;
    repaint();
}

void FxPlaceDrawer::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int p = 0; p < dsp::placeCount; ++p)
        if (row(p).contains(e.getPosition())) { choose(p); return; }
    for (int c = 0; c < dsp::characterCount; ++c)
        if (characterBox(c).contains(e.getPosition())) { chooseCharacter(c); return; }
}

//==============================================================================
FxLiminalityDrawer::FxLiminalityDrawer(juce::AudioProcessorValueTreeState& state) : FxPage("LIMINALITY  -  UNSTABLE, WORN, HALF-REMEMBERED") {
    const char* tips[dsp::lim::count] {
        "A subtle constant pitch shift (flat, like a slightly slow tape)",
        "The pitch wanders away over seconds and comes back",
        "Tape wow and flutter",
        "Each side wavers on its own, seasick; a slow swell and a faint chorus after each attack",
        "Faint delayed, slightly detuned ghost copies",
        "Tape / VHS degradation: lost highs and lows, saturation, hiss, crackle, dropouts",
        "The room and echo smeared and jittered in time",
        "Slow fluctuation of level and tone",
        "Occasional short pitch sags, most often on an attack",
        "Now and then the sound fades away (and darkens), then returns",
    };
    for (int k = 0; k < dsp::lim::count; ++k) {
        controls[std::size_t(k)] = std::make_unique<LevelControl>(state, fxp::limId(k).toRawUTF8(), juce::String(dsp::lim::names[k]).toUpperCase());
        controls[std::size_t(k)]->slider.setTooltip(tips[k]);
        addAndMakeVisible(*controls[std::size_t(k)]);
    }
}

void FxLiminalityDrawer::paint(juce::Graphics& g) {
    FxPage::paint(g);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.0f));
    g.drawFittedText("EACH CONTROL'S AMOUNT, SCALED BY THE BIG LIMINALITY KNOB (40 % = AS SET HERE, 100 % = 2.5 X) AND NUDGED BY THE PLACE",
                     juce::Rectangle<int>(16, getHeight() - 36, getWidth() - 32, 26), juce::Justification::topLeft, 2, 0.8f);
}

void FxLiminalityDrawer::resized() {
    const int top = 34, half = (getWidth() - 32 - 14) / 2;
    const int h = std::max(34, (getHeight() - top - 44) / 5);
    for (int k = 0; k < dsp::lim::count; ++k) controls[std::size_t(k)]->setBounds(16 + (k % 2) * (half + 14), top + (k / 2) * h, half, 34);
}

//==============================================================================
FxPresetDrawer::FxPresetDrawer(FxProcessor& p) : FxPage("PRESETS"), processor(p) {
    for (int i = 0; i < presets::sourceCount; ++i) {
        tabs[std::size_t(i)].setButtonText(presets::sourceNames[i]);
        tabs[std::size_t(i)].onClick = [this, i] { showSource(i); };
        addAndMakeVisible(tabs[std::size_t(i)]);
    }
    nameField.setTextToShowWhenEmpty("NAME YOUR PRESET", ink.withAlpha(0.4f));
    nameField.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x2207100f));
    nameField.setColour(juce::TextEditor::outlineColourId, ink.withAlpha(0.3f));
    nameField.setColour(juce::TextEditor::textColourId, ink);
    nameField.setFont(font(11.0f));
    addChildComponent(nameField);
    save.onClick = [this] {
        if (processor.saveUserPreset(nameField.getText()).existsAsFile()) {
            nameField.clear();
            showSource(int(presets::Source::user));
        }
    };
    addChildComponent(save);
    const char* qualityText[] { "ECO", "STANDARD", "HIGH" };
    for (int i = 0; i < 3; ++i) {
        quality[std::size_t(i)].setButtonText(qualityText[i]);
        quality[std::size_t(i)].onClick = [this, i] { setValue(processor.parameters, fxp::quality, float(i)); repaint(); };
        addAndMakeVisible(quality[std::size_t(i)]);
    }
    reset.setTooltip("INIT: no preset - everything back to its default (undoable)");
    reset.onClick = [this] { loadInit(); };
    undo.onClick = [this] { processor.undo(); if (onLoad) onLoad(); refreshUndo(); repaint(); };
    redo.onClick = [this] { processor.redo(); if (onLoad) onLoad(); refreshUndo(); repaint(); };
    for (auto* b : { &reset, &undo, &redo }) addAndMakeVisible(b);
    setWantsKeyboardFocus(true);
    showSource(0);
    refreshUndo();
}

void FxPresetDrawer::refreshUndo() {
    undo.setEnabled(processor.canUndo());
    redo.setEnabled(processor.canRedo());
    undo.setAlpha(undo.isEnabled() ? 1.0f : 0.35f);
    redo.setAlpha(redo.isEnabled() ? 1.0f : 0.35f);
}

int FxPresetDrawer::rows() const {
    return source == int(presets::Source::user) ? userFiles.size() : int(presets::inSource(presets::Source(source)).size());
}
juce::String FxPresetDrawer::rowName(int row) const {
    if (source == int(presets::Source::user)) return userFiles[row].getFileNameWithoutExtension().toUpperCase();
    return presets::factory()[std::size_t(presets::inSource(presets::Source(source))[std::size_t(row)])].name;
}
juce::String FxPresetDrawer::rowDescription(int row) const {
    if (source == int(presets::Source::user)) return "Your preset (" + userFiles[row].getFileName() + ")";
    return presets::factory()[std::size_t(presets::inSource(presets::Source(source))[std::size_t(row)])].description;
}

void FxPresetDrawer::showSource(int s) {
    source = std::clamp(s, 0, presets::sourceCount - 1);
    hover = -1;
    const bool user = source == int(presets::Source::user);
    if (user) userFiles = processor.userPresets();
    scroller.setGeometry(listArea(), 22, rows());
    scroller.restart(rows());
    for (int i = 0; i < presets::sourceCount; ++i) tabs[std::size_t(i)].setToggleState(i == source, juce::dontSendNotification);
    nameField.setVisible(user);
    save.setVisible(user);
    repaint();
}

void FxPresetDrawer::load(int row) {
    if (row < 0 || row >= rows()) return;
    if (source == int(presets::Source::user)) processor.loadUserPreset(userFiles[row]);
    else processor.applyPreset(presets::factory()[std::size_t(presets::inSource(presets::Source(source))[std::size_t(row)])]);
    if (onLoad) onLoad();
    refreshUndo();
    repaint();
}

void FxPresetDrawer::loadInit() {
    processor.initPreset();
    if (onLoad) onLoad();
    refreshUndo();
    repaint();
}

void FxPresetDrawer::paint(juce::Graphics& g) {
    heading = "PRESETS  -  NOW: " + processor.presetName().toUpperCase() + "  (" + (processor.abShown() == 0 ? "A" : "B") + ")";
    FxPage::paint(g);
    {
        const auto r = initRowArea().toFloat();
        const bool current = processor.presetName() == FxProcessor::initName;
        if (current || hoverInit) {
            g.setColour(ink.withAlpha(current ? 0.14f : 0.07f));
            g.fillRoundedRectangle(r, 3.0f);
        }
        g.setColour(ink.withAlpha(current ? 1.0f : hoverInit ? 0.9f : 0.72f));
        g.setFont(font(10.5f, current));
        g.drawText("INIT", r.reduced(10, 0), juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(0.45f));
        g.setFont(font(9.0f));
        g.drawText("NO PRESET  -  EVERYTHING AT ITS DEFAULT", r.reduced(10, 0).withTrimmedLeft(52), juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(0.12f));
        g.fillRect(16.0f, r.getBottom() + 2.0f, float(getWidth() - 32), 1.0f);
    }
    const int total = rows();
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(scroller.viewArea());
        for (int row = scroller.firstVisible(); row <= scroller.lastVisible() && row < total; ++row) {
            const auto r = scroller.rowBounds(row).toFloat();
            const bool current = rowName(row).equalsIgnoreCase(processor.presetName());
            if (current || hover == row) {
                g.setColour(ink.withAlpha(current ? 0.14f : 0.07f));
                g.fillRoundedRectangle(r, 3.0f);
            }
            g.setColour(ink.withAlpha(current ? 1.0f : hover == row ? 0.9f : 0.72f));
            g.setFont(font(10.5f, current));
            g.drawText(rowName(row), r.reduced(10, 0), juce::Justification::centredLeft);
        }
    }
    if (total == 0) {
        g.setColour(ink.withAlpha(0.45f));
        g.setFont(font(10.0f));
        g.drawText("NO PRESETS OF YOUR OWN YET - NAME ONE BELOW AND SAVE", listArea().withHeight(21), juce::Justification::centredLeft);
    }
    scroller.paintBar(g, ink);
    if (hover >= 0 && hover < total) {
        g.setColour(ink.withAlpha(0.55f));
        g.setFont(font(9.0f));
        g.drawFittedText(rowDescription(hover).toUpperCase(), juce::Rectangle<int>(16, listArea().getBottom() - 4, getWidth() - 32, 14), juce::Justification::centredLeft, 1, 0.8f);
    }
    g.setColour(ink.withAlpha(0.12f));
    g.fillRect(16, quality[0].getY() - 22, getWidth() - 32, 1);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.0f));
    g.drawText("QUALITY", 16, quality[0].getY() - 16, 120, 12, juce::Justification::centredLeft);
    const int q = valueOf(processor.parameters, fxp::quality);
    for (int i = 0; i < 3; ++i) quality[std::size_t(i)].setToggleState(i == q, juce::dontSendNotification);
}

void FxPresetDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    {
        auto header = area.removeFromTop(18).withTrimmedTop(-3);
        redo.setBounds(header.removeFromRight(46));
        undo.setBounds(header.removeFromRight(46));
        reset.setBounds(header.removeFromRight(56));
    }
    auto tabRow = area.removeFromTop(24);
    const int widths[] { 56, 56, 44, 50, 40, 40, 0 };
    for (int i = 0; i < presets::sourceCount; ++i) tabs[std::size_t(i)].setBounds(i + 1 < presets::sourceCount ? tabRow.removeFromLeft(widths[i]) : tabRow);
    scroller.setGeometry(listArea(), 22, rows());
    const int listBottom = listArea().getBottom() - 1 + 18;
    auto user = juce::Rectangle<int>(16, listBottom, getWidth() - 32, 24);
    save.setBounds(user.removeFromRight(70));
    nameField.setBounds(user.withTrimmedRight(8));
    auto q = juce::Rectangle<int>(16, listBottom + 46, getWidth() - 32, 22);
    const int qw = q.getWidth() / 3;
    for (auto& b : quality) b.setBounds(q.removeFromLeft(qw));
}

void FxPresetDrawer::mouseMove(const juce::MouseEvent& e) {
    const bool onInit = initRowArea().contains(e.getPosition());
    if (onInit != hoverInit) {
        hoverInit = onInit;
        repaint(initRowArea());
    }
    if (onInit) { setMouseCursor(juce::MouseCursor::PointingHandCursor); return; }
    const int h = scroller.rowAt(e.getPosition());
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}

void FxPresetDrawer::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    hoverInit = false;
    repaint();
}

void FxPresetDrawer::mouseDown(const juce::MouseEvent& e) {
    if (scroller.viewArea().contains(e.getPosition())) grabKeyboardFocus();
    if (scroller.mouseDown(e.getPosition())) repaint();
}

void FxPresetDrawer::mouseDrag(const juce::MouseEvent& e) {
    if (scroller.mouseDrag(e.getPosition())) {
        hover = -1;
        repaint();
    }
}

void FxPresetDrawer::mouseUp(const juce::MouseEvent& e) {
    const bool wasDragging = scroller.draggingThumb();
    scroller.mouseUp();
    if (wasDragging) { repaint(); return; }
    if (!e.mouseWasClicked()) return;
    if (initRowArea().contains(e.getPosition())) { loadInit(); return; }
    const int row = scroller.rowAt(e.getPosition());
    if (row >= 0) load(row);
}

void FxPresetDrawer::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (scroller.wheel(wheel)) {
        hover = scroller.rowAt(e.getPosition());
        repaint();
    }
}

bool FxPresetDrawer::keyPressed(const juce::KeyPress& k) {
    if (!scroller.key(k)) return false;
    repaint();
    return true;
}

//==============================================================================
FxView::FxView(FxProcessor& p)
    : Viewfinder(p.parameters, p.meters, p.vhsWarble),
      processor(p),
      mixKnob(p.parameters, fxp::mix, "MIX"),
      intensityKnob(p.parameters, fxp::intensity, "INTENSITY"),
      liminalityKnob(p.parameters, fxp::liminality, "LIMINALITY"),
      input(p.parameters, fxp::input, "INPUT"),
      output(p.parameters, fxp::output, "OUTPUT"),
      space(p.parameters, fxp::space, "SPACE"),
      roomSound(p.parameters, fxp::ambience, "ROOM SOUND"),
      places(p.parameters),
      liminalityPage(p.parameters),
      presetsPanel(p) {
    meterNames = { "OUT", "IN" };
    meterSources = { &meters.output, &meters.instrument };
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, fxp::bypass, bypass);
    bypass.setTooltip("BYPASS: the input untouched (a short cross-fade, in time with the track)");
    addAndMakeVisible(bypass);
    mixKnob.slider.setTooltip("MIX: the input untouched (0 %) to only TAPEWORN's world (100 %)");
    intensityKnob.slider.setTooltip("INTENSITY: how strongly the place transforms the sound (its processing and its room)");
    liminalityKnob.big = true;
    liminalityKnob.slider.setTooltip("LIMINALITY: how unstable, detuned, worn and half-remembered the sound is. 0 % clean, 40 % as set on its page, 100 % deeply unsettling");
    for (auto* c : std::initializer_list<juce::Component*> { &mixKnob, &intensityKnob, &liminalityKnob, &input, &output, &space, &roomSound }) addAndMakeVisible(c);
    placeButton.onClick = [this] { showPlace(!places.isVisible()); };
    liminalityButton.onClick = [this] { showLiminality(!liminalityPage.isVisible()); };
    presetsButton.onClick = [this] { showPresets(!presetsPanel.isVisible()); };
    for (auto* b : { &placeButton, &liminalityButton, &presetsButton }) addAndMakeVisible(b);
    aButton.setTooltip("Compare: setting A");
    bButton.setTooltip("Compare: setting B (starts as a copy of A)");
    copyButton.setTooltip("Copy the setting shown into the other one");
    aButton.onClick = [this] { processor.showAB(0); refreshAB(); presetsPanel.refreshUndo(); };
    bButton.onClick = [this] { processor.showAB(1); refreshAB(); presetsPanel.refreshUndo(); };
    copyButton.onClick = [this] { processor.copyAB(); };
    for (auto* b : { &aButton, &bButton, &copyButton }) addAndMakeVisible(b);
    addChildComponent(places);
    addChildComponent(liminalityPage);
    addChildComponent(presetsPanel);
    presetsPanel.onLoad = [this] { refreshAB(); if (syncPlace()) repaint(); };
    places.onChoose = [this] { if (syncPlace()) repaint(); };
    refreshAB();
    startViewfinder();
}

FxView::~FxView() = default;

void FxView::refreshAB() {
    aButton.setToggleState(processor.abShown() == 0, juce::dontSendNotification);
    bButton.setToggleState(processor.abShown() == 1, juce::dontSendNotification);
    copyButton.setButtonText(processor.abShown() == 0 ? "A>B" : "B>A");
}

bool FxView::drawerOpen() const { return places.isVisible() || liminalityPage.isVisible() || presetsPanel.isVisible(); }
void FxView::sceneCommitted() { places.refresh(); }

void FxView::refreshControls() {
    places.refresh();
    const bool b = processor.parameters.getRawParameterValue(fxp::bypass)->load() > 0.5f;
    if (b != bypassShown) {
        bypassShown = b;
        repaint(sceneArea());
    }
}

void FxView::closeDrawersExcept(juce::Component* keep) {
    for (auto* d : std::initializer_list<juce::Component*> { &places, &liminalityPage, &presetsPanel })
        if (d != keep) d->setVisible(false);
    placeButton.setToggleState(places.isVisible(), juce::dontSendNotification);
    liminalityButton.setToggleState(liminalityPage.isVisible(), juce::dontSendNotification);
    presetsButton.setToggleState(presetsPanel.isVisible(), juce::dontSendNotification);
    repaint();
}

void FxView::showPlace(bool show) {
    places.setVisible(show);
    closeDrawersExcept(show ? &places : nullptr);
}
void FxView::showLiminality(bool show) {
    liminalityPage.setVisible(show);
    closeDrawersExcept(show ? &liminalityPage : nullptr);
}
void FxView::showPresets(bool show) {
    presetsPanel.setVisible(show);
    if (show) presetsPanel.refreshUndo();
    closeDrawersExcept(show ? &presetsPanel : nullptr);
}

void FxView::resized() {
    Viewfinder::resized();
    {
        auto tabs = tabsArea();
        const std::pair<juce::Component*, int> parts[] { { &presetsButton, 86 }, { &liminalityButton, 104 }, { &placeButton, 70 } };
        for (const auto& [b, w] : parts) {
            b->setBounds(tabs.removeFromRight(w));
            tabs.removeFromRight(4);
        }
        tabs.removeFromRight(10);
        copyButton.setBounds(tabs.removeFromRight(44));
        tabs.removeFromRight(4);
        bButton.setBounds(tabs.removeFromRight(28));
        tabs.removeFromRight(2);
        aButton.setBounds(tabs.removeFromRight(28));
    }
    auto row = controlRow();
    for (auto* k : { &mixKnob, &intensityKnob }) k->setBounds(row.removeFromLeft(70).withSizeKeepingCentre(66, 76));
    liminalityKnob.setBounds(liminalityArea());
    row.removeFromLeft(18);
    bypass.setBounds(row.removeFromRight(96).withSizeKeepingCentre(96, 28));
    row.removeFromRight(14);
    const int half = (row.getWidth() - 14) / 2;
    auto top = row.removeFromTop(38), bottom = row;
    input.setBounds(top.removeFromLeft(half).withSizeKeepingCentre(half, 34));
    top.removeFromLeft(14);
    output.setBounds(top.withSizeKeepingCentre(top.getWidth(), 34));
    space.setBounds(bottom.removeFromLeft(half).withSizeKeepingCentre(half, 34));
    bottom.removeFromLeft(14);
    roomSound.setBounds(bottom.withSizeKeepingCentre(bottom.getWidth(), 34));
    for (auto* d : std::initializer_list<juce::Component*> { &places, &liminalityPage, &presetsPanel }) d->setBounds(drawerArea());
}

// Bypassed: the picture dims and the camcorder says so.
void FxView::paintOverChildren(juce::Graphics& g) {
    if (bypassShown) {
        g.setColour(juce::Colour(0x80060908));
        g.fillRect(sceneArea().withTrimmedTop(110));
        g.setColour(ink.withAlpha(0.85f));
        g.setFont(font(15.0f, true).withExtraKerningFactor(0.4f));
        g.drawText("BYPASSED", sceneArea().withTrimmedTop(110), juce::Justification::centred);
    }
    Viewfinder::paintOverChildren(g);
}

//==============================================================================
FxEditor::FxEditor(FxProcessor& p) : AudioProcessorEditor(p), processor(p), main(p) {
    const float scale = std::clamp(p.uiScale.load(), 0.85f, 1.5f);
    addAndMakeVisible(main);
    setResizable(true, true);
    setResizeLimits(juce::roundToInt(defaultWidth * 0.85f), juce::roundToInt(defaultHeight * 0.85f), juce::roundToInt(defaultWidth * 1.5f), juce::roundToInt(defaultHeight * 1.5f));
    getConstrainer()->setFixedAspectRatio(double(defaultWidth) / double(defaultHeight));
    setSize(juce::roundToInt(defaultWidth * scale), juce::roundToInt(defaultHeight * scale));
    ready = true;
    resized();
}

void FxEditor::resized() {
    const float scale = std::max(0.05f, std::min(float(getWidth()) / float(defaultWidth), float(getHeight()) / float(defaultHeight)));
    const float x = std::floor(0.5f * (float(getWidth()) - scale * float(defaultWidth)));
    const float y = std::floor(0.5f * (float(getHeight()) - scale * float(defaultHeight)));
    main.setBounds(0, 0, defaultWidth, defaultHeight);
    main.setTransform(juce::AffineTransform::scale(scale).translated(std::max(0.0f, x), std::max(0.0f, y)));
    if (ready) processor.uiScale.store(std::clamp(scale, 0.85f, 1.5f));
}

void FxEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff07100f)); }

bool FxEditor::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() != 'Z' && key.getKeyCode() != 'z') return false;
    if (!key.getModifiers().isCommandDown()) return false;
    const bool done = key.getModifiers().isShiftDown() ? processor.redo() : processor.undo();
    main.presetDrawer().refreshUndo();
    main.refreshAB();
    main.repaint();
    return done;
}

} // namespace liminal::fx
