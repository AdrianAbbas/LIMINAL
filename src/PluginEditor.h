#pragma once
#include "PluginProcessor.h"
class LiminalEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit LiminalEditor(LiminalProcessor &);
    ~LiminalEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    class RoomView final : public juce::Component, public juce::SettableTooltipClient
    {
      public:
        explicit RoomView(LiminalProcessor &p) : processor(p) {}
        void paint(juce::Graphics &) override;
        void mouseDown(const juce::MouseEvent &) override;
        void mouseDrag(const juce::MouseEvent &) override;
        void mouseUp(const juce::MouseEvent &) override;

      private:
        juce::Rectangle<float> roomBounds() const;
        juce::Point<float> marker(bool listener) const;
        LiminalProcessor &processor;
        bool draggingListener = false, dragging = false;
    };
    struct Control : juce::Component
    {
        Control(LiminalProcessor &, std::size_t, bool rotary = false);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct LayerStrip : juce::Component
    {
        LayerStrip(LiminalProcessor &, liminal::Param, liminal::Param, const char *, std::function<void()>);
        void resized() override;
        juce::ToggleButton enabled;
        juce::TextButton name;
        juce::Slider level;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    };
    std::array<std::unique_ptr<LayerStrip>, 6> layers, instrumentCards;
    juce::TextButton instrumentPage{"INSTRUMENTS"};
    bool showingInstruments = true;
    void selectGroup();
    void browsePreset(bool save);
    void timerCallback() override;
    LiminalProcessor &processor;
    juce::LookAndFeel_V4 look;
    RoomView room;
    juce::ComboBox presets, groups, modeBox;
    juce::TextButton prev{"<"}, next{">"}, save{"Save"}, load{"Load"}, reset{"Reset evolution"},
        sceneA{"Capture A"}, sceneB{"Capture B"}, live{"Live controls"};
    juce::ToggleButton active{"ACTIVE"}, follow{"Follow host"}, mute{"Mute"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttachment, followAttachment,
        muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::array<std::unique_ptr<Control>, 6> macros;
    std::vector<std::unique_ptr<Control>> details;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltip{this, 600};
    juce::Label status;
    int selectedPreset = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiminalEditor)
};
