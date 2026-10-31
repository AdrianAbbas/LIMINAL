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
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1, 0.7f);
}

juce::Font ViewfinderLook::getComboBoxFont(juce::ComboBox&) { return font(10.5f); }

// The macro knobs: a thin ring with a brighter arc and a short tick, no box.
void ViewfinderLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider& s) {
    const auto area = juce::Rectangle<float>(float(x), float(y), float(width), float(height)).reduced(3.0f);
    const float r = std::min(area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const float angle = startAngle + position * (endAngle - startAngle);
    juce::Path track, value;
    track.addCentredArc(c.x, c.y, r, r, 0, startAngle, endAngle, true);
    value.addCentredArc(c.x, c.y, r, r, 0, startAngle, angle, true);
    g.setColour(ink.withAlpha(0.18f));
    g.strokePath(track, juce::PathStrokeType(2.0f));
    g.setColour(ink.withAlpha(s.isMouseOverOrDragging() ? 1.0f : 0.85f));
    g.strokePath(value, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0x2207100f));
    g.fillEllipse(juce::Rectangle<float>(r * 1.4f, r * 1.4f).withCentre(c));
    const juce::Point<float> tip(c.x + std::sin(angle) * r * 0.62f, c.y - std::cos(angle) * r * 0.62f);
    g.setColour(ink.withAlpha(0.9f));
    g.drawLine({ c.getPointOnCircumference(r * 0.25f, angle), tip }, 1.6f);
}
juce::Font ViewfinderLook::getPopupMenuFont() { return font(12.0f); }

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
    const auto text = parameter->getCurrentValueAsText();
    const auto unit = parameter->getLabel();
    const auto value = text + (unit == "dB" && text != "-inf" ? " dB" : unit == "%" ? " %" : "");
    // Label and value never overlap: both shrink together if the cell is narrow.
    float size = 10.5f;
    while (size > 7.0f && juce::GlyphArrangement::getStringWidth(font(size), label) + juce::GlyphArrangement::getStringWidth(font(size - 0.5f), value) + 6.0f > top.getWidth())
        size -= 0.5f;
    g.setFont(font(size));
    g.setColour(ink.withAlpha(0.6f));
    g.drawText(label, top, juce::Justification::centredLeft);
    g.setColour(ink.withAlpha(0.9f));
    g.setFont(font(size - 0.5f));
    g.drawText(value, top, juce::Justification::centredRight);
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
// Milestone 4 main view parts.
PlaceStrip::PlaceStrip(juce::AudioProcessorValueTreeState& s) : state(s) { refresh(); }

juce::Rectangle<int> PlaceStrip::frame(int place) const {
    const int gap = 8, w = (getWidth() - gap * 9) / 10;
    return { place * (w + gap), 0, w, getHeight() };
}

bool PlaceStrip::refresh() {
    const int now = int(std::lround(state.getRawParameterValue(params::environment)->load()));
    if (now == selected) return false;
    selected = now;
    repaint();
    return true;
}

void PlaceStrip::choose(int place) {
    auto* p = state.getParameter(params::environment);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(place)));
    p->endChangeGesture();
    refresh();
    if (onChoose) onChoose();
}

void PlaceStrip::paint(juce::Graphics& g) {
    const float scale = std::max(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs(scale - thumbScale) > 0.01f) {   // the miniatures, drawn once per scale
        thumbScale = scale;
        for (auto& t : thumbs) t = {};
    }
    const char* shortNames[] { "HOSPITAL", "MALL", "SCHOOL", "GARAGE", "POOL", "HOTEL", "SUBWAY", "SUPERMARKET", "OFFICE", "IMPOSSIBLE" };
    for (int i = 0; i < 10; ++i) {
        const auto r = frame(i).toFloat();
        auto& t = thumbs[std::size_t(i)];
        if (!t.isValid()) t = ui::PhotoScene::thumbnail(i);   // the photographic places' own picture
        if (!t.isValid()) t = ui::renderPlace(i, 0, int(r.getWidth()), int(r.getHeight()), scale, true).image;
        const bool on = i == selected, over = i == hover;
        g.setOpacity(on ? 1.0f : over ? 0.85f : 0.55f);
        g.drawImage(t, r, juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);
        g.setColour(ink.withAlpha(on ? 0.95f : over ? 0.5f : 0.22f));
        g.drawRect(r, on ? 1.6f : 1.0f);
        // Sprocket-like marks: this is film.
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(r.withHeight(11.0f).withY(r.getBottom() - 11.0f));
        g.setColour(ink.withAlpha(on ? 1.0f : 0.7f));
        g.setFont(font(7.5f, on));
        g.drawFittedText(shortNames[i], r.withTrimmedTop(r.getHeight() - 11.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        if (on) {
            g.setColour(rec);
            g.fillEllipse(r.getX() + 4.0f, r.getY() + 4.0f, 5.0f, 5.0f);
        }
    }
}

void PlaceStrip::mouseMove(const juce::MouseEvent& e) {
    int h = -1;
    for (int i = 0; i < 10; ++i)
        if (frame(i).contains(e.getPosition())) h = i;
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}
void PlaceStrip::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    repaint();
}
void PlaceStrip::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int i = 0; i < 10; ++i)
        if (frame(i).contains(e.getPosition())) choose(i);
}

CharacterStrip::CharacterStrip(juce::AudioProcessorValueTreeState& s) : state(s) { refresh(); }

