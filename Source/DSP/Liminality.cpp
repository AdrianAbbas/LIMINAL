#include "DSP/Liminality.h"
#include <limits>

namespace liminal::dsp {

namespace {
std::uint32_t mix(std::uint32_t a, std::uint32_t b) {
    std::uint32_t x = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return x;
}
float unit(std::uint32_t h) { return float(h >> 8) * (1.0f / 16777216.0f); }
float lattice(std::uint32_t s, std::int64_t i) {
    return 2.0f * unit(mix(s, mix(std::uint32_t(std::uint64_t(i)), std::uint32_t(std::uint64_t(i) >> 32)))) - 1.0f;
}
// A smooth random curve in about [-1, 1]: Catmull-Rom through hashed points one unit apart. A pure
// function of x, so the same timeline position always gives the same value.
float valueNoise(std::uint32_t s, double x) {
    const double f = std::floor(x);
    const auto i = std::int64_t(f);
    const float t = float(x - f);
    const float p0 = lattice(s, i - 1), p1 = lattice(s, i), p2 = lattice(s, i + 1), p3 = lattice(s, i + 2);
    return 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t + (3.0f * (p1 - p2) + p3 - p0) * t * t * t);
}
constexpr double twoPiD = 6.283185307179586;
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }
float onePoleCoef(double hz, double sr) { return float(1.0 - std::exp(-twoPiD * std::clamp(hz, 1.0, 0.49 * sr) / sr)); }

// Room nudges: a multiplier per control, in lim::Control order
// (detune, drift, wow, warble, ghost, wear, timing, uneven, sour, fade).
constexpr float placeNudge[10][lim::count] {
    { 1.55f, 0.90f, 0.85f, 0.80f, 0.90f, 0.80f, 1.00f, 1.00f, 1.10f, 1.00f },   // hospital: cold, clinical detune
    { 1.00f, 1.00f, 1.20f, 1.30f, 1.10f, 1.10f, 1.00f, 1.00f, 1.00f, 1.00f },   // mall: muzak on an old PA, chorus-y
    { 1.00f, 1.10f, 1.60f, 1.00f, 1.00f, 1.50f, 1.05f, 1.10f, 1.00f, 1.00f },   // school: wow and tape wear
    { 1.05f, 1.15f, 1.00f, 0.90f, 1.30f, 1.10f, 1.00f, 1.00f, 1.00f, 1.00f },   // garage: concrete ghosts
    { 1.00f, 1.20f, 0.80f, 1.70f, 1.00f, 0.90f, 1.00f, 1.00f, 1.00f, 1.20f },   // pool: slow warble
    { 1.00f, 1.00f, 1.10f, 1.00f, 1.10f, 1.40f, 1.00f, 1.10f, 1.00f, 1.60f },   // hotel: wear and fade
    { 1.00f, 1.00f, 1.25f, 1.00f, 1.15f, 1.30f, 1.20f, 1.00f, 1.00f, 1.00f },   // subway: flutter, wear, loose timing
    { 1.00f, 1.00f, 1.30f, 1.20f, 1.00f, 1.20f, 1.00f, 1.00f, 1.00f, 1.00f },   // supermarket: tired muzak tape
    { 1.10f, 1.00f, 1.00f, 1.00f, 1.00f, 1.10f, 1.20f, 1.30f, 1.00f, 0.90f },   // office: drab, uneven, loose
    { 1.10f, 1.20f, 1.00f, 1.00f, 1.60f, 1.00f, 1.00f, 1.00f, 1.80f, 1.00f },   // impossible room: sour notes, ghost
};
struct CharacterNudge { int index; int control; float factor; };
// Characters (place * 3 + Character): the unsettling ones push further.
constexpr CharacterNudge characterNudges[] {
    { 0, lim::wear, 0.75f },  { 0, lim::ghost, 0.85f },                                                     // Sterile Silence
    { 1, lim::fade, 1.20f },  { 1, lim::wear, 1.10f },                                                      // After Hours
    { 2, lim::sour, 1.90f },  { 2, lim::drift, 1.60f }, { 2, lim::detune, 1.20f },                          // Something Wrong
    { 3, lim::fade, 1.20f },                                                                                // Closing Time
    { 4, lim::wear, 1.40f },  { 4, lim::fade, 1.30f }, { 4, lim::wow, 1.20f },                              // Forgotten
    { 5, lim::ghost, 1.30f }, { 5, lim::fade, 1.20f },                                                      // Endless
    { 7, lim::wow, 1.30f },   { 7, lim::warble, 1.20f }, { 7, lim::wear, 1.20f },                           // Childhood Memory
    { 8, lim::sour, 1.60f },  { 8, lim::drift, 1.50f }, { 8, lim::warble, 1.30f },                          // Nightmare
    { 10, lim::ghost, 1.20f },                                                                              // Distant Movement
    { 11, lim::timing, 1.30f }, { 11, lim::drift, 1.20f },                                                  // Pursuit
    { 12, lim::warble, 0.90f },                                                                             // Still Water
    { 13, lim::wow, 1.30f },  { 13, lim::wear, 1.30f },                                                     // Forgotten Summer
    { 14, lim::warble, 1.40f }, { 14, lim::fade, 1.50f }, { 14, lim::drift, 1.30f },                        // Submerged
    { 15, lim::fade, 1.10f },                                                                               // Late Night
    { 16, lim::wear, 1.20f }, { 16, lim::uneven, 1.20f },                                                   // Faded Luxury
    { 17, lim::sour, 1.90f }, { 17, lim::drift, 1.60f }, { 17, lim::ghost, 1.30f },                         // Room 404
    { 18, lim::ghost, 1.20f },                                                                              // Last Train
    { 19, lim::wear, 1.40f },                                                                               // Abandoned
    { 20, lim::drift, 1.30f }, { 20, lim::timing, 1.20f },                                                  // Approaching
    { 22, lim::wow, 1.50f },  { 22, lim::wear, 1.40f },                                                     // Nostalgia
    { 23, lim::sour, 1.40f }, { 23, lim::warble, 1.30f }, { 23, lim::detune, 1.20f },                       // Fluorescent Nightmare
    { 25, lim::warble, 1.40f }, { 25, lim::fade, 1.40f }, { 25, lim::ghost, 1.20f },                        // Dream State
    { 26, lim::ghost, 1.40f }, { 26, lim::timing, 1.20f },                                                  // Infinite Cubicles
    { 27, lim::sour, 0.90f }, { 27, lim::ghost, 0.90f },                                                    // Familiar
    { 28, lim::drift, 1.50f }, { 28, lim::warble, 1.40f }, { 28, lim::sour, 1.30f },                        // Unstable
    { 29, lim::sour, 1.80f }, { 29, lim::drift, 1.70f }, { 29, lim::ghost, 1.30f },                         // Beyond Reality
};
} // namespace

