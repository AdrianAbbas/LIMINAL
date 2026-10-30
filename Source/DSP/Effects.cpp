#include "DSP/Effects.h"
#include <cmath>

namespace liminal::dsp {

namespace {
// Each place's default effects (for the piano), then per-instrument adjustments. Reasons
// per place: docs/EFFECTS.md.
PlaceEffects basePlace(Place p) {
    PlaceEffects e;
    switch (p) {
    case Place::hospital:     // cold filtering, short reflections, a far-off stereo repeat
        e.drift = 0.25f; e.echo = 0.12f; e.echoMs = 420; e.echoFeedback = 0.3f; e.lowOctaves = 1.0f; e.highOctaves = 0.8f; e.reverbBias = -0.3f; break;
    case Place::mall:         // long diffuse reverb, high-frequency softening, gentle detuning
        e.drift = 0.2f; e.tape = 0.1f; e.chorus = 0.3f; e.grain = 0.08f; e.grainSize = 0.8f; e.lowOctaves = 1.2f; e.reverbBias = 0.8f; break;
    case Place::school:       // a worn cassette: wow, flutter, hiss, narrow and boxy
        e.drift = 0.3f; e.tape = 0.6f; e.chorus = 0.15f; e.echo = 0.1f; e.echoMs = 180; e.echoFeedback = 0.2f; e.lowOctaves = 0.8f; e.highOctaves = 0.5f; e.reverbBias = -0.2f; break;
    case Place::garage:       // concrete: a dull slap, heavy, dark
        e.tape = 0.1f; e.echo = 0.2f; e.echoMs = 300; e.echoFeedback = 0.25f; e.lowOctaves = 1.5f; e.reverbBias = 0.3f; break;
    case Place::pool:         // bright tiled reverb, strong echoes, minimal tape
        e.echo = 0.35f; e.echoMs = 340; e.echoFeedback = 0.45f; e.reverbBias = 0.6f; e.highOctaves = 0.4f; break;
    case Place::hotel:        // subtle tape degradation, intimate, muffled
        e.drift = 0.1f; e.tape = 0.5f; e.lowOctaves = 0.5f; e.reverbBias = -0.5f; break;
    case Place::subway:       // occasional pitch modulation, a metallic tunnel delay
        e.drift = 0.4f; e.echo = 0.3f; e.echoMs = 650; e.echoFeedback = 0.5f; e.grain = 0.1f; e.reverbBias = 0.4f; break;
    case Place::supermarket:  // mild chorus, thin nostalgic tape
        e.drift = 0.15f; e.tape = 0.3f; e.chorus = 0.45f; e.highOctaves = 0.6f; e.reverbBias = -0.6f; break;
    case Place::office:       // soft synthetic wash, a modulated delay, smeared
        e.chorus = 0.35f; e.echo = 0.25f; e.echoMs = 500; e.echoFeedback = 0.55f; e.grain = 0.2f; e.grainSize = 0.6f; e.lowOctaves = 0.7f; e.reverbBias = 0.4f; break;
    case Place::impossible:   // unstable pitch, spectral smearing, fragments
        e.drift = 0.6f; e.chorus = 0.2f; e.grain = 0.5f; e.grainSize = 0.5f; e.echo = 0.2f; e.echoMs = 777; e.echoFeedback = 0.6f; e.reverbBias = 0.8f; break;
    }
    return e;
}
} // namespace

PlaceEffects placeEffects(Place p, int instrument) {
    PlaceEffects e = basePlace(p);
    // How each instrument takes to the place's effects (piano = 1).
    switch (instrument) {
    case 1:   // synth: chorus and tape suit it; less granular
        e.chorus *= 1.3f; e.tape *= 1.2f; e.grain *= 0.7f; e.echoMs *= 0.75f; break;
    case 2:   // glass: echoes and room ring on; drift is obvious on pure tones, keep it lower
        e.echo *= 1.3f; e.drift *= 0.7f; e.reverbBias = std::min(1.0f, e.reverbBias + 0.2f); e.echoMs *= 1.15f; break;
    case 3:   // choir: smears beautifully; chorus would double the singers
        e.grain *= 1.3f; e.grainSize = std::min(1.0f, e.grainSize + 0.2f); e.chorus *= 0.5f; e.drift *= 0.6f; e.echoMs *= 1.3f; break;
    case 4:   // music box: a worn mechanism takes tape and drift
        e.tape *= 1.5f; e.drift *= 1.3f; e.echo *= 1.1f; e.echoMs *= 0.6f; break;
    case 5:   // drone: texture and length rather than echoes
        e.grain *= 1.5f; e.echo *= 0.5f; e.chorus *= 1.2f; e.echoMs *= 1.5f; e.reverbBias = std::min(1.0f, e.reverbBias + 0.1f); break;
    default: break;
    }
    e.tape = std::min(1.0f, e.tape);
    e.drift = std::min(1.0f, e.drift);
    e.grain = std::min(1.0f, e.grain);
    e.echoMs = std::min(1400.0f, e.echoMs);
    return e;
}

namespace {
float swingSamples(float cents, float hz, double sr) {
    return cents <= 0.0f ? 0.0f : float(cents * 0.693147 / (1200.0 * 6.283185 * std::max(0.05f, hz)) * sr);
}
constexpr int controlPeriod = 32;
// Beats per echo for the sync choices (Free, 1/16, 1/8, 1/8 dotted, 1/4, 1/4 dotted, 1/2, 1 bar).
constexpr float syncBeats[] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };
} // namespace

