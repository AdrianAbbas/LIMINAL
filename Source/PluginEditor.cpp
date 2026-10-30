#include "PluginEditor.h"
#include <cmath>

namespace liminal {

namespace {
const juce::Colour ink(0xffe6efe9);          // overlay text: a pale, slightly green white
const juce::Colour rec(0xffe0453a);

juce::Font font(float height, bool bold = false) {
    return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor(0.12f);
}

double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

int segmentsFor(float rms) {
    const float db = juce::Decibels::gainToDecibels(rms, -100.0f);
    return std::clamp(int(std::floor((db + 66.0f) / 9.0f)), 0, 6);   // -57 dB .. -12 dB
}
} // namespace

//==============================================================================
void ViewfinderLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                      float, juce::Slider::SliderStyle, juce::Slider&) {
    const float cy = float(y) + float(height) * 0.5f;
    g.setColour(ink.withAlpha(0.28f));
    g.fillRect(float(x), cy - 0.5f, float(width), 1.0f);
    g.setColour(ink.withAlpha(0.85f));
    g.fillRect(float(x), cy - 1.0f, sliderPos - float(x), 2.0f);
    g.fillRect(sliderPos - 1.0f, cy - 6.0f, 2.0f, 12.0f);
}

void ViewfinderLook::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) {
    const auto area = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto dot = juce::Rectangle<float>(9, 9).withCentre({ area.getX() + 8, area.getCentreY() });
    g.setColour(on ? juce::Colour(0xff9effc4) : ink.withAlpha(0.25f));
    g.fillEllipse(dot);
    if (on) {
        g.setColour(juce::Colour(0x339effc4));
        g.fillEllipse(dot.expanded(4));
    }
    g.setColour(ink.withAlpha(highlighted ? 1.0f : on ? 0.85f : 0.55f));
    g.setFont(font(11.0f));
    const auto text = b.getButtonText().isNotEmpty() ? b.getButtonText() : juce::String(on ? "POWER" : "POWER OFF");
    g.drawText(text, area.withTrimmedLeft(22), juce::Justification::centredLeft);
}

// Text buttons are words on the glass: an underline when selected, nothing else.
void ViewfinderLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool) {
    const auto area = b.getLocalBounds().toFloat();
    if (b.getToggleState()) {
        g.setColour(ink.withAlpha(0.85f));
        g.fillRect(area.getX() + 4, area.getBottom() - 3, area.getWidth() - 8, 1.5f);
    } else if (highlighted) {
        g.setColour(ink.withAlpha(0.25f));
        g.fillRect(area.getX() + 4, area.getBottom() - 3, area.getWidth() - 8, 1.0f);
    }
}

void ViewfinderLook::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool highlighted, bool) {
    g.setColour(ink.withAlpha(b.getToggleState() || highlighted ? 0.95f : 0.6f));
    g.setFont(font(11.0f));
    g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
LevelControl::LevelControl(juce::AudioProcessorValueTreeState& state, const char* id, juce::String text)
    : label(std::move(text)), parameter(state.getParameter(id)) {
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider);
    slider.setDoubleClickReturnValue(true, double(parameter->convertFrom0to1(parameter->getDefaultValue())));
}

void LevelControl::paint(juce::Graphics& g) {
    auto top = getLocalBounds().removeFromTop(16).toFloat();
    g.setFont(font(10.5f));
    g.setColour(ink.withAlpha(0.6f));
    g.drawText(label, top, juce::Justification::centredLeft);
    g.setColour(ink.withAlpha(0.9f));
    g.setFont(font(10.0f));
    const auto text = parameter->getCurrentValueAsText();
    const auto unit = parameter->getLabel();
    g.drawText(text + (unit == "dB" && text != "-inf" ? " dB" : unit == "%" ? " %" : ""), top, juce::Justification::centredRight);
}

void LevelControl::resized() { slider.setBounds(getLocalBounds().withTrimmedTop(16).withTrimmedLeft(-2).withTrimmedRight(-2)); }

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
} // namespace