std::uint32_t lim::hash(std::uint32_t a, std::uint32_t b) { return mix(a, b); }
float lim::unit(std::uint32_t h) { return dsp::unit(h); }
float lim::smoothNoise(std::uint32_t seed, double x) { return valueNoise(seed, x); }

LiminalAmounts liminalNudge(int place, int character) {
    LiminalAmounts n {};
    const int p = std::clamp(place, 0, 9);
    for (int k = 0; k < lim::count; ++k) n[std::size_t(k)] = placeNudge[p][k];
    const int index = p * 3 + std::clamp(character, 0, 2);
    for (const auto& c : characterNudges)
        if (c.index == index) n[std::size_t(c.control)] *= c.factor;
    return n;
}

LiminalAmounts liminalEffective(const LiminalAmounts& own, float macro, int place, int character, float environmentAmount) {
    const auto nudge = liminalNudge(place, character);
    const float scale = std::max(0.0f, macro) / lim::macroDefault;
    const float env = std::clamp(environmentAmount, 0.0f, 1.0f);
    LiminalAmounts e {};
    for (std::size_t k = 0; k < e.size(); ++k)
        e[k] = std::clamp(own[k] * scale * (1.0f + (nudge[k] - 1.0f) * env), 0.0f, lim::maxEffective);
    return e;
}

