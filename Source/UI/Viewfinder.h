#pragma once
// The viewfinder (2026-11-06, shared by VACANCY and VACANCY FX): the whole window is the place's
// scene - photographic (light passes composited live, UI/PhotoScene) or painted (UI/SceneArt) -
// with the VHS "found footage" look (UI/Vhs), the camcorder overlay (REC, time stamp, battery-style
// meters, frame corners), the film strip of places and the Character chips, lights that answer the
// sound, and clean place / Character changes (the new scene prepared off screen, swapped in whole in
// one frame, cross-faded from a frozen copy of the last frame).
//
// It was MainView's own (Milestone 4 .. 2026-10-31); the instrument's MainView and the effect's
// FxView now derive from it and add their controls. It reads only what both plug-ins share: the
// parameters by ID (environment, character, quality, vhs, macro_unreal), a SceneMeters block the
// audio thread fills, and the VHS warble it publishes back.
//
// Drawing stays cheap so a fanless MacBook Air never stalls: the scene is rendered once per size
// into an Image (compositing on the scene's own thread), each animation tick repaints only the
// regions that changed, and the FrameGovernor keeps frames from piling up on the message thread.
#include "SceneMeters.h"
#include "UI/FrameGovernor.h"
#include "UI/PhotoScene.h"
#include "UI/SceneArt.h"
#include "UI/Vhs.h"
#include "UI/Widgets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace liminal {

class Viewfinder : public juce::Component, private juce::Timer {
public:
    Viewfinder(juce::AudioProcessorValueTreeState&, SceneMeters&, std::atomic<float>& vhsWarbleOut);
    ~Viewfinder() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;   // the strip of places, the Character chips; the scene is drawn again for the new size

