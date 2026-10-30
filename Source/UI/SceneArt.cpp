#include "UI/SceneArt.h"
#include "DSP/Characters.h"
#include <algorithm>
#include <cmath>

namespace liminal::ui {

namespace {

juce::Colour mix(juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith(b, std::clamp(t, 0.0f, 1.0f)); }

struct Camera {
    float vx, vy, focal, eye, nearZ = 0.55f;
    juce::Point<float> at(float x, float y, float z) const {
        const float d = std::max(0.05f, z + nearZ);
        return { vx + focal * x / d, vy - focal * (y - eye) / d };
    }
};

void quad(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d) {
    juce::Path p;
    p.startNewSubPath(a);
    p.lineTo(b);
    p.lineTo(c);
    p.lineTo(d);
    p.closeSubPath();
    g.fillPath(p);
}
void line(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float thickness) { g.drawLine({ a, b }, thickness); }

// A rectangular room in metres: x across (±halfWidth), y up, z away from the camera.
struct Room {
    float halfWidth = 1.2f, height = 3.0f, length = 17.0f, fogDistance = 9.0f;
    juce::Colour fog { 0xff1d2a26 }, ceiling { 0xff8f9d94 }, wallUpper { 0xff7f9a8c }, wallLower { 0xff4d6b60 }, floor { 0xff5b6a62 };
    float wainscot = 0.95f;                 // height of the lower wall colour
    std::vector<float> lightZ;              // where the ceiling lights are (for shading)
    float lightReach = 1.7f, ambient = 0.35f;
    float fogAt(float z) const { return 1.0f - std::exp(-z / fogDistance); }
    float lightAt(float z) const {
        float sum = 0;
        for (const float lz : lightZ) {
            const float d = (z - lz) / lightReach;
            sum += std::exp(-d * d);
        }
        return std::min(1.0f, ambient + (1.0f - ambient) * sum);
    }
    juce::Colour shade(juce::Colour c, float z, float extra = 1.0f) const { return mix(c.withMultipliedBrightness(lightAt(z) * extra), fog, fogAt(z)); }
};

// Floor, ceiling and both walls in depth slices, far to near.
void shell(juce::Graphics& g, const Camera& cam, const Room& r, bool walls = true, bool ceiling = true) {
    const int slices = 90;
    for (int i = slices - 1; i >= 0; --i) {
        const float z0 = r.length * std::pow(float(i) / slices, 1.6f), z1 = r.length * std::pow(float(i + 1) / slices, 1.6f);
        const float zm = 0.5f * (z0 + z1);
        if (ceiling) {
            g.setColour(r.shade(r.ceiling, zm, 0.8f));
            quad(g, cam.at(-r.halfWidth, r.height, z0), cam.at(r.halfWidth, r.height, z0), cam.at(r.halfWidth, r.height, z1), cam.at(-r.halfWidth, r.height, z1));
        }
        g.setColour(r.shade(r.floor, zm, 0.9f));
        quad(g, cam.at(-r.halfWidth, 0, z0), cam.at(r.halfWidth, 0, z0), cam.at(r.halfWidth, 0, z1), cam.at(-r.halfWidth, 0, z1));
        if (!walls) continue;
        for (float side : { -1.0f, 1.0f }) {
            g.setColour(r.shade(r.wallLower, zm, side < 0 ? 0.85f : 0.75f));
            quad(g, cam.at(side * r.halfWidth, 0, z0), cam.at(side * r.halfWidth, r.wainscot, z0), cam.at(side * r.halfWidth, r.wainscot, z1), cam.at(side * r.halfWidth, 0, z1));
            g.setColour(r.shade(r.wallUpper, zm, side < 0 ? 0.9f : 0.8f));
            quad(g, cam.at(side * r.halfWidth, r.wainscot, z0), cam.at(side * r.halfWidth, r.height, z0), cam.at(side * r.halfWidth, r.height, z1),
                 cam.at(side * r.halfWidth, r.wainscot, z1));
        }
    }
}

void endWall(juce::Graphics& g, const Camera& cam, const Room& r, juce::Colour c) {
    g.setColour(mix(c, r.fog, r.fogAt(r.length)));
    quad(g, cam.at(-r.halfWidth, 0, r.length), cam.at(-r.halfWidth, r.height, r.length), cam.at(r.halfWidth, r.height, r.length), cam.at(r.halfWidth, 0, r.length));
}

// A ceiling fixture: the panel, its halation, its reflection on the floor; recorded for the animation.
void ceilingLight(juce::Graphics& g, Scene& scene, const Camera& cam, const Room& r, float x, float z, float halfW, float halfL, juce::Colour colour,
                  float glow = 0.42f, float reflection = 0.16f, float y = -1.0f) {
    const float yy = y < 0 ? r.height - 0.001f : y;
    const auto a = cam.at(x - halfW, yy, z - halfL), b = cam.at(x + halfW, yy, z - halfL), c = cam.at(x + halfW, yy, z + halfL), d = cam.at(x - halfW, yy, z + halfL);
    const float fog = r.fogAt(z);
    const auto panel = juce::Rectangle<float>(a, c).getUnion(juce::Rectangle<float>(b, d));
    const float rad = std::max(panel.getWidth(), panel.getHeight() * 2.0f) * 1.6f + 2.0f;
    const auto centre = panel.getCentre();
    g.setGradientFill(juce::ColourGradient(colour.withAlpha(glow * (1 - 0.6f * fog)), centre, colour.withAlpha(0.0f), centre.translated(rad, 0), true));
    g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 1.3f).withCentre(centre));
    g.setColour(mix(colour.brighter(0.3f), r.fog, fog * 0.7f));
    quad(g, a, b, c, d);
    Light light { juce::Rectangle<float>(rad * 2, rad * 1.3f).withCentre(centre), { -1, -1 }, rad };
    if (reflection > 0.0f) {
        const auto fa = cam.at(x, 0, z);
        const float rw = panel.getWidth() * 0.5f + 1.0f, rh = panel.getHeight() * 2.2f + 6.0f;
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(reflection * (1 - fog)), fa, colour.withAlpha(0.0f), fa.translated(0, rh), true));
        g.fillEllipse(juce::Rectangle<float>(rw * 2, rh * 2).withCentre(fa));
        light.reflection = fa;
    }
    scene.lights.push_back(light);
}

