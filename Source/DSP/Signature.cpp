#include "DSP/Signature.h"
#include "DSP/Instrument.h"
#include <cmath>

namespace liminal::dsp {

namespace {
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }
// Delay swing (samples) for a sinusoidal pitch wobble of `cents` at `hz`.
float swingFor(float cents, float hz, double sr) {
    return cents <= 0.0f ? 0.0f : float(cents * 0.693147 / (1200.0 * 6.283185 * std::max(0.02f, hz)) * sr);
}
float advance(float& phase, float hz, double sr) {
    phase += float(hz / sr);
    if (phase >= 1.0f) phase -= 1.0f;
    return sinCycle(phase);
}
Stereo panOf(float p) { return panGains(p); }

// Each place's three Characters: how they vary the signature (docs/ENVIRONMENT_RESEARCH.md).
//                                       depth spread tone extra voices mirror
const Signature::Settings table[placeCount][3] {
    { { 1.0f, 1.0f, 1.15f, 0.0f },  { 0.8f, 1.35f, 0.8f, 0.0f },  { 1.2f, 0.9f, 1.0f, 1.0f } },             // hospital: Sterile Silence / After Hours (wider, softer) / Something Wrong (the period wanders)
    { { 0.8f, 0.65f, 1.2f, 0.2f },  { 1.0f, 1.0f, 0.75f, 0.8f },  { 1.2f, 1.6f, 0.7f, 0.3f } },             // mall: Closing Time (a nearer wing) / Forgotten (a worn speaker) / Endless (furthest)
    { { 0.7f, 1.0f, 1.0f, 0.6f },   { 1.2f, 1.0f, 0.7f, 0.3f },   { 1.8f, 0.6f, 0.9f, 1.0f } },             // school: After Class / Childhood Memory (worn, warm) / Nightmare (slow, deep wow; lockers ring)
    { { 1.0f, 1.0f, 1.0f, 0.4f },   { 0.75f, 1.4f, 0.7f, 0.3f },  { 1.25f, 0.85f, 1.1f, 0.9f, 8 } },        // garage: Vacant / Distant Movement (further, darker) / Pursuit (more slaps, more boom)
    { { 0.9f, 1.0f, 1.0f, 0.0f },   { 0.5f, 1.0f, 1.25f, 0.0f },  { 3.0f, 1.3f, 0.7f, 0.0f } },             // pool: Still Water / Forgotten Summer (above the water) / Submerged (mostly under it)
    { { 1.1f, 1.0f, 1.0f, 0.6f },   { 0.5f, 1.0f, 1.3f, 0.3f },   { 1.7f, 1.0f, 0.8f, 1.0f } },             // hotel: Late Night / Faded Luxury (a lighter wall) / Room 404 (only the wall, ringing)
    { { 1.0f, 1.0f, 1.0f, 0.8f },   { 0.75f, 1.5f, 0.5f, 0.1f },  { 1.15f, 0.8f, 1.1f, 0.4f, 0, true } },   // subway: Last Train (rails ringing) / Abandoned (much further, darker, dead rails) / Approaching (the other tunnel)
    { { 1.0f, 1.0f, 1.0f, 0.0f, 6 },{ 0.8f, 1.0f, 1.0f, 0.0f, 3 },{ 1.1f, 1.1f, 0.7f, 0.5f, 6 } },         // supermarket: (Characters 1-3 are Closing Shift, Nostalgia, Fluorescent Nightmare; see settingsFor)
    { { 1.0f, 1.0f, 1.0f, 0.0f },   { 0.6f, 1.0f, 0.5f, 0.0f },   { 1.8f, 1.0f, 1.2f, 1.0f } },             // office: After Hours / Dream State (soft) / Infinite Cubicles (the ballast stutters)
    { { 0.5f, 1.0f, 1.0f, 0.2f },   { 1.4f, 0.6f, 1.0f, 1.0f },   { 1.6f, 1.4f, 0.8f, 0.6f } },             // impossible: Familiar / Unstable (short reversed swells, freezes often) / Beyond Reality (reversed, long)
};
} // namespace

Signature::Settings Signature::settingsFor(Place p, int character) {
    auto s = table[std::clamp(int(p), 0, placeCount - 1)][std::clamp(character, 0, 2)];
    if (p == Place::supermarket) {   // Closing Shift: half the speakers off; Nostalgia: old narrow speakers; Fluorescent Nightmare: every speaker, harsh
        const Settings rows[3] { { 0.9f, 1.0f, 1.0f, 0.0f, 3 }, { 1.0f, 1.0f, 0.7f, 0.5f, 6 }, { 1.15f, 0.9f, 1.15f, 1.0f, 8 } };
        s = rows[std::clamp(character, 0, 2)];
    }
    return s;
}

