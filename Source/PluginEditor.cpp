#include "PluginEditor.h"
#include <cmath>

namespace liminal {

namespace {
using ui::style::ink;
using ui::style::rec;
using ui::style::font;
using ui::style::nowMs;
} // namespace










//==============================================================================
namespace {
struct InstrumentInfo {
    const char* name; const char* shortName; const char* on; const char* level;
    std::array<std::pair<const char*, const char*>, 6> controls;
};
const std::array<InstrumentInfo, InstrumentDrawer::count> instrumentInfo { {
    { "FORGOTTEN PIANO", "PIANO", params::pianoOn, params::pianoLevel,
      { { { params::pianoAge, "AGE" }, { params::pianoDetune, "DETUNE" }, { params::pianoSoftness, "SOFTNESS" },
          { params::pianoNoise, "MECH. NOISE" }, { params::pianoDecay, "DECAY" }, { params::pianoDistance, "DISTANCE" } } } },
    { "FADED SYNTH", "SYNTH", params::synthOn, params::synthLevel,
      { { { params::synthDetune, "DETUNE" }, { params::synthWarmth, "WARMTH" }, { params::synthDrift, "DRIFT" },
          { params::synthAttack, "ATTACK" }, { params::synthRelease, "RELEASE" }, { params::synthWidth, "WIDTH" } } } },
    { "GLASS MEMORIES", "GLASS", params::glassOn, params::glassLevel,
      { { { params::glassFragility, "FRAGILITY" }, { params::glassBrightness, "BRIGHTNESS" }, { params::glassResonance, "RESONANCE" },
          { params::glassDetune, "DETUNE" }, { params::glassDecay, "DECAY" }, { params::glassDistance, "DISTANCE" } } } },
    { "EMPTY CHOIR", "CHOIR", params::choirOn, params::choirLevel,
      { { { params::choirBreath, "BREATH" }, { params::choirFormant, "FORMANT" }, { params::choirDetune, "DETUNE" },
          { params::choirWidth, "WIDTH" }, { params::choirDarkness, "DARKNESS" }, { params::choirRelease, "RELEASE" } } } },
    { "LOST MUSIC BOX", "MUSIC BOX", params::boxOn, params::boxLevel,
      { { { params::boxWear, "WEAR" }, { params::boxDrift, "PITCH DRIFT" }, { params::boxMechanism, "MECHANISM" },
          { params::boxBrightness, "BRIGHTNESS" }, { params::boxDecay, "DECAY" }, { params::boxDistance, "DISTANCE" } } } },
    { "HOLLOW DRONE", "DRONE", params::droneOn, params::droneLevel,
      { { { params::droneDensity, "DENSITY" }, { params::droneInstability, "INSTABILITY" }, { params::droneHarmonics, "HARMONICS" },
          { params::droneSub, "SUB" }, { params::droneMovement, "MOVEMENT" }, { params::droneRelease, "RELEASE" } } } },
} };
const char* tabNames[] { "PIANO", "SYNTH", "GLASS", "CHOIR", "BOX", "DRONE" };
// The instrument library (2026-11-02): what each slot holds, its names.
const char* libraryTabs[] { "PIANO", "SYNTH", "GLASS", "CHOIR", "BOX", "DRONE", "GUITAR", "STRINGS", "PIZZ", "TOY", "E-PIANO",
                            "VIBES", "CHIMES", "VOICE", "HUM", "TAPE", "ARCADE", "BASS", "ORGAN", "BOWED" };
int slotType(juce::AudioProcessorValueTreeState& st, int slot) {
    return std::clamp(int(std::lround(st.getRawParameterValue(params::slotTypeId(slot))->load())), 0, params::libraryCount - 1);
}
juce::String instrumentName(int type) { return juce::String(params::libraryNames[std::clamp(type, 0, params::libraryCount - 1)]).toUpperCase(); }
juce::String instrumentShort(int type) { return dsp::library::info(type).shortName; }
} // namespace













namespace {
const char* shelfIds[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
} // namespace

InstrumentShelf::InstrumentShelf(juce::AudioProcessorValueTreeState& s) : state(s) { refresh(); }

juce::Rectangle<float> InstrumentShelf::slot(int i) const {
    const float w = float(getWidth()) / 6.0f;
    return { float(i) * w, 0, w, float(getHeight()) };
}

bool InstrumentShelf::refresh() {
    std::array<bool, 6> now {};
    std::array<int, 6> held {};
    for (int i = 0; i < 6; ++i) {
        now[std::size_t(i)] = state.getRawParameterValue(shelfIds[i])->load() > 0.5f;
        held[std::size_t(i)] = slotType(state, i);
    }
    if (now == on && held == types) return false;
    on = now;
    types = held;
    repaint();
    return true;
}

void InstrumentShelf::toggle(int i) {
    auto* p = state.getParameter(shelfIds[i]);
    p->beginChangeGesture();
    p->setValueNotifyingHost(on[std::size_t(i)] ? 0.0f : 1.0f);
    p->endChangeGesture();
    refresh();
}

void InstrumentShelf::paint(juce::Graphics& g) {
    for (int i = 0; i < 6; ++i) {
        const auto r = slot(i).reduced(4, 0);
        const bool lit = on[std::size_t(i)];
        if (lit) {   // a soft light on the object
            g.setGradientFill(juce::ColourGradient(ink.withAlpha(0.18f), r.getCentre().translated(0, -6), ink.withAlpha(0.0f), r.getCentre().translated(r.getWidth() * 0.5f, -6), true));
            g.fillEllipse(r.withSizeKeepingCentre(r.getWidth(), r.getHeight() * 0.9f).translated(0, -6));
        }
        g.setColour(ink.withAlpha(lit ? 0.95f : hover == i ? 0.6f : 0.32f));
        ui::drawInstrumentIcon(g, types[std::size_t(i)], r.withTrimmedBottom(18).reduced(r.getWidth() * 0.12f, 2));
        g.setFont(font(8.5f, lit));
        g.drawFittedText(instrumentShort(types[std::size_t(i)]), r.withTop(r.getBottom() - 16).toNearestInt(), juce::Justification::centred, 1, 0.5f);
    }
}
void InstrumentShelf::mouseMove(const juce::MouseEvent& e) {
    int h = -1;
    for (int i = 0; i < 6; ++i)
        if (slot(i).contains(e.position)) h = i;
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}
void InstrumentShelf::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    repaint();
}
void InstrumentShelf::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int i = 0; i < 6; ++i)
        if (slot(i).contains(e.position)) toggle(i);
}




//==============================================================================
ActiveInstruments::ActiveInstruments(juce::AudioProcessorValueTreeState& s) : state(s) {
    refresh();
}

bool ActiveInstruments::refresh() {
    std::array<bool, params::instrumentCount> now {};
    std::array<int, params::instrumentCount> held {};
    for (int i = 0; i < params::instrumentCount; ++i) {
        now[std::size_t(i)] = state.getRawParameterValue(instrumentInfo[std::size_t(i)].on)->load() > 0.5f;
        held[std::size_t(i)] = slotType(state, i);
    }
    const int s = int(std::lround(state.getRawParameterValue(params::solo)->load()));
    if (now == on && s == solo && held == types) return false;
    on = now;
    types = held;
    solo = s;
    layout();
    repaint();
    return true;
}

juce::String ActiveInstruments::text() const {
    juce::StringArray list;
    for (int i = 0; i < params::instrumentCount; ++i)
        if (on[std::size_t(i)]) list.add(instrumentShort(types[std::size_t(i)]));
    return list.isEmpty() ? juce::String("NO INSTRUMENT ON") : list.joinIntoString(" + ");
}

void ActiveInstruments::resized() { layout(); }

void ActiveInstruments::layout() {
    names.clear();
    const auto f = font(11.5f, true);
    float x = 64.0f;   // after "PLAYING"
    for (int i = 0; i < params::instrumentCount; ++i) {
        if (!on[std::size_t(i)]) continue;
        const float w = juce::GlyphArrangement::getStringWidth(f, instrumentShort(types[std::size_t(i)])) + 4.0f;
        names.push_back({ i, { x, 0.0f, w, float(getHeight()) } });
        x += w + 22.0f;   // room for " + "
    }
}

int ActiveInstruments::nameAt(juce::Point<float> p) const {
    for (const auto& n : names)
        if (n.area.expanded(5.0f, 0.0f).contains(p)) return n.index;
    return -1;
}