// A warm wall light (hotel sconces, balcony lamps): a pool of light on the wall.
void wallLight(juce::Graphics& g, Scene& scene, const Camera& cam, const Room& r, float side, float y, float z, juce::Colour colour) {
    const auto p = cam.at(side * (r.halfWidth - 0.01f), y, z);
    const float s = cam.focal / (z + cam.nearZ);
    const float rad = 0.9f * s + 2.0f;
    const float fog = r.fogAt(z);
    g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.55f * (1 - 0.7f * fog)), p, colour.withAlpha(0.0f), p.translated(rad, 0), true));
    g.fillEllipse(juce::Rectangle<float>(rad * 1.3f, rad * 2.2f).withCentre(p));
    g.setColour(mix(colour.brighter(0.5f), r.fog, fog * 0.6f));
    g.fillEllipse(juce::Rectangle<float>(0.12f * s + 1.0f, 0.2f * s + 1.0f).withCentre(p));
    scene.lights.push_back({ juce::Rectangle<float>(rad * 1.3f, rad * 2.2f).withCentre(p), { -1, -1 }, rad });
}

void tilesOnFloor(juce::Graphics& g, const Camera& cam, const Room& r, float step, float alpha, float x0, float x1) {
    for (float z = 0.3f; z < r.length; z += step) {
        g.setColour(juce::Colours::black.withAlpha(alpha * (1.0f - r.fogAt(z))));
        line(g, cam.at(x0, 0, z), cam.at(x1, 0, z), 1.0f);
    }
    for (float x = x0 + step; x < x1 - 0.01f; x += step) {
        g.setColour(juce::Colours::black.withAlpha(alpha * 0.85f));
        line(g, cam.at(x, 0, 0), cam.at(x, 0, r.length), 1.0f);
    }
}

void doorsAlong(juce::Graphics& g, const Camera& cam, const Room& r, juce::Random& random, juce::Colour door, juce::Colour frameColour, float spacing,
                float openChance, float doorHeight = 2.15f, bool glassPanel = true) {
    for (float side : { -1.0f, 1.0f })
        for (float z = side < 0 ? spacing * 0.7f : spacing; z < r.length - 1.5f; z += spacing + random.nextFloat() * spacing * 0.25f) {
            const float fog = r.fogAt(z + 0.5f), x = side * (r.halfWidth - 0.001f);
            const bool open = random.nextFloat() < openChance;
            g.setColour(mix(open ? juce::Colour(0xff0d1211) : door.withMultipliedBrightness(r.lightAt(z + 0.5f)), r.fog, fog));
            quad(g, cam.at(x, 0, z), cam.at(x, doorHeight, z), cam.at(x, doorHeight, z + 1.05f), cam.at(x, 0, z + 1.05f));
            g.setColour(mix(frameColour, r.fog, fog));
            juce::Path frame;
            frame.startNewSubPath(cam.at(x, 0, z));
            frame.lineTo(cam.at(x, doorHeight, z));
            frame.lineTo(cam.at(x, doorHeight, z + 1.05f));
            frame.lineTo(cam.at(x, 0, z + 1.05f));
            g.strokePath(frame, juce::PathStrokeType(std::max(1.0f, 3.0f / (z + cam.nearZ))));
            if (!open && glassPanel) {
                g.setColour(mix(juce::Colour(0xff9fbfb0), r.fog, fog).withAlpha(0.5f));
                quad(g, cam.at(x, 1.25f, z + 0.65f), cam.at(x, 1.85f, z + 0.65f), cam.at(x, 1.85f, z + 0.85f), cam.at(x, 1.25f, z + 0.85f));
            }
        }
}

// A box (pillar, shelf, partition) between x0..x1, y0..y1, z0..z1: the faces the camera sees.
void box(juce::Graphics& g, const Camera& cam, const Room& r, float x0, float x1, float y0, float y1, float z0, float z1, juce::Colour c) {
    const float fog = r.fogAt(z0), light = r.lightAt(z0);
    // Side face toward the centre line.
    if (x1 < 0.0f) {
        g.setColour(mix(c.withMultipliedBrightness(light * 0.7f), r.fog, fog));
        quad(g, cam.at(x1, y0, z0), cam.at(x1, y1, z0), cam.at(x1, y1, z1), cam.at(x1, y0, z1));
    } else if (x0 > 0.0f) {
        g.setColour(mix(c.withMultipliedBrightness(light * 0.6f), r.fog, fog));
        quad(g, cam.at(x0, y0, z0), cam.at(x0, y1, z0), cam.at(x0, y1, z1), cam.at(x0, y0, z1));
    }
    // Top (if below the eye) and front face.
    if (y1 < cam.eye) {
        g.setColour(mix(c.withMultipliedBrightness(light * 1.1f), r.fog, fog));
        quad(g, cam.at(x0, y1, z0), cam.at(x1, y1, z0), cam.at(x1, y1, z1), cam.at(x0, y1, z1));
    }
    g.setColour(mix(c.withMultipliedBrightness(light * 0.9f), r.fog, fog));
    quad(g, cam.at(x0, y0, z0), cam.at(x0, y1, z0), cam.at(x1, y1, z0), cam.at(x1, y0, z0));
}

//==============================================================================
void hospital(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    Room r;
    r.length = 17.0f;
    for (float z = 1.6f; z < r.length; z += 3.4f) r.lightZ.push_back(z);
    shell(g, cam, r);
    endWall(g, cam, r, juce::Colour(0xff3c4a44));
    g.setColour(mix(juce::Colour(0xff161c1a), r.fog, r.fogAt(r.length) * 0.6f));
    quad(g, cam.at(-0.75f, 0, r.length), cam.at(-0.75f, 2.2f, r.length), cam.at(0.75f, 2.2f, r.length), cam.at(0.75f, 0, r.length));
    {   // the exit sign
        const auto a = cam.at(-0.22f, 2.38f, r.length), b = cam.at(0.22f, 2.6f, r.length);
        const juce::Rectangle<float> sign(a.x, b.y, b.x - a.x, a.y - b.y);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x5540ff90), sign.getCentre(), juce::Colour(0x0040ff90), sign.getCentre().translated(sign.getWidth() * 2.5f, 0), true));
        g.fillEllipse(sign.expanded(sign.getWidth() * 2.0f, sign.getHeight() * 3.0f));
        g.setColour(juce::Colour(0xff8affb8));
        g.fillRect(sign);
    }
    tilesOnFloor(g, cam, r, 0.6f, 0.13f, -r.halfWidth, r.halfWidth);
    doorsAlong(g, cam, r, random, juce::Colour(0xff6f7f74), juce::Colour(0xff2a3530), 4.6f, 0.35f);
    for (float side : { -1.0f, 1.0f }) {
        g.setColour(juce::Colour(0x60c8d8cc));
        line(g, cam.at(side * (r.halfWidth - 0.06f), 0.95f, 0), cam.at(side * (r.halfWidth - 0.06f), 0.95f, r.length), 2.0f);
    }
    for (const float z : r.lightZ) ceilingLight(g, scene, cam, r, 0, z, 0.35f, 0.6f, juce::Colour(0xffe8fff2));
    scene.lightColour = juce::Colour(0xffe0fff0);
}

