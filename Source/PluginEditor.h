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
#include "Export.h"
#include "PluginProcessor.h"
#include "UI/FrameGovernor.h"
#include "UI/PhotoScene.h"
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
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
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

// Milestone 4 main view parts ===============================================================

// The ten places as a strip of film frames (their own artwork in miniature): click to go.
class PlaceStrip final : public juce::Component {
public:
    explicit PlaceStrip(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool refresh();
    juce::Rectangle<int> frame(int place) const;
    void choose(int place);
    std::function<void()> onChoose;

private:
    juce::AudioProcessorValueTreeState& state;
    std::array<juce::Image, 10> thumbs;
    float thumbScale = 0;
    int selected = -1, hover = -1;
};

// The place's three Characters as words under the caption: click to choose.
class CharacterStrip final : public juce::Component {
public:
    explicit CharacterStrip(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool refresh();
    void choose(int character);
    std::function<void()> onChoose;

private:
    juce::Rectangle<float> chip(int character) const;
    juce::AudioProcessorValueTreeState& state;
    int place = -1, selected = -1, hover = -1;
};

// The six instruments as objects (a piano, a cassette, a glass, voices, a music box, a
// waveform): lit when on, click to switch.
class InstrumentShelf final : public juce::Component {
public:
    explicit InstrumentShelf(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool refresh();
    void toggle(int instrument);

private:
    juce::Rectangle<float> slot(int instrument) const;
    juce::AudioProcessorValueTreeState& state;
    std::array<bool, 6> on {};
    int hover = -1;
};

// A large ring knob with its name and value under it (the four macros).
class MacroKnob final : public juce::Component {
public:
    MacroKnob(juce::AudioProcessorValueTreeState&, const char* id, juce::String label);
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
    enum class Icon { instruments, environment, effects, perform, presets };
    DrawerButton(const juce::String& text, Icon);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    bool compact = true;   // a page tab: the word alone (no icon, no chevron)

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

// The environment drawer: the places as a list (click to go there), what each one does,
// and Environment Amount (how strongly the place processes the instruments).
class EnvironmentDrawer final : public juce::Component {
public:
    explicit EnvironmentDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void choose(int place);
    bool refresh();   // true if the selected place changed
    std::function<void()> onChoose;