void ActiveInstruments::paint(juce::Graphics& g) {
    const float h = float(getHeight());
    g.setFont(font(9.5f));
    g.setColour(ink.withAlpha(0.45f));
    g.drawText("PLAYING", juce::Rectangle<float>(0, 0, 60, h), juce::Justification::centredLeft);
    if (names.empty()) {
        g.setColour(ink.withAlpha(0.55f));
        g.setFont(font(11.0f));
        g.drawText("NO INSTRUMENT ON  -  OPEN INSTRUMENTS", juce::Rectangle<float>(64, 0, 400, h), juce::Justification::centredLeft);
        return;
    }
    const bool soloing = solo >= 1 && solo <= params::instrumentCount && on[std::size_t(solo - 1)];
    for (std::size_t k = 0; k < names.size(); ++k) {
        const auto& n = names[k];
        const bool soloed = soloing && solo - 1 == n.index, muted = soloing && !soloed;
        if (k > 0) {
            g.setColour(ink.withAlpha(0.4f));
            g.setFont(font(11.0f));
            g.drawText("+", juce::Rectangle<float>(n.area.getX() - 22.0f, 0, 22.0f, h), juce::Justification::centred);
        }
        if (soloed) {   // a lit tag behind the soloed name
            g.setColour(juce::Colour(0xffe8c35a).withAlpha(0.9f));
            g.fillRoundedRectangle(n.area.expanded(4.0f, -2.0f), 3.0f);
        }
        g.setFont(font(11.5f, true));
        g.setColour(soloed ? juce::Colour(0xff15120a) : ink.withAlpha(muted ? 0.3f : hover == n.index ? 1.0f : 0.85f));
        g.drawText(instrumentShort(types[std::size_t(n.index)]), n.area, juce::Justification::centred);
        if (hover == n.index && !soloed) {
            g.setColour(ink.withAlpha(0.7f));
            g.fillRect(n.area.getX() + 2.0f, h - 3.0f, n.area.getWidth() - 4.0f, 1.0f);
        }
    }
    const float after = names.back().area.getRight() + 16.0f;
    g.setFont(font(9.5f));
    g.setColour(ink.withAlpha(0.4f));
    const auto hint = soloing ? juce::String("SOLO  -  CLICK AGAIN TO HEAR ALL") : names.size() > 1 ? juce::String("CLICK A NAME TO SOLO IT") : juce::String();
    g.drawText(hint, juce::Rectangle<float>(after, 0, std::max(0.0f, float(getWidth()) - after), h), juce::Justification::centredLeft);
}

void ActiveInstruments::mouseMove(const juce::MouseEvent& e) {
    const int h = nameAt(e.position);
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}

void ActiveInstruments::mouseExit(const juce::MouseEvent&) {
    if (hover != -1) {
        hover = -1;
        repaint();
    }
}

void ActiveInstruments::mouseUp(const juce::MouseEvent& e) {
    if (e.mouseWasClicked()) {
        const int i = nameAt(e.position);
        if (i >= 0) clickName(i);
    }
}

void ActiveInstruments::clickName(int instrument) {
    auto* p = state.getParameter(params::solo);
    const int next = solo == instrument + 1 ? 0 : instrument + 1;
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(next)));
    p->endChangeGesture();
    refresh();
}

//==============================================================================
InstrumentPicker::InstrumentPicker(juce::AudioProcessorValueTreeState& s) : state(s) {
    setWantsKeyboardFocus(true);
}

void InstrumentPicker::open(int which) {
    slot = std::clamp(which, 0, params::instrumentCount - 1);
    hover = -1;
    scroller.setGeometry(getLocalBounds().reduced(4, 22).withTrimmedBottom(-18), rowHeight, params::libraryCount);
    scroller.restart(params::libraryCount);
    scroller.ensureVisible(slotType(state, slot));
    setVisible(true);
    repaint();
}

void InstrumentPicker::resized() { scroller.setGeometry(getLocalBounds().reduced(4, 22).withTrimmedBottom(-18), rowHeight, params::libraryCount); }

void InstrumentPicker::choose(int instrument) {
    if (instrument < 0 || instrument >= params::libraryCount) return;
    auto* p = state.getParameter(params::slotTypeId(slot));
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(instrument)));
    p->endChangeGesture();
    if (onChoose) onChoose();
}

void InstrumentPicker::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xf00b1412));   // opaque enough to read over the page's controls
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(ink.withAlpha(0.3f));
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(ink.withAlpha(0.5f));
    g.setFont(font(9.0f));
    g.drawText("CHOOSE WHAT THIS SLOT PLAYS  -  ITS EFFECTS AND ROOM STAY", area.reduced(10, 4).removeFromTop(14), juce::Justification::centredLeft);
    const int held = slotType(state, slot);
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(scroller.viewArea());
        for (int row = scroller.firstVisible(); row <= scroller.lastVisible() && row < params::libraryCount; ++row) {
            const auto r = scroller.rowBounds(row).toFloat();
            const bool current = row == held;
            if (current || hover == row) {
                g.setColour(ink.withAlpha(current ? 0.16f : 0.07f));
                g.fillRoundedRectangle(r, 3.0f);
            }
            g.setColour(ink.withAlpha(current ? 1.0f : hover == row ? 0.92f : 0.72f));
            ui::drawInstrumentIcon(g, row, r.withWidth(r.getHeight()).reduced(3.0f));
            g.setFont(font(10.5f, current));
            g.drawText(instrumentName(row), r.withTrimmedLeft(r.getHeight() + 6.0f).withWidth(170.0f), juce::Justification::centredLeft);
            g.setFont(font(8.5f));
            g.setColour(ink.withAlpha(0.45f));
            g.drawFittedText(juce::String(dsp::library::info(row).description).toUpperCase(),
                             r.withTrimmedLeft(r.getHeight() + 180.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
        }
    }
    scroller.paintBar(g, ink);
}

void InstrumentPicker::mouseMove(const juce::MouseEvent& e) {
    const int h = scroller.rowAt(e.getPosition());
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) { hover = h; repaint(); }
}
void InstrumentPicker::mouseExit(const juce::MouseEvent&) { hover = -1; repaint(); }
void InstrumentPicker::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (scroller.mouseDown(e.getPosition())) repaint();
}
void InstrumentPicker::mouseDrag(const juce::MouseEvent& e) {
    if (scroller.mouseDrag(e.getPosition())) { hover = -1; repaint(); }
}
void InstrumentPicker::mouseUp(const juce::MouseEvent& e) {
    const bool dragged = scroller.draggingThumb();
    scroller.mouseUp();
    if (dragged) { repaint(); return; }
    if (!e.mouseWasClicked()) return;
    const int row = scroller.rowAt(e.getPosition());
    if (row >= 0) choose(row);
}
void InstrumentPicker::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (scroller.wheel(w)) { hover = scroller.rowAt(e.getPosition()); repaint(); }
}
bool InstrumentPicker::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey) { setVisible(false); return true; }
    if (!scroller.key(k)) return false;
    repaint();
    return true;
}

InstrumentDrawer::InstrumentDrawer(juce::AudioProcessorValueTreeState& s) : state(s), picker(s) {
    for (int i = 0; i < count; ++i) {
        auto& page = pages[std::size_t(i)];
        const auto& info = instrumentInfo[std::size_t(i)];
        page.on.setButtonText(info.name);
        page.onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, info.on, page.on);
        addAndMakeVisible(page.on);
        page.level = std::make_unique<LevelControl>(state, info.level, "LEVEL");
        addAndMakeVisible(*page.level);
        page.tab.setButtonText(tabNames[i]);
        page.tab.setClickingTogglesState(false);
        page.tab.onClick = [this, i] { focus(i); };
        addAndMakeVisible(page.tab);
        for (int k = 0; k < dsp::lim::count; ++k) {
            page.liminal.push_back(std::make_unique<LevelControl>(state, params::limId(i, dsp::lim::keys[k]).toRawUTF8(), juce::String(dsp::lim::names[k]).toUpperCase()));
            addChildComponent(*page.liminal.back());
        }
    }
    soundTab.setButtonText("SOUND");
    liminalTab.setButtonText("LIMINALITY");
    soundTab.setTooltip("The instrument's own controls");
    liminalTab.setTooltip("How unstable, detuned and half-remembered this instrument is (0 = clean; scaled by the LIMINALITY knob, nudged by the place)");
    soundTab.onClick = [this] { showLiminality(false); };
    liminalTab.onClick = [this] { showLiminality(true); };
    addAndMakeVisible(soundTab);
    addAndMakeVisible(liminalTab);
    choose.setTooltip("Load any of the twenty instruments into this slot");
    choose.onClick = [this] { showPicker(!picker.isVisible()); };
    addAndMakeVisible(choose);
    addChildComponent(picker);
    picker.onChoose = [this] {
        showPicker(false);
        refresh();
    };
    refresh();
    focus(0);
}