void mall(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    cam.vy *= 1.12f;   // a higher horizon: the atrium rises above us
    Room r;
    r.halfWidth = 9.0f; r.height = 13.0f; r.length = 70.0f; r.fogDistance = 32.0f;
    r.fog = juce::Colour(0xff2a2219); r.ceiling = juce::Colour(0xff2f3846); r.wallUpper = juce::Colour(0xff8a7a62); r.wallLower = juce::Colour(0xff6a5a46);
    r.floor = juce::Colour(0xffa89a84); r.wainscot = 4.5f; r.ambient = 0.5f; r.lightReach = 4.0f;
    for (float z = 4.0f; z < r.length; z += 8.0f) r.lightZ.push_back(z);
    shell(g, cam, r);
    // The skylight: a glass roof against the night.
    for (float z = 0; z < r.length; z += 3.0f) {
        g.setColour(mix(juce::Colour(0xff1c2a40), r.fog, r.fogAt(z)).withAlpha(0.7f));
        quad(g, cam.at(-4.0f, r.height, z), cam.at(4.0f, r.height, z), cam.at(4.0f, r.height, z + 2.85f), cam.at(-4.0f, r.height, z + 2.85f));
    }
    endWall(g, cam, r, juce::Colour(0xff4a3e30));
    // Three storeys of shopfronts behind balconies; shutters half down; one still lit.
    for (int storey = 0; storey < 3; ++storey) {
        const float y0 = 4.5f * float(storey), y1 = y0 + 3.4f;
        for (float side : { -1.0f, 1.0f })
            for (float z = 2.0f + float(storey); z < r.length - 4.0f; z += 7.0f) {
                const float fog = r.fogAt(z), x = side * (r.halfWidth - 0.01f);
                const bool lit = random.nextFloat() < 0.12f;
                g.setColour(mix(lit ? juce::Colour(0xffd8b070) : juce::Colour(0xff151210), r.fog, fog));
                quad(g, cam.at(x, y0 + 0.2f, z), cam.at(x, y1, z), cam.at(x, y1, z + 6.0f), cam.at(x, y0 + 0.2f, z + 6.0f));
                const float shutter = y1 - (y1 - y0) * (0.3f + 0.7f * random.nextFloat());
                g.setColour(mix(juce::Colour(0xff6c6458), r.fog, fog));
                quad(g, cam.at(x, shutter, z), cam.at(x, y1, z), cam.at(x, y1, z + 6.0f), cam.at(x, shutter, z + 6.0f));
                g.setColour(mix(juce::Colour(0x40000000), r.fog, fog));
                for (float yy = shutter; yy < y1; yy += 0.25f) line(g, cam.at(x, yy, z), cam.at(x, yy, z + 6.0f), 1.0f);
            }
        if (storey > 0)   // the balcony slab edge and its rail
            for (float side : { -1.0f, 1.0f }) {
                g.setColour(juce::Colour(0xb0d8c8a8));
                line(g, cam.at(side * (r.halfWidth - 2.0f), y0, 0), cam.at(side * (r.halfWidth - 2.0f), y0, r.length), 3.0f);
                g.setColour(juce::Colour(0x70e8dcc0));
                line(g, cam.at(side * (r.halfWidth - 2.0f), y0 + 1.1f, 0), cam.at(side * (r.halfWidth - 2.0f), y0 + 1.1f, r.length), 1.5f);
            }
    }
    // The escalator, rising to the first floor.
    {
        const float z0 = 16.0f, z1 = 26.0f;
        g.setColour(mix(juce::Colour(0xff3a3530), r.fog, r.fogAt(z0)));
        quad(g, cam.at(-1.4f, 0.0f, z0), cam.at(1.4f, 0.0f, z0), cam.at(1.4f, 4.5f, z1), cam.at(-1.4f, 4.5f, z1));
        g.setColour(juce::Colour(0x80d8d0c0));
        for (float side : { -1.5f, 1.5f }) line(g, cam.at(side, 1.0f, z0), cam.at(side, 5.5f, z1), 2.0f);
        g.setColour(juce::Colour(0x40000000));
        for (float t = 0; t < 1.0f; t += 0.05f) line(g, cam.at(-1.4f, 4.5f * t, z0 + 10.0f * t), cam.at(1.4f, 4.5f * t, z0 + 10.0f * t), 1.0f);
    }
    tilesOnFloor(g, cam, r, 2.0f, 0.08f, -r.halfWidth, r.halfWidth);
    for (const float z : r.lightZ)
        for (float side : { -1.0f, 1.0f }) ceilingLight(g, scene, cam, r, side * (r.halfWidth - 2.5f), z, 0.3f, 0.3f, juce::Colour(0xffffc880), 0.5f, 0.18f, 4.4f);
    scene.lightColour = juce::Colour(0xffffcf90);
}