//==============================================================================
void Liminality::prepare(double sampleRate, int instrumentIndex) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    instrument = instrumentIndex;
    int n = 1;
    while (n < int(0.2 * sr) + 8) n <<= 1;   // the ghost (<= 95 ms + a 50 ms window) and the chorus
    lineL.assign(std::size_t(n), 0.0f);
    lineR.assign(std::size_t(n), 0.0f);
    lineMask = n - 1;
    const float h = unit(mix(0xC0FFEEu, std::uint32_t(instrument)));
    ghostDelay = float((0.055 + 0.04 * double(h)) * sr);
    ghostWindow = float(0.05 * sr);
    for (std::size_t k = 0; k < window.size(); ++k) {
        const double s = std::sin(0.5 * twoPiD * double(k) / double(window.size() - 1));
        window[k] = float(s * s);
    }
    const double blurMs[4] { 3.1, 4.7, 3.7, 5.3 };
    for (std::size_t k = 0; k < blur.size(); ++k) blur[k].line.assign(std::size_t(std::max(2.0, blurMs[k] * 0.001 * sr)), 0.0f);
    smooth = float(1.0 - std::exp(-1.0 / (0.02 * sr)));
    envAttack = float(1.0 - std::exp(-1.0 / (0.005 * sr)));
    envRelease = float(1.0 - std::exp(-1.0 / (0.45 * sr)));
    dropCoefDown = float(1.0 - std::exp(-1.0 / (0.004 * sr)));
    dropCoefUp = float(1.0 - std::exp(-1.0 / (0.008 * sr)));
    wearEnvCoef = float(1.0 - std::exp(-1.0 / (1.0 * sr)));
    relocateDecay = float(std::exp(-64.0 / (0.1 * sr)));
    ghostToneL.setHz(4500.0f, sr);
    ghostToneR.setHz(4500.0f, sr);
    reset();
}

void Liminality::reset() {
    now = target;
    pitch = {};
    last = {};
    relocateOffset = 0;
    clockKnown = false;
    queued = 0;
    noteDelay.fill(0);
    lastDue.fill(0);
    bypassed = true;
    lineLive = false;
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    write = 0;
    ghostPos = 0;
    ghostGain = ghostTarget = 0;
    lineWarm = 0;
    ghostToneL.z = ghostToneR.z = 0;
    warbleAmp = warbleAmpTarget = warbleChorus = warbleChorusTarget = 0;
    warbleGain = 1;
    warbleGainStep = chorusDelayStep = 0;
    chorusDelay = float(0.011 * sr);
    sinceNote = 10;
    sustainWarble = sustainFade = 0;
    fadeMix = fadeMixTarget = blurMix = blurMixTarget = 0;
    fadeCoef = 1;
    fadeCoefStep = 0;
    fadeOctaves = 0;
    resetFilters();
    wearMix = wearMixTarget = 0;
    drive = driveTarget = 1;
    wearGain = wearGainTarget = 1;
    wearIn = wearOut = 0;
    outGain = outGainTarget = 1;
    envelope = 0;
    hissLevel = crackleRate = crackleLevel = crackle = crackleZ = 0;
    dropGain = dropTarget = 1;
    dropLeft = 0;
    dropoutCount = 0;
    noise.state = mix(seed, std::uint32_t(instrument) + 77u) | 1u;
}

void Liminality::resetFilters() {
    fadeL1 = fadeL2 = fadeR1 = fadeR2 = 0;
    for (auto& a : blur) {
        std::fill(a.line.begin(), a.line.end(), 0.0f);
        a.pos = 0;
    }
    wl1 = wl2 = wr1 = wr2 = hl1 = hl2 = hr1 = hr2 = 0;
    hissZ1 = hissZ2 = hissH1 = hissH2 = 0;
}

std::uint32_t Liminality::salt(std::uint32_t k) const { return mix(mix(seed, std::uint32_t(instrument) + 1u), k); }

// Drift and Wow & Flutter at a timeline position, in cents.
float Liminality::modulators(double t) const {
    float c = 0;
    const float dr = now[lim::drift], ww = now[lim::wow];
    if (dr > 0.0f)   // wanders away over several seconds and comes back
        c += 22.0f * dr * (0.7f * valueNoise(salt(11), t / 9.0) + 0.3f * valueNoise(salt(12), t / 3.7));
    if (ww > 0.0f) {
        const double h1 = unit(salt(13)), h2 = unit(salt(14)), h3 = unit(salt(15));
        const double fw = 0.55 + 0.3 * h1, f1 = 6.5 + 2.0 * h2, f2 = 10.5 + 2.0 * h3;
        const float wowNow = float(std::sin(std::fmod(twoPiD * fw * t, twoPiD) + twoPiD * h2)) * (0.7f + 0.3f * valueNoise(salt(16), t / 2.3));
        const float flutter = float(0.65 * std::sin(std::fmod(twoPiD * f1 * t, twoPiD)) + 0.35 * std::sin(std::fmod(twoPiD * f2 * t, twoPiD) + twoPiD * h1))
                              * (0.6f + 0.4f * valueNoise(salt(17), t / 0.7));
        c += ww * (12.0f * wowNow + 3.5f * flutter);
    }
    return c;
}