bool CharacterStrip::refresh() {
    const int p = int(std::lround(state.getRawParameterValue(params::environment)->load()));
    const int c = int(std::lround(state.getRawParameterValue(params::character)->load()));
    if (p == place && c == selected) return false;
    place = p;
    selected = c;
    repaint();
    return true;
}

juce::Rectangle<float> CharacterStrip::chip(int character) const {
    float x = 0;
    const auto f = font(9.5f, true);
    for (int c = 0; c < character; ++c) x += juce::GlyphArrangement::getStringWidth(f, dsp::character(std::max(0, place), c).name) + 26.0f;
    return { x, 0, juce::GlyphArrangement::getStringWidth(f, dsp::character(std::max(0, place), character).name) + 14.0f, float(getHeight()) };
}

void CharacterStrip::choose(int character) {
    auto* p = state.getParameter(params::character);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(character)));
    p->endChangeGesture();
    refresh();
    if (onChoose) onChoose();
}

void CharacterStrip::paint(juce::Graphics& g) {
    for (int c = 0; c < dsp::characterCount; ++c) {
        const auto r = chip(c);
        const bool on = c == selected;
        if (on) {
            g.setColour(ink.withAlpha(0.85f));
            g.fillRoundedRectangle(r, 3.0f);
        } else {
            g.setColour(ink.withAlpha(hover == c ? 0.55f : 0.25f));
            g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        }
        g.setColour(on ? juce::Colour(0xff0b1412) : ink.withAlpha(hover == c ? 1.0f : 0.75f));
        g.setFont(font(9.5f, true));
        g.drawText(dsp::character(std::max(0, place), c).name, r, juce::Justification::centred);
    }
}
void CharacterStrip::mouseMove(const juce::MouseEvent& e) {
    int h = -1;
    for (int c = 0; c < dsp::characterCount; ++c)
        if (chip(c).contains(e.position)) h = c;
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}
void CharacterStrip::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    repaint();
}
void CharacterStrip::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int c = 0; c < dsp::characterCount; ++c)
        if (chip(c).contains(e.position)) choose(c);
}

