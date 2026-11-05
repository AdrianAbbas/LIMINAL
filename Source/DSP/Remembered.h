#pragma once
// REMEMBERED (2026-11-05, patch notes 2-4 and 7): a built-in treatment that is part of an instrument,
// not an effect the player has to find. The developer's voice (Empty Choir, Solo Voice, the Hollow
// Drone's voice layer), the violin (Faded Strings, Hallway Pizzicato) and the arcade lead sounded
// recorded, close and crisp out of the box; LIMINAL's instruments should sound remembered: heard from
// a distance, from an old tape, a little blurred. One stereo chain, its amounts set by a profile:
//
//   bandwidth   a tape's and a far wall's limits: high-pass and a four-pole low-pass (plus a wall's
//               low-mid body for "through a wall")
//   de-ess      a dynamic high band: sibilants, consonants and breaths (the energy above ~4.5 kHz that
//               jumps above the voice's own) are pressed down, the vowel is left alone
//   soften      a slower attack: the onset of every note is held back by an envelope that cannot rise
//               faster than the profile allows (a struck or plucked edge rounded off, a click softened)
//   saturation  a little tape warmth (soft, level-matched)
//   wow, drift  a slow wobble and a slower wander of pitch (a modulated delay; scaled by Liminality's
//               wander, so LIMINALITY 0 is perfectly stable, as everywhere else)
//   blur        a short allpass diffuser mixed in: the edges of consonants and plucks smear
//   distance    early reflections after a pre-delay, darker than the direct sound; the direct sound
//               is a little quieter (heard down a corridor), the reflections carry the rest
//   echo        one far, dark repeat (the arcade across the mall)
//   noise       a faint tape hiss that follows the playing (it dies with the last note: silence is silence)
//
// It is level-matched by a fixed make-up per profile (tests: library, recorded). Real-time rules as
// everywhere: fixed storage set in prepare(), no allocation, coefficients at control rate (32 samples).
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include <array>
#include <vector>