// The SOUND controls for what the slot holds. One of the original six: its own controls (shared by
// every slot that plays it), with the slot's own Distance; one of the new fourteen: the slot's six
// library controls, named as that instrument reads them.
void InstrumentDrawer::build(int slot) {
    auto& page = pages[std::size_t(slot)];
    page.glideAttachment.reset();
    page.glide.reset();
    page.controls.clear();
    const int type = page.type;
    const char* slotDistance[] { params::pianoDistance, nullptr, params::glassDistance, nullptr, params::boxDistance, nullptr };
    const auto add = [&](const char* id, const juce::String& label) {
        page.controls.push_back(std::make_unique<LevelControl>(state, id, label));
        addChildComponent(*page.controls.back());
    };
    if (dsp::library::isOriginal(type)) {
        for (const auto& [id, label] : instrumentInfo[std::size_t(type)].controls) {
            const bool distance = juce::String(label) == "DISTANCE";
            if (!distance) add(id, label);
            else if (slotDistance[slot] != nullptr) add(slotDistance[slot], "DISTANCE");
        }
        if (type == 1) {
            page.glide = std::make_unique<juce::ToggleButton>("GLIDE  (OFF: NOTES START ON PITCH)");
            page.glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, params::synthGlide, *page.glide);
            addChildComponent(*page.glide);
            add(params::synthGlideTime, "GLIDE TIME");
            add(params::synthPatch, "PATCH");          // Milestone 8: the recorded patch
            add(params::synthBlend, "ANALOG BLEND");   // and the synthesised voice under it
        }
        if (type == 5) add(params::droneAttack, "ATTACK");
    } else {
        const juce::String labels[] { "TONE", "AGE", "SOFTNESS", "DECAY", "WIDTH", dsp::library::info(type).colour };
        for (int k = 0; k < params::lib::count; ++k) add(params::libId(slot, params::lib::keys[k]).toRawUTF8(), labels[k]);
        if (slotDistance[slot] != nullptr) add(slotDistance[slot], "DISTANCE");
        // 2026-11-06: the woodwind blend, on the instruments that have one, and its voice.
        if (dsp::library::windOf(type) != dsp::woodwind::none) {
            add(params::libId(slot, params::wind::amount).toRawUTF8(), dsp::library::windLabel(type));
            add(params::libId(slot, params::wind::voice).toRawUTF8(), "WIND VOICE");
        }
    }
}

bool InstrumentDrawer::refresh() {
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        auto& page = pages[std::size_t(i)];
        const int type = slotType(state, i);
        if (type == page.type) continue;
        page.type = type;
        page.on.setButtonText(instrumentName(type));
        page.tab.setButtonText(libraryTabs[type]);
        build(i);
        changed = true;
    }
    if (changed) {
        focus(current);
        resized();
    }
    return changed;
}

juce::StringArray InstrumentDrawer::controlLabels() const {
    juce::StringArray labels;
    for (const auto& c : pages[std::size_t(current)].controls) labels.add(c->getTitle());
    return labels;
}

void InstrumentDrawer::showPicker(bool show) {
    if (show) {
        picker.open(current);
        picker.toFront(false);
    } else {
        picker.setVisible(false);
    }
    choose.setToggleState(show, juce::dontSendNotification);
    choose.setButtonText(show ? "CLOSE" : "CHOOSE");
    focus(current);   // the page's controls hide under an open picker
}

void InstrumentDrawer::focus(int index) {
    if (picker.isVisible() && std::clamp(index, 0, count - 1) != current) picker.open(std::clamp(index, 0, count - 1));
    current = std::clamp(index, 0, count - 1);
    for (int i = 0; i < count; ++i) {
        auto& page = pages[std::size_t(i)];
        const bool shown = i == current && !picker.isVisible();
        page.tab.setToggleState(shown, juce::dontSendNotification);
        for (auto& c : page.controls) c->setVisible(shown && !liminalPage);
        if (page.glide != nullptr) page.glide->setVisible(shown && !liminalPage);
        for (auto& c : page.liminal) c->setVisible(shown && liminalPage);
        page.tab.setToggleState(i == current, juce::dontSendNotification);
    }
    soundTab.setToggleState(!liminalPage, juce::dontSendNotification);
    liminalTab.setToggleState(liminalPage, juce::dontSendNotification);
    choose.setVisible(!liminalPage);
    repaint();
}

void InstrumentDrawer::showLiminality(bool show) {
    liminalPage = show;
    if (show) showPicker(false);
    focus(current);
}

juce::Rectangle<int> InstrumentDrawer::chooserArea() const {
    return { liminalTab.getRight() + 12, liminalTab.getY(), getWidth() - 16 - liminalTab.getRight() - 12, liminalTab.getHeight() };
}
juce::Rectangle<int> InstrumentDrawer::controlsArea() const { return { 16, liminalTab.getBottom() + 6, getWidth() - 32, getHeight() - liminalTab.getBottom() - 6 - 8 }; }

void InstrumentDrawer::paint(juce::Graphics& g) {
    // The frosted glass itself is painted by the editor (from its cached scene).
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.5f));
    g.drawText("INSTRUMENTS  -  MIDI PLAYS EVERY ONE THAT IS ON", area.reduced(16, 10).removeFromTop(14), juce::Justification::centredLeft);
    g.setColour(ink.withAlpha(0.12f));
    g.fillRect(16.0f, float(pages[0].tab.getY() - 5), area.getWidth() - 32.0f, 1.0f);
    g.setColour(ink.withAlpha(0.4f));
    g.setFont(font(9.0f));
    if (liminalPage) {   // what the sliders on the Liminality half mean
        g.drawText("0 = CLEAN  -  x LIMINALITY KNOB", chooserArea(), juce::Justification::centredRight);
    } else {             // the instrument this slot holds: its icon and name, CHOOSE beside it
        const auto r = chooserArea().withTrimmedRight(choose.getWidth() + 8).toFloat();
        const int type = pages[std::size_t(current)].type;
        g.setColour(ink.withAlpha(0.9f));
        ui::drawInstrumentIcon(g, type, r.withWidth(r.getHeight()).reduced(1.0f));
        g.setFont(font(9.0f, true));
        g.drawFittedText(instrumentName(type), r.withTrimmedLeft(r.getHeight() + 6.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.5f);
    }
}

void InstrumentDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    area.removeFromTop(18);
    for (auto& page : pages) {
        auto row = area.removeFromTop(30);
        page.on.setBounds(row.removeFromLeft(178).withSizeKeepingCentre(178, 24));
        page.level->setBounds(row.withSizeKeepingCentre(row.getWidth(), 30));
    }
    area.removeFromTop(10);
    auto tabs = area.removeFromTop(26);
    const int tabWidth = tabs.getWidth() / count;
    for (auto& page : pages) page.tab.setBounds(tabs.removeFromLeft(tabWidth));
    area.removeFromTop(6);
    {   // the page's two halves: SOUND and LIMINALITY; the slot's instrument and CHOOSE on the right
        auto halves = area.removeFromTop(22);
        soundTab.setBounds(halves.removeFromLeft(70));
        halves.removeFromLeft(6);
        liminalTab.setBounds(halves.removeFromLeft(100));
        choose.setBounds(halves.removeFromRight(70));
    }
    area.removeFromTop(6);
    picker.setBounds(juce::Rectangle<int>(10, liminalTab.getBottom() + 4, getWidth() - 20, getHeight() - liminalTab.getBottom() - 10));
    // Liminality: ten controls in three columns, four rows.
    {
        const int lw = (area.getWidth() - 2 * 16) / 3;
        for (auto& page : pages)
            for (std::size_t k = 0; k < page.liminal.size(); ++k)
                page.liminal[k]->setBounds(area.getX() + int(k % 3) * (lw + 16), area.getY() + int(k / 3) * 38, lw, 36);
    }
    // Three columns: the six controls in two rows, then any extra row (Glide, Attack, Distance).
    const int w = (area.getWidth() - 2 * 16) / 3;
    for (auto& page : pages) {
        int extraRow = 2;
        if (page.glide != nullptr) {
            page.glide->setBounds(area.getX(), area.getY() + 2 * 42 + 6, area.getWidth(), 24);
            extraRow = 3;
        }
        for (std::size_t k = 0; k < page.controls.size(); ++k) {
            const int col = k < 6 ? int(k % 3) : int(k - 6), rowIndex = k < 6 ? int(k / 3) : extraRow;
            page.controls[k]->setBounds(area.getX() + col * (w + 16), area.getY() + rowIndex * 42 - (k >= 6 && page.glide != nullptr ? 8 : 0), w, 36);
        }
    }
}

//==============================================================================
namespace {
} // namespace

EnvironmentDrawer::EnvironmentDrawer(juce::AudioProcessorValueTreeState& s)
    : state(s), amount(s, params::environmentAmount, "ENVIRONMENT AMOUNT") {
    addAndMakeVisible(amount);
    refresh();
}

juce::Rectangle<int> EnvironmentDrawer::row(int place) const {
    return { 16, 30 + place * 23, getWidth() - 32, 22 };
}

juce::Rectangle<int> EnvironmentDrawer::characterBox(int character) const {
    const int w = (getWidth() - 32 - 2 * 6) / dsp::characterCount;
    return { 16 + character * (w + 6), row(dsp::placeCount - 1).getBottom() + 48, w, 24 };
}

void EnvironmentDrawer::chooseCharacter(int character) {
    auto* p = state.getParameter(params::character);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(character)));
    p->endChangeGesture();
    refresh();
    if (onChoose) onChoose();
}

