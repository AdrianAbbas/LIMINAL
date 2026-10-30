#include "UI/PhotoScene.h"
#include "UI/SceneArt.h"
#if LIMINAL_HAS_SCENES
#include "SceneData.h"
#endif
#include <array>
#include <cmath>

namespace liminal::ui {

namespace {
constexpr int toneSize = 4096;
constexpr float toneScale = 512.0f;   // linear value x 512 = the tone table's index (0..8)

// sRGB byte -> linear light.
const std::array<float, 256>& linearOf() {
    static const auto table = [] {
        std::array<float, 256> t {};
        for (int i = 0; i < 256; ++i) {
            const double x = i / 255.0;
            t[std::size_t(i)] = float(x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4));
        }
        return t;
    }();
    return table;
}

// The film curve (the same as tools/scenes/preview.py): a linear toe, a soft shoulder, then sRGB.
const std::array<float, toneSize>& toneOf() {
    static const auto table = [] {
        std::array<float, toneSize> t {};
        for (int i = 0; i < toneSize; ++i) {
            const double x = i / double(toneScale);
            double y = std::clamp(x / (1.0 + x * 0.45) * 1.12, 0.0, 1.0);
            y = y <= 0.0031308 ? 12.92 * y : 1.055 * std::pow(y, 1.0 / 2.4) - 0.055;
            t[std::size_t(i)] = float(y * 255.0);
        }
        return t;
    }();
    return table;
}

struct Source {   // where a scene's files come from
    juce::File folder;   // the developer's own renders, if present
    juce::String name;
    juce::MemoryBlock read(const juce::String& file) const {
        juce::MemoryBlock block;
        if (folder.isDirectory()) {
            folder.getChildFile(file).loadFileAsData(block);
            return block;
        }
#if LIMINAL_HAS_SCENES
        for (int i = 0; i < SceneData::namedResourceListSize; ++i)
            if (file == SceneData::originalFilenames[i]) {
                int size = 0;
                if (const char* data = SceneData::getNamedResource(SceneData::namedResourceList[i], size)) block.append(data, std::size_t(size));
                break;
            }
#endif
        return block;
    }
};

Source sourceFor(const char* name) {
    Source s;
    s.name = name;
    const auto own = photographFolder().getChildFile(name);
    if (own.getChildFile(juce::String(name) + "_manifest.json").existsAsFile()) s.folder = own;
    return s;
}
} // namespace

class PhotoScene::Worker : public juce::Thread {
public:
    explicit Worker(PhotoScene& s) : juce::Thread("LIMINAL scene"), scene(s) { startThread(juce::Thread::Priority::low); }
    ~Worker() override { stopThread(4000); }
    void run() override {
        if (!scene.load()) return;
        while (!threadShouldExit()) {
            Look look;
            {
                std::lock_guard<std::mutex> lock(scene.handoff);
                if (scene.pending) {
                    look = scene.wanted;
                    scene.pending = false;
                }
            }
            if (look.levels.empty()) {
                wait(100);
                continue;
            }
            juce::Image image(juce::Image::RGB, scene.width, scene.height, false);
            scene.composite(look, image);
            std::lock_guard<std::mutex> lock(scene.handoff);
            scene.finished = image;
            scene.fresh = true;
        }
    }

private:
    PhotoScene& scene;
};

const char* PhotoScene::sceneName(int place) {
    switch (place) {
    case 0: return "hospital";
    case 4: return "pool";
    case 8: return "office";
    default: return nullptr;
    }
}

bool PhotoScene::available(int place) {
    const char* name = sceneName(place);
    if (name == nullptr) return false;
    // The developer's photograph of the place (Milestone 4's drop-in) still wins over renders.
    if (photographFolder().getChildFile(juce::String(place + 1) + ".png").existsAsFile()) return false;
    return sourceFor(name).read(juce::String(name) + "_manifest.json").getSize() > 0;
}

juce::Image PhotoScene::thumbnail(int place) {
    if (!available(place)) return {};
    const auto block = sourceFor(sceneName(place)).read(juce::String(sceneName(place)) + "_thumb.jpg");
    return juce::ImageFileFormat::loadFrom(block.getData(), block.getSize());
}

PhotoScene::PhotoScene(int place) : placeIndex(place) { worker = std::make_unique<Worker>(*this); }

PhotoScene::~PhotoScene() { worker.reset(); }

