#pragma once
// The VHS "found footage" look (2026-10-31, evening): every scene as if filmed on an old
// camcorder and played back from a worn tape.
//
// Two halves, so the editor stays cheap:
//
// 1. Baker - the STATIC part, baked into the scene's cached composite (on the compositor's own
//    thread, only when the scene, a light or the slow colour drift changes): faint barrel
//    curvature, SD-tape softness (luma low-passed to ~240 TV lines, a trailing smear and the
//    player's edge-enhancement halo), chroma bleed (colour at a quarter of the luma's resolution,
//    smeared to the right and offset from the picture), red/cyan misregistration, the tape grade
//    (lifted milky blacks, blooming and slightly clipped highlights, oversaturated reds, a faint
//    green/magenta cast), scanlines and edge vignetting. It runs AFTER the place's and Character's
//    own grade, so each keeps its look underneath.
//
// 2. Tape + paint() - the ANIMATED part, drawn over the cached picture at <= 24 frames a second:
//    pre-generated tiling grain textures, coloured speckle and dropouts, the head-switching strip
//    at the bottom of the frame, line wobble, and the sparse glitches (a tracking band rolling
//    through; with Unreal or a "wrong" Character, more rolls and brief frame tears). Glitches are
//    scheduled at irregular, sparse moments with minimum gaps: never a constant strobe.
//
// Plus the camcorder OSD (a blocky 5 x 7 pixel font drawn here, slightly glowing and misaligned).
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace liminal::ui::vhs {

// The parameter's three settings and how much tape wear each means (Light sits between "light"
// and "medium", as asked).
inline float amountOf(int level) { return level <= 0 ? 0.0f : level == 1 ? 0.55f : 1.0f; }
// How "wrong" a place's Character is (0..1): more tracking rolls, colour drift and tears.
float wrongness(int place, int character);
// Unreal and the Character together (either can make it wrong).
inline float wrongOf(float unreal, float characterWrongness) { return 1.0f - (1.0f - std::clamp(unreal, 0.0f, 1.0f)) * (1.0f - characterWrongness); }

struct Static {
    float amount = 0;   // 0 = off (the picture untouched), 0.55 Light, 1 Heavy
    float drift = 0;    // the slow colour cast: -1 green .. +1 magenta (quantised by the caller)
    float wrong = 0;    // a "wrong" Character / Unreal: a little more misregistration and smear
    bool operator==(const Static& o) const { return amount == o.amount && drift == o.drift && wrong == o.wrong; }
};

// Bakes the static tape look. `src` is the finished picture as RGB bytes (R, G, B; rows of
// width * 3); the result goes into `out` (ARGB, written with alpha 255, or RGB) of the same size.
// Keeps its row buffers between calls (no allocation once warmed up for a size).
class Baker {
public:
    void bake(const juce::uint8* src, int width, int height, const Static&, juce::Image::BitmapData& out);

private:
    std::vector<float> y, u, v, a, b, cu, cv, pu, pv, hl, ph, gain, ct, bt;
    std::vector<int> map, ci, bi;   // each pixel's source through the curvature; chroma / bloom positions
    int mapW = 0, mapH = 0;
    float mapK = -1, mapEdge = -1;
};
// The same for a whole image, in place (the painted scenes and dropped-in photographs).
void bakeImage(juce::Image&, const Static&);

// The tape's animated state: advanced once per editor tick, drawn by paint().
class Tape {
public:
    explicit Tape(unsigned seed = 1983) : random(int(seed)) { reset(); }
    void reset();
    // Moves the tape on by `dt` seconds. `amount` as in Static, `wrong` 0..1, `fps` the rate of the
    // animated layers (24; 12 when frames are slow or in Eco). `calm`: a cross-fade is running
    // (no picture displacement then). True if a new tape frame is due (repaint the scene).
    bool advance(double dt, float amount, float wrong, double fps, bool calm);

    // What a frame shows.
    struct Slice { float y = 0, h = 0, dx = 0; };      // y, h: 0..1 of the scene's height; dx in px (at 960 wide)
    struct Streak { float x = 0, y = 0, length = 0, alpha = 0; juce::uint32 colour = 0xffffffff; };
    int grainFrame = 0;
    juce::Point<int> grainOffset;
    float jitter = 0;                          // the whole picture's sideways jump this frame (px)
    std::array<Slice, 3> wobble {};            // thin bands of lines nudged sideways
    int wobbleCount = 0;
    std::array<Streak, 16> dropouts {};        // white horizontal streaks
    int dropoutCount = 0;
    std::array<Streak, 12> speckle {};         // coloured specks
    int speckleCount = 0;
    float headShift = 0;                       // the head-switching strip's sideways shift (px)
    unsigned headSeed = 0;
    // The tracking band rolling through (strength 0 = none): centre and height 0..1 of the scene.
    float trackY = 0, trackH = 0, trackStrength = 0;
    unsigned trackSeed = 0;
    // A frame tear (Unreal / wrong Characters): below tearY the picture jumps sideways.
    float tearY = -1, tearShift = 0;
    float drift = 0;                           // the slow colour cast, -1..1 (the bake uses it)
    float warble = 0;                          // 0..1: how hard the tape is struggling (the audio warble follows)
    // Counters (tests): rolls and tears started, tape frames.
    int rolls = 0, tears = 0, frames = 0;
    double time() const { return now; }

private:
    juce::Random random;
    double now = 0, nextFrame = 0, nextRoll = 0, rollStart = -1, rollLength = 1, nextTear = 0, tearUntil = -1;
    float rollFrom = 0, rollTo = 0, rollHeight = 0.06f, rollPower = 1;
    double driftPhase1 = 0, driftPhase2 = 0;
    float lastAmount = -1, lastWrong = -1;
    double gap(double mean, double minimum);
};

// The fine moving grain (and the scanlines) over the whole viewfinder: three frames made once at
// screen resolution with their opacity baked in, drawn at a new offset each tape frame - a plain
// blend, several times cheaper than a tiled fill. Rebuilt only when the size or amount changes.
class Grain {
public:
    void prepare(int width, int height, float scale, float amount);   // logical size of the viewfinder
    void draw(juce::Graphics&, juce::Rectangle<int> scene, int frame, juce::Point<int> offset) const;
    bool ready() const { return frames[0].isValid(); }

private:
    std::array<juce::Image, 3> frames;
    int w = 0, h = 0;
    float s = 0, a = -1;
};

// Draws one tape frame over the picture. `picture` covers `pictureBounds` (the editor draws it
// stretched over the whole view); everything is drawn inside `scene` (the viewfinder above the
// control strip). `amount` as in Static.
void paint(juce::Graphics&, const juce::Image& picture, juce::Rectangle<float> pictureBounds, juce::Rectangle<int> scene, const Tape&, float amount, float wrong,
           const Grain* grain = nullptr);

// The camcorder's on-screen display: text in a blocky 5 x 7 font, each font pixel `px` wide,
// with a soft glow, a faint colour fringe and a slight per-glyph misalignment, as an image at
// `scale` (physical pixels per logical). Characters: A-Z 0-9 space . : - / and '>' (a play
// triangle), '*' (the REC dot).
juce::Image osdText(const juce::String& text, juce::Colour colour, float px, float scale);
float osdWidth(const juce::String& text, float px);   // logical width of osdText()

} // namespace liminal::ui::vhs