void EffectRack::prepare(double sampleRate, int instrumentIndex) {
    sr = sampleRate;
    instrument = instrumentIndex;
    for (auto& c : colours) c.prepare(sr);
    fadeL.assign(1024, 0.0f);
    fadeR.assign(1024, 0.0f);
    dryL.assign(1024, 0.0f);
    dryR.assign(1024, 0.0f);
    driftLineL.assign(std::size_t(0.04 * sr) + 4, 0.0f);
    driftLineR.assign(driftLineL.size(), 0.0f);
    tapeLineL.assign(std::size_t(0.04 * sr) + 4, 0.0f);
    tapeLineR.assign(tapeLineL.size(), 0.0f);
    chorusLineL.assign(std::size_t(0.04 * sr) + 4, 0.0f);
    chorusLineR.assign(chorusLineL.size(), 0.0f);
    grainLine.assign(std::size_t(2.0 * sr) + 4, 0.0f);
    echoLineL.assign(std::size_t(1.6 * sr) + 4, 0.0f);
    echoLineR.assign(echoLineL.size(), 0.0f);
    smoothCoef = float(1.0 - std::exp(-double(controlPeriod) / (0.025 * sr)));
    colourFadeStep = float(1.0 / (0.06 * sr));
    followerRelease = float(1.0 - std::exp(-1.0 / (0.3 * sr)));
    echoDelayCoef = float(1.0 - std::exp(-1.0 / (0.1 * sr)));
    hissTone.setHz(3500.0f, sr);
    hissToneR.setHz(3500.0f, sr);
    placeKnown = false;
    reset();
}

void EffectRack::reset() {
    for (auto* v : { &driftLineL, &driftLineR, &tapeLineL, &tapeLineR, &chorusLineL, &chorusLineR, &grainLine, &echoLineL, &echoLineR })
        std::fill(v->begin(), v->end(), 0.0f);
    driftWrite = tapeWrite = chorusWrite = grainWrite = echoWrite = 0;
    rng.state = 0x51ED270Bu ^ (unsigned(instrument + 1) * 0x9E3779B9u);
    noise.state = rng.state ^ 0xA5A5A5A5u;
    driftWanderL.start(rng);
    driftWanderR.start(rng);
    driftNowL = driftNowR = 0;
    wowPhase = flutterPhase = chorusPhase = 0;
    for (auto* f : { &tapeLp1L, &tapeLp1R, &tapeLp2L, &tapeLp2R, &echoToneL, &echoToneR, &echoLowL, &echoLowR }) f->z = 0;
    hissTone.z = hissToneR.z = 0;
    follower = 0;
    dropGain = dropTarget = 1;
    dropCountdown = int(sr);
    lowL.clear();
    lowR.clear();
    highL.clear();
    highR.clear();
    filterLowAt = filterHighAt = filterResAt = -1;
    for (auto& g : grains) g.active = false;
    grainCountdown = 0;
    clock = 0;
    for (auto& c : colours) c.reset();
    colourFade = 1;
    now = target;
    echoDelay = echoDelayTarget;
}

