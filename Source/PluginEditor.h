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
#include "UI/Vhs.h"
#include "UI/PhotoScene.h"
#include "UI/SceneArt.h"
#include "UI/ScrollList.h"
#include "UI/InstrumentIcons.h"
#include "UI/Viewfinder.h"
#include "UI/Widgets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>

namespace liminal {

// The six slots as objects: each the icon of the instrument it holds (2026-11-02: any of twenty),
// lit when on, click to switch.
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
    std::array<int, 6> types { -1, -1, -1, -1, -1, -1 };
    int hover = -1;
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
    std::array<int, params::instrumentCount> types {};
    int solo = 0, hover = -1;
    std::vector<Name> names;
};

// The instrument drawer: slides over the right of the scene. Six rows (on/off and level
// per instrument), tabs to pick which one is edited, and its controls. The full
// INSTRUMENTS page with artwork comes with Milestone 4.
// The instrument picker (2026-11-02): the twenty instruments as a scrolling list (icon, name, what it
// is), over the page of the slot being edited; a click loads one into the slot.
class InstrumentPicker final : public juce::Component {
public:
    explicit InstrumentPicker(juce::AudioProcessorValueTreeState&);
    void open(int slot);   // shows it for that slot, scrolled to its instrument
    int slotShown() const { return slot; }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void choose(int instrument);   // as a click on that row
    std::function<void()> onChoose;
    const ui::ScrollList& list() const { return scroller; }
    static constexpr int rowHeight = 28;

private:
    juce::AudioProcessorValueTreeState& state;
    ui::ScrollList scroller;
    int slot = 0, hover = -1;
};

class InstrumentDrawer final : public juce::Component {
public:
    explicit InstrumentDrawer(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void focus(int index);
    int focused() const { return current; }
    // The instrument library (2026-11-02): each slot's page follows the instrument it holds (names,
    // icon, its controls); CHOOSE opens the picker.
    bool refresh();   // true if a slot's instrument changed (its page was rebuilt)
    void showPicker(bool show);
    bool pickerShown() const { return picker.isVisible(); }
    InstrumentPicker& instrumentPicker() { return picker; }
    juce::StringArray controlLabels() const;   // the shown page's SOUND controls (tests)
    // Each instrument's page has two halves: its SOUND (its own controls) and its LIMINALITY
    // (the ten Liminality controls, 0 = clean, scaled by the LIMINALITY knob on the main screen).
    void showLiminality(bool show);
    bool showingLiminality() const { return liminalPage; }
    static constexpr int count = params::instrumentCount;

private:
    struct Page {
        int type = -1;   // the instrument the controls below were built for
        juce::ToggleButton on;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        std::unique_ptr<LevelControl> level;
        juce::TextButton tab;
        std::vector<std::unique_ptr<LevelControl>> controls;
        std::unique_ptr<juce::ToggleButton> glide;   // the synth only
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> glideAttachment;
        std::vector<std::unique_ptr<LevelControl>> liminal;   // Liminality (2026-11-01)
    };
    void build(int slot);   // the page's SOUND controls for the instrument the slot holds
    juce::Rectangle<int> chooserArea() const;
    juce::Rectangle<int> controlsArea() const;
    juce::AudioProcessorValueTreeState& state;
    std::array<Page, count> pages;
    int current = 0;
    bool liminalPage = false;
    juce::TextButton soundTab, liminalTab, choose { "CHOOSE" };
    InstrumentPicker picker;
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
    // The picture (2026-10-31): the VHS look's amount (Off / Light / Heavy) and the optional
    // tape warble on the audio.
    std::array<juce::TextButton, 3> vhsButtons;
    juce::ToggleButton warble { "TAPE WARBLE (AUDIO)" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> warbleAttachment;

public:
    juce::Button& vhsButton(int level) { return vhsButtons[std::size_t(std::clamp(level, 0, 2))]; }   // (tests)
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
    bool refreshNames();   // the tabs name what each slot holds (2026-11-02)

private:
    std::array<int, params::instrumentCount> tabTypes { -1, -1, -1, -1, -1, -1 };
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
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void showCategory(int category);
    int shownCategory() const { return category; }
    void load(int row);              // as a click on that row (tests)
    int rows() const;
    // INIT (2026-11-06): "no preset", pinned above every group's list (it never scrolls away),
    // and RESET / UNDO / REDO beside the preset's name.
    void loadInit();                 // as a click on the INIT row (or RESET)
    juce::Rectangle<int> initRowArea() const { return { 16, 58, getWidth() - 32, 22 }; }
    juce::Button& resetButton() { return reset; }
    juce::Button& undoButton() { return undo; }
    juce::Button& redoButton() { return redo; }
    void refreshUndo();              // the buttons follow what can be undone / redone
    bool poll();                     // export progress; true if something changed
    std::function<void()> onLoad;
    // The list's scrolling (2026-11-02: wheel / trackpad, the bar, keys; see UI/ScrollList.h).
    const ui::ScrollList& list() const { return scroller; }
    int rowAt(juce::Point<int> p) const { return scroller.rowAt(p); }   // (tests: what a click there loads)

private:
    juce::Rectangle<int> listArea() const { return { 16, 84, getWidth() - 32, visibleRows * 22 }; }
    bool hoverInit = false;
    juce::String headerText() const;
    juce::String shownName;   // the name the header was laid out for (RESET sits right after it)
    juce::TextButton reset { "RESET" }, undo { "UNDO" }, redo { "REDO" };
    ui::ScrollList scroller;
    juce::String rowName(int row) const;
    juce::String rowDescription(int row) const;
    void startExport();
    LiminalProcessor& processor;
    int category = 3, hover = -1;   // hover: the row under the mouse (-1: none)
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
    static constexpr int visibleRows = 9;
};

// The instrument's window (Milestone 4 .. 2026-11-06): the shared viewfinder (UI/Viewfinder) with
// the instrument's controls on its strip and its pages (instruments, place, effects, perform,
// presets).
class MainView final : public Viewfinder {
public:
    explicit MainView(LiminalProcessor&);
    ~MainView() override;
    void resized() override;

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
    InstrumentShelf& instrumentShelf() { return shelf; }

private:
    bool drawerOpen() const override;
    void sceneCommitted() override;
    void refreshControls() override;
    LiminalProcessor& processor;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    LevelControl master, instrument, ambience, space;
    InstrumentShelf shelf;
    MacroKnob presenceKnob, scaleKnob, activityKnob, unrealKnob;
    MacroKnob liminalityKnob;   // Liminality (2026-11-01): big, on the scene's lower left
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainView)
};

// The plug-in window: the main view, scaled to the window (resizable, fixed aspect ratio,
// 85-150 %; the size is remembered in the song).
class LiminalEditor final : public juce::AudioProcessorEditor {
public:
    explicit LiminalEditor(LiminalProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    static float fitScale(int width, int height);   // the view's scale in a window of that size (whole, centred)
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