void Liminality::relocate(std::uint64_t clock, bool sounding) {
    // The pitch is a pure function of the position: from silence it continues from there without a
    // blend (a blend would depend on where the jump came from, and a replay would drift out of
    // phase). While notes still ring (a cycle jump in the middle of a chord) it glides over from
    // where it was in about 150 ms instead of stepping, and the audio side carries on untouched.
    noise.state = mix(salt(99), std::uint32_t(clock) ^ std::uint32_t(clock >> 32)) | 1u;
    if (sounding && clockKnown) {
        const float before = pitch.cents;
        lastClock = clock;
        relocateOffset = before - modulators(double(clock) / sr);
        relocateDecay = float(std::exp(-64.0 / (0.15 * sr)));
        return;
    }
    lastClock = clock;
    dropLeft = 0;          // a dropout in progress ends (ramping back up)
    // Wear's level match starts again from a typical value for its amount (a replay must match)
    wearIn = wearOut = 0;
    wearGain = wearGainTarget = now[lim::wear] > 0.0f ? dbGain(2.5f * std::min(now[lim::wear], 1.0f)) : 1.0f;
    // the ghost's read heads, from the position too (else a replay's ghost would sit elsewhere)
    ghostPos = float(std::fmod(double(clock) * 0.0137, double(ghostWindow)));
    dropTarget = 1.0f;
}

void Liminality::advance(std::uint64_t clock) {
    // The amounts, smoothed over ~30 ms (automation never jumps the pitch).
    const float k = float(1.0 - std::exp(-64.0 / (0.03 * sr)));
    for (std::size_t i = 0; i < now.size(); ++i) {
        now[i] += k * (target[i] - now[i]);
        if (std::abs(target[i] - now[i]) < 1.0e-5f) now[i] = target[i];
    }
    clockKnown = true;
    lastClock = clock;
    // The note side's pitch.
    relocateOffset *= relocateDecay;
    if (std::abs(relocateOffset) < 1.0e-4f) relocateOffset = 0;
    pitch.cents = modulators(double(clock) / sr) + relocateOffset;
    pitch.warble = 16.0f * now[lim::warble];
    pitch.wander = std::clamp(std::max(now[lim::drift], now[lim::wow]) / 0.3f, 0.0f, 1.0f);
}

void Liminality::control(Instrument& in, std::uint64_t clock) {
    advance(clock);
    in.setLiminalPitch(pitch);
    audioSide(clock);
}

void Liminality::controlAudio(std::uint64_t clock) {
    advance(clock);
    audioSide(clock);
}