bool EnvironmentDrawer::refresh() {
    const int now = int(std::lround(state.getRawParameterValue(params::environment)->load()));
    const int ch = int(std::lround(state.getRawParameterValue(params::character)->load()));
    if (now == selected && ch == selectedCharacter) return false;
    selected = now;
    selectedCharacter = ch;
    repaint();
    return true;
}

void EnvironmentDrawer::choose(int place) {
    auto* p = state.getParameter(params::environment);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(place)));
    p->endChangeGesture();
    refresh();
    if (onChoose) onChoose();
}

void EnvironmentDrawer::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.5f));
    g.drawText("ENVIRONMENT  -  WHERE THE INSTRUMENTS ARE PLAYED", area.reduced(16, 10).removeFromTop(14), juce::Justification::centredLeft);
    // Ten compact rows (name and a two-word summary); what the hovered (or the selected)
    // place does is described under the list.
    for (int i = 0; i < dsp::placeCount; ++i) {
        const auto r = row(i).toFloat();
        const bool on = i == selected;
        if (on) {
            g.setColour(ink.withAlpha(0.14f));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(ink.withAlpha(0.8f));
            g.fillRect(r.getX(), r.getY() + 5, 2.0f, r.getHeight() - 10);
        } else if (hover == i) {
            g.setColour(ink.withAlpha(0.07f));
            g.fillRoundedRectangle(r, 4.0f);
        }
        auto text = r.reduced(12, 0);
        g.setColour(ink.withAlpha(on ? 1.0f : hover == i ? 0.9f : 0.7f));
        g.setFont(font(11.5f, on));
        g.drawText(dsp::profile(dsp::Place(i)).name, text, juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(on ? 0.7f : 0.42f));
        g.setFont(font(9.0f));
        g.drawText(juce::String(placeInfo[i].summary).toUpperCase(), text, juce::Justification::centredRight);
    }
    {
        const int shown = std::clamp(hover >= 0 ? hover : selected, 0, dsp::placeCount - 1);
        const auto box = juce::Rectangle<int>(28, row(dsp::placeCount - 1).getBottom() + 6, getWidth() - 56, 26);
        g.setColour(ink.withAlpha(0.62f));
        g.setFont(font(9.0f));
        g.drawFittedText(placeInfo[shown].detail, box, juce::Justification::topLeft, 2, 1.0f);
    }
    // The place's three Characters.
    const int place = std::clamp(selected, 0, dsp::placeCount - 1);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.0f));
    g.drawText("CHARACTER", characterBox(0).translated(0, -15).withHeight(13), juce::Justification::centredLeft);
    for (int c = 0; c < dsp::characterCount; ++c) {
        const auto r = characterBox(c).toFloat();
        const bool on = c == selectedCharacter;
        g.setColour(on ? ink.withAlpha(0.85f) : ink.withAlpha(hoverCharacter == c ? 0.12f : 0.05f));
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(ink.withAlpha(on ? 0.0f : 0.3f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        g.setColour(on ? juce::Colour(0xff0b1412) : ink.withAlpha(0.8f));
        g.setFont(font(8.5f, on));
        g.drawFittedText(dsp::character(place, c).name, r.reduced(4, 0).toNearestInt(), juce::Justification::centred, 2, 0.8f);
    }
    {
        const auto& ch = dsp::character(place, std::clamp(hoverCharacter >= 0 ? hoverCharacter : selectedCharacter, 0, 2));
        g.setColour(ink.withAlpha(0.55f));
        g.setFont(font(9.0f));
        g.drawFittedText(juce::String(ch.mood).toUpperCase() + "  -  SUITS " + ch.recommended,
                         characterBox(0).withWidth(getWidth() - 32).translated(0, 28).withHeight(13), juce::Justification::centredLeft, 1, 0.8f);
    }
    {   // The room as the place and its Character make it: dimensions, materials, the processing.
        const auto prof = dsp::profileFor(dsp::Place(place), std::clamp(selectedCharacter, 0, 2));
        const char* materials[] { "VINYL FLOOR, PAINTED PLASTER, STEEL DOORS", "TERRAZZO, GLASS ROOF, SHUTTERS, STEEL", "LINOLEUM, METAL LOCKERS, PLASTER",
                                  "BARE CONCRETE, STEEL BEAMS", "CERAMIC TILE, WATER, HIGH GLASS", "CARPET, WALLPAPER, WOOD DOORS",
                                  "GLAZED TILE, STEEL RAILS, CONCRETE", "VINYL FLOOR, STEEL SHELVING, COLD GLASS", "CARPET TILES, CEILING TILES, FABRIC SCREENS",
                                  "NONE THAT CAN BE NAMED" };
        const char* patterns[] { "PARALLEL WALLS (FLUTTER)", "PACKED, IRREGULAR", "A FEW FAR SURFACES", "A TUNNEL", "A REPEATING GRID" };
        const auto& r = prof.room;
        const auto& c = prof.colour;
        juce::StringArray lines;
        lines.add("ROOM  -  RT60 " + juce::String(r.rt60Low, 1) + " S LOW / " + juce::String(r.rt60High, 1) + " S HIGH,  SIZE X" + juce::String(r.size, 1)
                  + ",  REFLECTIONS: " + patterns[int(r.pattern)]);
        lines.add(juce::String("MATERIALS  -  ") + materials[place]);
        lines.add("ON THE INSTRUMENTS  -  " + juce::String(juce::roundToInt(c.highPassHz)) + " HZ TO " + juce::String(c.lowPassHz / 1000.0f, 1) + " KHZ"
                  + (c.driftCents > 0 ? ",  DRIFT " + juce::String(juce::roundToInt(c.driftCents)) + " CENTS" : juce::String())
                  + (c.shiftMix > 0 ? ",  SHIFTED PARTIALS" : juce::String()) + ",  WIDTH X" + juce::String(c.width, 1));
        g.setColour(ink.withAlpha(0.5f));
        g.setFont(font(8.5f));
        int y = characterBox(0).getBottom() + 24;
        for (const auto& l : lines) {
            g.drawFittedText(l, juce::Rectangle<int>(16, y, getWidth() - 32, 12), juce::Justification::centredLeft, 1, 0.75f);
            y += 14;
        }
    }
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.5f));

}

void EnvironmentDrawer::resized() {
    amount.setBounds(getLocalBounds().reduced(16, 10).removeFromBottom(34));
}

void EnvironmentDrawer::mouseMove(const juce::MouseEvent& e) {
    int h = -1, hc = -1;
    for (int i = 0; i < dsp::placeCount; ++i)
        if (row(i).contains(e.getPosition())) h = i;
    for (int c = 0; c < dsp::characterCount; ++c)
        if (characterBox(c).contains(e.getPosition())) hc = c;
    setMouseCursor(h >= 0 || hc >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover || hc != hoverCharacter) {
        hover = h;
        hoverCharacter = hc;
        repaint();
    }
}

void EnvironmentDrawer::mouseExit(const juce::MouseEvent&) {
    hover = hoverCharacter = -1;
    repaint();
}

void EnvironmentDrawer::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int i = 0; i < dsp::placeCount; ++i)
        if (row(i).contains(e.getPosition())) choose(i);
    for (int c = 0; c < dsp::characterCount; ++c)
        if (characterBox(c).contains(e.getPosition())) chooseCharacter(c);
}

//==============================================================================
namespace {
struct ModuleInfo {
    const char* name; const char* on;
    std::vector<std::pair<const char*, const char*>> controls;
};
const std::vector<ModuleInfo>& modules() {
    static const std::vector<ModuleInfo> list {
        { "DRIFT", params::fx::driftOn, { { params::fx::drift, "AMT" }, { params::fx::driftRate, "RATE" } } },
        { "TAPE", params::fx::tapeOn, { { params::fx::tape, "AMT" }, { params::fx::tapeTone, "TONE" }, { params::fx::tapeHiss, "HISS" }, { params::fx::tapeDropouts, "DROP" } } },
        { "REVERB", params::fx::reverbOn, { { params::fx::reverb, "SEND" } } },
        { "ECHO", params::fx::echoOn, { { params::fx::echo, "AMT" }, { params::fx::echoTime, "MS" }, { params::fx::echoFeedback, "FDBK" }, { params::fx::echoTone, "TONE" } } },
        { "CHORUS", params::fx::chorusOn, { { params::fx::chorus, "AMT" }, { params::fx::chorusRate, "RATE" } } },
        { "FILTER", params::fx::filterOn, { { params::fx::filter, "AMT" }, { params::fx::filterLow, "LP" }, { params::fx::filterHigh, "HP" }, { params::fx::filterRes, "RES" } } },
        { "GRANULAR", params::fx::grainOn, { { params::fx::grain, "AMT" }, { params::fx::grainSize, "SIZE" }, { params::fx::grainSpread, "SPRD" } } },
    };
    return list;
}
void styleCombo(juce::ComboBox& c) {
    c.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0x2207100f));
    c.setColour(juce::ComboBox::outlineColourId, ink.withAlpha(0.3f));
    c.setColour(juce::ComboBox::textColourId, ink.withAlpha(0.9f));
    c.setColour(juce::ComboBox::arrowColourId, ink.withAlpha(0.7f));
}
} // namespace

