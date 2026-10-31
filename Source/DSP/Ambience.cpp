#include "DSP/Ambience.h"
#include <cmath>

namespace liminal::dsp {

float Ambience::Pink::next(float w) {
    // Paul Kellet's refined pink-noise filter.
    b0 = 0.99886f * b0 + w * 0.0555179f;
    b1 = 0.99332f * b1 + w * 0.0750759f;
    b2 = 0.96900f * b2 + w * 0.1538520f;
    b3 = 0.86650f * b3 + w * 0.3104856f;
    b4 = 0.55000f * b4 + w * 0.5329522f;
    b5 = -0.7616f * b5 - w * 0.0168980f;
    const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
    b6 = w * 0.115926f;
    return out * 0.11f;
}

void Ambience::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    configure(place, seed);
}

void Ambience::configure(Place p, unsigned s, const Character* ch) {
    place = p;
    seed = s;
    // Defaults: everything off, then each place switches on its own bed and events.
    airLevel = humLevel = buzzLevel = gritLevel = clockLevel = trickleLevel = whineLevel = 0;
    humWeights = {};
    beatDepth = 0.15f;
    for (auto& d : drones) d = Drone {};
    humDrifts = dropouts = false;
    float airHz = 400.0f;
    bedSend = 0.3f;
    minGapSeconds = 20.0f;
    meanGapSeconds = 45.0f;
    scheduleCount = 0;
    const auto add = [&](Kind k, float weight, float level) {
        if (scheduleCount < int(schedules.size())) schedules[std::size_t(scheduleCount++)] = { k, weight, level };
    };
    const auto drone = [&](int i, float hz, float level, std::array<float, 4> w, float amHz = 0, float amDepth = 0) {
        auto& d = drones[std::size_t(i)];
        d.hz = hz;
        d.level = level;
        d.weights = w;
        d.amHz = amHz;
        d.amDepth = amDepth;
        return &d;
    };
    switch (p) {
    case Place::hospital:   // mains hum and ballasts; HVAC far below; a machine's tone somewhere, sometimes
        airLevel = 0.008f; airHz = 1800.0f;
        humLevel = 0.0042f; humWeights = { 1.0f, 0.55f, 0.32f, 0.2f, 0.12f, 0.07f };
        buzzLevel = 0.0034f; gritLevel = 0.0011f;
        if (auto* d = drone(0, 1420.0f, 0.00045f, { 1, 0, 0, 0 })) { d->gate = Drone::Gate::sometimes; d->onSeconds = 70; d->offSeconds = 50; d->fadeSeconds = 4; }
        bedSend = 0.35f;
        minGapSeconds = 20.0f; meanGapSeconds = 50.0f;
        add(Kind::monitor, 1.0f, 1.0f);
        add(Kind::ballastTick, 1.0f, 1.0f);
        add(Kind::doorClick, 0.8f, 1.0f);
        add(Kind::flicker, 0.7f, 1.0f);
        break;
    case Place::mall:       // big-building HVAC drone, the escalator's motor, a fountain trickling far off
        airLevel = 0.012f; airHz = 260.0f;
        humLevel = 0.0012f; humWeights = { 1.0f, 0.4f, 0.2f };
        drone(0, 38.0f, 0.0045f, { 1.0f, 0.5f, 0.2f, 0.08f }, 0.11f, 0.12f);          // the air handlers
        drone(1, 74.0f, 0.0020f, { 1.0f, 0.45f, 0.3f, 0.15f }, 1.65f, 0.18f);         // escalator: steady, with its steps' periodicity
        trickleLevel = 0.0016f;
        bedSend = 0.7f;
        minGapSeconds = 25.0f; meanGapSeconds = 60.0f;
        add(Kind::paChime, 1.0f, 1.0f);
        add(Kind::shutter, 0.8f, 1.0f);
        add(Kind::flicker, 0.3f, 1.0f);   // a gallery downlight failing (Part 3: the hum sags with it)
        break;
    case Place::school:     // tubes humming, a wall clock ticking once a second, a radiator
        airLevel = 0.005f; airHz = 700.0f;
        humLevel = 0.0034f; humWeights = { 1.0f, 0.7f, 0.45f, 0.25f, 0.15f, 0.1f };
        buzzLevel = 0.0026f; gritLevel = 0.0008f;
        drone(0, 96.0f, 0.0014f, { 1.0f, 0.3f, 0.12f, 0.0f }, 0.07f, 0.1f);           // radiator hum
        clockLevel = 0.012f;
        bedSend = 0.25f;
        minGapSeconds = 20.0f; meanGapSeconds = 50.0f;
        add(Kind::radiatorPing, 1.0f, 1.0f);
        add(Kind::locker, 0.8f, 1.0f);
        add(Kind::bell, 0.25f, 1.0f);
        add(Kind::flicker, 0.4f, 1.0f);
        break;
    case Place::garage:     // a ventilation fan's low rumble, lamp buzz, electrical hum
        airLevel = 0.010f; airHz = 160.0f;
        humLevel = 0.0016f; humWeights = { 1.0f, 0.3f, 0.5f, 0.1f };
        buzzLevel = 0.0020f; gritLevel = 0.0010f;
        drone(0, 43.0f, 0.0060f, { 1.0f, 0.55f, 0.25f, 0.1f }, 0.9f, 0.08f);          // the fan (blade rate wobble)
        bedSend = 0.45f;
        minGapSeconds = 25.0f; meanGapSeconds = 65.0f;
        add(Kind::carDoor, 1.0f, 1.0f);
        add(Kind::barrier, 0.6f, 1.0f);
        add(Kind::drip, 1.0f, 1.0f);
        add(Kind::flicker, 0.4f, 1.0f);
        break;
    case Place::pool:       // almost silent: a filtration pump far off, distant ventilation. The water is still.
        airLevel = 0.005f; airHz = 220.0f;
        drone(0, 58.0f, 0.0018f, { 1.0f, 0.35f, 0.15f, 0.05f }, 0.05f, 0.05f);        // pump
        bedSend = 0.55f;
        minGapSeconds = 30.0f; meanGapSeconds = 55.0f;   // a drip every 30-90 s
        add(Kind::drip, 1.0f, 1.0f);
        add(Kind::ripple, 0.12f, 1.0f);
        break;
    case Place::hotel:      // very quiet room tone; an ice machine or HVAC far down the hall; carpet
        airLevel = 0.005f; airHz = 300.0f;
        humLevel = 0.0008f; humWeights = { 0.3f, 1.0f, 0.4f };
        drone(0, 118.0f, 0.0011f, { 1.0f, 0.5f, 0.2f, 0.1f }, 0.03f, 0.1f);          // the ice machine
        bedSend = 0.2f;
        minGapSeconds = 25.0f; meanGapSeconds = 65.0f;
        add(Kind::liftChime, 1.0f, 1.0f);
        add(Kind::doorClose, 1.0f, 1.0f);
        add(Kind::flicker, 0.3f, 1.0f);   // a sconce's bulb failing: the hum sags with it
        break;
    case Place::subway:     // tunnel airflow, the substation's transformer, rails humming far off
        airLevel = 0.014f; airHz = 180.0f;
        humLevel = 0.0034f; humWeights = { 0.4f, 1.0f, 0.3f, 0.45f, 0.1f, 0.2f };      // transformer: 120 Hz family
        buzzLevel = 0.0014f; gritLevel = 0.0006f;
        drone(0, 212.0f, 0.0005f, { 1.0f, 0.4f, 0.0f, 0.0f }, 0.04f, 0.3f);          // the rails, singing faintly
        bedSend = 0.5f;
        minGapSeconds = 25.0f; meanGapSeconds = 60.0f;
        add(Kind::trainRumble, 0.6f, 1.0f);
        add(Kind::railTick, 1.0f, 1.0f);
        add(Kind::flicker, 0.4f, 1.0f);
        break;
    case Place::supermarket:   // refrigeration that cycles itself, the tubes, the cold cases
        airLevel = 0.006f; airHz = 900.0f;
        humLevel = 0.0030f; humWeights = { 1.0f, 0.6f, 0.3f, 0.15f, 0.1f, 0.05f };
        buzzLevel = 0.0030f; gritLevel = 0.0010f;
        if (auto* d = drone(0, 49.0f, 0.0050f, { 1.0f, 0.8f, 0.45f, 0.25f }, 0.3f, 0.06f)) {   // the compressor
            d->gate = Drone::Gate::cycles; d->onSeconds = 150; d->offSeconds = 90; d->fadeSeconds = 2.5f;
        }
        bedSend = 0.35f;
        minGapSeconds = 20.0f; meanGapSeconds = 55.0f;
        add(Kind::freezerSeal, 1.0f, 1.0f);
        add(Kind::cart, 0.7f, 1.0f);
        add(Kind::flicker, 0.5f, 1.0f);
        break;
    case Place::office:     // the fluorescent hum-buzz is the room: slightly too loud. No white noise.
        airLevel = 0.004f; airHz = 350.0f;
        humLevel = 0.0044f; humWeights = { 0.6f, 1.0f, 0.35f, 0.5f, 0.15f, 0.25f };
        buzzLevel = 0.0078f; gritLevel = 0.0022f;
        whineHz = 11800.0f; whineLevel = 0.0003f;
        bedSend = 0.4f;
        minGapSeconds = 35.0f; meanGapSeconds = 80.0f;   // almost nothing happens
        add(Kind::flicker, 1.0f, 1.0f);
        add(Kind::ballastTick, 0.3f, 0.8f);
        break;
    case Place::impossible:   // too quiet; a hum whose pitch drifts as no hum can; total silences
        airLevel = 0.0025f; airHz = 300.0f;
        humLevel = 0.0018f; humWeights = { 1.0f, 0.3f, 0.5f, 0.0f, 0.2f };
        humDrifts = dropouts = true;
        bedSend = 0.5f;
        minGapSeconds = 30.0f; meanGapSeconds = 75.0f;
        add(Kind::ghostNote, 1.0f, 1.0f);
        add(Kind::wrongWall, 0.7f, 1.0f);
        add(Kind::flicker, 0.4f, 1.0f);   // the light from nowhere falters, and the hum with it
        break;
    }
    characterRate = 1;
    if (ch != nullptr) {   // the Character: which events and how often (never below the minimum gap), and the bed
        int kept = 0;
        for (int i = 0; i < scheduleCount; ++i) {
            auto sc = schedules[std::size_t(i)];
            sc.weight *= ch->eventWeight[std::size_t(std::min(i, 4))];
            if (sc.kind == Kind::flicker) sc.weight *= 0.5f + 3.0f * ch->flicker;   // a flickering Character's tube fails more
            if (sc.weight <= 0.0f) continue;
            schedules[std::size_t(kept++)] = sc;
        }
        scheduleCount = kept;
        if (ch->extraKind >= 0) add(Kind(ch->extraKind), 0.6f, 0.9f);
        characterRate = std::clamp(std::sqrt(ch->eventRate), 0.6f, 1.6f);
        airLevel *= ch->bed;
        for (auto& d : drones) d.level *= ch->bed;
        trickleLevel *= ch->bed;
        clockLevel *= ch->bed;
        airHz = std::clamp(airHz * ch->bedTone, 80.0f, 4000.0f);
        humLevel *= ch->hum;
        buzzLevel *= ch->hum;
        gritLevel *= ch->hum;
        whineLevel *= ch->hum;
    }
    weightSum = 0;
    for (int i = 0; i < scheduleCount; ++i) weightSum += schedules[std::size_t(i)].weight;
    mainsStep = 60.0 / sr;   // North American mains
    constexpr float beats[humPartials] { 0.031f, 0.047f, 0.023f, 0.071f, 0.053f, 0.089f };   // slow beating per harmonic, Hz
    for (int k = 0; k < humPartials; ++k) beatHz[std::size_t(k)] = beats[k];
    airL1.setHz(airHz, sr);
    airL2.setHz(airHz, sr);
    airR1.setHz(airHz * 1.07f, sr);
    airR2.setHz(airHz * 1.07f, sr);
    gritL.set(4200.0f, 1.4f, sr);
    gritR.set(4700.0f, 1.4f, sr);
    clockTone.set(2300.0f, 6.0f, sr);
    trickleA.set(1700.0f, 9.0f, sr);
    trickleB.set(2900.0f, 9.0f, sr);
    lampCoef = float(1.0 - std::exp(-1.0 / (0.003 * sr)));
    silenceFall = float(1.0 - std::exp(-1.0 / (0.01 * sr)));
    silenceRise = float(1.0 - std::exp(-1.0 / (0.15 * sr)));
    for (auto& d : drones) d.gCoef = float(1.0 - std::exp(-1.0 / (std::max(0.05f, d.fadeSeconds) * 0.4f * float(sr))));
    reset();
}