void school(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    Room r;
    r.halfWidth = 1.5f; r.height = 3.0f; r.length = 22.0f; r.fogDistance = 11.0f;
    r.fog = juce::Colour(0xff2c2818); r.ceiling = juce::Colour(0xffb8b090); r.wallUpper = juce::Colour(0xffc4b278); r.wallLower = juce::Colour(0xff6f7a5c);
    r.floor = juce::Colour(0xff8a7c60); r.wainscot = 1.8f;
    for (float z = 1.5f; z < r.length; z += 3.0f) r.lightZ.push_back(z);
    shell(g, cam, r);
    endWall(g, cam, r, juce::Colour(0xff5a5236));
    // Lockers: vertical seams, vents, handles.
    for (float side : { -1.0f, 1.0f })
        for (float z = 0.0f; z < r.length; z += 0.32f) {
            const float x = side * (r.halfWidth - 0.002f);
            g.setColour(juce::Colours::black.withAlpha(0.22f * (1 - r.fogAt(z))));
            line(g, cam.at(x, 0.1f, z), cam.at(x, 1.8f, z), 1.0f);
            if (random.nextFloat() < 0.5f) {
                g.setColour(juce::Colours::black.withAlpha(0.18f * (1 - r.fogAt(z))));
                line(g, cam.at(x, 1.55f, z + 0.08f), cam.at(x, 1.55f, z + 0.24f), 1.0f);
            }
        }
    // Notice boards and classroom doors.
    for (float z = 3.0f; z < r.length - 3.0f; z += 6.5f) {
        const float x = -(r.halfWidth - 0.003f);
        g.setColour(mix(juce::Colour(0xff8a6440), r.fog, r.fogAt(z)));
        quad(g, cam.at(x, 1.9f, z), cam.at(x, 2.6f, z), cam.at(x, 2.6f, z + 1.6f), cam.at(x, 1.9f, z + 1.6f));
        for (int k = 0; k < 5; ++k) {
            const float zz = z + 0.1f + 1.3f * random.nextFloat(), yy = 2.0f + 0.45f * random.nextFloat();
            g.setColour(mix(juce::Colour(0xffe8e0c8), r.fog, r.fogAt(zz)).withAlpha(0.85f));
            quad(g, cam.at(x, yy, zz), cam.at(x, yy + 0.18f, zz), cam.at(x, yy + 0.18f, zz + 0.14f), cam.at(x, yy, zz + 0.14f));
        }
    }
    doorsAlong(g, cam, r, random, juce::Colour(0xff7a5a36), juce::Colour(0xff3a2c1c), 5.5f, 0.25f, 2.1f, true);
    tilesOnFloor(g, cam, r, 0.3f, 0.07f, -r.halfWidth, r.halfWidth);
    // Ceiling tiles and old tubes.
    for (float z = 0.6f; z < r.length; z += 0.6f) {
        g.setColour(juce::Colours::black.withAlpha(0.12f * (1 - r.fogAt(z))));
        line(g, cam.at(-r.halfWidth, r.height, z), cam.at(r.halfWidth, r.height, z), 1.0f);
    }
    for (const float z : r.lightZ) ceilingLight(g, scene, cam, r, 0, z, 0.15f, 0.6f, juce::Colour(0xfffff2c8), 0.38f, 0.12f);
    scene.lightColour = juce::Colour(0xfffff0c0);
}

void garage(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    cam.vy *= 0.96f;
    Room r;
    r.halfWidth = 9.0f; r.height = 2.7f; r.length = 48.0f; r.fogDistance = 16.0f;
    r.fog = juce::Colour(0xff120c08); r.ceiling = juce::Colour(0xff58544e); r.wallUpper = juce::Colour(0xff4a4640); r.wallLower = juce::Colour(0xff3e3a34);
    r.floor = juce::Colour(0xff5a564e); r.wainscot = 0.8f; r.ambient = 0.2f; r.lightReach = 2.6f;
    std::vector<std::pair<float, float>> lit;   // x, z of the lights that still work
    for (float z = 3.0f; z < r.length; z += 7.5f)
        for (float x : { -4.5f, 0.0f, 4.5f })
            if (random.nextFloat() < (x == 0.0f ? 0.7f : 0.35f)) lit.push_back({ x, z });
    for (const auto& [x, z] : lit) r.lightZ.push_back(z);
    shell(g, cam, r);
    endWall(g, cam, r, juce::Colour(0xff1a1612));
    // Parking bays: lines from both walls.
    for (float side : { -1.0f, 1.0f })
        for (float z = 1.0f; z < r.length; z += 2.5f) {
            g.setColour(juce::Colour(0xffd8d0b8).withAlpha(0.45f * (1 - r.fogAt(z)) * r.lightAt(z)));
            line(g, cam.at(side * r.halfWidth, 0, z), cam.at(side * (r.halfWidth - 5.0f), 0, z), 1.5f);
        }
    g.setColour(juce::Colour(0x60d8c060));
    line(g, cam.at(-0.08f, 0.001f, 0), cam.at(-0.08f, 0.001f, r.length), 2.0f);
    // Ceiling beams and pillars in rows.
    for (float z = 3.75f; z < r.length; z += 7.5f) {
        g.setColour(mix(juce::Colour(0xff2c2924), r.fog, r.fogAt(z)));
        quad(g, cam.at(-r.halfWidth, r.height - 0.35f, z), cam.at(r.halfWidth, r.height - 0.35f, z), cam.at(r.halfWidth, r.height - 0.35f, z + 0.4f),
             cam.at(-r.halfWidth, r.height - 0.35f, z + 0.4f));
    }
    for (float z = r.length - 3.75f; z > 0.5f; z -= 7.5f)
        for (float x : { -3.0f, 3.0f }) {
            box(g, cam, r, x - 0.3f, x + 0.3f, 0, r.height, z, z + 0.6f, juce::Colour(0xff6e6a62));
            g.setColour(mix(juce::Colour(0xffc8a830), r.fog, r.fogAt(z)).withAlpha(0.8f));
            quad(g, cam.at(x - 0.3f, 0, z), cam.at(x - 0.3f, 0.3f, z), cam.at(x + 0.3f, 0.3f, z), cam.at(x + 0.3f, 0, z));
        }
    for (const auto& [x, z] : lit) ceilingLight(g, scene, cam, r, x, z, 0.25f, 0.15f, juce::Colour(0xffffa040), 0.32f, 0.25f);
    scene.lightColour = juce::Colour(0xffffa848);
}

