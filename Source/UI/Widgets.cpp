#include "UI/Widgets.h"
#include <cmath>

namespace liminal {

namespace {
using ui::style::ink;
using ui::style::rec;
using ui::style::font;
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
    setTitle(label);
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
        if (onHover) onHover(h);
    }
}

void PlaceStrip::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    repaint();
    if (onHover) onHover(-1);
}

void PlaceStrip::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked()) return;
    for (int i = 0; i < 10; ++i)
        if (frame(i).contains(e.getPosition())) choose(i);
}

CharacterStrip::CharacterStrip(juce::AudioProcessorValueTreeState& s) : state(s) { refresh(); }

// The chips follow the picture (show()), not the parameters: nothing to poll.
bool CharacterStrip::refresh() { return false; }

void CharacterStrip::show(int p, int c) {
    if (p == place && c == selected) return;
    place = p;
    selected = c;
    repaint();
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

void MacroKnob::resized() { slider.setBounds(big ? getLocalBounds().reduced(10, 6).withTrimmedBottom(32) : getLocalBounds().withTrimmedBottom(26)); }

void MacroKnob::paint(juce::Graphics& g) {
    if (big) {   // the LIMINALITY knob sits on the scene: its own dark glass plate
        const auto plate = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xc407100f));
        g.fillRect(plate);
        g.setColour(ink.withAlpha(0.18f));
        g.drawRect(plate, 1.0f);
    }
    auto r = getLocalBounds().reduced(big ? 6 : 0).removeFromBottom(big ? 32 : 26);
    g.setColour(ink.withAlpha(big ? 0.85f : 0.7f));
    g.setFont(font(big ? 10.5f : 9.0f, true));
    g.drawText(label, r.removeFromTop(big ? 16 : 13), juce::Justification::centred);
    g.setColour(ink.withAlpha(big ? 0.65f : 0.5f));
    g.setFont(font(big ? 10.0f : 9.0f));
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

} // namespace liminal