void Ambience::reset() {
    rng = {};
    rng.state = 0x9E3779B9u ^ (seed * 0x85EBCA6Bu) ^ (unsigned(place) * 0xC2B2AE35u);
    noiseL.state = rng.state ^ 0x1234567u;
    noiseR.state = rng.state ^ 0x7654321u;
    pinkL = {};
    pinkR = {};
    airL1.z = airL2.z = airR1.z = airR2.z = 0;
    for (auto* f : { &gritL, &gritR, &clockTone, &trickleA, &trickleB }) f->clear();
    mainsPhase = 0;
    for (int k = 0; k < humPartials; ++k) beatPhase[std::size_t(k)] = random();
    buzzWander.start(rng);
    humWander.start(rng);
    buzzFlutter = humDrift = restrike = 0;
    lampNow = lampTarget = 1;
    stutterFlicks = stutterCountdown = 0;
    flickerCount = 0;
    for (auto& d : drones) {
        d.phase = random();
        d.amPhase = random();
        d.g = d.gTarget = 1;
        d.countdown = 0;
        if (d.gate != Drone::Gate::always)   // starts somewhere in its cycle
            d.countdown = int(sr * double(d.onSeconds) * (0.4 + 0.6 * double(random())));
    }
    clockEnv = trickleEnv = whinePhase = 0;
    clockCountdown = int(sr * 0.5);
    clockBeat = 0;
    silenceGain = silenceTarget = 1;
    silenceCountdown = int(sr * (25.0 + 30.0 * double(random())));   // never in the first 25 s
    controlClock = 0;
    for (auto& e : events) e.active = false;
    sinceLast = 1e9;
    // The first event comes somewhere in a normal gap (at least ten seconds in), not at once.
    countdown = sr * (0.5 * double(minGapSeconds) + double(random()) * double(meanGapSeconds));
}