    void chooseCharacter(int character);

private:
    juce::Rectangle<int> row(int place) const;
    juce::Rectangle<int> characterBox(int character) const;
    juce::AudioProcessorValueTreeState& state;
    LevelControl amount;
    int selected = 0, hover = -1, selectedCharacter = 0, hoverCharacter = -1;
};

// The performance drawer (Milestone 5): the four macros, evolution (rate, freeze, seed,
// reset) and scenes A/B (store, morph).
class PerformDrawer final : public juce::Component {
public:
    PerformDrawer(LiminalProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshScenes();

private:
    LiminalProcessor& processor;
    LevelControl presence, scale, activity, unreal, rate, morph;
    juce::ToggleButton freeze { "FREEZE" }, morphOn { "MORPH A - B" };
    juce::TextButton reset { "RESET" }, storeA { "STORE A" }, storeB { "STORE B" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment, morphOnAttachment;
    LevelControl seed;
};

// The effects drawer (Milestone 3): one instrument at a time (tabs). Each module is a row:
// its switch, its amount and its details. Under them: Effects Mix, Environment Influence,
// Effect Order, and what the current place adds to this instrument.
class EffectsDrawer final : public juce::Component {
public:
    explicit EffectsDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void focus(int instrument);
    int focused() const { return current; }

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    struct Row {
        std::unique_ptr<juce::ToggleButton> on;
        std::unique_ptr<ButtonAttachment> onAttachment;
        std::vector<std::unique_ptr<LevelControl>> controls;
        std::unique_ptr<juce::ComboBox> choice;          // echo sync
        std::unique_ptr<ComboAttachment> choiceAttachment;
        std::unique_ptr<juce::ToggleButton> extra;       // granular freeze
        std::unique_ptr<ButtonAttachment> extraAttachment;
    };
    juce::AudioProcessorValueTreeState& state;
    std::array<juce::TextButton, params::instrumentCount> tabs;
    std::vector<Row> rows;
    std::unique_ptr<LevelControl> mix, influence;
    std::vector<std::unique_ptr<LevelControl>> midi;   // key range, velocity to volume and filter
    std::unique_ptr<juce::ComboBox> order;
    std::unique_ptr<ComboAttachment> orderAttachment;
    int current = 0;
};

// The presets drawer (Milestone 6): the five groups as tabs, a scrolling list (click to
// load), saving your own; quality (Eco / Standard / High); exporting the place's ambience.
class PresetDrawer final : public juce::Component {
public:
    explicit PresetDrawer(LiminalProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void showCategory(int category);
    void load(int row);              // as a click on that row (tests)
    int rows() const;
    bool poll();                     // export progress; true if something changed
    std::function<void()> onLoad;

private:
    juce::Rectangle<int> rowArea(int visibleRow) const;
    juce::String rowName(int row) const;
    juce::String rowDescription(int row) const;
    void startExport();
    LiminalProcessor& processor;
    int category = 3, scroll = 0, hover = -1;
    juce::Array<juce::File> userFiles;
    std::array<juce::TextButton, 5> tabs;
    juce::TextEditor nameField;
    juce::TextButton save { "SAVE" };
    std::array<juce::TextButton, 3> quality;
    juce::ComboBox duration, rate;
    juce::ToggleButton room { "WITH ROOM" };
    juce::TextButton exportButton { "EXPORT AMBIENCE" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<exporter::Job> job;
    juce::String status;
    float shownProgress = -1;
    static constexpr int visibleRows = 10;
};

class MainView final : public juce::Component, private juce::Timer {
public:
    explicit MainView(LiminalProcessor&);
    ~MainView() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 960, defaultHeight = 720;   // logical size; the editor scales it
    void showInstruments(bool show);
    InstrumentDrawer& instrumentDrawer() { return drawer; }
    ActiveInstruments& activeInstruments() { return active; }
    juce::Button& instrumentsToggle() { return instrumentsButton; }
    void showEnvironment(bool show);
    EnvironmentDrawer& environmentDrawer() { return places; }
    juce::Button& environmentToggle() { return environmentButton; }
    void showEffects(bool show);
    void showPerform(bool show);
    void showPresets(bool show);
    PresetDrawer& presetDrawer() { return presetsPanel; }
    juce::Button& presetsToggle() { return presetsButton; }
    PerformDrawer& performDrawer() { return perform; }
    juce::Button& performToggle() { return performButton; }
    EffectsDrawer& effectsDrawer() { return effects; }
    juce::Button& effectsToggle() { return effectsButton; }
    const ui::FrameGovernor& governor() const { return frames; }
    // One animation tick now (tests, snapshots): the first picture of a photographic place or
    // Character is composited on the spot; later ones go through its thread as usual.
    void animate() { frames = {}; settle = true; timerCallback(); settle = false; }
    bool showingPhoto() const { return photo != nullptr && photoImage.isValid() && photo->place() == placeShown; }
    PlaceStrip& placeSelector() { return placeStrip; }
    CharacterStrip& characterSelector() { return characterStrip; }
    InstrumentShelf& instrumentShelf() { return shelf; }

private:
    void timerCallback() override;
    juce::Rectangle<int> recArea() const { return { 30, 26, 70, 22 }; }
    juce::Rectangle<int> tabsArea() const { return { getWidth() - 470, 76, 452, 26 }; }
    bool syncPlace();
    juce::Rectangle<int> captionArea() const { return { 52, 48, 600, 20 }; }
    juce::Rectangle<int> clockArea() const { return { getWidth() - 270, 26, 240, 22 }; }
    juce::Rectangle<int> meterArea() const { return { getWidth() - 270, 50, 240, 20 }; }
    juce::Rectangle<int> stripArea() const { return getLocalBounds().removeFromBottom(156); }
    juce::Rectangle<int> drawerArea() const { return { getWidth() - 418, 110, 400, stripArea().getY() - 110 - 10 }; }

    LiminalProcessor& processor;
    ViewfinderLook look;
    ui::Scene scene;
    float sceneScale = 0;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    LevelControl master, instrument, ambience, space;
    PlaceStrip placeStrip;
    CharacterStrip characterStrip;
    InstrumentShelf shelf;
    MacroKnob presenceKnob, scaleKnob, activityKnob, unrealKnob;
    // Lights that answer the music and the place (Milestone 4).
    std::array<float, 12> pulse {}, shownPulse {};
    int notesSeen = 0, eventsSeen = 0;
    double nextFlicker = 3, shimmerUntil = 0, nextShimmer = 6;
    float shimmerY = 0, shimmerH = 0, shimmerShift = 0;
    juce::Point<float> mouse { -1000, -1000 };
    void mouseMove(const juce::MouseEvent&) override;
    void paintLights(juce::Graphics&);
    // The photographic scene (Direction Update 2): light passes composited live (UI/PhotoScene).
    void animatePhoto(bool still, juce::RectangleList<int>& dirty);
    std::unique_ptr<ui::PhotoScene> photo;
    juce::Image photoImage, photoFrosted;
    std::vector<float> photoSwell;
    std::vector<float> photoLevelsSent;
    float photoLamp = 1, photoPhaseSent = -1;
    int photoCharacterSent = -1, photoNotesSeen = 0, photoEventsSeen = 0;
    bool photoNeedsFrost = true, settle = false;
    std::array<int, 10> photoKnown { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // has a photographic scene (cached)
    bool photoPlace(int place);
    juce::Image photoPlaceholder;   // the film strip's small picture, stretched, until the scene is decoded
    int placeholderFor = -1;
    DrawerButton instrumentsButton { "INSTRUMENTS", DrawerButton::Icon::instruments };
    DrawerButton environmentButton { "ENVIRONMENT", DrawerButton::Icon::environment };
    DrawerButton effectsButton { "EFFECTS", DrawerButton::Icon::effects };
    DrawerButton performButton { "PERFORM", DrawerButton::Icon::perform };
    DrawerButton presetsButton { "PRESETS", DrawerButton::Icon::presets };
    ActiveInstruments active;
    InstrumentDrawer drawer;
    EnvironmentDrawer places;
    EffectsDrawer effects;
    PerformDrawer perform;
    PresetDrawer presetsPanel;
    void closeDrawersExcept(juce::Component* keep);
    int characterShown = -1;
    int activityLit = -1;
    juce::Rectangle<int> activityArea() const { return { 52, 70, 240, 14 }; }
    int placeShown = -1;
    juce::Colour grade;   // the place's colour cast over the scene (Milestone 4 paints each place)

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainView)
};

// The plug-in window: the main view, scaled to the window (resizable, fixed aspect ratio,
// 85-150 %; the size is remembered in the song).
class LiminalEditor final : public juce::AudioProcessorEditor {
public:
    explicit LiminalEditor(LiminalProcessor&);
    void resized() override;
    static constexpr int defaultWidth = MainView::defaultWidth, defaultHeight = MainView::defaultHeight;
    MainView& view() { return main; }
    // For tests and callers: the view's parts.
    void showInstruments(bool s) { main.showInstruments(s); }
    void showEnvironment(bool s) { main.showEnvironment(s); }
    void showEffects(bool s) { main.showEffects(s); }
    void showPerform(bool s) { main.showPerform(s); }
    void showPresets(bool s) { main.showPresets(s); }
    InstrumentDrawer& instrumentDrawer() { return main.instrumentDrawer(); }
    EnvironmentDrawer& environmentDrawer() { return main.environmentDrawer(); }
    EffectsDrawer& effectsDrawer() { return main.effectsDrawer(); }
    PerformDrawer& performDrawer() { return main.performDrawer(); }
    PresetDrawer& presetDrawer() { return main.presetDrawer(); }
    ActiveInstruments& activeInstruments() { return main.activeInstruments(); }
    juce::Button& instrumentsToggle() { return main.instrumentsToggle(); }
    juce::Button& environmentToggle() { return main.environmentToggle(); }
    juce::Button& effectsToggle() { return main.effectsToggle(); }
    juce::Button& performToggle() { return main.performToggle(); }
    juce::Button& presetsToggle() { return main.presetsToggle(); }
    const ui::FrameGovernor& governor() const { return main.governor(); }

private:
    LiminalProcessor& processor;
    MainView main;
    bool ready = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalEditor)
};

} // namespace liminal
