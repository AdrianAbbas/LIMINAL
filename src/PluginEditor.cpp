#include "PluginEditor.h"
using namespace liminal;
namespace
{
const juce::Colour bg{0xff11191b}, panel{0xff1b2729}, ink{0xffe3e3d5}, accent{0xff8db4aa}, dim{0xff829493};
const juce::StringArray groupNames{
    "Room",         "Electrical",     "Ventilation", "Structure",      "Atmosphere",  "Distant",
    "Substructure", "Evolution",      "Performance", "Output",         "Scenes",      "Forgotten Piano",
    "Faded Synth",  "Glass Memories", "Empty Choir", "Lost Music Box", "Hollow Drone"};
} // namespace
LiminalEditor::Control::Control(LiminalProcessor &p, std::size_t i, bool rotary)
{
    const auto &spec = specs[i];
    label.setText(spec.name, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, ink);
    label.setFont(juce::FontOptions(12));
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);
    slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag
                                 : juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setTooltip(juce::String(spec.name) + " | " + spec.id + " | Double-click to reset");
    slider.setDoubleClickReturnValue(true, spec.initial);
    addAndMakeVisible(slider);
    attachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, spec.id, slider);
    slider.textFromValueFunction = [places = spec.discrete ? 0 : (spec.max > 10 ? 1 : 2)](double value)
    { return juce::String(value, places); };
    slider.valueFromTextFunction = [](const juce::String &text) { return text.getDoubleValue(); };
    slider.updateText();
}
void LiminalEditor::Control::resized()
{
    auto b = getLocalBounds();
    label.setBounds(b.removeFromTop(21));
    slider.setBounds(b);
}
LiminalEditor::LayerStrip::LayerStrip(LiminalProcessor &p, Param on, Param gain, const char *text,
                                      std::function<void()> select)
{
    addAndMakeVisible(enabled);
    addAndMakeVisible(name);
    addAndMakeVisible(level);
    name.setButtonText(text);
    name.onClick = std::move(select);
    enabled.setTooltip(juce::String(text) + " enable");
    level.setSliderStyle(juce::Slider::LinearHorizontal);
    level.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    level.setTooltip(juce::String(text) + " level");
    level.setDoubleClickReturnValue(true, specs[gain].initial);
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, specs[on].id, enabled);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state, specs[gain].id, level);
    level.textFromValueFunction = [](double value) { return juce::String(value, 2); };
    level.valueFromTextFunction = [](const juce::String &text) { return text.getDoubleValue(); };
    level.updateText();
}
void LiminalEditor::LayerStrip::resized()
{
    auto b = getLocalBounds().reduced(3);
    auto row = b.removeFromTop(26);
    enabled.setBounds(row.removeFromLeft(25));
    name.setBounds(row);
    level.setBounds(b);
}
LiminalEditor::LiminalEditor(LiminalProcessor &p) : AudioProcessorEditor(p), processor(p), room(p)
{
    look.setColour(juce::ResizableWindow::backgroundColourId, bg);
    look.setColour(juce::Slider::thumbColourId, accent);
    look.setColour(juce::Slider::rotarySliderFillColourId, accent);
    look.setColour(juce::Slider::trackColourId, accent);
    look.setColour(juce::Slider::textBoxTextColourId, ink);
    look.setColour(juce::Slider::textBoxBackgroundColourId, bg);
    look.setColour(juce::TextButton::buttonColourId, panel);
    look.setColour(juce::TextButton::textColourOffId, ink);
    look.setColour(juce::ComboBox::backgroundColourId, panel);
    look.setColour(juce::ComboBox::textColourId, ink);
    look.setColour(juce::ToggleButton::textColourId, ink);
    setLookAndFeel(&look);
    for (auto *c : std::initializer_list<juce::Component *>{&room, &presets, &groups, &modeBox, &prev, &next,
                                                            &save, &load, &active, &follow, &mute, &reset,
                                                            &sceneA, &sceneB, &live, &status})
        addAndMakeVisible(c);
    const auto &f = factoryPresets();
    for (int i = 0; i < 15; ++i)
    {
        if (i == 0 || juce::String(f[std::size_t(i)].category) != f[std::size_t(i - 1)].category)
            presets.addSectionHeading(f[std::size_t(i)].category);
        presets.addItem(f[std::size_t(i)].name, i + 1);
    }
    presets.setText(p.presetName(), juce::dontSendNotification);
    presets.onChange = [this]
    {
        selectedPreset = presets.getSelectedId() - 1;
        if (selectedPreset >= 0)
        {
            processor.loadFactory(selectedPreset);
            status.setText(factoryPresets()[std::size_t(selectedPreset)].description,
                           juce::dontSendNotification);
        }
    };
    prev.onClick = [this] { presets.setSelectedId((selectedPreset + 14) % 15 + 1); };
    next.onClick = [this] { presets.setSelectedId((selectedPreset + 1) % 15 + 1); };
    const Param ons[]{electricalOn, ventilationOn, structureOn, atmosphereOn, distantOn, subOn};
    const Param levels[]{electricalLevel, ventilationLevel, structureLevel,
                         atmosphereLevel, distantLevel,     subLevel};
    for (int i = 0; i < 6; ++i)
    {
        auto group = groupNames[i + 1];
        layers[std::size_t(i)] = std::make_unique<LayerStrip>(p, ons[i], levels[i], group.toRawUTF8(),
                                                              [this, i] { groups.setSelectedId(i + 2); });
        addAndMakeVisible(*layers[std::size_t(i)]);
    }
    groups.addItemList(groupNames, 1);
    groups.onChange = [this] { selectGroup(); };
    groups.setSelectedId(1);
    modeBox.addItem("Environment", 1);
    modeBox.addItem("Instrument / MIDI", 2);
    modeAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "mode", modeBox);
    activeAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "active", active);
    followAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "follow", follow);
    muteAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "mute", mute);
    const std::size_t ids[]{presence, scale, activity, unreal, master, morph};
    for (std::size_t i = 0; i < 6; ++i)
    {
        macros[i] = std::make_unique<Control>(p, ids[i], true);
        addAndMakeVisible(*macros[i]);
    }
    save.onClick = [this] { browsePreset(true); };
    load.onClick = [this] { browsePreset(false); };
    reset.onClick = [this] { processor.resetEvolution(); };
    sceneA.onClick = [this]
    {
        processor.captureScene(false);
        status.setText("Captured A. Capture B after editing; morph blends continuous environmental controls.",
                       juce::dontSendNotification);
    };
    sceneB.onClick = [this]
    {
        processor.captureScene(true);
        status.setText("Captured B. Morph active. Live controls exits morph for editing.",
                       juce::dontSendNotification);
    };
    live.onClick = [this]
    {
        processor.disableScenes();
        status.setText("Live controls active. Stored A/B slots retained.", juce::dontSendNotification);
    };
    room.setTooltip("Drag S (source) or L (listener). Room geometry changes the acoustic model.");
    status.setColour(juce::Label::textColourId, dim);
    status.setFont(juce::FontOptions(12));
    status.setText("Procedural environmental synthesis / approximate architectural acoustics",
                   juce::dontSendNotification);
    addAndMakeVisible(instrumentPage);
    const Param instrumentOns[]{pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn};
    const Param instrumentLevels[]{pianoLevel, synthLevel, glassLevel, choirLevel, boxLevel, droneLevel};
    for (int i = 0; i < 6; ++i)
    {
        instrumentCards[i] = std::make_unique<LayerStrip>(p, instrumentOns[i], instrumentLevels[i],
                                                          groupNames[i + 11].toRawUTF8(),
                                                          [this, i] { groups.setSelectedId(i + 12); });
        addAndMakeVisible(*instrumentCards[i]);
    }
    instrumentPage.onClick = [this]
    {
        showingInstruments = !showingInstruments;
        instrumentPage.setButtonText(showingInstruments ? "ENVIRONMENT" : "INSTRUMENTS");
        resized();
    };
    instrumentPage.setButtonText("ENVIRONMENT");
    groups.setSelectedId(12);
    busControls[0] = std::make_unique<Control>(p, instrumentBusLevel);
    busControls[1] = std::make_unique<Control>(p, ambienceBusLevel);
    for (auto &control : busControls)
        addAndMakeVisible(*control);
    setResizable(true, true);
    setResizeLimits(1040, 760, 1600, 1100);
    setSize(1160, 820);
    startTimerHz(20);
}
LiminalEditor::~LiminalEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void LiminalEditor::selectGroup()
{
    details.clear();
    auto name = groups.getText();
    for (std::size_t i = 0; i < count; ++i)
        if (name == specs[i].group)
        {
            auto c = std::make_unique<Control>(processor, i);
            addAndMakeVisible(*c);
            details.push_back(std::move(c));
        }
    resized();
}
void LiminalEditor::resized()
{
    auto b = getLocalBounds().reduced(22);
    b.removeFromTop(40);
    auto top = b.removeFromTop(34);
    prev.setBounds(top.removeFromLeft(30));
    presets.setBounds(top.removeFromLeft(270));
    next.setBounds(top.removeFromLeft(30));
    top.removeFromLeft(10);
    save.setBounds(top.removeFromLeft(55));
    load.setBounds(top.removeFromLeft(55));
    top.removeFromLeft(10);
    modeBox.setBounds(top.removeFromLeft(170));
    active.setBounds(top.removeFromLeft(100));
    follow.setBounds(top.removeFromLeft(110));
    mute.setBounds(top);
    b.removeFromTop(16);
    status.setBounds(b.removeFromBottom(26));
    auto bottom = b.removeFromBottom(166);
    int macroWidth = (bottom.getWidth() - 80) / 6;
    for (auto &c : macros)
        c->setBounds(bottom.removeFromLeft(macroWidth).reduced(5));
    b.removeFromBottom(12);
    auto side = b.removeFromRight(420);
    b.removeFromRight(16);
    auto strip = b.removeFromLeft(150);
    b.removeFromLeft(12);
    int stripH = strip.getHeight() / 6;
    for (auto &layer : layers)
        layer->setBounds(strip.removeFromTop(stripH));
    room.setBounds(b);
    instrumentPage.setBounds(350, 20, 150, 28);
    for (int i = 0; i < 2; ++i)
        if (busControls[i])
            busControls[i]->setBounds(520 + i * 220, 5, 210, 52);
    room.setVisible(!showingInstruments);
    for (int i = 0; i < 6; ++i)
    {
        if (!instrumentCards[i])
            continue;
        instrumentCards[i]->setVisible(showingInstruments);
        instrumentCards[i]->setBounds(b.getX() + (i % 2) * b.getWidth() / 2,
                                      b.getY() + (i / 2) * b.getHeight() / 3, b.getWidth() / 2 - 8,
                                      b.getHeight() / 3 - 12);
    }

    groups.setBounds(side.removeFromTop(30));
    side.removeFromTop(8);
    auto actions = side.removeFromBottom(62);
    auto row = actions.removeFromTop(28);
    sceneA.setBounds(row.removeFromLeft(130));
    sceneB.setBounds(row.removeFromLeft(130));
    live.setBounds(row);
    actions.removeFromTop(5);
    reset.setBounds(actions);
    int rows = int((details.size() + 1) / 2);
    int h = rows > 0 ? std::min(64, side.getHeight() / rows) : 64;
    for (std::size_t i = 0; i < details.size(); ++i)
        details[i]->setBounds(side.getX() + int(i % 2) * 210, side.getY() + int(i / 2) * h, 204, h);
}
void LiminalEditor::paint(juce::Graphics &g)
{
    g.fillAll(bg);
    g.setColour(ink);
    g.setFont(juce::FontOptions(26, juce::Font::bold));
    g.drawText("L I M I N A L", 22, 12, 300, 38, juce::Justification::centredLeft);
    g.setColour(dim);
    g.setFont(juce::FontOptions(12));

    int x = getWidth() - 78, y = getHeight() - 205, h = 140;
    g.setColour(panel);
    g.fillRect(x, y, 38, h);
    const float peaks[]{processor.meterL.load(), processor.meterR.load()};
    for (int c = 0; c < 2; ++c)
    {
        auto level = juce::jlimit(0.f, 1.f, (juce::Decibels::gainToDecibels(peaks[c], -60.f) + 60) / 60);
        g.setColour(peaks[c] > .9f ? juce::Colours::orange : accent);
        g.fillRect(x + 4 + c * 17, y + h - int(level * float(h - 8)) - 4, 12, int(level * float(h - 8)));
    }
}
void LiminalEditor::timerCallback()
{
    room.repaint();
    repaint(getWidth() - 90, getHeight() - 220, 80, 180);
    if (!presets.isPopupActive() && presets.getText() != processor.presetName())
        presets.setText(processor.presetName(), juce::dontSendNotification);
}
void LiminalEditor::browsePreset(bool saving)
{
    chooser = std::make_unique<juce::FileChooser>(
        saving ? "Save LIMINAL preset" : "Load LIMINAL preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.liminal");
    auto safe = juce::Component::SafePointer<LiminalEditor>(this);
    chooser->launchAsync(
        juce::FileBrowserComponent::canSelectFiles |
            (saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                    : juce::FileBrowserComponent::openMode),
        [safe, saving](const juce::FileChooser &fc)
        {
            if (!safe)
                return;
            auto f = fc.getResult();
            if (f == juce::File{})
                return;
            auto result = saving ? safe->processor.savePreset(f.withFileExtension("liminal"))
                                 : safe->processor.loadPreset(f);
            safe->status.setText(result.wasOk() ? (saving ? "Preset saved" : "Preset loaded")
                                                : result.getErrorMessage(),
                                 juce::dontSendNotification);
        });
}
juce::Rectangle<float> LiminalEditor::RoomView::roomBounds() const
{
    auto b = getLocalBounds().toFloat().reduced(40, 85);
    auto p = processor.settings();
    float aspect = std::clamp(p[roomWidth] / p[roomLength], .25f, 3.f);
    float w = std::min(b.getWidth(), b.getHeight() * aspect), h = w / aspect;
    return {b.getCentreX() - w * .5f, b.getCentreY() - h * .5f, w, h};
}
juce::Point<float> LiminalEditor::RoomView::marker(bool listener) const
{
    auto b = roomBounds();
    auto p = processor.settings();
    return {b.getX() + p[listener ? listenerX : sourceX] * b.getWidth(),
            b.getY() + p[listener ? listenerY : sourceY] * b.getHeight()};
}
void LiminalEditor::RoomView::paint(juce::Graphics &g)
{
    g.fillAll(panel);
    g.setColour(dim.withAlpha(.12f));
    for (int x = 0; x < getWidth(); x += 20)
        g.drawVerticalLine(x, 0, float(getHeight()));
    for (int y = 0; y < getHeight(); y += 20)
        g.drawHorizontalLine(y, 0, float(getWidth()));
    auto b = roomBounds();
    auto p = processor.settings();
    auto s = marker(false), l = marker(true);
    g.setColour(accent);
    g.drawRect(b, 2);
    g.setColour(dim.withAlpha(.5f));
    g.drawLine({s, l}, 1);
    for (int i = 0; i < 4; ++i)
    {
        juce::Point<float> reflection{i % 2 ? b.getRight() : b.getX(),
                                      b.getY() + b.getHeight() * (float(i) + 1) / 5};
        g.drawLine({s, reflection});
        g.drawLine({reflection, l});
    }
    g.setColour(juce::Colour(0xffd6b47d));
    g.fillEllipse(s.x - 8, s.y - 8, 16, 16);
    g.setColour(accent);
    g.fillEllipse(l.x - 8, l.y - 8, 16, 16);
    g.setColour(ink);
    g.setFont(juce::FontOptions(14));
    g.drawText("S", int(s.x) - 6, int(s.y) - 30, 20, 20, juce::Justification::centred);
    g.drawText("L", int(l.x) - 6, int(l.y) - 30, 20, 20, juce::Justification::centred);
    const char *materials[]{"CONCRETE", "TILE", "METAL", "GLASS", "CARPET", "WOOD", "MIXED"};
    g.drawText("ARCHITECTURAL FIELD / " + juce::String(materials[std::clamp(int(p[material]), 0, 6)]), 20, 15,
               getWidth() - 40, 24, juce::Justification::centredLeft);
    g.setColour(dim);
    g.drawText(juce::String(p[roomWidth], 1) + " x " + juce::String(p[roomLength], 1) + " x " +
                   juce::String(p[roomHeight], 1) + " m",
               20, getHeight() - 60, getWidth() - 40, 20, juce::Justification::centred);
    g.drawText("DRAG SOURCE / LISTENER     ·     APPROXIMATE REFLECTIONS", 10, getHeight() - 35,
               getWidth() - 20, 20, juce::Justification::centred);
}
void LiminalEditor::RoomView::mouseDown(const juce::MouseEvent &e)
{
    auto s = marker(false), l = marker(true);
    if (std::min(s.getDistanceFrom(e.position), l.getDistanceFrom(e.position)) > 25)
        return;
    dragging = true;
    draggingListener = l.getDistanceFrom(e.position) < s.getDistanceFrom(e.position);
    for (auto id : draggingListener ? std::array<Param, 2>{listenerX, listenerY}
                                    : std::array<Param, 2>{sourceX, sourceY})
        processor.state.getParameter(specs[id].id)->beginChangeGesture();
    mouseDrag(e);
}
void LiminalEditor::RoomView::mouseDrag(const juce::MouseEvent &e)
{
    if (!dragging)
        return;
    auto b = roomBounds();
    float values[]{(e.position.x - b.getX()) / b.getWidth(), (e.position.y - b.getY()) / b.getHeight()};
    int c = 0;
    for (auto id : draggingListener ? std::array<Param, 2>{listenerX, listenerY}
                                    : std::array<Param, 2>{sourceX, sourceY})
        processor.state.getParameter(specs[id].id)
            ->setValueNotifyingHost(juce::jlimit(0.f, 1.f, values[c++]));
    repaint();
}
void LiminalEditor::RoomView::mouseUp(const juce::MouseEvent &)
{
    if (!dragging)
        return;
    for (auto id : draggingListener ? std::array<Param, 2>{listenerX, listenerY}
                                    : std::array<Param, 2>{sourceX, sourceY})
        processor.state.getParameter(specs[id].id)->endChangeGesture();
    dragging = false;
}