void EffectRack::setPlace(Place p) {
    if (placeKnown && p == place) return;
    const unsigned seed = 0x2F6Bu + unsigned(instrument) * 131u + unsigned(p) * 7u;
    if (!placeKnown) {   // first time: no fade
        place = p;
        placeKnown = true;
        colours[std::size_t(activeColour)].configure(profile(p).colour, seed);
        colours[std::size_t(activeColour)].setAmount(now.colour);
        colours[std::size_t(activeColour)].snapAmount();
        colourFade = 1;
        setSettings(user, envAmount, bpm);
        now = target;
        echoDelay = echoDelayTarget;
        return;
    }
    place = p;
    if (colourFade < 1.0f) colourFade = 1.0f;   // a change during a fade: the newest one wins
    activeColour = 1 - activeColour;
    auto& next = colours[std::size_t(activeColour)];
    next.configure(profile(p).colour, seed);
    next.setAmount(now.colour);
    next.snapAmount();
    colourFade = 0;
    setSettings(user, envAmount, bpm);
}

void EffectRack::setSettings(const EffectSettings& s, float environmentAmount, double hostBpm) {
    user = s;
    envAmount = std::clamp(environmentAmount, 0.0f, 1.0f);
    bpm = hostBpm > 1.0 ? hostBpm : 120.0;
    const float k = std::clamp(s.influence, 0.0f, 1.0f) * envAmount;   // how much the place counts
    const PlaceEffects e = placeEffects(place, instrument);
    target.colour = k;
    placeGrainSize = e.grainSize * k;
    target.drift = s.driftOn ? std::clamp(s.drift + k * e.drift, 0.0f, 1.0f) : 0.0f;
    target.tape = s.tapeOn ? std::clamp(s.tape + k * e.tape, 0.0f, 1.0f) : 0.0f;
    target.chorus = s.chorusOn ? std::clamp(s.chorus + k * e.chorus, 0.0f, 1.0f) : 0.0f;
    target.grain = s.grainOn ? std::clamp(s.grain + k * e.grain, 0.0f, 1.0f) : 0.0f;
    target.echo = s.echoOn ? std::clamp(s.echo + k * e.echo, 0.0f, 1.0f) : 0.0f;
    // The echo's time and feedback: the player's when they use the echo, else the place's.
    float ms = s.echoMs;
    if (s.echoSync > 0 && s.echoSync < 8) ms = float(60000.0 / bpm * syncBeats[s.echoSync]);
    const bool own = s.echo > 0.001f;
    target.echoMs = std::clamp(own ? ms : e.echoMs, 20.0f, 1500.0f);
    target.echoFeedback = std::clamp(own ? s.echoFeedback : e.echoFeedback, 0.0f, 0.95f);
    if (s.filterOn) {
        target.lowPass = std::clamp(s.filterLow * std::pow(2.0f, -k * e.lowOctaves), 60.0f, 20000.0f);
        target.highPass = std::clamp(s.filterHigh * std::pow(2.0f, k * e.highOctaves), 20.0f, 4000.0f);
    } else {
        target.lowPass = 20000.0f;
        target.highPass = 20.0f;
    }
    target.reverb = s.reverbOn ? std::max(0.0f, 2.0f * s.reverb * (1.0f + k * e.reverbBias)) : 0.0f;
    echoDelayTarget = std::clamp(float(target.echoMs * 0.001 * sr), 1.0f, float(echoLineL.size()) - 4.0f);
    for (auto& c : colours) c.setAmount(k);
}