EffectsDrawer::EffectsDrawer(juce::AudioProcessorValueTreeState& s) : state(s) {
    for (int i = 0; i < params::instrumentCount; ++i) {
        auto& tab = tabs[std::size_t(i)];
        tab.setButtonText(tabNames[i]);
        tab.onClick = [this, i] { focus(i); };
        addAndMakeVisible(tab);
    }
    refreshNames();
    focus(0);
}

bool EffectsDrawer::refreshNames() {
    bool changed = false;
    for (int i = 0; i < params::instrumentCount; ++i) {
        const int type = slotType(state, i);
        if (type == tabTypes[std::size_t(i)]) continue;
        tabTypes[std::size_t(i)] = type;
        tabs[std::size_t(i)].setButtonText(libraryTabs[type]);
        changed = true;
    }
    return changed;
}

// Controls exist only for the instrument shown (cheaper than 192 hidden ones).
void EffectsDrawer::focus(int instrument) {
    current = std::clamp(instrument, 0, params::instrumentCount - 1);
    for (int i = 0; i < params::instrumentCount; ++i) tabs[std::size_t(i)].setToggleState(i == current, juce::dontSendNotification);
    orderAttachment.reset();   // attachments go before the controls they are attached to
    rows.clear();
    const auto id = [&](const char* n) { return params::fxId(current, n); };
    for (const auto& m : modules()) {
        Row row;
        row.on = std::make_unique<juce::ToggleButton>(m.name);
        row.onAttachment = std::make_unique<ButtonAttachment>(state, id(m.on), *row.on);
        addAndMakeVisible(*row.on);
        for (const auto& [n, label] : m.controls) {
            row.controls.push_back(std::make_unique<LevelControl>(state, id(n).toRawUTF8(), label));
            addAndMakeVisible(*row.controls.back());
        }
        if (m.on == params::fx::echoOn) {
            row.choice = std::make_unique<juce::ComboBox>();
            row.choice->addItemList(juce::StringArray(params::fx::syncNames, 8), 1);
            styleCombo(*row.choice);
            row.choiceAttachment = std::make_unique<ComboAttachment>(state, id(params::fx::echoSync), *row.choice);
            row.choice->setTooltip("Echo time: free (TIME) or synced to the song's tempo");
            addAndMakeVisible(*row.choice);
        }
        if (m.on == params::fx::grainOn) {
            row.extra = std::make_unique<juce::ToggleButton>("FREEZE");
            row.extraAttachment = std::make_unique<ButtonAttachment>(state, id(params::fx::grainFreeze), *row.extra);
            addAndMakeVisible(*row.extra);
        }
        rows.push_back(std::move(row));
    }
    midi.clear();
    for (const auto& [n, label] : { std::pair<const char*, const char*> { params::midi::keyLow, "LOW" }, { params::midi::keyHigh, "HIGH" },
                                    { params::midi::velocityVolume, "V>VOL" }, { params::midi::velocityFilter, "V>FLT" } }) {
        midi.push_back(std::make_unique<LevelControl>(state, params::midiId(current, n).toRawUTF8(), label));
        addAndMakeVisible(*midi.back());
    }
    mix = std::make_unique<LevelControl>(state, id(params::fx::mix).toRawUTF8(), "EFFECTS MIX");
    influence = std::make_unique<LevelControl>(state, id(params::fx::influence).toRawUTF8(), "INFLUENCE");
    addAndMakeVisible(*mix);
    addAndMakeVisible(*influence);
    order = std::make_unique<juce::ComboBox>();
    order->addItemList(juce::StringArray(params::fx::orderNames, 3), 1);
    styleCombo(*order);
    orderAttachment = std::make_unique<ComboAttachment>(state, id(params::fx::order), *order);
    order->setTooltip("Effect order: Standard (drift, tape, filter, chorus, granular, echo), Texture first, Echo into texture");
    addAndMakeVisible(*order);
    resized();
    repaint();
}

void EffectsDrawer::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.5f));
    g.drawText("EFFECTS  -  EACH INSTRUMENT HAS ITS OWN", area.reduced(16, 10).removeFromTop(14), juce::Justification::centredLeft);
    g.setColour(ink.withAlpha(0.1f));
    for (std::size_t i = 1; i < rows.size(); ++i) g.fillRect(16.0f, float(rows[i].on->getY() - 3), area.getWidth() - 32.0f, 1.0f);
    if (!midi.empty()) {
        g.fillRect(16.0f, float(midi[0]->getY() - 3), area.getWidth() - 32.0f, 1.0f);
        g.setColour(ink.withAlpha(0.75f));
        g.setFont(font(11.0f));
        g.drawText("MIDI", 38, midi[0]->getY() + 1, 60, 18, juce::Justification::centredLeft);
    }
    // What the place adds to this instrument at full Influence.
    const int place = std::clamp(int(std::lround(state.getRawParameterValue(params::environment)->load())), 0, dsp::placeCount - 1);
    const auto e = dsp::placeEffects(dsp::Place(place), current);
    juce::StringArray adds;
    const auto pct = [](float v) { return juce::String(juce::roundToInt(v * 100.0f)); };
    if (e.drift > 0) adds.add("DRIFT " + pct(e.drift));
    if (e.tape > 0) adds.add("TAPE " + pct(e.tape));
    if (e.chorus > 0) adds.add("CHORUS " + pct(e.chorus));
    if (e.grain > 0) adds.add("GRAIN " + pct(e.grain));
    if (e.echo > 0) adds.add("ECHO " + pct(e.echo) + " @ " + juce::String(juce::roundToInt(e.echoMs)) + " MS");
    if (e.lowOctaves > 0) adds.add("LP -" + juce::String(e.lowOctaves, 1) + " OCT");
    if (e.highOctaves > 0) adds.add("HP +" + juce::String(e.highOctaves, 1) + " OCT");
    if (e.reverbBias != 0) adds.add(juce::String("ROOM ") + (e.reverbBias > 0 ? "+" : "") + pct(e.reverbBias) + " %");
    g.setColour(ink.withAlpha(0.5f));
    g.setFont(font(9.0f));
    g.drawFittedText(juce::String(dsp::profile(dsp::Place(place)).name) + " ADDS (AT 100 %):  " + adds.joinIntoString("  /  "),
                     juce::Rectangle<int>(16, getHeight() - 28, getWidth() - 32, 24), juce::Justification::topLeft, 2, 1.0f);
}

void EffectsDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    area.removeFromTop(16);
    auto tabRow = area.removeFromTop(24);
    const int tabWidth = tabRow.getWidth() / params::instrumentCount;
    for (auto& tab : tabs) tab.setBounds(tabRow.removeFromLeft(tabWidth));
    area.removeFromTop(6);
    for (auto& row : rows) {
        auto r = area.removeFromTop(37);
        auto left = r.removeFromLeft(96);
        row.on->setBounds(left.removeFromTop(19));
        if (row.choice != nullptr) row.choice->setBounds(left.withTrimmedTop(1).withHeight(17).withTrimmedRight(8));
        if (row.extra != nullptr) row.extra->setBounds(left.withHeight(18));
        const int w = r.getWidth() / 4;
        for (auto& c : row.controls) c->setBounds(r.removeFromLeft(w).withTrimmedRight(8).withSizeKeepingCentre(w - 8, 34));
    }
    {   // MIDI: which keys play this instrument, and what velocity does
        auto r = area.removeFromTop(34);
        r.removeFromLeft(96);
        const int w = r.getWidth() / 4;
        for (auto& c : midi) c->setBounds(r.removeFromLeft(w).withTrimmedRight(8).withSizeKeepingCentre(w - 8, 32));
    }
    area.removeFromTop(2);
    auto bottom = area.removeFromTop(36);
    const int w = bottom.getWidth() / 3;
    mix->setBounds(bottom.removeFromLeft(w).withTrimmedRight(10));
    influence->setBounds(bottom.removeFromLeft(w).withTrimmedRight(10));
    order->setBounds(bottom.withSizeKeepingCentre(bottom.getWidth(), 22).translated(0, 6));
}