//==============================================================================
DrawerButton::DrawerButton(const juce::String& text, Icon i) : juce::Button(text), icon(i) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setClickingTogglesState(false);
    setTooltip("Open or close the " + text.toLowerCase() + " drawer");
}

void DrawerButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    const bool open = getToggleState();
    g.setColour(open ? ink.withAlpha(0.88f) : juce::Colour(0x3307100f).withMultipliedAlpha(highlighted ? 2.0f : 1.0f));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(ink.withAlpha(open ? 0.95f : highlighted ? 0.85f : 0.5f));
    g.drawRoundedRectangle(area, 4.0f, highlighted || open ? 1.4f : 1.0f);
    const juce::Colour fg = open ? juce::Colour(0xff0b1412) : ink.withAlpha(highlighted ? 1.0f : 0.82f);
    g.setColour(fg);
    auto box = area.reduced(10.0f, 0.0f);
    auto glyph = box.removeFromLeft(16.0f).withSizeKeepingCentre(16.0f, 12.0f).translated(0, down ? 1.0f : 0.0f);
    if (icon == Icon::instruments) {   // three keys of a keyboard
        for (int k = 0; k < 3; ++k) g.drawRect(glyph.getX() + 5.0f * float(k), glyph.getY(), 5.5f, glyph.getHeight(), 1.0f);
        for (int k = 0; k < 2; ++k) g.fillRect(glyph.getX() + 3.5f + 5.0f * float(k), glyph.getY(), 3.0f, 7.0f);
    } else {   // a doorway in perspective
        juce::Path p;
        p.startNewSubPath(glyph.getX(), glyph.getBottom());
        p.lineTo(glyph.getX() + 4, glyph.getY() + 2);
        p.lineTo(glyph.getRight() - 4, glyph.getY() + 2);
        p.lineTo(glyph.getRight(), glyph.getBottom());
        g.strokePath(p, juce::PathStrokeType(1.0f));
        g.drawRect(glyph.getCentreX() - 2.0f, glyph.getY() + 5.0f, 4.0f, glyph.getHeight() - 5.0f, 1.0f);
    }
    box.removeFromLeft(7.0f);
    g.setFont(font(11.0f, open));
    g.drawText(getButtonText(), box.translated(0, down ? 1.0f : 0.0f), juce::Justification::centredLeft);
    // A small chevron: which way the drawer will move.
    const float cx = area.getRight() - 12.0f, cy = area.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath(cx - 3.0f, cy + (open ? 2.0f : -2.0f));
    chevron.lineTo(cx, cy + (open ? -1.5f : 1.5f));
    chevron.lineTo(cx + 3.0f, cy + (open ? 2.0f : -2.0f));
    g.strokePath(chevron, juce::PathStrokeType(1.2f));
}

//==============================================================================
ActiveInstruments::ActiveInstruments(juce::AudioProcessorValueTreeState& s) : state(s) {
    refresh();
}

bool ActiveInstruments::refresh() {
    std::array<bool, params::instrumentCount> now {};
    for (int i = 0; i < params::instrumentCount; ++i) now[std::size_t(i)] = state.getRawParameterValue(instrumentInfo[std::size_t(i)].on)->load() > 0.5f;
    const int s = int(std::lround(state.getRawParameterValue(params::solo)->load()));
    if (now == on && s == solo) return false;
    on = now;
    solo = s;
    layout();
    repaint();
    return true;
}

juce::String ActiveInstruments::text() const {
    juce::StringArray list;
    for (int i = 0; i < params::instrumentCount; ++i)
        if (on[std::size_t(i)]) list.add(instrumentInfo[std::size_t(i)].shortName);
    return list.isEmpty() ? juce::String("NO INSTRUMENT ON") : list.joinIntoString(" + ");
}

void ActiveInstruments::resized() { layout(); }