namespace liminal::dsp {

struct RememberedProfile {
    float lowCut = 90, highCut = 6000;     // Hz: the bandwidth (a four-pole low-pass)
    float wall = 0;                        // 0..1: through a wall (low-mid body, a steeper top)
    float deEss = 0;                       // 0..1: sibilants / breaths pressed down (up to ~12 dB)
    float soften = 0;                      // seconds: the fastest the attack may rise to full (0 = untouched)
    float saturation = 0;                  // 0..1
    float wow = 0, drift = 0;              // cents (at Liminality's wander 1)
    float blur = 0;                        // 0..1: the diffuser's share
    float reflections = 0;                 // 0..1: early reflections' share
    float preDelayMs = 12;                 // before the first reflection
    float spacing = 1;                     // the reflections' spread (1 = a room, 2 = a corridor)
    float direct = 1;                      // the direct sound's level (distance)
    float echo = 0, echoMs = 300;          // one far repeat (and a faint second)
    float hissDb = -200;                   // the hiss under the playing, relative to it
    float makeupDb = 0;                    // level-matched
};

class Remembered {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        line.assign(std::size_t(0.25 * sr) + 8, 0.0f);   // wow/drift + pre-delay + reflections + echo
        lineR.assign(line.size(), 0.0f);
        echoLine.assign(std::size_t(0.8 * sr) + 8, 0.0f);
        echoLineR.assign(echoLine.size(), 0.0f);
        static constexpr float lengths[diffusers] { 0.0047f, 0.0083f, 0.0131f, 0.0173f };
        for (int k = 0; k < diffusers; ++k) {
            const auto size = std::size_t(lengths[k] * float(sr)) + 1;
            ap[k].assign(size, 0.0f);
            apR[k].assign(std::size_t(lengths[k] * 1.09f * float(sr)) + 1, 0.0f);
        }
        reset();
        setProfile(profile);
    }
    void reset() {
        std::fill(line.begin(), line.end(), 0.0f);
        std::fill(lineR.begin(), lineR.end(), 0.0f);
        std::fill(echoLine.begin(), echoLine.end(), 0.0f);
        std::fill(echoLineR.begin(), echoLineR.end(), 0.0f);
        for (int k = 0; k < diffusers; ++k) {
            std::fill(ap[k].begin(), ap[k].end(), 0.0f);
            std::fill(apR[k].begin(), apR[k].end(), 0.0f);
            apPos[k] = apPosR[k] = 0;
        }
        for (auto* b : { &hpL, &hpR, &lp1L, &lp1R, &lp2L, &lp2R, &wallL, &wallR }) b->clear();
        for (auto* o : { &essL, &essR, &reflToneL, &reflToneR, &echoToneL, &echoToneR }) o->z = 0;
        write = echoWrite = 0;
        envFast = envSlow = envLevel = 0;
        softGain = 1;
        essEnv = bodyEnv = 0;
        gate = 0;
        wowPhase = 0.13f;
        driftPhase = 0.61f;
        noise.state = 0x6A09E667u;
        clock = 0;
        swing = swingTarget = swingStep = 0;
        quiet = 0;
        asleep = true;
        wanderNow = 0;
    }
    void setProfile(const RememberedProfile& p) {
        profile = p;
        hpL.highPass(p.lowCut, 0.6, sr);
        lp1L.lowPass(p.highCut, 0.55, sr);
        lp2L.lowPass(p.highCut * (p.wall > 0 ? 1.0f - 0.25f * p.wall : 1.15f), 0.75, sr);
        wallL.peak(320.0, 0.8, 4.0 * p.wall, sr);
        for (auto [a, b] : { std::pair { &hpL, &hpR }, { &lp1L, &lp1R }, { &lp2L, &lp2R }, { &wallL, &wallR } }) {
            b->b0 = a->b0; b->b1 = a->b1; b->b2 = a->b2; b->a1 = a->a1; b->a2 = a->a2;
        }
        essL.setHz(4500.0f, sr);
        essR.setHz(4500.0f, sr);
        reflToneL.setHz(std::min(p.highCut * 0.6f, 3000.0f), sr);
        reflToneR.c = reflToneL.c;
        echoToneL.setHz(1400.0f, sr);
        echoToneR.c = echoToneL.c;
        makeup = std::pow(10.0f, p.makeupDb / 20.0f);
        hiss = p.hissDb > -150.0f ? std::pow(10.0f, p.hissDb / 20.0f) : 0.0f;
        // The soft attack: the slowest envelope that still lets a note rise fully in `soften` seconds.
        riseCoef = p.soften > 0.0f ? float(1.0 - std::exp(-1.0 / (double(p.soften) * 0.35 * sr))) : 1.0f;
        fastCoef = float(1.0 - std::exp(-1.0 / (0.002 * sr)));
        fallCoef = float(1.0 - std::exp(-1.0 / (0.08 * sr)));
        gainCoef = float(1.0 - std::exp(-1.0 / (0.002 * sr)));
        levelCoef = float(1.0 - std::exp(-1.0 / (0.3 * sr)));
        // Reflections: six taps after the pre-delay, spread by `spacing`, each a little darker and quieter.
        static constexpr float at[taps] { 0.0f, 0.0061f, 0.0113f, 0.0172f, 0.0244f, 0.0331f };
        static constexpr float gl[taps] { 0.62f, 0.0f, 0.45f, 0.30f, 0.0f, 0.2f };
        static constexpr float gr[taps] { 0.0f, 0.58f, 0.28f, 0.0f, 0.36f, 0.22f };
        for (int k = 0; k < taps; ++k) {
            tapAt[k] = (p.preDelayMs * 0.001f + at[k] * p.spacing) * float(sr);
            tapL[k] = gl[k];
            tapR[k] = gr[k];
        }
        echoAt = std::clamp(p.echoMs * 0.001f, 0.02f, 0.7f) * float(sr);
    }
    const RememberedProfile& currentProfile() const { return profile; }

