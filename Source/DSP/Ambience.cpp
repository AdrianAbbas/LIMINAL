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
    // Defaults: everything off, then each place switches on its own generators.
    airLevel = ductLevel = lapLevel = rumbleLevel = humLevel = buzzLevel = motorLevel = subLevel = clackLevel = 0;
    whineLevel = clusterLevel = 0;
    motorCycles = false;
    humWeights = {};
    motorWeights = {};
    double mains = 60.0;
    float airHz = 1400.0f, ductHz = 340.0f, ductQ = 8.0f, rumbleHz = 120.0f;
    bedSend = 0.3f;
    scheduleCount = 0;
    const auto add = [&](Kind k, float mean, float minimum, float level, float send) {
        if (scheduleCount < int(schedules.size())) schedules[std::size_t(scheduleCount++)] = { k, mean, minimum, level, send, 0 };
    };
    switch (p) {
    case Place::hospital:   // cold air handling, 60 Hz lights, distant monitors
        airLevel = 0.06f; airHz = 1500.0f;
        humLevel = 0.0045f; humWeights = { 1.0f, 0.5f, 0.25f, 0.1f };
        buzzLevel = 0.0024f;
        bedSend = 0.35f;
        add(Kind::beep, 9.0f, 2.5f, 1.0f, 0.8f);
        add(Kind::rattle, 22.0f, 6.0f, 1.0f, 0.5f);
        add(Kind::metal, 30.0f, 8.0f, 1.0f, 0.75f);
        break;
    case Place::mall:       // a huge hall: low roar, far ventilation, the escalator
        mains = 60.0;
        airLevel = 0.045f; airHz = 700.0f;
        rumbleLevel = 0.05f; rumbleHz = 220.0f;
        motorHz = 47.0f; motorLevel = 0.006f; motorWeights = { 1.0f, 0.6f, 0.35f, 0.2f }; motorAmRate = 0.9f; motorAmDepth = 0.35f;
        clackLevel = 0.012f; clackInterval = 0.55f;
        bedSend = 0.7f;
        add(Kind::groan, 14.0f, 5.0f, 1.0f, 0.9f);
        add(Kind::metal, 16.0f, 5.0f, 1.0f, 0.6f);
        add(Kind::thud, 24.0f, 8.0f, 1.0f, 0.8f);
        break;
    case Place::school:     // old ballasts, ducts that whistle, pipes and creaks
        mains = 50.0;
        airLevel = 0.03f; airHz = 900.0f;
        ductLevel = 0.03f; ductHz = 340.0f; ductQ = 10.0f;
        humLevel = 0.006f; humWeights = { 1.0f, 0.8f, 0.6f, 0.3f, 0.2f };
        buzzLevel = 0.0035f;
        bedSend = 0.25f;
        add(Kind::creak, 7.0f, 2.0f, 1.0f, 0.4f);
        add(Kind::pipe, 19.0f, 7.0f, 1.0f, 0.5f);
        add(Kind::ticks, 9.0f, 3.0f, 1.0f, 0.35f);
        add(Kind::thud, 40.0f, 12.0f, 0.6f, 0.7f);
        break;
    case Place::garage:     // concrete: deep ventilation, sub vibration, cars far off
        mains = 50.0;
        airLevel = 0.035f; airHz = 400.0f;
        rumbleLevel = 0.12f; rumbleHz = 110.0f;
        motorHz = 26.0f; motorLevel = 0.006f; motorWeights = { 1.0f, 0.7f, 0.45f, 0.3f }; motorAmRate = 2.1f; motorAmDepth = 0.5f;
        subHz = 38.0f; subLevel = 0.008f;
        humLevel = 0.002f; humWeights = { 1.0f, 0.3f, 0.5f };
        bedSend = 0.45f;
        add(Kind::car, 26.0f, 9.0f, 0.55f, 0.6f);
        add(Kind::thud, 16.0f, 5.0f, 0.6f, 0.7f);
        add(Kind::metal, 21.0f, 6.0f, 0.8f, 0.65f);
        add(Kind::rattle, 35.0f, 12.0f, 0.7f, 0.6f);
        break;
    case Place::pool:       // tiles and water: drips, lapping, a low pump
        mains = 50.0;
        airLevel = 0.06f; airHz = 500.0f;
        lapLevel = 0.2f;
        motorHz = 50.0f; motorLevel = 0.003f; motorWeights = { 1.0f, 0.4f, 0.2f }; motorAmRate = 0.15f; motorAmDepth = 0.2f;
        bedSend = 0.55f;
        add(Kind::drip, 1.8f, 0.2f, 1.6f, 0.6f);
        add(Kind::plop, 9.0f, 2.0f, 1.4f, 0.7f);
        add(Kind::metal, 18.0f, 6.0f, 0.7f, 0.8f);
        break;
    case Place::hotel:      // carpeted quiet: low air, the lift in its shaft, voices through the wall
        mains = 60.0;
        airLevel = 0.06f; airHz = 1000.0f;
        humLevel = 0.006f; humWeights = { 0.2f, 1.0f, 0.4f };
        bedSend = 0.2f;
        add(Kind::elevator, 16.0f, 6.0f, 1.0f, 0.25f);
        add(Kind::murmur, 11.0f, 4.0f, 1.0f, 0.2f);
        add(Kind::thud, 14.0f, 4.0f, 0.7f, 0.3f);
        add(Kind::creak, 12.0f, 3.0f, 0.45f, 0.3f);
        break;
    case Place::subway:     // tunnel air, the substation's rectifier hum, rails singing far off
        mains = 60.0;
        airLevel = 0.03f; airHz = 650.0f;
        rumbleLevel = 0.07f; rumbleHz = 85.0f;
        subHz = 31.0f; subLevel = 0.009f;
        humLevel = 0.0028f; humWeights = { 1.0f, 0.25f, 0.1f, 0.05f, 0.0f, 0.5f, 0.0f, 0.2f };
        bedSend = 0.5f;
        add(Kind::rail, 20.0f, 7.0f, 1.0f, 0.8f);
        add(Kind::gust, 13.0f, 4.0f, 1.0f, 0.6f);
        add(Kind::metal, 12.0f, 4.0f, 1.0f, 0.55f);
        add(Kind::ticks, 9.0f, 2.5f, 1.0f, 0.4f);
        break;
    case Place::supermarket:   // refrigeration cycling, fluorescent hum, a till that beeps at no one
        mains = 60.0;
        airLevel = 0.025f; airHz = 1300.0f;
        ductLevel = 0.012f; ductHz = 1150.0f; ductQ = 14.0f;   // a cold-case fan's tone
        humLevel = 0.007f; humWeights = { 1.0f, 0.6f, 0.3f, 0.15f };
        buzzLevel = 0.003f;
        motorHz = 49.0f; motorLevel = 0.007f; motorWeights = { 1.0f, 0.8f, 0.5f, 0.3f }; motorAmRate = 0.3f; motorAmDepth = 0.15f;
        motorCycles = true;
        bedSend = 0.35f;
        add(Kind::beep, 14.0f, 4.0f, 1.0f, 0.5f);
        add(Kind::rattle, 15.0f, 5.0f, 1.0f, 0.4f);
        add(Kind::ticks, 10.0f, 3.0f, 1.0f, 0.35f);
        add(Kind::clunk, 45.0f, 15.0f, 0.8f, 0.5f);
        break;
    case Place::office:     // HVAC, transformers, coil whine, drives seeking in machines nobody uses
        mains = 60.0;
        airLevel = 0.06f; airHz = 850.0f;
        ductLevel = 0.01f; ductHz = 210.0f; ductQ = 4.0f;
        humLevel = 0.0045f; humWeights = { 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.3f };
        buzzLevel = 0.0018f;
        whineHz = 9400.0f; whineLevel = 0.0009f;
        motorHz = 63.0f; motorLevel = 0.002f; motorWeights = { 0.5f, 1.0f, 0.3f }; motorAmRate = 0.07f; motorAmDepth = 0.1f;
        bedSend = 0.4f;
        add(Kind::seek, 6.0f, 1.5f, 1.0f, 0.4f);
        add(Kind::chirp, 9.0f, 3.0f, 1.0f, 0.5f);
        add(Kind::ticks, 17.0f, 5.0f, 0.7f, 0.5f);
        add(Kind::thud, 30.0f, 10.0f, 0.5f, 0.6f);
        break;
    case Place::impossible:   // a room tone that will not settle, things that arrive backwards
        mains = 50.0;
        airLevel = 0.08f; airHz = 380.0f;
        clusterLevel = 0.011f;
        bedSend = 0.5f;
        add(Kind::swell, 8.0f, 3.0f, 1.0f, 0.7f);
        add(Kind::phantom, 11.0f, 4.0f, 1.0f, 0.8f);
        add(Kind::ticks, 14.0f, 3.0f, 0.6f, 0.7f);
        add(Kind::thud, 22.0f, 7.0f, 0.6f, 0.8f);
        break;
    }
    if (ch != nullptr) {   // the Character: which events, how often, and the bed
        int kept = 0;
        for (int i = 0; i < scheduleCount; ++i) {
            auto sc = schedules[std::size_t(i)];
            const float w = ch->eventWeight[std::size_t(std::min(i, 4))] * ch->eventRate;
            if (w <= 0.0f) continue;
            sc.meanSeconds /= w;
            sc.minSeconds /= std::max(1.0f, std::sqrt(w));
            schedules[std::size_t(kept++)] = sc;
        }
        scheduleCount = kept;
        if (ch->extraKind >= 0) add(Kind(ch->extraKind), ch->extraMean / std::max(0.1f, ch->eventRate), ch->extraMean * 0.25f, 0.9f, 0.6f);
        airLevel *= ch->bed;
        ductLevel *= ch->bed;
        lapLevel *= ch->bed;
        rumbleLevel *= ch->bed;
        motorLevel *= ch->bed;
        clusterLevel *= ch->bed;
        airHz = std::clamp(airHz * ch->bedTone, 80.0f, 6000.0f);
        humLevel *= ch->hum;
        buzzLevel *= ch->hum;
        whineLevel *= ch->hum;
    }
    mainsStep = mains / sr;
    airL.set(airHz, 0.6f, sr);
    airR.set(airHz * 1.03f, 0.6f, sr);
    ductL.set(ductHz, ductQ, sr);
    ductR.set(ductHz * 2.03f, ductQ, sr);   // the second duct mode on the other side
    lapL.set(520.0f, 1.2f, sr);
    lapR.set(610.0f, 1.2f, sr);
    rumbleL.setHz(rumbleHz, sr);
    rumbleR.setHz(rumbleHz * 1.1f, sr);
    breathCoef = float(1.0 - std::exp(-1.0 / (1.8 * sr)));
    buzzCoef = float(1.0 - std::exp(-1.0 / (0.004 * sr)));
    clackDecay = t60(0.04, sr);
    clackTone.set(1800.0f, 3.0f, sr);
    motorGateCoef = float(1.0 - std::exp(-1.0 / (0.6 * sr)));
    {   // the impossible room's cluster: no two tones in a simple ratio
        constexpr float ratios[] { 1.0f, 1.414f, 1.887f, 2.519f, 3.174f, 4.237f };
        for (int k = 0; k < clusterTones; ++k) clusterHz[std::size_t(k)] = 97.0f * ratios[k];
    }
    reset();
}