namespace {
// The instruments as small line drawings, in a box of `r`.
void drawInstrument(juce::Graphics& g, int which, juce::Rectangle<float> r) {
    const float t = 1.3f;
    switch (which) {
    case 0: {   // an upright piano: body and keys
        g.drawRect(r.reduced(r.getWidth() * 0.1f, r.getHeight() * 0.08f), t);
        const auto keys = r.reduced(r.getWidth() * 0.16f, 0).withTrimmedTop(r.getHeight() * 0.55f).withHeight(r.getHeight() * 0.18f);
        g.drawRect(keys, 1.0f);
        for (int k = 1; k < 7; ++k) g.fillRect(keys.getX() + keys.getWidth() * float(k) / 7.0f, keys.getY(), 1.0f, keys.getHeight() * (k % 3 == 0 ? 1.0f : 0.6f));
        break;
    }
    case 1: {   // a cassette
        const auto body = r.reduced(r.getWidth() * 0.06f, r.getHeight() * 0.2f);
        g.drawRoundedRectangle(body, 2.0f, t);
        for (float fx : { 0.33f, 0.67f }) g.drawEllipse(juce::Rectangle<float>(body.getHeight() * 0.32f, body.getHeight() * 0.32f).withCentre({ body.getX() + body.getWidth() * fx, body.getCentreY() - 1 }), 1.0f);
        g.drawRect(body.reduced(body.getWidth() * 0.22f, 0).withTrimmedTop(body.getHeight() * 0.72f), 1.0f);
        break;
    }
    case 2: {   // a wine glass
        juce::Path p;
        const float cx = r.getCentreX(), top = r.getY() + r.getHeight() * 0.06f, bowl = r.getY() + r.getHeight() * 0.55f;
        p.startNewSubPath(cx - r.getWidth() * 0.22f, top);
        p.quadraticTo(cx - r.getWidth() * 0.24f, bowl, cx, bowl);
        p.quadraticTo(cx + r.getWidth() * 0.24f, bowl, cx + r.getWidth() * 0.22f, top);
        p.startNewSubPath(cx, bowl);
        p.lineTo(cx, r.getBottom() - r.getHeight() * 0.1f);
        p.startNewSubPath(cx - r.getWidth() * 0.15f, r.getBottom() - r.getHeight() * 0.1f);
        p.lineTo(cx + r.getWidth() * 0.15f, r.getBottom() - r.getHeight() * 0.1f);
        g.strokePath(p, juce::PathStrokeType(t));
        break;
    }
    case 3: {   // three voices: open mouths in a row
        for (int k = 0; k < 3; ++k) {
            const auto head = juce::Rectangle<float>(r.getWidth() * 0.26f, r.getHeight() * 0.42f).withCentre({ r.getX() + r.getWidth() * (0.22f + 0.28f * float(k)), r.getCentreY() - (k == 1 ? 4.0f : 0.0f) });
            g.drawEllipse(head, t);
            g.fillEllipse(juce::Rectangle<float>(head.getWidth() * 0.3f, head.getHeight() * 0.32f).withCentre({ head.getCentreX(), head.getCentreY() + head.getHeight() * 0.15f }));
        }
        break;
    }
    case 4: {   // a music box with its crank
        const auto box = r.reduced(r.getWidth() * 0.14f, r.getHeight() * 0.28f).translated(-r.getWidth() * 0.05f, r.getHeight() * 0.08f);
        g.drawRect(box, t);
        g.drawLine(box.getX(), box.getY() + box.getHeight() * 0.3f, box.getRight(), box.getY() + box.getHeight() * 0.3f, 1.0f);
        g.drawLine(box.getRight(), box.getCentreY(), box.getRight() + r.getWidth() * 0.12f, box.getCentreY(), 1.0f);
        g.drawLine(box.getRight() + r.getWidth() * 0.12f, box.getCentreY(), box.getRight() + r.getWidth() * 0.12f, box.getY(), 1.0f);
        break;
    }
    default: {   // the drone: stacked slow waves
        for (int k = 0; k < 3; ++k) {
            juce::Path p;
            const float y = r.getY() + r.getHeight() * (0.3f + 0.2f * float(k));
            p.startNewSubPath(r.getX() + 2, y);
            for (float x = 0; x <= 1.0f; x += 0.05f) p.lineTo(r.getX() + 2 + x * (r.getWidth() - 4), y + std::sin(x * 6.283f * (1.0f + 0.5f * float(k))) * r.getHeight() * 0.07f);
            g.strokePath(p, juce::PathStrokeType(1.0f + 0.3f * float(2 - k)));
        }
    }
    }
}
const char* shelfNames[] { "PIANO", "SYNTH", "GLASS", "CHOIR", "MUSIC BOX", "DRONE" };
const char* shelfIds[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
} // namespace

InstrumentShelf::InstrumentShelf(juce::AudioProcessorValueTreeState& s) : state(s) { refresh(); }

juce::Rectangle<float> InstrumentShelf::slot(int i) const {
    const float w = float(getWidth()) / 6.0f;
    return { float(i) * w, 0, w, float(getHeight()) };
}

bool InstrumentShelf::refresh() {
    std::array<bool, 6> now {};
    for (int i = 0; i < 6; ++i) now[std::size_t(i)] = state.getRawParameterValue(shelfIds[i])->load() > 0.5f;
    if (now == on) return false;
    on = now;
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
        drawInstrument(g, i, r.withTrimmedBottom(18).reduced(r.getWidth() * 0.18f, 4));
        g.setFont(font(8.5f, lit));
        g.drawFittedText(shelfNames[i], r.withTop(r.getBottom() - 16).toNearestInt(), juce::Justification::centred, 1, 0.7f);
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

MacroKnob::MacroKnob(juce::AudioProcessorValueTreeState& state, const char* id, juce::String text)
    : label(std::move(text)), parameter(state.getParameter(id)) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider);
    slider.setDoubleClickReturnValue(true, double(parameter->convertFrom0to1(parameter->getDefaultValue())));
}
void MacroKnob::resized() { slider.setBounds(getLocalBounds().withTrimmedBottom(26)); }
void MacroKnob::paint(juce::Graphics& g) {
    auto r = getLocalBounds().removeFromBottom(26);
    g.setColour(ink.withAlpha(0.7f));
    g.setFont(font(9.0f, true));
    g.drawText(label, r.removeFromTop(13), juce::Justification::centred);
    g.setColour(ink.withAlpha(0.5f));
    g.setFont(font(9.0f));
    g.drawText(parameter->getCurrentValueAsText() + " %", r, juce::Justification::centred);
}

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
    if (compact) {
        g.setFont(font(10.5f, open));
        g.drawFittedText(getButtonText(), getLocalBounds().translated(0, down ? 1 : 0), juce::Justification::centred, 1, 0.8f);
        return;
    }
    auto box = area.reduced(10.0f, 0.0f);
    auto glyph = box.removeFromLeft(16.0f).withSizeKeepingCentre(16.0f, 12.0f).translated(0, down ? 1.0f : 0.0f);
    if (icon == Icon::instruments) {   // three keys of a keyboard
        for (int k = 0; k < 3; ++k) g.drawRect(glyph.getX() + 5.0f * float(k), glyph.getY(), 5.5f, glyph.getHeight(), 1.0f);
        for (int k = 0; k < 2; ++k) g.fillRect(glyph.getX() + 3.5f + 5.0f * float(k), glyph.getY(), 3.0f, 7.0f);
    } else if (icon == Icon::presets) {   // a stack of cards
        for (int k = 0; k < 3; ++k) g.drawRect(glyph.getX() + 2.0f * float(k), glyph.getY() + 4.0f - 2.0f * float(k), glyph.getWidth() - 4.0f, 8.0f, 1.0f);
    } else if (icon == Icon::perform) {   // a fader between two scenes
        g.drawRect(glyph.getX(), glyph.getY() + 2.0f, 4.0f, 8.0f, 1.0f);
        g.drawRect(glyph.getRight() - 4.0f, glyph.getY() + 2.0f, 4.0f, 8.0f, 1.0f);
        g.fillRect(glyph.getX() + 4.0f, glyph.getCentreY() - 0.5f, glyph.getWidth() - 8.0f, 1.0f);
        g.fillRect(glyph.getCentreX() - 1.5f, glyph.getY(), 3.0f, glyph.getHeight());
    } else if (icon == Icon::effects) {   // three sliders on a rack
        for (int k = 0; k < 3; ++k) {
            const float x = glyph.getX() + 2.0f + 5.5f * float(k);
            g.fillRect(x, glyph.getY(), 1.0f, glyph.getHeight());
            g.fillRect(x - 2.0f, glyph.getY() + (k == 1 ? 7.0f : k == 0 ? 2.0f : 5.0f), 5.0f, 2.5f);
        }
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
namespace {
struct PlaceInfo { const char* shortName; const char* summary; const char* detail; juce::uint32 grade; };
const PlaceInfo placeInfo[dsp::placeCount] {
    { "HOSPITAL", "Cold corridor", "Dark, filtered, unsteady pitch, far-off stereo reflections. Monitors, vents, fluorescent hum.", 0x00000000 },
    { "SHOPPING MALL", "Huge atrium", "Washed out, very wide, endless soft decay. Escalator, far ventilation, the steel groaning.", 0x26ffc27a },
    { "SCHOOL", "Narrow hallway", "Detuned, worn tape, boxy, tight slap-back. Creaks, pipes, old ballasts ticking.", 0x2ae8c060 },
    { "PARKING GARAGE", "Concrete levels", "Heavy low-mids, dark top, hard early reflections. Deep rumble, cars far off.", 0x30ff7a1e },
    { "INDOOR POOL", "Tiled hall", "Bright, distinct echoes, long shimmering tail. Drips, lapping water, a low pump.", 0x2a38c4e8 },
    { "HOTEL", "Carpeted corridor", "Muffled through the wall, worn tape, a short soft room. The lift, voices next door, doors.", 0x2cd29a64 },
    { "SUBWAY", "Empty platform", "Bass resonance, metallic ringing, a long tunnel echo, sagging pitch. Singing rails, tunnel wind.", 0x28a6c46a },
    { "SUPERMARKET", "Frozen aisle", "Thin and nostalgic, a mild chorus, mains hum in the notes. Compressors cycling, a till beeping.", 0x24d6f2ff },
    { "OFFICE", "Open plan, forever", "Soft and distant, moving spectral notches, a wavering smeared delay. HVAC, drives, coil whine.", 0x22c4dcb4 },
    { "IMPOSSIBLE ROOM", "Should not exist", "Dissonant shifted partials, echoes a tritone higher, a rising tail, a drifting image.", 0x2ab27aff },
};
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
    focus(0);
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
    for (const auto& [y, text] : { std::pair<int, const char*> { presence.getY() - 16, "MACROS" }, { rate.getY() - 16, "EVOLUTION" }, { storeA.getY() - 18, "SCENES" } }) {
        g.setColour(ink.withAlpha(0.12f));
        g.fillRect(16, y - 4, getWidth() - 32, 1);
        g.setColour(ink.withAlpha(0.45f));
        g.drawText(text, 16, y, 120, 12, juce::Justification::centredLeft);
    }
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
    showCategory(3);
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
    scroll = 0;
    hover = -1;
    if (category == 4) userFiles = processor.userPresets();
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
    repaint();
}

juce::Rectangle<int> PresetDrawer::rowArea(int visibleRow) const { return { 16, 60 + visibleRow * 22, getWidth() - 32, 21 }; }

void PresetDrawer::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0x5507100f));
    g.fillRect(area);
    g.setColour(ink.withAlpha(0.18f));
    g.drawRect(area, 1.0f);
    g.setFont(font(9.5f));
    g.setColour(ink.withAlpha(0.45f));
    g.drawText("PRESETS  -  NOW: " + processor.presetName().toUpperCase(), area.reduced(16, 10).removeFromTop(14), juce::Justification::centredLeft);
    const int total = rows();
    for (int i = 0; i < visibleRows && scroll + i < total; ++i) {
        const auto r = rowArea(i).toFloat();
        const int row = scroll + i;
        const bool current = rowName(row).equalsIgnoreCase(processor.presetName());
        if (current || hover == i) {
            g.setColour(ink.withAlpha(current ? 0.14f : 0.07f));
            g.fillRoundedRectangle(r, 3.0f);
        }
        g.setColour(ink.withAlpha(current ? 1.0f : hover == i ? 0.9f : 0.72f));
        g.setFont(font(10.5f, current));
        g.drawText(rowName(row), r.reduced(10, 0), juce::Justification::centredLeft);
    }
    if (total == 0) {
        g.setColour(ink.withAlpha(0.45f));
        g.setFont(font(10.0f));
        g.drawText("NO PRESETS OF YOUR OWN YET - NAME ONE BELOW AND SAVE", rowArea(0), juce::Justification::centredLeft);
    }
    if (total > visibleRows) {   // a thin scroll indicator
        const float h = float(visibleRows * 22), y0 = 60.0f + h * float(scroll) / float(total), len = h * float(visibleRows) / float(total);
        g.setColour(ink.withAlpha(0.35f));
        g.fillRect(area.getRight() - 9.0f, y0, 2.0f, len);
    }
    if (hover >= 0 && scroll + hover < total) {
        g.setColour(ink.withAlpha(0.55f));
        g.setFont(font(9.0f));
        g.drawFittedText(rowDescription(scroll + hover).toUpperCase(), rowArea(visibleRows).withHeight(14).translated(0, -4), juce::Justification::centredLeft, 1, 0.8f);
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

void PresetDrawer::resized() {
    auto area = getLocalBounds().reduced(16, 10);
    area.removeFromTop(18);
    auto tabRow = area.removeFromTop(24);
    const int widths[] { 58, 92, 100, 62, 0 };   // by the length of each word
    for (int i = 0; i < 5; ++i) tabs[std::size_t(i)].setBounds(i < 4 ? tabRow.removeFromLeft(widths[i]) : tabRow);
    const int listBottom = rowArea(visibleRows - 1).getBottom() + 18;
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
    int h = -1;
    for (int i = 0; i < visibleRows; ++i)
        if (rowArea(i).contains(e.getPosition()) && scroll + i < rows()) h = i;
    setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h != hover) {
        hover = h;
        repaint();
    }
}

