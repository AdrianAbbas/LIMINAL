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
        for (auto& b : scene.buffers) b = scene.makeImage();   // once per scene, never per frame
        double last = -1.0e9;
        while (!threadShouldExit()) {
            // At most 30 composites a second, however often the editor asks.
            const double remaining = last + PhotoScene::minFrameMs - juce::Time::getMillisecondCounterHiRes();
            if (remaining > 0.5) {
                this->wait(int(std::ceil(remaining)));
                continue;
            }
            Look look;
            {
                std::lock_guard<std::mutex> lock(scene.handoff);
                if (scene.pending) {
                    look = scene.wanted;
                    scene.pending = false;
                }
            }
            if (look.levels.empty()) {
                this->wait(100);
                continue;
            }
            last = juce::Time::getMillisecondCounterHiRes();
            scene.composite(look, scene.buffers[std::size_t(scene.back)]);   // the back image: nobody else touches it
            {
                std::lock_guard<std::mutex> lock(scene.handoff);
                std::swap(scene.back, scene.readyIndex);
                scene.fresh = true;
            }
            ++scene.compositeCount;
            scene.compositeTime.store(scene.compositeTime.load() + juce::Time::getMillisecondCounterHiRes() - last);
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
        const bool argb = data.pixelFormat == juce::Image::ARGB;   // (a decoded JPEG is opaque: no unpremultiplying)
        const bool grey = data.pixelFormat == juce::Image::SingleChannel;
        const int iR = grey ? 0 : argb ? juce::PixelARGB::indexR : juce::PixelRGB::indexR, iG = grey ? 0 : argb ? juce::PixelARGB::indexG : juce::PixelRGB::indexG,
                  iB = grey ? 0 : argb ? juce::PixelARGB::indexB : juce::PixelRGB::indexB;
        for (int y = 0; y < height; ++y) {
            const juce::uint8* in = data.getLinePointer(y);
            auto* o = rgb.data() + std::size_t(y) * std::size_t(width) * 3;
            for (int x = 0; x < width; ++x, in += data.pixelStride, o += 3) {
                o[0] = in[iR];
                o[1] = in[iG];
                o[2] = in[iB];
            }
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

void PhotoScene::addPasses(const std::vector<std::pair<int, float>>& moved) {
    const auto& lin = linearOf();
    float* s = sum.data();
    const std::size_t n = sum.size();
    std::size_t k = 0;
    for (; k + 1 < moved.size(); k += 2) {   // two passes per sweep
        const juce::uint8* a = pixels[std::size_t(moved[k].first)].data();
        const juce::uint8* b = pixels[std::size_t(moved[k + 1].first)].data();
        const float da = moved[k].second, dbl = moved[k + 1].second;
        for (std::size_t i = 0; i < n; ++i) s[i] += da * lin[a[i]] + dbl * lin[b[i]];
    }
    if (k < moved.size()) {
        const juce::uint8* a = pixels[std::size_t(moved[k].first)].data();
        const float da = moved[k].second;
        for (std::size_t i = 0; i < n; ++i) s[i] += da * lin[a[i]];
    }
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
    std::swap(front, readyIndex);   // the old front goes back into the ring; the caller drops it now
    out = buffers[std::size_t(front)];
    fresh = false;
    return true;
}

juce::Image PhotoScene::makeImage() const {
    juce::Image image(juce::Image::ARGB, width, height, false);
    image.clear(image.getBounds(), juce::Colours::black);   // opaque from the start
    return image;
}

juce::Image PhotoScene::renderNow(const Look& look) {
    if (!load()) return {};
    auto image = makeImage();
    composite(look.levels.empty() ? defaultLook() : look, image);
    return image;
}

void PhotoScene::composite(const Look& look, juce::Image& into) {
    std::lock_guard<std::mutex> lock(workLock);
    // 1. The running sum follows the levels: only the passes whose level moved are added in, all
    // of them in one sweep over the sum (it is memory that costs here, not arithmetic).
    std::vector<std::pair<int, float>> moved;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const float want = i < look.levels.size() ? std::max(0.0f, look.levels[i]) : passList[i].defaultLevel;
        const float delta = want - levelsNow[i];
        if (std::abs(delta) > 1.0e-4f) {
            moved.emplace_back(int(i), delta);
            levelsNow[i] = want;
        }
    }
    addPasses(moved);
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
    // Byte order of the destination: ARGB (always, from makeImage) writes alpha 255 too.
    const bool argb = out.pixelFormat == juce::Image::ARGB;
    const int iR = argb ? juce::PixelARGB::indexR : juce::PixelRGB::indexR, iG = argb ? juce::PixelARGB::indexG : juce::PixelRGB::indexG,
              iB = argb ? juce::PixelARGB::indexB : juce::PixelRGB::indexB;
    if (grain.empty() || grainFor != look.grainSeed) {   // the film grain is the same every frame: made once
        grain.resize(std::size_t(width) * std::size_t(height));
        std::uint32_t noise = 0x9E3779B9u ^ (look.grainSeed * 2654435761u);
        for (auto& g : grain) {
            noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
            g = juce::int8(int(noise & 7u) - 3);
        }
        grainFor = look.grainSeed;
    }
    // Per row, in three simple loops: the exposure per pixel, the tone-table index per channel
    // (plain float arithmetic the compiler vectorises), then the tables, the grain and the bytes.
    const auto w = static_cast<std::size_t>(width);
    std::vector<float> exposure(w), extra(w, 0.0f);
    std::vector<int> index(w * 3);
    const float top = float(toneSize - 1);
    for (int y = 0; y < height; ++y) {
        const float* s = sum.data() + std::size_t(y) * std::size_t(width) * 3;
        const juce::uint8* vig = vignette.data() + std::size_t(y) * std::size_t(width);
        for (int x = 0; x < width; ++x) {
            float v = float(vig[x]) * (1.0f / 255.0f);
            if (hazy) v *= v;
            exposure[std::size_t(x)] = gain * v;
        }
        if (caustic) {
            const juce::uint8* ca = caustics[std::size_t(fa)].data() + std::size_t(causticY[std::size_t(y)]) * std::size_t(cw);
            const juce::uint8* cb = caustics[std::size_t(fb)].data() + std::size_t(causticY[std::size_t(y)]) * std::size_t(cw);
            for (int x = 0; x < width; ++x) {
                const int cx = causticX[std::size_t(x)];
                extra[std::size_t(x)] = lin[ca[cx]] + blend * (lin[cb[cx]] - lin[ca[cx]]);
            }
        }
        int* idx = index.data();
        for (int x = 0; x < width; ++x)
            for (int c = 0; c < 3; ++c) {
                const float value = (s[x * 3 + c] + extra[std::size_t(x)] * tint[c]) * exposure[std::size_t(x)];
                idx[x * 3 + c] = int(std::clamp(value, 0.0f, top));
            }
        auto* row = out.getLinePointer(y);
        const juce::int8* grainRow = grain.data() + std::size_t(y) * w;
        for (int x = 0; x < width; ++x) {
            const int n = grainRow[x];
            auto* p = row + x * out.pixelStride;
            p[iR] = juce::uint8(std::clamp(int(table[0][std::size_t(idx[x * 3])]) + n, 0, 255));
            p[iG] = juce::uint8(std::clamp(int(table[1][std::size_t(idx[x * 3 + 1])]) + n, 0, 255));
            p[iB] = juce::uint8(std::clamp(int(table[2][std::size_t(idx[x * 3 + 2])]) + n, 0, 255));
            if (argb) p[juce::PixelARGB::indexA] = 255;
        }
    }
}

} // namespace liminal::ui
