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
    g.setColour(ink.withAlpha(highlighted ? 1.0f : 0.8f));
    g.setFont(font(11.0f));
    g.drawText(on ? "POWER" : "POWER OFF", area.withTrimmedLeft(22), juce::Justification::centredLeft);
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
    g.drawText(text + (parameter->getLabel() == "dB" && text != "-inf" ? " dB" : ""), top, juce::Justification::centredRight);
}

void LevelControl::resized() { slider.setBounds(getLocalBounds().withTrimmedTop(16).withTrimmedLeft(-2).withTrimmedRight(-2)); }

//==============================================================================
LiminalEditor::LiminalEditor(LiminalProcessor& p)
    : AudioProcessorEditor(p), processor(p),
      master(p.parameters, params::master, "MASTER"),
      instrument(p.parameters, params::instrument, "INSTRUMENT"),
      ambience(p.parameters, params::ambience, "AMBIENCE"),
      space(p.parameters, params::space, "SPACE") {
    setLookAndFeel(&look);
    setOpaque(true);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, params::power, power);
    addAndMakeVisible(power);
    for (auto* c : { &instrument, &ambience, &space, &master }) addAndMakeVisible(c);
    setSize(defaultWidth, defaultHeight);
    startTimerHz(30);
}

LiminalEditor::~LiminalEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void LiminalEditor::resized() {
    auto strip = stripArea().reduced(32, 22);
    power.setBounds(strip.removeFromLeft(100).withSizeKeepingCentre(100, 28));
    strip.removeFromLeft(14);
    strip.removeFromRight(140);   // wordmark
    const int each = strip.getWidth() / 4;
    for (auto* c : { &instrument, &ambience, &space, &master }) {
        c->setBounds(strip.removeFromLeft(each).withTrimmedRight(22).withSizeKeepingCentre(each - 22, 40));
    }
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

    // Frosted control strip.
    const auto strip = stripArea();
    {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(strip);
        g.drawImage(scene.frosted, bounds, juce::RectanglePlacement::stretchToFit);
    }
    g.setColour(ink.withAlpha(0.18f));
    g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1);
    auto mark = strip.reduced(32, 22).removeFromRight(140).toFloat();
    g.setColour(ink.withAlpha(0.92f));
    g.setFont(font(17.0f, true).withExtraKerningFactor(0.45f));
    g.drawText("LIMINAL", mark.removeFromTop(26), juce::Justification::centredRight);
    g.setColour(ink.withAlpha(0.45f));
    g.setFont(font(9.5f));
    g.drawText("M0  -  TEST INSTRUMENT", mark, juce::Justification::centredRight);

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
    if (dirty.isEmpty()) {
        frames.painted(0.0, now);   // nothing to draw: the frame is done
        return;
    }
    for (const auto& r : dirty) repaint(r);
}

} // namespace liminal