float EffectRack::readLine(const std::vector<float>& line, int write, float delay) const {
    const int size = int(line.size());
    float pos = float(write) - delay;
    while (pos < 0.0f) pos += float(size);
    const int i0 = int(pos);
    const float f = pos - float(i0);
    const float y0 = line[std::size_t(i0 % size)], y1 = line[std::size_t((i0 + 1) % size)];
    return y0 + f * (y1 - y0);
}

void EffectRack::control() {
    const auto glide = [&](float& v, float t) { v += smoothCoef * (t - v); if (std::abs(t - v) < 1.0e-4f) v = t; };
    glide(now.drift, target.drift);
    glide(now.tape, target.tape);
    glide(now.chorus, target.chorus);
    glide(now.grain, target.grain);
    glide(now.echo, target.echo);
    glide(now.echoFeedback, target.echoFeedback);
    glide(now.reverb, target.reverb);
    glide(now.colour, target.colour);
    now.echoMs = target.echoMs;
    // Filter cutoffs glide in octaves.
    const auto octaveGlide = [&](float& v, float t) {
        const float lv = std::log2(std::max(1.0f, v)), lt = std::log2(t);
        float n = lv + smoothCoef * (lt - lv);
        if (std::abs(lt - n) < 1.0e-4f) n = lt;
        v = std::pow(2.0f, n);
    };
    if (now.lowPass <= 0.0f) now.lowPass = target.lowPass;
    if (now.highPass <= 0.0f) now.highPass = target.highPass;
    octaveGlide(now.lowPass, target.lowPass);
    octaveGlide(now.highPass, target.highPass);
    filterActive = user.filterOn && user.filter > 0.0f && (now.lowPass < 19500.0f || now.highPass > 21.0f);
    if (filterActive && (std::abs(now.lowPass - filterLowAt) > 0.003f * filterLowAt || std::abs(now.highPass - filterHighAt) > 0.003f * filterHighAt
                         || user.filterRes != filterResAt)) {
        const float q = 0.6f + 7.0f * user.filterRes * user.filterRes;
        lowL.set(std::min(now.lowPass, float(0.45 * sr)), q, sr);
        lowR.set(std::min(now.lowPass, float(0.45 * sr)), q, sr);
        highL.highPass(now.highPass, 0.707, sr);
        highR.b0 = highL.b0; highR.b1 = highL.b1; highR.b2 = highL.b2; highR.a1 = highL.a1; highR.a2 = highL.a2;
        filterLowAt = now.lowPass;
        filterHighAt = now.highPass;
        filterResAt = user.filterRes;
    }
    const float tick = float(controlPeriod / sr);
    const float driftHz = 0.05f * std::pow(40.0f, user.driftRate);
    driftNowL = driftWanderL.tick(rng, driftHz, tick);
    const float ownR = driftWanderR.tick(rng, driftHz * 1.13f, tick);
    // The player's own drift detunes the sides against each other; the place's moves both together.
    const float stereo = now.drift > 1.0e-4f ? std::clamp(user.drift / now.drift, 0.0f, 1.0f) : 0.0f;
    driftNowR = driftNowL + stereo * (ownR - driftNowL);
    chorusStereo = now.chorus > 1.0e-4f ? std::clamp(user.chorus / now.chorus, 0.0f, 1.0f) : 0.0f;
    echoStereo = now.echo > 1.0e-4f ? std::clamp(user.echo / now.echo, 0.0f, 1.0f) : 0.0f;
    const float toneHz = 18000.0f * std::pow(2.0f, -now.tape * (1.0f - 0.7f * user.tapeTone) * 3.5f);
    for (auto* f : { &tapeLp1L, &tapeLp1R, &tapeLp2L, &tapeLp2R }) f->setHz(toneHz, sr);
    const float echoHz = 1500.0f * std::pow(8.0f, user.echoOn && user.echo > 0.001f ? user.echoTone : 0.5f);
    echoToneL.setHz(echoHz, sr);
    echoToneR.setHz(echoHz, sr);
    echoLowL.setHz(120.0f, sr);
    echoLowR.setHz(120.0f, sr);
    // Tape dropouts: now and then the level dips, as worn oxide passes the head.
    if (--dropCountdown <= 0) {
        const float rate = 2.0f * user.tapeDropouts * float(now.tape > 0.01f);
        if (dropTarget < 1.0f) {
            dropTarget = 1.0f;
            dropCountdown = int(sr / controlPeriod * (0.5 + rng.uniform()));
        } else if (rate > 0.0f && rng.uniform() < rate * 0.5f) {
            dropTarget = 0.15f + 0.3f * rng.uniform();
            dropCountdown = int(sr / controlPeriod * (0.04 + 0.12 * rng.uniform()));
        } else {
            dropCountdown = int(sr / controlPeriod * 0.5);
        }
    }
}