void Ambience::reset() {
    rng = {};
    rng.state = 0x9E3779B9u ^ (seed * 0x85EBCA6Bu) ^ (unsigned(place) * 0xC2B2AE35u);
    noiseL.state = rng.state ^ 0x1234567u;
    noiseR.state = rng.state ^ 0x7654321u;
    pinkL = {};
    pinkR = {};
    for (auto* f : { &airL, &airR, &ductL, &ductR, &lapL, &lapR, &clackTone }) f->clear();
    rumbleL.z = rumbleR.z = brownL = brownR = 0;
    breath = breathTarget = 1;
    breathCountdown = 0;
    lap = lapTarget = 0.5f;
    lapCountdown = 0;
    mainsPhase = 0;
    buzz = buzzTarget = 1;
    buzzCountdown = int(sr * 3);
    motorPhase = motorAmPhase = subPhase = 0;
    subWander.start(rng);
    clackCountdown = int(sr * 0.3);
    clackEnv = 0;
    // (Only the places that use them draw from the generator, so the others' event
    // sequences stay exactly as they were.)
    motorGate = motorGateTarget = 1;
    cycleCountdown = motorCycles ? int(sr * (8.0 + 10.0 * random())) : 0;
    whinePhase = 0;
    for (int k = 0; k < clusterTones; ++k) {
        clusterWeight[std::size_t(k)] = 0.5f;
        if (clusterLevel <= 0.0f) continue;
        clusterPhase[std::size_t(k)] = random();
        clusterWander[std::size_t(k)].start(rng);
    }
    for (auto& e : events) e.active = false;
    // First events come at a random point of their usual interval, not all at once.
    for (int i = 0; i < scheduleCount; ++i) {
        auto& s = schedules[std::size_t(i)];
        s.countdown = float(sr) * (s.minSeconds + random() * s.meanSeconds);
    }
}