float Signature::random01() {
    rngState = rngState * 1664525u + 1013904223u;
    return float(rngState >> 8) * (1.0f / 16777216.0f);
}

void Signature::prepare(double sampleRate) {
    sr = sampleRate;
    lineL.assign(std::size_t(1.7 * sr) + 8, 0.0f);
    lineR.assign(lineL.size(), 0.0f);
    combL.assign(std::size_t(0.06 * sr) + 8, 0.0f);
    combR.assign(combL.size(), 0.0f);
    revL.assign(std::size_t(2.0 * 0.75 * sr) + 8, 0.0f);
    revR.assign(revL.size(), 0.0f);
    freezeL.assign(std::size_t(0.25 * sr) + 8, 0.0f);
    freezeR.assign(freezeL.size(), 0.0f);
    const float apMs[4] { 7.1f, 11.3f, 17.9f, 23.7f };
    for (int k = 0; k < 4; ++k) ap[std::size_t(k)].assign(std::size_t(apMs[k] * 0.001 * sr) + 1, 0.0f);
    shiftA.prepare(sr);
    shiftB.prepare(sr);
    configure(where, 0, 1);
}

float Signature::read(const std::vector<float>& line, float delay) const {
    const int size = int(line.size());
    float pos = float(write) - std::clamp(delay, 1.0f, float(size - 3));
    if (pos < 0.0f) pos += float(size);
    const int i0 = int(pos);
    const float f = pos - float(i0);
    const float a = line[std::size_t(i0 % size)], b = line[std::size_t((i0 + 1) % size)];
    return a + f * (b - a);
}