void Liminality::audioSide(std::uint64_t clock) {
    const float tick = float(64.0 / sr);
    const double t = double(clock) / sr;
    // The audio side. Warble and Fade grow while notes are held (since the last note-on); a new
    // note brings them back down quickly but smoothly.
    sinceNote = std::min(sinceNote + tick, 1000.0f);
    const auto follow = [&](float& v, float to) { v += (to < v ? 0.25f : 1.0f) * (to - v); };
    follow(sustainWarble, 1.0f - std::exp(-sinceNote / 0.8f));
    const float ef = now[lim::fade];
    const float tau = 4.0f - 2.0f * std::min(ef, 1.2f);
    follow(sustainFade, 1.0f - std::exp(-sinceNote / tau));

    // Ghost: level, and its constant detune (pitch up on even instruments, down on odd ones).
    const float eg = now[lim::ghost];
    ghostTarget = eg > 0.0f ? dbGain(-22.0f + 16.0f * std::min(eg, 1.25f)) : 0.0f;
    const float ghostCents = (4.0f + 9.0f * std::min(eg, 1.2f)) * (instrument % 2 == 0 ? 1.0f : -1.0f);
    ghostStep = cents(ghostCents) - 1.0f;

    // Warble: slow uneven swell and a faint chorus copy.
    const float ew = now[lim::warble];
    warbleChorusTarget = 0.3f * std::min(ew, 1.4f) * sustainWarble;
    {
        const double h = unit(salt(21));
        const float lfo = float(0.7 * std::sin(std::fmod(twoPiD * (0.41 + 0.12 * h) * t, twoPiD))) + 0.3f * valueNoise(salt(22), t / 1.1);
        const float g = 1.0f + 0.22f * std::min(ew, 1.4f) * sustainWarble * lfo;
        warbleGainStep = (g - warbleGain) / 64.0f;
        const float d = float((0.011 + 0.0035 * std::sin(std::fmod(twoPiD * (0.33 + 0.1 * h) * t, twoPiD))) * sr);
        chorusDelayStep = (d - chorusDelay) / 64.0f;
    }

    // Fade: the highs close in over seconds of holding, plus a slight blur.
    // (Audio input, VACANCY FX: no held notes to fade; its own Fade makes the sound fade away and return.)
    fadeMixTarget = audioInput ? 0.0f : std::min(1.0f, 5.0f * ef);
    blurMixTarget = audioInput ? 0.0f : 0.5f * std::min(ef, 1.3f) * sustainFade;
    {
        const float octaves = 3.5f * ef * sustainFade;
        fadeOctaves += (octaves < fadeOctaves ? 0.3f : 1.0f) * (octaves - fadeOctaves);
        const float hz = std::max(400.0f, 20000.0f * std::exp2(-fadeOctaves));
        fadeCoefStep = (onePoleCoef(hz, sr) - fadeCoef) / 64.0f;
    }

    // Wear.
    const float ea = now[lim::wear];
    wearMixTarget = std::min(1.0f, 5.0f * ea);
    driveTarget = 1.0f + 2.5f * ea;
    lowCoef = onePoleCoef(20000.0 * std::exp2(-2.3 * double(ea)), sr);
    highCoef = onePoleCoef(20.0 * (1.0 + 6.0 * double(ea)), sr);
    // Worn, not quieter: a slow (1 s) level match of what the filters and saturation took away,
    // held while the instrument is (nearly) silent.
    if (wearIn > 1.0e-7f) wearGainTarget = std::clamp(std::sqrt(wearIn / std::max(wearOut, 1.0e-12f)), 1.0f, 2.8f);
    if (ea <= 0.0f) wearGainTarget = 1.0f;
    // Unevenness takes some notes down: the instrument as a whole is brought back up by its average
    // loss (about 2.2 dB at 100 %), so only the uneven notes are quieter than before.
    // (Audio input: Unevenness is the FX's own slow level and tone fluctuation, no make-up here.)
    outGainTarget = audioInput ? 1.0f : dbGain(2.2f * std::min(now[lim::uneven], lim::maxEffective));
    hissLevel = ea > 0.0f ? dbGain(-70.0f + 14.0f * std::min(ea, 1.25f)) : 0.0f;   // 6 dB under the 2026-11 level
    crackleRate = ea > 0.08f ? 5.0f * ea : 0.0f;
    crackleLevel = 0.003f * std::min(ea, 1.5f);   // 6 dB under the 2026-11 level, with the hiss
    if (ea > 0.05f && dropLeft == 0 && dropTarget >= 1.0f) {
        const float perSecond = 0.25f * std::pow(ea, 1.5f);
        if (noise.uniform() < perSecond * tick) {
            dropLeft = int((0.025f + 0.065f * noise.uniform()) * float(sr));
            dropTarget = dbGain(-(8.0f + 16.0f * noise.uniform()) * std::min(1.0f, ea));
            ++dropoutCount;
        }
    }

    // Bypassed exactly when nothing on the audio side is doing anything.
    const bool quiet = ghostTarget == 0.0f && ghostGain < 1.0e-6f && warbleChorusTarget == 0.0f && warbleChorus < 1.0e-6f
                       && std::abs(warbleGain - 1.0f) < 1.0e-6f && std::abs(warbleGainStep) < 1.0e-9f && fadeMixTarget == 0.0f && fadeMix < 1.0e-6f
                       && wearMixTarget == 0.0f && wearMix < 1.0e-6f && dropLeft == 0 && std::abs(dropGain - 1.0f) < 1.0e-6f
                       && outGainTarget == 1.0f && std::abs(outGain - 1.0f) < 1.0e-6f;
    if (quiet) {
        bypassed = true;
        lineLive = false;
        ghostGain = warbleChorus = fadeMix = wearMix = blurMix = 0;
        warbleGain = outGain = 1;
        wearGain = wearGainTarget = 1;
        wearIn = wearOut = 0;
        warbleGainStep = 0;
        dropGain = dropTarget = 1;
        return;
    }
    if (bypassed) {   // waking up: no stale history, filters from rest (their mixes rise from 0)
        bypassed = false;
        resetFilters();
    }
    if (!lineLive && (ghostTarget > 0.0f || warbleChorusTarget > 0.0f)) {
        std::fill(lineL.begin(), lineL.end(), 0.0f);
        std::fill(lineR.begin(), lineR.end(), 0.0f);
        lineLive = true;
        // The copies read the line up to ~145 ms back: they stay silent until it holds real history
        // (else the ghost would jump from the zeroed line to the playing: a click, Milestone 8).
        lineWarm = int(ghostDelay + ghostWindow) + 4;
    }
}