    static constexpr int defaultWidth = 960, defaultHeight = 720;   // logical size; the editor scales it
    const ui::FrameGovernor& governor() const { return frames; }
    // One animation tick now (tests, snapshots): the first picture of a photographic place or
    // Character is composited on the spot; later ones go through its thread as usual.
    void animate() { frames = {}; settle = true; timerCallback(); settle = false; }
    // One real timer tick (tests): the frame governor and the scene's own thread as in a host.
    // lastRepaint() is what that tick asked to be repainted (empty: nothing changed or skipped).
    void tick() { lastDirty.clear(); timerCallback(); }
    const juce::RectangleList<int>& lastRepaint() const { return lastDirty; }
    juce::Rectangle<int> sceneArea() const { return getLocalBounds().withBottom(stripArea().getY()); }
    // The scene picture the last paint drew (tests: valid until the next tick).
    const juce::Image& paintedPicture() const { return lastPicture; }
    int photoComposites() const { return photo != nullptr ? photo->composites() : 0; }
    double photoCompositeMs() const { return photo != nullptr ? photo->compositeMs() : 0.0; }
    bool showingPhoto() const { return photo != nullptr && photoImage.isValid() && photo->place() == placeShown; }
    // Place and Character changes (2026-10-31): the new scene is prepared off screen, then swapped
    // in whole in one frame - picture, frosted glass, caption, Character chips, light levels -
    // and cross-faded from a frozen copy of the last frame. What the last paint drew:
    struct FrameInfo {
        int fromPlace = -1, fromCharacter = -1;   // the frame fading out (-1: none)
        int place = -1, character = -1;           // the scene fading in / on screen
        float alpha = 1;                          // how far the fade is (1: only the new scene)
        int captionPlace = -1, captionCharacter = -1, chipsPlace = -1, chipsCharacter = -1;
        bool thumbnail = false;                   // a stretched thumbnail stood in for the scene
    };
    const FrameInfo& lastFrame() const { return frameInfo; }
    int shownPlace() const { return placeShown; }
    int shownCharacter() const { return characterShown; }
    bool switching() const { return pendingSwitch() || fadeStartMs >= 0; }
    static constexpr double fadeSeconds = 0.32;
    PlaceStrip& placeSelector() { return placeStrip; }
    CharacterStrip& characterSelector() { return characterStrip; }
    // The VHS look (2026-10-31): its animated half and what the composite bakes in.
    const ui::vhs::Tape& tape() const { return vhsTape; }
    ui::vhs::Tape& tapeForTests() { return vhsTape; }
    float vhsAmount() const { return vhsAmountNow; }
    ui::vhs::Static vhsStatic() const;
    // Tests and snapshots: the current look composited now, on this thread (no waiting for the
    // scene's own thread).
    void settlePicture();

protected:
    // For the plug-in's own view.
    void startViewfinder();                                  // at the end of the derived constructor
    virtual bool drawerOpen() const { return false; }        // frosted glass behind drawerArea()
    virtual void sceneCommitted() {}                         // a new place / Character is on screen
    virtual void refreshControls() {}                        // every drawn tick (the controls follow the parameters)
    bool syncPlace();
    juce::Rectangle<int> recArea() const { return { 30, 26, 70, 22 }; }
    juce::Rectangle<int> tabsArea() const { return { getWidth() - 470, 76, 452, 26 }; }
    juce::Rectangle<int> captionArea() const { return { 52, 48, 600, 20 }; }
    juce::Rectangle<int> clockArea() const { return { getWidth() - 270, 26, 240, 22 }; }
    juce::Rectangle<int> meterArea() const { return { getWidth() - 270, 50, 240, 20 }; }
    juce::Rectangle<int> stripArea() const { return getLocalBounds().removeFromBottom(156); }
    juce::Rectangle<int> drawerArea() const { return { getWidth() - 418, 110, 400, stripArea().getY() - 110 - 10 }; }
    juce::Rectangle<int> liminalityArea() const { return { 52, stripArea().getY() - 140, 116, 124 }; }
    juce::Rectangle<int> activityArea() const { return { 52, 70, 240, 14 }; }
    // The row of controls under the film strip (76 high).
    juce::Rectangle<int> controlRow() const { return stripArea().reduced(32, 0).withTrimmedTop(10 + 52 + 8).withHeight(76); }
    // The two battery meters (right to left): their names and what they show (RMS, linear).
    std::array<const char*, 2> meterNames { "AMB", "INST" };
    std::array<const std::atomic<float>*, 2> meterSources {};
    juce::AudioProcessorValueTreeState& state;
    SceneMeters& meters;
    ViewfinderLook look;
    PlaceStrip placeStrip;
    CharacterStrip characterStrip;
    double clock = 0;
    double playUntil = 3.5;   // the deck's "PLAY" shows for a few seconds (opening, a new place)
    juce::Random random { 1979 };

private:
    void timerCallback() override;
    bool pendingSwitch() const { return targetPlace >= 0 && (targetPlace != placeShown || targetCharacter != characterShown); }
    std::atomic<float>& warbleOut;
    ui::Scene scene;
    float sceneScale = 0;
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
    juce::RectangleList<int> lastDirty;
    juce::Image lastPicture;
    juce::Image photoImage, photoFrosted;
    std::vector<float> photoSwell;
    std::vector<float> photoLevelsSent;
    float photoLamp = 1, photoPhaseSent = -1;
    int photoCharacterSent = -1, photoNotesSeen = 0, photoEventsSeen = 0;
    bool photoNeedsFrost = true, settle = false;
    std::array<int, 10> photoKnown { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // has a photographic scene (cached)
    bool photoPlace(int place);
    // The transition (see FrameInfo).
    int targetPlace = -1, targetCharacter = -1;          // what the parameters ask for
    std::unique_ptr<ui::PhotoScene> incoming, spare;     // the scene being prepared; the last one left (or one hovered), kept decoded
    unsigned lookSerial = 0, targetSerial = 0;           // targetSerial: the composite that shows the target look
    int requestedPlace = -1, requestedCharacter = -1;
    std::vector<float> targetLevels;
    float targetPhase = 0;
    juce::Image fromPicture, fromFrosted;                // the frame fading out (a private copy: nothing writes to it)
    int fromPlace = -1, fromCharacter = -1;
    double fadeStartMs = -1;   // wall time (the animation clock slows when frames are skipped)
    float fadeNow = 1;          // this frame's fade, set once per tick (1: only the new scene)
    void startFade();
    int hoverPlace = -1, hoverTicks = 0;
    bool lightsStill = false;   // Eco or slow frames: the lights hold still
    FrameInfo frameInfo;
    float fadeAlpha() const;
    void advanceSwitch(juce::RectangleList<int>& dirty);
    void freezeCurrentFrame();
    void commitSwitch(int place, int character, juce::RectangleList<int>& dirty);
    void makeFrost();
    ui::PhotoScene::Look photoLook(const ui::PhotoScene&, int place, int character, bool live) const;
    static juce::Colour gradeFor(int place, int character);
    juce::Image photoPlaceholder;   // the film strip's small picture, stretched, until the scene is decoded
    int placeholderFor = -1;
    // The VHS look: the tape's animated layers, the static half sent with each look.
    ui::vhs::Tape vhsTape;
    ui::vhs::Grain vhsGrain;
    int vhsLevel = -1;
    float vhsAmountNow = 0, vhsWrongNow = 0;
    ui::vhs::Static photoVhsSent;
    float sceneVhs = -1;   // the tape amount the painted scene was baked with
    juce::String osdClock;
    struct Osd { juce::String text; float scale = 0; juce::uint32 colour = 0; juce::Image image; };
    std::array<Osd, 5> osd;
    const juce::Image& osdImage(int slot, const juce::String& text, juce::Colour colour, float scale);
    void paintOsd(juce::Graphics&, float scale);
    void renderPainted(int place, int character, float scale);
    // The picture at screen resolution (with the tape look the viewfinder repaints every tape frame).
    int pictureSerial = 0, screenSerial = -1;
    const void* screenSource = nullptr;
    juce::Image screenPicture;
    const juce::Image& screenCopy(const juce::Image& picture, float scale);
    int characterShown = -1;
    int activityLit = -1;
    int placeShown = -1;
    juce::Colour grade;   // the place's colour cast over the scene (Milestone 4 paints each place)

    ui::FrameGovernor frames;
    double paintStarted = 0;
    bool recOn = true;
    juce::String clockText;
    int instrumentSegments = 0, ambienceSegments = 0;
    float instrumentShown = 0, ambienceShown = 0;
    // One ceiling light is failing: mostly on, sometimes it stutters.
    float flickerDim = 0;
    double flickerNext = 5, flickerUntil = 0, flickerToggle = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Viewfinder)
};

} // namespace liminal