void Ambience::scheduleNext() {
    // A minimum gap, then an exponential wait (a Poisson process): no rhythm, never busy.
    const double wait = double(minGapSeconds) - double(meanGapSeconds - minGapSeconds) / double(characterRate)
                                                  * std::log(std::max(1.0e-4, double(random())));
    countdown = sr * std::min(wait, 6.0 * double(meanGapSeconds));
    sinceLast = 0;
}

void Ambience::disturb() {
    // Evolution's disturbance brings the next event forward, never closer than 3/4 of the gap.
    if (scheduleCount > 0 && sinceLast > 0.75 * double(minGapSeconds) * sr) countdown = 0;
}

void Ambience::startStutter(float depth) {
    stutterFlicks = 2 + int(random() * 5.0f);
    stutterDepth = std::clamp(depth, 0.3f, 1.0f);
    stutterCountdown = 0;
    ++flickerCount;
}

Ambience::Event* Ambience::spawn(Kind kind, float level, int delay, float pitch) {
    if (kind == Kind::flicker) {   // the tube fails for a moment: no sound of its own, the buzz stutters
        startStutter(0.6f + 0.4f * random() * level);
        return nullptr;
    }
    int busy = 0;
    for (auto& e : events) busy += e.active ? 1 : 0;
    if (busy >= eventCap) return nullptr;
    Event* slot = nullptr;
    for (auto& e : events)
        if (!e.active) { slot = &e; break; }
    if (slot == nullptr) return nullptr;
    auto& e = *slot;
    ++spawnCount;
    e = Event {};
    e.active = true;
    e.kind = kind;
    e.pan = bipolar() * 0.7f;
    e.noise.state = rng.state ^ 0xDEADBEEFu;
    e.noise.uniform();
    const float fs = float(sr);
    const float limit = 0.45f * fs;
    const auto pg = panGains(e.pan);
    // Most events are far away: little direct sound, mostly the room.
    e.dry = 0.35f;
    e.send = 0.8f;
    const auto mode = [&](int k, float hz, float amp, float seconds) {
        if (hz < limit) e.modes.set(k, amp, 0.0f, twoPi * hz / fs, t60(seconds, sr), 1.0f, pg.l, pg.r);
    };
    const auto click = [&](float amp, float seconds, float lowPassHz) {   // the contact noise of a strike
        e.env = amp;
        e.decay = t60(seconds, sr);
        e.lp.setHz(lowPassHz, sr);
    };
    switch (kind) {
    case Kind::monitor: {   // a patient monitor two corridors away: three soft beeps
        const float f = 976.0f * (1.0f + 0.003f * bipolar());
        e.amp = 0.010f * level;
        e.f = f;
        e.rate = fs * 0.16f;                // tone length
        e.gap = int(fs * 0.28f);            // beep to beep
        e.repeats = 3;
        e.length = e.repeats * (int(e.rate) + e.gap);
        e.lp.setHz(2400.0f, sr);
        e.dry = 0.25f; e.send = 0.85f;
        break;
    }
    case Kind::ballastTick: {   // a ballast or relay ticking once, above the ceiling
        e.amp = 0.012f * level;
        mode(0, 3100.0f + 900.0f * random(), e.amp, 0.04f);
        mode(1, 5200.0f + 800.0f * random(), e.amp * 0.5f, 0.03f);
        e.modes.retune(1.0f, false);
        click(e.amp * 1.5f, 0.004f, 6000.0f);
        e.length = int(fs * 0.12f);
        e.dry = 0.5f; e.send = 0.5f;
        break;
    }
    case Kind::doorClick: {   // a door's latch somewhere far down the corridor: two small clicks
        e.amp = 0.014f * level;
        mode(0, 820.0f + 300.0f * random(), e.amp, 0.06f);
        mode(1, 2100.0f + 500.0f * random(), e.amp * 0.6f, 0.04f);
        mode(2, 140.0f + 40.0f * random(), e.amp * 0.8f, 0.12f);
        e.modes.retune(1.0f, false);
        click(e.amp, 0.005f, 3000.0f);
        e.length = int(fs * 0.3f);
        e.dry = 0.2f; e.send = 0.9f;
        break;
    }
    case Kind::paChime: {   // the PA in another wing: a two- or three-note chime, band-limited
        constexpr float notes[] { 783.99f, 659.26f, 523.25f };
        const int count = random() < 0.5f ? 2 : 3;
        e.amp = 0.012f * level;
        const float f = pitch > 0.0f ? pitch : notes[0];
        mode(0, f, e.amp, 1.6f);
        mode(1, f * 2.0f, e.amp * 0.25f, 0.9f);
        mode(2, f * 3.01f, e.amp * 0.08f, 0.5f);
        e.modes.retune(1.0f, false);
        click(0.0f, 0.01f, 4000.0f);
        e.lp.setHz(3800.0f, sr);
        e.lp2.setHz(320.0f, sr);   // (high-pass by subtraction: the PA's small horns)
        e.length = int(fs * 2.2f);
        e.dry = 0.15f; e.send = 0.95f;
        e.pan = -0.6f;
        if (pitch <= 0.0f)
            for (int k = 1; k < count; ++k)
                if (auto* next = spawn(Kind::paChime, level, delay + int(fs * 0.55f * float(k)), notes[k])) next->pan = e.pan;
        break;
    }
    case Kind::shutter: {   // a shop's shutter rattling far away: a short run of soft metal ticks
        e.amp = 0.010f * level;
        e.repeats = 12 + int(random() * 18.0f);
        e.f = 1600.0f + 800.0f * random();
        e.bp1.set(e.f, 5.0f, sr);
        e.rate = 0.035f + 0.02f * random();   // seconds between ticks
        e.length = int(fs * (float(e.repeats) * e.rate + 0.3f));
        e.decay = t60(0.015, sr);
        e.dry = 0.15f; e.send = 0.95f;
        break;
    }
    case Kind::radiatorPing: {   // a radiator pinging as it warms or cools
        e.amp = 0.010f * level;
        const float f = 1300.0f + 1100.0f * random();
        mode(0, f, e.amp, 0.5f);
        mode(1, f * 2.76f, e.amp * 0.3f, 0.25f);
        e.modes.retune(1.0f, false);
        click(e.amp, 0.003f, 5000.0f);
        e.length = int(fs * 0.7f);
        e.dry = 0.45f; e.send = 0.6f;
        break;
    }
    case Kind::locker: {   // a locker settling: a low sheet-metal thunk
        e.amp = 0.016f * level;
        const float f = 160.0f + 60.0f * random();
        constexpr float ratios[] { 1.0f, 1.83f, 2.71f, 4.1f };
        for (int k = 0; k < 4; ++k) mode(k, f * ratios[k], e.amp * std::pow(0.6f, float(k)), 0.35f - 0.06f * float(k));
        e.modes.retune(1.0f, false);
        click(e.amp, 0.01f, 1500.0f);
        e.length = int(fs * 0.5f);
        e.dry = 0.3f; e.send = 0.8f;
        break;
    }
    case Kind::bell: {   // the school bell, far away and muffled, a single stroke
        e.amp = 0.012f * level;
        const float f = 620.0f + 40.0f * random();
        constexpr float ratios[] { 1.0f, 2.0f, 2.42f, 3.0f };
        for (int k = 0; k < 4; ++k) mode(k, f * ratios[k], e.amp * std::pow(0.45f, float(k)), 2.5f - 0.5f * float(k));
        e.modes.retune(1.0f, false);
        click(0.0f, 0.01f, 900.0f);
        e.lp.setHz(900.0f, sr);
        e.lp2.setHz(900.0f, sr);
        e.length = int(fs * 3.0f);
        e.dry = 0.12f; e.send = 0.95f;
        break;
    }
    case Kind::carDoor: {   // a car door on another level: a dull distant thunk
        e.amp = 0.020f * level;
        const float f = 62.0f + 20.0f * random();
        constexpr float ratios[] { 1.0f, 1.59f, 2.14f, 2.9f };
        for (int k = 0; k < 4; ++k) mode(k, f * ratios[k], e.amp * std::pow(0.6f, float(k)), 0.3f - 0.05f * float(k));
        e.modes.retune(1.0f, false);
        click(e.amp, 0.02f, 300.0f);
        e.length = int(fs * 0.5f);
        e.dry = 0.12f; e.send = 0.95f;
        break;
    }
    case Kind::barrier: {   // the exit barrier coming down, very far away: one clank
        e.amp = 0.012f * level;
        const float f = 190.0f + 80.0f * random();
        constexpr float ratios[] { 1.0f, 2.756f, 5.404f };
        for (int k = 0; k < 3; ++k) mode(k, f * ratios[k], e.amp * std::pow(0.5f, float(k)), 0.9f / (1.0f + float(k)));
        e.modes.retune(1.0f, false);
        click(e.amp, 0.006f, 1200.0f);
        e.length = int(fs * 1.0f);
        e.dry = 0.12f; e.send = 0.95f;
        break;
    }
    case Kind::drip: {
        if (place == Place::pool) {   // a single drop into still water: a soft plink, then the tiles answer
            e.f = 900.0f + 900.0f * random();
            e.rate = 6.0f + 10.0f * random();   // the bubble's pitch rises as it shrinks
            e.decay = t60(0.06 + 0.04 * random(), sr);
            e.amp = 0.020f * level * (0.7f + 0.3f * random());
            e.env = e.amp;
            e.length = int(fs * 0.25f);
            e.dry = 0.3f; e.send = 1.0f;
        } else {                      // on concrete: a small wet tick
            e.amp = 0.010f * level;
            mode(0, 2400.0f + 900.0f * random(), e.amp, 0.05f);
            e.modes.retune(1.0f, false);
            click(e.amp * 0.8f, 0.003f, 5000.0f);
            e.length = int(fs * 0.12f);
            e.dry = 0.3f; e.send = 0.8f;
        }
        break;
    }
    case Kind::ripple: {   // very rarely, the water laps once against the edge: soft and low
        e.amp = 0.004f * level;
        e.bp1.set(320.0f + 80.0f * random(), 2.5f, sr);
        e.length = int(fs * (0.5f + 0.3f * random()));
        e.dry = 0.4f; e.send = 0.6f;
        break;
    }
    case Kind::liftChime: {   // the lift arriving two floors down: one soft "ding"
        e.amp = 0.010f * level;
        const float f = pitch > 0.0f ? pitch : 1046.5f;
        mode(0, f, e.amp, 1.4f);
        mode(1, f * 2.0f, e.amp * 0.2f, 0.7f);
        mode(2, f * 2.76f, e.amp * 0.06f, 0.4f);
        e.modes.retune(1.0f, false);
        click(0.0f, 0.01f, 1300.0f);
        e.lp.setHz(1300.0f, sr);   // through the floors and the carpet
        e.lp2.setHz(1300.0f, sr);
        e.length = int(fs * 2.0f);
        e.dry = 0.3f; e.send = 0.7f;
        if (pitch <= 0.0f && random() < 0.4f)   // sometimes the lower note too (going down)
            if (auto* next = spawn(Kind::liftChime, level, delay + int(fs * 0.5f), 830.6f)) next->pan = e.pan;
        break;
    }
    case Kind::doorClose: {   // a door closing far down the hall
        e.amp = 0.020f * level;
        const float f = 70.0f + 25.0f * random();
        constexpr float ratios[] { 1.0f, 1.62f, 2.3f, 3.1f };
        for (int k = 0; k < 4; ++k) mode(k, f * ratios[k], e.amp * std::pow(0.55f, float(k)), 0.25f - 0.04f * float(k));
        e.modes.retune(1.0f, false);
        click(e.amp, 0.015f, 260.0f);
        e.length = int(fs * 0.45f);
        e.dry = 0.4f; e.send = 0.5f;
        break;
    }
    case Kind::trainRumble: {   // a train somewhere in the tunnels that never arrives: low and slow, then gone
        e.amp = 0.0045f * level;
        e.f = 34.0f + 10.0f * random();
        e.length = int(fs * (9.0f + 6.0f * random()));
        e.lp.setHz(110.0f, sr);
        e.pan = random() < 0.5f ? -0.8f : 0.8f;
        e.dry = 0.5f; e.send = 0.6f;
        break;
    }
    case Kind::railTick: {   // a rail ticking as it cools
        e.amp = 0.009f * level;
        const float f = 1700.0f + 900.0f * random();
        mode(0, f, e.amp, 0.25f);
        mode(1, f * 2.32f, e.amp * 0.4f, 0.15f);
        e.modes.retune(1.0f, false);
        click(e.amp, 0.003f, 6000.0f);
        e.length = int(fs * 0.4f);
        e.dry = 0.25f; e.send = 0.85f;
        break;
    }
    case Kind::freezerSeal: {   // a freezer door's seal letting go: a soft, padded thunk
        e.amp = 0.014f * level;
        const float f = 95.0f + 30.0f * random();
        constexpr float ratios[] { 1.0f, 1.7f, 2.6f };
        for (int k = 0; k < 3; ++k) mode(k, f * ratios[k], e.amp * std::pow(0.5f, float(k)), 0.16f);
        e.modes.retune(1.0f, false);
        click(e.amp * 0.6f, 0.02f, 500.0f);
        e.length = int(fs * 0.35f);
        e.dry = 0.35f; e.send = 0.7f;
        break;
    }
    case Kind::cart: {   // a trolley settling somewhere: two or three small clinks
        e.amp = 0.008f * level;
        e.repeats = 2 + int(random() * 2.0f);
        e.f = 2200.0f + 1200.0f * random();
        e.bp1.set(e.f, 8.0f, sr);
        e.rate = 0.08f + 0.08f * random();
        e.length = int(fs * (float(e.repeats) * e.rate + 0.3f));
        e.decay = t60(0.03, sr);
        e.dry = 0.25f; e.send = 0.8f;
        break;
    }
    case Kind::ghostNote: {   // a reflection of a note nobody played: only in the room
        constexpr float notes[] { 392.0f, 440.0f, 523.25f, 587.33f, 659.26f, 783.99f };
        const float f = notes[int(random() * 6.0f) % 6] * (1.0f + 0.006f * bipolar());
        e.amp = 0.010f * level;
        mode(0, f, e.amp, 2.4f);
        mode(1, f * 2.001f, e.amp * 0.3f, 1.4f);
        mode(2, f * 3.0f, e.amp * 0.1f, 0.8f);
        e.modes.retune(1.0f, false);
        click(0.0f, 0.01f, 3000.0f);
        e.length = int(fs * 3.0f);
        e.dry = 0.0f; e.send = 1.0f;
        break;
    }
    case Kind::wrongWall: {   // a knock from a direction where there is no wall: one side only, no room
        e.amp = 0.016f * level;
        const float f = 180.0f + 80.0f * random();
        mode(0, f, e.amp, 0.15f);
        mode(1, f * 2.3f, e.amp * 0.5f, 0.1f);
        e.modes.retune(1.0f, false);
        click(e.amp, 0.008f, 1400.0f);
        e.length = int(fs * 0.3f);
        e.pan = random() < 0.5f ? -1.0f : 1.0f;
        e.dry = 1.0f; e.send = 0.0f;
        break;
    }
    case Kind::flicker: break;
    }
    e.age = -delay;
    return &e;
}

