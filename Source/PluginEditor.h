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

// Milestone 1a's instrument drawer: slides over the right of the scene. Three rows (on/off
// and level per instrument), tabs to pick which one is edited, and its six controls (plus
// Glide for the synth). The full INSTRUMENTS page with artwork comes with 1b / Milestone 4.
class InstrumentDrawer final : public juce::Component {
public:
    explicit InstrumentDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void focus(int index);
    int focused() const { return current; }
    static constexpr int count = 3;

private:
    struct Page {
        juce::ToggleButton on;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        std::unique_ptr<LevelControl> level;
        juce::TextButton tab;
        std::vector<std::unique_ptr<LevelControl>> controls;
        std::unique_ptr<juce::ToggleButton> glide;
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

    static constexpr int defaultWidth = 920, defaultHeight = 580;
    void showInstruments(bool show);
    InstrumentDrawer& instrumentDrawer() { return drawer; }
    const ui::FrameGovernor& governor() const { return frames; }

private:
    void timerCallback() override;
    juce::Rectangle<int> recArea() const { return { 30, 26, 70, 22 }; }
    juce::Rectangle<int> clockArea() const { return { getWidth() - 270, 26, 240, 22 }; }
    juce::Rectangle<int> meterArea() const { return { getWidth() - 270, 50, 240, 20 }; }
    juce::Rectangle<int> stripArea() const { return getLocalBounds().removeFromBottom(92); }
    juce::Rectangle<int> drawerArea() const { return { getWidth() - 376, 78, 358, stripArea().getY() - 78 - 12 }; }
    juce::String instrumentCaption() const;

    LiminalProcessor& processor;
    ViewfinderLook look;
    ui::Scene scene;
    float sceneScale = 0;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    LevelControl master, instrument, ambience, space;
    juce::TextButton instrumentsButton { "INSTRUMENTS" };
    InstrumentDrawer drawer;
    juce::String caption;

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