    // In place, on the instrument's own output (nothing else in the buffers). `wander`: Liminality's
    // gate on the instrument's own pitch movement (0: stable).
    void process(float* left, float* right, int n, float wander) {
        // Asleep while nothing comes in and its tails have died (0.8 s): no cost, exact silence.
        bool silent = true;
        for (int i = 0; i < n && silent; ++i) silent = left[i] == 0.0f && right[i] == 0.0f;
        if (silent) {
            quiet += n;
            if (asleep) return;
            if (quiet > int(0.8 * sr)) { reset(); asleep = true; std::fill(left, left + n, 0.0f); std::fill(right, right + n, 0.0f); return; }
        } else {
            quiet = 0;
            asleep = false;
        }
        const auto& p = profile;
        const int size = int(line.size()), esize = int(echoLine.size());
        const float base = std::round(0.004f * float(sr));   // the modulated read sits 4 ms back (plus its swing)
        for (int i = 0; i < n; ++i) {
            if (clock == 0) {   // control rate: the delay's swing for wow (0.55 Hz) and drift (0.07 Hz)
                const float tick = 32.0f / float(sr);
                wanderNow += std::min(1.0f, tick / 0.3f) * (wander - wanderNow);
                const float wowDepth = std::max(0.0f, p.wow) * wanderNow, driftDepth = std::max(0.0f, p.drift) * wanderNow;
                wowPhase += 0.55f * tick;
                wowPhase -= std::floor(wowPhase);
                driftPhase += 0.07f * tick;
                driftPhase -= std::floor(driftPhase);
                // cents -> seconds of delay swing: depth / (1200 ln2^-1 * 2 pi f)
                const float wowS = wowDepth * 0.000577623f / (twoPi * 0.55f), driftS = driftDepth * 0.000577623f / (twoPi * 0.07f);
                swingTarget = (wowS * std::sin(twoPi * wowPhase) + driftS * (0.7f * std::sin(twoPi * driftPhase) + 0.3f * std::sin(twoPi * 2.3f * driftPhase + 1.1f))) * float(sr);
                swingStep = (swingTarget - swing) / 32.0f;
                clock = 32;
            }
            --clock;
            swing += swingStep;
            float l = left[i], r = right[i];
            // Tape hiss under the playing (before the bandwidth, so the tape's own limits shape it): it
            // follows the level (300 ms) and dies with it.
            if (hiss > 0.0f) {
                envLevel += levelCoef * ((std::abs(l) + std::abs(r)) - envLevel);
                const float h = hiss * std::min(envLevel, 0.5f);
                l += h * noise.bipolar();
                r += h * noise.bipolar();
            }
            // Bandwidth (and the wall's body).
            l = lp2L.tick(lp1L.tick(hpL.tick(l)));
            r = lp2R.tick(lp1R.tick(hpR.tick(r)));
            if (p.wall > 0.0f) { l = wallL.tick(l); r = wallR.tick(r); }
            // De-ess: the band above 4.5 kHz against the whole; its excess pressed down.
            if (p.deEss > 0.0f) {
                const float hl = l - essL.lp(l), hr = r - essR.lp(r);
                const float band = std::abs(hl) + std::abs(hr), body = std::abs(l) + std::abs(r);
                essEnv += (band > essEnv ? 0.08f : 0.0015f) * (band - essEnv);
                bodyEnv += 0.002f * (body - bodyEnv);
                const float ratio = essEnv / std::max(1.0e-9f, 0.25f * bodyEnv);
                const float cut = ratio > 1.0f ? std::max(0.25f, 1.0f - p.deEss * (1.0f - 1.0f / ratio)) : 1.0f;
                l -= hl * (1.0f - cut) + hl * 0.35f * p.deEss;   // and the band a little lower always
                r -= hr * (1.0f - cut) + hr * 0.35f * p.deEss;
            }
            // Soft attack: the power envelope that rises no faster than the profile allows, against one that
            // follows in 2 ms; their ratio (smoothed over 2 ms, so it never ripples with the waveform) holds
            // back each onset.
            if (riseCoef < 1.0f) {
                const float power = l * l + r * r;
                envFast += (power > envFast ? fastCoef : fallCoef) * (power - envFast);
                envSlow += (power > envSlow ? riseCoef : fallCoef) * (power - envSlow);
                const float target = envFast > 1.0e-14f ? std::min(1.0f, std::sqrt(envSlow / envFast)) : 1.0f;
                softGain += gainCoef * (target - softGain);
                l *= softGain;
                r *= softGain;
            }
            if (p.saturation > 0.0f) {
                const float d = 1.0f + 3.0f * p.saturation;
                l = softClip(d * l) / d;
                r = softClip(d * r) / d;
            }
            // Wow and drift, then the pre-delayed reflections, from one line.
            line[std::size_t(write)] = l;
            lineR[std::size_t(write)] = r;
            const auto read = [&](const std::vector<float>& v, float delay) {
                float pos = float(write) - delay;
                while (pos < 0) pos += float(size);
                int a = int(pos);
                const float f = pos - float(a);
                if (a >= size) a -= size;   // (-1e-5 + size rounds to size in float)
                const int b = a + 1 >= size ? 0 : a + 1;
                return v[std::size_t(a)] + f * (v[std::size_t(b)] - v[std::size_t(a)]);
            };
            const float d = base + swing;
            float dl = read(line, d), dr = read(lineR, d);
            float rl = 0, rr = 0;
            if (p.reflections > 0.0f)
                for (int k = 0; k < taps; ++k) {
                    if (tapL[k] > 0.0f) rl += tapL[k] * read(line, d + tapAt[k]);
                    if (tapR[k] > 0.0f) rr += tapR[k] * read(lineR, d + tapAt[k]);
                }
            if (++write >= size) write = 0;
            // Blur: a short diffuser on the direct sound.
            if (p.blur > 0.0f) {
                float bl = dl, br = dr;
                for (int k = 0; k < diffusers; ++k) {
                    bl = allpass(ap[k], apPos[k], bl);
                    br = allpass(apR[k], apPosR[k], br);
                }
                dl += p.blur * (bl - dl);
                dr += p.blur * (br - dr);
            }
            float outL = p.direct * dl + p.reflections * reflToneL.lp(rl), outR = p.direct * dr + p.reflections * reflToneR.lp(rr);
            // One far echo (and a faint second through the feedback), dark.
            if (p.echo > 0.0f) {
                float pos = float(echoWrite) - echoAt;
                if (pos < 0) pos += float(esize);
                const auto e = std::size_t(std::min(esize - 1, int(pos)));
                const float el = echoToneL.lp(echoLine[e]), er = echoToneR.lp(echoLineR[e]);
                echoLine[std::size_t(echoWrite)] = outL + 0.25f * er;
                echoLineR[std::size_t(echoWrite)] = outR + 0.25f * el;
                if (++echoWrite >= esize) echoWrite = 0;
                outL += p.echo * er;
                outR += p.echo * el;
            }
            left[i] = outL * makeup;
            right[i] = outR * makeup;
        }
    }

private:
    static constexpr int diffusers = 4, taps = 6;
    static float allpass(std::vector<float>& v, int& pos, float x) {
        const float g = 0.55f;
        const float y = v[std::size_t(pos)];
        const float u = x + g * y;
        v[std::size_t(pos)] = u;
        if (++pos >= int(v.size())) pos = 0;
        return y - g * u;
    }
    double sr = 48000.0;
    RememberedProfile profile;
    std::vector<float> line, lineR, echoLine, echoLineR;
    std::array<std::vector<float>, diffusers> ap, apR;
    std::array<int, diffusers> apPos {}, apPosR {};
    Biquad hpL, hpR, lp1L, lp1R, lp2L, lp2R, wallL, wallR;
    OnePole essL, essR, reflToneL, reflToneR, echoToneL, echoToneR;
    float tapAt[taps] {}, tapL[taps] {}, tapR[taps] {};
    float echoAt = 1000;
    int write = 0, echoWrite = 0, clock = 0;
    float envFast = 0, envSlow = 0, envLevel = 0, essEnv = 0, bodyEnv = 0, gate = 0;
    float riseCoef = 1, fastCoef = 1, fallCoef = 0.001f, levelCoef = 0.0001f, gainCoef = 0.01f, softGain = 1;
    float makeup = 1, hiss = 0;
    float wowPhase = 0, driftPhase = 0, swing = 0, swingTarget = 0, swingStep = 0;
    Random noise;
    int quiet = 0;
    bool asleep = true;
    float wanderNow = 0;
};