Stereo Ambience::renderEvent(Event& e, float& sendOut) {
    if (e.age < 0) { ++e.age; return {}; }   // waiting to start
    const float fs = float(sr);
    const float t = float(e.age);
    const auto pg = panGains(e.pan);
    float mono = 0;
    Stereo st {};
    bool done = e.age >= e.length;
    switch (e.kind) {
    case Kind::monitor: {
        const int cycle = int(e.rate) + e.gap;
        const int at = e.age % std::max(1, cycle);
        if (at < int(e.rate)) {
            const float ramp = std::min({ 1.0f, float(at) / (0.006f * fs), (e.rate - float(at)) / (0.012f * fs) });
            e.phase += e.f / fs;
            if (e.phase >= 1.0f) e.phase -= 1.0f;
            mono = e.amp * ramp * sinCycle(e.phase);
        }
        mono = e.lp.lp(mono);
        break;
    }
    case Kind::shutter:
    case Kind::cart: {
        if (e.repeats > 0 && e.age >= e.step) {
            e.x = e.amp * (0.5f + 0.5f * e.noise.uniform());
            e.step = e.age + int(fs * e.rate * (0.7f + 0.6f * e.noise.uniform()));
            --e.repeats;
        }
        const float n = e.x * e.noise.bipolar();
        e.x *= e.decay;
        e.bp1.tick(n);
        mono = e.bp1.band * 3.0f;
        break;
    }
    case Kind::drip:
        if (place == Place::pool) {
            const float rise = 1.0f + e.rate * t / fs;
            e.phase += std::min(0.45f, e.f * rise / fs);
            if (e.phase >= 1.0f) e.phase -= 1.0f;
            mono = e.env * sinCycle(e.phase);
            e.env *= e.decay;
            break;
        }
        [[fallthrough]];
    case Kind::ballastTick:
    case Kind::doorClick:
    case Kind::paChime:
    case Kind::radiatorPing:
    case Kind::locker:
    case Kind::bell:
    case Kind::carDoor:
    case Kind::barrier:
    case Kind::liftChime:
    case Kind::doorClose:
    case Kind::railTick:
    case Kind::freezerSeal:
    case Kind::ghostNote:
    case Kind::wrongWall: {
        st = e.modes.tick();
        if (e.env > 1.0e-7f) {
            mono = e.lp.lp(e.noise.bipolar() * e.env);
            e.env *= e.decay;
        }
        if (e.kind == Kind::bell || e.kind == Kind::liftChime) {   // through walls and floors
            st.l = e.lp.lp(st.l);
            st.r = e.lp2.lp(st.r);
        }
        if (e.kind == Kind::paChime) {   // small horns: no lows
            st.l -= e.lp2.lp(st.l);
            st.r = st.l;
            st.l *= pg.l * 1.4f;
            st.r *= pg.r * 1.4f;
        }
        const float fade = std::clamp((float(e.length) - t) / (0.05f * fs), 0.0f, 1.0f);   // no cut at the end
        st.l *= fade;
        st.r *= fade;
        break;
    }
    case Kind::ripple: {
        const float u = t / float(e.length);
        const float s = sinCycle(0.5f * u);
        e.bp1.tick(e.noise.bipolar());
        mono = e.amp * s * s * e.bp1.band * 4.0f;
        break;
    }
    case Kind::trainRumble: {
        const float u = t / float(e.length);
        const float s = sinCycle(0.5f * u);
        e.phase += e.f / fs;
        if (e.phase >= 1.0f) e.phase -= 1.0f;
        e.phase2 += e.f * 1.5f / fs;
        if (e.phase2 >= 1.0f) e.phase2 -= 1.0f;
        e.x = 0.995f * e.x + 0.05f * e.noise.bipolar();
        mono = e.amp * s * s * (sinCycle(e.phase) + 0.4f * sinCycle(e.phase2) + 3.0f * e.lp.lp(e.x));
        break;
    }
    case Kind::flicker: done = true; break;
    }
    st.l += mono * pg.l;
    st.r += mono * pg.r;
    sendOut += 0.5f * (st.l + st.r) * e.send;
    e.age++;
    if (done) e.active = false;
    return { st.l * e.dry, st.r * e.dry };
}

