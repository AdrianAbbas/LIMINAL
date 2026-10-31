// The GarageBand flicker hotfix (2026-10-31): the scene must be on screen in every frame.
//
// 240 consecutive editor frames at 30 fps in real time, as a host would show them: the editor's
// own timer tick (frame governor, the scene's compositing thread running alongside), then only
// the regions it asked for are painted into a persistent "screen" whose dirty regions start
// black (what a host hands a view). Meanwhile notes play through processBlock, the failing tube
// flickers with the hum, the place's events fire, the Character and the environment switch
// (photographic and painted places), Unreal slips the picture. Each frame:
//   - the screen's scene region against the settled picture of the same place and Character,
//     on a 12 x 8 grid of tiles: no tile may go dark (blank or partly blank), the whole scene
//     may not drop below half its brightness;
//   - the same for a full repaint of the editor;
//   - the picture the editor drew is opaque (a transparent picture is a blank screen on macOS).
// And the compositor alone: the picture take() hands out never changes until the next take().
// Editor CPU is reported: the process's CPU time minus the audio's and the test's own analysis.
#pragma once
#include <ctime>
#include <map>
#include <thread>
#include <sys/resource.h>

namespace frames {

double threadCpuMs() {
    timespec t {};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return double(t.tv_sec) * 1000.0 + double(t.tv_nsec) / 1.0e6;
}
double processCpuMs() {
    rusage u {};
    getrusage(RUSAGE_SELF, &u);
    return (double(u.ru_utime.tv_sec) + double(u.ru_stime.tv_sec)) * 1000.0 + (double(u.ru_utime.tv_usec) + double(u.ru_stime.tv_usec)) / 1000.0;
}

constexpr int cols = 12, rows = 8;
// Mean brightness (0..255) of each tile of `area`, sampled every third pixel.
std::vector<double> tiles(const juce::Image& image, juce::Rectangle<int> area) {
    std::vector<double> out(std::size_t(cols * rows), 0.0);
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    for (int ty = 0; ty < rows; ++ty)
        for (int tx = 0; tx < cols; ++tx) {
            const int x0 = area.getX() + area.getWidth() * tx / cols, x1 = area.getX() + area.getWidth() * (tx + 1) / cols;
            const int y0 = area.getY() + area.getHeight() * ty / rows, y1 = area.getY() + area.getHeight() * (ty + 1) / rows;
            double sum = 0;
            int n = 0;
            for (int y = y0; y < y1; y += 3)
                for (int x = x0; x < x1; x += 3) {
                    const auto c = d.getPixelColour(x, y);
                    sum += (int(c.getRed()) * 2 + int(c.getGreen()) * 5 + int(c.getBlue())) / 8.0;
                    ++n;
                }
            out[std::size_t(ty * cols + tx)] = sum / double(std::max(1, n));
        }
    return out;
}
// How a frame compares with the settled picture: 0 = fine, 1 = partly blank, 2 = blank.
int blankness(const std::vector<double>& frame, const std::vector<double>& reference, double& worstRatio) {
    double sumF = 0, sumR = 0;
    bool partly = false;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        sumF += frame[i];
        sumR += reference[i];
        if (reference[i] >= 10.0) {   // (tiles that are dark in the picture itself can't go blank)
            const double ratio = frame[i] / reference[i];
            worstRatio = std::min(worstRatio, ratio);
            // Blank means black (what the GarageBand bug showed: missing pictures), not a dimmer
            // lighting of the same place (a light group mid-flicker, or for a frame the previous
            // Character's light while a photograph's new look is composited).
            partly = partly || (ratio < 0.3 && frame[i] < 6.0) || frame[i] < 3.0;
        }
    }
    if (sumF < 0.5 * sumR && sumF < 8.0 * double(frame.size())) return 2;   // (tiles are 0-255 luma)
    return partly ? 1 : 0;
}
// Fraction of sampled pixels that are not fully opaque.
double transparency(const juce::Image& image) {
    if (!image.isValid()) return 1.0;
    if (!image.hasAlphaChannel()) return 0.0;
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    long seen = 0, clear = 0;
    for (int y = 0; y < image.getHeight(); y += 7)
        for (int x = 0; x < image.getWidth(); x += 7) {
            ++seen;
            clear += d.getPixelColour(x, y).getAlpha() < 255 ? 1 : 0;
        }
    return double(clear) / double(std::max(1L, seen));
}
std::uint64_t checksum(const juce::Image& image) {
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    std::uint64_t h = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); y += 5)
        for (int x = 0; x < image.getWidth(); x += 5) h = (h ^ d.getPixelColour(x, y).getARGB()) * 1099511628211ull;
    return h;
}

} // namespace frames