void pool(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    cam.vy *= 1.05f;
    Room r;
    r.halfWidth = 8.0f; r.height = 8.0f; r.length = 34.0f; r.fogDistance = 22.0f;
    r.fog = juce::Colour(0xff1c3438); r.ceiling = juce::Colour(0xff6a8a90); r.wallUpper = juce::Colour(0xffa8c8cc); r.wallLower = juce::Colour(0xff88b0b8);
    r.floor = juce::Colour(0xffb0c4c4); r.wainscot = 2.5f; r.ambient = 0.55f; r.lightReach = 5.0f;
    for (float z = 4.0f; z < r.length; z += 7.0f) r.lightZ.push_back(z);
    shell(g, cam, r);
    endWall(g, cam, r, juce::Colour(0xff6f9298));
    // Wall tiles.
    for (float side : { -1.0f, 1.0f }) {
        const float x = side * (r.halfWidth - 0.002f);
        for (float y = 0.25f; y < 3.0f; y += 0.25f) {
            g.setColour(juce::Colours::black.withAlpha(0.08f));
            line(g, cam.at(x, y, 0), cam.at(x, y, r.length), 1.0f);
        }
        for (float z = 0; z < r.length; z += 0.5f) {
            g.setColour(juce::Colours::black.withAlpha(0.08f * (1 - r.fogAt(z))));
            line(g, cam.at(x, 0, z), cam.at(x, 3.0f, z), 1.0f);
        }
        // High windows: pale daylight.
        for (float z = 2.0f; z < r.length - 3; z += 4.0f) {
            g.setColour(mix(juce::Colour(0xffd8f0f4), r.fog, r.fogAt(z) * 0.7f));
            quad(g, cam.at(x, 4.0f, z), cam.at(x, 6.8f, z), cam.at(x, 6.8f, z + 2.6f), cam.at(x, 4.0f, z + 2.6f));
        }
    }
    // The pool: water below the deck, lane lines, the light on it.
    const float px = 5.5f, pz0 = 2.5f, pz1 = r.length - 3.0f;
    for (int i = 59; i >= 0; --i) {
        const float z0 = pz0 + (pz1 - pz0) * float(i) / 60.0f, z1 = pz0 + (pz1 - pz0) * float(i + 1) / 60.0f;
        g.setColour(mix(juce::Colour(0xff2a8ca0).withMultipliedBrightness(0.6f + 0.4f * r.lightAt(z0)), r.fog, r.fogAt(z0)));
        quad(g, cam.at(-px, -0.25f, z0), cam.at(px, -0.25f, z0), cam.at(px, -0.25f, z1), cam.at(-px, -0.25f, z1));
    }
    for (float x = -px + 2.2f; x < px; x += 2.2f) {
        g.setColour(juce::Colour(0x60104858));
        line(g, cam.at(x, -0.25f, pz0), cam.at(x, -0.25f, pz1), 2.0f);
    }
    g.setColour(juce::Colour(0xffe8f4f4));
    line(g, cam.at(-px, 0, pz0), cam.at(px, 0, pz0), 2.5f);
    for (float side : { -1.0f, 1.0f }) line(g, cam.at(side * px, 0, pz0), cam.at(side * px, 0, pz1), 2.0f);
    // Caustic glints.
    for (int k = 0; k < 140; ++k) {
        const float z = pz0 + (pz1 - pz0) * std::pow(random.nextFloat(), 0.7f), x = (random.nextFloat() * 2 - 1) * px;
        const auto p = cam.at(x, -0.25f, z);
        const float s = 0.15f * cam.focal / (z + cam.nearZ);
        g.setColour(juce::Colour(0xffd0fbff).withAlpha(0.25f * (1 - r.fogAt(z))));
        g.fillEllipse(juce::Rectangle<float>(s * 2.5f, s * 0.6f + 0.5f).withCentre(p));
    }
    // A ladder at the near edge.
    g.setColour(juce::Colour(0xffd8e0e0));
    for (float x : { -px + 0.6f, -px + 1.1f }) line(g, cam.at(x, -0.25f, pz0 + 0.3f), cam.at(x, 1.0f, pz0 - 0.05f), 2.5f);
    for (const float z : r.lightZ) ceilingLight(g, scene, cam, r, 0, z, 0.6f, 0.6f, juce::Colour(0xffe0faff), 0.4f, 0.2f);
    scene.lightColour = juce::Colour(0xffd8f8ff);
}

void hotel(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    Room r;
    r.halfWidth = 1.1f; r.height = 2.6f; r.length = 26.0f; r.fogDistance = 10.0f;
    r.fog = juce::Colour(0xff1e140e); r.ceiling = juce::Colour(0xff8a7a64); r.wallUpper = juce::Colour(0xffa88660); r.wallLower = juce::Colour(0xff4a3020);
    r.floor = juce::Colour(0xff5a2420); r.wainscot = 0.9f; r.ambient = 0.3f; r.lightReach = 1.4f;
    for (float z = 2.0f; z < r.length; z += 3.5f) r.lightZ.push_back(z);
    shell(g, cam, r);
    endWall(g, cam, r, juce::Colour(0xff2a2030));
    {   // a curtained window at the end, night outside
        g.setColour(mix(juce::Colour(0xff28344c), r.fog, r.fogAt(r.length) * 0.5f));
        quad(g, cam.at(-0.5f, 0.9f, r.length - 0.01f), cam.at(-0.5f, 2.1f, r.length - 0.01f), cam.at(0.5f, 2.1f, r.length - 0.01f), cam.at(0.5f, 0.9f, r.length - 0.01f));
    }
    // Wallpaper stripes.
    for (float side : { -1.0f, 1.0f })
        for (float z = 0.0f; z < r.length; z += 0.18f) {
            g.setColour(juce::Colours::black.withAlpha(0.06f * (1 - r.fogAt(z))));
            line(g, cam.at(side * (r.halfWidth - 0.002f), 0.9f, z), cam.at(side * (r.halfWidth - 0.002f), r.height, z), 1.0f);
        }
    // Carpet: a diamond pattern and a border.
    for (float z = 0.2f; z < r.length; z += 0.5f) {
        g.setColour(juce::Colour(0x30d0a060).withMultipliedAlpha(1 - r.fogAt(z)));
        line(g, cam.at(-0.7f, 0.001f, z), cam.at(0.0f, 0.001f, z + 0.25f), 1.0f);
        line(g, cam.at(0.0f, 0.001f, z + 0.25f), cam.at(0.7f, 0.001f, z), 1.0f);
    }
    for (float side : { -1.0f, 1.0f }) {
        g.setColour(juce::Colour(0x50c09050));
        line(g, cam.at(side * 0.8f, 0.001f, 0), cam.at(side * 0.8f, 0.001f, r.length), 1.5f);
    }
    doorsAlong(g, cam, r, random, juce::Colour(0xff5a3a24), juce::Colour(0xffb08840), 3.5f, 0.1f, 2.1f, false);
    // Brass room numbers.
    for (float side : { -1.0f, 1.0f })
        for (float z = side < 0 ? 2.45f : 3.5f; z < r.length - 1.5f; z += 3.5f) {
            g.setColour(mix(juce::Colour(0xffd8b060), r.fog, r.fogAt(z)));
            const auto p = cam.at(side * (r.halfWidth - 0.01f), 1.6f, z + 0.5f);
            const float s = cam.focal / (z + cam.nearZ);
            g.fillRect(juce::Rectangle<float>(0.05f * s + 1, 0.06f * s + 1).withCentre(p));
        }
    for (const float z : r.lightZ) wallLight(g, scene, cam, r, (int(z) % 2 == 0) ? -1.0f : 1.0f, 1.85f, z, juce::Colour(0xffffb860));
    scene.lightColour = juce::Colour(0xffffc070);
}