void ActiveInstruments::layout() {
    names.clear();
    const auto f = font(11.5f, true);
    float x = 64.0f;   // after "PLAYING"
    for (int i = 0; i < params::instrumentCount; ++i) {
        if (!on[std::size_t(i)]) continue;
        const float w = juce::GlyphArrangement::getStringWidth(f, instrumentInfo[std::size_t(i)].shortName) + 4.0f;
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
        g.drawText(instrumentInfo[std::size_t(n.index)].shortName, n.area, juce::Justification::centred);
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
InstrumentDrawer::InstrumentDrawer(juce::AudioProcessorValueTreeState& state) {
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
        for (const auto& [id, label] : info.controls) {
            page.controls.push_back(std::make_unique<LevelControl>(state, id, label));
            addChildComponent(*page.controls.back());
        }
    }
    auto& synth = pages[1];
    synth.glide = std::make_unique<juce::ToggleButton>("GLIDE  (OFF: NOTES START ON PITCH)");
    synth.glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, params::synthGlide, *synth.glide);
    addChildComponent(*synth.glide);
    synth.controls.push_back(std::make_unique<LevelControl>(state, params::synthGlideTime, "GLIDE TIME"));
    addChildComponent(*synth.controls.back());
    auto& drone = pages[5];
    drone.controls.push_back(std::make_unique<LevelControl>(state, params::droneAttack, "ATTACK"));
    addChildComponent(*drone.controls.back());
    focus(0);
}

void InstrumentDrawer::focus(int index) {
    current = std::clamp(index, 0, count - 1);
    for (int i = 0; i < count; ++i) {
        auto& page = pages[std::size_t(i)];
        const bool shown = i == current;
        page.tab.setToggleState(shown, juce::dontSendNotification);
        for (auto& c : page.controls) c->setVisible(shown);
        if (page.glide != nullptr) page.glide->setVisible(shown);
    }
    repaint();
}

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
}

void InstrumentDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    area.removeFromTop(18);
    for (auto& page : pages) {
        auto row = area.removeFromTop(32);
        page.on.setBounds(row.removeFromLeft(168).withSizeKeepingCentre(168, 24));
        page.level->setBounds(row.withSizeKeepingCentre(row.getWidth(), 31));
    }
    area.removeFromTop(10);
    auto tabs = area.removeFromTop(26);
    const int tabWidth = tabs.getWidth() / count;
    for (auto& page : pages) page.tab.setBounds(tabs.removeFromLeft(tabWidth));
    area.removeFromTop(8);
    // Three columns: the six controls in two rows, then any extra row (Glide, Attack).
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
LiminalEditor::LiminalEditor(LiminalProcessor& p)
    : AudioProcessorEditor(p), processor(p),
      master(p.parameters, params::master, "MASTER"),
      instrument(p.parameters, params::instrument, "INST"),
      ambience(p.parameters, params::ambience, "AMB"),
      space(p.parameters, params::space, "SPACE"),
      active(p.parameters),
      drawer(p.parameters) {
    setLookAndFeel(&look);
    setOpaque(true);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, params::power, power);
    addAndMakeVisible(power);
    for (auto* c : { &instrument, &ambience, &space, &master }) addAndMakeVisible(c);
    instrumentsButton.onClick = [this] { showInstruments(!drawer.isVisible()); };
    addAndMakeVisible(instrumentsButton);
    addAndMakeVisible(active);
    addChildComponent(drawer);
    setSize(defaultWidth, defaultHeight);
    startTimerHz(30);
}

LiminalEditor::~LiminalEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void LiminalEditor::showInstruments(bool show) {
    drawer.setVisible(show);
    instrumentsButton.setToggleState(show, juce::dontSendNotification);
    repaint(drawerArea());
    repaint(instrumentsButton.getBounds());
}

