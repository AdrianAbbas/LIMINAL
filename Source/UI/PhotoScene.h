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
// Assets: bundled (BinaryData from assets/scenes/<name>/), or the developer's own renders in
// photographFolder()/<name>/ (same files: <name>_manifest.json, <name>_<pass>.jpg ...), which win.
#include <juce_graphics/juce_graphics.h>
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

    void request(const Look&);            // composite this look (the newest request wins)
    bool take(juce::Image& out);          // the newest finished picture, if there is a new one
    // Tests and snapshots: load and composite on the calling thread.
    juce::Image renderNow(const Look&);
    bool ensureLoaded() { return load(); }   // (blocks while decoding)
    Look defaultLook() const;

private:
    class Worker;
    bool load();
    void composite(const Look&, juce::Image& into);
    void addPass(int index, float delta);

    int placeIndex = 0;
    std::atomic<bool> ready { false }, broken { false };
    int width = 0, height = 0, cw = 0, ch = 0;
    float headroom = 1, causticRate = 5, causticDefault = 1;
    juce::Colour causticTint { 0xffcff8f0 };
    std::vector<Pass> passList;
    std::vector<std::vector<juce::uint8>> pixels;     // each pass: sRGB bytes, RGB
    std::vector<std::vector<juce::uint8>> caustics;   // each frame: luminance bytes, cw x ch
    std::vector<float> sum, levelsNow;                // the running linear sum and the levels in it
    std::vector<juce::uint8> vignette;                // per pixel, 255 = untouched
    std::vector<int> causticX, causticY;              // nearest caustic sample per column / row
    std::mutex workLock;                              // the sum and the passes (worker vs renderNow)
    std::mutex handoff;                               // the requested look and the finished image
    Look wanted;
    bool pending = false;
    juce::Image finished;
    bool fresh = false;
    std::unique_ptr<Worker> worker;
};

} // namespace liminal::ui