void subway(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    cam.vx *= 0.82f;   // standing on the platform, the tracks to the right
    Room r;
    r.halfWidth = 6.0f; r.height = 4.2f; r.length = 60.0f; r.fogDistance = 20.0f;
    r.fog = juce::Colour(0xff121a14); r.ceiling = juce::Colour(0xff3c4440); r.wallUpper = juce::Colour(0xffbcb89c); r.wallLower = juce::Colour(0xff6a7060);
    r.floor = juce::Colour(0xff6c6a62); r.wainscot = 1.2f; r.ambient = 0.3f; r.lightReach = 3.0f;
    for (float z = 2.5f; z < r.length; z += 5.0f) r.lightZ.push_back(z);
    shell(g, cam, r);
    // The track bed: a dark trench on the right with rails.
    const float edge = 1.6f;
    for (int i = 59; i >= 0; --i) {
        const float z0 = r.length * std::pow(float(i) / 60, 1.5f), z1 = r.length * std::pow(float(i + 1) / 60, 1.5f);
        g.setColour(mix(juce::Colour(0xff141210), r.fog, r.fogAt(z0)));
        quad(g, cam.at(edge, -1.1f, z0), cam.at(r.halfWidth, -1.1f, z0), cam.at(r.halfWidth, -1.1f, z1), cam.at(edge, -1.1f, z1));
        g.setColour(mix(juce::Colour(0xff2a2824), r.fog, r.fogAt(z0)));
        quad(g, cam.at(edge, 0, z0), cam.at(edge, -1.1f, z0), cam.at(edge, -1.1f, z1), cam.at(edge, 0, z1));
    }
    for (float x : { 3.0f, 4.45f }) {
        g.setColour(juce::Colour(0xb0c8ccd0));
        line(g, cam.at(x, -1.0f, 0.2f), cam.at(x, -1.0f, r.length), 1.6f);
    }
    g.setColour(juce::Colour(0xffd8c040));   // the yellow line
    line(g, cam.at(edge - 0.35f, 0.002f, 0), cam.at(edge - 0.35f, 0.002f, r.length), 3.0f);
    // The tunnel mouth at the end: black.
    g.setColour(mix(juce::Colour(0xff3a3a32), r.fog, r.fogAt(r.length)));
    quad(g, cam.at(-r.halfWidth, 0, r.length), cam.at(-r.halfWidth, r.height, r.length), cam.at(r.halfWidth, r.height, r.length), cam.at(r.halfWidth, 0, r.length));
    {
        juce::Path arch;
        const auto bl = cam.at(edge, -1.1f, r.length), br = cam.at(r.halfWidth, -1.1f, r.length), top = cam.at(0.5f * (edge + r.halfWidth), 3.6f, r.length);
        arch.startNewSubPath(bl);
        arch.quadraticTo({ bl.x, top.y }, top);
        arch.quadraticTo({ br.x, top.y }, br);
        arch.closeSubPath();
        g.setColour(juce::Colour(0xff050605));
        g.fillPath(arch);
    }
    // Tiles on the far wall, a sign band, columns on the platform.
    const float wx = r.halfWidth - 0.002f;
    for (float z = 0; z < r.length; z += 0.6f) {
        g.setColour(juce::Colours::black.withAlpha(0.1f * (1 - r.fogAt(z))));
        line(g, cam.at(wx, 0, z), cam.at(wx, r.height, z), 1.0f);
    }
    for (float z = 6.0f; z < r.length - 6; z += 12.0f) {
        g.setColour(mix(juce::Colour(0xff1c2a4a), r.fog, r.fogAt(z)));
        quad(g, cam.at(wx, 2.3f, z), cam.at(wx, 2.9f, z), cam.at(wx, 2.9f, z + 4.0f), cam.at(wx, 2.3f, z + 4.0f));
        g.setColour(mix(juce::Colour(0xffd8dce8), r.fog, r.fogAt(z)).withAlpha(0.7f));
        quad(g, cam.at(wx, 2.52f, z + 0.5f), cam.at(wx, 2.68f, z + 0.5f), cam.at(wx, 2.68f, z + 2.4f), cam.at(wx, 2.52f, z + 2.4f));
    }
    for (float z = r.length - 4.0f; z > 2.0f; z -= 8.0f) box(g, cam, r, -2.2f, -1.6f, 0, r.height, z, z + 0.6f, juce::Colour(0xff9a9880));
    for (const float z : r.lightZ)
        if (random.nextFloat() < 0.85f) ceilingLight(g, scene, cam, r, -2.5f, z, 0.08f, 1.5f, juce::Colour(0xffe8f8e0), 0.4f, 0.14f);
    scene.lightColour = juce::Colour(0xffe8f8e0);
}