void Signature::configure(Place place, int character, unsigned seed) {
    where = place;
    s = settingsFor(place, character);
    rngState = seed * 2654435761u + 12345u;
    for (auto& b : bq) b.identity();
    for (auto& o : op) o.setHz(20000.0f, sr);
    for (auto& g : resGain) g = 0;
    tapCount = 0;
    dryGain = 1;
    wetGain = 1;
    sendGain = 0;
    blend = 0;
    makeup = 1;
    combFb = 0;
    const float t = s.tone;
    const auto setTaps = [&](std::initializer_list<float> ms, std::initializer_list<float> gains, std::initializer_list<float> pans, float lpHz, float lpSlope) {
        tapCount = 0;
        auto g = gains.begin();
        auto p = pans.begin();
        for (const float m : ms) {
            if (tapCount >= int(taps.size()) || g == gains.end() || p == pans.end()) break;
            auto& tap = taps[std::size_t(tapCount)];
            tap = {};
            tap.ms = m * s.spread;
            tap.gain = *g++ * s.depth;
            tap.pan = s.mirror ? -*p++ : *p++;
            const float hz = lpHz / (1.0f + lpSlope * float(tapCount));
            tap.lpL.setHz(hz, sr);
            tap.lpR.setHz(hz, sr);
            ++tapCount;
        }
    };
    switch (place) {
    case Place::hospital: {
        // Parallel walls ~2.3 m apart: a repeat every 2 x 2.3 / 343 = 13.4 ms (a buzz at ~75 Hz),
        // high-passed in the loop (the walls are hard; the low end leaks out through them).
        const float width = 2.3f * s.spread;
        combDL = float(2.0 * width / 343.0 * sr);
        combDR = combDL * 1.065f;
        combFb = std::min(0.9f, 0.74f + 0.06f * s.depth);
        for (int c : { 0, 1 }) {
            bq[std::size_t(c)].highPass(170.0, 0.6, sr);            // thin low end
            bq[std::size_t(2 + c)].highShelf(3500.0, 3.5 * t, sr);   // the cold sheen
            bq[std::size_t(4 + c)].peak(2800.0, 1.0, 2.5, sr);
            op[std::size_t(c)].setHz(650.0f, sr);                    // into the flutter: only the upper part
            op[std::size_t(2 + c)].setHz(6500.0f * t, sr);           // in the loop: each pass a little duller
            op[std::size_t(4 + c)].setHz(450.0f, sr);                // and thinner
        }
        wetGain = 0.55f * s.depth;
        makeup = dbGain(1.5f);
        break;
    }
    case Place::mall: {
        // A 70 V ceiling speaker in another wing: ~380 Hz - 4 kHz, a cone resonance and break-up,
        // the transformer's grit; four speakers' sound arriving late; most of it into the atrium.
        bq[0].highPass(380.0 / t, 0.8, sr);
        bq[1].highPass(380.0 / t, 0.8, sr);
        bq[2].lowPass(4200.0 * t, 0.8, sr);
        bq[3].lowPass(4200.0 * t, 0.8, sr);
        bq[4].peak(1300.0, 1.6, 6.0, sr);
        bq[5].peak(3100.0 * t, 3.0, 4.0, sr);
        setTaps({ 95.0f, 137.0f, 188.0f, 251.0f }, { 0.55f, 0.45f, 0.36f, 0.28f }, { -0.7f, 0.5f, -0.2f, 0.8f }, 3200.0f * t, 0.25f);
        dryGain = std::clamp(0.3f - 0.25f * s.depth, 0.04f, 1.0f);   // heard only over the speakers (and their reverberation)
        wetGain = 1.0f;
        sendGain = 1.4f * s.spread;
        makeup = dbGain(9.0f);
        break;
    }
    case Place::school:
        for (int c : { 0, 1 }) {
            bq[std::size_t(c)].peak(650.0, 1.3, 4.0, sr);                   // boxy
            bq[std::size_t(2 + c)].lowPass(7500.0 * t, 0.7, sr);
            op[std::size_t(c)].setHz(300.0f, sr);                            // into the lockers: no bass
        }
        {   // steel locker doors: thin panels with sparse, high-Q modes (approximate)
            const float hz[4] { 415.0f, 662.0f, 1096.0f, 1730.0f };
            for (int k = 0; k < 4; ++k) {
                res[std::size_t(k)].set(hz[k], 28.0f, sr);
                resGain[std::size_t(k)] = 0.06f * (0.4f + s.extra);
            }
        }
        makeup = dbGain(-2.0f);
        break;
    case Place::garage: {
        // Pillars and walls 4.6-30 m away: discrete slaps 27-177 ms, dark (concrete + distance),
        // no feedback; the 2.5 m ceiling's first mode, 343 / 5 = 69 Hz, booms.
        if (s.voices >= 8) setTaps({ 23.0f, 27.0f, 46.0f, 59.0f, 71.0f, 98.0f, 133.0f, 177.0f }, { 0.5f, 0.45f, 0.42f, 0.38f, 0.36f, 0.3f, 0.24f, 0.18f },
                                   { 0.3f, -0.6f, 0.6f, -0.2f, -0.4f, 0.7f, -0.7f, 0.4f }, 2600.0f * t, 0.12f);
        else setTaps({ 27.0f, 46.0f, 71.0f, 98.0f, 133.0f, 177.0f }, { 0.5f, 0.42f, 0.36f, 0.3f, 0.24f, 0.18f }, { -0.6f, 0.6f, -0.4f, 0.7f, -0.7f, 0.4f }, 2600.0f * t, 0.12f);
        for (int c : { 0, 1 }) {
            bq[std::size_t(c)].lowPass(6000.0 * t, 0.7, sr);   // the dark direct sound
            bq[std::size_t(2 + c)].highPass(120.0, 0.7, sr);   // the slaps: no low rumble
        }
        res[0].set(69.0f, 6.0f, sr);
        res[1].set(104.0f, 5.0f, sr);
        resGain[0] = 0.55f * (0.5f + s.extra);
        resGain[1] = 0.3f * (0.5f + s.extra);
        op[0].setHz(300.0f, sr);   // what excites the boom
        makeup = dbGain(-2.0f);
        break;
    }
    case Place::pool:
        // Under the surface (the ear at the water line): air-water transmission loses the highs
        // first; a steep 24 dB/oct low-pass, slow wobble of pitch and level.
        for (int c : { 0, 1 }) {
            bq[std::size_t(2 + 2 * c)].lowPass(520.0 * t, 0.75, sr);
            bq[std::size_t(3 + 2 * c)].lowPass(520.0 * t, 0.65, sr);
        }
        blend = std::clamp(0.25f * s.depth, 0.0f, 0.8f);
        wetGain = dbGain(7.0f);
        break;
    case Place::hotel:
        // The partition between rooms: the mass-air-mass resonance (~95 Hz) passes, everything
        // above it is cut by the mass law (and steeper), a faint ring near the coincidence dip.
        for (int c : { 0, 1 }) {
            bq[std::size_t(c)].peak(95.0, 2.0, 5.0 + 4.0 * s.extra, sr);
            bq[std::size_t(2 + c)].lowPass(300.0 * t, 0.7, sr);
            bq[std::size_t(4 + c)].lowPass(1400.0 * t, 0.5, sr);
            bq[std::size_t(6 + c)].peak(2600.0, 4.0, 6.0, sr);
            bq[std::size_t(8 + c)].lowPass(6500.0, 0.7, sr);   // this room: warm, carpeted
            bq[std::size_t(10 + c)].peak(220.0, 0.9, 2.0, sr);
        }
        blend = std::clamp(0.5f * s.depth, 0.0f, 0.9f);
        wetGain = dbGain(9.0f);
        break;
    case Place::subway:
        // Echoes travelling away down the tunnel: each later one darker, quieter, further to one
        // side and drifting flat; the rails ring in them (a short comb, ~290 Hz).
        setTaps({ 110.0f, 240.0f, 395.0f, 580.0f, 800.0f, 1060.0f }, { 0.55f, 0.4f, 0.29f, 0.21f, 0.15f, 0.11f }, { 0.15f, 0.3f, 0.45f, 0.6f, 0.72f, 0.85f },
                4500.0f * t, 0.6f);
        for (int k = 0; k < tapCount; ++k) {
            auto& tap = taps[std::size_t(k)];
            tap.cents = 6.0f + 5.0f * float(k);
            tap.modHz = 0.06f + 0.013f * float(k);
            tap.phase = 0.17f * float(k);
            if (s.mirror) tap.gain *= 1.0f + 0.08f * float(tapCount - k);   // Approaching: the far ones louder
        }
        combDL = float(0.0034 * sr);
        combDR = float(0.0037 * sr);
        combFb = 0.45f + 0.3f * s.extra;
        makeup = dbGain(1.0f);
        for (int c : { 0, 1 }) {
            bq[std::size_t(c)].peak(2400.0, 2.0, 4.0, sr);
            bq[std::size_t(2 + c)].highPass(150.0, 0.7, sr);
        }
        break;
    case Place::supermarket: {
        // The ceiling grid: the same sound from several small speakers 2-16 m away, at different
        // positions: comb filtering, gloss; one bounce off the polished floor.
        if (s.voices <= 3) setTaps({ 6.0f, 21.0f, 38.0f }, { 0.5f, 0.4f, 0.29f }, { -0.8f, -0.3f, -0.6f }, 9000.0f * t, 0.05f);
        else if (s.voices >= 8) setTaps({ 6.0f, 13.5f, 21.0f, 29.0f, 38.0f, 47.0f, 56.0f, 66.0f }, { 0.5f, 0.46f, 0.4f, 0.34f, 0.29f, 0.24f, 0.2f, 0.17f },
                                        { -0.8f, 0.6f, -0.3f, 0.9f, -0.6f, 0.2f, 0.75f, -0.9f }, 9000.0f * t, 0.05f);
        else setTaps({ 6.0f, 13.5f, 21.0f, 29.0f, 38.0f, 47.0f }, { 0.5f, 0.46f, 0.4f, 0.34f, 0.29f, 0.24f }, { -0.8f, 0.6f, -0.3f, 0.9f, -0.6f, 0.2f }, 9000.0f * t, 0.05f);
        bq[0].highPass(160.0 / t, 0.7, sr);
        bq[1].lowPass(9000.0 * t, 0.7, sr);
        bq[2].peak(2000.0 * std::max(1.0f, t), 1.2, 3.0 + 3.0 * s.extra, sr);
        bq[3].highShelf(5000.0, 4.0, sr);
        bq[4].highShelf(5000.0, 4.0, sr);
        for (int k = 0; k < tapCount; ++k) {   // walking under the grid: the arrivals shift slowly (0.03-0.1 Hz), the comb sweeps
            taps[std::size_t(k)].modHz = 0.03f + 0.011f * float(k);
            taps[std::size_t(k)].phase = 0.29f * float(k);
        }
        dryGain = 0.72f;
        makeup = dbGain(-0.5f);
        break;
    }
    case Place::office:
        for (int c : { 0, 1 }) {
            bq[std::size_t(c)].lowPass(5200.0 * std::min(1.0f, t), 0.6, sr);   // carpet, tiles: dead
            bq[std::size_t(2 + c)].highPass(90.0, 0.7, sr);
        }
        op[0].setHz(900.0f * t, sr);   // the carrier, softened (Dream State: softer still)
        op[1].setHz(8.0f, sr);         // the instrument's envelope
        op[2].setHz(1000.0f, sr);      // the gritty part of the buzz (high-passed)
        stutterNext = int(sr * (6.0 + 10.0 * random01()));
        makeup = dbGain(-1.0f);
        break;
    case Place::impossible: {
        revBlock = std::clamp(int(0.5 * s.spread * sr), 64, int(revL.size() / 2) - 2);
        revPos = 0;
        shiftA.setSemitones(7.0f);
        shiftB.setSemitones(-5.0f);
        freezeLength = std::clamp(int(0.2 * sr), 64, int(freezeL.size()) - 2);
        freezeNext = int(sr * (8.0 + 12.0 * random01()));
        dryGain = 1.0f - 0.15f * std::min(1.0f, s.depth);
        makeup = dbGain(-0.5f);
        break;
    }
    }
    reset();
}