void PresetDrawer::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    repaint();
}

void PresetDrawer::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int i = 0; i < visibleRows; ++i)
        if (rowArea(i).contains(e.getPosition())) load(scroll + i);
}

void PresetDrawer::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) {
    const int next = std::clamp(scroll + (wheel.deltaY < 0 ? 2 : -2), 0, std::max(0, rows() - visibleRows));
    if (next != scroll) {
        scroll = next;
        repaint();
    }
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
    const auto name = juce::File::createLegalFileName(juce::String("LIMINAL ") + where.name + " - " + dsp::character(int(processor.placeNow().place()), processor.placeNow().characterIndex()).name + ".wav");
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
    : processor(p),
      master(p.parameters, params::master, "MASTER"),
      instrument(p.parameters, params::instrument, "INST"),
      ambience(p.parameters, params::ambience, "AMB"),
      space(p.parameters, params::space, "SPACE"),
      placeStrip(p.parameters), characterStrip(p.parameters), shelf(p.parameters),
      presenceKnob(p.parameters, params::presence, "PRESENCE"), scaleKnob(p.parameters, params::scale, "SCALE"),
      activityKnob(p.parameters, params::activity, "ACTIVITY"), unrealKnob(p.parameters, params::unreal, "UNREAL"),
      active(p.parameters),
      drawer(p.parameters),
      places(p.parameters),
      effects(p.parameters),
      perform(p),
      presetsPanel(p) {
    setLookAndFeel(&look);
    setOpaque(true);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, params::power, power);
    addAndMakeVisible(power);
    for (auto* c : { &instrument, &ambience, &space, &master }) addAndMakeVisible(c);
    for (auto* c : std::initializer_list<juce::Component*> { &placeStrip, &characterStrip, &shelf, &presenceKnob, &scaleKnob, &activityKnob, &unrealKnob })
        addAndMakeVisible(c);
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
    placeStrip.onChoose = [this] { if (syncPlace()) repaint(); };
    characterStrip.onChoose = [this] { if (syncPlace()) repaint(); };
    syncPlace();
    setSize(defaultWidth, defaultHeight);
    startTimerHz(30);
}