void supermarket(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    Room r;
    r.halfWidth = 1.7f; r.height = 4.6f; r.length = 32.0f; r.fogDistance = 18.0f;
    r.fog = juce::Colour(0xff1c2224); r.ceiling = juce::Colour(0xff2a2c2e); r.wallUpper = juce::Colour(0xff3a3e40); r.wallLower = juce::Colour(0xffc0c4c0);
    r.floor = juce::Colour(0xffc8ccc8); r.wainscot = 2.1f; r.ambient = 0.6f; r.lightReach = 1.2f;
    for (float z = 0.8f; z < r.length; z += 1.6f) r.lightZ.push_back(z);
    shell(g, cam, r);
    // The cold cases at the end of the aisle, glowing.
    endWall(g, cam, r, juce::Colour(0xff9ab8c8));
    // Shelves: shelf edges and faded packaging.
    const juce::Colour packs[] { juce::Colour(0xffb8584a), juce::Colour(0xffd8b860), juce::Colour(0xff5a8ab0), juce::Colour(0xff6aa070), juce::Colour(0xffd0d0c8), juce::Colour(0xffa070a8) };
    for (float side : { -1.0f, 1.0f }) {
        const float x = side * (r.halfWidth - 0.002f);
        for (float z = 0.2f; z < r.length - 0.4f; z += 0.32f)
            for (float y = 0.15f; y < 2.0f; y += 0.42f) {
                if (random.nextFloat() < 0.12f) continue;   // gaps on the shelves
                const auto c = packs[random.nextInt(6)].withMultipliedSaturation(0.55f);
                g.setColour(mix(c.withMultipliedBrightness(r.lightAt(z)), r.fog, r.fogAt(z)));
                quad(g, cam.at(x, y, z), cam.at(x, y + 0.3f, z), cam.at(x, y + 0.3f, z + 0.26f), cam.at(x, y, z + 0.26f));
            }
        for (float y = 0.12f; y < 2.1f; y += 0.42f) {
            g.setColour(juce::Colour(0xc0e8ece8));
            line(g, cam.at(x, y, 0), cam.at(x, y, r.length), 1.5f);
        }
    }
    // Long fluorescent rows above the aisle, and their reflections on the glossy floor.
    for (const float z : r.lightZ) ceilingLight(g, scene, cam, r, 0, z, 0.06f, 0.75f, juce::Colour(0xfff0fbff), 0.3f, 0.22f, 4.3f);
    scene.lightColour = juce::Colour(0xffeef8ff);
}

void office(juce::Graphics& g, Scene& scene, Camera& cam, float, float, juce::Random& random) {
    cam.vy *= 0.94f;
    Room r;
    r.halfWidth = 14.0f; r.height = 2.8f; r.length = 60.0f; r.fogDistance = 26.0f;
    r.fog = juce::Colour(0xff1e2420); r.ceiling = juce::Colour(0xffb4b8ac); r.wallUpper = juce::Colour(0xff8a9088); r.wallLower = juce::Colour(0xff6a7068);
    r.floor = juce::Colour(0xff4c5458); r.wainscot = 1.0f; r.ambient = 0.55f; r.lightReach = 1.4f;
    for (float z = 1.2f; z < r.length; z += 2.4f) r.lightZ.push_back(z);
    shell(g, cam, r);
    // Ceiling grid.
    for (float z = 0.6f; z < r.length; z += 0.6f) {
        g.setColour(juce::Colours::black.withAlpha(0.1f * (1 - r.fogAt(z))));
        line(g, cam.at(-r.halfWidth, r.height, z), cam.at(r.halfWidth, r.height, z), 1.0f);
    }
    for (float x = -r.halfWidth; x <= r.halfWidth; x += 0.6f) {
        g.setColour(juce::Colours::black.withAlpha(0.06f));
        line(g, cam.at(x, r.height, 0), cam.at(x, r.height, r.length), 1.0f);
    }
    // Cubicles: the same bay, again and again.
    for (float z = r.length - 3.0f; z > 1.0f; z -= 3.0f)
        for (float x = -12.0f; x <= 12.0f; x += 3.0f) {
            if (std::abs(x) < 1.0f) continue;   // the walkway
            const juce::Colour fabric(0xff6a7470);
            box(g, cam, r, x - 1.4f, x + 1.4f, 0, 1.35f, z, z + 0.06f, fabric);
            if (random.nextFloat() < 0.3f) {   // a dead monitor on a desk
                g.setColour(mix(juce::Colour(0xff101412), r.fog, r.fogAt(z + 1.0f)));
                const float xm = x - 0.5f + random.nextFloat();
                quad(g, cam.at(xm - 0.25f, 0.8f, z + 1.2f), cam.at(xm - 0.25f, 1.2f, z + 1.2f), cam.at(xm + 0.25f, 1.2f, z + 1.2f), cam.at(xm + 0.25f, 0.8f, z + 1.2f));
            }
        }
    for (const float z : r.lightZ)
        for (float x = -9.6f; x <= 9.6f; x += 4.8f) ceilingLight(g, scene, cam, r, x, z, 0.3f, 0.3f, juce::Colour(0xfff0f8e8), 0.22f, 0.0f);
    scene.lightColour = juce::Colour(0xfff0f8ea);
}

void impossible(juce::Graphics& g, Scene& scene, Camera& cam, float w, float h, juce::Random& random) {
    // A corridor that should not fit: tilted, doors on the ceiling, frames inside frames.
    g.addTransform(juce::AffineTransform::rotation(0.07f, w * 0.5f, h * 0.45f));
    cam.vx *= 1.06f;
    Room r;
    r.halfWidth = 1.4f; r.height = 3.2f; r.length = 24.0f; r.fogDistance = 12.0f;
    r.fog = juce::Colour(0xff1c1228); r.ceiling = juce::Colour(0xff9a8cb0); r.wallUpper = juce::Colour(0xffb8a8c8); r.wallLower = juce::Colour(0xff6a5a84);
    r.floor = juce::Colour(0xff8478a0); r.wainscot = 1.0f; r.ambient = 0.45f;
    for (float z = 2.0f; z < r.length; z += 4.0f) r.lightZ.push_back(z);
    shell(g, cam, r);
    endWall(g, cam, r, juce::Colour(0xff201830));
    // Checkerboard floor.
    for (float z = 0.0f; z < r.length; z += 0.7f)
        for (float x = -r.halfWidth; x < r.halfWidth - 0.01f; x += 0.7f)
            if ((int(std::lround(z / 0.7f)) + int(std::lround((x + r.halfWidth) / 0.7f))) % 2 == 0) {
                g.setColour(juce::Colours::black.withAlpha(0.18f * (1 - r.fogAt(z))));
                quad(g, cam.at(x, 0.001f, z), cam.at(x + 0.7f, 0.001f, z), cam.at(x + 0.7f, 0.001f, z + 0.7f), cam.at(x, 0.001f, z + 0.7f));
            }
    // Door frames receding inside each other, each a little turned.
    for (int k = 8; k >= 1; --k) {
        const float z = float(k) * 2.6f;
        g.setColour(mix(juce::Colour(0xffe8d8ff), r.fog, r.fogAt(z)).withAlpha(0.6f));
        juce::Path frame;
        frame.startNewSubPath(cam.at(-0.9f, 0, z));
        frame.lineTo(cam.at(-0.9f, 2.4f, z));
        frame.lineTo(cam.at(0.9f, 2.4f, z));
        frame.lineTo(cam.at(0.9f, 0, z));
        g.strokePath(frame, juce::PathStrokeType(std::max(1.0f, 6.0f / (z + cam.nearZ))), juce::AffineTransform::rotation(0.05f * float(k % 3 - 1), cam.vx, cam.vy));
    }
    // Doors on the ceiling and stairs into a wall.
    for (float z = 3.0f; z < r.length - 3; z += 5.5f) {
        g.setColour(mix(juce::Colour(0xff3a2c50), r.fog, r.fogAt(z)));
        quad(g, cam.at(-0.45f, r.height - 0.001f, z), cam.at(0.45f, r.height - 0.001f, z), cam.at(0.45f, r.height - 0.001f, z + 2.0f), cam.at(-0.45f, r.height - 0.001f, z + 2.0f));
    }
    for (int s = 0; s < 9; ++s) {
        const float z = 5.0f + 0.3f * float(s), y = 0.2f * float(s + 1);
        box(g, cam, r, 0.4f, r.halfWidth - 0.01f, 0, y, z, z + 0.3f, juce::Colour(0xff9080a8));
    }
    juce::ignoreUnused(random);
    for (const float z : r.lightZ) ceilingLight(g, scene, cam, r, 0, z, 0.3f, 0.3f, juce::Colour(0xfff0d8ff), 0.45f, 0.18f);
    scene.lightColour = juce::Colour(0xffe8c8ff);
}