void testFrames(Suite& s) {
    s.section("frames");
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    const auto set = [&](const char* id, float value) {
        auto* param = p.parameters.getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) {
        s.check(false, "the editor is LIMINAL's");
        return;
    }
    auto& view = lm->view();
    const auto scene = view.sceneArea();
    set(params::activity, 1.0f);   // events and the ballast's stutter as often as they come

    // The script: (place, Character, Unreal) per stretch of frames. Hospital, pool and office are
    // photographs (light passes composited live); since Part 3 garage and hotel are photographs too.
    struct Stretch { int place, character, frames; float unreal; };
    const std::vector<Stretch> script {
        { 0, 0, 50, 0.0f }, { 4, 0, 40, 0.0f }, { 8, 2, 30, 0.8f }, { 3, 0, 12, 0.0f }, { 0, 2, 20, 0.0f },
        { 4, 2, 20, 0.0f }, { 5, 1, 12, 0.0f }, { 8, 0, 20, 0.0f }, { 0, 1, 16, 0.6f }, { 4, 1, 20, 0.0f },
    };
    // The settled picture of each place and Character (composited on the spot), as tiles.
    std::map<std::pair<int, int>, std::vector<double>> reference;
    for (const auto& st : script) {
        if (reference.count({ st.place, st.character }) != 0) continue;
        set(params::environment, float(st.place));
        set(params::character, float(st.character));
        view.animate();
        view.animate();
        reference[{ st.place, st.character }] = frames::tiles(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f), scene);
    }

    juce::Image screen(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g(screen);
        editor->paintEntireComponent(g, true);
    }
    juce::AudioBuffer<float> buffer(p.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    juce::Random random(31);
    int placeholders = 0, frame = 0, blankScreen = 0, partlyScreen = 0, blankFull = 0, partlyFull = 0, transparentFrames = 0, painted = 0, repaintedArea = 0;
    double worstScreen = 1e9, worstFull = 1e9, worstTransparency = 0;
    std::string firstBad;
    double audioMs = 0, analysisMs = 0;
    const int compositesBefore = view.photoComposites();
    int compositesSeen = 0, lastComposites = 0;
    double compositeMs = 0, lastCompositeMs = 0, paintMs = 0;
    const double wall0 = juce::Time::getMillisecondCounterHiRes(), cpu0 = frames::processCpuMs();
    double next = wall0;
    for (std::size_t k = 0; k < script.size(); ++k) {
        const auto& st = script[k];
        set(params::environment, float(st.place));   // the environment (and Character) switch: the editor follows on its next tick
        set(params::character, float(st.character));
        set(params::unreal, st.unreal);
        for (int f = 0; f < st.frames; ++f, ++frame) {
            // The audio for one frame (about 33 ms): a note now and then, the lamp's stutter, events.
            const double a0 = frames::threadCpuMs();
            for (int b = 0; b < 3; ++b) {
                midi.clear();
                if (b == 0 && frame % 6 == 0) midi.addEvent(on(48 + random.nextInt(24), 60 + random.nextInt(60)), 0);
                if (b == 1 && frame % 6 == 3) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                p.processBlock(buffer, midi);
            }
            if (frame % 5 == 1) p.meters.lamp.store(0.05f);   // the failing tube flickers
            if (frame % 23 == 7) ++p.meters.events;           // the place's own sounds
            audioMs += frames::threadCpuMs() - a0;

            view.tick();   // the editor's timer
            const auto dirty = view.lastRepaint();
            const double p0 = frames::threadCpuMs();
            for (const auto& r : dirty) {   // the host paints only what was asked for, into a region that starts black
                juce::Graphics g(screen);
                g.reduceClipRegion(r);
                g.fillAll(juce::Colours::black);
                editor->paintEntireComponent(g, true);
                repaintedArea += r.getWidth() * r.getHeight();
            }
            paintMs += frames::threadCpuMs() - p0;
            painted += dirty.isEmpty() ? 0 : 1;

            const double c0 = frames::threadCpuMs();
            // The settled picture of what is on screen (the old scene until the new one is swapped in).
            const auto& ref = reference.count({ view.shownPlace(), view.shownCharacter() }) != 0 ? reference[{ view.shownPlace(), view.shownCharacter() }] : reference[{ st.place, st.character }];
            const auto screenTiles = frames::tiles(screen, scene);
            const auto full = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
            const auto fullTiles = frames::tiles(full, scene);
            // Since the clean-transition fix a thumbnail never stands in (the old scene stays until
            // the new one is composited, then they cross-fade); counted in case one does. Such frames
            // only fail if they are actually dark (no tile over 3 %).
            const bool placeholder = view.lastFrame().thumbnail;
            const auto dark = [](const std::vector<double>& t) { return *std::max_element(t.begin(), t.end()) < 3.0 ? 2 : 0; };
            const int onScreen = placeholder ? dark(screenTiles) : frames::blankness(screenTiles, ref, worstScreen);
            const int whole = placeholder ? dark(fullTiles) : frames::blankness(fullTiles, ref, worstFull);
            placeholders += placeholder ? 1 : 0;
            const double clear = frames::transparency(view.paintedPicture());
            worstTransparency = std::max(worstTransparency, clear);
            blankScreen += onScreen == 2 ? 1 : 0;
            partlyScreen += onScreen == 1 ? 1 : 0;
            blankFull += whole == 2 ? 1 : 0;
            partlyFull += whole == 1 ? 1 : 0;
            transparentFrames += clear > 0.0 ? 1 : 0;
            if (firstBad.empty() && (onScreen != 0 || whole != 0 || clear > 0.0))
                firstBad = "frame " + std::to_string(frame) + " (place " + std::to_string(st.place + 1) + ", Character " + std::to_string(st.character + 1) + "): screen " +
                           std::to_string(onScreen) + ", full " + std::to_string(whole) + ", transparent " + num(clear * 100.0, 1) + " %";
            if (view.photoComposites() < lastComposites) {   // (a new place: a new compositor)
                compositesSeen += lastComposites;
                compositeMs += lastCompositeMs;
            }
            lastComposites = view.photoComposites();
            lastCompositeMs = view.photoCompositeMs();
            analysisMs += frames::threadCpuMs() - c0;

            next += 1000.0 / 30.0;   // real time: the compositor runs alongside, as in a host
            const double wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0) std::this_thread::sleep_for(std::chrono::microseconds(juce::int64(wait * 1000.0)));
        }
    }
    const double wallMs = juce::Time::getMillisecondCounterHiRes() - wall0, cpuMs = frames::processCpuMs() - cpu0;
    compositesSeen += lastComposites - compositesBefore;
    compositeMs += lastCompositeMs;
    s.check(frame >= 200, "200+ consecutive editor frames rendered (lights flickering, notes playing, places and Characters switching)", std::to_string(frame) + " frames");
    s.check(blankScreen == 0 && partlyScreen == 0, "no frame on screen is blank or partly blank (only the changed regions repainted)",
            std::to_string(blankScreen) + " blank, " + std::to_string(partlyScreen) + " partly; darkest tile " + num(worstScreen * 100.0, 1) + " % of the settled picture" +
                (firstBad.empty() ? "" : "; first: " + firstBad));
    s.check(blankFull == 0 && partlyFull == 0, "no full repaint is blank or partly blank",
            std::to_string(blankFull) + " blank, " + std::to_string(partlyFull) + " partly; darkest tile " + num(worstFull * 100.0, 1) + " %");
    s.check(transparentFrames == 0, "every picture the editor draws is opaque (alpha 255: macOS keeps RGB pictures as ARGB)",
            std::to_string(transparentFrames) + " frames, worst " + num(worstTransparency * 100.0, 1) + " % transparent");
    const double editorMs = cpuMs - audioMs - analysisMs;
    s.metric("frames painted / ticks", double(painted), (" / " + std::to_string(frame)).c_str());
    s.metric("frames showing a decoding place's thumbnail", double(placeholders));
    s.metric("mean repainted area per tick", double(repaintedArea) / double(std::max(1, frame)) / double(editor->getWidth() * editor->getHeight()) * 100.0, " % of the window");
    s.metric("compositor: pictures per second", double(compositesSeen) / (wallMs / 1000.0));
    s.metric("editor CPU (message thread + compositor, minus audio and the test's analysis), % of one core", editorMs / wallMs * 100.0, " %");
    s.metric("  of which painting (software renderer), % of one core", paintMs / wallMs * 100.0, (" (" + num(paintMs / std::max(1, painted), 1) + " ms a painted frame)").c_str());
    s.metric("  of which compositing (wall time on its own low-priority thread, so an upper bound), % of one core", compositeMs / wallMs * 100.0, (" (" + num(compositeMs / std::max(1, compositesSeen), 1) + " ms a picture)").c_str());
    s.report(compositesSeen / (wallMs / 1000.0) <= 31.0, "the compositor stays at or under 30 pictures a second", num(compositesSeen / (wallMs / 1000.0), 1));
    p.editorBeingDeleted(editor.get());
    editor.reset();

    // The pool's water is turquoise, not yellow (on macOS the compositor wrote RGB byte order into
    // an ARGB picture: red and blue swapped). Checked on the picture the compositor makes and on
    // what the editor puts on screen.
    {
        ui::PhotoScene pool(4);
        pool.ensureLoaded();
        const auto picture = pool.renderNow(pool.defaultLook());
        const auto water = [](const juce::Image& image, juce::Rectangle<float> area01) {
            const auto r = area01.transformedBy(juce::AffineTransform::scale(float(image.getWidth()), float(image.getHeight()))).toNearestInt();
            double red = 0, green = 0, blue = 0;
            int n = 0;
            for (int y = r.getY(); y < r.getBottom(); y += 2)
                for (int x = r.getX(); x < r.getRight(); x += 2) {
                    const auto c = image.getPixelAt(x, y);
                    red += c.getRed(); green += c.getGreen(); blue += c.getBlue();
                    ++n;
                }
            return std::array<double, 3> { red / n, green / n, blue / n };
        };
        const auto w = water(picture, { 0.18f, 0.62f, 0.12f, 0.06f });   // the water left of the sun patch, near the camera
        s.check(w[2] > w[0] + 25.0 && w[1] > w[0] + 25.0, "the pool's water is turquoise in the composite (blue and green well over red)",
                "R " + num(w[0], 1) + " G " + num(w[1], 1) + " B " + num(w[2], 1));
    }

    // The compositor alone, pushed hard: every picture take() hands out is complete and opaque,
    // and it is never written to while the editor holds it (until the next take()).
    {
        ui::PhotoScene photo(0);
        photo.ensureLoaded();
        const auto base = photo.defaultLook();
        const auto refTiles = frames::tiles(photo.renderNow(base), { 0, 0, 1440, 1080 });
        {   // one picture, on this thread: one light changed, then two
            auto look = base;
            const double t0 = frames::threadCpuMs();
            for (int k = 0; k < 10; ++k) {
                look.levels[1] = 0.5f + 0.05f * float(k);
                (void) photo.renderNow(look);
            }
            const double t1 = frames::threadCpuMs();
            for (int k = 0; k < 10; ++k) {
                look.levels[1] = 0.5f + 0.05f * float(k);
                look.levels[2] = 0.7f + 0.05f * float(k);
                (void) photo.renderNow(look);
            }
            s.metric("hospital composite, one light changed / two (CPU)", (t1 - t0) / 10.0, (" / " + num((frames::threadCpuMs() - t1) / 10.0, 1) + " ms").c_str());
        }
        int taken = 0, changedWhileHeld = 0, bad = 0;
        double worst = 1e9;
        juce::Image held;
        std::uint64_t heldSum = 0;
        const double until = juce::Time::getMillisecondCounterHiRes() + 2500.0;
        for (int k = 0; juce::Time::getMillisecondCounterHiRes() < until; ++k) {
            auto look = base;
            for (std::size_t i = 1; i < look.levels.size(); ++i) look.levels[i] *= (k + int(i)) % 3 == 0 ? 0.1f : 1.3f;   // lights flicker hard
            photo.request(look);
            if (held.isValid() && frames::checksum(held) != heldSum) ++changedWhileHeld;
            juce::Image next;
            if (photo.take(next)) {
                held = next;
                heldSum = frames::checksum(held);
                ++taken;
                if (frames::transparency(held) > 0.0 || frames::blankness(frames::tiles(held, held.getBounds()), refTiles, worst) == 2) ++bad;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        s.check(taken >= 20 && bad == 0, "the compositor hands out only complete, opaque pictures (lights flickering hard)",
                std::to_string(taken) + " pictures, " + std::to_string(bad) + " bad");
        s.check(changedWhileHeld == 0, "a picture the editor holds is never written to (double-buffered: the worker composites into a back image)",
                std::to_string(changedWhileHeld) + " changes seen");
        s.report(taken <= 2.5 * 31, "the compositor's 30 fps cap holds when asked every 4 ms", std::to_string(taken) + " pictures in 2.5 s");
    }
}

// Clean place and Character changes (2026-10-31: "the previous place's image stays on screen for a
// split second and overlaps the new one"). The new scene is prepared off screen, swapped in whole in
// one frame, then cross-faded. Every frame around a switch is rendered (the editor's own ticks in
// real time, the compositor on its thread) and fitted as  frame = (1 - a) x old + a x new  against
// the settled pictures of the two scenes, separately for the picture and for the frosted glass of
// the control strip:
//   - the fit is exact (small residual): no third picture - no thumbnail, no stale frame, no old
//     Character's lights under the new one, no frosted glass from the wrong scene;
//   - a only grows, from 0 to 1, and matches the fade the editor reports;
//   - the caption and the Character chips name the old scene exactly while a = 0 and the new
//     scene as soon as a > 0 (HUD, Characters and lights change in the same frame);
//   - the picture and the glass move together.
namespace transitions {
struct Samples {
    std::vector<float> scene, glass;
};
// The scene below the captions and above the strip, and the frosted bands of the strip that have no
// controls on them (above the film strip and between it and the knobs), every 4th pixel, RGB.
Samples sample(const juce::Image& image) {
    Samples out;
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    const auto add = [&](std::vector<float>& v, int x0, int x1, int y0, int y1, int step) {
        for (int y = y0; y < y1; y += step)
            for (int x = x0; x < x1; x += step) {
                const auto c = d.getPixelColour(x, y);
                v.push_back(c.getRed());
                v.push_back(c.getGreen());
                v.push_back(c.getBlue());
            }
    };
    add(out.scene, 30, 930, 140, 552, 4);
    add(out.glass, 40, 920, 566, 573, 2);
    add(out.glass, 40, 920, 627, 633, 2);
    return out;
}
struct Fit { double a = 0, residual = 0, spread = 0; };
// Least-squares a in f = A + a (B - A), and the RMS residual (0-255).
Fit fit(const std::vector<float>& f, const std::vector<float>& A, const std::vector<float>& B) {
    double num = 0, den = 0;
    for (std::size_t i = 0; i < f.size(); ++i) {
        num += double(f[i] - A[i]) * double(B[i] - A[i]);
        den += double(B[i] - A[i]) * double(B[i] - A[i]);
    }
    Fit out;
    out.a = den > 0 ? num / den : 0.0;
    double r = 0;
    for (std::size_t i = 0; i < f.size(); ++i) {
        const double e = double(f[i]) - (double(A[i]) + out.a * double(B[i] - A[i]));
        r += e * e;
    }
    out.residual = std::sqrt(r / double(std::max<std::size_t>(1, f.size())));
    out.spread = std::sqrt(den / double(std::max<std::size_t>(1, f.size())));   // how different the two scenes are
    return out;
}
} // namespace transitions

void testTransitions(Suite& s) {
    s.section("frames: clean place and Character changes");
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    const auto set = [&](const char* id, float value) {
        auto* param = p.parameters.getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) {
        s.check(false, "the editor is LIMINAL's");
        return;
    }
    auto& view = lm->view();
    // Still lights, so the only change on screen is the switch: no events, no notes, Eco (no
    // caustic drift), the mouse elsewhere.
    set(params::activity, 0.0f);
    set(params::quality, 0.0f);
    set(params::unreal, 0.0f);
    const auto snapshot = [&] { return editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f); };
    double next = juce::Time::getMillisecondCounterHiRes();
    const auto frameTick = [&] {   // one editor frame in real time
        next += 1000.0 / 30.0;
        const double wait = next - juce::Time::getMillisecondCounterHiRes();
        if (wait > 0) std::this_thread::sleep_for(std::chrono::microseconds(juce::int64(wait * 1000.0)));
        else next = juce::Time::getMillisecondCounterHiRes();
        view.tick();
    };
    struct Case {
        const char* name;
        int fromPlace, fromCharacter, toPlace, toCharacter;
        int hoverFirst = -1;       // hover this film frame for a while before clicking (decoded ahead)
        int detourPlace = -1;      // click this place first, then the target one frame later
    };
    const std::vector<Case> cases {
        { "hospital -> pool (place, decoded on the click)", 0, 0, 4, 0 },
        { "pool -> pool, Submerged (Character, same scene)", 4, 0, 4, 2 },
        { "pool -> office (place)", 4, 2, 8, 1 },
        { "office -> pool (back: the scene just left, kept decoded)", 8, 1, 4, 2 },
        { "pool -> hospital, hovered first (decoded ahead)", 4, 2, 0, 0, 0 },
        { "hospital -> garage, then hotel a frame later (changed mind)", 0, 0, 5, 1, -1, 3 },
        { "hotel -> hotel, Room 404 (Character)", 5, 1, 5, 2 },
    };
    int badFits = 0, badOrder = 0, badHud = 0, badReported = 0, badGlass = 0, thumbnails = 0, unfinished = 0, totalFrames = 0;
    double worstResidual = 0, worstGlass = 0, slowest = 0;
    std::string firstBad;
    for (const auto& c : cases) {
        // Settle on the old scene (composited on the spot), then run real frames until nothing moves.
        set(params::environment, float(c.fromPlace));
        set(params::character, float(c.fromCharacter));
        view.animate();
        view.animate();
        for (int k = 0; k < 20 || view.switching(); ++k) frameTick();
        const auto A = transitions::sample(snapshot());
        // The switch, then every frame until the fade is over (and a few more).
        if (c.hoverFirst >= 0) {
            view.placeSelector().onHover(c.hoverFirst);
            for (int k = 0; k < 15; ++k) frameTick();   // ~0.5 s with the mouse over it: decoded meanwhile
            view.placeSelector().onHover(-1);
        }
        struct Frame { transitions::Samples samples; MainView::FrameInfo info; double ms; };
        std::vector<Frame> recorded;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        if (c.detourPlace >= 0) {
            view.placeSelector().choose(c.detourPlace);
            frameTick();
            recorded.push_back({ transitions::sample(snapshot()), view.lastFrame(), juce::Time::getMillisecondCounterHiRes() - t0 });
        }
        view.placeSelector().choose(c.toPlace);
        view.characterSelector().choose(c.toCharacter);
        int after = 0;
        for (int k = 0; k < 90 && after < 4; ++k) {
            frameTick();
            recorded.push_back({ transitions::sample(snapshot()), view.lastFrame(), juce::Time::getMillisecondCounterHiRes() - t0 });
            after = view.switching() ? 0 : after + 1;
        }
        if (view.switching()) ++unfinished;
        const auto B = recorded.back().samples;
        // Each frame against the two settled pictures.
        double lastA = -1, firstNewMs = -1, fadeEndMs = -1;
        for (const auto& f : recorded) {
            ++totalFrames;
            const auto pic = transitions::fit(f.samples.scene, A.scene, B.scene);
            const auto glass = transitions::fit(f.samples.glass, A.glass, B.glass);
            const bool hudNew = f.info.captionPlace == c.toPlace && f.info.captionCharacter == c.toCharacter && f.info.chipsPlace == c.toPlace && f.info.chipsCharacter == c.toCharacter;
            const bool hudOld = f.info.captionPlace == c.fromPlace && f.info.captionCharacter == c.fromCharacter && f.info.chipsPlace == c.fromPlace && f.info.chipsCharacter == c.fromCharacter;
            const double reported = f.info.fromPlace < 0 ? (hudNew ? 1.0 : 0.0) : double(f.info.alpha);
            worstResidual = std::max(worstResidual, pic.residual);
            std::string bad;
            if (pic.residual > 3.0) { ++badFits; bad = "the picture is not old/new alone or a blend of the two (residual " + num(pic.residual, 2) + ")"; }
            if (pic.a < lastA - 0.02) { ++badOrder; bad = "the fade went backwards (" + num(lastA, 2) + " -> " + num(pic.a, 2) + ")"; }
            if (!((pic.a < 0.02 && hudOld) || (pic.a > 0.02 && hudNew))) { ++badHud; bad = "caption/chips do not match the picture (a = " + num(pic.a, 3) + ")"; }
            if (std::abs(pic.a - reported) > 0.05) { ++badReported; bad = "the picture's fade " + num(pic.a, 3) + " is not the editor's " + num(reported, 3); }
            if (glass.spread > 2.0) {   // (only where the two scenes' glass differs visibly)
                worstGlass = std::max(worstGlass, std::max(glass.residual, std::abs(glass.a - pic.a) * glass.spread));
                if (glass.residual > 3.0 || std::abs(glass.a - pic.a) > 0.08) { ++badGlass; bad = "the frosted glass is not in step with the picture (a " + num(glass.a, 2) + " vs " + num(pic.a, 2) + ")"; }
            }
            if (f.info.thumbnail) { ++thumbnails; bad = "a thumbnail stood in"; }
            if (firstBad.empty() && !bad.empty()) firstBad = std::string(c.name) + ", " + num(f.ms, 0) + " ms: " + bad;
            if (pic.a > 0.02 && firstNewMs < 0) firstNewMs = f.ms;
            if (pic.a > 0.98 && fadeEndMs < 0) fadeEndMs = f.ms;
            lastA = std::max(lastA, pic.a);
        }
        slowest = std::max(slowest, firstNewMs);
        s.metric(std::string(c.name) + ": click to the first frame of the new scene / fade", firstNewMs, (" / " + num(fadeEndMs - firstNewMs, 0) + " ms").c_str());
    }
    s.check(badFits == 0 && thumbnails == 0, "every frame around a switch is the old scene, the new one, or a cross-fade of exactly those two (no stale frame, thumbnail or wrong light passes)",
            std::to_string(badFits) + " bad, " + std::to_string(thumbnails) + " thumbnails in " + std::to_string(totalFrames) + " frames; worst residual " + num(worstResidual, 2) + " / 255" +
                (firstBad.empty() ? "" : "; first: " + firstBad));
    s.check(badHud == 0, "caption and Character chips change in the same frame as the picture", std::to_string(badHud) + " frames out of step");
    s.check(badOrder == 0 && badReported == 0, "the fade only moves forward, as far as the editor says", std::to_string(badOrder) + " backwards, " + std::to_string(badReported) + " off");
    s.check(badGlass == 0, "the frosted glass of the strip fades in step with the picture", std::to_string(badGlass) + " frames; worst " + num(worstGlass, 2));
    s.check(unfinished == 0, "every switch finishes", std::to_string(unfinished) + " still switching after 3 s");
    s.report(slowest < 700.0, "the new scene is on screen within 0.7 s of the click (decoding included, this container)", num(slowest, 0) + " ms");
    p.editorBeingDeleted(editor.get());
}
