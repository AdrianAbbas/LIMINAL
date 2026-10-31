#pragma once
// Photographic scenes (Direction Update 2, Section 3): each place rendered offline with Cycles
// (tools/scenes) as separate *light passes* in linear light - a base pass with every lamp off and
// one pass per light group - plus, for water, a looping caustic animation. The plug-in adds them
// up live, base + sum(level_i x pass_i), so every light group can flicker, dim and swell on its own
// (the failing tube with the hum's stutter, the lamps with the notes), then grades the result like
// a photograph (the Character's grade and light, fade, vignette, grain).
//
// Work happens on a background thread: decoding the passes when the scene opens, then each
// composite (a running linear sum, so a changed light costs one pass, then one tone-mapping
// pass over the picture). The editor asks for a look and takes the newest picture when it is
// ready; it never waits.
//
// Pictures are triple-buffered: the worker composites into a back image the editor never sees,
// then swaps it with the "ready" slot under a short lock; take() swaps "ready" with the image
// the editor shows. So the editor only ever draws a complete frame, nothing is reallocated per
// frame, and the worker never writes into a picture that is on screen. The images are ARGB with
// alpha 255 everywhere (macOS stores even Image::RGB as ARGB; an unwritten alpha byte made every
// other frame transparent - the GarageBand flicker of 2026-10-31). At most 30 composites a second.
//
// Assets: bundled (BinaryData from assets/scenes/<name>/), or the developer's own renders in
// photographFolder()/<name>/ (same files: <name>_manifest.json, <name>_<pass>.jpg ...), which win.
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace liminal::ui {

class PhotoScene {
public:
    struct Pass {
        juce::String name, kind;            // kind: base, tube, failing, sign, lamp, sun, sky
        juce::Point<float> anchor { -1, -1 };   // where the light group sits (0..1 of the picture)
        float defaultLevel = 1;
        std::array<float, 3> characters { 1, 1, 1 };   // its level under each of the place's Characters
        std::array<bool, 3> flicker {};                // flickers with the hum's stutter (kind "failing" always)
        bool flickers(int character) const { return kind == "failing" || flicker[std::size_t(std::clamp(character, 0, 2))]; }
        float level(int character) const { return characters[std::size_t(std::clamp(character, 0, 2))]; }
    };
    // What to show. Levels are per pass (base first), 1 = as rendered.
    struct Look {
        std::vector<float> levels;
        float causticPhase = 0, causticLevel = 1;   // phase 0..1 around the loop
        juce::uint32 grade = 0;   // the Character's grade (ARGB; alpha = strength)
        float light = 1;          // the Character's light level
        int character = 0;        // 1: a faded memory; 2: darker, hazier edges
        unsigned grainSeed = 1;
    };

    // Which places have a photographic scene (bundled or dropped in), and its file name.
    static const char* sceneName(int place);   // nullptr if none
    static bool available(int place);
    static juce::Image thumbnail(int place);   // a small picture for the film strip (or invalid)

    explicit PhotoScene(int place);   // starts loading in the background
    ~PhotoScene();

    int place() const { return placeIndex; }
    bool loaded() const { return ready.load(); }
    bool failed() const { return broken.load(); }
    const std::vector<Pass>& passes() const { return passList; }   // valid once loaded()
    bool hasCaustics() const { return !caustics.empty(); }
    int causticFrames() const { return int(caustics.size()); }
    float causticFps() const { return causticRate; }
    float causticCharacter(int character) const { return causticCharacters[std::size_t(std::clamp(character, 0, 2))]; }

    void request(const Look&);            // composite this look (the newest request wins)
    // The newest finished picture, if there is a new one. It stays complete and unchanged until
    // the next take() (message thread only).
    bool take(juce::Image& out);
    int composites() const { return compositeCount.load(); }   // how many the worker has made
    double compositeMs() const { return compositeTime.load(); }   // and the time they took
    static constexpr double minFrameMs = 1000.0 / 30.0;       // the worker's frame cap
    // Tests and snapshots: load and composite on the calling thread.
    juce::Image renderNow(const Look&);
    bool ensureLoaded() { return load(); }   // (blocks while decoding)
    Look defaultLook() const;
    // The default look under one of the place's Characters: its light per group (the manifest's
    // per-Character levels; variant passes are 0 except for their Character). Grade and light are
    // the caller's (dsp::character).
    Look characterLook(int character) const;

private:
    class Worker;
    bool load();
    void composite(const Look&, juce::Image& into);
    juce::Image makeImage() const;   // opaque ARGB, the scene's size
    void addPasses(const std::vector<std::pair<int, float>>& moved);   // (pass, level change)

    int placeIndex = 0;
    std::atomic<bool> ready { false }, broken { false };
    int width = 0, height = 0, cw = 0, ch = 0;
    float headroom = 1, causticRate = 5, causticDefault = 1;
    juce::Colour causticTint { 0xffcff8f0 };
    std::array<float, 3> causticCharacters { 1, 1, 1 };
    std::vector<Pass> passList;
    std::vector<std::vector<juce::uint8>> pixels;     // each pass: sRGB bytes, RGB
    std::vector<std::vector<juce::uint8>> caustics;   // each frame: luminance bytes, cw x ch
    std::vector<float> sum, levelsNow;                // the running linear sum and the levels in it
    std::vector<juce::uint8> vignette;                // per pixel, 255 = untouched
    std::vector<juce::int8> grain;                    // per pixel, -3..+3 (made on the first composite)
    unsigned grainFor = 0;
    std::vector<int> causticX, causticY;              // nearest caustic sample per column / row
    std::mutex workLock;                              // the sum and the passes (worker vs renderNow)
    std::mutex handoff;                               // the requested look and the finished image
    Look wanted;
    bool pending = false;
    std::array<juce::Image, 3> buffers;   // [front]: the editor's; [ready]: newest complete; [back]: the worker's
    int front = 0, readyIndex = 1, back = 2;   // swapped under `handoff` (back is only read by the worker)
    bool fresh = false;
    std::atomic<int> compositeCount { 0 };
    std::atomic<double> compositeTime { 0 };
    std::unique_ptr<Worker> worker;
};

} // namespace liminal::ui