bool PhotoScene::load() {
    std::lock_guard<std::mutex> lock(workLock);
    if (ready.load()) return true;
    const char* name = sceneName(placeIndex);
    if (name == nullptr) {
        broken = true;
        return false;
    }
    const auto source = sourceFor(name);
    const auto manifestData = source.read(juce::String(name) + "_manifest.json");
    const auto manifest = juce::JSON::parse(manifestData.toString());
    width = int(manifest["width"]);
    height = int(manifest["height"]);
    headroom = float(double(manifest.getProperty("headroom", 1.0)));
    const auto* list = manifest["passes"].getArray();
    if (width <= 0 || height <= 0 || list == nullptr || list->isEmpty()) {
        broken = true;
        return false;
    }
    const auto decode = [&](const juce::String& file) {
        const auto block = source.read(file);
        return juce::ImageFileFormat::loadFrom(block.getData(), block.getSize());
    };
    for (const auto& p : *list) {
        Pass pass;
        pass.name = p["name"].toString();
        pass.kind = p["kind"].toString();
        pass.defaultLevel = float(double(p.getProperty("default", 1.0)));
        if (const auto* a = p["anchor"].getArray(); a != nullptr && a->size() == 2) pass.anchor = { float(double((*a)[0])), float(double((*a)[1])) };
        auto image = decode(juce::String(name) + "_" + pass.name + ".jpg");
        if (!image.isValid()) continue;
        if (image.getWidth() != width || image.getHeight() != height) image = image.rescaled(width, height);
        std::vector<juce::uint8> rgb(std::size_t(width) * std::size_t(height) * 3);
        const juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto c = data.getPixelColour(x, y);
                auto* o = rgb.data() + (std::size_t(y) * std::size_t(width) + std::size_t(x)) * 3;
                o[0] = c.getRed();
                o[1] = c.getGreen();
                o[2] = c.getBlue();
            }
        passList.push_back(pass);
        pixels.push_back(std::move(rgb));
    }
    if (pixels.empty()) {
        broken = true;
        return false;
    }
    // The caustic loop: luminance frames at their own (smaller) size, in a fixed tint.
    if (const auto* frames = manifest["caustics"]["frames"].getArray()) {
        causticRate = float(double(manifest["caustics"].getProperty("fps", 5.0)));
        causticDefault = float(double(manifest["caustics"].getProperty("default", 1.0)));
        double r = 0, g = 0, b = 0;
        for (const auto& f : *frames) {
            const auto image = decode(f.toString());
            if (!image.isValid()) continue;
            cw = image.getWidth();
            ch = image.getHeight();
            std::vector<juce::uint8> lum(std::size_t(cw) * std::size_t(ch));
            const juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < ch; ++y)
                for (int x = 0; x < cw; ++x) {
                    const auto c = data.getPixelColour(x, y);
                    lum[std::size_t(y) * std::size_t(cw) + std::size_t(x)] = std::max({ c.getRed(), c.getGreen(), c.getBlue() });
                    if (caustics.empty()) { r += c.getRed(); g += c.getGreen(); b += c.getBlue(); }
                }
            caustics.push_back(std::move(lum));
        }
        const double m = std::max({ r, g, b, 1.0 });
        causticTint = juce::Colour::fromFloatRGBA(float(r / m), float(g / m), float(b / m), 1.0f);
        causticX.resize(std::size_t(width));
        causticY.resize(std::size_t(height));
        for (int x = 0; x < width; ++x) causticX[std::size_t(x)] = std::min(cw - 1, x * cw / width);
        for (int y = 0; y < height; ++y) causticY[std::size_t(y)] = std::min(ch - 1, y * ch / height);
    }
    // A soft photographic vignette (applied in linear light, before the curve).
    vignette.resize(std::size_t(width) * std::size_t(height));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const float dx = (float(x) / float(width) - 0.5f) * 2.0f, dy = (float(y) / float(height) - 0.48f) * 2.0f;
            const float d = std::sqrt(dx * dx * 0.8f + dy * dy * 0.9f);
            vignette[std::size_t(y) * std::size_t(width) + std::size_t(x)] = juce::uint8(std::clamp(255.0f * (1.0f - 0.55f * std::pow(std::max(0.0f, d - 0.35f), 1.6f)), 60.0f, 255.0f));
        }
    sum.assign(std::size_t(width) * std::size_t(height) * 3, 0.0f);
    levelsNow.assign(pixels.size(), 0.0f);
    ready = true;
    return true;
}

void PhotoScene::addPass(int index, float delta) {
    const auto& lin = linearOf();
    const auto& src = pixels[std::size_t(index)];
    float* s = sum.data();
    const std::size_t n = sum.size();
    for (std::size_t i = 0; i < n; ++i) s[i] += delta * lin[src[i]];
}

PhotoScene::Look PhotoScene::defaultLook() const {
    Look look;
    for (const auto& p : passList) look.levels.push_back(p.defaultLevel);
    look.causticLevel = causticDefault;
    return look;
}

void PhotoScene::request(const Look& look) {
    {
        std::lock_guard<std::mutex> lock(handoff);
        wanted = look;
        pending = true;
    }
    if (worker != nullptr) worker->notify();
}

bool PhotoScene::take(juce::Image& out) {
    std::lock_guard<std::mutex> lock(handoff);
    if (!fresh) return false;
    out = finished;
    fresh = false;
    return true;
}