MainView::~MainView() {
    stopTimer();
    setLookAndFeel(nullptr);
}

// A new place: caption, button text and colour cast follow the Environment parameter.
bool MainView::syncPlace() {
    placeStrip.refresh();
    characterStrip.refresh();
    const int place = std::clamp(int(std::lround(processor.parameters.getRawParameterValue(params::environment)->load())), 0, dsp::placeCount - 1);
    const int ch = std::clamp(int(std::lround(processor.parameters.getRawParameterValue(params::character)->load())), 0, dsp::characterCount - 1);
    if (place == placeShown && ch == characterShown) return false;
    placeShown = place;
    characterShown = ch;
    // The Character's grade (Milestone 4 adds its light and flicker to the painted scene).
    const auto characterGrade = dsp::character(place, ch).grade;
    grade = juce::Colour(characterGrade != 0 ? characterGrade : placeInfo[place].grade);
    environmentButton.setButtonText("PLACE");
    effects.repaint();   // it shows what the place adds
    return true;
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
    characterStrip.setBounds(52, 88, 390, 20);
    active.setBounds(44, 112, 400, 22);
    auto strip = stripArea().reduced(32, 0);
    strip.removeFromTop(10);
    placeStrip.setBounds(strip.removeFromTop(52));
    strip.removeFromTop(8);
    auto row = strip.removeFromTop(76);
    shelf.setBounds(row.removeFromLeft(300));
    row.removeFromLeft(14);
    for (auto* k : { &presenceKnob, &scaleKnob, &activityKnob, &unrealKnob }) k->setBounds(row.removeFromLeft(62).withSizeKeepingCentre(62, 76));
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
    scene = {};
    sceneScale = 0;
}

void MainView::mouseMove(const juce::MouseEvent& e) { mouse = e.position; }

// The lights answering the music: each pulse brightens a fixture, its halo and its
// reflection (drawn over the cached scene, only where something changed).
void MainView::paintLights(juce::Graphics& g) {
    const auto count = std::min<std::size_t>(shownPulse.size(), scene.lights.size());
    for (std::size_t i = 0; i < count; ++i) {
        const float p = shownPulse[i];
        const auto& light = scene.lights[i];
        if (p > 0.005f) {
            g.setGradientFill(juce::ColourGradient(scene.lightColour.withAlpha(std::min(0.5f, 0.32f * p)), light.glow.getCentre(), scene.lightColour.withAlpha(0.0f),
                                                   light.glow.getCentre().translated(light.glow.getWidth() * 0.5f, 0), true));
            g.fillEllipse(light.glow);
            if (light.reflection.x >= 0) {
                const auto r = juce::Rectangle<float>(light.glow.getWidth() * 0.4f, light.glow.getHeight() * 0.9f).withCentre(light.reflection);
                g.setGradientFill(juce::ColourGradient(scene.lightColour.withAlpha(std::min(0.35f, 0.22f * p)), light.reflection, scene.lightColour.withAlpha(0.0f),
                                                       light.reflection.translated(0, r.getHeight() * 0.5f), true));
                g.fillEllipse(r);
            }
        } else if (p < -0.005f) {   // a failing tube: its glow dims
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0a0e0d).withAlpha(std::min(0.8f, -p)), light.glow.getCentre(), juce::Colour(0x000a0e0d),
                                                   light.glow.getCentre().translated(light.glow.getWidth() * 0.5f, 0), true));
            g.fillEllipse(light.glow);
        }
    }
}

