#include "UI/SceneArt.h"
#include <algorithm>
#include <cmath>

namespace liminal::ui {

namespace {

// Corridor in metres: 2.4 m wide, 3 m high, 17 m long; camera at eye height 1.6 m.
constexpr float halfWidth = 1.2f, height = 3.0f, eye = 1.6f, length = 17.0f, nearZ = 0.55f;

struct Camera {
    float vx, vy, focal;
    juce::Point<float> at(float x, float y, float z) const {
        const float d = z + nearZ;
        return { vx + focal * x / d, vy - focal * (y - eye) / d };
    }
};

juce::Colour mix(juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith(b, std::clamp(t, 0.0f, 1.0f)); }

// Fog: distant surfaces fade into a dim, greenish haze.
const juce::Colour fogColour(0xff1d2a26);
float fogAt(float z) { return 1.0f - std::exp(-z / 9.0f); }

// Light reaching a surface at depth z from the ceiling panels (one every 3.4 m).
constexpr float lightSpacing = 3.4f, firstLight = 1.6f;
float lightAt(float z) {
    float sum = 0;
    for (float lz = firstLight; lz < length; lz += lightSpacing) {
        const float d = (z - lz) / 1.7f;
        sum += std::exp(-d * d);
    }
    return std::min(1.0f, 0.35f + 0.65f * sum);
}

void quad(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d) {
    juce::Path p;
    p.startNewSubPath(a);
    p.lineTo(b);
    p.lineTo(c);
    p.lineTo(d);
    p.closeSubPath();
    g.fillPath(p);
}

void line(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float thickness) {
    g.drawLine({ a, b }, thickness);
}

} // namespace

Scene renderCorridor(int width, int height_, float scale, unsigned seed) {
    Scene scene;
    const int pw = std::max(1, juce::roundToInt(float(width) * scale)), ph = std::max(1, juce::roundToInt(float(height_) * scale));
    scene.image = juce::Image(juce::Image::RGB, pw, ph, true);
    juce::Random random { juce::int64(seed) };
    const float w = float(width), h = float(height_);
    const Camera cam { w * 0.5f, h * 0.47f, w * 0.27f };
    {
        juce::Graphics g(scene.image);
        g.addTransform(juce::AffineTransform::scale(scale));
        g.fillAll(fogColour);

        // Surfaces in depth slices, far to near, each lit by the nearest panels and fogged.
        const juce::Colour ceiling(0xff8f9d94), wall(0xff7f9a8c), wallLow(0xff4d6b60), floorC(0xff5b6a62);
        const int slices = 90;
        for (int i = slices - 1; i >= 0; --i) {
            const float z0 = length * std::pow(float(i) / slices, 1.6f), z1 = length * std::pow(float(i + 1) / slices, 1.6f);
            const float zm = 0.5f * (z0 + z1), light = lightAt(zm), fog = fogAt(zm);
            auto shade = [&](juce::Colour c, float extra) { return mix(c.withMultipliedBrightness(light * extra), fogColour, fog); };
            g.setColour(shade(ceiling, 0.8f));
            quad(g, cam.at(-halfWidth, height, z0), cam.at(halfWidth, height, z0), cam.at(halfWidth, height, z1), cam.at(-halfWidth, height, z1));
            g.setColour(shade(floorC, 0.9f));
            quad(g, cam.at(-halfWidth, 0, z0), cam.at(halfWidth, 0, z0), cam.at(halfWidth, 0, z1), cam.at(-halfWidth, 0, z1));
            for (float side : { -1.0f, 1.0f }) {
                // Painted lower wall (to the handrail), paler upper wall.
                g.setColour(shade(wallLow, side < 0 ? 0.85f : 0.75f));
                quad(g, cam.at(side * halfWidth, 0, z0), cam.at(side * halfWidth, 0.95f, z0), cam.at(side * halfWidth, 0.95f, z1), cam.at(side * halfWidth, 0, z1));
                g.setColour(shade(wall, side < 0 ? 0.9f : 0.8f));
                quad(g, cam.at(side * halfWidth, 0.95f, z0), cam.at(side * halfWidth, height, z0), cam.at(side * halfWidth, height, z1), cam.at(side * halfWidth, 0.95f, z1));
            }
        }

        // The end wall, with double doors and a green exit sign above them.
        g.setColour(mix(juce::Colour(0xff3c4a44), fogColour, fogAt(length)));
        quad(g, cam.at(-halfWidth, 0, length), cam.at(-halfWidth, height, length), cam.at(halfWidth, height, length), cam.at(halfWidth, 0, length));
        g.setColour(mix(juce::Colour(0xff161c1a), fogColour, fogAt(length) * 0.6f));
        quad(g, cam.at(-0.75f, 0, length), cam.at(-0.75f, 2.2f, length), cam.at(0.75f, 2.2f, length), cam.at(0.75f, 0, length));
        g.setColour(juce::Colour(0x40a8c8b8));
        line(g, cam.at(0, 0, length), cam.at(0, 2.2f, length), 1.0f);
        for (float side : { -1.0f, 1.0f }) {   // the doors' small wired-glass windows
            g.setColour(juce::Colour(0x3090b8a8));
            quad(g, cam.at(side * 0.2f, 1.3f, length), cam.at(side * 0.2f, 1.8f, length), cam.at(side * 0.5f, 1.8f, length), cam.at(side * 0.5f, 1.3f, length));
        }
        {
            const auto a = cam.at(-0.22f, 2.38f, length), b = cam.at(0.22f, 2.6f, length);
            const juce::Rectangle<float> sign(a.x, b.y, b.x - a.x, a.y - b.y);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x5540ff90), sign.getCentre(), juce::Colour(0x0040ff90),
                                                   sign.getCentre().translated(sign.getWidth() * 2.5f, 0), true));
            g.fillEllipse(sign.expanded(sign.getWidth() * 2.0f, sign.getHeight() * 3.0f));
            g.setColour(juce::Colour(0xff8affb8));
            g.fillRect(sign);
        }

        // Floor tiles: joints across and along the corridor, fading with distance.
        for (float z = 0.3f; z < length; z += 0.6f) {
            g.setColour(juce::Colours::black.withAlpha(0.14f * (1.0f - fogAt(z))));
            line(g, cam.at(-halfWidth, 0, z), cam.at(halfWidth, 0, z), 1.0f);
        }
        for (float x = -halfWidth + 0.6f; x < halfWidth - 0.01f; x += 0.6f) {
            g.setColour(juce::Colours::black.withAlpha(0.12f));
            line(g, cam.at(x, 0, 0), cam.at(x, 0, length), 1.0f);
        }

        // Doors along both walls (some ajar to darkness), skirting boards and handrails.
        for (float side : { -1.0f, 1.0f }) {
            for (float z = side < 0 ? 3.3f : 4.6f; z < length - 1.5f; z += 4.6f + random.nextFloat() * 1.2f) {
                const float fog = fogAt(z + 0.5f), x = side * (halfWidth - 0.001f);
                const bool open = random.nextFloat() < 0.35f;
                // An open door shows an unlit room: dark, but not a flat black hole.
                g.setColour(mix(open ? juce::Colour(0xff0e1714) : juce::Colour(0xff6f7f74).withMultipliedBrightness(lightAt(z + 0.5f)), fogColour, fog));
                quad(g, cam.at(x, 0, z), cam.at(x, 2.15f, z), cam.at(x, 2.15f, z + 1.05f), cam.at(x, 0, z + 1.05f));
                g.setColour(mix(juce::Colour(0xff2a3530), fogColour, fog));
                juce::Path frame;
                frame.startNewSubPath(cam.at(x, 0, z));
                frame.lineTo(cam.at(x, 2.15f, z));
                frame.lineTo(cam.at(x, 2.15f, z + 1.05f));
                frame.lineTo(cam.at(x, 0, z + 1.05f));
                g.strokePath(frame, juce::PathStrokeType(std::max(1.0f, 3.0f / (z + nearZ))));
                if (!open) {   // a narrow glass panel
                    g.setColour(mix(juce::Colour(0xff9fbfb0), fogColour, fog).withAlpha(0.55f));
                    quad(g, cam.at(x, 1.25f, z + 0.65f), cam.at(x, 1.85f, z + 0.65f), cam.at(x, 1.85f, z + 0.85f), cam.at(x, 1.25f, z + 0.85f));
                }
            }
            g.setColour(juce::Colour(0x70101614));
            line(g, cam.at(side * halfWidth, 0.12f, 0), cam.at(side * halfWidth, 0.12f, length), 1.5f);
            g.setColour(juce::Colour(0x60c8d8cc));
            line(g, cam.at(side * (halfWidth - 0.06f), 0.95f, 0), cam.at(side * (halfWidth - 0.06f), 0.95f, length), 2.0f);
        }

        // Ceiling panels, their halation and their reflections on the polished floor.
        for (float z = firstLight; z < length; z += lightSpacing) {
            const auto a = cam.at(-0.35f, height - 0.001f, z - 0.6f), b = cam.at(0.35f, height - 0.001f, z - 0.6f);
            const auto c = cam.at(0.35f, height - 0.001f, z + 0.6f), d = cam.at(-0.35f, height - 0.001f, z + 0.6f);
            const float fog = fogAt(z);
            const juce::Rectangle<float> panel = juce::Rectangle<float>(a, c).getUnion(juce::Rectangle<float>(b, d));
            const float r = panel.getWidth() * 1.6f;
            const auto centre = panel.getCentre();
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe8fff2).withAlpha(0.42f * (1 - 0.6f * fog)), centre,
                                                   juce::Colour(0x00e8fff2), centre.translated(r, 0), true));
            g.fillEllipse(juce::Rectangle<float>(r * 2, r * 1.3f).withCentre(centre));
            g.setColour(mix(juce::Colour(0xfff4fff8), fogColour, fog * 0.7f));
            quad(g, a, b, c, d);
            scene.lights.push_back(juce::Rectangle<float>(r * 2, r * 1.3f).withCentre(centre));
            // Floor reflection: a soft vertical smear under the panel.
            const auto fa = cam.at(0, 0, z);
            const float rw = panel.getWidth() * 0.5f, rh = panel.getHeight() * 2.2f + 6.0f;
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd8f0e4).withAlpha(0.16f * (1 - fog)), fa,
                                                   juce::Colour(0x00d8f0e4), fa.translated(0, rh), true));
            g.fillEllipse(juce::Rectangle<float>(rw * 2, rh * 2).withCentre(fa));
        }

        // Haze toward the far end, then a cold grade and the vignette.
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x382e4a40), { cam.vx, cam.vy }, juce::Colour(0x002e4a40), { cam.vx + w * 0.45f, cam.vy }, true));
        g.fillRect(0.0f, 0.0f, w, h);
        g.setColour(juce::Colour(0x1a1e5a46));
        g.fillRect(0.0f, 0.0f, w, h);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x00000000), { w * 0.5f, h * 0.48f }, juce::Colour(0xc0000000), { 0, 0 }, true));
        g.fillRect(0.0f, 0.0f, w, h);
    }

    // Film grain (luminance noise), baked in once.
    {
        juce::Image::BitmapData data(scene.image, juce::Image::BitmapData::readWrite);
        juce::Random grain { juce::int64(seed) * 7919 + 1 };
        for (int y = 0; y < ph; ++y)
            for (int x = 0; x < pw; ++x) {
                const auto c = data.getPixelColour(x, y);
                const int n = grain.nextInt(17) - 8;
                auto ch = [n](juce::uint8 v) { return juce::uint8(std::clamp(int(v) + n, 0, 255)); };
                data.setPixelColour(x, y, juce::Colour(ch(c.getRed()), ch(c.getGreen()), ch(c.getBlue())));
            }
    }

    // The control strip's frosted glass: the scene shrunk (a cheap wide blur) and darkened.
    {
        const auto small = scene.image.rescaled(std::max(1, pw / 16), std::max(1, ph / 16), juce::Graphics::mediumResamplingQuality);
        scene.frosted = juce::Image(juce::Image::RGB, pw, ph, true);
        juce::Graphics g(scene.frosted);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(small, 0, 0, pw, ph, 0, 0, small.getWidth(), small.getHeight());
        g.fillAll(juce::Colour(0x8c0a0e0d));
    }
    return scene;
}

} // namespace liminal::ui