//==============================================================================
PerformDrawer::PerformDrawer(LiminalProcessor& p)
    : processor(p),
      presence(p.parameters, params::presence, "PRESENCE"), scale(p.parameters, params::scale, "SCALE"),
      activity(p.parameters, params::activity, "ACTIVITY"), unreal(p.parameters, params::unreal, "UNREAL"),
      rate(p.parameters, params::evolution, "EVOLUTION RATE"), morph(p.parameters, params::sceneMorph, "MORPH"),
      seed(p.parameters, params::evolutionSeed, "SEED") {
    for (auto* c : { &presence, &scale, &activity, &unreal, &rate, &morph, &seed }) addAndMakeVisible(c);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, params::evolutionFreeze, freeze);
    morphOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, params::sceneOn, morphOn);
    addAndMakeVisible(freeze);
    addAndMakeVisible(morphOn);
    reset.setTooltip("Start the place's evolution again from its seed");
    reset.onClick = [this] { processor.requestEvolutionReset(); };
    storeA.setTooltip("Store everything as it is now as scene A");
    storeB.setTooltip("Store everything as it is now as scene B");
    storeA.onClick = [this] { processor.storeScene(0); refreshScenes(); };
    storeB.onClick = [this] { processor.storeScene(1); refreshScenes(); };
    for (auto* b : { &reset, &storeA, &storeB }) addAndMakeVisible(b);
    // The VHS look (saved with the song): how worn the tape is, and the optional audio warble.
    const char* vhsText[] { "OFF", "LIGHT", "HEAVY" };
    for (int i = 0; i < 3; ++i) {
        auto& b = vhsButtons[std::size_t(i)];
        b.setButtonText(vhsText[i]);
        b.setTooltip("The found-footage look: the picture as if filmed on an old camcorder and played from a worn tape");
        b.onClick = [this, i] {
            auto* v = processor.parameters.getParameter(params::vhs);
            v->beginChangeGesture();
            v->setValueNotifyingHost(v->convertTo0to1(float(i)));
            v->endChangeGesture();
            repaint();
        };
        addAndMakeVisible(b);
    }
    warble.setTooltip("A faint tape warble on the sound while the picture's tape struggles through a heavy tracking glitch (off by default)");
    warbleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, params::vhsWarble, warble);
    addAndMakeVisible(warble);
    refreshScenes();
}

void PerformDrawer::refreshScenes() {
    storeA.setButtonText(processor.sceneStored(0) ? "STORE A  *" : "STORE A");
    storeB.setButtonText(processor.sceneStored(1) ? "STORE B  *" : "STORE B");
    repaint();
}

void PerformDrawer::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setFont(font(9.5f));
    g.setColour(ink.withAlpha(0.45f));
    g.drawText("PERFORM  -  THE PLACE, AS YOU PLAY", area.reduced(16, 10).removeFromTop(14), juce::Justification::centredLeft);
    g.setFont(font(9.0f));
    for (const auto& [y, text] : { std::pair<int, const char*> { presence.getY() - 16, "MACROS" }, { rate.getY() - 16, "EVOLUTION" }, { storeA.getY() - 18, "SCENES" },
                                   { vhsButtons[0].getY() - 18, "PICTURE  -  VHS LOOK (FOUND FOOTAGE)" } }) {
        g.setColour(ink.withAlpha(0.12f));
        g.fillRect(16, y - 4, getWidth() - 32, 1);
        g.setColour(ink.withAlpha(0.45f));
        g.drawText(text, 16, y, getWidth() - 32, 12, juce::Justification::centredLeft);
    }
    const int v = int(std::lround(processor.parameters.getRawParameterValue(params::vhs)->load()));
    for (int i = 0; i < 3; ++i) vhsButtons[std::size_t(i)].setToggleState(i == v, juce::dontSendNotification);
    g.setColour(ink.withAlpha(0.42f));
    g.drawFittedText("While MORPH is on, the stored scenes play (an unstored scene = the knobs as they are). Place, Character and switches change at half-way.",
                     juce::Rectangle<int>(16, getHeight() - 32, getWidth() - 32, 24), juce::Justification::topLeft, 2, 1.0f);
}

void PerformDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    area.removeFromTop(36);
    const int half = (area.getWidth() - 16) / 2;
    auto row = area.removeFromTop(38);
    presence.setBounds(row.removeFromLeft(half));
    row.removeFromLeft(16);
    scale.setBounds(row);
    area.removeFromTop(6);
    row = area.removeFromTop(38);
    activity.setBounds(row.removeFromLeft(half));
    row.removeFromLeft(16);
    unreal.setBounds(row);
    area.removeFromTop(30);
    row = area.removeFromTop(38);
    rate.setBounds(row.removeFromLeft(half));
    row.removeFromLeft(16);
    seed.setBounds(row);
    area.removeFromTop(4);
    row = area.removeFromTop(24);
    freeze.setBounds(row.removeFromLeft(half));
    reset.setBounds(row.removeFromLeft(90));
    area.removeFromTop(30);
    row = area.removeFromTop(26);
    storeA.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(8);
    storeB.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(8);
    morphOn.setBounds(row);
    area.removeFromTop(4);
    morph.setBounds(area.removeFromTop(38));
    area.removeFromTop(30);
    row = area.removeFromTop(24);
    for (auto& b : vhsButtons) b.setBounds(row.removeFromLeft(62));
    row.removeFromLeft(16);
    warble.setBounds(row);
}

//==============================================================================
PresetDrawer::PresetDrawer(LiminalProcessor& p) : processor(p) {
    const char* tabText[] { "PLACES", "CHARACTERS", "INSTRUMENTS", "SCENES", "USER" };
    for (int i = 0; i < 5; ++i) {
        tabs[std::size_t(i)].setButtonText(tabText[i]);
        tabs[std::size_t(i)].onClick = [this, i] { showCategory(i); };
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
            showCategory(4);
        }
    };
    addChildComponent(save);
    const char* qualityText[] { "ECO", "STANDARD", "HIGH" };
    for (int i = 0; i < 3; ++i) {
        quality[std::size_t(i)].setButtonText(qualityText[i]);
        quality[std::size_t(i)].onClick = [this, i] {
            auto* q = processor.parameters.getParameter(params::quality);
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->convertTo0to1(float(i)));
            q->endChangeGesture();
            repaint();
        };
        addAndMakeVisible(quality[std::size_t(i)]);
    }
    duration.addItemList({ "30 SECONDS", "1 MINUTE", "2 MINUTES", "5 MINUTES", "10 MINUTES" }, 1);
    duration.setSelectedId(2, juce::dontSendNotification);
    rate.addItemList({ "44.1 KHZ", "48 KHZ", "96 KHZ" }, 1);
    rate.setSelectedId(2, juce::dontSendNotification);
    for (auto* c : { &duration, &rate }) {
        styleCombo(*c);
        addAndMakeVisible(c);
    }
    room.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(room);
    exportButton.setTooltip("Render the place's own sounds (no instruments) to a WAV file, with the same engine, offline");
    exportButton.onClick = [this] { startExport(); };
    addAndMakeVisible(exportButton);
    reset.setTooltip("INIT: no preset - every instrument, the place, the effects and LIMINALITY back to their defaults (undoable)");
    reset.onClick = [this] { loadInit(); };
    undo.setTooltip("Undo the last preset load");
    undo.onClick = [this] {
        processor.undoPreset();
        if (onLoad) onLoad();
        refreshUndo();
        repaint();
    };
    redo.setTooltip("Redo the preset load you undid");
    redo.onClick = [this] {
        processor.redoPreset();
        if (onLoad) onLoad();
        refreshUndo();
        repaint();
    };
    for (auto* b : { &reset, &undo, &redo }) addAndMakeVisible(b);
    setWantsKeyboardFocus(true);   // the list's keys (arrows, page up / down, home / end) once it is clicked
    showCategory(3);
    refreshUndo();
}

void PresetDrawer::refreshUndo() {
    if (shownName != processor.presetName()) {
        shownName = processor.presetName();
        resized();
        repaint();
    }
    undo.setEnabled(processor.canUndoPreset());
    redo.setEnabled(processor.canRedoPreset());
    undo.setAlpha(undo.isEnabled() ? 1.0f : 0.35f);
    redo.setAlpha(redo.isEnabled() ? 1.0f : 0.35f);
    undo.setTooltip(undo.isEnabled() ? "Undo loading " + processor.undoPresetName() : "Nothing to undo");
    redo.setTooltip(redo.isEnabled() ? "Redo loading " + processor.redoPresetName() : "Nothing to redo");
}

void PresetDrawer::loadInit() {
    processor.initPreset();
    if (onLoad) onLoad();
    refreshUndo();
    repaint();
}

int PresetDrawer::rows() const {
    return category == 4 ? userFiles.size() : int(presets::inCategory(presets::Category(category)).size());
}
juce::String PresetDrawer::rowName(int row) const {
    if (category == 4) return userFiles[row].getFileNameWithoutExtension().toUpperCase();
    return presets::factory()[std::size_t(presets::inCategory(presets::Category(category))[std::size_t(row)])].name;
}
juce::String PresetDrawer::rowDescription(int row) const {
    if (category == 4) return "Your preset (" + userFiles[row].getFileName() + ")";
    return presets::factory()[std::size_t(presets::inCategory(presets::Category(category))[std::size_t(row)])].description;
}