void EffectRack::moduleTick(Module m, float& l, float& r) {
    const float fs = float(sr);
    switch (m) {
    case drift: {
        if (now.drift <= 1.0e-4f) {   // keep the line fed so turning it up is seamless
            driftLineL[std::size_t(driftWrite)] = l;
            driftLineR[std::size_t(driftWrite)] = r;
            if (++driftWrite >= int(driftLineL.size())) driftWrite = 0;
            return;
        }
        const float hz = 0.05f * std::pow(40.0f, user.driftRate);
        const float depth = swingSamples(40.0f * now.drift, hz, sr);
        driftLineL[std::size_t(driftWrite)] = l;
        driftLineR[std::size_t(driftWrite)] = r;
        l = readLine(driftLineL, driftWrite, depth * (1.0f + driftNowL) + 1.0f);
        r = readLine(driftLineR, driftWrite, depth * (1.0f + driftNowR) + 1.0f);
        if (++driftWrite >= int(driftLineL.size())) driftWrite = 0;
        return;
    }
    case tape: {
        const float a = now.tape;
        follower = std::max(std::abs(l) + std::abs(r), follower + followerRelease * (0.0f - follower));
        if (a <= 1.0e-4f) {
            tapeLineL[std::size_t(tapeWrite)] = l;
            tapeLineR[std::size_t(tapeWrite)] = r;
            if (++tapeWrite >= int(tapeLineL.size())) tapeWrite = 0;
            return;
        }
        wowPhase += 0.55f / fs;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += 6.8f / fs;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        const float wow = swingSamples(9.0f * a, 0.55f, sr), flutter = swingSamples(3.0f * a, 6.8f, sr);
        tapeLineL[std::size_t(tapeWrite)] = l;
        tapeLineR[std::size_t(tapeWrite)] = r;
        const float d = 1.0f + wow * (1.0f + sinCycle(wowPhase)) + flutter * (1.0f + sinCycle(flutterPhase));
        l = readLine(tapeLineL, tapeWrite, d);
        r = readLine(tapeLineR, tapeWrite, d * 1.0005f);
        if (++tapeWrite >= int(tapeLineL.size())) tapeWrite = 0;
        l = tapeLp2L.lp(tapeLp1L.lp(l));
        r = tapeLp2R.lp(tapeLp1R.lp(r));
        const float drive = 1.0f + 4.0f * a;
        l = softClip(l * drive) / drive;
        r = softClip(r * drive) / drive;
        // Hiss while something is playing (it fades with the music, so silence stays silent).
        const float gate = std::min(1.0f, follower * 40.0f);
        const float h = user.tapeHiss * a * 0.003f * gate;
        if (h > 0.0f) {
            const float wl = noise.bipolar(), wr = noise.bipolar();
            l += h * (wl - hissTone.lp(wl));
            r += h * (wr - hissToneR.lp(wr));
        }
        dropGain += 0.004f * (dropTarget - dropGain);
        l *= dropGain;
        r *= dropGain;
        return;
    }
    case filter: {
        if (!filterActive) return;
        lowL.tick(l);
        lowR.tick(r);
        const float fl = highL.tick(lowL.low), fr = highR.tick(lowR.low);
        l += user.filter * (fl - l);
        r += user.filter * (fr - r);
        return;
    }
    case chorus: {
        chorusLineL[std::size_t(chorusWrite)] = l;
        chorusLineR[std::size_t(chorusWrite)] = r;
        if (now.chorus > 1.0e-4f) {
            chorusPhase += 0.1f * std::pow(30.0f, user.chorusRate) / fs;
            if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
            float other = chorusPhase + 0.5f;
            if (other >= 1.0f) other -= 1.0f;
            float third = chorusPhase + 0.25f;
            if (third >= 1.0f) third -= 1.0f;
            const float base = 0.009f * fs, depth = 0.0025f * fs * now.chorus;
            const float cl = readLine(chorusLineL, chorusWrite, base + depth * (1.0f + sinCycle(chorusPhase)));
            const float cr = readLine(chorusLineR, chorusWrite, base * 1.3f + depth * (1.0f + sinCycle(other)));
            const float cx = readLine(chorusLineL, chorusWrite, base * 1.7f + depth * (1.0f + sinCycle(third)));
            const float m = 0.7f * now.chorus;
            // Stereo voices for the player's chorus; the same voice on both sides for the place's.
            const float vl = cl - 0.3f * chorusStereo * cx, vr = cr + 0.3f * chorusStereo * cx;
            const float mono = 0.5f * (vl + vr);
            l = l * (1.0f - 0.3f * m) + m * (mono + chorusStereo * (vl - mono));
            r = r * (1.0f - 0.3f * m) + m * (mono + chorusStereo * (vr - mono));
        }
        if (++chorusWrite >= int(chorusLineL.size())) chorusWrite = 0;
        return;
    }
    case grain: {
        const int size = int(grainLine.size());
        if (!(user.grainFreeze && now.grain > 0.001f)) {   // frozen: the buffer stops listening
            grainLine[std::size_t(grainWrite)] = 0.5f * (l + r);
            if (++grainWrite >= size) grainWrite = 0;
        }
        if (now.grain <= 1.0e-4f) return;
        const float length = fs * 0.03f * std::pow(10.0f, user.grainSize * 0.7f + 0.3f * placeGrainSize);
        if (--grainCountdown <= 0) {   // a new grain somewhere in the last second or two
            for (auto& g : grains) {
                if (g.active) continue;
                const float spread = user.grainSpread;
                g.active = true;
                g.age = 0;
                g.length = std::max(64, int(length * (0.7f + 0.6f * rng.uniform())));
                float semis = 30.0f * spread * rng.bipolar() / 100.0f;   // a scatter of cents
                const float chance = rng.uniform();
                if (spread > 0.5f && chance < (spread - 0.5f)) semis += chance < 0.5f * (spread - 0.5f) ? 12.0f : -12.0f;   // fragments an octave away
                g.step = std::pow(2.0f, semis / 12.0f);
                const float back = float(g.length) * std::max(1.0f, g.step) + fs * (0.02f + 1.2f * spread * rng.uniform());
                g.position = float(grainWrite) - std::min(back, float(size) - 8.0f);
                while (g.position < 0.0f) g.position += float(size);
                const float pan = rng.bipolar() * (0.3f + 0.6f * spread);
                g.panL = std::sqrt(0.5f * (1.0f - pan));
                g.panR = std::sqrt(0.5f * (1.0f + pan));
                break;
            }
            grainCountdown = std::max(32, int(length / (2.0f + 2.0f * user.grainSpread) * (0.6f + 0.8f * rng.uniform())));
        }
        float gl = 0, gr = 0;
        for (auto& g : grains) {
            if (!g.active) continue;
            const float u = float(g.age) / float(g.length);
            const float w = sinCycle(0.5f * u);
            const int i0 = int(g.position) % size;
            const float f = g.position - std::floor(g.position);
            const float x = grainLine[std::size_t(i0)] + f * (grainLine[std::size_t((i0 + 1) % size)] - grainLine[std::size_t(i0)]);
            gl += x * w * w * g.panL;
            gr += x * w * w * g.panR;
            g.position += g.step;
            if (g.position >= float(size)) g.position -= float(size);
            if (++g.age >= g.length) g.active = false;
        }
        const float a = now.grain;
        l = l * (1.0f - 0.6f * a) + 1.1f * a * gl;
        r = r * (1.0f - 0.6f * a) + 1.1f * a * gr;
        return;
    }
    case echo: {
        echoDelay += echoDelayCoef * (echoDelayTarget - echoDelay);
        const float yl = readLine(echoLineL, echoWrite, echoDelay);
        const float yr = readLine(echoLineR, echoWrite, echoDelay);
        const float fl = echoLowL.hp(echoToneL.lp(yl)), fr = echoLowR.hp(echoToneR.lp(yr));
        const float fb = now.echoFeedback;
        const float in = now.echo;
        // The player's echo ping-pongs (enters on the left, crosses sides on every repeat, so
        // repeats stay on the beat); the place's stays where it was played.
        const float toL = fl + echoStereo * (fr - fl), toR = fr + echoStereo * (fl - fr);
        const float mono = 0.5f * (l + r);
        echoLineL[std::size_t(echoWrite)] = softClip((l + echoStereo * (mono - l)) * in + fb * toL);
        echoLineR[std::size_t(echoWrite)] = softClip(r * (1.0f - echoStereo) * in + fb * toR);
        if (++echoWrite >= int(echoLineL.size())) echoWrite = 0;
        l += fl;
        r += fr;
        return;
    }
    }
}

