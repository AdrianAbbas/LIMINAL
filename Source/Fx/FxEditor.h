#pragma once
// VACANCY FX's window (2026-11-06): the instrument's viewfinder (UI/Viewfinder: the place's scene,
// the VHS look, the camcorder overlay, the film strip of places, the Character chips, the clean
// place changes) with the effect's controls - Mix, Intensity, Input, Output and Bypass on the strip,
// the big LIMINALITY knob on the scene, A / B beside the page buttons - and three pages: PLACE (the
// ten rooms and their Characters, Space, Room Sound and the macros), LIMINALITY (its ten
// controls) and PRESETS (the factory presets by source, your own, INIT, UNDO / REDO, Quality).
#include "Fx/FxProcessor.h"
#include "UI/ScrollList.h"
#include "UI/Viewfinder.h"
#include "UI/Widgets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>

namespace liminal::fx {

// A page's frame (frosted, drawn by the viewfinder) with a title.
class FxPage : public juce::Component {
public:
    explicit FxPage(juce::String title) : heading(std::move(title)) {}
    void paint(juce::Graphics&) override;

protected:
    juce::String heading;
};

// PLACE: the ten rooms (click), the place's three Characters, and how the place is heard.
class FxPlaceDrawer final : public FxPage {
public:
    explicit FxPlaceDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool refresh();
    void choose(int place);
    void chooseCharacter(int character);
    std::function<void()> onChoose;
    juce::Rectangle<int> row(int place) const { return { 16, 30 + place * 19, getWidth() - 32, 18 }; }
    juce::Rectangle<int> characterBox(int character) const;

private:
    juce::AudioProcessorValueTreeState& state;
    std::array<std::unique_ptr<LevelControl>, 8> controls;
    std::array<juce::TextButton, 3> vhs;
    int selected = -1, selectedCharacter = -1, hover = -1, hoverCharacter = -1;
};

// LIMINALITY: its ten controls.
class FxLiminalityDrawer final : public FxPage {
public:
    explicit FxLiminalityDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::array<std::unique_ptr<LevelControl>, dsp::lim::count> controls;
};

// PRESETS: by source (vocals, guitar, keys, drums, mix, any) and your own; INIT, RESET, UNDO / REDO.
class FxPresetDrawer final : public FxPage {
public:
    explicit FxPresetDrawer(FxProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void showSource(int source);
    int shownSource() const { return source; }
    void load(int row);   // as a click on that row (tests)
    void loadInit();
    int rows() const;
    void refreshUndo();
    std::function<void()> onLoad;
    juce::Rectangle<int> initRowArea() const { return { 16, 58, getWidth() - 32, 22 }; }
    const ui::ScrollList& list() const { return scroller; }

private:
    juce::Rectangle<int> listArea() const { return { 16, 84, getWidth() - 32, visibleRows * 22 }; }
    juce::String rowName(int row) const;
    juce::String rowDescription(int row) const;
    FxProcessor& processor;
    ui::ScrollList scroller;
    int source = 0, hover = -1;
    bool hoverInit = false;
    juce::Array<juce::File> userFiles;
    std::array<juce::TextButton, presets::sourceCount> tabs;
    juce::TextButton reset { "RESET" }, undo { "UNDO" }, redo { "REDO" }, save { "SAVE" };
    juce::TextEditor nameField;
    std::array<juce::TextButton, 3> quality;
    static constexpr int visibleRows = 9;
};

class FxView final : public Viewfinder {
public:
    explicit FxView(FxProcessor&);
    ~FxView() override;
    void resized() override;
    void paintOverChildren(juce::Graphics&) override;
    void showPlace(bool);
    void showLiminality(bool);
    void showPresets(bool);
    FxPlaceDrawer& placeDrawer() { return places; }
    FxLiminalityDrawer& liminalityDrawer() { return liminalityPage; }
    FxPresetDrawer& presetDrawer() { return presetsPanel; }
    juce::Button& placeToggle() { return placeButton; }
    juce::Button& liminalityToggle() { return liminalityButton; }
    juce::Button& presetsToggle() { return presetsButton; }
    juce::Button& abButton(int which) { return which == 0 ? aButton : bButton; }
    juce::Button& bypassButton() { return bypass; }
    void refreshAB();

private:
    bool drawerOpen() const override;
    void sceneCommitted() override;
    void refreshControls() override;
    void closeDrawersExcept(juce::Component*);
    FxProcessor& processor;
    MacroKnob mixKnob, intensityKnob, liminalityKnob;
    LevelControl input, output, space, roomSound;
    juce::ToggleButton bypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    DrawerButton placeButton { "PLACE", DrawerButton::Icon::environment };
    DrawerButton liminalityButton { "LIMINALITY", DrawerButton::Icon::effects };
    DrawerButton presetsButton { "PRESETS", DrawerButton::Icon::presets };
    juce::TextButton aButton { "A" }, bButton { "B" }, copyButton { "A>B" };
    FxPlaceDrawer places;
    FxLiminalityDrawer liminalityPage;
    FxPresetDrawer presetsPanel;
    bool bypassShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxView)
};

class FxEditor final : public juce::AudioProcessorEditor {
public:
    explicit FxEditor(FxProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    static constexpr int defaultWidth = Viewfinder::defaultWidth, defaultHeight = Viewfinder::defaultHeight;
    FxView& view() { return main; }

private:
    FxProcessor& processor;
    FxView main;
    bool ready = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxEditor)
};

} // namespace liminal::fx