void Signature::reset() {
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    std::fill(combL.begin(), combL.end(), 0.0f);
    std::fill(combR.begin(), combR.end(), 0.0f);
    std::fill(revL.begin(), revL.end(), 0.0f);
    std::fill(revR.begin(), revR.end(), 0.0f);
    std::fill(freezeL.begin(), freezeL.end(), 0.0f);
    std::fill(freezeR.begin(), freezeR.end(), 0.0f);
    for (auto& a : ap) std::fill(a.begin(), a.end(), 0.0f);
    apPos = {};
    write = combWrite = 0;
    for (auto& b : bq) b.clear();
    for (auto& o : op) o.z = 0;
    for (auto& r : res) r.clear();
    for (auto& tap : taps) tap.lpL.z = tap.lpR.z = 0;
    phaseA = phaseB = phaseC = 0;
    wander = wanderTarget = 0;
    wanderClock = 0;
    freezeEnv = freezeTarget = 0;
    freezeHeld = 0;
    freezeClock = freezePos = 0;
    level = 0;
    stutter = 0;
    stutterClock = 0;
    shiftA.reset();
    shiftB.reset();
}

void Signature::process(const float* inL, const float* inR, float* outL, float* outR, float* sendL, float* sendR, int n) {
    const int size = int(lineL.size());
    const float fs = float(sr);
    for (int i = 0; i < n; ++i) {
        const float xl = inL[i], xr = inR[i];
        float l = xl, r = xr, sl = 0, sr_ = 0;
        switch (where) {
        case Place::hospital: {
            const float dl = bq[4].tick(bq[2].tick(bq[0].tick(xl))), dr = bq[5].tick(bq[3].tick(bq[1].tick(xr)));
            // The flutter: a feedback comb at the walls' round trip; Something Wrong lets the
            // period wander (the corridor will not keep its width).
            float dL = combDL, dR = combDR;
            if (s.extra > 0.0f) {
                const float m = 1.0f + 0.04f * s.extra * advance(phaseA, 0.13f, sr);
                dL *= m;
                dR *= m;
            }
            const int cs = int(combL.size());
            const auto combRead = [&](const std::vector<float>& line, float d) {
                float pos = float(combWrite) - d;
                if (pos < 0.0f) pos += float(cs);
                const int i0 = int(pos);
                const float f = pos - float(i0);
                return line[std::size_t(i0 % cs)] + f * (line[std::size_t((i0 + 1) % cs)] - line[std::size_t(i0 % cs)]);
            };
            const float yl = combRead(combL, dL), yr = combRead(combR, dR);
            combL[std::size_t(combWrite)] = op[0].hp(xl) + combFb * op[4].hp(op[2].lp(yl));
            combR[std::size_t(combWrite)] = op[1].hp(xr) + combFb * op[5].hp(op[3].lp(yr));
            if (++combWrite >= cs) combWrite = 0;
            l = dl + wetGain * yl;
            r = dr + wetGain * yr;
            break;
        }
        case Place::mall: {
            float pa = 0.5f * (xl + xr);
            pa = bq[5].tick(bq[4].tick(bq[3].tick(bq[2].tick(bq[1].tick(bq[0].tick(pa))))));
            const float g = 1.0f + 6.0f * s.extra;   // the transformer and a tired cone
            pa = softClip(pa * 4.0f * g + 0.05f * s.extra) / (4.0f * g) - softClip(0.05f * s.extra) / (4.0f * g);
            lineL[std::size_t(write)] = pa;
            float wl = 0, wr = 0;
            for (int k = 0; k < tapCount; ++k) {
                auto& tap = taps[std::size_t(k)];
                const float y = read(lineL, tap.ms * 0.001f * fs);
                const float yl = tap.lpL.lp(y) * tap.gain;
                const auto p = panOf(tap.pan);
                wl += yl * p.l;
                wr += yl * p.r;
            }
            l = xl * dryGain + wl * wetGain * makeup;
            r = xr * dryGain + wr * wetGain * makeup;
            sl = wl * sendGain * makeup;
            sr_ = wr * sendGain * makeup;
            break;
        }
        case Place::school: {
            lineL[std::size_t(write)] = xl;
            lineR[std::size_t(write)] = xr;
            // The tape: wow (~0.55 Hz), flutter (~6.8 Hz) and a slow unsteadiness.
            if (--wanderClock <= 0) {
                wanderClock = int(sr * 0.25);
                wanderTarget = (random01() * 2.0f - 1.0f);
            }
            wander += 0.00002f * (wanderTarget - wander);
            const float wowHz = 0.55f * s.spread, flutHz = 6.8f;
            const float d = 0.012f * fs + swingFor(9.0f * s.depth, wowHz, sr) * (1.0f + advance(phaseA, wowHz, sr))
                            + swingFor(3.0f * s.depth, flutHz, sr) * (1.0f + advance(phaseB, flutHz, sr)) + swingFor(4.0f * s.depth, 0.1f, sr) * (1.0f + wander);
            float tl = read(lineL, d), tr = read(lineR, d);
            if (s.tone < 0.8f) {   // a worn tape: the loud parts round off
                tl = softClip(tl * 1.6f) / 1.6f;
                tr = softClip(tr * 1.6f) / 1.6f;
            }
            l = bq[2].tick(bq[0].tick(tl));
            r = bq[3].tick(bq[1].tick(tr));
            // The lockers ringing in sympathy (bandpass resonators, alternately left and right).
            const float e = op[0].hp(0.5f * (xl + xr));
            for (int k = 0; k < 4; ++k) {
                res[std::size_t(k)].tick(e);
                const float v = res[std::size_t(k)].band * resGain[std::size_t(k)];
                l += (k % 2 == 0 ? 1.0f : 0.45f) * v;
                r += (k % 2 == 0 ? 0.45f : 1.0f) * v;
            }
            break;
        }
        case Place::garage: {
            lineL[std::size_t(write)] = 0.5f * (xl + xr);
            float wl = 0, wr = 0;
            for (int k = 0; k < tapCount; ++k) {
                auto& tap = taps[std::size_t(k)];
                const float y = tap.lpR.lp(tap.lpL.lp(read(lineL, tap.ms * 0.001f * fs))) * tap.gain;
                const auto p = panOf(tap.pan);
                wl += y * p.l;
                wr += y * p.r;
            }
            wl = bq[2].tick(wl);
            wr = bq[3].tick(wr);
            const float e = op[0].lp(0.5f * (xl + xr));
            res[0].tick(e);
            res[1].tick(e);
            const float boom = res[0].band * resGain[0] + res[1].band * resGain[1];
            l = bq[0].tick(xl) + wl + boom;
            r = bq[1].tick(xr) + wr + boom;
            break;
        }
        case Place::pool: {
            lineL[std::size_t(write)] = xl;
            lineR[std::size_t(write)] = xr;
            const float v1 = advance(phaseA, 0.21f, sr), v2 = advance(phaseB, 0.37f, sr), a = advance(phaseC, 0.15f, sr);
            const float base = 0.008f * fs;
            const float dl = base + swingFor(16.0f, 0.21f, sr) * (1.0f + v1) + swingFor(7.0f, 0.37f, sr) * (1.0f + v2);
            const float dr = base + swingFor(16.0f, 0.21f, sr) * (1.0f - v1) + swingFor(7.0f, 0.37f, sr) * (1.0f + v2);
            const float gain = wetGain * (1.0f + 0.2f * a);
            const float ul = bq[3].tick(bq[2].tick(read(lineL, dl))) * gain;
            const float ur = bq[5].tick(bq[4].tick(read(lineR, dr))) * gain;
            l = (1.0f - blend) * xl + blend * ul;
            r = (1.0f - blend) * xr + blend * ur;
            break;
        }
        case Place::hotel: {
            lineL[std::size_t(write)] = xl;
            lineR[std::size_t(write)] = xr;
            // The neighbour's room, small: two close reflections before the wall.
            const float nl = xl + 0.35f * read(lineL, 0.007f * fs) + 0.25f * read(lineR, 0.011f * fs);
            const float nr = xr + 0.35f * read(lineR, 0.0075f * fs) + 0.25f * read(lineL, 0.0105f * fs);
            const float wl = bq[6].tick(bq[4].tick(bq[2].tick(bq[0].tick(nl)))) * wetGain;
            const float wr = bq[7].tick(bq[5].tick(bq[3].tick(bq[1].tick(nr)))) * wetGain;
            const float hl = bq[10].tick(bq[8].tick(xl)), hr = bq[11].tick(bq[9].tick(xr));
            l = (1.0f - blend) * hl + blend * wl;
            r = (1.0f - blend) * hr + blend * wr;
            break;
        }
        case Place::subway: {
            lineL[std::size_t(write)] = 0.5f * (xl + xr);
            float wl = 0, wr = 0;
            for (int k = 0; k < tapCount; ++k) {
                auto& tap = taps[std::size_t(k)];
                // Each echo drifts a little flat and back (a slow swing of its delay).
                const float sw = swingFor(tap.cents, tap.modHz, sr);
                const float d = tap.ms * 0.001f * fs + sw * (1.0f + advance(tap.phase, tap.modHz, sr));
                const float y = tap.lpR.lp(tap.lpL.lp(read(lineL, d))) * tap.gain;
                const auto p = panOf(tap.pan);
                wl += y * p.l;
                wr += y * p.r;
            }
            // The rails: a short comb on the echoes only.
            const int cs = int(combL.size());
            const float cl = combL[std::size_t((combWrite - int(combDL) + cs) % cs)], cr = combR[std::size_t((combWrite - int(combDR) + cs) % cs)];
            combL[std::size_t(combWrite)] = wl + combFb * cl;
            combR[std::size_t(combWrite)] = wr + combFb * cr;
            if (++combWrite >= cs) combWrite = 0;
            const float el = bq[2].tick(bq[0].tick(0.5f * (wl + cl * (1.0f - combFb))));
            const float er = bq[3].tick(bq[1].tick(0.5f * (wr + cr * (1.0f - combFb))));
            l = xl + 1.5f * el;
            r = xr + 1.5f * er;
            break;
        }
        case Place::supermarket: {
            float sp = bq[2].tick(bq[1].tick(bq[0].tick(0.5f * (xl + xr))));
            if (s.extra > 0.0f) sp = softClip(sp * (1.0f + 3.0f * s.extra)) / (1.0f + 3.0f * s.extra);   // older speakers, pushed
            lineL[std::size_t(write)] = sp;
            lineR[std::size_t(write)] = 0.5f * (xl + xr);
            float wl = 0, wr = 0;
            for (int k = 0; k < tapCount; ++k) {
                auto& tap = taps[std::size_t(k)];
                const float d = tap.ms * 0.001f * fs + 0.00035f * fs * (1.0f + advance(tap.phase, tap.modHz, sr));
                const float y = tap.lpL.lp(read(lineL, d)) * tap.gain;
                const auto p = panOf(tap.pan);
                wl += y * p.l;
                wr += y * p.r;
            }
            const float floorBounce = read(lineR, 0.0031f * fs) * 0.45f;   // the polished floor
            l = xl * dryGain + wl + bq[3].tick(floorBounce);
            r = xr * dryGain + wr + bq[4].tick(floorBounce);
            break;
        }
        case Place::office: {
            const float dl = bq[2].tick(bq[0].tick(xl)), dr = bq[3].tick(bq[1].tick(xr));
            // The mains, full-wave rectified: the ballast's 120 Hz with its harmonics.
            phaseA += 60.0f / fs;
            if (phaseA >= 1.0f) phaseA -= 1.0f;
            const float raw = (std::abs(sinCycle(phaseA)) - 0.6366f) * 3.25f;
            const float carrier = op[0].lp(raw);
            if (s.extra > 0.0f && ++stutterClock >= stutterNext) {   // Infinite Cubicles: the ballast stutters now and then
                stutterClock = 0;
                stutterNext = int(sr * (6.0 + 12.0 * random01()));
                stutter = 1.0f;
            }
            stutter *= 0.99985f;
            const float depth = std::min(0.75f, 0.16f * s.depth * (1.0f + 2.0f * (1.0f - lamp) + 1.5f * s.extra * stutter));
            const float env = op[1].lp(std::abs(dl) + std::abs(dr));
            const float buzz = env * op[2].hp(raw) * 0.05f * s.depth;   // a little of the buzz rides on the notes
            l = dl * (1.0f + depth * carrier) + buzz;
            r = dr * (1.0f + depth * carrier) + buzz;
            break;
        }
        case Place::impossible: {
            lineL[std::size_t(write)] = xl;
            lineR[std::size_t(write)] = xr;
            // The reversed swell: each half second played backwards, rising into the moment it
            // was played (one block late), smeared by four allpasses.
            const int rs = 2 * revBlock;
            revL[std::size_t(revPos)] = xl;
            revR[std::size_t(revPos)] = xr;
            const int inBlock = revPos % revBlock;
            const int otherStart = revPos < revBlock ? revBlock : 0;
            const int back = otherStart + (revBlock - 1 - inBlock);
            const float u = float(inBlock) / float(revBlock);
            const float env = u * u * std::min(1.0f, (1.0f - u) * 25.0f);
            float rl = revL[std::size_t(back)] * env, rr = revR[std::size_t(back)] * env;
            if (++revPos >= rs) revPos = 0;
            float m = 0.5f * (rl + rr);
            for (int k = 0; k < 4; ++k) {
                auto& line = ap[std::size_t(k)];
                const float d = line[std::size_t(apPos[std::size_t(k)])];
                const float v = m + 0.6f * d;
                line[std::size_t(apPos[std::size_t(k)])] = v;
                if (++apPos[std::size_t(k)] >= int(line.size())) apPos[std::size_t(k)] = 0;
                m = d - 0.6f * v;
            }
            const float revAmount = 0.5f * s.depth;
            rl = (0.6f * rl + 0.8f * m) * revAmount;
            rr = (0.6f * rr - 0.8f * m) * revAmount;
            // Reflections that come back at the wrong pitch.
            const float pa = shiftA.tick(read(lineL, 0.037f * fs)), pb = shiftB.tick(read(lineR, 0.061f * fs));
            const float shifted = 0.22f * std::min(1.5f, s.depth);
            // Now and then the sound freezes in the air: the last 0.2 s held, looped, then let go.
            level += 0.0005f * (std::abs(xl) + std::abs(xr) - level);
            if (++freezeClock >= freezeNext && freezeTarget == 0.0f && freezeEnv < 1.0e-3f) {
                freezeClock = 0;
                const float rate = 0.4f + 1.6f * s.extra;
                freezeNext = int(sr * (14.0 + 18.0 * random01()) / rate);
                if (level > 0.003f) {
                    for (int k = 0; k < freezeLength; ++k) {
                        freezeL[std::size_t(k)] = read(lineL, float(freezeLength - k + 1));
                        freezeR[std::size_t(k)] = read(lineR, float(freezeLength - k + 1));
                    }
                    freezeTarget = 1.0f;
                    freezeHeld = 2.5f * fs;
                }
            }
            if (freezeTarget > 0.0f && (freezeHeld -= 1.0f) <= 0.0f) freezeTarget = 0.0f;
            if (freezeTarget > freezeEnv) freezeEnv = std::min(1.0f, freezeEnv + 1.0f / (0.4f * fs));   // in over 0.4 s
            else freezeEnv = std::max(0.0f, freezeEnv - 1.0f / (1.4f * fs));                         // out over 1.4 s
            float fl = 0, fr = 0;
            if (freezeEnv > 0.0f) {   // two heads half a loop apart, cross-faded: no seam
                const int a = freezePos, b = (freezePos + freezeLength / 2) % freezeLength;
                const float wa = sinCycle(0.5f * float(a) / float(freezeLength)), wb = sinCycle(0.5f * float(b) / float(freezeLength));
                fl = (freezeL[std::size_t(a)] * wa * wa + freezeL[std::size_t(b)] * wb * wb) * freezeEnv * 0.35f * std::min(1.2f, s.depth);
                fr = (freezeR[std::size_t(a)] * wa * wa + freezeR[std::size_t(b)] * wb * wb) * freezeEnv * 0.35f * std::min(1.2f, s.depth);
                if (++freezePos >= freezeLength) freezePos = 0;
            }
            l = xl * dryGain + rl + shifted * pa + fl;
            r = xr * dryGain + rr + shifted * pb + fr;
            break;
        }
        }
        if (++write >= size) write = 0;
        outL[i] = l * (where == Place::mall ? 1.0f : makeup);
        outR[i] = r * (where == Place::mall ? 1.0f : makeup);
        sendL[i] = sl;
        sendR[i] = sr_;
    }
}

} // namespace liminal::dsp