//==============================================================================
// Notes.
void Liminality::noteOn(Instrument& in, int note, float velocity, std::uint64_t nowS, std::uint64_t clock) {
    if (note < 0 || note > 127) return;
    const auto& e = now;
    const std::uint32_t base = mix(salt(std::uint32_t(note) + 1000u), std::uint32_t(clock) ^ (std::uint32_t(clock >> 32) * 0x632BE5ABu));
    const auto r = [&](std::uint32_t k) { return unit(mix(base, k)); };                       // this strike
    const auto key = [&](std::uint32_t k) { return unit(mix(salt(5000u + k), std::uint32_t(note))); };   // this key, always
    Decision d;
    if (e[lim::detune] > 0.0f) {   // mostly the key's own mistuning, a little per strike
        const float strike = r(1) - r(2);
        d.cents += 30.0f * e[lim::detune] * (0.6f * (2.0f * key(1) - 1.0f) + 0.4f * strike);
    }
    if (e[lim::sour] > 0.0f) {
        const float chance = std::min(0.35f, 0.16f * std::pow(e[lim::sour], 1.3f));
        if (r(3) < chance) {
            d.sour = true;
            const float size = 32.0f + 22.0f * std::min(e[lim::sour], 1.0f) + 8.0f * r(4);
            d.cents += (r(5) < 0.5f ? -1.0f : 1.0f) * size;
        }
    }
    d.cents = std::clamp(d.cents, -85.0f, 85.0f);   // always nearer this note than the next key
    if (e[lim::uneven] > 0.0f) {
        const float k2 = key(2), s2 = r(6);
        const float quietness = 0.55f * k2 * k2 * k2 * k2 + 0.45f * s2 * s2 * s2 * s2;   // most notes near full, a few clearly quieter
        const float dB = -11.0f * e[lim::uneven] * quietness;
        d.velocityScale = std::pow(10.0f, dB / 20.0f / 1.6f);   // the instruments' level goes as ~velocity^1.6
        d.tone = std::clamp(1.2f * e[lim::uneven] * (0.6f * (2.0f * key(3) - 1.0f) + 0.4f * (2.0f * r(7) - 1.0f)), -1.0f, 1.0f);
    }
    if (e[lim::timing] > 0.0f) d.delay = int(0.06f * e[lim::timing] * std::pow(r(8), 1.6f) * float(sr));
    last = d;
    const NoteShape shape { d.cents, d.tone, base };
    const float v = std::clamp(velocity * d.velocityScale, 0.01f, 1.0f);
    // A note never overtakes an earlier event of the same key.
    const std::uint64_t due = std::max(nowS + std::uint64_t(d.delay), lastDue[std::size_t(note)]);
    noteDelay[std::size_t(note)] = std::uint32_t(due - nowS);
    if (due <= nowS || queued >= int(queue.size())) {
        startNote(in, note, v, shape);
        return;
    }
    lastDue[std::size_t(note)] = due;
    queue[std::size_t(queued++)] = { due, note, v, shape, true };
}

void Liminality::startNote(Instrument& in, int note, float velocity, const NoteShape& shape) {
    in.setLiminalPitch(pitch);
    in.shapeNextNote(shape);
    in.noteOn(note, velocity);
    sinceNote = 0;
}