void PresetDrawer::showCategory(int c) {
    category = std::clamp(c, 0, 4);
    hover = -1;
    if (category == 4) userFiles = processor.userPresets();
    scroller.setGeometry(listArea(), 22, rows());
    scroller.restart(rows());   // a new list starts at its top (the only thing that ever moves it back)
    for (int i = 0; i < 5; ++i) tabs[std::size_t(i)].setToggleState(i == category, juce::dontSendNotification);
    nameField.setVisible(category == 4);
    save.setVisible(category == 4);
    repaint();
}

void PresetDrawer::load(int row) {
    if (row < 0 || row >= rows()) return;
    if (category == 4) processor.loadUserPreset(userFiles[row]);
    else processor.applyPreset(presets::factory()[std::size_t(presets::inCategory(presets::Category(category))[std::size_t(row)])]);
    if (onLoad) onLoad();
    refreshUndo();
    repaint();
}

void PresetDrawer::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setFont(font(9.5f));
    g.setColour(ink.withAlpha(0.45f));
    g.drawFittedText(headerText(), area.reduced(16, 10).removeFromTop(14).toNearestInt().withRight(reset.getX() - 2), juce::Justification::centredLeft, 1, 0.8f);
    {   // INIT: no preset, above the list
        const auto r = initRowArea().toFloat();
        const bool current = processor.presetName() == LiminalProcessor::initName;
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
        g.reduceClipRegion(scroller.viewArea());   // a row half scrolled out is cut at the list's edge
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
    scroller.paintBar(g, ink);   // drag it, click its track, or use the wheel / arrow keys
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
    g.drawText("EXPORT THE PLACE'S AMBIENCE (WAV, STEREO, NO INSTRUMENTS)", 16, duration.getY() - 16, getWidth() - 32, 12, juce::Justification::centredLeft);
    const int q = int(std::lround(processor.parameters.getRawParameterValue(params::quality)->load()));
    for (int i = 0; i < 3; ++i) quality[std::size_t(i)].setToggleState(i == q, juce::dontSendNotification);
    g.setColour(ink.withAlpha(0.6f));
    g.drawFittedText(status, juce::Rectangle<int>(exportButton.getRight() + 12, exportButton.getY(), getWidth() - exportButton.getRight() - 28, exportButton.getHeight()),
                     juce::Justification::centredLeft, 1, 0.8f);
}

juce::String PresetDrawer::headerText() const { return "PRESETS  -  NOW: " + processor.presetName().toUpperCase(); }

void PresetDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    {   // the preset's name, RESET right beside it; UNDO / REDO at the end of the line
        auto header = area.removeFromTop(18).withTrimmedTop(-3);
        redo.setBounds(header.removeFromRight(46));
        undo.setBounds(header.removeFromRight(46));
        const int nameWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font(9.5f), headerText())) + 10;
        reset.setBounds(header.getX() + std::min(nameWidth, header.getWidth() - 56), header.getY(), 56, header.getHeight());
    }
    auto tabRow = area.removeFromTop(24);
    const int widths[] { 58, 92, 100, 62, 0 };   // by the length of each word
    for (int i = 0; i < 5; ++i) tabs[std::size_t(i)].setBounds(i < 4 ? tabRow.removeFromLeft(widths[i]) : tabRow);
    scroller.setGeometry(listArea(), 22, rows());   // (the offset stays where it was, kept in range)
    const int listBottom = listArea().getBottom() - 1 + 18;
    auto user = juce::Rectangle<int>(16, listBottom, getWidth() - 32, 24);
    save.setBounds(user.removeFromRight(70));
    nameField.setBounds(user.withTrimmedRight(8));
    auto q = juce::Rectangle<int>(16, listBottom + 46, getWidth() - 32, 22);
    const int qw = q.getWidth() / 3;
    for (auto& b : quality) b.setBounds(q.removeFromLeft(qw));
    auto e = juce::Rectangle<int>(16, listBottom + 88, getWidth() - 32, 22);
    duration.setBounds(e.removeFromLeft(108));
    e.removeFromLeft(6);
    rate.setBounds(e.removeFromLeft(86));
    e.removeFromLeft(6);
    room.setBounds(e);
    exportButton.setBounds(16, listBottom + 114, 150, 22);
}

void PresetDrawer::mouseMove(const juce::MouseEvent& e) {
    const bool onInit = initRowArea().contains(e.getPosition());
    if (onInit != hoverInit) {
        hoverInit = onInit;
        repaint(initRowArea());
    }
    if (onInit) { setMouseCursor(juce::MouseCursor::PointingHandCursor); return; }
    const int h = scroller.rowAt(e.getPosition());
    const bool onBar = scroller.scrollable() && scroller.barArea().expanded(3, 0).contains(e.getPosition());
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : onBar ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}

void PresetDrawer::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    hoverInit = false;
    repaint();
}

void PresetDrawer::mouseDown(const juce::MouseEvent& e) {
    if (scroller.viewArea().contains(e.getPosition())) grabKeyboardFocus();
    if (scroller.mouseDown(e.getPosition())) repaint();
}

void PresetDrawer::mouseDrag(const juce::MouseEvent& e) {
    if (scroller.mouseDrag(e.getPosition())) {
        hover = -1;
        repaint();
    }
}

void PresetDrawer::mouseUp(const juce::MouseEvent& e) {
    const bool wasDragging = scroller.draggingThumb();
    scroller.mouseUp();
    if (wasDragging) { repaint(); return; }
    if (!e.mouseWasClicked()) return;
    if (initRowArea().contains(e.getPosition())) { loadInit(); return; }
    const int row = scroller.rowAt(e.getPosition());
    if (row >= 0) load(row);
}

void PresetDrawer::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (scroller.wheel(wheel)) {
        hover = scroller.rowAt(e.getPosition());   // the row now under the mouse
        repaint();
    }
}

bool PresetDrawer::keyPressed(const juce::KeyPress& k) {
    if (!scroller.key(k)) return false;
    repaint();
    return true;
}

void PresetDrawer::startExport() {
    if (job != nullptr && !job->finished()) return;
    const double seconds[] { 30.0, 60.0, 120.0, 300.0, 600.0 };
    const double rates[] { 44100.0, 48000.0, 96000.0 };
    exporter::Options options;
    options.seconds = seconds[std::clamp(duration.getSelectedId() - 1, 0, 4)];
    options.sampleRate = rates[std::clamp(rate.getSelectedId() - 1, 0, 2)];
    options.room = room.getToggleState();
    const auto& where = dsp::profile(processor.placeNow().place());
    const auto name = juce::File::createLegalFileName(juce::String(brand::product) + " " + where.name + " - " + dsp::character(int(processor.placeNow().place()), processor.placeNow().characterIndex()).name + ".wav");
    // The host's sandbox allows a file the user picks: ask where.
    chooser = std::make_unique<juce::FileChooser>("Export the ambience", juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile(name), "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, options](const juce::FileChooser& fc) {
                             const auto file = fc.getResult();
                             if (file == juce::File()) return;
                             juce::MemoryBlock state;
                             processor.getStateInformation(state);
                             job = std::make_unique<exporter::Job>(std::move(state), options, file.withFileExtension("wav"));
                             status = "EXPORTING...";
                             repaint();
                         });
}

bool PresetDrawer::poll() {
    if (shownName != processor.presetName() || undo.isEnabled() != processor.canUndoPreset() || redo.isEnabled() != processor.canRedoPreset()) refreshUndo();   // (a host program, automation)
    if (job == nullptr) return false;
    if (job->finished()) {
        status = job->succeeded() ? "SAVED " + job->file().getFileName().toUpperCase() : "EXPORT FAILED";
        job.reset();
        shownProgress = -1;
        repaint();
        return true;
    }
    const float p = job->progress();
    if (std::abs(p - shownProgress) > 0.01f) {
        shownProgress = p;
        status = "EXPORTING  " + juce::String(juce::roundToInt(p * 100.0f)) + " %";
        repaint(exportButton.getRight(), exportButton.getY(), getWidth() - exportButton.getRight(), exportButton.getHeight());
    }
    return false;
}

