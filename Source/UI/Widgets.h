#pragma once
// The editor's shared parts (2026-11-06, VACANCY FX): the look, the level slider, the film strip
// of places, the Character chips, the ring knobs and the page buttons. Used by VACANCY (the
// instrument) and VACANCY FX (the effect) alike; they bind to the parameters both share by ID
// (environment, character, the macros), so each plug-in's own APVTS works with them.
#include "DSP/Characters.h"
#include "DSP/Environment.h"
#include "Parameters.h"
#include "UI/PhotoScene.h"
#include "UI/SceneArt.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

namespace liminal {

namespace ui::style {
inline const juce::Colour ink(0xffe6efe9);   // overlay text: a pale, slightly green white
inline const juce::Colour rec(0xffe0453a);
inline juce::Font font(float height, bool bold = false) {
    return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor(0.12f);
}
inline double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }
// A level (RMS, linear) as the battery meters' segments.
inline int segmentsFor(float rms) {
    const float db = juce::Decibels::gainToDecibels(rms, -100.0f);
    return std::clamp(int(std::floor((db + 66.0f) / 9.0f)), 0, 6);   // -57 dB .. -12 dB
}
} // namespace ui::style

// Each place's short name, summary and grade (the PLACE pages of both plug-ins).
struct PlaceInfo { const char* shortName; const char* summary; const char* detail; juce::uint32 grade; };
inline const PlaceInfo placeInfo[dsp::placeCount] {
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
    std::function<void(int)> onHover;   // the place under the mouse (-1: none), so it can be decoded ahead

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
    // The chips name the Characters of the place on screen and light the one on screen: they
    // change in the same frame as the picture (MainView's transition), not when the parameter does.
    void show(int place, int character);
    int shownPlace() const { return place; }
    int shownCharacter() const { return selected; }

private:
    juce::Rectangle<float> chip(int character) const;
    juce::AudioProcessorValueTreeState& state;
    int place = -1, selected = -1, hover = -1;
};

// A large ring knob with its name and value under it (the four macros).
class MacroKnob final : public juce::Component {
public:
    MacroKnob(juce::AudioProcessorValueTreeState&, const char* id, juce::String label);
    bool big = false;   // the LIMINALITY knob: larger lettering
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

} // namespace liminal
