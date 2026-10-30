#include "DSP/Environment.h"
#include <cmath>

namespace liminal::dsp {

namespace {
// The five environments of Milestone 2a. Why each number is what it is: docs/ENVIRONMENTS.md.
Profile makeHospital() {
    Profile p { "VACANT HOSPITAL", "EAST WING, LEVEL 2", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // a long, hard, sterile corridor: flutter between the walls
    r.rt60Low = 2.4f; r.rt60High = 1.5f; r.crossoverHz = 1800.0f; r.size = 1.0f; r.preDelayMs = 22.0f;
    r.diffusion = 0.45f; r.modulation = 0.15f; r.pattern = Space::Pattern::flutter; r.reflections = 12;
    r.firstMs = 6.0f; r.spacingMs = 11.0f; r.reflectionDecay = 0.8f; r.reflectionLevel = 0.45f; r.width = 0.9f;
    r.lowCutHz = 90.0f; r.highCutHz = 7000.0f;
    auto& c = p.colour;   // dark, filtered, a little unstable, cold resonance, set back
    c.highPassHz = 110.0f; c.lowPassHz = 3200.0f; c.peakHz = 2400.0f; c.peakQ = 3.0f; c.peakDb = 6.0f;
    c.driftCents = 9.0f; c.wowHz = 0.17f; c.wowCents = 4.0f;
    c.width = 1.2f; c.dryDb = -4.0f;
    auto& e = p.echo;     // distant stereo reflections from the corridor's far end
    e.leftMs = 241.0f; e.rightMs = 367.0f; e.feedback = 0.38f; e.level = 0.42f; e.lowPassHz = 2200.0f; e.highPassHz = 300.0f;
    p.wetDb = 0.0f;
    return p;
}
Profile makeMall() {
    Profile p { "EMPTY SHOPPING MALL", "CENTRAL ATRIUM, AFTER HOURS", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // enormous and diffuse: late, soft, endless
    r.rt60Low = 6.8f; r.rt60High = 3.6f; r.crossoverHz = 1200.0f; r.size = 2.4f; r.preDelayMs = 60.0f;
    r.diffusion = 0.85f; r.modulation = 0.6f; r.pattern = Space::Pattern::sparse; r.reflections = 6;
    r.firstMs = 40.0f; r.spacingMs = 22.0f; r.reflectionDecay = 0.75f; r.reflectionLevel = 0.2f; r.width = 1.1f;
    r.lowCutHz = 50.0f; r.highCutHz = 5500.0f;
    auto& c = p.colour;   // washed out: soft highs, a wide chorus wash, far away
    c.highPassHz = 60.0f; c.lowPassHz = 7000.0f; c.shelfHz = 3000.0f; c.shelfDb = -7.0f;
    c.chorusHz = 0.37f; c.chorusCents = 7.0f; c.chorusMix = 0.6f; c.driftCents = 3.0f;
    c.width = 1.45f; c.dryDb = -8.0f;
    p.wetDb = 4.0f;
    p.ambienceTrim = 0.6f;
    return p;
}
Profile makeSchool() {
    Profile p { "ABANDONED SCHOOL", "B CORRIDOR, NEAR ROOM 14", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // a narrow hallway: short, dark, tight flutter
    r.rt60Low = 1.5f; r.rt60High = 0.6f; r.crossoverHz = 900.0f; r.size = 0.55f; r.preDelayMs = 6.0f;
    r.diffusion = 0.35f; r.modulation = 0.1f; r.pattern = Space::Pattern::flutter; r.reflections = 14;
    r.firstMs = 3.0f; r.spacingMs = 6.5f; r.reflectionDecay = 0.82f; r.reflectionLevel = 0.55f; r.width = 0.5f;
    r.lowCutHz = 120.0f; r.highCutHz = 4500.0f;
    auto& c = p.colour;   // nostalgic: detuned, worn tape, boxy, narrow
    c.highPassHz = 210.0f; c.lowPassHz = 4300.0f; c.peakHz = 1100.0f; c.peakQ = 1.1f; c.peakDb = 4.0f;
    c.wowHz = 0.55f; c.wowCents = 7.0f; c.flutterHz = 6.5f; c.flutterCents = 3.5f;
    c.chorusHz = 0.23f; c.chorusCents = 11.0f; c.chorusMix = 0.4f;
    c.drive = 0.5f; c.width = 0.5f; c.dryDb = -1.5f;
    auto& e = p.echo;     // the opposite wall, close: a short dark slap
    e.leftMs = 37.0f; e.rightMs = 44.0f; e.feedback = 0.3f; e.level = 0.38f; e.lowPassHz = 2500.0f; e.highPassHz = 250.0f; e.pingPong = false;
    p.wetDb = -1.0f;
    return p;
}
Profile makeGarage() {
    Profile p { "UNDERGROUND PARKING GARAGE", "LEVEL P3, ROW F", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // concrete: strong, dense early reflections, heavy lows, dull highs
    r.rt60Low = 3.6f; r.rt60High = 1.4f; r.crossoverHz = 700.0f; r.size = 1.6f; r.preDelayMs = 10.0f;
    r.diffusion = 0.3f; r.modulation = 0.0f; r.pattern = Space::Pattern::dense; r.reflections = 16;
    r.firstMs = 4.0f; r.spacingMs = 5.0f; r.reflectionDecay = 0.88f; r.reflectionLevel = 0.85f; r.width = 1.0f;
    r.lowCutHz = 30.0f; r.highCutHz = 3800.0f;
    auto& c = p.colour;   // heavy low-mid resonance, darkened top
    c.highPassHz = 35.0f; c.lowPassHz = 2500.0f; c.lowPassQ = 0.9f; c.peakHz = 170.0f; c.peakQ = 0.9f; c.peakDb = 9.0f;
    c.width = 1.0f; c.dryDb = -4.0f;
    auto& e = p.echo;     // the far wall of the bay
    e.leftMs = 96.0f; e.rightMs = 113.0f; e.feedback = 0.18f; e.level = 0.25f; e.lowPassHz = 1600.0f; e.highPassHz = 80.0f; e.pingPong = false;
    p.wetDb = 1.0f;
    p.ambienceTrim = 0.4f;
    return p;
}
Profile makePool() {
    Profile p { "ABANDONED INDOOR POOL", "MAIN HALL, DEEP END", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // tiles and water: long and bright
    r.rt60Low = 4.6f; r.rt60High = 4.2f; r.crossoverHz = 3000.0f; r.size = 1.9f; r.preDelayMs = 32.0f;
    r.diffusion = 0.7f; r.modulation = 0.35f; r.pattern = Space::Pattern::sparse; r.reflections = 9;
    r.firstMs = 18.0f; r.spacingMs = 12.0f; r.reflectionDecay = 0.85f; r.reflectionLevel = 0.6f; r.width = 1.3f;
    r.lowCutHz = 70.0f; r.highCutHz = 14000.0f;
    auto& c = p.colour;   // bright, glassy, spacious
    c.highPassHz = 90.0f; c.shelfHz = 5000.0f; c.shelfDb = 6.0f; c.peakHz = 3200.0f; c.peakQ = 1.0f; c.peakDb = 2.0f;
    c.width = 1.45f; c.dryDb = -4.0f;
    auto& e = p.echo;     // distinct echoes off the far tiled wall
    e.leftMs = 173.0f; e.rightMs = 291.0f; e.feedback = 0.45f; e.level = 0.5f; e.lowPassHz = 7500.0f; e.highPassHz = 220.0f;
    p.wetDb = 2.0f;
    return p;
}
const std::array<Profile, placeCount> profiles { makeHospital(), makeMall(), makeSchool(), makeGarage(), makePool() };

// Delay-line swing (samples) for a sinusoidal pitch wobble of `cents` at `hz`.
float swing(float cents, float hz, double sr) {
    return cents <= 0.0f ? 0.0f : float(cents * 0.693147 / (1200.0 * 6.283185 * std::max(0.05f, hz)) * sr);
}
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }
} // namespace

const Profile& profile(Place p) { return profiles[std::size_t(std::clamp(int(p), 0, placeCount - 1))]; }

//==============================================================================
void Colour::prepare(double sampleRate) {
    sr = sampleRate;
    lineL.assign(std::size_t(0.06 * sr) + 4, 0.0f);
    lineR.assign(lineL.size(), 0.0f);
    amountCoef = float(1.0 - std::exp(-1.0 / (0.02 * sr)));
    reset();
}

void Colour::configure(const Profile::Colour& colour, unsigned seed) {
    c = colour;
    rng.state = 0xB5297A4Du ^ (seed * 0x68E31DA4u);
    drift.start(rng);
    reset();
}

void Colour::reset() {
    for (auto* b : { &hpL, &hpR, &peakL, &peakR, &shelfL, &shelfR, &lpL, &lpR }) b->clear();
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    write = clock = 0;
    wowPhase = flutterPhase = 0;
    chorusPhase = 0.25f;
    driftNow = 0;
    filtersAt = -1;
    updateFilters();
}

void Colour::updateFilters() {
    const float a = amount;
    const double nyquistSafe = 0.45 * sr;
    hpL.highPass(10.0 * std::pow(double(c.highPassHz) / 10.0, double(a)), 0.707, sr);
    lpL.lowPass(std::min(nyquistSafe, 22000.0 * std::pow(double(c.lowPassHz) / 22000.0, double(a))), 0.707 + (c.lowPassQ - 0.707) * a, sr);
    peakL.peak(c.peakHz, c.peakQ, c.peakDb * a, sr);
    shelfL.highShelf(c.shelfHz, c.shelfDb * a, sr);
    // Same coefficients on the right.
    const auto copy = [](Biquad& to, const Biquad& from) { to.b0 = from.b0; to.b1 = from.b1; to.b2 = from.b2; to.a1 = from.a1; to.a2 = from.a2; };
    copy(hpR, hpL);
    copy(lpR, lpL);
    copy(peakR, peakL);
    copy(shelfR, shelfL);
    filtersAt = a;
}

void Colour::process(float* left, float* right, int n) {
    const int size = int(lineL.size());
    const auto read = [&](const std::vector<float>& line, float delay) {
        float pos = float(write) - delay;
        if (pos < 0.0f) pos += float(size);
        const int i0 = int(pos);
        const float f = pos - float(i0);
        const float y0 = line[std::size_t(i0 % size)], y1 = line[std::size_t((i0 + 1) % size)];
        return y0 + f * (y1 - y0);
    };
    for (int i = 0; i < n; ++i) {
        if (clock == 0) {   // control rate: filters follow the amount, the drift wanders
            if (std::abs(amount - filtersAt) > 0.0005f) updateFilters();
            driftNow = drift.tick(rng, 0.3f, float(32.0 / sr));
            clock = 32;
        }
        --clock;
        amount += amountCoef * (amountTarget - amount);
        if (std::abs(amountTarget - amount) < 1.0e-3f) amount = amountTarget;
        const float a = amount;
        float l = left[i], r = right[i];
        if (a <= 0.0f && filtersAt <= 0.0f) {   // untouched: Milestone 1's sound
            lineL[std::size_t(write)] = l;
            lineR[std::size_t(write)] = r;
            if (++write >= size) write = 0;
            bypassed = true;
            continue;
        }
        if (bypassed) {   // coming back: the filters forget what they held before
            for (auto* b : { &hpL, &hpR, &peakL, &peakR, &shelfL, &shelfR, &lpL, &lpR }) b->clear();
            bypassed = false;
        }
        l = lpL.tick(shelfL.tick(peakL.tick(hpL.tick(l))));
        r = lpR.tick(shelfR.tick(peakR.tick(hpR.tick(r))));
        // Pitch: wow, flutter and a random drift, as a delay that swings around a centre.
        wowPhase += c.wowHz / float(sr);
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += c.flutterHz / float(sr);
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        chorusPhase += c.chorusHz / float(sr);
        if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
        const float wow = swing(c.wowCents * a, c.wowHz, sr), flutter = swing(c.flutterCents * a, c.flutterHz, sr),
                    wander = swing(c.driftCents * a, 0.3f, sr);
        const float centre = wow + flutter + wander;
        const float d = centre + wow * sinCycle(wowPhase) + flutter * sinCycle(flutterPhase) + wander * driftNow;
        lineL[std::size_t(write)] = l;
        lineR[std::size_t(write)] = r;
        l = read(lineL, d);
        r = read(lineR, d);
        if (c.chorusMix > 0.0f) {   // a second, detuned player a few ms behind (opposite swing per side)
            const float depth = swing(c.chorusCents * a, c.chorusHz, sr);
            const float base = centre + depth + float(0.007 * sr) * a;
            float other = chorusPhase + 0.5f;
            if (other >= 1.0f) other -= 1.0f;
            const float cl = read(lineL, base + depth * sinCycle(chorusPhase));
            const float cr = read(lineR, base + depth * sinCycle(other));
            const float m = c.chorusMix * a;
            l = l * (1.0f - 0.45f * m) + cl * 0.65f * m;
            r = r * (1.0f - 0.45f * m) + cr * 0.65f * m;
        }
        if (++write >= size) write = 0;
        if (c.drive > 0.0f) {   // tape-like: loud passages round off, quiet ones pass
            const float g = 1.0f + 8.0f * c.drive * a;
            l = softClip(l * g) / g;
            r = softClip(r * g) / g;
        }
        const float w = 1.0f + (c.width - 1.0f) * a;
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
        left[i] = mid + side;
        right[i] = mid - side;
    }
}

//==============================================================================
void Echo::prepare(double sampleRate) {
    sr = sampleRate;
    lineL.assign(std::size_t(1.2 * sr) + 4, 0.0f);
    lineR.assign(lineL.size(), 0.0f);
    reset();
}

void Echo::configure(const Profile::Echo& echo) {
    e = echo;
    const int size = int(lineL.size());
    delayL = std::clamp(int(e.leftMs * 0.001 * sr), 1, size - 1);
    delayR = std::clamp(int(e.rightMs * 0.001 * sr), 1, size - 1);
    lpL.setHz(e.lowPassHz, sr);
    lpR.setHz(e.lowPassHz, sr);
    hpL.setHz(e.highPassHz, sr);
    hpR.setHz(e.highPassHz, sr);
    reset();
}

void Echo::reset() {
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    write = 0;
    lpL.z = lpR.z = hpL.z = hpR.z = 0;
}

Stereo Echo::tick(Stereo in, float amount) {
    const int size = int(lineL.size());
    const float yl = lineL[std::size_t((write - delayL + size) % size)];
    const float yr = lineR[std::size_t((write - delayR + size) % size)];
    // The loop's band-pass: each repeat darker and thinner, like a real far wall.
    const float fl = hpL.hp(lpL.lp(yl)), fr = hpR.hp(lpR.lp(yr));
    const float fb = e.feedback;
    const float level = e.level * amount;
    const float mono = 0.5f * (in.l + in.r);
    lineL[std::size_t(write)] = (e.pingPong ? mono : in.l) * level + fb * (e.pingPong ? fr : fl);
    lineR[std::size_t(write)] = (e.pingPong ? 0.0f : in.r * level) + fb * (e.pingPong ? fl : fr);
    if (++write >= size) write = 0;
    return { fl, fr };
}

//==============================================================================
void EnvironmentChain::prepare(double sampleRate) {
    sr = sampleRate;
    dryColour.prepare(sr);
    sendColour.prepare(sr);
    echo.prepare(sr);
    room.prepare(sr);
    ambience.prepare(sr);
    bufL.assign(1024, 0.0f);
    bufR.assign(1024, 0.0f);
    sbufL.assign(1024, 0.0f);
    sbufR.assign(1024, 0.0f);
    amountCoef = float(1.0 - std::exp(-1.0 / (0.02 * sr)));
    liveCoef = float(1.0 - std::exp(-1.0 / (0.03 * sr)));
    ambienceCoef = float(1.0 - std::exp(-1.0 / (0.6 * sr)));   // about 2 s to fade across
    killStep = float(1.0 / (0.03 * sr));
    quietLimit = int(sr);
    configure(current, 1);
}

void EnvironmentChain::configure(Place place, unsigned seed) {
    current = place;
    p = &profile(place);
    dryColour.configure(p->colour, seed);
    sendColour.configure(p->colour, seed);   // same seed: both paths wobble together
    echo.configure(p->echo);
    room.configure(p->room, seed + unsigned(place) * 7919u);
    room.reset();
    ambience.trim = p->ambienceTrim;
    ambience.configure(Ambience::Place(int(place)), seed);
    wetGain = dbGain(p->wetDb);
    quietFor = 0;
    killing = false;
    outFade = 1;
}

void EnvironmentChain::process(const float* instL, const float* instR, const float* sendL, const float* sendR, const float* ambienceGain,
                               const float* space, float* outL, float* outR, float* ambienceOut, int n) {
    n = std::min(n, int(bufL.size()));
    for (int i = 0; i < n; ++i) {
        live += liveCoef * (liveTarget - live);
        bufL[std::size_t(i)] = instL[i] * live;
        bufR[std::size_t(i)] = instR[i] * live;
        sbufL[std::size_t(i)] = sendL[i] * live;
        sbufR[std::size_t(i)] = sendR[i] * live;
    }
    dryColour.process(bufL.data(), bufR.data(), n);
    sendColour.process(sbufL.data(), sbufR.data(), n);
    const float dryTarget = p->colour.dryDb;
    for (int i = 0; i < n; ++i) {
        amount += amountCoef * (amountTarget - amount);
        if (std::abs(amountTarget - amount) < 1.0e-3f) amount = amountTarget;
        ambienceFade += ambienceCoef * (liveTarget - ambienceFade);
        if (killing) outFade = std::max(0.0f, outFade - killStep);
        const float dry = dbGain(dryTarget * amount);
        const float ag = ambienceGain[i] * ambienceFade;
        const auto amb = ambience.tick();
        const Stereo send { sbufL[std::size_t(i)], sbufR[std::size_t(i)] };
        const Stereo echoed = echo.tick(send, amount);
        const Stereo wet = room.tick({ send.l + 0.5f * echoed.l + amb.send.l * ag, send.r + 0.5f * echoed.r + amb.send.r * ag });
        const float sp = space[i];
        const float keep = 1.0f - 0.45f * sp;
        const float ambL = amb.dry.l * ag, ambR = amb.dry.r * ag;
        const float l = (keep * (bufL[std::size_t(i)] * dry + ambL) + echoed.l + sp * wetGain * wet.l) * outFade;
        const float r = (keep * (bufR[std::size_t(i)] * dry + ambR) + echoed.r + sp * wetGain * wet.r) * outFade;
        outL[i] += l;
        outR[i] += r;
        ambienceOut[i] += 0.5f * (ambL * ambL + ambR * ambR);
        quietFor = std::abs(l) + std::abs(r) < 3.0e-5f ? quietFor + 1 : 0;
    }
}

//==============================================================================
void Environments::prepare(double sampleRate, Place initial) {
    for (auto& c : chains) c.prepare(sampleRate);
    changes = 0;
    active = 0;
    pending = -1;
    chains[0].configure(initial, 1);
    chains[0].setAmount(amount);
    chains[0].snapAmount();
    chains[0].setLive(true);
    chains[0].snapLive();
    chains[1].setLive(false);
    chains[1].snapLive();
    running = { true, false };
}

void Environments::setAmount(float a) {
    amount = std::clamp(a, 0.0f, 1.0f);
    for (auto& c : chains) c.setAmount(amount);
}

void Environments::select(Place place) {
    if (pending >= 0) { pending = int(place); return; }   // starts once the other chain is clear
    if (place == chains[std::size_t(active)].place()) return;
    const int other = 1 - active;
    if (running[std::size_t(other)] && !chains[std::size_t(other)].silent()) {
        chains[std::size_t(other)].kill();   // still ringing from an earlier change: fade it quickly first
        pending = int(place);
        return;
    }
    start(place);
}

void Environments::start(Place place) {
    const int other = 1 - active;
    auto& next = chains[std::size_t(other)];
    next.configure(place, ++changes + 1);
    next.setAmount(amount);
    next.snapAmount();
    next.setLive(false);
    next.snapLive();
    next.setLive(true);   // fades in
    chains[std::size_t(active)].setLive(false);   // stops hearing the instruments; its tail rings out
    running[std::size_t(other)] = true;
    active = other;
}

void Environments::process(const float* instL, const float* instR, const float* sendL, const float* sendR, const float* ambienceGain,
                           const float* space, float* outL, float* outR, float* ambienceOut, int n) {
    for (int c = 0; c < 2; ++c)
        if (running[std::size_t(c)]) chains[std::size_t(c)].process(instL, instR, sendL, sendR, ambienceGain, space, outL, outR, ambienceOut, n);
    const int other = 1 - active;
    auto& old = chains[std::size_t(other)];
    if (running[std::size_t(other)] && (old.silent() || old.killed())) running[std::size_t(other)] = false;
    if (pending >= 0 && !running[std::size_t(other)]) {
        const auto place = Place(pending);
        pending = -1;
        if (place != chains[std::size_t(active)].place()) start(place);
    }
}

} // namespace liminal::dsp