void MainView::paint(juce::Graphics& g) {
    paintStarted = nowMs();
    const float scale = std::max(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    // A photographic scene (light passes composited live) where there is one; else the drawing.
    const bool photographic = photoPlace(std::max(0, placeShown));
    if (photographic && placeholderFor != placeShown) {
        photoPlaceholder = ui::PhotoScene::thumbnail(placeShown);
        placeholderFor = placeShown;
    }
    if (!photographic && (!scene.image.isValid() || std::abs(scale - sceneScale) > 0.01f || scene.place != placeShown || scene.character != characterShown)) {
        scene = ui::renderPlace(std::max(0, placeShown), std::max(0, characterShown), getWidth(), getHeight(), scale);
        sceneScale = scale;
    }
    const auto bounds = getLocalBounds().toFloat();
    if (photographic) scene.lights.clear();   // (the drawing's lights do not belong to the photograph)
    // Every repaint, however small its region, draws the whole scene under it: a base fill (so a
    // missing picture is never "nothing"), then the picture (opaque; clipped to the region).
    g.fillAll(juce::Colour(0xff0a0e0d));
    const juce::Image& picture = !photographic ? scene.image : showingPhoto() ? photoImage : photoPlaceholder;
    const juce::Image& frosted = !photographic ? scene.frosted : photoFrosted;
    lastPicture = picture;
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    g.drawImage(picture, bounds, juce::RectanglePlacement::stretchToFit);

    if (!photographic) paintLights(g);   // (a photograph's lights are its passes)
    else {   // a photograph's ceiling lights sit behind the viewfinder's captions: shade the top a little
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x66080b0a), 0, 0, juce::Colour(0x00080b0a), 0, 150, false));
        g.fillRect(bounds.withHeight(150));
    }
    const auto drawFrosted = [&](juce::Rectangle<int> area) {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(area);
        if (frosted.isValid()) g.drawImage(frosted, bounds, juce::RectanglePlacement::stretchToFit);
        else g.fillAll(juce::Colour(0xe00a0e0d));
    };
    if (clock < shimmerUntil) {   // Unreal: a band of the picture slips sideways, like worn tape
        const auto band = juce::Rectangle<float>(0, shimmerY, bounds.getWidth(), shimmerH);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(band.toNearestInt());
        g.drawImage(picture, bounds.translated(shimmerShift, 0), juce::RectanglePlacement::stretchToFit);
        g.setColour(ink.withAlpha(0.12f));
        g.fillRect(band.withHeight(1.0f).translated(0, shimmerH * 0.6f));
    }

    if (drawer.isVisible() || places.isVisible() || effects.isVisible() || perform.isVisible() || presetsPanel.isVisible())   // frosted glass behind the drawer
        drawFrosted(drawerArea());

    // Frosted control strip.
    const auto strip = stripArea();
    drawFrosted(strip);
    g.setColour(ink.withAlpha(0.18f));
    g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1);
    g.setColour(ink.withAlpha(0.08f));
    g.fillRect(strip.getX() + 32, placeStrip.getBottom() + 4, strip.getWidth() - 64, 1);
    // The wordmark, quiet, at the top of the viewfinder.
    g.setColour(ink.withAlpha(0.5f));
    g.setFont(font(12.0f, true).withExtraKerningFactor(0.6f));
    g.drawText("LIMINAL", juce::Rectangle<int>(getWidth() / 2 - 80, 26, 160, 20), juce::Justification::centred);

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
    const auto& where = dsp::profile(dsp::Place(std::clamp(placeShown, 0, dsp::placeCount - 1)));
    g.drawText(juce::String(where.name) + "  /  " + dsp::character(placeShown, std::max(0, characterShown)).name + "  /  " + where.location,
               captionArea().toFloat(), juce::Justification::centredLeft);
    // Evolution: a quiet row of ticks, lit by how much the place is changing right now.
    {
        auto box = activityArea().toFloat();
        g.setColour(ink.withAlpha(0.4f));
        g.setFont(font(8.5f));
        g.drawText("EVOLVING", box.removeFromLeft(62), juce::Justification::centredLeft);
        for (int i = 0; i < 12; ++i) {
            g.setColour(ink.withAlpha(i < activityLit ? 0.65f : 0.15f));
            g.fillRect(box.getX() + float(i) * 6.0f, box.getCentreY() - 2.0f, 3.0f, 4.0f);
        }
    }

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

bool MainView::photoPlace(int place) {
    auto& known = photoKnown[std::size_t(std::clamp(place, 0, 9))];
    if (known < 0) known = ui::PhotoScene::available(place) ? 1 : 0;
    return known == 1;
}