juce::Image PhotoScene::renderNow(const Look& look) {
    if (!load()) return {};
    juce::Image image(juce::Image::RGB, width, height, false);
    composite(look.levels.empty() ? defaultLook() : look, image);
    return image;
}

void PhotoScene::composite(const Look& look, juce::Image& into) {
    std::lock_guard<std::mutex> lock(workLock);
    // 1. The running sum follows the levels: only the passes whose level moved are added in.
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const float want = i < look.levels.size() ? std::max(0.0f, look.levels[i]) : passList[i].defaultLevel;
        const float delta = want - levelsNow[i];
        if (std::abs(delta) > 1.0e-4f) {
            addPass(int(i), delta);
            levelsNow[i] = want;
        }
    }
    // 2. The finish, as three per-channel tables: the curve, the Character's light and grade,
    // the faded wash of the second Character.
    const auto& tone = toneOf();
    const juce::Colour grade(look.grade);
    const float alpha = grade.getFloatAlpha();
    const float fade = look.character == 1 ? 0.16f : 0.0f;
    const juce::Colour wash(0xff8a8478);
    std::array<std::array<juce::uint8, toneSize>, 3> table {};
    const float rgbGrade[3] { float(grade.getRed()), float(grade.getGreen()), float(grade.getBlue()) };
    const float rgbWash[3] { float(wash.getRed()), float(wash.getGreen()), float(wash.getBlue()) };
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < toneSize; ++i) {
            float v = tone[std::size_t(std::min(toneSize - 1, int(float(i) * look.light)))];
            v = v * (1.0f - alpha) + rgbGrade[c] * alpha;
            v = v * (1.0f - fade) + rgbWash[c] * fade;
            table[std::size_t(c)][std::size_t(i)] = juce::uint8(std::clamp(v + 0.5f, 0.0f, 255.0f));
        }
    // 3. Per pixel: the sum, the caustics, the vignette, the tables, a little grain.
    const auto& lin = linearOf();
    const bool caustic = !caustics.empty() && look.causticLevel > 0.001f;
    int fa = 0, fb = 0;
    float blend = 0;
    if (caustic) {
        const float pos = (look.causticPhase - std::floor(look.causticPhase)) * float(caustics.size());
        fa = int(pos) % int(caustics.size());
        fb = (fa + 1) % int(caustics.size());
        blend = pos - std::floor(pos);
    }
    const float tint[3] { causticTint.getFloatRed() * look.causticLevel, causticTint.getFloatGreen() * look.causticLevel, causticTint.getFloatBlue() * look.causticLevel };
    const float gain = toneScale / std::max(0.05f, headroom);
    const bool hazy = look.character == 2;
    juce::Image::BitmapData out(into, juce::Image::BitmapData::writeOnly);
    std::uint32_t noise = 0x9E3779B9u ^ (look.grainSeed * 2654435761u);
    for (int y = 0; y < height; ++y) {
        auto* row = out.getLinePointer(y);
        const float* s = sum.data() + std::size_t(y) * std::size_t(width) * 3;
        const juce::uint8* vig = vignette.data() + std::size_t(y) * std::size_t(width);
        const juce::uint8* ca = caustic ? caustics[std::size_t(fa)].data() + std::size_t(causticY[std::size_t(y)]) * std::size_t(cw) : nullptr;
        const juce::uint8* cb = caustic ? caustics[std::size_t(fb)].data() + std::size_t(causticY[std::size_t(y)]) * std::size_t(cw) : nullptr;
        for (int x = 0; x < width; ++x, s += 3) {
            float v = float(vig[x]) * (1.0f / 255.0f);
            if (hazy) v *= v;
            const float k = gain * v;
            float extra = 0;
            if (caustic) {
                const int cx = causticX[std::size_t(x)];
                extra = lin[ca[cx]] + blend * (lin[cb[cx]] - lin[ca[cx]]);
            }
            noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
            const int grain = int(noise & 7u) - 3;
            auto* p = row + x * out.pixelStride;
            const int r = std::min(toneSize - 1, int((s[0] + extra * tint[0]) * k));
            const int g = std::min(toneSize - 1, int((s[1] + extra * tint[1]) * k));
            const int b = std::min(toneSize - 1, int((s[2] + extra * tint[2]) * k));
            p[juce::PixelRGB::indexR] = juce::uint8(std::clamp(int(table[0][std::size_t(std::max(0, r))]) + grain, 0, 255));
            p[juce::PixelRGB::indexG] = juce::uint8(std::clamp(int(table[1][std::size_t(std::max(0, g))]) + grain, 0, 255));
            p[juce::PixelRGB::indexB] = juce::uint8(std::clamp(int(table[2][std::size_t(std::max(0, b))]) + grain, 0, 255));
        }
    }
}

} // namespace liminal::ui