void LiminalEditor::resized() {
    auto strip = stripArea().reduced(32, 0);
    active.setBounds(strip.removeFromTop(34).withTrimmedTop(10).withTrimmedRight(160));
    strip = strip.withTrimmedBottom(16);
    power.setBounds(strip.removeFromLeft(92).withSizeKeepingCentre(92, 28));
    strip.removeFromLeft(4);
    instrumentsButton.setBounds(strip.removeFromLeft(146).withSizeKeepingCentre(146, 30));
    strip.removeFromLeft(18);
    strip.removeFromRight(124);   // wordmark
    const int each = strip.getWidth() / 4;
    for (auto* c : { &instrument, &ambience, &space, &master }) {
        c->setBounds(strip.removeFromLeft(each).withTrimmedRight(18).withSizeKeepingCentre(each - 18, 40));
    }
    drawer.setBounds(drawerArea());
    scene = {};
    sceneScale = 0;
}

void LiminalEditor::paint(juce::Graphics& g) {
    paintStarted = nowMs();
    const float scale = std::max(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (!scene.image.isValid() || std::abs(scale - sceneScale) > 0.01f) {
        scene = ui::renderCorridor(getWidth(), getHeight(), scale);
        sceneScale = scale;
    }
    const auto bounds = getLocalBounds().toFloat();
    g.drawImage(scene.image, bounds, juce::RectanglePlacement::stretchToFit);

    // The failing light: its glow dims while it stutters.
    if (flickerDim > 0.0f && scene.lights.size() > 1) {
        const auto r = scene.lights[1];
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff10181a).withAlpha(0.75f * flickerDim), r.getCentre(),
                                               juce::Colour(0x0010181a), r.getCentre().translated(r.getWidth() * 0.5f, 0), true));
        g.fillEllipse(r);
    }

    if (drawer.isVisible()) {   // frosted glass behind the drawer
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(drawerArea());
        g.drawImage(scene.frosted, bounds, juce::RectanglePlacement::stretchToFit);
    }

    // Frosted control strip.
    const auto strip = stripArea();
    {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(strip);
        g.drawImage(scene.frosted, bounds, juce::RectanglePlacement::stretchToFit);
    }
    g.setColour(ink.withAlpha(0.18f));
    g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1);
    auto mark = strip.reduced(32, 0).withTrimmedBottom(16).withTrimmedTop(34).removeFromRight(124).toFloat();
    g.setColour(ink.withAlpha(0.92f));
    g.setFont(font(17.0f, true).withExtraKerningFactor(0.45f));
    g.drawText("LIMINAL", mark, juce::Justification::centredRight);
    g.setColour(ink.withAlpha(0.08f));
    g.fillRect(strip.getX() + 32, strip.getY() + 34, strip.getWidth() - 64, 1);

    // Viewfinder frame corners and centre mark.
    const auto frame = getLocalBounds().withTrimmedBottom(strip.getHeight()).reduced(18).toFloat();
    g.setColour(ink.withAlpha(0.55f));
    const float l = 26.0f, t = 1.5f;
    for (const auto corner : { frame.getTopLeft(), frame.getTopRight(), frame.getBottomLeft(), frame.getBottomRight() }) {
        const float sx = corner.x < frame.getCentreX() ? 1.0f : -1.0f, sy = corner.y < frame.getCentreY() ? 1.0f : -1.0f;
        g.fillRect(juce::Rectangle<float>(corner, corner.translated(sx * l, sy * t)));
        g.fillRect(juce::Rectangle<float>(corner, corner.translated(sx * t, sy * l)));
    }
    const auto c = frame.getCentre();
    g.setColour(ink.withAlpha(0.22f));
    g.fillRect(c.x - 9, c.y - 0.5f, 18.0f, 1.0f);
    g.fillRect(c.x - 0.5f, c.y - 9, 1.0f, 18.0f);

    // REC and the location caption.
    const auto recBox = recArea().toFloat();
    if (recOn) {
        g.setColour(rec);
        g.fillEllipse(juce::Rectangle<float>(10, 10).withCentre({ recBox.getX() + 9, recBox.getCentreY() }));
    }
    g.setColour(ink.withAlpha(0.9f));
    g.setFont(font(13.0f, true));
    g.drawText("REC", recBox.withTrimmedLeft(22), juce::Justification::centredLeft);
    g.setColour(ink.withAlpha(0.6f));
    g.setFont(font(10.5f));
    g.drawText("VACANT HOSPITAL  /  EAST WING, LEVEL 2", juce::Rectangle<float>(52, 50, 400, 16), juce::Justification::centredLeft);

    // Time stamp and battery-style meters.
    g.setColour(ink.withAlpha(0.9f));
    g.setFont(font(13.0f));
    g.drawText(clockText, clockArea(), juce::Justification::centredRight);
    auto meters = meterArea().toFloat();
    for (const auto& [name, segments] : { std::pair<const char*, int> { "AMB", ambienceSegments }, { "INST", instrumentSegments } }) {
        auto box = meters.removeFromRight(46).withSizeKeepingCentre(40, 12);
        g.setColour(ink.withAlpha(0.7f));
        g.drawRect(box, 1.0f);
        g.fillRect(box.getRight(), box.getCentreY() - 2.5f, 2.5f, 5.0f);
        auto inner = box.reduced(2.5f);
        const float seg = inner.getWidth() / 6.0f;
        for (int i = 0; i < segments; ++i) g.fillRect(inner.getX() + float(i) * seg + 0.5f, inner.getY(), seg - 1.0f, inner.getHeight());
        g.setFont(font(10.0f));
        g.setColour(ink.withAlpha(0.6f));
        g.drawText(name, meters.removeFromRight(40), juce::Justification::centredRight);
        meters.removeFromRight(14);
    }
}