void Liminality::noteOff(Instrument& in, int note, std::uint64_t nowS) {
    if (note < 0 || note > 127) return;
    const std::uint64_t due = std::max(nowS + noteDelay[std::size_t(note)], lastDue[std::size_t(note)]);
    if (due <= nowS || queued >= int(queue.size())) {
        in.noteOff(note);
        return;
    }
    lastDue[std::size_t(note)] = due;
    queue[std::size_t(queued++)] = { due, note, 0.0f, {}, false };
}

void Liminality::flush(Instrument&) {
    queued = 0;
    noteDelay.fill(0);
    lastDue.fill(0);
}

std::uint64_t Liminality::nextDue() const {
    std::uint64_t m = std::numeric_limits<std::uint64_t>::max();
    for (int i = 0; i < queued; ++i) m = std::min(m, queue[std::size_t(i)].due);
    return m;
}

void Liminality::fireDue(Instrument& in, std::uint64_t nowS) {
    while (queued > 0) {
        int first = 0;
        for (int i = 1; i < queued; ++i)
            if (queue[std::size_t(i)].due < queue[std::size_t(first)].due) first = i;
        const Pending p = queue[std::size_t(first)];
        if (p.due > nowS) return;
        for (int i = first; i + 1 < queued; ++i) queue[std::size_t(i)] = queue[std::size_t(i + 1)];   // keeps the order
        --queued;
        if (p.on) startNote(in, p.note, p.velocity, p.shape);
        else in.noteOff(p.note);
    }
}

//==============================================================================
// Audio.
float Liminality::read(const std::vector<float>& line, float delay) const {
    const float p = float(write) - delay;
    const float fl = std::floor(p);
    const int i0 = int(fl) & lineMask;
    const float fr = p - fl;
    const float a = line[std::size_t(i0)], b = line[std::size_t((i0 + 1) & lineMask)];
    return a + fr * (b - a);
}