void EffectRack::process(float* left, float* right, float* roomL, float* roomR, float* echoL, float* echoR, float sendFactor, int n) {
    n = std::min(n, int(fadeL.size()));
    if (!placeKnown) setPlace(place);
    std::copy(left, left + n, dryL.begin());
    std::copy(right, right + n, dryR.begin());
    // 1. The instrument's effects, in the chosen order.
    static constexpr Module orders[3][6] { { drift, tape, filter, chorus, grain, echo },
                                           { grain, drift, tape, filter, chorus, echo },
                                           { drift, tape, filter, chorus, echo, grain } };
    const auto& order = orders[std::clamp(user.order, 0, 2)];
    for (int i = 0; i < n; ++i) {
        if (clock == 0) {
            control();
            clock = controlPeriod;
        }
        --clock;
        float l = left[i], r = right[i];
        for (const Module m : order) {
            if (m == drift && !user.driftOn) continue;
            if (m == tape && !user.tapeOn) continue;
            if (m == filter && !user.filterOn) continue;
            if (m == chorus && !user.chorusOn) continue;
            if (m == grain && !user.grainOn) continue;
            if (m == echo && !user.echoOn) continue;
            moduleTick(m, l, r);
        }
        left[i] = l;
        right[i] = r;
    }
    // 2. The place shapes all of it last (cross-faded from the old place after a change).
    if (colourFade < 1.0f) {
        std::copy(left, left + n, fadeL.begin());
        std::copy(right, right + n, fadeR.begin());
        colours[std::size_t(1 - activeColour)].process(fadeL.data(), fadeR.data(), n);
    }
    colours[std::size_t(activeColour)].process(left, right, n);
    if (colourFade < 1.0f)
        for (int i = 0; i < n; ++i) {
            colourFade = std::min(1.0f, colourFade + colourFadeStep);
            const float in = sinCycle(0.25f * colourFade), out = sinCycle(0.25f * (1.0f - colourFade));
            left[i] = left[i] * in + fadeL[std::size_t(i)] * out;
            right[i] = right[i] * in + fadeR[std::size_t(i)] * out;
        }
    // 3. Mix with the dry instrument; the sends.
    const float mix = std::clamp(user.mix, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i) {
        const float l = dryL[std::size_t(i)] + mix * (left[i] - dryL[std::size_t(i)]);
        const float r = dryR[std::size_t(i)] + mix * (right[i] - dryR[std::size_t(i)]);
        left[i] = l;
        right[i] = r;
        const float send = now.reverb * sendFactor;
        roomL[i] += l * send;
        roomR[i] += r * send;
        echoL[i] += l * now.colour * mix;   // the place's echo is an effect too: Mix 0 is dry
        echoR[i] += r * now.colour * mix;
    }
}

} // namespace liminal::dsp