Ambience::Out Ambience::tick() {
    if (--controlClock <= 0) {   // slow things, on a 64-sample control clock
        controlClock = 64;
        const float dt = float(64.0 / sr);
        buzzFlutter = buzzWander.tick(rng, 3.0f, dt);
        if (humDrifts) humDrift = humWander.tick(rng, 0.03f, dt);
        for (auto& d : drones) {
            if (d.level <= 0.0f || d.gate == Drone::Gate::always) continue;
            d.countdown -= 64;
            if (d.countdown <= 0) {   // on for a while, off for a while
                const bool starting = d.gTarget < 0.5f;
                d.gTarget = starting ? 1.0f : 0.0f;
                const float span = starting ? d.onSeconds : d.offSeconds;
                d.countdown = int(sr * double(span) * (0.7 + 0.6 * double(random())));
            }
        }
        if (dropouts) {   // the impossible room: a few seconds of absolute silence, now and then
            silenceCountdown -= 64;
            if (silenceCountdown <= 0) {
                const bool silent = silenceTarget > 0.5f;
                silenceTarget = silent ? 0.0f : 1.0f;
                silenceCountdown = int(sr * (silent ? 1.5 + 2.5 * double(random()) : 25.0 + 45.0 * double(random())));
            }
        }
    }
    float l = 0, r = 0;
    // Room tone: pink noise, two poles low, constant. Well under the hum.
    const float wl = noiseL.bipolar(), wr = noiseR.bipolar();
    if (airLevel > 0.0f) {
        l += airLevel * airL2.lp(airL1.lp(pinkL.next(wl)));
        r += airLevel * airR2.lp(airR1.lp(pinkR.next(wr)));
    }
    // Mains: 60 Hz and harmonics, each beating slowly against a near twin.
    mainsPhase += mainsStep * (1.0 + 0.07 * double(humDrift));
    if (mainsPhase >= 1.0) mainsPhase -= 1.0;
    const float mp = float(mainsPhase);
    if (humLevel > 0.0f) {
        float hl = 0, hr = 0;
        for (int k = 0; k < humPartials; ++k) {
            const float w = humWeights[std::size_t(k)];
            if (w <= 0.0f) continue;
            float& bp = beatPhase[std::size_t(k)];
            bp += beatHz[std::size_t(k)] / float(sr);
            if (bp >= 1.0f) bp -= 1.0f;
            float p = mp * float(k + 1);
            p -= std::floor(p);
            const float v = w * sinCycle(p);
            const float b = beatDepth * sinCycle(bp);
            hl += v * (1.0f + b);
            hr += v * (1.0f - b);
        }
        // Where nothing buzzes (incandescent sconces, downlights, the impossible room's light from
        // nowhere) a failing light is heard as the mains hum sagging with it.
        const float sag = buzzLevel > 0.0f ? 1.0f : 0.55f + 0.45f * lampNow;
        l += humLevel * sag * hl;
        r += humLevel * sag * hr * 0.92f;
    }
    // The failing tube: a few quick flicks, then steady again.
    if (stutterFlicks > 0 && --stutterCountdown <= 0) {
        const bool dark = lampTarget >= 1.0f;
        lampTarget = dark ? 1.0f - stutterDepth * (0.6f + 0.4f * random()) : 1.0f;
        stutterCountdown = int(sr * (dark ? 0.02 + 0.07 * double(random()) : 0.04 + 0.18 * double(random())));
        if (!dark) { --stutterFlicks; restrike = 1.0f; }
    }
    lampNow += lampCoef * (lampTarget - lampNow);
    if (buzzLevel > 0.0f) {
        // Full-wave shape: 120 Hz with its harmonics; a tiny random flicker in level.
        const float s = sinCycle(mp);
        const float rect = std::abs(s) - 0.6366f;   // |sin| without its DC
        float p2 = 2.0f * mp, p4 = 4.0f * mp;
        p2 -= std::floor(p2);
        p4 -= std::floor(p4);
        const float tone = rect + 0.35f * sinCycle(p2) + 0.12f * sinCycle(p4);
        // The arc re-ignites near each zero crossing: a burst of grit 120 times a second.
        const float near = 1.0f - std::abs(s);
        const float gate = near * near * near * near;
        gritL.tick(wl * gate);
        gritR.tick(wr * gate);
        const float lit = lampNow * (1.0f + 0.03f * buzzFlutter);
        const float sputter = 1.0f + 2.5f * restrike;
        restrike *= 0.9995f;
        l += buzzLevel * lit * tone + gritLevel * lit * sputter * gritL.band;
        r += buzzLevel * lit * tone * 0.85f + gritLevel * lit * sputter * gritR.band;
    }
    for (auto& d : drones) {   // motors, pumps, fans, compressors: steady tones
        if (d.level <= 0.0f) continue;
        d.g += d.gCoef * (d.gTarget - d.g);
        d.phase += d.hz / float(sr);
        if (d.phase >= 1.0f) d.phase -= 1.0f;
        d.amPhase += d.amHz / float(sr);
        if (d.amPhase >= 1.0f) d.amPhase -= 1.0f;
        float m = 0;
        for (int k = 0; k < 4; ++k) {
            if (d.weights[std::size_t(k)] <= 0.0f) continue;
            float p = d.phase * float(k + 1);
            p -= std::floor(p);
            m += d.weights[std::size_t(k)] * sinCycle(p);
        }
        m *= d.level * d.g * (1.0f - d.amDepth * (0.5f + 0.5f * sinCycle(d.amPhase)));
        l += m;
        r += m * 0.9f;
    }
    if (clockLevel > 0.0f) {   // the wall clock: once a second, tick and tock slightly different
        if (--clockCountdown <= 0) {
            clockCountdown = int(sr);
            clockEnv = clockLevel * ((clockBeat++ & 1) ? 0.8f : 1.0f);
            clockTone.set((clockBeat & 1) ? 2300.0f : 2050.0f, 6.0f, sr);
        }
        clockTone.tick(wl * clockEnv);
        clockEnv *= 0.997f;
        l += clockTone.band * 0.9f;
        r += clockTone.band * 0.7f;
    }
    if (trickleLevel > 0.0f) {   // the fountain, far off: tiny drops, steady, never splashing
        if (random() < float(35.0 / sr)) {
            trickleEnv = trickleLevel * (0.4f + 0.6f * random());
            trickleA.set(1300.0f + 1500.0f * random(), 9.0f, sr);
            trickleB.set(2400.0f + 1500.0f * random(), 9.0f, sr);
        }
        const float x = trickleEnv * wl;
        trickleEnv *= 0.995f;
        trickleA.tick(x);
        trickleB.tick(x);
        l += trickleA.band;
        r += trickleB.band;
    }
    if (whineLevel > 0.0f) {   // a coil whining, almost too high to notice
        whinePhase += whineHz / float(sr);
        if (whinePhase >= 1.0f) whinePhase -= 1.0f;
        const float w = whineLevel * sinCycle(whinePhase) * lampNow;
        l += w;
        r += 0.7f * w;
    }
    float send = 0.5f * (l + r) * bedSend;
    // Rare events, one scheduler for the place.
    sinceLast += double(activity);
    if (activity > 0.0f && scheduleCount > 0) {
        countdown -= double(activity);
        if (countdown <= 0.0) {
            float pick = random() * weightSum;
            int which = 0;
            for (; which < scheduleCount - 1; ++which) {
                pick -= schedules[std::size_t(which)].weight;
                if (pick <= 0.0f) break;
            }
            const auto& sc = schedules[std::size_t(which)];
            spawn(sc.kind, sc.level);
            scheduleNext();
        }
    }
    float eventSend = 0;
    for (auto& e : events)
        if (e.active) {
            const Stereo o = renderEvent(e, eventSend);
            l += o.l;
            r += o.r;
        }
    send += eventSend;
    if (dropouts) {   // falls away in 30 ms, comes back over half a second
        silenceGain += (silenceTarget < silenceGain ? silenceFall : silenceRise) * (silenceTarget - silenceGain);
        l *= silenceGain;
        r *= silenceGain;
        send *= silenceGain;
    }
    return { { l * trim, r * trim }, { send * trim, send * trim } };
}

} // namespace liminal::dsp