// The photographic scene, one frame: every light group's level from the music (notes swell the
// nearest bank), the place's events, the failing tube (the hum's stutter, from the audio
// thread), the Character and the mouse; the caustics drift on their loop. The compositing runs
// on the scene's own thread; this only asks for a look and picks up finished pictures.
void MainView::animatePhoto(bool still, juce::RectangleList<int>& dirty) {
    const int place = std::max(0, placeShown), character = std::max(0, characterShown);
    if (!photoPlace(place)) {
        if (photo != nullptr) {
            photo.reset();
            photoImage = {};
            photoFrosted = {};
            dirty.add(getLocalBounds());
        }
        return;
    }
    if (photo == nullptr || photo->place() != place) {
        photo = std::make_unique<ui::PhotoScene>(place);
        photoImage = {};
        photoFrosted = {};
        photoLevelsSent.clear();
        photoSwell.clear();
        photoCharacterSent = -1;
    }
    const bool settleNow = settle && (!photoImage.isValid() || character != photoCharacterSent);
    if (settleNow && photo->ensureLoaded()) {   // tests and snapshots: the first picture of a place or Character, composited now
        auto look = photo->characterLook(character);   // the Character's light (from the manifest)
        look.light = dsp::character(place, character).light;
        look.grade = grade.getARGB();
        photoImage = photo->renderNow(look);
        photoNeedsFrost = true;
    }
    if (!photo->loaded()) return;
    const auto& passes = photo->passes();
    if (photoSwell.size() != passes.size()) photoSwell.assign(passes.size(), 0.0f);
    const auto& ch = dsp::character(place, character);
    const int notes = processor.meters.notes.load(), events = processor.meters.events.load();
    // The failing tube follows the hum: the darkest it got since the last frame.
    const float lamp = processor.meters.lamp.exchange(1.0f);
    photoLamp = lamp < photoLamp ? lamp : photoLamp + 0.6f * (lamp - photoLamp);
    if (!still) {
        if (notes != photoNotesSeen) {   // the nearest bank of lights swells with the note
            for (std::size_t i = 0; i < passes.size(); ++i)
                if (passes[i].kind != "base" && passes[i].kind != "failing" && passes[i].level(character) > 0.05f) {
                    photoSwell[i] = std::max(photoSwell[i], 0.08f + 0.22f * processor.meters.velocity.load());
                    break;
                }
        }
        if (events != photoEventsSeen && passes.size() > 1)   // the place's own sounds: a light wavers
            photoSwell[std::size_t(1 + random.nextInt(int(passes.size()) - 1))] -= 0.18f;
    }
    photoNotesSeen = notes;
    photoEventsSeen = events;
    auto look = photo->defaultLook();
    for (std::size_t i = 0; i < passes.size(); ++i) {
        float level = passes[i].defaultLevel * passes[i].level(character);   // how the Character lights this group
        if (passes[i].flickers(character)) level *= 0.02f + 0.98f * photoLamp * photoLamp * photoLamp;
        if (!still) {
            level *= 1.0f + photoSwell[i];
            const auto a = passes[i].anchor;   // the light under the mouse warms a little
            if (a.x >= 0 && mouse.getDistanceFrom({ a.x * float(getWidth()), a.y * float(getHeight()) }) < 90.0f) level *= 1.08f;
        }
        look.levels[i] = std::max(0.0f, level);
        photoSwell[i] *= 0.88f;
        if (std::abs(photoSwell[i]) < 0.01f) photoSwell[i] = 0.0f;   // (a 1 % swell is invisible: stop compositing for it)
    }
    look.light = ch.light;
    look.grade = grade.getARGB();
    look.character = character;
    look.causticLevel *= photo->causticCharacter(character);
    if (photo->hasCaustics() && !still) {   // two steps per caustic frame: smooth, and no composite every tick
        const double steps = 2.0 * double(photo->causticFrames());
        look.causticPhase = float(std::floor(std::fmod(clock * double(photo->causticFps()) / double(photo->causticFrames()), 1.0) * steps) / steps);
    }
    else look.causticPhase = photoPhaseSent < 0 ? 0.0f : photoPhaseSent;
    bool changed = character != photoCharacterSent || photoLevelsSent.size() != look.levels.size() || std::abs(look.causticPhase - photoPhaseSent) > 1.0e-4f;
    // Only a light whose intensity really moved (over 2 % of its level, about the smallest step the
    // eye sees on a whole light group) asks for a new picture.
    for (std::size_t i = 0; !changed && i < look.levels.size(); ++i)
        changed = std::abs(look.levels[i] - photoLevelsSent[i]) > std::max(0.004f, 0.02f * std::max(look.levels[i], photoLevelsSent[i]));
    if (changed && !settleNow) {
        photo->request(look);
        photoLevelsSent = look.levels;
        photoPhaseSent = look.causticPhase;
        if (character != photoCharacterSent) photoNeedsFrost = true;
        photoCharacterSent = character;
    }
    if (settleNow) {
        photoLevelsSent = look.levels;
        photoPhaseSent = look.causticPhase;
        photoCharacterSent = character;
    }
    juce::Image next;
    if (photo->take(next)) {
        photoImage = next;
        dirty.add(getLocalBounds().withBottom(stripArea().getY()));
    }
    if (photoNeedsFrost && photoImage.isValid()) {   // frosted glass for the strip and the pages
        const auto small = photoImage.rescaled(std::max(1, photoImage.getWidth() / 16), std::max(1, photoImage.getHeight() / 16), juce::Graphics::mediumResamplingQuality);
        photoFrosted = juce::Image(juce::Image::ARGB, photoImage.getWidth() / 2, photoImage.getHeight() / 2, false);
        juce::Graphics g(photoFrosted);
        g.fillAll(juce::Colours::black);   // opaque (macOS keeps even RGB images as ARGB)
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(small, 0, 0, photoFrosted.getWidth(), photoFrosted.getHeight(), 0, 0, small.getWidth(), small.getHeight());
        g.fillAll(juce::Colour(0x8c0a0e0d));
        photoNeedsFrost = false;
        dirty.add(getLocalBounds());
    }
}