// The profiles (the patch notes' wording in each comment).
namespace remembered {
// Empty Choir: a choir remembered in an empty building, recognisably sung.
inline RememberedProfile choir() {
    RememberedProfile p;
    p.lowCut = 110; p.highCut = 4200; p.deEss = 0.8f; p.soften = 0.4f; p.saturation = 0.2f;
    p.wow = 4.0f; p.drift = 6.0f; p.blur = 0.45f; p.reflections = 0.55f; p.preDelayMs = 18; p.spacing = 1.3f;
    p.direct = 0.8f; p.hissDb = -58; p.makeupDb = 9.5f;
    return p;
}
// Solo Voice: one distant singer down a corridor / through a wall.
inline RememberedProfile soloVoice() {
    RememberedProfile p;
    p.lowCut = 150; p.highCut = 3000; p.wall = 0.6f; p.deEss = 1.0f; p.soften = 0.4f; p.saturation = 0.25f;
    p.wow = 5.0f; p.drift = 7.0f; p.blur = 0.6f; p.reflections = 0.8f; p.preDelayMs = 26; p.spacing = 2.2f;
    p.direct = 0.6f; p.hissDb = -56; p.makeupDb = 10.0f;
    return p;
}
// The Hollow Drone's voice layer (lighter: it is one layer of a pad).
inline RememberedProfile droneVoice() {
    RememberedProfile p = choir();
    p.highCut = 3800; p.reflections = 0.4f; p.direct = 0.85f; p.hissDb = -200; p.makeupDb = 3.5f; p.soften = 0.0f;
    return p;
}
// Faded Strings: softer, slower, darker; tape and distance.
inline RememberedProfile strings() {
    RememberedProfile p;
    p.lowCut = 100; p.highCut = 4000; p.deEss = 0.5f; p.soften = 0.45f; p.saturation = 0.2f;
    p.wow = 3.0f; p.drift = 5.0f; p.blur = 0.35f; p.reflections = 0.45f; p.preDelayMs = 14; p.spacing = 1.2f;
    p.direct = 0.85f; p.hissDb = -60; p.makeupDb = 9.6f;
    return p;
}
// Hallway Pizzicato: the pluck rounded off (less clicky), darker, tape and distance.
inline RememberedProfile pizzicato() {
    RememberedProfile p;
    p.lowCut = 120; p.highCut = 3600; p.deEss = 0.7f; p.soften = 0.018f; p.saturation = 0.15f;
    p.wow = 3.0f; p.drift = 4.0f; p.blur = 0.4f; p.reflections = 0.5f; p.preDelayMs = 16; p.spacing = 1.6f;
    p.direct = 0.8f; p.hissDb = -60; p.makeupDb = 6.1f;
    return p;
}
// Arcade Lead: an empty arcade at night heard from across a closed mall; carpet-damped.
inline RememberedProfile arcade() {
    RememberedProfile p;
    p.lowCut = 220; p.highCut = 2400; p.wall = 0.35f; p.soften = 0.03f; p.saturation = 0.2f;
    p.wow = 9.0f; p.drift = 6.0f; p.blur = 0.3f; p.reflections = 0.5f; p.preDelayMs = 30; p.spacing = 2.4f;
    p.direct = 0.7f; p.echo = 0.22f; p.echoMs = 360; p.hissDb = -60; p.makeupDb = 8.5f;
    return p;
}
// Distant Radio: a melody through a small speaker in another room (patch note 5).
inline RememberedProfile radio() {
    RememberedProfile p;
    p.lowCut = 380; p.highCut = 2900; p.wall = 0.8f; p.soften = 0.02f; p.saturation = 0.45f;
    p.wow = 3.0f; p.drift = 4.0f; p.blur = 0.2f; p.reflections = 0.6f; p.preDelayMs = 20; p.spacing = 1.8f;
    p.direct = 0.75f; p.hissDb = -44; p.makeupDb = 5.0f;
    return p;
}
} // namespace remembered

} // namespace liminal::dsp