void LiminalEditor::paintOverChildren(juce::Graphics&) {
    const double now = nowMs();
    frames.painted(now - paintStarted, now);
}

void LiminalEditor::timerCallback() {
    double elapsed = 0;
    const double now = nowMs();
    if (!frames.beginFrame(now, elapsed)) return;
    clock += elapsed;
    // Slow frames for two seconds: stop the light's flicker (the cheapest animation to lose).
    const bool reduced = frames.slowFor(now);
    juce::RectangleList<int> dirty;

    const bool nextRec = std::fmod(clock, 1.2) < 0.75;
    if (nextRec != recOn) {
        recOn = nextRec;
        dirty.add(recArea());
    }
    const auto text = juce::Time::getCurrentTime().formatted("%b %d %Y   %H:%M:%S").toUpperCase();
    if (text != clockText) {
        clockText = text;
        dirty.add(clockArea());
    }
    // Meters fall slowly, rise at once.
    instrumentShown = std::max(processor.meters.instrument.load(), instrumentShown * 0.82f);
    ambienceShown = std::max(processor.meters.ambience.load(), ambienceShown * 0.82f);
    const int inst = segmentsFor(instrumentShown), amb = segmentsFor(ambienceShown);
    if (inst != instrumentSegments || amb != ambienceSegments) {
        instrumentSegments = inst;
        ambienceSegments = amb;
        dirty.add(meterArea());
    }
    float dim = 0.0f;
    if (!reduced) {
        if (clock >= flickerNext) {
            flickerUntil = clock + 0.15 + 0.5 * random.nextDouble();
            flickerNext = clock + 4.0 + 9.0 * random.nextDouble();
        }
        if (clock < flickerUntil) {
            if (clock >= flickerToggle) flickerToggle = clock + 0.03 + 0.08 * random.nextDouble();
            dim = std::fmod(flickerToggle * 37.0, 1.0) < 0.55 ? 0.85f : 0.1f;
        }
    }
    if (dim != flickerDim) {
        flickerDim = dim;
        if (scene.lights.size() > 1) dirty.add(scene.lights[1].getSmallestIntegerContainer().expanded(2));
    }
    active.refresh();   // repaints itself when the instruments or the solo change
    if (dirty.isEmpty()) {
        frames.painted(0.0, now);   // nothing to draw: the frame is done
        return;
    }
    for (const auto& r : dirty) repaint(r);
}

} // namespace liminal