void MainView::paintOverChildren(juce::Graphics&) {
    const double now = nowMs();
    frames.painted(now - paintStarted, now);
}

void MainView::timerCallback() {
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
    // The lights: notes make them swell, the place's own sounds make one flicker, the
    // Character decides how unreliable the tubes are, the mouse warms the nearest one.
    // Eco quality (or slow frames) keeps them still.
    const bool eco = std::lround(processor.parameters.getRawParameterValue(params::quality)->load()) == 0;
    const int notes = processor.meters.notes.load(), events = processor.meters.events.load();
    const auto lightCount = std::min<std::size_t>(pulse.size(), scene.lights.size());
    if (!reduced && !eco && lightCount > 0) {
        if (notes != notesSeen)
            for (std::size_t i = 0; i < lightCount; ++i) pulse[i] = std::max(pulse[i], 0.9f - 0.06f * float(i));
        if (events != eventsSeen) pulse[std::size_t(random.nextInt(int(lightCount)))] = 0.7f;
        const float flicker = dsp::character(std::max(0, placeShown), std::max(0, characterShown)).flicker;
        if (clock >= nextFlicker) {
            nextFlicker = clock + (1.0 + 8.0 * random.nextDouble()) / (0.3 + 1.5 * flicker);
            pulse[std::size_t(random.nextInt(int(lightCount)))] = -0.85f;
        }
        for (std::size_t i = 0; i < lightCount; ++i) {
            float target = pulse[i];
            const auto& glow = scene.lights[i].glow;
            if (glow.getWidth() < 400.0f && glow.contains(mouse)) target = std::max(target, 0.25f);
            if (std::abs(target - shownPulse[i]) > 0.01f) {
                shownPulse[i] = target;
                dirty.add(glow.getSmallestIntegerContainer().expanded(2));
                if (scene.lights[i].reflection.x >= 0)
                    dirty.add(juce::Rectangle<float>(glow.getWidth() * 0.4f, glow.getHeight() * 0.9f).withCentre(scene.lights[i].reflection).getSmallestIntegerContainer().expanded(2));
            }
            pulse[i] *= pulse[i] < 0.0f ? 0.7f : 0.86f;   // a flicker is short; a swell fades
            if (std::abs(pulse[i]) < 0.01f) pulse[i] = 0.0f;
        }
        // Unreal: now and then the picture slips like worn tape.
        const float u = processor.parameters.getRawParameterValue(params::unreal)->load();
        if (u > 0.35f && clock >= nextShimmer) {
            shimmerY = float(40 + random.nextInt(std::max(1, stripArea().getY() - 80)));
            shimmerH = 6.0f + 18.0f * random.nextFloat();
            shimmerShift = (random.nextBool() ? 1.0f : -1.0f) * (4.0f + 10.0f * u);
            shimmerUntil = clock + 0.12 + 0.15 * random.nextDouble();
            nextShimmer = clock + (2.0 + 6.0 * random.nextDouble()) / u;
            dirty.add(juce::Rectangle<int>(0, int(shimmerY) - 1, getWidth(), int(shimmerH) + 2));
        } else if (shimmerUntil > 0 && clock >= shimmerUntil) {
            shimmerUntil = 0;
            dirty.add(juce::Rectangle<int>(0, int(shimmerY) - 1, getWidth(), int(shimmerH) + 2));
        }
    } else if (std::any_of(shownPulse.begin(), shownPulse.end(), [](float v) { return v != 0.0f; })) {
        shownPulse = {};
        pulse = {};
        dirty.add(getLocalBounds().withBottom(stripArea().getY()));
    }
    notesSeen = notes;
    eventsSeen = events;
    animatePhoto(reduced || eco, dirty);
    {   // the evolution indicator: at most a few repaints a second of a tiny area
        const int lit = std::clamp(int(std::lround(processor.meters.activity.load() * 12.0f)), 0, 12);
        if (lit != activityLit) {
            activityLit = lit;
            dirty.add(activityArea());
        }
    }
    if (presetsPanel.isVisible()) presetsPanel.poll();
    active.refresh();   // repaints itself when the instruments or the solo change
    placeStrip.refresh();
    characterStrip.refresh();
    shelf.refresh();
    places.refresh();
    if (syncPlace()) dirty.add(getLocalBounds());
    lastDirty = dirty;
    if (dirty.isEmpty()) {
        frames.painted(0.0, now);   // nothing to draw: the frame is done
        return;
    }
    for (const auto& r : dirty) repaint(r);
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

void LiminalEditor::resized() {
    const float scale = float(getWidth()) / float(defaultWidth);
    main.setBounds(0, 0, defaultWidth, defaultHeight);
    main.setTransform(juce::AffineTransform::scale(scale));
    if (ready) processor.uiScale.store(scale);
}

} // namespace liminal
