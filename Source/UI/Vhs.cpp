#include "UI/Vhs.h"
#include <cmath>

namespace liminal::ui::vhs {

namespace {
constexpr float pi = 3.14159265f;

// A running-sum box blur along a row, edges clamped (cost independent of the radius).
void box(const float* in, float* out, int n, int r) {
    if (r <= 0 || n <= 1) {
        std::copy(in, in + n, out);
        return;
    }
    const float norm = 1.0f / float(2 * r + 1);
    float acc = in[0] * float(r + 1);
    for (int i = 1; i <= r; ++i) acc += in[std::min(i, n - 1)];
    for (int i = 0; i < n; ++i) {
        out[i] = acc * norm;
        acc += in[std::min(i + r + 1, n - 1)] - in[std::max(i - r, 0)];
    }
}
// The tape's smear to the right: a one-pole low-pass run left to right.
void trail(float* x, int n, float keep) {
    float s = x[0];
    for (int i = 0; i < n; ++i) {
        s = x[i] + keep * (s - x[i]);
        x[i] = s;
    }
}
// Linear interpolation into a quarter-resolution row at full-resolution position `x`.
inline float quarter(const float* c, int n, float x) {
    const float f = std::clamp(x * 0.25f - 0.375f, 0.0f, float(n - 1));
    const int i = std::min(int(f), n - 2 < 0 ? 0 : n - 2);
    const float t = f - float(i);
    return n < 2 ? c[0] : c[i] + t * (c[i + 1] - c[i]);
}

// Four tiling grain textures (luma noise, a little stretched sideways like tape noise; light and
// dark specks, premultiplied), made once.
const std::array<juce::Image, 4>& grainTiles() {
    static const std::array<juce::Image, 4> tiles = [] {
        std::array<juce::Image, 4> out;
        constexpr int size = 256, coarse = size / 2;
        for (int t = 0; t < 4; ++t) {
            juce::Image image(juce::Image::ARGB, size, size, true);
            juce::Random r(7717 + t * 131);
            std::vector<float> row(coarse);
            juce::Image::BitmapData d(image, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < size; ++y) {
                for (auto& c : row) c = r.nextFloat() * 2.0f - 1.0f;
                const float lineGain = 0.8f + 0.4f * r.nextFloat();   // each line a little different (line noise)
                for (int x = 0; x < size; ++x) {
                    const float c0 = row[std::size_t(x / 2)], c1 = row[std::size_t((x / 2 + 1) % coarse)];
                    const float smooth = c0 + (c1 - c0) * float(x % 2) * 0.5f;
                    const float n = std::clamp((0.65f * smooth + 0.35f * (r.nextFloat() * 2.0f - 1.0f)) * lineGain, -1.0f, 1.0f);
                    // Noise (light or dark specks) over the scanline (every other row a dark line;
                    // drawn at the tape's opacity that is a 6-8 % dip, aligned to screen pixels).
                    const float a = 0.8f * std::min(1.0f, std::pow(std::abs(n), 1.5f));
                    const float line = (y & 1) != 0 ? 1.0f : 0.0f;
                    const float outA = a + line * (1.0f - a);
                    const auto premultiplied = juce::uint8(n > 0 ? std::lround(255.0f * a) : 0);
                    *reinterpret_cast<juce::PixelARGB*>(d.getPixelPointer(x, y)) = juce::PixelARGB(juce::uint8(std::lround(255.0f * outA)), premultiplied, premultiplied, premultiplied);
                }
            }
            out[std::size_t(t)] = image;
        }
        return out;
    }();
    return tiles;
}
} // namespace

float wrongness(int place, int character) {
    // Per place, the three Characters (docs/DEVLOG.md lists why): the unsettling ones glitch more.
    static constexpr float table[10][3] {
        { 0.0f, 0.1f, 1.0f },    // hospital: Sterile Silence, After Hours, SOMETHING WRONG
        { 0.0f, 0.1f, 0.35f },   // mall: Closing Time, Forgotten, Endless
        { 0.0f, 0.1f, 0.9f },    // school: After Class, Childhood Memory, NIGHTMARE
        { 0.0f, 0.25f, 0.6f },   // garage: Vacant, Distant Movement, PURSUIT
        { 0.0f, 0.05f, 0.2f },   // pool: Still Water, Forgotten Summer, Submerged
        { 0.0f, 0.05f, 0.9f },   // hotel: Late Night, Faded Luxury, ROOM 404
        { 0.05f, 0.2f, 0.4f },   // subway: Last Train, Abandoned, Approaching
        { 0.0f, 0.1f, 0.8f },    // supermarket: Closing Shift, Nostalgia, FLUORESCENT NIGHTMARE
        { 0.05f, 0.3f, 0.5f },   // office: After Hours, Dream State, Infinite Cubicles
        { 0.3f, 0.85f, 1.0f },   // impossible: Familiar, UNSTABLE, BEYOND REALITY
    };
    if (place < 0 || place > 9) return 0.0f;
    return table[place][std::clamp(character, 0, 2)];
}

//==============================================================================
void Baker::bake(const juce::uint8* src, int width, int height, const Static& st, juce::Image::BitmapData& out) {
    const int w = width, h = height;
    if (w <= 1 || h <= 1) return;
    const float A = std::clamp(st.amount, 0.0f, 1.0f), wrong = std::clamp(st.wrong, 0.0f, 1.0f);
    const float s = float(w) / 1440.0f;   // pixel sizes are tuned for the 1440-wide scenes
    const int cw = (w + 3) / 4;           // chroma (and bloom) at a quarter of the width
    for (auto* vec : { &y, &u, &v, &a, &b }) vec->resize(std::size_t(w));
    for (auto* vec : { &cu, &cv, &pu, &pv, &hl, &ph }) vec->assign(std::size_t(cw + 1), 0.0f);

    // Faint barrel curvature: the corners stay put, the middle bulges ~4 % at Heavy. The source of
    // each pixel and its edge vignette are worked out once per size and amount.
    const float k = 0.045f * A, edge = 0.10f + 0.12f * A;
    if (mapW != w || mapH != h || mapK != k || mapEdge != edge) {
        mapW = w;
        mapH = h;
        mapK = k;
        mapEdge = edge;
        map.resize(std::size_t(w) * std::size_t(h));
        gain.resize(std::size_t(w) * std::size_t(h));
        const float norm = 1.0f / (1.0f + k), cx = float(w - 1) * 0.5f, cy = float(h - 1) * 0.5f;
        for (int row = 0; row < h; ++row) {
            const float yn = (float(row) - cy) / cy;
            for (int x = 0; x < w; ++x) {
                const float xn = (float(x) - cx) / cx;
                const float r2 = 0.5f * (xn * xn + yn * yn);
                const float f = (1.0f + k * r2) * norm;
                const int sx = std::clamp(int(cx + (float(x) - cx) * f + 0.5f), 0, w - 1);
                const int sy = std::clamp(int(cy + (float(row) - cy) * f + 0.5f), 0, h - 1);
                map[std::size_t(row) * std::size_t(w) + std::size_t(x)] = (sy * w + sx) * 3;
                gain[std::size_t(row) * std::size_t(w) + std::size_t(x)] = 1.0f - edge * r2 * r2;
            }
        }
    }
    // SD tape: luma low-passed to roughly 240 TV lines (a box twice), a trailing smear and the
    // deck's edge enhancement (a soft halo on edges).
    const int lumaR = std::max(1, int(std::lround((1.4f + 1.6f * A) * s)));
    const int wideR = lumaR * 4;
    const float sharpen = 0.18f + 0.17f * A, keepY = 0.4f + 0.22f * A;
    // Chroma: a sixth or less of the luma's bandwidth, smeared to the right, offset from the picture.
    const int chromaR = std::max(1, int(std::lround((2.0f + 1.6f * A) * s)));
    const float keepC = std::min(0.9f, 0.5f + 0.2f * A + 0.12f * wrong);
    const float chromaShift = (3.0f + 5.0f * A + 6.0f * wrong) * s;
    const int misreg = std::max(1, int(std::lround((1.0f + 1.6f * A + 2.0f * wrong) * s)));   // red vs cyan
    const float redBoost = 1.22f + 0.22f * A, otherSat = 0.93f - 0.05f * A, blueSat = 0.86f - 0.06f * A, warmSat = 1.04f;
    const float cast = st.drift * (3.5f + 5.0f * A);   // + magenta, - green
    // Highlights bloom (sideways, like tape) and clip a little; blacks lift to a milky grey.
    const float bloomFrom = 178.0f, bloomGain = 0.4f + 0.4f * A;
    const int bloomR = std::max(1, int(std::lround((3.0f + 3.0f * A) * s)));
    std::array<std::array<juce::uint8, 512>, 3> lut {};
    {
        const float lift = 9.0f + 13.0f * A, knee = 200.0f, ceiling = 251.0f - 7.0f * A;
        const float tint[3] { 0.0f, 0.5f, 1.5f };   // milky blacks lean a little blue-grey
        for (int c = 0; c < 3; ++c)
            for (int i = 0; i < 512; ++i) {
                const float x = float(i - 64);
                float o = lift + tint[c] + x * (255.0f - lift) / 255.0f;
                if (o > knee) o = knee + (ceiling - knee) * std::tanh((o - knee) / (ceiling - knee));
                lut[std::size_t(c)][std::size_t(i)] = juce::uint8(std::clamp(o + 0.5f, 0.0f, 255.0f));
            }
    }
    // Where each full-width pixel reads the quarter-width chroma (shifted) and bloom rows.
    ci.resize(std::size_t(w));
    ct.resize(std::size_t(w));
    bi.resize(std::size_t(w));
    bt.resize(std::size_t(w));
    for (int x = 0; x < w; ++x) {
        const auto at = [&](float pos, int& i, float& t) {
            const float f = std::clamp(pos * 0.25f - 0.375f, 0.0f, float(cw - 1));
            i = std::min(int(f), cw - 1);
            t = f - float(i);
        };
        at(float(x) - chromaShift, ci[std::size_t(x)], ct[std::size_t(x)]);
        at(float(x), bi[std::size_t(x)], bt[std::size_t(x)]);
    }
    const bool argb = out.pixelFormat == juce::Image::ARGB;
    const int iR = argb ? juce::PixelARGB::indexR : juce::PixelRGB::indexR, iG = argb ? juce::PixelARGB::indexG : juce::PixelRGB::indexG,
              iB = argb ? juce::PixelARGB::indexB : juce::PixelRGB::indexB;
    float* Y = y.data();
    float* U = u.data();
    float* V = v.data();
    float* ta = a.data();
    float* tb = b.data();
    float* CU = cu.data();
    float* CV = cv.data();
    float* HL = hl.data();
    for (int row = 0; row < h; ++row) {
        // 1. Gather through the curvature: luma and colour difference, with the edge vignette.
        const int* m = map.data() + std::size_t(row) * std::size_t(w);
        const float* gn = gain.data() + std::size_t(row) * std::size_t(w);
        for (int x = 0; x < w; ++x) {
            const juce::uint8* p = src + m[x];
            const float R = p[0], G = p[1], B = p[2];
            const float l = 0.299f * R + 0.587f * G + 0.114f * B;
            Y[x] = l * gn[x];
            U[x] = (B - l) * gn[x];
            V[x] = (R - l) * gn[x];
        }
        // 2. Luma: soft, the deck's halo, the smear.
        box(Y, ta, w, lumaR);
        box(ta, tb, w, lumaR);   // tb: the soft luma
        box(tb, ta, w, wideR);   // ta: much softer, for the halo
        for (int x = 0; x < w; ++x) Y[x] = tb[x] + sharpen * (tb[x] - ta[x]);
        trail(Y, w, keepY);
        // 3. Bloom from the highlights, at a quarter of the width, carried a little down the frame.
        for (int i = 0; i < cw; ++i) {
            const int x0 = std::min(w - 4, i * 4);
            HL[i] = std::max(0.0f, 0.25f * (tb[x0] + tb[x0 + 1] + tb[x0 + 2] + tb[x0 + 3]) - bloomFrom);
        }
        box(HL, ta, cw, bloomR);
        box(ta, HL, cw, bloomR);
        for (int i = 0; i < cw; ++i) {
            ph[std::size_t(i)] = 0.55f * ph[std::size_t(i)] + 0.45f * HL[i];
            HL[i] = ph[std::size_t(i)] * bloomGain;
        }
        HL[cw] = HL[cw - 1];
        // 4. Chroma: a quarter of the width, the tape's saturation (reds pushed), the cast, smeared
        // right, averaged with the line above (the deck's comb), shifted off the picture.
        for (int i = 0; i < cw; ++i) {
            const int x0 = std::min(w - 4, i * 4);
            float su = 0.25f * (U[x0] + U[x0 + 1] + U[x0 + 2] + U[x0 + 3]);
            float sv = 0.25f * (V[x0] + V[x0 + 1] + V[x0 + 2] + V[x0 + 3]);
            sv *= sv > 0 ? redBoost : otherSat;   // reds pushed (the tape's famous bleeding red), cyans a little less
            su *= su > 0 ? blueSat : warmSat;     // blues fade, yellows and oranges keep their warmth
            CU[i] = su + cast * 0.8f;
            CV[i] = sv + cast;
        }
        box(CU, ta, cw, chromaR);
        box(ta, CU, cw, chromaR);
        box(CV, ta, cw, chromaR);
        box(ta, CV, cw, chromaR);
        trail(CU, cw, keepC);
        trail(CV, cw, keepC);
        for (int i = 0; i < cw; ++i) {
            const float nu = CU[i], nv = CV[i];
            CU[i] = row == 0 ? nu : 0.5f * (nu + pu[std::size_t(i)]);
            CV[i] = row == 0 ? nv : 0.5f * (nv + pv[std::size_t(i)]);
            pu[std::size_t(i)] = nu;
            pv[std::size_t(i)] = nv;
        }
        CU[cw] = CU[cw - 1];
        CV[cw] = CV[cw - 1];
        // 5. Back to RGB (red from slightly misregistered luma), through the tape's curve.
        auto* line = out.getLinePointer(row);
        for (int x = 0; x < w; ++x) {
            const int c0 = ci[std::size_t(x)], b0 = bi[std::size_t(x)];
            const float tc = ct[std::size_t(x)], tbl = bt[std::size_t(x)];
            const float bl = HL[b0] + tbl * (HL[b0 + 1] - HL[b0]);
            const float cU = CU[c0] + tc * (CU[c0 + 1] - CU[c0]), cV = CV[c0] + tc * (CV[c0 + 1] - CV[c0]);
            const float yg = Y[x] + bl, yr = Y[x >= misreg ? x - misreg : 0] + bl;
            const int R = int(yr + cV) + 64, Bv = int(yg + cU) + 64, G = int(yg - 0.509f * cV - 0.194f * cU) + 64;
            auto* px = line + x * out.pixelStride;
            px[iR] = lut[0][std::size_t(std::clamp(R, 0, 511))];
            px[iG] = lut[1][std::size_t(std::clamp(G, 0, 511))];
            px[iB] = lut[2][std::size_t(std::clamp(Bv, 0, 511))];
            if (argb) px[juce::PixelARGB::indexA] = 255;
        }
    }
}

void bakeImage(juce::Image& image, const Static& st) {
    if (!image.isValid() || st.amount <= 0.0f) return;
    const int w = image.getWidth(), h = image.getHeight();
    std::vector<juce::uint8> rgb(std::size_t(w) * std::size_t(h) * 3);
    {
        const juce::Image::BitmapData d(image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto c = d.getPixelColour(x, y);
                auto* o = rgb.data() + (std::size_t(y) * std::size_t(w) + std::size_t(x)) * 3;
                o[0] = c.getRed();
                o[1] = c.getGreen();
                o[2] = c.getBlue();
            }
    }
    juce::Image::BitmapData d(image, juce::Image::BitmapData::writeOnly);
    Baker baker;
    baker.bake(rgb.data(), w, h, st, d);
}

//==============================================================================
void Tape::reset() {
    now = 0;
    nextFrame = 0;
    rollStart = -1;
    nextRoll = 6.0 + 8.0 * random.nextDouble();   // not straight away
    nextTear = 4.0 + 6.0 * random.nextDouble();
    tearUntil = -1;
    driftPhase1 = random.nextDouble() * 6.2832;
    driftPhase2 = random.nextDouble() * 6.2832;
    trackStrength = 0;
    tearY = -1;
    warble = 0;
    lastAmount = lastWrong = -1;
}

// An irregular wait: exponentially distributed over a minimum, so glitches never come in a rhythm
// and never close together.
double Tape::gap(double mean, double minimum) {
    const double r = std::max(1.0e-4, random.nextDouble());
    return minimum - std::log(r) * std::max(0.0, mean - minimum);
}

bool Tape::advance(double dt, float amount, float wrong, double fps, bool calm) {
    dt = std::clamp(dt, 0.0, 0.25);
    now += dt;
    if (amount <= 0.0f) {
        trackStrength = 0;
        tearY = -1;
        warble = 0;
        rollStart = -1;
        return false;
    }
    wrong = std::clamp(wrong, 0.0f, 1.0f);
    // Tracking rolls: every ~26 s on average at Light, ~13 s at Heavy, up to 4x as often when it
    // is wrong; never closer than a third of that (and never under 2.5 s).
    const double meanRoll = (amount >= 0.99f ? 13.0 : 26.0) / (1.0 + 2.0 * double(wrong));
    const double minRoll = std::max(2.0, 0.3 * meanRoll);
    if (lastWrong >= 0 && rollStart < 0 && (wrong > lastWrong + 0.2f || amount > lastAmount + 0.2f))   // it just got worse: soon, not in a minute
        nextRoll = std::min(nextRoll, now + gap(0.5 * meanRoll, 1.5));
    lastAmount = amount;
    lastWrong = wrong;
    if (rollStart < 0 && now >= nextRoll && !calm) {
        rollStart = now;
        rollLength = 0.9 + 1.3 * random.nextDouble();
        const bool up = random.nextBool();
        rollFrom = up ? 1.08f : -0.08f;
        rollTo = up ? -0.08f : 1.08f;
        rollHeight = (0.045f + 0.06f * random.nextFloat()) * (0.7f + 0.3f * amount);
        rollPower = (0.55f + 0.45f * amount) * (1.0f + 0.6f * wrong);
        trackSeed = unsigned(random.nextInt());
        ++rolls;
    }
    if (rollStart >= 0) {
        const double t = (now - rollStart) / rollLength;
        if (t >= 1.0) {
            rollStart = -1;
            trackStrength = 0;
            nextRoll = now + gap(meanRoll, minRoll);
        } else {
            const float e = std::sin(pi * float(t));
            const float eased = float(t * t * (3.0 - 2.0 * t));
            trackStrength = rollPower * std::pow(e, 0.6f);
            trackY = rollFrom + (rollTo - rollFrom) * eased;
            trackH = rollHeight;
        }
    }
    // Frame tears: only when something is wrong (Unreal or the Character); brief (2-4 frames),
    // ~every 14 s at the wrongest, never within 3 s of the last.
    if (wrong > 0.05f) {
        if (tearUntil < 0 && now >= nextTear && !calm) {
            tearUntil = now + double(2 + random.nextInt(3)) / 24.0;
            tearY = 0.12f + 0.76f * random.nextFloat();
            tearShift = (random.nextBool() ? 1.0f : -1.0f) * (12.0f + 40.0f * random.nextFloat()) * (0.5f + 0.5f * wrong);
            ++tears;
        }
        if (tearUntil >= 0 && now >= tearUntil) {
            tearUntil = -1;
            tearY = -1;
            nextTear = now + gap(14.0 / double(wrong), 3.0);
        }
    } else {
        tearY = -1;
        tearUntil = -1;
        nextTear = std::max(nextTear, now + 4.0);
    }
    // The colour cast drifts slowly between green and magenta (faster and wider when wrong).
    const double speed = 1.0 + 0.5 * double(wrong);
    driftPhase1 += dt * speed * 6.2832 / 37.0;
    driftPhase2 += dt * speed * 6.2832 / 15.3;
    drift = std::clamp((0.35f + 0.35f * amount) * (1.0f + 0.5f * wrong) * float(0.65 * std::sin(driftPhase1) + 0.35 * std::sin(driftPhase2)), -1.0f, 1.0f);
    // The audio warble follows only the heavy glitches.
    const bool heavy = amount >= 0.99f || wrong >= 0.5f;
    warble = heavy ? std::clamp(std::max(trackStrength / std::max(0.1f, rollPower), tearY >= 0 ? 1.0f : 0.0f), 0.0f, 1.0f) : 0.0f;

    if (now < nextFrame) return false;
    nextFrame = now - nextFrame > 1.0 / fps ? now + 1.0 / fps : nextFrame + 1.0 / fps;
    ++frames;
    grainFrame = (grainFrame + 1 + random.nextInt(3)) % 4;
    grainOffset = { random.nextInt(256), random.nextInt(256) };
    jitter = !calm && random.nextFloat() < 0.02f + 0.05f * amount + 0.08f * wrong ? (random.nextBool() ? 1.0f : -1.0f) * (random.nextFloat() < wrong ? 2.0f : 1.0f) : 0.0f;
    wobbleCount = calm ? 0 : random.nextFloat() < 0.25f + 0.35f * amount ? 1 + random.nextInt(amount > 0.9f ? 3 : 2) : 0;
    for (int i = 0; i < wobbleCount; ++i)
        wobble[std::size_t(i)] = { random.nextFloat(), (1.5f + 4.0f * random.nextFloat()) / 564.0f, (random.nextBool() ? 1.0f : -1.0f) * (0.8f + 1.8f * random.nextFloat() * amount) };
    // Dropouts and coloured specks: a Poisson-like number per frame (mean from a rate per second).
    const auto howMany = [&](float perSecond, int most) { return std::min(most, int(perSecond / float(fps) + random.nextFloat())); };
    dropoutCount = howMany((1.0f + 4.5f * amount * amount) * (1.0f + 0.5f * wrong), int(dropouts.size()));
    for (int i = 0; i < dropoutCount; ++i) {
        const float len = 6.0f + 70.0f * random.nextFloat() * random.nextFloat();
        dropouts[std::size_t(i)] = { random.nextFloat() * 960.0f, random.nextFloat(), len, 0.45f + 0.45f * random.nextFloat(), 0xfff4f6ff };
    }
    static constexpr juce::uint32 colours[] { 0xffff3a3a, 0xff3aff6a, 0xff4a6aff, 0xffff3aff, 0xff3affff, 0xffffe03a };
    speckleCount = howMany((1.5f + 6.0f * amount * amount) * (1.0f + 0.5f * wrong), int(speckle.size()));
    for (int i = 0; i < speckleCount; ++i)
        speckle[std::size_t(i)] = { random.nextFloat() * 960.0f, random.nextFloat(), 1.5f + 3.0f * random.nextFloat(), 0.35f + 0.4f * random.nextFloat(), colours[random.nextInt(6)] };
    headShift = (8.0f + 20.0f * random.nextFloat()) * (0.6f + 0.4f * amount);
    headSeed = unsigned(random.nextInt());
    return true;
}

//==============================================================================
namespace {
constexpr int grainMargin = 64;   // logical pixels of slack for the per-frame offset
}

void Grain::prepare(int width, int height, float scale, float amount) {
    if (width == w && height == h && std::abs(scale - s) < 0.01f && std::abs(amount - a) < 0.001f && ready()) return;
    w = width;
    h = height;
    s = scale;
    a = amount;
    const int pw = juce::roundToInt(float(width + grainMargin) * scale), ph = juce::roundToInt(float(height + grainMargin) * scale);
    const float opacity = 0.035f + 0.045f * amount;
    std::uint32_t state = 0x2545F491u;
    const auto next = [&] {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return float(state & 0xffffu) * (2.0f / 65535.0f) - 1.0f;
    };
    const int step = std::max(1, juce::roundToInt(2.0f * scale));   // the noise a little stretched sideways, like tape noise
    std::vector<float> coarse(std::size_t(pw / step + 2));
    for (auto& image : frames) {
        image = juce::Image(juce::Image::ARGB, pw, ph, false);
        juce::Image::BitmapData d(image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < ph; ++y) {
            for (auto& c : coarse) c = next();
            const float lineGain = 0.8f + 0.2f * (next() + 1.0f);
            const bool scan = (int(float(y) / scale) & 1) != 0;   // every other (logical) line a dark scanline
            auto* px = reinterpret_cast<juce::PixelARGB*>(d.getLinePointer(y));
            for (int x = 0; x < pw; ++x) {
                const int i = x / step;
                const float t = float(x % step) / float(step);
                const float n = std::clamp((0.65f * (coarse[std::size_t(i)] + t * (coarse[std::size_t(i + 1)] - coarse[std::size_t(i)])) + 0.35f * next()) * lineGain, -1.0f, 1.0f);
                const float na = 0.8f * std::min(1.0f, std::pow(std::abs(n), 1.5f));
                const float alpha = opacity * (na + (scan ? 1.0f : 0.0f) * (1.0f - na));
                const auto white = juce::uint8(n > 0 ? std::lround(255.0f * opacity * na) : 0);
                px[x] = juce::PixelARGB(juce::uint8(std::lround(255.0f * alpha)), white, white, white);
            }
        }
    }
}

void Grain::draw(juce::Graphics& g, juce::Rectangle<int> scene, int frame, juce::Point<int> offset) const {
    if (!ready()) return;
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(scene);
    const int ox = offset.x % grainMargin, oy = (offset.y % grainMargin) & ~1;   // (even: the scanlines stay put)
    g.drawImageTransformed(frames[std::size_t(std::abs(frame) % 3)],
                           juce::AffineTransform::scale(1.0f / s).translated(float(scene.getX() - ox), float(scene.getY() - oy)));
}

void paint(juce::Graphics& g, const juce::Image& picture, juce::Rectangle<float> pb, juce::Rectangle<int> sceneInt, const Tape& tape, float amount, float wrong, const Grain* grain) {
    if (amount <= 0.0f) return;
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(sceneInt);
    const auto scene = sceneInt.toFloat();
    const float top = scene.getY(), H = scene.getHeight(), W = scene.getWidth(), unit = W / 960.0f;
    const auto& tiles = grainTiles();
    const auto shifted = [&](juce::Rectangle<float> band, float dx) {
        if (!picture.isValid() || band.getHeight() <= 0.0f) return;
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(band.getSmallestIntegerContainer());
        g.drawImage(picture, pb.translated(dx, 0), juce::RectanglePlacement::stretchToFit);
        if (std::abs(dx) > 2.5f * unit) {   // the line's blanking shows where the picture moved away
            g.setColour(juce::Colour(0xff050606));
            g.fillRect(dx > 0 ? juce::Rectangle<float>(scene.getX(), band.getY(), dx, band.getHeight())
                              : juce::Rectangle<float>(scene.getRight() + dx, band.getY(), -dx, band.getHeight()));
        }
    };
    // Lines nudged sideways.
    for (int i = 0; i < tape.wobbleCount; ++i) {
        const auto& w = tape.wobble[std::size_t(i)];
        shifted({ scene.getX(), top + w.y * H, W, std::max(1.0f, w.h * H) }, w.dx * unit);
    }
    // The tracking band rolling through: torn, noisy, a little brighter.
    if (tape.trackStrength > 0.01f) {
        const float strength = std::min(1.5f, tape.trackStrength);
        const float cy = top + tape.trackY * H, bh = std::max(6.0f, tape.trackH * H);
        juce::Random r(int(tape.trackSeed ^ unsigned(tape.frames * 2654435761u)));
        constexpr int parts = 8;
        for (int k = 0; k < parts; ++k) {
            const float envelope = std::sin(pi * (float(k) + 0.5f) / float(parts));
            const float dx = strength * envelope * (3.0f + 15.0f * r.nextFloat()) * (r.nextFloat() < 0.75f ? 1.0f : -0.5f) * unit;
            shifted({ scene.getX(), cy - bh * 0.5f + bh * float(k) / float(parts), W, bh / float(parts) + 0.5f }, dx);
        }
        const auto band = juce::Rectangle<float>(scene.getX(), cy - bh * 0.5f, W, bh);
        g.setColour(juce::Colours::white.withAlpha(0.05f * strength));
        g.fillRect(band);
        g.setTiledImageFill(tiles[std::size_t((tape.grainFrame + 2) % 4)], r.nextInt(256), r.nextInt(256), std::min(1.0f, 0.5f * strength));
        g.fillRect(band.reduced(0, bh * 0.15f));
        const int streaks = 2 + r.nextInt(4);
        for (int k = 0; k < streaks; ++k) {
            g.setColour(juce::Colours::white.withAlpha((0.25f + 0.4f * r.nextFloat()) * std::min(1.0f, strength)));
            g.fillRect(scene.getX() + r.nextFloat() * W * 0.8f, cy + (r.nextFloat() - 0.5f) * bh * 0.8f, (20.0f + 220.0f * r.nextFloat()) * unit, 1.0f);
        }
        if (wrong > 0.3f) {   // a wrong Character: the colour lurches inside the band
            g.setColour(juce::Colour(r.nextBool() ? 0xffff40d0 : 0xff40ff70).withAlpha(0.07f * strength * wrong));
            g.fillRect(band);
        }
    }
    // A frame tear: below a line, the picture jumps sideways (wrapping, with its blanking edge).
    if (tape.tearY >= 0.0f && picture.isValid()) {
        const float ty = top + tape.tearY * H;
        const auto below = juce::Rectangle<float>(scene.getX(), ty, W, top + H - ty);
        {
            juce::Graphics::ScopedSaveState s(g);
            g.reduceClipRegion(below.getSmallestIntegerContainer());
            const float dx = tape.tearShift * unit;
            g.drawImage(picture, pb.translated(dx, 0), juce::RectanglePlacement::stretchToFit);
            g.drawImage(picture, pb.translated(dx > 0 ? dx - pb.getWidth() : dx + pb.getWidth(), 0), juce::RectanglePlacement::stretchToFit);
            g.setColour(juce::Colour(0xd0050606));
            g.fillRect(dx > 0 ? juce::Rectangle<float>(scene.getX() + dx - 4.0f * unit, ty, 4.0f * unit, below.getHeight())
                              : juce::Rectangle<float>(scene.getRight() + dx, ty, 4.0f * unit, below.getHeight()));
        }
        g.setColour(juce::Colour(0x8c050606));
        g.fillRect(scene.getX(), ty - 1.0f, W, 2.5f);
    }
    // Fine moving luma grain over everything (a pre-made tile, a new one and offset each frame).
    if (grain != nullptr && grain->ready()) grain->draw(g, sceneInt, tape.grainFrame, tape.grainOffset);
    else {
        g.setTiledImageFill(tiles[std::size_t(tape.grainFrame)], tape.grainOffset.x, tape.grainOffset.y & ~1, 0.035f + 0.045f * amount);   // (even rows: the scanlines stay put)
        g.fillRect(scene);
    }
    // The head-switching noise along the bottom of the frame.
    {
        const float hh = (5.0f + 5.0f * amount) * unit;
        const auto head = juce::Rectangle<float>(scene.getX(), top + H - hh, W, hh);
        juce::Random r(int(tape.headSeed));
        shifted(head.withHeight(hh * 0.45f), tape.headShift * unit);
        shifted(head.withTrimmedTop(hh * 0.45f), tape.headShift * (1.4f + 0.6f * r.nextFloat()) * unit);
        g.setTiledImageFill(tiles[std::size_t((tape.grainFrame + 1) % 4)], r.nextInt(256), r.nextInt(256), 0.6f);
        g.fillRect(head);
        g.setColour(juce::Colours::white.withAlpha(0.12f + 0.12f * r.nextFloat()));
        g.fillRect(head.getX(), head.getY(), W, 1.0f);
        g.setColour(juce::Colour(0x60000000));
        g.fillRect(head.getX(), head.getBottom() - 1.5f * unit, W, 1.5f * unit);
    }
    // Dropouts (short white streaks) and coloured specks.
    for (int i = 0; i < tape.dropoutCount; ++i) {
        const auto& d = tape.dropouts[std::size_t(i)];
        const float yy = top + d.y * (H - 12.0f * unit);
        g.setColour(juce::Colour(d.colour).withAlpha(d.alpha * 0.35f));
        g.fillRect(d.x * unit - 2.0f, yy - 0.6f, d.length * unit + 4.0f, 2.2f);
        g.setColour(juce::Colour(d.colour).withAlpha(d.alpha));
        g.fillRect(d.x * unit, yy, d.length * unit, 1.0f);
    }
    for (int i = 0; i < tape.speckleCount; ++i) {
        const auto& d = tape.speckle[std::size_t(i)];
        g.setColour(juce::Colour(d.colour).withAlpha(d.alpha));
        g.fillRect(d.x * unit, top + d.y * (H - 12.0f * unit), d.length * unit, 1.2f);
    }
}

//==============================================================================
bool resize(const juce::Image& src, juce::Image& dst) {
    if (!src.isValid() || !dst.isValid() || src.getFormat() != juce::Image::ARGB || dst.getFormat() != juce::Image::ARGB) return false;
    const juce::Image::BitmapData s(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData d(dst, juce::Image::BitmapData::writeOnly);
    if (s.pixelStride != 4 || d.pixelStride != 4) return false;
    const int sw = src.getWidth(), sh = src.getHeight(), dw = dst.getWidth(), dh = dst.getHeight();
    // Two channels at a time (0x00ff00ff lanes), 8-bit weights: channel order does not matter.
    const auto lerp = [](std::uint32_t a, std::uint32_t b, std::uint32_t f) {
        const std::uint32_t g = 256 - f;
        const std::uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
        const std::uint32_t ag = (((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) & 0xff00ff00u;
        return rb | ag;
    };
    std::vector<int> x0(std::size_t(dw), 0);
    std::vector<std::uint32_t> fx(std::size_t(dw), 0);
    for (int x = 0; x < dw; ++x) {
        const float sx = std::clamp((float(x) + 0.5f) * float(sw) / float(dw) - 0.5f, 0.0f, float(sw - 1));
        x0[std::size_t(x)] = std::min(int(sx), sw - 2 < 0 ? 0 : sw - 2);
        fx[std::size_t(x)] = std::uint32_t(std::lround((sx - float(x0[std::size_t(x)])) * 256.0f));
    }
    std::vector<std::uint32_t> rowA(static_cast<std::size_t>(dw)), rowB(static_cast<std::size_t>(dw));
    int haveA = -1, haveB = -1;
    const auto horizontal = [&](int y, std::vector<std::uint32_t>& out) {
        const auto* line = reinterpret_cast<const std::uint32_t*>(s.getLinePointer(y));
        for (int x = 0; x < dw; ++x) {
            const int i = x0[std::size_t(x)];
            out[std::size_t(x)] = sw < 2 ? line[0] : lerp(line[i], line[i + 1], fx[std::size_t(x)]);
        }
    };
    for (int y = 0; y < dh; ++y) {
        const float sy = std::clamp((float(y) + 0.5f) * float(sh) / float(dh) - 0.5f, 0.0f, float(sh - 1));
        const int y0 = std::min(int(sy), std::max(0, sh - 2)), y1 = std::min(sh - 1, y0 + 1);
        const auto fy = std::uint32_t(std::lround((sy - float(y0)) * 256.0f));
        if (haveA != y0) {
            if (haveB == y0) {
                std::swap(rowA, rowB);
                std::swap(haveA, haveB);
            } else {
                horizontal(y0, rowA);
                haveA = y0;
            }
        }
        if (haveB != y1) {
            horizontal(y1, rowB);
            haveB = y1;
        }
        auto* out = reinterpret_cast<std::uint32_t*>(d.getLinePointer(y));
        for (int x = 0; x < dw; ++x) out[x] = lerp(rowA[std::size_t(x)], rowB[std::size_t(x)], fy) | 0xff000000u;
    }
    return true;
}

//==============================================================================
namespace {
struct Glyph { char c; std::array<juce::uint8, 7> rows; bool narrow = false; };
// A 5 x 7 pixel font drawn for LIMINAL (bit 4 = the leftmost column).
const Glyph* glyphFor(char c) {
    static const Glyph font[] {
        { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
        { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
        { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
        { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
        { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
        { 'A', { 0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
        { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
        { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
        { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
        { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
        { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
        { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { 'N', { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
        { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
        { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } }, { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
        { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
        { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
        { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } }, { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
        { 'Y', { 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 } }, { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
        { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 }, true }, { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C }, true },
        { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } }, { '/', { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } },
        { '>', { 0x10, 0x18, 0x1C, 0x1E, 0x1C, 0x18, 0x10 } }, { '*', { 0x00, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E, 0x00 } },
    };
    for (const auto& g : font)
        if (g.c == c) return &g;
    return nullptr;   // (a space, or anything not in the font)
}
float advanceOf(char c) {
    const auto* g = glyphFor(c);
    return g == nullptr ? 4.0f : g->narrow ? 3.0f : 6.0f;
}
} // namespace

float osdWidth(const juce::String& text, float px) {
    float w = 0;
    for (const auto ch : text.toUpperCase()) w += advanceOf(char(ch));
    return std::max(0.0f, w - 1.0f) * px;
}

juce::Image osdText(const juce::String& text, juce::Colour colour, float px, float scale) {
    const auto upper = text.toUpperCase();
    const float margin = 2.0f * px;
    const int iw = std::max(1, int(std::ceil((osdWidth(upper, px) + 2.0f * margin) * scale)));
    const int ih = std::max(1, int(std::ceil((7.0f * px + 2.0f * margin) * scale)));
    juce::Image image(juce::Image::ARGB, iw, ih, true);
    juce::Graphics g(image);
    g.addTransform(juce::AffineTransform::scale(scale));
    // Every font pixel's rectangle, each glyph nudged a little off the line (cheap OSD chips).
    std::vector<juce::Rectangle<float>> cells;
    float x = margin;
    int index = 0;
    static constexpr float wobble[] { 0.0f, 0.0f, 0.5f, 0.0f, -0.5f, 0.0f, 0.0f, 0.5f };
    for (const auto ch : upper) {
        if (const auto* glyph = glyphFor(char(ch))) {
            const float dy = wobble[index % 8] * px, dx = glyph->narrow ? -px : 0.0f;
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if ((glyph->rows[std::size_t(row)] >> (4 - col)) & 1) cells.emplace_back(x + dx + float(col) * px, margin + dy + float(row) * px, px, px);
        }
        x += advanceOf(char(ch)) * px;
        ++index;
    }
    const auto pass = [&](juce::Colour c, float grow, float dx) {
        g.setColour(c);
        for (const auto& r : cells) g.fillRect(r.expanded(grow).translated(dx, 0));
    };
    pass(colour.withAlpha(0.07f), px * 0.9f, 0);          // the glow
    pass(colour.withAlpha(0.14f), px * 0.4f, 0);
    pass(juce::Colour(0xffff3c64).withAlpha(0.28f), 0.0f, px * 0.55f);   // a colour fringe, as the tape smears it
    pass(juce::Colour(0xff3cf0ff).withAlpha(0.16f), 0.0f, -px * 0.45f);
    g.setColour(colour.withMultipliedAlpha(0.95f));
    for (const auto& r : cells) g.fillRect(r.withSizeKeepingCentre(px * 0.94f, px * 0.94f));
    return image;
}

} // namespace liminal::ui::vhs