void Liminality::process(float* left, float* right, int n) {
    if (bypassed) return;
    constexpr float allpassGain = 0.55f;
    const float window0 = ghostWindow, half = 0.5f * ghostWindow, toTable = float(window.size() - 1) / ghostWindow;
    for (int i = 0; i < n; ++i) {
        const float inL = left[i], inR = right[i];
        float l = inL, r = inR;
        if (lineLive) {
            lineL[std::size_t(write)] = inL;
            lineR[std::size_t(write)] = inR;
        }
        // Ghost: two read heads sweeping through a 50 ms window at the detune's rate, cross-faded
        // (sin^2 + cos^2 = 1), darker, and placed a little to the other side.
        if (lineWarm > 0) --lineWarm;
        ghostGain += smooth * ((lineWarm > 0 ? 0.0f : ghostTarget) - ghostGain);
        if (lineLive && ghostGain > 1.0e-6f) {
            ghostPos -= ghostStep;
            if (ghostPos < 0.0f) ghostPos += window0;
            if (ghostPos >= window0) ghostPos -= window0;
            float other = ghostPos + half;
            if (other >= window0) other -= window0;
            const float u = ghostPos * toTable;
            const int k = std::min(int(u), int(window.size()) - 2);
            const float w1 = window[std::size_t(k)] + (u - float(k)) * (window[std::size_t(k + 1)] - window[std::size_t(k)]);
            const float w2 = 1.0f - w1;
            const float gl = ghostToneL.lp(read(lineL, ghostDelay + ghostPos) * w1 + read(lineL, ghostDelay + other) * w2);
            const float gr = ghostToneR.lp(read(lineR, ghostDelay + ghostPos) * w1 + read(lineR, ghostDelay + other) * w2);
            l += ghostGain * (0.4f * gl + 0.6f * gr);
            r += ghostGain * (0.6f * gl + 0.4f * gr);
        }
        // Warble: the chorus copy, then the swell.
        warbleChorus += smooth * ((lineWarm > 0 ? 0.0f : warbleChorusTarget) - warbleChorus);
        chorusDelay += chorusDelayStep;
        if (lineLive && warbleChorus > 1.0e-6f) {
            const float cl = read(lineL, chorusDelay), cr = read(lineR, chorusDelay * 1.13f);
            l = l * (1.0f - 0.35f * warbleChorus) + 0.8f * warbleChorus * cr;
            r = r * (1.0f - 0.35f * warbleChorus) + 0.8f * warbleChorus * cl;
        }
        warbleGain += warbleGainStep;
        l *= warbleGain;
        r *= warbleGain;
        // Fade: two one-poles closing in, and a slight blur (two allpasses per side).
        fadeMix += smooth * (fadeMixTarget - fadeMix);
        blurMix += smooth * (blurMixTarget - blurMix);
        fadeCoef += fadeCoefStep;
        if (fadeMix > 1.0e-6f) {
            fadeL1 += fadeCoef * (l - fadeL1);
            fadeL2 += fadeCoef * (fadeL1 - fadeL2);
            fadeR1 += fadeCoef * (r - fadeR1);
            fadeR2 += fadeCoef * (fadeR1 - fadeR2);
            float bl = fadeL2, br = fadeR2;
            const auto allpass = [&](Allpass& a, float x) {
                const float d = a.line[std::size_t(a.pos)];
                const float y = d - allpassGain * x;
                a.line[std::size_t(a.pos)] = x + allpassGain * y;
                if (++a.pos >= int(a.line.size())) a.pos = 0;
                return y;
            };
            const float smearL = allpass(blur[1], allpass(blur[0], bl));
            const float smearR = allpass(blur[3], allpass(blur[2], br));
            bl += blurMix * (smearL - bl);
            br += blurMix * (smearR - br);
            l += fadeMix * (bl - l);
            r += fadeMix * (br - r);
        }
        // Wear: lost highs and lows, soft saturation, hiss and crackle under the playing.
        wearMix += smooth * (wearMixTarget - wearMix);
        drive += smooth * (driveTarget - drive);
        if (wearMix > 1.0e-6f) {
            const float level = std::max(std::abs(inL), std::abs(inR));
            envelope += (level > envelope ? envAttack : envRelease) * (level - envelope);
            const float gate = std::clamp((envelope - 0.001f) * 40.0f, 0.0f, 1.0f);   // under the playing only
            wl1 += lowCoef * (l - wl1);
            wl2 += lowCoef * (wl1 - wl2);
            wr1 += lowCoef * (r - wr1);
            wr2 += lowCoef * (wr1 - wr2);
            hl1 += highCoef * (wl2 - hl1);
            const float xl = wl2 - hl1;
            hl2 += highCoef * (xl - hl2);
            hr1 += highCoef * (wr2 - hr1);
            const float xr = wr2 - hr1;
            hr2 += highCoef * (xr - hr2);
            float yl = softClip(drive * (xl - hl2)) / drive;
            float yr = softClip(drive * (xr - hr2)) / drive;
            wearIn += wearEnvCoef * (l * l + r * r - wearIn);
            wearOut += wearEnvCoef * (yl * yl + yr * yr - wearOut);
            wearGain += smooth * (wearGainTarget - wearGain);
            yl *= wearGain;
            yr *= wearGain;
            if (gate > 1.0e-4f) {
                const float nl = noise.bipolar(), nr = noise.bipolar();
                hissZ1 += 0.6f * (nl - hissZ1);   // bright, not harsh
                hissZ2 += 0.6f * (nr - hissZ2);
                hissH1 += 0.06f * (hissZ1 - hissH1);
                hissH2 += 0.06f * (hissZ2 - hissH2);
                yl += (hissZ1 - hissH1) * hissLevel * gate;
                yr += (hissZ2 - hissH2) * hissLevel * gate;
                if (crackleRate > 0.0f && noise.uniform() < crackleRate / float(sr))
                    crackle = crackleLevel * (0.3f + noise.uniform()) * (noise.uniform() < 0.5f ? -1.0f : 1.0f);
            }
            if (std::abs(crackle) > 1.0e-7f) {
                const float c = crackle - crackleZ;   // a tiny tick, its low end removed
                crackleZ += 0.3f * (crackle - crackleZ);
                crackle *= 0.8f;
                yl += c * gate;
                yr += 0.6f * c * gate;
            } else {
                crackle = 0;
            }
            l += wearMix * (yl - l);
            r += wearMix * (yr - r);
        }
        // Dropouts (Wear): a quick dip and back.
        if (dropLeft > 0 && --dropLeft == 0) dropTarget = 1.0f;
        dropGain += (dropTarget < dropGain ? dropCoefDown : dropCoefUp) * (dropTarget - dropGain);
        outGain += smooth * (outGainTarget - outGain);
        l *= dropGain * outGain;
        r *= dropGain * outGain;
        left[i] = l;
        right[i] = r;
        if (lineLive) write = (write + 1) & lineMask;
    }
}

} // namespace liminal::dsp
