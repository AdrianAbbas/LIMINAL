// The VHS "found footage" look (2026-10-31 evening).
//
// - editor CPU in normal playing (one place, a note every 2 s, default Activity), measured the
//   same way as the frames test, at each VHS amount (Off / Light / Heavy; before the VHS look
//   existed, the parameter is absent and only one row is measured);
// - the static bake (softening, chroma bleed, grade) lives in the composite: a still picture
//   makes no new composites, only the cheap animated layers repaint;
// - the glitch schedule: sparse and irregular, never strobing; Unreal and the "wrong" Characters
//   glitch more;
// - the snapshots of every place at Off / Light / Heavy (writeVhsSnapshots, --snapshots).
#pragma once

namespace vhstest {

struct CpuResult { double editor = 0, paint = 0, composite = 0, painted = 0, ticks = 0, area = 0; };

// `seconds` of real-time editor frames in `place`, a note every 2 s, painting only what the
// editor asks for (as a host does). Editor CPU = process CPU minus the audio and the test itself.
inline CpuResult normalPlaying(LiminalProcessor& p, juce::AudioProcessorEditor& editor, MainView& view, int place, double seconds) {
    const auto set = [&](const char* id, float value) {
        auto* param = p.parameters.getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    set(params::environment, float(place));
    set(params::character, 0.0f);
    set(params::unreal, 0.0f);
    for (int k = 0; k < 3; ++k) view.animate();
    juce::Image screen(juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    {
        juce::Graphics g(screen);
        editor.paintEntireComponent(g, true);
    }
    juce::AudioBuffer<float> buffer(p.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    juce::Random random(7);
    const int frames = int(seconds * 30.0);
    double audioMs = 0, paintMs = 0;
    long area = 0;
    int painted = 0;
    const int composites0 = view.photoComposites();
    const double composite0 = view.photoCompositeMs();
    const double wall0 = juce::Time::getMillisecondCounterHiRes(), cpu0 = frames::processCpuMs();
    double next = wall0;
    for (int f = 0; f < frames; ++f) {
        const double a0 = frames::threadCpuMs();
        for (int b = 0; b < 3; ++b) {
            midi.clear();
            if (b == 0 && f % 60 == 0) midi.addEvent(on(55 + random.nextInt(12), 80), 0);
            if (b == 0 && f % 60 == 40) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
            p.processBlock(buffer, midi);
        }
        audioMs += frames::threadCpuMs() - a0;
        view.tick();
        const double p0 = frames::threadCpuMs();
        for (const auto& r : view.lastRepaint()) {
            juce::Graphics g(screen);
            g.reduceClipRegion(r);
            g.fillAll(juce::Colours::black);
            editor.paintEntireComponent(g, true);
            area += long(r.getWidth()) * r.getHeight();
        }
        paintMs += frames::threadCpuMs() - p0;
        painted += view.lastRepaint().isEmpty() ? 0 : 1;
        next += 1000.0 / 30.0;
        const double wait = next - juce::Time::getMillisecondCounterHiRes();
        if (wait > 0) std::this_thread::sleep_for(std::chrono::microseconds(juce::int64(wait * 1000.0)));
    }
    const double wallMs = juce::Time::getMillisecondCounterHiRes() - wall0, cpuMs = frames::processCpuMs() - cpu0;
    CpuResult r;
    r.editor = (cpuMs - audioMs) / wallMs * 100.0;
    r.paint = paintMs / wallMs * 100.0;
    r.composite = (view.photoComposites() >= composites0 ? view.photoCompositeMs() - composite0 : view.photoCompositeMs()) / wallMs * 100.0;
    r.painted = painted;
    r.ticks = frames;
    r.area = double(area) / double(frames) / double(editor.getWidth() * editor.getHeight()) * 100.0;
    return r;
}

} // namespace vhstest

// Editor CPU in normal playing, per VHS amount (report-only metrics; the before/after table in
// docs/DEVLOG.md comes from here and from the frames test).
void testEditorCpu(Suite& s) {
    s.section("editor CPU, normal playing");
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) {
        s.check(false, "the editor is LIMINAL's");
        return;
    }
    auto* vhs = p.parameters.getParameter("vhs");
    const char* names[] { "Off", "Light", "Heavy" };
    for (int level = 0; level < (vhs != nullptr ? 3 : 1); ++level) {
        if (vhs != nullptr) vhs->setValueNotifyingHost(vhs->convertTo0to1(float(level)));
        const auto r = vhstest::normalPlaying(p, *editor, lm->view(), 0, 8.0);
        const std::string label = vhs != nullptr ? std::string("VHS ") + names[level] : std::string("no VHS look (before)");
        s.metric("normal playing, hospital, " + label + ": editor CPU, % of one core", r.editor, " %");
        s.metric("  " + label + ": painting / compositing, % of one core", r.paint, (" / " + num(r.composite, 1) + " %").c_str());
        s.metric("  " + label + ": frames painted / ticks, mean repainted area", r.painted, (" / " + num(r.ticks, 0) + ", " + num(r.area, 1) + " % of the window").c_str());
    }
    p.editorBeingDeleted(editor.get());
}

// docs/snapshots: every place at VHS Off / Light / Heavy (one sheet per place, the whole window),
// full-size frames of one place, and the glitches (a tracking roll, a tear, the head switching).
int writeVhsSnapshots(const juce::String& path) {
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    folder.createDirectory();
    const auto save = [&](const juce::Image& image, const juce::String& name) {
        auto file = folder.getChildFile(name + ".jpg");
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::JPEGImageFormat jpeg;
        jpeg.setQuality(0.88f);
        jpeg.writeImageToStream(image, out);
    };
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
    auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
    if (lm == nullptr) return 1;
    auto& view = lm->view();
    const auto set = [&](const char* id, float value) {
        auto* param = p.parameters.getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    const auto frame = [&] {
        for (int k = 0; k < 4; ++k) view.animate();   // the scene, then a few tape frames
        view.settlePicture();
        return editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    };
    const char* names[] { "hospital", "mall", "school", "garage", "pool", "hotel", "subway", "supermarket", "office", "impossible" };
    const char* levels[] { "OFF", "LIGHT", "HEAVY" };
    const int w = 640, h = 480;
    for (int place = 0; place < dsp::placeCount; ++place) {
        set(params::environment, float(place));
        set(params::character, 0.0f);
        juce::Image sheet(juce::Image::RGB, w * 3, h, true);
        juce::Graphics g(sheet);
        for (int level = 0; level < 3; ++level) {
            set(params::vhs, float(level));
            const auto shot = frame();
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(shot, level * w, 0, w, h, 0, 0, shot.getWidth(), shot.getHeight());
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect(level * w + 8, h - 28, 150, 20);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(13.0f));
            g.drawText(juce::String("VHS ") + levels[level], level * w + 14, h - 28, 140, 20, juce::Justification::centredLeft);
            if (place == 0 || place == 4) save(shot, "vhs-" + juce::String(names[place]) + "-" + juce::String(levels[level]).toLowerCase() + "-1x");
        }
        save(sheet, "vhs-" + juce::String(place + 1).paddedLeft('0', 2) + "-" + names[place] + "-off-light-heavy");
    }
    {   // The glitches, posed: a tracking band rolling through, a frame tear (Something Wrong), the
        // head-switching strip (always there, at the bottom of the viewfinder), a wrong Character's roll.
        set(params::vhs, 2.0f);
        set(params::environment, 0.0f);
        juce::Image sheet(juce::Image::RGB, w * 2, h * 2, true);
        juce::Graphics g(sheet);
        const char* captions[] { "TRACKING BAND ROLLING THROUGH (HEAVY)", "FRAME TEAR (SOMETHING WRONG)", "HEAD-SWITCHING NOISE, BOTTOM OF THE FRAME (CROP, 2X)",
                                 "ROOM 404: A WRONG CHARACTER'S ROLL" };
        for (int k = 0; k < 4; ++k) {
            set(params::environment, k == 3 ? 5.0f : 0.0f);
            set(params::character, k == 1 ? 2.0f : k == 3 ? 2.0f : 0.0f);
            for (int i = 0; i < 6; ++i) view.animate();
            view.settlePicture();
            auto& t = view.tapeForTests();
            t.trackStrength = k == 0 || k == 3 ? 1.1f : 0.0f;
            t.trackY = k == 3 ? 0.62f : 0.4f;
            t.trackH = 0.09f;
            t.tearY = k == 1 ? 0.55f : -1.0f;
            t.tearShift = 38.0f;
            auto shot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
            if (k == 2) shot = shot.getClippedImage(view.sceneArea().withTop(view.sceneArea().getBottom() - 240).withWidth(320).withX(320));
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(shot, (k % 2) * w, (k / 2) * h, w, h, 0, 0, shot.getWidth(), shot.getHeight());
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect((k % 2) * w + 8, (k / 2) * h + h - 28, 420, 20);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(13.0f));
            g.drawText(captions[k], (k % 2) * w + 14, (k / 2) * h + h - 28, 410, 20, juce::Justification::centredLeft);
        }
        save(sheet, "vhs-glitches");
    }
    set(params::vhs, 1.0f);
    set(params::environment, 0.0f);
    set(params::character, 0.0f);
    p.editorBeingDeleted(editor.get());
    std::cout << "wrote VHS snapshots to " << folder.getFullPathName() << "\n";
    return 0;
}

namespace vhstest {
// Mean |difference| between horizontal neighbours (a sharpness measure), luma, on a 0..255 scale.
inline double sharpness(const juce::Image& image) {
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    double sum = 0;
    long n = 0;
    for (int y = 4; y < image.getHeight() - 4; y += 3)
        for (int x = 4; x < image.getWidth() - 5; ++x) {
            const auto a = d.getPixelColour(x, y), b = d.getPixelColour(x + 1, y);
            sum += std::abs((a.getRed() * 2 + a.getGreen() * 5 + a.getBlue()) - (b.getRed() * 2 + b.getGreen() * 5 + b.getBlue())) / 8.0;
            ++n;
        }
    return sum / double(std::max(1L, n));
}
// The mean colour of an image, as (r, g, b) 0..255.
inline std::array<double, 3> meanColour(const juce::Image& image) {
    const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
    double r = 0, g = 0, b = 0;
    long n = 0;
    for (int y = 0; y < image.getHeight(); y += 4)
        for (int x = 0; x < image.getWidth(); x += 4) {
            const auto c = d.getPixelColour(x, y);
            r += c.getRed();
            g += c.getGreen();
            b += c.getBlue();
            ++n;
        }
    return { r / double(n), g / double(n), b / double(n) };
}
// Bakes a synthetic picture (RGB bytes) and returns the result.
inline juce::Image bakeSynthetic(const std::vector<juce::uint8>& src, int w, int h, ui::vhs::Static st) {
    juce::Image out(juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData d(out, juce::Image::BitmapData::writeOnly);
    ui::vhs::Baker baker;
    baker.bake(src.data(), w, h, st, d);
    return out;
}
} // namespace vhstest

void testVhs(Suite& s) {
    s.section("vhs");
    // --- The parameters: Off / Light / Heavy, Light by default, saved with the song; the warble off.
    {
        LiminalProcessor p;
        auto* vhs = dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter(params::vhs));
        auto* warble = p.parameters.getParameter(params::vhsWarble);
        s.check(vhs != nullptr && vhs->choices == juce::StringArray { "Off", "Light", "Heavy" } && vhs->getIndex() == 1, "VHS look: Off / Light / Heavy, Light by default");
        s.check(warble != nullptr && warble->getValue() < 0.5f, "the tape warble on the audio is off by default");
        vhs->setValueNotifyingHost(vhs->convertTo0to1(2.0f));
        juce::MemoryBlock state;
        p.getStateInformation(state);
        LiminalProcessor fresh;
        fresh.setStateInformation(state.getData(), int(state.getSize()));
        s.check(std::lround(fresh.parameters.getRawParameterValue(params::vhs)->load()) == 2, "the VHS amount is saved with the song (Heavy comes back)");
        p.resetToDefaults();   // a scene preset / Init
        s.check(std::lround(p.parameters.getRawParameterValue(params::vhs)->load()) == 2, "loading a scene preset keeps the chosen VHS amount (a viewing preference)");
    }
    // --- The static bake, on synthetic pictures: softness, chroma bleed and offset, red/cyan
    // misregistration, the grade.
    {
        const int w = 720, h = 540;
        std::vector<juce::uint8> src(std::size_t(w * h * 3), 128);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                auto* p = src.data() + std::size_t((y * w + x) * 3);
                const bool red = x >= 300 && x < 400 && y >= 200 && y < 340;   // a red block on grey
                const bool stripes = y >= 400 && ((x / 2) % 2 == 0);           // fine vertical stripes below
                const bool bar = y >= 80 && y < 160 && x >= 100 && x < 220;      // a white bar (its edges show the misregistration)
                p[0] = red ? 200 : stripes || bar ? 230 : 128;
                p[1] = red ? 40 : stripes || bar ? 230 : 128;
                p[2] = red ? 40 : stripes || bar ? 230 : 128;
            }
        const auto light = vhstest::bakeSynthetic(src, w, h, { 0.55f, 0.0f, 0.0f }), heavy = vhstest::bakeSynthetic(src, w, h, { 1.0f, 0.0f, 0.0f });
        // Fine stripes (2 px) are nearly gone: SD tape resolves ~240 TV lines.
        const auto contrast = [&](const juce::Image& im) {
            double lo = 255, hi = 0;
            for (int x = 200; x < 520; ++x) {
                const double l = im.getPixelAt(x, 470).getBrightness() * 255.0;
                lo = std::min(lo, l);
                hi = std::max(hi, l);
            }
            return hi - lo;
        };
        s.check(contrast(light) < 0.4 * 102.0 && contrast(heavy) < 0.8 * contrast(light), "soft like SD tape: fine stripes (~270 TV lines) lose most of their contrast (Light; Heavy softer still)",
                "Light " + num(contrast(light), 1) + ", Heavy " + num(contrast(heavy), 1) + " of 102");
        // Colour runs past the block's right edge (smeared and offset), much less on the left.
        const auto redness = [](juce::Colour c) { return double(c.getRed()) - 0.5 * (c.getGreen() + c.getBlue()); };
        const double right = redness(light.getPixelAt(412, 270)), left = redness(light.getPixelAt(288, 270)), inside = redness(light.getPixelAt(350, 270));
        s.check(right > 3.0 * std::max(1.0, left) && right > 15.0, "chroma bleed: the red runs past the block's right edge, offset to the right", "12 px right " + num(right, 1) + ", 12 px left " + num(left, 1) + ", inside " + num(inside, 1));
        // Red/cyan misregistration on a luma edge: just inside the right edge of a white bar, red is ahead of green.
        double fringe = 0;
        for (int x = 92; x < 112; ++x) fringe = std::max(fringe, double(heavy.getPixelAt(x, 120).getGreen()) - double(heavy.getPixelAt(x, 120).getRed()));   // the bar's left edge: cyan
        s.check(fringe > 4.0, "red/cyan misregistration at edges", num(fringe, 1) + " levels");
        // Reds oversaturated: the block's red against the grey is stronger than in the source.
        const double srcRed = 200.0 - 40.0;
        s.check(inside > 0.9 * srcRed * (light.getPixelAt(350, 100).getBrightness() > 0 ? 1.0 : 1.0) && redness(heavy.getPixelAt(350, 270)) >= inside - 2.0,
                "reds stay strong (oversaturated relative to the faded rest)", "red " + num(inside, 1) + " (source " + num(srcRed, 0) + ")");
        // Lifted, milky blacks and slightly clipped highlights.
        std::vector<juce::uint8> black(std::size_t(w * h * 3), 0), white(std::size_t(w * h * 3), 255);
        const auto b = vhstest::bakeSynthetic(black, w, h, { 0.55f, 0.0f, 0.0f }).getPixelAt(w / 2, h / 2), bh = vhstest::bakeSynthetic(black, w, h, { 1.0f, 0.0f, 0.0f }).getPixelAt(w / 2, h / 2);
        const auto wh = vhstest::bakeSynthetic(white, w, h, { 1.0f, 0.0f, 0.0f }).getPixelAt(w / 2, h / 2);
        s.check(b.getGreen() >= 8 && bh.getGreen() > b.getGreen() && bh.getBlue() >= bh.getRed(), "lifted, milky blacks (Heavy lifts more, a little blue-grey)",
                "Light " + juce::String(b.getRed()).toStdString() + "/" + juce::String(b.getGreen()).toStdString() + "/" + juce::String(b.getBlue()).toStdString() + ", Heavy " +
                    juce::String(bh.getRed()).toStdString() + "/" + juce::String(bh.getGreen()).toStdString() + "/" + juce::String(bh.getBlue()).toStdString());
        s.check(wh.getRed() < 252 && wh.getRed() > 235, "highlights clip a little below full white", juce::String(wh.getRed()).toStdString());
        // The colour cast: + magenta lowers green against red/blue, - green raises it.
        std::vector<juce::uint8> grey(std::size_t(w * h * 3), 128);
        const auto mag = vhstest::bakeSynthetic(grey, w, h, { 1.0f, 1.0f, 0.0f }).getPixelAt(w / 2, h / 2), grn = vhstest::bakeSynthetic(grey, w, h, { 1.0f, -1.0f, 0.0f }).getPixelAt(w / 2, h / 2);
        s.check(mag.getGreen() + 4 < grn.getGreen() && mag.getRed() > grn.getRed(), "the faint green / magenta cast moves the colour",
                "magenta G " + juce::String(mag.getGreen()).toStdString() + ", green G " + juce::String(grn.getGreen()).toStdString());
        // Cost of the bake alone (720 x 540).
        juce::Image out(juce::Image::ARGB, w, h, true);
        ui::vhs::Baker baker;
        const double t0 = frames::threadCpuMs();
        for (int k = 0; k < 8; ++k) {
            juce::Image::BitmapData d(out, juce::Image::BitmapData::writeOnly);
            baker.bake(src.data(), w, h, { 0.55f, 0.2f, 0.0f }, d);
        }
        s.metric("bake 720 x 540 (CPU, software)", (frames::threadCpuMs() - t0) / 8.0, " ms");
    }
    // --- The photographs: each place keeps its own grade underneath; the composite's cost.
    {
        std::vector<std::vector<double>> off, light;
        const auto layout = [](const juce::Image& image) {   // mean colour of a 4 x 3 grid of cells
            std::vector<double> v;
            for (int cy = 0; cy < 3; ++cy)
                for (int cx = 0; cx < 4; ++cx) {
                    const auto m = vhstest::meanColour(image.getClippedImage({ image.getWidth() * cx / 4, image.getHeight() * cy / 3, image.getWidth() / 4, image.getHeight() / 3 }));
                    v.insert(v.end(), m.begin(), m.end());
                }
            return v;
        };
        double sharpOff = 0, sharpLight = 0, sharpHeavy = 0;
        for (int place = 0; place < dsp::placeCount; ++place) {
            ui::PhotoScene scene(place);
            if (!scene.ensureLoaded()) continue;
            auto look = scene.characterLook(0);
            look.grade = dsp::character(place, 0).grade;
            look.light = dsp::character(place, 0).light;
            const auto a = scene.renderNow(look);
            look.vhs = { 0.55f, 0.0f, 0.0f };
            const auto b = scene.renderNow(look);
            look.vhs = { 1.0f, 0.0f, 0.0f };
            const auto c = scene.renderNow(look);
            const auto small = a.rescaled(b.getWidth(), b.getHeight(), juce::Graphics::highResamplingQuality);
            off.push_back(layout(small));
            light.push_back(layout(b));
            sharpOff += vhstest::sharpness(small);
            sharpLight += vhstest::sharpness(b);
            sharpHeavy += vhstest::sharpness(c);
            if (place == 0) s.check(b.getWidth() == a.getWidth() / 2 && b.getHeight() == a.getHeight() / 2, "with the tape look the scene is composited at half size (~480 lines, like VHS)",
                                    juce::String(b.getWidth()).toStdString() + " x " + juce::String(b.getHeight()).toStdString());
        }
        int own = 0;
        double worst = 1e9;
        for (std::size_t i = 0; i < light.size(); ++i) {
            double best = 1e9, mine = 0;
            std::size_t at = 0;
            for (std::size_t j = 0; j < off.size(); ++j) {
                double d = 0;
                for (std::size_t c = 0; c < light[i].size(); ++c) d += std::pow(light[i][c] - off[j][c], 2.0);
                if (j == i) mine = d;
                if (d < best) { best = d; at = j; }
            }
            own += at == i ? 1 : 0;
            if (at != i) std::cout << "  (place " << i + 1 << " at Light is nearest place " << at + 1 << " Off: " << std::sqrt(best) << " vs own " << std::sqrt(mine) << ")\n";
            worst = std::min(worst, std::sqrt(mine));
        }
        s.check(own == int(light.size()) && !light.empty(), "each place keeps its own grade under the tape (its Light picture is closest in colour layout to its own Off picture)",
                std::to_string(own) + " of " + std::to_string(light.size()));
        s.check(sharpLight < 0.8 * sharpOff && sharpHeavy < sharpLight, "the scenes are softer with the tape (Light, then Heavy)",
                "neighbour difference Off " + num(sharpOff / 10.0, 2) + ", Light " + num(sharpLight / 10.0, 2) + ", Heavy " + num(sharpHeavy / 10.0, 2));
        ui::PhotoScene hospital(0);
        hospital.ensureLoaded();
        auto look = hospital.defaultLook();
        double ms[2] {};
        for (int t = 0; t < 2; ++t) {
            look.vhs = { t == 0 ? 0.0f : 0.55f, 0.0f, 0.0f };
            (void) hospital.renderNow(look);
            const double t0 = frames::threadCpuMs();
            for (int k = 0; k < 8; ++k) {
                look.levels[1] = 0.5f + 0.05f * float(k);
                (void) hospital.renderNow(look);
            }
            ms[t] = (frames::threadCpuMs() - t0) / 8.0;
        }
        s.metric("hospital composite, one light changed: without / with the tape look (CPU)", ms[0], (" / " + num(ms[1], 1) + " ms").c_str());
        s.check(ms[1] < 2.0 * ms[0] + 5.0, "the tape look at most about doubles a composite (it runs at half size)", num(ms[1], 1) + " vs " + num(ms[0], 1) + " ms");
    }
    // --- The glitch schedule, simulated: 10 minutes of 30 Hz editor ticks per case.
    {
        struct Case { const char* name; float amount, wrong; };
        const Case cases[] { { "Light", 0.55f, 0.0f }, { "Heavy", 1.0f, 0.0f }, { "Light, Unreal 100 % / Something Wrong", 0.55f, 1.0f }, { "Heavy, Room 404", 1.0f, 0.9f } };
        int rollsOf[4] {}, tearsOf[4] {};
        for (int c = 0; c < 4; ++c) {
            ui::vhs::Tape tape(unsigned(11 + c));
            const double dt = 1.0 / 30.0, seconds = 600.0;
            std::vector<double> rollStarts, tearStarts;
            double rollTime = 0, maxDriftStep = 0, lastDrift = 0, minDrift = 0, maxDrift = 0;
            int frames = 0, dropouts = 0, tearTicks = 0, longestTear = 0;
            bool rolling = false, tearing = false;
            for (int i = 0; i < int(seconds / dt); ++i) {
                frames += tape.advance(dt, cases[c].amount, cases[c].wrong, 24.0, false) ? 1 : 0;
                if (tape.trackStrength > 0.0f && !rolling) rollStarts.push_back(tape.time());
                rolling = tape.trackStrength > 0.0f;
                rollTime += rolling ? dt : 0.0;
                if (tape.tearY >= 0.0f && !tearing) {
                    tearStarts.push_back(tape.time());
                    tearTicks = 0;
                }
                tearing = tape.tearY >= 0.0f;
                if (tearing) longestTear = std::max(longestTear, ++tearTicks);
                dropouts += tape.dropoutCount;
                if (i > 0) maxDriftStep = std::max(maxDriftStep, std::abs(double(tape.drift) - lastDrift) / dt);
                lastDrift = tape.drift;
                minDrift = std::min(minDrift, lastDrift);
                maxDrift = std::max(maxDrift, lastDrift);
            }
            const auto minGap = [](const std::vector<double>& t) {
                double g = 1e9;
                for (std::size_t i = 1; i < t.size(); ++i) g = std::min(g, t[i] - t[i - 1]);
                return g;
            };
            double mean = 0, var = 0;
            for (std::size_t i = 1; i < rollStarts.size(); ++i) mean += rollStarts[i] - rollStarts[i - 1];
            mean /= double(std::max<std::size_t>(1, rollStarts.size() - 1));
            for (std::size_t i = 1; i < rollStarts.size(); ++i) var += std::pow(rollStarts[i] - rollStarts[i - 1] - mean, 2.0);
            const double cv = std::sqrt(var / double(std::max<std::size_t>(1, rollStarts.size() - 1))) / std::max(1e-9, mean);
            rollsOf[c] = int(rollStarts.size());
            tearsOf[c] = int(tearStarts.size());
            const std::string n = cases[c].name;
            s.metric(n + ": tracking rolls / tears in 10 min", double(rollStarts.size()), (" / " + std::to_string(tearStarts.size()) + ", rolling " + num(rollTime / seconds * 100.0, 1) +
                                                                                           " % of the time, mean gap " + num(mean, 1) + " s, irregularity (CV) " + num(cv, 2)).c_str());
            s.check(minGap(rollStarts) >= 2.5 && rollTime / seconds < (cases[c].wrong > 0.5f ? 0.45 : 0.15), n + ": tracking rolls are sparse (never within 2.5 s; rolling only part of the time)",
                    "closest " + num(minGap(rollStarts), 1) + " s, " + num(rollTime / seconds * 100.0, 1) + " %");
            s.check(rollStarts.size() >= 3 && cv > 0.25, n + ": at irregular moments (gaps vary: no rhythm)", "CV " + num(cv, 2));
            s.check(cases[c].wrong > 0.05f ? (tearStarts.size() >= 3 && minGap(tearStarts) >= 3.0 && longestTear * dt <= 0.21) : tearStarts.empty(),
                    n + (cases[c].wrong > 0.05f ? ": brief frame tears (at most ~0.2 s), never within 3 s" : ": no frame tears unless something is wrong"),
                    std::to_string(tearStarts.size()) + " tears, closest " + num(minGap(tearStarts), 1) + " s, longest " + num(longestTear * dt, 2) + " s");
            s.check(double(frames) / seconds <= 24.5, n + ": the animated layers move at most 24 times a second", num(double(frames) / seconds, 1) + " fps");
            s.check(double(dropouts) / seconds < (cases[c].amount > 0.9f ? 11.0 : 5.0), n + ": dropouts are occasional (single-frame streaks)", num(double(dropouts) / seconds, 1) + " a second");
            s.check(maxDriftStep < (cases[c].wrong > 0.5f ? 0.45 : 0.25) && minDrift < -0.1 && maxDrift > 0.1, n + ": the green / magenta cast drifts slowly both ways (a little faster when wrong)",
                    "fastest " + num(maxDriftStep, 3) + " per second, range " + num(minDrift, 2) + " .. " + num(maxDrift, 2));
        }
        s.check(rollsOf[1] > rollsOf[0] && rollsOf[2] > 1.8 * rollsOf[0] && rollsOf[3] > 1.6 * rollsOf[1], "Heavy rolls more than Light; Unreal and wrong Characters roll far more",
                std::to_string(rollsOf[0]) + " / " + std::to_string(rollsOf[1]) + " / " + std::to_string(rollsOf[2]) + " / " + std::to_string(rollsOf[3]));
        s.check(ui::vhs::wrongness(0, 2) > 0.9f && ui::vhs::wrongness(5, 2) > 0.8f && ui::vhs::wrongness(9, 2) > 0.9f && ui::vhs::wrongness(4, 0) == 0.0f && ui::vhs::wrongOf(1.0f, 0.0f) == 1.0f,
                "Something Wrong, Room 404, Beyond Reality are wrong; Still Water is not; Unreal 100 % is");
        ui::vhs::Tape slow;
        int f12 = 0;
        for (int i = 0; i < 300; ++i) f12 += slow.advance(1.0 / 30.0, 1.0f, 0.0f, 12.0, false) ? 1 : 0;
        s.check(f12 <= 125, "slow frames / Eco: the tape drops to 12 frames a second", std::to_string(f12) + " in 10 s");
    }
    // --- The editor: Off animates nothing; Light repaints the viewfinder at most 24 times a
    // second; a place switch with the tape running shows only whole pictures.
    {
        LiminalProcessor p;
        p.prepareToPlay(48000.0, 512);
        const auto set = [&](const char* id, float value) {
            auto* param = p.parameters.getParameter(id);
            param->setValueNotifyingHost(param->convertTo0to1(value));
        };
        set(params::activity, 0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
        auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
        if (lm == nullptr) return;
        auto& view = lm->view();
        juce::Image screen(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        const auto run = [&](int ticks, int& sceneRepaints, double& area) {
            sceneRepaints = 0;
            area = 0;
            double next = juce::Time::getMillisecondCounterHiRes();
            for (int k = 0; k < ticks; ++k) {
                view.tick();
                for (const auto& r : view.lastRepaint()) {   // painted as a host would (the frame governor waits for it)
                    juce::Graphics g(screen);
                    g.reduceClipRegion(r);
                    editor->paintEntireComponent(g, true);
                }
                for (const auto& r : view.lastRepaint()) area += double(r.getWidth() * r.getHeight());
                sceneRepaints += view.lastRepaint().getBounds().contains(view.sceneArea()) ? 1 : 0;
                next += 1000.0 / 30.0;
                const double wait = next - juce::Time::getMillisecondCounterHiRes();
                if (wait > 0) std::this_thread::sleep_for(std::chrono::microseconds(juce::int64(wait * 1000.0)));
            }
            area /= double(ticks) * double(editor->getWidth() * editor->getHeight());
        };
        int repaints = 0;
        double area = 0;
        set(params::vhs, 0.0f);
        for (int k = 0; k < 3; ++k) view.animate();
        run(60, repaints, area);
        s.check(repaints == 0 && area < 0.05, "VHS Off: a still scene repaints only the clock, REC and meters (as before)", std::to_string(repaints) + " whole-viewfinder repaints, " + num(area * 100.0, 2) + " % a tick");
        set(params::vhs, 1.0f);
        run(90, repaints, area);
        s.check(repaints >= 50 && repaints <= 75, "VHS Light: the tape's layers repaint the viewfinder at most 24 times a second", std::to_string(repaints) + " in 3 s");
        s.check(view.paintedPicture().getWidth() <= 720, "the picture on screen has the tape look baked in (half-size composite)", std::to_string(view.paintedPicture().getWidth()) + " px wide");
        // Ten seconds idle at Light: the static half is not recomposited every frame (only the
        // colour cast's small steps and nothing else move it).
        const int before = view.photoComposites();
        run(150, repaints, area);
        const int composites = view.photoComposites() - before;
        s.check(composites <= 10, "a still scene with the tape look recomposites only as the colour cast drifts (not per frame)", std::to_string(composites) + " composites in 5 s");
        // A place switch with the tape running: every frame shows whole pictures (no blank frame).
        const auto scene = view.sceneArea();
        int dark = 0;
        set(params::environment, 4.0f);
        for (int k = 0; k < 30; ++k) {
            view.tick();
            {
                juce::Graphics g(screen);
                editor->paintEntireComponent(g, true);
            }
            const auto shot = editor->createComponentSnapshot(scene, true, 1.0f);
            const auto m = vhstest::meanColour(shot);
            dark += (m[0] + m[1] + m[2]) / 3.0 < 25.0 ? 1 : 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        s.check(dark == 0 && view.shownPlace() == 4, "with the tape running, a place switch never shows a blank frame", std::to_string(dark) + " dark frames");
        p.editorBeingDeleted(editor.get());
    }
    // --- The audio warble (off by default): nothing at all when off; faint and smooth when on.
    {
        const double rate = 48000.0;
        const auto sine = [&](std::vector<float>& l, std::vector<float>& r, int n, double& phase) {
            l.resize(std::size_t(n));
            r.resize(std::size_t(n));
            for (int i = 0; i < n; ++i) {
                l[std::size_t(i)] = r[std::size_t(i)] = float(0.5 * std::sin(phase));
                phase += 2.0 * 3.141592653589793 * 1000.0 / rate;
            }
        };
        // Pitch deviation (cents RMS) of a 1 kHz sine from zero crossings.
        const auto cents = [&](const std::vector<float>& x) {
            std::vector<double> crossings;
            for (std::size_t i = 1; i < x.size(); ++i)
                if (x[i - 1] < 0 && x[i] >= 0) crossings.push_back(double(i - 1) + double(-x[i - 1] / (x[i] - x[i - 1])));
            double sum = 0;
            int n = 0;
            for (std::size_t i = 1; i < crossings.size(); ++i) {
                const double f = rate / (crossings[i] - crossings[i - 1]);
                sum += std::pow(1200.0 * std::log2(f / 1000.0), 2.0);
                ++n;
            }
            return std::sqrt(sum / std::max(1, n));
        };
        double results[3] {};
        double jump = 0;
        for (int mode = 0; mode < 3; ++mode) {   // off, on and steady, on and glitching
            liminal::dsp::TapeWarble warble;
            warble.prepare(rate);
            double phase = 0;
            std::vector<float> l, r, all;
            for (int block = 0; block < 200; ++block) {
                sine(l, r, 480, phase);
                warble.process(l.data(), r.data(), 480, mode > 0, mode == 2 ? 1.0f : 0.0f);
                if (block > 40) all.insert(all.end(), l.begin(), l.end());
                if (mode == 1 && block > 0) jump = std::max(jump, double(std::abs(l[0] - all.empty() ? 0.0f : 0.0f)));
            }
            results[mode] = cents(all);
        }
        s.check(results[0] < 0.05 && results[1] < 0.5, "the warble does nothing when off, and nothing audible when on but the tape is steady", num(results[0], 3) + " / " + num(results[1], 3) + " cents");
        s.check(results[2] > 3.0 && results[2] < 40.0, "during a heavy glitch the sound warbles faintly", num(results[2], 1) + " cents RMS");
        // Switching it on mid-note: no click (the largest sample step stays near a sine's own).
        liminal::dsp::TapeWarble warble;
        warble.prepare(rate);
        double phase = 0, worst = 0, ownStep = 2.0 * 3.141592653589793 * 1000.0 / rate * 0.5;
        float last = 0;
        std::vector<float> l, r;
        for (int block = 0; block < 100; ++block) {
            sine(l, r, 480, phase);
            warble.process(l.data(), r.data(), 480, block >= 30 && block < 70, block >= 40 && block < 60 ? 1.0f : 0.0f);
            for (const float v : l) {
                worst = std::max(worst, double(std::abs(v - last)));
                last = v;
            }
        }
        s.check(worst < 1.6 * ownStep, "switching the warble on and off is click-free", num(worst / ownStep, 2) + " x a sine's own step");
        // Off by default in the plug-in: the output is bit-identical whatever the editor publishes.
        Rig a, b;
        b.p->vhsWarble.store(1.0f);
        const auto x = a.render(1.0, { { 0.05, on(60, 100) } }), y = b.render(1.0, { { 0.05, on(60, 100) } });
        bool same = x.first.size() == y.first.size();
        for (std::size_t i = 0; same && i < x.first.size(); ++i) same = x.first[i] == y.first[i] && x.second[i] == y.second[i];
        s.check(same, "with the warble off (default) the sound is untouched by the picture's glitches");
    }
}