void Ambience::disturb() {
    if (scheduleCount == 0) return;
    Schedule s = schedules[std::size_t(int(random() * float(scheduleCount)) % scheduleCount)];
    s.level *= 1.4f;
    spawn(s);
}

Ambience::Event* Ambience::spawn(const Schedule& s, int delay) {
    Event* slot = nullptr;
    int busy = 0;
    for (auto& e : events) busy += e.active ? 1 : 0;
    if (busy >= eventCap) return nullptr;
    for (auto& e : events)
        if (!e.active) { slot = &e; break; }
    if (slot == nullptr) return nullptr;
    auto& e = *slot;
    e = Event {};
    e.active = true;
    e.kind = s.kind;
    e.age = 0;
    e.send = s.send;
    e.pan = bipolar() * 0.8f;
    e.noise.state = rng.state ^ 0xDEADBEEFu;
    e.noise.uniform();
    const float fs = float(sr);
    const float limit = 0.45f * fs;
    const auto pg = panGains(e.pan);
    switch (s.kind) {
    case Kind::beep: {   // a distant monitor: one to three soft tones (a till scanner: one short high one)
        constexpr float tones[] { 698.5f, 880.0f, 987.8f, 1046.5f, 1318.5f };
        const bool till = place == Place::supermarket;
        e.f = till ? 2850.0f + 150.0f * random() : tones[int(random() * 5.0f) % 5] * (1.0f + 0.004f * bipolar());
        e.rate = fs * (till ? 0.07f + 0.03f * random() : 0.1f + 0.15f * random());          // tone length
        e.gap = int(fs * (0.3f + 0.6f * random()));
        e.repeats = till ? (random() < 0.2f ? 2 : 1) : 1 + int(random() * 3.0f);
        e.length = e.repeats * (int(e.rate) + e.gap);
        e.amp = 0.05f * s.level * (0.5f + random());
        e.lp.setHz(till ? 5000.0f : 2600.0f, sr);
        break;
    }
    case Kind::rattle: {   // a loose vent cover in a gust of air
        e.length = int(fs * (0.4f + 1.6f * random()));
        e.rate = 12.0f + 25.0f * random();
        e.f = place == Place::garage ? 250.0f + 200.0f * random() : place == Place::supermarket ? 1400.0f + 1500.0f * random() : 650.0f + 1100.0f * random();
        e.bp1.set(e.f, 6.0f, sr);
        e.bp2.set(e.f * 2.3f, 5.0f, sr);
        e.amp = 0.14f * s.level * (0.5f + random());
        break;
    }
    case Kind::metal: {   // something metal struck: free-bar modes 1 : 2.76 : 5.40 : 8.93
        constexpr float ratios[] { 1.0f, 2.756f, 5.404f, 8.933f };
        const float f0 = place == Place::garage ? 90.0f + 160.0f * random() : place == Place::pool ? 420.0f + 500.0f * random()
                       : place == Place::subway ? 260.0f + 300.0f * random() : 180.0f + 520.0f * random();
        const float ring = 0.4f + 2.0f * random();
        e.amp = 0.05f * s.level * (0.4f + random());
        for (int k = 0; k < 4; ++k) {
            const float f = f0 * ratios[k] * (1.0f + 0.01f * bipolar());
            const float a = f < limit ? e.amp * std::pow(0.55f, float(k)) : 0.0f;
            e.modes.set(k, a, 0.0f, twoPi * f / fs, t60(ring / (1.0f + 0.6f * float(k)), sr), 1.0f, pg.l, pg.r);
        }
        e.modes.retune(1.0f, false);
        e.env = e.amp * 2.0f;   // the strike's click
        e.decay = t60(0.008, sr);
        e.length = int(fs * (ring + 0.2f));
        break;
    }
    case Kind::thud: {   // a door, a joint in the floor: low, hollow
        const float f0 = 55.0f + 60.0f * random();
        constexpr float ratios[] { 1.0f, 1.59f, 2.14f, 2.65f };
        e.amp = 0.15f * s.level * (0.5f + random());
        for (int k = 0; k < 4; ++k) e.modes.set(k, e.amp * std::pow(0.6f, float(k)), 0.0f, twoPi * f0 * ratios[k] / fs, t60(0.35f - 0.05f * float(k), sr), 1.0f, pg.l, pg.r);
        e.modes.retune(1.0f, false);
        e.env = e.amp * 1.5f;
        e.decay = t60(0.03, sr);
        e.lp.setHz(place == Place::hotel ? 200.0f : 500.0f, sr);   // in a hotel: a door in another room
        e.length = int(fs * 0.6f);
        // A car crossing an expansion joint: two thuds, front and back wheels.
        if (delay == 0 && place == Place::garage && random() < 0.6f) {
            const float pan = e.pan;
            if (auto* back = spawn(s, int(fs * (0.12f + 0.15f * random())))) back->pan = pan;
        }
        break;
    }
    case Kind::drip: {   // a drop into water: a rising sine "plink" (bubble resonance)
        e.f = 700.0f + 1600.0f * random();
        e.rate = 8.0f + 30.0f * random();   // pitch rise per second (relative)
        e.decay = t60(0.04 + 0.05 * random(), sr);
        e.amp = 0.08f * s.level * (0.3f + random());
        e.env = e.amp;
        e.length = int(fs * 0.25f);
        e.pan = bipolar() * 0.9f;
        // A leaking spot drips for a while at its own pace, then stops.
        if (random() < 0.25f) { e.repeats = 2 + int(random() * 6.0f); e.gap = int(fs * (0.6f + 0.9f * random())); }
        break;
    }
    case Kind::plop: {   // a bigger drop, lower and slower
        e.f = 180.0f + 260.0f * random();
        e.rate = 3.0f + 6.0f * random();
        e.decay = t60(0.12 + 0.1 * random(), sr);
        e.amp = 0.1f * s.level * (0.4f + random());
        e.env = e.amp;
        e.length = int(fs * 0.5f);
        break;
    }
    case Kind::creak: {   // stick-slip friction through wood/metal resonances
        e.length = int(fs * (0.4f + 1.5f * random()));
        e.rate = 15.0f + 40.0f * random();
        e.rate2 = e.rate * (0.5f + 1.5f * random());
        e.f = 220.0f + 300.0f * random();
        e.bp1.set(e.f, 9.0f, sr);
        e.bp2.set(e.f * 2.6f, 7.0f, sr);
        e.amp = 0.6f * s.level * (0.4f + random());
        break;
    }
    case Kind::pipe: {   // water moving in old pipes, with a knock as it starts
        e.length = int(fs * (2.0f + 4.0f * random()));
        e.f = 260.0f + 340.0f * random();
        e.bp1.set(e.f, 1.6f, sr);
        e.rate = 4.0f + 8.0f * random();
        e.amp = 0.2f * s.level * (0.5f + random());
        const float knock = 120.0f + 80.0f * random();
        e.modes.set(0, 0.03f * s.level, 0.0f, twoPi * knock / fs, t60(0.25, sr), 1.0f, pg.l, pg.r);
        e.modes.set(1, 0.015f * s.level, 0.0f, twoPi * knock * 2.3f / fs, t60(0.15, sr), 1.0f, pg.l, pg.r);
        e.modes.retune(1.0f, false);
        break;
    }
    case Kind::groan: {   // the building's steel moving: slow, low, resonant
        e.length = int(fs * (2.0f + 3.5f * random()));
        e.f = 85.0f + 140.0f * random();
        e.rate = 0.15f + 0.3f * random();
        e.bp1.set(e.f, 25.0f, sr);
        e.bp2.set(e.f * 2.31f, 25.0f, sr);
        e.lp.setHz(1200.0f, sr);
        e.amp = 2.5f * s.level * (0.5f + random());
        break;
    }
    case Kind::car: {   // a car somewhere on another level: engine and tyres, passing
        e.length = int(fs * (5.0f + 5.0f * random()));
        e.f = 32.0f + 22.0f * random();
        e.pan = random() < 0.5f ? -0.9f : 0.9f;
        e.lp.setHz(280.0f, sr);
        e.amp = 0.08f * s.level * (0.5f + random());
        break;
    }
    case Kind::ticks: {   // a radiator or ceiling tile ticking as it cools (subway: relays)
        e.repeats = place == Place::subway ? 1 + int(random() * 3.0f) : 3 + int(random() * 8.0f);
        e.f = place == Place::subway ? 900.0f + 600.0f * random() : 1800.0f + 2400.0f * random();
        e.bp1.set(e.f, 4.0f, sr);
        e.amp = 0.5f * s.level * (0.4f + random());
        e.length = int(fs * 4.0f);
        e.gap = 0;
        e.decay = t60(0.01, sr);
        break;
    }
    case Kind::elevator: {   // the lift: a relay clunk, the motor spinning up, running, slowing, a clunk
        e.length = int(fs * (6.0f + 5.0f * random()));
        e.f = 95.0f + 40.0f * random();
        e.lp.setHz(650.0f, sr);
        e.bp1.set(e.f * 7.0f, 6.0f, sr);   // the cable sheave's whine
        e.amp = 0.03f * s.level * (0.6f + 0.6f * random());
        e.pan = bipolar() * 0.5f;
        Schedule clunk { Kind::clunk, 0, 0, s.level * 0.6f, s.send };
        spawn(clunk, 0);
        spawn(clunk, e.length - int(fs * 0.3f));
        break;
    }
    case Kind::murmur: {   // voices or a television in another room: syllables without words
        e.length = int(fs * (2.0f + 4.0f * random()));
        e.f = 0.0f;
        e.rate = 3.0f + 2.5f * random();   // syllables per second
        e.bp1.set(350.0f, 3.0f, sr);
        e.bp2.set(900.0f, 3.0f, sr);
        e.lp.setHz(480.0f, sr);
        e.amp = 0.35f * s.level * (0.5f + random());
        break;
    }
    case Kind::rail: {   // rails singing as a train moves somewhere in the tunnels; a far rumble swells
        e.length = int(fs * (6.0f + 6.0f * random()));
        e.f = 900.0f + 700.0f * random();
        e.rate = 0.3f + 0.5f * random();
        e.pan = random() < 0.5f ? -0.9f : 0.9f;
        e.lp.setHz(140.0f, sr);
        e.amp = 0.012f * s.level * (0.5f + random());
        break;
    }
    case Kind::gust: {   // air pushed through the tunnel: a long swell of wind
        e.length = int(fs * (4.0f + 5.0f * random()));
        e.f = 220.0f + 300.0f * random();
        e.bp1.set(e.f, 0.7f, sr);
        e.amp = 0.12f * s.level * (0.5f + random());
        break;
    }
    case Kind::clunk: {   // a relay or compressor starting: a short low knock with a rattle
        const float f0 = 110.0f + 90.0f * random();
        constexpr float ratios[] { 1.0f, 1.71f, 2.43f, 3.9f };
        e.amp = 0.09f * s.level * (0.6f + 0.6f * random());
        for (int k = 0; k < 4; ++k) e.modes.set(k, e.amp * std::pow(0.55f, float(k)), 0.0f, twoPi * f0 * ratios[k] / fs, t60(0.18f - 0.03f * float(k), sr), 1.0f, pg.l, pg.r);
        e.modes.retune(1.0f, false);
        e.env = e.amp;
        e.decay = t60(0.02, sr);
        e.lp.setHz(place == Place::hotel ? 300.0f : 1500.0f, sr);
        e.length = int(fs * 0.4f);
        break;
    }
    case Kind::seek: {   // a hard drive seeking: a burst of tiny clicks
        e.repeats = 3 + int(random() * 10.0f);
        e.f = 2800.0f + 1800.0f * random();
        e.bp1.set(e.f, 3.0f, sr);
        e.amp = 0.35f * s.level * (0.4f + random());
        e.length = int(fs * 2.0f);
        e.gap = 0;
        e.decay = t60(0.004, sr);
        e.pan = bipolar() * 0.6f;
        break;
    }
    case Kind::chirp: {   // a device somewhere answering nothing: short square blips
        e.f = 1100.0f + 2900.0f * random();
        e.rate = fs * (0.015f + 0.035f * random());
        e.gap = int(fs * (0.04f + 0.12f * random()));
        e.repeats = 1 + int(random() * 4.0f);
        e.length = e.repeats * (int(e.rate) + e.gap);
        e.amp = 0.012f * s.level * (0.5f + random());
        e.lp.setHz(3500.0f, sr);
        break;
    }
    case Kind::swell: {   // something arriving backwards: a band of noise growing, then gone
        e.length = int(fs * (1.5f + 3.0f * random()));
        e.f = 200.0f + 1800.0f * random();
        e.bp1.set(e.f, 5.0f, sr);
        e.amp = 0.15f * s.level * (0.5f + random());
        break;
    }
    case Kind::phantom: {   // two tones nobody plays, beating, out of nowhere
        constexpr float tones[] { 311.1f, 370.0f, 466.2f, 523.3f, 659.3f, 740.0f };
        e.f = tones[int(random() * 6.0f) % 6] * (1.0f + 0.01f * bipolar());
        e.rate = 0.6f + 2.0f * random();   // beating, Hz
        e.length = int(fs * (3.0f + 4.0f * random()));
        e.amp = 0.006f * s.level * (0.5f + random());
        break;
    }
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
    case Kind::beep: {
        const int cycle = int(e.rate) + e.gap;
        const int at = e.age % std::max(1, cycle);
        if (at < int(e.rate)) {
            const float ramp = std::min({ 1.0f, float(at) / (0.004f * fs), (e.rate - float(at)) / (0.004f * fs) });
            e.phase += e.f / fs;
            if (e.phase >= 1.0f) e.phase -= 1.0f;
            float p3 = 3.0f * e.phase;
            p3 -= std::floor(p3);
            mono = e.amp * ramp * (sinCycle(e.phase) + 0.15f * sinCycle(p3));
        }
        mono = e.lp.lp(mono);
        break;
    }
    case Kind::rattle: {
        e.phase += e.rate * (1.0f + 0.3f * e.noise.bipolar()) / fs;
        if (e.phase >= 1.0f) { e.phase -= 1.0f; e.env = 0.6f + 0.4f * e.noise.uniform(); }
        const float x = e.env * e.noise.bipolar();
        e.env *= 0.995f;
        e.bp1.tick(x);
        e.bp2.tick(x);
        const float s = std::sin(pi * t / float(e.length));
        mono = e.amp * s * s * (e.bp1.band + 0.6f * e.bp2.band);
        break;
    }
    case Kind::metal:
    case Kind::thud:
    case Kind::pipe: {
        st = e.modes.tick();
        if (e.kind != Kind::pipe && e.env > 1.0e-6f) {
            const float n = e.noise.bipolar() * e.env;
            mono = e.kind == Kind::thud ? e.lp.lp(n) : n - e.lp.lp(n);
            e.env *= e.decay;
        }
        if (e.kind == Kind::pipe) {
            e.phase += e.rate * (0.7f + 0.6f * e.noise.uniform()) / fs;
            if (e.phase >= 1.0f) e.phase -= 1.0f;
            e.bp1.tick(e.noise.bipolar());
            const float s = std::sin(pi * t / float(e.length));
            mono = e.amp * s * (0.55f + 0.45f * sinCycle(e.phase)) * e.bp1.band;
        }
        break;
    }
    case Kind::drip:
    case Kind::plop: {
        const int local = e.repeats > 0 && e.gap > 0 ? e.age % e.gap : e.age;
        if (local == 0 && e.age > 0) { e.env = e.amp * (0.7f + 0.3f * e.noise.uniform()); e.phase = 0; }
        const float rise = 1.0f + e.rate * float(local) / fs;   // the bubble's pitch rises as it shrinks
        e.phase += std::min(0.45f, e.f * rise / fs);
        if (e.phase >= 1.0f) e.phase -= 1.0f;
        mono = e.env * sinCycle(e.phase);
        e.env *= e.decay;
        if (e.repeats > 0 && e.gap > 0) done = e.age >= e.repeats * e.gap;
        break;
    }
    case Kind::creak: {
        const float rate = e.rate + (e.rate2 - e.rate) * t / float(e.length);
        e.phase += rate * (1.0f + 0.25f * e.noise.bipolar()) / fs;
        float x = 0;
        if (e.phase >= 1.0f) { e.phase -= 1.0f; x = 1.0f; }
        e.bp1.tick(x);
        e.bp2.tick(x);
        const float s = std::sin(pi * t / float(e.length));
        mono = e.amp * s * (e.bp1.band + 0.5f * e.bp2.band);
        break;
    }
    case Kind::groan: {
        if ((e.age & 63) == 0) {
            e.phase2 += 64.0f * e.rate / fs;
            const float bendBy = 1.0f + 0.05f * std::sin(twoPi * e.phase2);
            e.bp1.set(e.f * bendBy, 25.0f, sr);
            e.bp2.set(e.f * 2.31f * bendBy, 25.0f, sr);
        }
        const float x = e.noise.bipolar();
        e.bp1.tick(x);
        e.bp2.tick(x);
        const float s = std::sin(pi * t / float(e.length));
        mono = e.lp.lp(e.amp * s * s * e.bp1.k * (e.bp1.band + 0.7f * e.bp2.band));
        break;
    }
    case Kind::car: {
        const float u = t / float(e.length);
        const float position = (e.pan < 0 ? -0.9f : 0.9f) * (1.0f - 2.0f * u);
        const auto g = panGains(position);
        const float doppler = 1.0f + 0.035f * (0.5f - u);
        e.phase += e.f * doppler / fs;
        if (e.phase >= 1.0f) e.phase -= 1.0f;
        float p2 = 2.0f * e.phase, p3 = 3.0f * e.phase;
        p2 -= std::floor(p2);
        p3 -= std::floor(p3);
        e.x = 0.995f * e.x + 0.05f * e.noise.bipolar();   // tyres: brown noise
        const float s = std::sin(pi * u);
        const float sound = e.amp * s * s * (sinCycle(e.phase) + 0.6f * sinCycle(p2) + 0.4f * sinCycle(p3) + 6.0f * e.lp.lp(e.x));
        sendOut += sound * e.send;
        e.age++;
        if (e.age >= e.length) e.active = false;
        return { sound * g.l, sound * g.r };
    }
    case Kind::elevator: {
        const float u = t / float(e.length);
        const float ramp = std::clamp(std::min(u, 1.0f - u) * 6.0f, 0.0f, 1.0f);   // spin up, run, slow down
        e.phase += e.f * (0.8f + 0.2f * ramp) / fs;
        if (e.phase >= 1.0f) e.phase -= 1.0f;
        float p2 = 2.0f * e.phase, p3 = 3.0f * e.phase;
        p2 -= std::floor(p2);
        p3 -= std::floor(p3);
        e.bp1.tick(e.noise.bipolar());
        mono = e.lp.lp(e.amp * ramp * (sinCycle(e.phase) + 0.5f * sinCycle(p2) + 0.3f * sinCycle(p3)) + e.amp * 2.0f * ramp * e.bp1.band);
        break;
    }
    case Kind::murmur: {
        e.phase += e.rate * (0.7f + 0.6f * e.noise.uniform()) / fs;
        if (e.phase >= 1.0f) {   // the next syllable: a new vowel
            e.phase -= 1.0f;
            e.bp1.set(280.0f + 300.0f * e.noise.uniform(), 3.0f, sr);
            e.bp2.set(800.0f + 900.0f * e.noise.uniform(), 3.0f, sr);
            e.env = 0.3f + 0.7f * e.noise.uniform();
        }
        const float x = e.noise.bipolar();
        e.bp1.tick(x);
        e.bp2.tick(x);
        const float syllable = e.env * sinCycle(0.5f * e.phase);
        const float s = std::sin(pi * t / float(e.length));
        mono = e.lp.lp(e.amp * s * syllable * (e.bp1.band + 0.5f * e.bp2.band));
        break;
    }
    case Kind::rail: {
        const float u = t / float(e.length);
        const float s = std::sin(pi * u);
        const float position = (e.pan < 0 ? -0.9f : 0.9f) * (1.0f - 1.6f * u);
        const auto g = panGains(position);
        e.phase2 += e.rate / fs;
        if (e.phase2 >= 1.0f) e.phase2 -= 1.0f;
        float sing = 0;
        constexpr float ratios[] { 1.0f, 1.47f, 2.09f };
        for (int k = 0; k < 3; ++k) {
            float ph = e.phase * ratios[k];
            ph -= std::floor(ph);
            sing += sinCycle(ph) * (0.6f + 0.4f * sinCycle(std::fmod(e.phase2 + 0.33f * float(k), 1.0f))) / float(k + 1);
        }
        e.phase += e.f * (1.0f + 0.004f * sinCycle(e.phase2)) / fs;
        if (e.phase >= 1000.0f) e.phase -= 1000.0f;
        e.x = 0.996f * e.x + 0.04f * e.noise.bipolar();
        const float sound = e.amp * s * s * (sing + 12.0f * e.lp.lp(e.x) * s);
        sendOut += sound * e.send;
        e.age++;
        if (e.age >= e.length) e.active = false;
        return { sound * g.l, sound * g.r };
    }
    case Kind::gust: {
        const float u = t / float(e.length);
        const float s = std::sin(pi * u);
        if ((e.age & 63) == 0) e.bp1.set(e.f * (0.6f + 1.2f * s), 0.7f, sr);
        e.bp1.tick(e.noise.bipolar());
        const float position = 0.8f * (2.0f * u - 1.0f);
        const auto g = panGains(position);
        const float sound = e.amp * s * s * s * e.bp1.band;
        sendOut += sound * e.send;
        e.age++;
        if (e.age >= e.length) e.active = false;
        return { sound * g.l, sound * g.r };
    }
    case Kind::clunk: {
        st = e.modes.tick();
        if (e.env > 1.0e-6f) {
            mono = e.lp.lp(e.noise.bipolar() * e.env);
            e.env *= e.decay;
        }
        if (place == Place::hotel) { st.l *= 0.4f; st.r *= 0.4f; }
        break;
    }
    case Kind::seek: {
        if (e.gap <= 0 && e.repeats > 0) {
            e.env = e.amp * (0.6f + 0.4f * e.noise.uniform());
            e.gap = int(fs * (0.003f + 0.012f * e.noise.uniform()));
            --e.repeats;
        }
        --e.gap;
        const float x = e.env * e.noise.bipolar();
        e.env *= e.decay;
        e.bp1.tick(x);
        mono = e.bp1.band;
        done = done || (e.repeats <= 0 && e.gap <= 0);
        break;
    }
    case Kind::chirp: {
        const int cycle = int(e.rate) + e.gap;
        const int at = e.age % std::max(1, cycle);
        if (at < int(e.rate)) {
            e.phase += e.f / fs;
            if (e.phase >= 1.0f) e.phase -= 1.0f;
            mono = e.amp * (e.phase < 0.5f ? 1.0f : -1.0f);
        }
        mono = e.lp.lp(mono);
        break;
    }
    case Kind::swell: {
        const float u = t / float(e.length);
        const float fadeOut = std::clamp((float(e.length) - t) / (0.012f * fs), 0.0f, 1.0f);
        e.bp1.tick(e.noise.bipolar());
        mono = e.amp * u * u * u * fadeOut * e.bp1.band;
        break;
    }
    case Kind::phantom: {
        const float u = t / float(e.length);
        const float s = std::sin(pi * u);
        e.phase += e.f / fs;
        if (e.phase >= 1.0f) e.phase -= 1.0f;
        e.phase2 += (e.f + e.rate) / fs;
        if (e.phase2 >= 1.0f) e.phase2 -= 1.0f;
        st.l = e.amp * s * s * sinCycle(e.phase);
        st.r = e.amp * s * s * sinCycle(e.phase2);
        break;
    }
    case Kind::ticks: {
        if (e.gap <= 0 && e.repeats > 0) {
            e.env = e.amp * (0.5f + 0.5f * e.noise.uniform());
            e.gap = int(fs * (0.05f + 0.4f * e.noise.uniform()));
            --e.repeats;
        }
        --e.gap;
        const float x = e.env * e.noise.bipolar();
        e.env *= e.decay;
        e.bp1.tick(x);
        mono = e.bp1.band;
        done = done || (e.repeats <= 0 && e.gap <= 0);
        break;
    }
    }
    st.l += mono * pg.l;
    st.r += mono * pg.r;
    sendOut += 0.5f * (st.l + st.r) * e.send;
    e.age++;
    if (done) e.active = false;
    return st;
}

