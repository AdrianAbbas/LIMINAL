#pragma once
// Milestone 0 editor: the start of the "viewfinder" direction (docs/DEVLOG.md). The whole
// window is the environment scene; a few controls float on a frosted strip, with a
// camcorder-style overlay (REC, time stamp, battery-style meters, frame corners).
// Milestone 4 replaces this with the full design.
//
// Drawing stays cheap so a fanless MacBook Air never stalls: the scene is rendered once
// per size into an Image, each animation tick repaints only the small regions that
// changed (REC dot, clock, meters, one flickering light), and the FrameGovernor (from
// WORLDBUILDER) keeps frames from piling up on the message thread.
#include "PluginProcessor.h"
#include "UI/FrameGovernor.h"
#include "UI/SceneArt.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>

namespace liminal {

class ViewfinderLook final : public juce::LookAndFeel_V4 {
public:
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
};

// A thin slider with a small institutional label and its value.
class LevelControl final : public juce::Component {
public:
    LevelControl(juce::AudioProcessorValueTreeState&, const char* id, juce::String label);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;

private:
    juce::String label;
    juce::RangedAudioParameter* parameter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A strip button that opens a drawer: an icon and a word in a frosted pill, brighter on
// hover, filled while its drawer is open, with a pointing-hand cursor.
class DrawerButton final : public juce::Button {
public:
    enum class Icon { instruments, environment };
    DrawerButton(const juce::String& text, Icon);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

// The instruments that are on, as clickable names ("PIANO + GLASS"): clicking a name
// solos it, clicking it again (or another) changes or clears the solo.
class ActiveInstruments final : public juce::Component {
public:
    explicit ActiveInstruments(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool refresh();   // true if what it shows changed
    juce::String text() const;
    void clickName(int instrument);   // as a click on that name (tests)

private:
    struct Name { int index; juce::Rectangle<float> area; };
    void layout();
    int nameAt(juce::Point<float>) const;
    juce::AudioProcessorValueTreeState& state;
    std::array<bool, params::instrumentCount> on {};
    int solo = 0, hover = -1;
    std::vector<Name> names;
};

// The instrument drawer: slides over the right of the scene. Six rows (on/off and level
// per instrument), tabs to pick which one is edited, and its controls. The full
// INSTRUMENTS page with artwork comes with Milestone 4.
class InstrumentDrawer final : public juce::Component {
public:
    explicit InstrumentDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void focus(int index);
    int focused() const { return current; }
    static constexpr int count = params::instrumentCount;

private:
    struct Page {
        juce::ToggleButton on;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        std::unique_ptr<LevelControl> level;
        juce::TextButton tab;
        std::vector<std::unique_ptr<LevelControl>> controls;
        std::unique_ptr<juce::ToggleButton> glide;   // the synth only
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> glideAttachment;
    };
    std::array<Page, count> pages;
    int current = 0;
};

class LiminalEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LiminalEditor(LiminalProcessor&);
    ~LiminalEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 920, defaultHeight = 620;
    void showInstruments(bool show);
    InstrumentDrawer& instrumentDrawer() { return drawer; }
    ActiveInstruments& activeInstruments() { return active; }
    juce::Button& instrumentsToggle() { return instrumentsButton; }
    const ui::FrameGovernor& governor() const { return frames; }

private:
    void timerCallback() override;
    juce::Rectangle<int> recArea() const { return { 30, 26, 70, 22 }; }
    juce::Rectangle<int> clockArea() const { return { getWidth() - 270, 26, 240, 22 }; }
    juce::Rectangle<int> meterArea() const { return { getWidth() - 270, 50, 240, 20 }; }
    juce::Rectangle<int> stripArea() const { return getLocalBounds().removeFromBottom(108); }
    juce::Rectangle<int> drawerArea() const { return { getWidth() - 418, 78, 400, stripArea().getY() - 78 - 12 }; }

    LiminalProcessor& processor;
    ViewfinderLook look;
    ui::Scene scene;
    float sceneScale = 0;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    LevelControl master, instrument, ambience, space;
    DrawerButton instrumentsButton { "INSTRUMENTS", DrawerButton::Icon::instruments };
    ActiveInstruments active;
    InstrumentDrawer drawer;

    ui::FrameGovernor frames;
    double paintStarted = 0, clock = 0;
    bool recOn = true;
    juce::String clockText;
    int instrumentSegments = 0, ambienceSegments = 0;
    float instrumentShown = 0, ambienceShown = 0;
    // One ceiling light is failing: mostly on, sometimes it stutters.
    float flickerDim = 0;
    double flickerNext = 5, flickerUntil = 0, flickerToggle = 0;
    juce::Random random { 1979 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalEditor)
};

} // namespace liminal
