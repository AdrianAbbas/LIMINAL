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
            partly = partly || ratio < 0.3 || frame[i] < 3.0;
        }
    }
    if (sumF < 0.5 * sumR) return 2;
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
    // photographs (light passes composited live); garage and hotel are painted.
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
    int frame = 0, blankScreen = 0, partlyScreen = 0, blankFull = 0, partlyFull = 0, transparentFrames = 0, painted = 0, repaintedArea = 0;
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
            const auto& ref = reference[{ st.place, st.character }];
            const auto screenTiles = frames::tiles(screen, scene);
            const int onScreen = frames::blankness(screenTiles, ref, worstScreen);
            const auto full = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
            const int whole = frames::blankness(frames::tiles(full, scene), ref, worstFull);
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