//==============================================================================
MainView::MainView(LiminalProcessor& p)
    : Viewfinder(p.parameters, p.meters, p.vhsWarble),
      processor(p),
      master(p.parameters, params::master, "MASTER"),
      instrument(p.parameters, params::instrument, "INST"),
      ambience(p.parameters, params::ambience, "AMB"),
      space(p.parameters, params::space, "SPACE"),
      shelf(p.parameters),
      presenceKnob(p.parameters, params::presence, "PRESENCE"), scaleKnob(p.parameters, params::scale, "SCALE"),
      activityKnob(p.parameters, params::activity, "ACTIVITY"), unrealKnob(p.parameters, params::unreal, "UNREAL"),
      liminalityKnob(p.parameters, params::liminality, "LIMINALITY"),
      active(p.parameters),
      drawer(p.parameters),
      places(p.parameters),
      effects(p.parameters),
      perform(p),
      presetsPanel(p) {
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, params::power, power);
    addAndMakeVisible(power);
    for (auto* c : { &instrument, &ambience, &space, &master }) addAndMakeVisible(c);
    for (auto* c : std::initializer_list<juce::Component*> { &shelf, &presenceKnob, &scaleKnob, &activityKnob, &unrealKnob })
        addAndMakeVisible(c);
    liminalityKnob.big = true;
    liminalityKnob.slider.setTooltip("LIMINALITY: how unstable, detuned and half-remembered every instrument is. 0 % clean, 40 % as set per instrument, 100 % deeply unsettling");
    addAndMakeVisible(liminalityKnob);
    instrumentsButton.onClick = [this] { showInstruments(!drawer.isVisible()); };
    addAndMakeVisible(instrumentsButton);
    environmentButton.onClick = [this] { showEnvironment(!places.isVisible()); };
    addAndMakeVisible(environmentButton);
    effectsButton.onClick = [this] { showEffects(!effects.isVisible()); };
    addAndMakeVisible(effectsButton);
    performButton.onClick = [this] { showPerform(!perform.isVisible()); };
    addAndMakeVisible(performButton);
    presetsButton.onClick = [this] { showPresets(!presetsPanel.isVisible()); };
    addAndMakeVisible(presetsButton);
    addAndMakeVisible(active);
    addChildComponent(drawer);
    addChildComponent(places);
    addChildComponent(effects);
    addChildComponent(perform);
    addChildComponent(presetsPanel);
    presetsPanel.onLoad = [this] {
        effects.focus(effects.focused());   // re-attach to what the preset set
        perform.refreshScenes();
        shelf.refresh();
        active.refresh();
        if (syncPlace()) repaint();
    };
    places.onChoose = [this] { if (syncPlace()) repaint(); };
    startViewfinder();
}

MainView::~MainView() = default;

bool MainView::drawerOpen() const {
    return drawer.isVisible() || places.isVisible() || effects.isVisible() || perform.isVisible() || presetsPanel.isVisible();
}

// A new place or Character is on screen: the page button names it, the effects page shows what it adds.
void MainView::sceneCommitted() {
    environmentButton.setButtonText("PLACE");
    effects.repaint();   // it shows what the place adds
}

void MainView::refreshControls() {
    if (presetsPanel.isVisible()) presetsPanel.poll();
    active.refresh();   // repaints itself when the instruments or the solo change
    shelf.refresh();
    places.refresh();
    drawer.refresh();          // a slot's instrument changed (the picker, a preset, automation)
    effects.refreshNames();
}









// One drawer at a time: opening one closes the others.
void MainView::closeDrawersExcept(juce::Component* keep) {
    const std::pair<juce::Component*, DrawerButton*> all[] { { &drawer, &instrumentsButton }, { &places, &environmentButton }, { &effects, &effectsButton },
                                                             { &perform, &performButton }, { &presetsPanel, &presetsButton } };
    for (const auto& [panel, button] : all)
        if (panel != keep && panel->isVisible()) {
            panel->setVisible(false);
            button->setToggleState(false, juce::dontSendNotification);
            repaint(button->getBounds());
        }
}

void MainView::showPresets(bool show) {
    if (show) closeDrawersExcept(&presetsPanel);
    presetsPanel.setVisible(show);
    if (show) presetsPanel.refreshUndo();
    presetsButton.setToggleState(show, juce::dontSendNotification);
    repaint(drawerArea());
    repaint(presetsButton.getBounds());
}

void MainView::showPerform(bool show) {
    if (show) {
        closeDrawersExcept(&perform);
        perform.refreshScenes();
    }
    perform.setVisible(show);
    performButton.setToggleState(show, juce::dontSendNotification);
    repaint(drawerArea());
    repaint(performButton.getBounds());
}

void MainView::showEffects(bool show) {
    if (show) {
        closeDrawersExcept(&effects);
        effects.focus(drawer.focused());   // the instrument last looked at
    }
    effects.setVisible(show);
    effectsButton.setToggleState(show, juce::dontSendNotification);
    repaint(drawerArea());
    repaint(effectsButton.getBounds());
}

void MainView::showEnvironment(bool show) {
    if (show) closeDrawersExcept(&places);
    places.setVisible(show);
    environmentButton.setToggleState(show, juce::dontSendNotification);
    repaint(drawerArea());
    repaint(environmentButton.getBounds());
}

void MainView::showInstruments(bool show) {
    if (show) closeDrawersExcept(&drawer);
    drawer.setVisible(show);
    instrumentsButton.setToggleState(show, juce::dontSendNotification);
    repaint(drawerArea());
    repaint(instrumentsButton.getBounds());
}

// The main view (Milestone 4): the scene fills the window; the viewfinder overlay names the
// place and its Characters, and holds the page tabs; the strip below is a film strip of
// the ten places, the instruments as objects, the four macros, and the outputs.
void MainView::resized() {
    {
        auto tabs = tabsArea();
        const std::pair<DrawerButton*, int> buttons[] { { &presetsButton, 86 }, { &performButton, 88 }, { &effectsButton, 80 }, { &environmentButton, 70 }, { &instrumentsButton, 110 } };
        for (const auto& [b, w] : buttons) {
            b->setBounds(tabs.removeFromRight(w));
            tabs.removeFromRight(4);
        }
    }
    Viewfinder::resized();   // the film strip, the Character chips; the scene is drawn again
    active.setBounds(44, 112, 400, 22);
    auto row = controlRow();
    shelf.setBounds(row.removeFromLeft(300));
    row.removeFromLeft(14);
    for (auto* k : { &presenceKnob, &scaleKnob, &activityKnob, &unrealKnob }) k->setBounds(row.removeFromLeft(62).withSizeKeepingCentre(62, 76));
    liminalityKnob.setBounds(liminalityArea());
    row.removeFromLeft(14);
    power.setBounds(row.removeFromRight(84).withSizeKeepingCentre(84, 28));
    row.removeFromRight(8);
    const int half = (row.getWidth() - 12) / 2;
    auto top = row.removeFromTop(38), bottom = row;
    master.setBounds(top.removeFromLeft(half).withSizeKeepingCentre(half, 34));
    top.removeFromLeft(12);
    instrument.setBounds(top.withSizeKeepingCentre(top.getWidth(), 34));
    ambience.setBounds(bottom.removeFromLeft(half).withSizeKeepingCentre(half, 34));
    bottom.removeFromLeft(12);
    space.setBounds(bottom.withSizeKeepingCentre(bottom.getWidth(), 34));
    drawer.setBounds(drawerArea());
    places.setBounds(drawerArea());
    effects.setBounds(drawerArea());
    perform.setBounds(drawerArea());
    presetsPanel.setBounds(drawerArea());
}















//==============================================================================
LiminalEditor::LiminalEditor(LiminalProcessor& p) : AudioProcessorEditor(p), processor(p), main(p) {
    const float scale = std::clamp(p.uiScale.load(), 0.85f, 1.5f);   // before the calls below resize us
    addAndMakeVisible(main);
    setResizable(true, true);
    setResizeLimits(juce::roundToInt(defaultWidth * 0.85f), juce::roundToInt(defaultHeight * 0.85f), juce::roundToInt(defaultWidth * 1.5f), juce::roundToInt(defaultHeight * 1.5f));
    getConstrainer()->setFixedAspectRatio(double(defaultWidth) / double(defaultHeight));
    setSize(juce::roundToInt(defaultWidth * scale), juce::roundToInt(defaultHeight * scale));
    ready = true;
    resized();
}

// The view is scaled to fit the window whole (2026-11-06): a host that gives the window another
// shape than ours (or a size outside the limits) gets the view centred, never cut off - the page
// buttons, PRESETS among them, stay on screen and clickable at every size.
float LiminalEditor::fitScale(int width, int height) {
    return std::max(0.05f, std::min(float(width) / float(defaultWidth), float(height) / float(defaultHeight)));
}

void LiminalEditor::resized() {
    const float scale = fitScale(getWidth(), getHeight());
    const float x = std::floor(0.5f * (float(getWidth()) - scale * float(defaultWidth)));
    const float y = std::floor(0.5f * (float(getHeight()) - scale * float(defaultHeight)));
    main.setBounds(0, 0, defaultWidth, defaultHeight);
    main.setTransform(juce::AffineTransform::scale(scale).translated(std::max(0.0f, x), std::max(0.0f, y)));
    if (ready) processor.uiScale.store(std::clamp(scale, 0.85f, 1.5f));
}

void LiminalEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff07100f)); }   // (only seen beside a view the host's window does not fit)

// Cmd+Z / Cmd+Shift+Z: undo / redo the last preset load (when the host passes the keys on).
bool LiminalEditor::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() != 'Z' && key.getKeyCode() != 'z') return false;
    if (!key.getModifiers().isCommandDown()) return false;
    const bool done = key.getModifiers().isShiftDown() ? processor.redoPreset() : processor.undoPreset();
    main.presetDrawer().refreshUndo();
    main.repaint();
    return done;
}

} // namespace liminal