using Builder = void (*)(juce::Graphics&, Scene&, Camera&, float, float, juce::Random&);
constexpr Builder builders[] { hospital, mall, school, garage, pool, hotel, subway, supermarket, office, impossible };

// The photographic finish, in place: halation around bright areas and film grain.
void develop(juce::Image& image, unsigned seed) {
    const int pw = image.getWidth(), ph = image.getHeight();
    const auto small = image.rescaled(std::max(1, pw / 10), std::max(1, ph / 10), juce::Graphics::mediumResamplingQuality);
    const auto bloom = small.rescaled(pw, ph, juce::Graphics::mediumResamplingQuality);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readWrite);
    juce::Image::BitmapData glow(bloom, juce::Image::BitmapData::readOnly);
    std::uint32_t state = 0x9E3779B9u ^ (seed * 2654435761u);
    for (int y = 0; y < ph; ++y) {
        auto* row = data.getLinePointer(y);
        const auto* grow = glow.getLinePointer(y);
        for (int x = 0; x < pw; ++x) {
            auto* p = row + x * data.pixelStride;
            const auto* b = grow + x * glow.pixelStride;
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            const int n = int(state % 15u) - 7;
            for (int c = 0; c < 3; ++c) {
                const int bright = std::max(0, int(b[c]) - 150);
                p[c] = juce::uint8(std::clamp(int(p[c]) + bright * 6 / 10 + n, 0, 255));
            }
        }
    }
}

} // namespace

juce::File photographFolder() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Veil Audio").getChildFile("LIMINAL").getChildFile("Scenes");
}

Scene renderPlace(int place, int character, int width, int height, float scale, bool quick) {
    Scene scene;
    scene.place = std::clamp(place, 0, 9);
    scene.character = std::clamp(character, 0, 2);
    const int pw = std::max(1, juce::roundToInt(float(width) * scale)), ph = std::max(1, juce::roundToInt(float(height) * scale));
    scene.image = juce::Image(juce::Image::RGB, pw, ph, true);
    const float w = float(width), h = float(height);
    const auto& ch = dsp::character(scene.place, scene.character);
    juce::Random random { juce::int64(1979 + scene.place * 97) };
    {
        juce::Graphics g(scene.image);
        g.addTransform(juce::AffineTransform::scale(scale));
        // The developer's own photograph of the place, if there is one; else the drawing.
        const auto photo = juce::ImageFileFormat::loadFrom(photographFolder().getChildFile(juce::String(scene.place + 1) + ".png"));
        Camera cam { w * 0.5f, h * 0.47f, w * 0.27f, 1.6f };
        if (photo.isValid()) {
            g.drawImage(photo, juce::Rectangle<float>(0, 0, w, h), juce::RectanglePlacement::fillDestination);
        } else {
            juce::Graphics::ScopedSaveState save(g);
            g.fillAll(juce::Colours::black);
            builders[scene.place](g, scene, cam, w, h, random);
        }
        // The Character: its grade over everything, its light level.
        if (ch.grade != 0) {
            g.setColour(juce::Colour(ch.grade));
            g.fillRect(0.0f, 0.0f, w, h);
        }
        if (ch.light < 1.0f) {
            g.setColour(juce::Colours::black.withAlpha(std::clamp(1.0f - ch.light, 0.0f, 0.7f)));
            g.fillRect(0.0f, 0.0f, w, h);
        } else if (ch.light > 1.0f) {
            g.setColour(scene.lightColour.withAlpha(std::clamp((ch.light - 1.0f) * 0.25f, 0.0f, 0.2f)));
            g.fillRect(0.0f, 0.0f, w, h);
        }
        // Haze toward the vanishing point, then the vignette.
        g.setGradientFill(juce::ColourGradient(scene.lightColour.withAlpha(0.08f), { w * 0.5f, h * 0.46f }, scene.lightColour.withAlpha(0.0f), { w * 0.95f, h * 0.46f }, true));
        g.fillRect(0.0f, 0.0f, w, h);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x00000000), { w * 0.5f, h * 0.48f }, juce::Colour(0xc0000000), { 0, 0 }, true));
        g.fillRect(0.0f, 0.0f, w, h);
    }
    if (!quick) develop(scene.image, unsigned(7 + scene.place * 3 + scene.character));
    // Frosted glass for panels: the scene shrunk (a cheap wide blur) and darkened.
    {
        const auto small = scene.image.rescaled(std::max(1, pw / 16), std::max(1, ph / 16), juce::Graphics::mediumResamplingQuality);
        scene.frosted = juce::Image(juce::Image::RGB, pw, ph, true);
        juce::Graphics g(scene.frosted);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(small, 0, 0, pw, ph, 0, 0, small.getWidth(), small.getHeight());
        g.fillAll(juce::Colour(0x8c0a0e0d));
    }
    // Nearest lights first (the animation favours them).
    std::stable_sort(scene.lights.begin(), scene.lights.end(), [](const Light& a, const Light& b) { return a.size > b.size; });
    return scene;
}

} // namespace liminal::ui
