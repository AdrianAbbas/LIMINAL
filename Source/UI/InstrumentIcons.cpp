#include "UI/InstrumentIcons.h"
#include <cmath>

namespace liminal::ui {

namespace {
using juce::Path;

// A violin's body (upright, centred on x = 0.5, from y0 to y1), with its C-bouts.
void violinBody(Path& p, float cx, float y0, float y1, float w) {
    const float h = y1 - y0;
    const auto y = [&](float t) { return y0 + h * t; };
    p.startNewSubPath(cx, y(0.0f));
    p.cubicTo(cx - w * 0.9f, y(0.0f), cx - w * 1.0f, y(0.32f), cx - w * 0.55f, y(0.42f));
    p.cubicTo(cx - w * 0.42f, y(0.48f), cx - w * 0.42f, y(0.54f), cx - w * 0.6f, y(0.58f));
    p.cubicTo(cx - w * 1.15f, y(0.68f), cx - w * 1.0f, y(1.0f), cx, y(1.0f));
    p.cubicTo(cx + w * 1.0f, y(1.0f), cx + w * 1.15f, y(0.68f), cx + w * 0.6f, y(0.58f));
    p.cubicTo(cx + w * 0.42f, y(0.54f), cx + w * 0.42f, y(0.48f), cx + w * 0.55f, y(0.42f));
    p.cubicTo(cx + w * 1.0f, y(0.32f), cx + w * 0.9f, y(0.0f), cx, y(0.0f));
    p.closeSubPath();
}

void keysRow(Path& p, float x0, float x1, float y0, float y1, int keys) {
    p.addRectangle(x0, y0, x1 - x0, y1 - y0);
    for (int k = 1; k < keys; ++k) {
        const float x = x0 + (x1 - x0) * float(k) / float(keys);
        p.startNewSubPath(x, y0);
        p.lineTo(x, y1);
    }
}

void arc(Path& p, float cx, float cy, float r, float from, float to) { p.addCentredArc(cx, cy, r, r, 0.0f, from, to, true); }

// Builds icon `i` in a unit square: strokes in `s`, small filled marks in `f`, dashed strokes in `d`.
void build(int i, Path& s, Path& f, Path& d) {
    constexpr float pi = 3.14159265f;
    switch (i) {
    case 0: {   // Forgotten Piano: an upright, its keyboard and lid
        s.addRoundedRectangle(0.18f, 0.12f, 0.64f, 0.74f, 0.03f);
        s.startNewSubPath(0.12f, 0.52f);
        s.lineTo(0.88f, 0.52f);
        s.addRectangle(0.18f, 0.52f, 0.64f, 0.13f);
        for (float x : { 0.29f, 0.40f, 0.56f, 0.67f }) f.addRectangle(x, 0.52f, 0.045f, 0.075f);
        s.startNewSubPath(0.24f, 0.86f); s.lineTo(0.24f, 0.93f);
        s.startNewSubPath(0.76f, 0.86f); s.lineTo(0.76f, 0.93f);
        s.startNewSubPath(0.30f, 0.26f); s.lineTo(0.70f, 0.26f);
        break;
    }
    case 1: {   // Faded Synth: a small keyboard synthesizer with knobs and a slider
        s.addRoundedRectangle(0.06f, 0.30f, 0.88f, 0.44f, 0.05f);
        for (float x : { 0.19f, 0.33f, 0.47f }) s.addEllipse(x - 0.05f, 0.36f, 0.10f, 0.10f);
        s.startNewSubPath(0.62f, 0.41f); s.lineTo(0.86f, 0.41f);
        f.addRectangle(0.70f, 0.375f, 0.035f, 0.07f);
        keysRow(s, 0.12f, 0.88f, 0.54f, 0.68f, 7);
        break;
    }
    case 2: {   // Glass Memories: a wine glass
        s.startNewSubPath(0.30f, 0.10f);
        s.quadraticTo(0.28f, 0.56f, 0.50f, 0.56f);
        s.quadraticTo(0.72f, 0.56f, 0.70f, 0.10f);
        s.startNewSubPath(0.30f, 0.10f); s.lineTo(0.70f, 0.10f);
        s.startNewSubPath(0.50f, 0.56f); s.lineTo(0.50f, 0.86f);
        s.startNewSubPath(0.34f, 0.88f); s.lineTo(0.66f, 0.88f);
        s.startNewSubPath(0.36f, 0.28f); s.quadraticTo(0.37f, 0.42f, 0.45f, 0.46f);
        break;
    }
    case 3: {   // Empty Choir: three singers, mouths open
        const float xs[] { 0.20f, 0.50f, 0.80f }, ys[] { 0.44f, 0.36f, 0.44f };
        for (int k = 0; k < 3; ++k) {
            s.addEllipse(xs[k] - 0.13f, ys[k] - 0.15f, 0.26f, 0.30f);
            f.addEllipse(xs[k] - 0.035f, ys[k] + 0.03f, 0.07f, 0.08f);
            arc(s, xs[k], ys[k] + 0.50f, 0.17f, -0.42f * pi, 0.42f * pi);
        }
        break;
    }
    case 4: {   // Lost Music Box: a box, its lid open, the crank
        s.addRectangle(0.12f, 0.46f, 0.58f, 0.38f);
        s.startNewSubPath(0.12f, 0.46f); s.lineTo(0.30f, 0.20f); s.lineTo(0.88f, 0.20f); s.lineTo(0.70f, 0.46f);
        s.startNewSubPath(0.12f, 0.58f); s.lineTo(0.70f, 0.58f);
        s.startNewSubPath(0.70f, 0.70f); s.lineTo(0.86f, 0.70f); s.lineTo(0.86f, 0.58f);
        f.addEllipse(0.835f, 0.53f, 0.06f, 0.06f);
        break;
    }
    case 5: {   // Hollow Drone: long waves, stacked
        for (int k = 0; k < 3; ++k) {
            const float y = 0.30f + 0.20f * float(k);
            s.startNewSubPath(0.08f, y);
            for (float x = 0.0f; x <= 1.0001f; x += 0.05f) s.lineTo(0.08f + 0.84f * x, y + std::sin(x * 6.2832f * (1.0f + 0.5f * float(k))) * 0.06f);
        }
        break;
    }
    case 6: {   // Old Guitar: a classical guitar, tilted, its sound hole and head
        Path g;
        g.startNewSubPath(0.50f, 0.30f);
        g.cubicTo(0.34f, 0.30f, 0.32f, 0.46f, 0.40f, 0.53f);
        g.cubicTo(0.25f, 0.59f, 0.26f, 0.90f, 0.50f, 0.90f);
        g.cubicTo(0.74f, 0.90f, 0.75f, 0.59f, 0.60f, 0.53f);
        g.cubicTo(0.68f, 0.46f, 0.66f, 0.30f, 0.50f, 0.30f);
        g.closeSubPath();
        g.addEllipse(0.445f, 0.555f, 0.11f, 0.11f);
        g.startNewSubPath(0.47f, 0.30f); g.lineTo(0.47f, 0.10f);
        g.startNewSubPath(0.53f, 0.30f); g.lineTo(0.53f, 0.10f);
        g.addRoundedRectangle(0.44f, 0.02f, 0.12f, 0.09f, 0.02f);
        g.startNewSubPath(0.44f, 0.80f); g.lineTo(0.56f, 0.80f);
        g.applyTransform(juce::AffineTransform::rotation(0.55f, 0.5f, 0.5f));
        s.addPath(g);
        break;
    }
    case 7: {   // Faded Strings: a violin and its bow across it
        Path v;
        violinBody(v, 0.42f, 0.30f, 0.90f, 0.20f);
        v.startNewSubPath(0.42f, 0.30f); v.lineTo(0.42f, 0.12f);
        v.addEllipse(0.39f, 0.04f, 0.07f, 0.07f);
        v.startNewSubPath(0.36f, 0.56f); v.lineTo(0.35f, 0.66f);
        v.startNewSubPath(0.48f, 0.56f); v.lineTo(0.49f, 0.66f);
        s.addPath(v);
        s.startNewSubPath(0.10f, 0.40f); s.lineTo(0.94f, 0.78f);
        s.startNewSubPath(0.12f, 0.45f); s.lineTo(0.90f, 0.80f);
        f.addEllipse(0.07f, 0.37f, 0.06f, 0.06f);
        break;
    }
    case 8: {   // Hallway Pizzicato: a violin plucked, its notes echoing away
        Path v;
        violinBody(v, 0.34f, 0.30f, 0.90f, 0.18f);
        v.startNewSubPath(0.34f, 0.30f); v.lineTo(0.34f, 0.12f);
        v.addEllipse(0.31f, 0.04f, 0.07f, 0.07f);
        v.startNewSubPath(0.29f, 0.57f); v.lineTo(0.28f, 0.66f);
        v.startNewSubPath(0.39f, 0.57f); v.lineTo(0.40f, 0.66f);
        s.addPath(v);
        arc(s, 0.52f, 0.56f, 0.13f, 0.22f * pi, 0.78f * pi);
        arc(s, 0.52f, 0.56f, 0.25f, 0.27f * pi, 0.73f * pi);
        d.addCentredArc(0.52f, 0.56f, 0.37f, 0.37f, 0.0f, 0.31f * pi, 0.69f * pi, true);
        break;
    }
    case 9: {   // Toy Piano: a small rounded toy on legs, one key out of line
        s.startNewSubPath(0.18f, 0.66f);
        s.lineTo(0.18f, 0.36f);
        s.quadraticTo(0.18f, 0.20f, 0.36f, 0.20f);
        s.lineTo(0.64f, 0.20f);
        s.quadraticTo(0.82f, 0.20f, 0.82f, 0.36f);
        s.lineTo(0.82f, 0.66f);
        s.closeSubPath();
        s.startNewSubPath(0.18f, 0.48f); s.lineTo(0.82f, 0.48f);
        for (float x : { 0.31f, 0.44f, 0.69f }) { s.startNewSubPath(x, 0.48f); s.lineTo(x, 0.66f); }
        s.addRectangle(0.56f, 0.52f, 0.09f, 0.18f);   // the broken key
        s.startNewSubPath(0.26f, 0.66f); s.lineTo(0.22f, 0.90f);
        s.startNewSubPath(0.74f, 0.66f); s.lineTo(0.78f, 0.90f);
        f.addEllipse(0.46f, 0.29f, 0.08f, 0.08f);
        break;
    }
    case 10: {   // Lounge E-Piano: a slim electric piano on an X stand
        s.addRoundedRectangle(0.06f, 0.34f, 0.88f, 0.18f, 0.03f);
        for (int k = 1; k < 9; ++k) { const float x = 0.06f + 0.88f * float(k) / 9.0f; s.startNewSubPath(x, 0.42f); s.lineTo(x, 0.52f); }
        s.startNewSubPath(0.12f, 0.42f); s.lineTo(0.88f, 0.42f);
        s.startNewSubPath(0.26f, 0.52f); s.lineTo(0.74f, 0.90f);
        s.startNewSubPath(0.74f, 0.52f); s.lineTo(0.26f, 0.90f);
        break;
    }
    case 11: {   // Elevator Vibraphone: bars getting shorter, two mallets
        const float heights[] { 0.56f, 0.49f, 0.42f, 0.36f, 0.30f };
        for (int k = 0; k < 5; ++k) s.addRoundedRectangle(0.08f + 0.115f * float(k), 0.16f, 0.085f, heights[k], 0.02f);
        s.startNewSubPath(0.74f, 0.92f); s.lineTo(0.86f, 0.44f);
        s.startNewSubPath(0.62f, 0.92f); s.lineTo(0.92f, 0.56f);
        s.addEllipse(0.82f, 0.36f, 0.09f, 0.09f);
        s.addEllipse(0.89f, 0.49f, 0.09f, 0.09f);
        break;
    }
    case 12: {   // Mall Chimes: a public-address horn, two tones leaving it
        s.addRectangle(0.10f, 0.42f, 0.14f, 0.16f);
        s.startNewSubPath(0.24f, 0.44f); s.lineTo(0.56f, 0.26f);
        s.startNewSubPath(0.24f, 0.56f); s.lineTo(0.56f, 0.74f);
        s.addEllipse(0.52f, 0.26f, 0.08f, 0.48f);
        arc(s, 0.62f, 0.50f, 0.14f, 0.22f * pi, 0.78f * pi);
        arc(s, 0.62f, 0.50f, 0.26f, 0.26f * pi, 0.74f * pi);
        s.startNewSubPath(0.17f, 0.58f); s.lineTo(0.17f, 0.88f);
        break;
    }
    case 13: {   // Solo Voice: one singer, far off, a single line of sound
        s.addEllipse(0.30f, 0.16f, 0.30f, 0.34f);
        f.addEllipse(0.42f, 0.36f, 0.07f, 0.08f);
        arc(s, 0.45f, 0.96f, 0.26f, -0.45f * pi, 0.45f * pi);
        arc(s, 0.62f, 0.40f, 0.14f, 0.28f * pi, 0.72f * pi);
        break;
    }
    case 14: {   // Hummed Memory: a face half there (dashed), lips closed, a hum leaving it
        d.addEllipse(0.16f, 0.18f, 0.34f, 0.40f);
        s.startNewSubPath(0.28f, 0.46f); s.lineTo(0.38f, 0.46f);
        s.startNewSubPath(0.52f, 0.44f);
        for (float x = 0.0f; x <= 1.0001f; x += 0.05f) s.lineTo(0.52f + 0.38f * x, 0.44f + std::sin(x * 6.2832f * 1.5f) * 0.05f * (1.0f - 0.5f * x));
        arc(s, 0.33f, 1.02f, 0.26f, -0.42f * pi, 0.42f * pi);
        break;
    }
    case 15: {   // Cassette Strings: a cassette
        s.addRoundedRectangle(0.06f, 0.24f, 0.88f, 0.52f, 0.04f);
        s.addEllipse(0.25f, 0.38f, 0.14f, 0.14f);
        s.addEllipse(0.61f, 0.38f, 0.14f, 0.14f);
        s.addRectangle(0.36f, 0.40f, 0.28f, 0.10f);
        s.startNewSubPath(0.24f, 0.76f); s.lineTo(0.30f, 0.62f); s.lineTo(0.70f, 0.62f); s.lineTo(0.76f, 0.76f);
        break;
    }
    case 16: {   // Arcade Lead: a game pad, its cross and two buttons
        s.startNewSubPath(0.22f, 0.34f);
        s.lineTo(0.78f, 0.34f);
        s.quadraticTo(0.96f, 0.36f, 0.94f, 0.62f);
        s.quadraticTo(0.92f, 0.78f, 0.78f, 0.74f);
        s.lineTo(0.22f, 0.74f);
        s.quadraticTo(0.08f, 0.78f, 0.06f, 0.62f);
        s.quadraticTo(0.04f, 0.36f, 0.22f, 0.34f);
        s.closeSubPath();
        f.addRectangle(0.21f, 0.51f, 0.16f, 0.05f);
        f.addRectangle(0.265f, 0.455f, 0.05f, 0.16f);
        s.addEllipse(0.62f, 0.50f, 0.08f, 0.08f);
        s.addEllipse(0.74f, 0.42f, 0.08f, 0.08f);
        s.startNewSubPath(0.50f, 0.34f); s.cubicTo(0.50f, 0.20f, 0.62f, 0.22f, 0.62f, 0.10f);
        break;
    }
    case 17: {   // Basement Bass: a bass clef above a floor
        s.startNewSubPath(0.30f, 0.30f);
        s.cubicTo(0.30f, 0.12f, 0.66f, 0.10f, 0.66f, 0.34f);
        s.cubicTo(0.66f, 0.54f, 0.48f, 0.66f, 0.24f, 0.70f);
        f.addEllipse(0.24f, 0.25f, 0.11f, 0.11f);
        f.addEllipse(0.74f, 0.22f, 0.07f, 0.07f);
        f.addEllipse(0.74f, 0.38f, 0.07f, 0.07f);
        s.startNewSubPath(0.08f, 0.82f); s.lineTo(0.92f, 0.82f);
        for (float x : { 0.16f, 0.36f, 0.56f, 0.76f }) { s.startNewSubPath(x, 0.82f); s.lineTo(x + 0.08f, 0.92f); }
        break;
    }
    case 18: {   // Warped Organ: drawbars over a keyboard that has bent
        const float tops[] { 0.36f, 0.24f, 0.42f, 0.18f, 0.30f };
        for (int k = 0; k < 5; ++k) {
            const float x = 0.20f + 0.15f * float(k);
            s.startNewSubPath(x, 0.10f); s.lineTo(x, tops[k]);
            f.addRectangle(x - 0.04f, tops[k], 0.08f, 0.07f);
        }
        s.startNewSubPath(0.08f, 0.62f);
        for (float x = 0.0f; x <= 1.0001f; x += 0.05f) s.lineTo(0.08f + 0.84f * x, 0.62f + std::sin(x * 3.1416f) * -0.05f);
        s.lineTo(0.92f, 0.86f); s.lineTo(0.08f, 0.86f); s.closeSubPath();
        for (int k = 1; k < 7; ++k) { const float x = 0.08f + 0.84f * float(k) / 7.0f; s.startNewSubPath(x, 0.62f - std::sin(float(k) / 7.0f * 3.1416f) * 0.05f + 0.05f); s.lineTo(x, 0.86f); }
        break;
    }
    default: {   // Bowed Glass: a glass bowl, its rim, a bow drawn across it
        s.addEllipse(0.20f, 0.34f, 0.60f, 0.12f);
        s.startNewSubPath(0.20f, 0.40f);
        s.cubicTo(0.20f, 0.72f, 0.32f, 0.86f, 0.50f, 0.86f);
        s.cubicTo(0.68f, 0.86f, 0.80f, 0.72f, 0.80f, 0.40f);
        s.startNewSubPath(0.06f, 0.20f); s.lineTo(0.94f, 0.48f);
        s.startNewSubPath(0.07f, 0.25f); s.lineTo(0.90f, 0.51f);
        f.addRectangle(0.04f, 0.16f, 0.07f, 0.08f);
        d.addCentredArc(0.50f, 0.62f, 0.42f, 0.42f, 0.0f, 0.62f * pi, 0.82f * pi, true);
        d.addCentredArc(0.50f, 0.62f, 0.42f, 0.42f, 0.0f, -0.82f * pi, -0.62f * pi, true);
        break;
    }
    }
}
} // namespace

void drawInstrumentIcon(juce::Graphics& g, int instrument, juce::Rectangle<float> area, float weight) {
    const float side = std::min(area.getWidth(), area.getHeight());
    if (side <= 1.0f) return;
    const auto box = area.withSizeKeepingCentre(side, side);
    Path s, f, d;
    build(std::clamp(instrument, 0, 19), s, f, d);
    const auto t = juce::AffineTransform::scale(side).translated(box.getX(), box.getY());
    s.applyTransform(t);
    f.applyTransform(t);
    d.applyTransform(t);
    const float stroke = std::max(1.0f, side * 0.045f) * weight;
    const juce::PathStrokeType type(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.strokePath(s, type);
    g.fillPath(f);
    if (!d.isEmpty()) {
        const float dash[] { stroke * 2.2f, stroke * 1.8f };
        Path dashed;
        type.createDashedStroke(dashed, d, dash, 2);
        g.fillPath(dashed);
    }
}

} // namespace liminal::ui