Ambience::Out Ambience::tick() {
    // The air breathes: a new target loudness every 2-6 s, approached slowly.
    if (--breathCountdown <= 0) {
        breathTarget = 0.75f + 0.5f * random();
        breathCountdown = int(sr * (2.0 + 4.0 * random()));
    }
    breath += breathCoef * (breathTarget - breath);
    const float wl = noiseL.bipolar(), wr = noiseR.bipolar();
    const float pl = pinkL.next(wl), pr = pinkR.next(wr);
    float l = 0, r = 0;
    if (airLevel > 0.0f) {
        airL.tick(pl);
        airR.tick(pr);
        l += airLevel * breath * airL.low;
        r += airLevel * breath * airR.low;
    }
    if (ductLevel > 0.0f) {   // ducts resonate: a faint, unsteady whistle
        ductL.tick(pl);
        ductR.tick(pr);
        l += ductLevel * breath * breath * ductL.band * ductL.k;
        r += ductLevel * breath * breath * ductR.band * ductR.k;
    }
    if (rumbleLevel > 0.0f) {   // brown noise: deep and heavy
        brownL = 0.998f * brownL + 0.02f * wl;
        brownR = 0.998f * brownR + 0.02f * wr;
        l += rumbleLevel * breath * rumbleL.lp(brownL);
        r += rumbleLevel * breath * rumbleR.lp(brownR);
    }
    if (lapLevel > 0.0f) {   // water against the pool's edge, in slow irregular waves
        if (--lapCountdown <= 0) {
            lapTarget = 0.15f + 0.85f * random();
            lapCountdown = int(sr * (0.8 + 1.8 * random()));
        }
        lap += float(1.0 / (0.5 * sr)) * (lapTarget - lap);
        lapL.tick(wl);
        lapR.tick(wr);
        l += lapLevel * lap * lap * lapL.band;
        r += lapLevel * lap * lap * lapR.band;
    }
    mainsPhase += mainsStep;
    if (mainsPhase >= 1.0) mainsPhase -= 1.0;
    const float mp = float(mainsPhase);
    if (humLevel > 0.0f) {
        float hum = 0;
        for (std::size_t k = 0; k < humWeights.size(); ++k) {
            if (humWeights[k] <= 0.0f) continue;
            float p = mp * float(k + 1);
            p -= std::floor(p);
            hum += humWeights[k] * sinCycle(p);
        }
        l += humLevel * hum;
        r += humLevel * hum * 0.9f;
    }
    if (buzzLevel > 0.0f) {   // a tired fluorescent tube: mostly steady, sometimes it stutters
        if (--buzzCountdown <= 0) {
            const bool stutter = buzzTarget >= 1.0f && random() < 0.35f;
            buzzTarget = stutter ? 0.15f + 0.3f * random() : 1.0f;
            buzzCountdown = stutter ? int(sr * (0.03 + 0.12 * random())) : int(sr * (1.5 + 6.0 * random()));
        }
        buzz += buzzCoef * (buzzTarget - buzz);
        float b = 0;
        constexpr float weights[] { 0.5f, 0.35f, 0.25f, 0.15f };
        for (int k = 0; k < 4; ++k) {
            float p = mp * float(2 + 4 * k);
            p -= std::floor(p);
            b += weights[k] * sinCycle(p);
        }
        l += buzzLevel * buzz * b * 1.1f;
        r += buzzLevel * buzz * b * 0.8f;
    }
    if (motorCycles) {   // the compressor's own schedule: on for a while, off for a while
        if (--cycleCountdown <= 0) {
            const bool starting = motorGateTarget < 0.5f;
            motorGateTarget = starting ? 1.0f : 0.4f;
            cycleCountdown = int(sr * (starting ? 14.0 + 20.0 * random() : 8.0 + 14.0 * random()));
            Schedule clunk { Kind::clunk, 0, 0, starting ? 1.0f : 0.7f, 0.5f };
            spawn(clunk);
        }
        motorGate += motorGateCoef * (motorGateTarget - motorGate);
    }
    if (motorLevel > 0.0f) {   // a motor or fan: harmonics, beating slowly
        motorPhase += motorHz / float(sr);
        if (motorPhase >= 1.0f) motorPhase -= 1.0f;
        motorAmPhase += motorAmRate / float(sr);
        if (motorAmPhase >= 1.0f) motorAmPhase -= 1.0f;
        float m = 0;
        for (std::size_t k = 0; k < motorWeights.size(); ++k) {
            if (motorWeights[k] <= 0.0f) continue;
            float p = motorPhase * float(k + 1);
            p -= std::floor(p);
            m += motorWeights[k] * sinCycle(p);
        }
        m *= motorLevel * (motorCycles ? motorGate : 1.0f) * (1.0f - motorAmDepth * (0.5f + 0.5f * sinCycle(motorAmPhase)));
        l += m;
        r += m * 0.85f;
    }
    if (subLevel > 0.0f) {   // the structure itself vibrating
        subPhase += subHz / float(sr);
        if (subPhase >= 1.0f) subPhase -= 1.0f;
        if ((breathCountdown & 63) == 0) subWobble = 0.5f + 0.5f * subWander.tick(rng, 0.2f, float(64.0 / sr));
        const float s = subLevel * subWobble * sinCycle(subPhase);
        l += s;
        r += s;
    }
    if (clackLevel > 0.0f) {   // escalator steps passing the comb plate, far away
        if (--clackCountdown <= 0) {
            clackEnv = clackLevel * (0.6f + 0.4f * random());
            clackCountdown = int(sr * clackInterval * (0.92f + 0.16f * random()));
        }
        clackTone.tick(noiseL.bipolar() * clackEnv);
        clackEnv *= clackDecay;
        l += clackTone.band * 0.8f;
        r += clackTone.band;
    }
    if (whineLevel > 0.0f) {   // a coil whining, almost too high to notice
        whinePhase += whineHz * (1.0f + 0.002f * subWobble) / float(sr);
        if (whinePhase >= 1.0f) whinePhase -= 1.0f;
        const float w = whineLevel * sinCycle(whinePhase);
        l += w;
        r += 0.7f * w;
    }
    if (clusterLevel > 0.0f) {   // tones drifting in and out of balance, never resolving
        if ((breathCountdown & 63) == 0)
            for (int k = 0; k < clusterTones; ++k) clusterWeight[std::size_t(k)] = 0.5f + 0.5f * clusterWander[std::size_t(k)].tick(rng, 0.12f, float(64.0 / sr));
        float cl = 0, cr = 0;
        for (int k = 0; k < clusterTones; ++k) {
            auto& ph = clusterPhase[std::size_t(k)];
            ph += clusterHz[std::size_t(k)] * (1.0f + 0.01f * clusterWeight[std::size_t(k)]) / float(sr);
            if (ph >= 1.0f) ph -= 1.0f;
            const float v = clusterWeight[std::size_t(k)] * clusterWeight[std::size_t(k)] * sinCycle(ph) / float(1 + k / 2);
            if (k & 1) cr += v; else cl += v;
        }
        l += clusterLevel * (cl + 0.3f * cr);
        r += clusterLevel * (cr + 0.3f * cl);
    }
    float send = 0.5f * (l + r) * bedSend;
    // Incidental events.
    for (int i = 0; i < scheduleCount; ++i) {
        auto& s = schedules[std::size_t(i)];
        s.countdown -= activity;
        if (s.countdown <= 0.0f) {
            spawn(s);
            // Exponential gaps (a Poisson process) above a minimum: no fixed rhythm.
            s.countdown = float(sr) * (s.minSeconds - s.meanSeconds * std::log(std::max(1.0e-4f, random())));
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
    return { { l * trim, r * trim }, { send * trim, send * trim } };
}

} // namespace liminal::dsp
