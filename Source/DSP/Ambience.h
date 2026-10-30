#pragma once
// An environment's own sounds (Milestone 2): a continuous bed (air, hum, machinery, water)
// and incidental events (beeps, rattles, impacts, drips, creaks, passing cars...), all
// procedural. Each environment uses its own generators and event types with its own
// timing, so no two places share a palette, and events are drawn from random intervals
// with random parameters, so nothing repeats in a short loop. The bed is a bed: about
// -40 dBFS, 10-15 dB under the instruments.
//
// `tick` returns the dry sound and, separately, what goes into the room (distant events
// are mostly room). Deterministic for a given seed; fixed storage, no allocation.
#include "DSP/Characters.h"
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include <array>

namespace liminal::dsp {

class Ambience {
public:
    enum class Place { hospital, mall, school, garage, pool, hotel, subway, supermarket, office, impossible };
    enum class Kind { beep, rattle, metal, thud, drip, creak, pipe, groan, car, ticks, plop,
                      elevator, murmur, rail, gust, clunk, seek, chirp, swell, phantom };

    void prepare(double sampleRate);
    void configure(Place, unsigned seed, const Character* = nullptr);   // also resets
    void setActivity(float rate) { activity = std::clamp(rate, 0.1f, 8.0f); }   // events x rate (Activity macro)
    void disturb();   // an evolution disturbance: one of the place's events, now, louder
    int spawned() const { return spawnCount; }
    void setQuality(int q) { eventCap = q == 0 ? 5 : 12; }   // Eco: at most 5 events at once
    void reset();
    struct Out { Stereo dry, send; };
    Out tick();
    float trim = 1.0f;   // overall level (set per environment)

private:
    struct Pink {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float next(float w);
    };
    // One incidental sound while it plays.
    struct Event {
        Kind kind = Kind::beep;
        bool active = false;
        int age = 0, length = 0;
        float amp = 0, pan = 0, send = 0;
        float f = 0, rate = 0, rate2 = 0, phase = 0, phase2 = 0, env = 0, decay = 0, x = 0, y = 0;
        int repeats = 0, gap = 0;
        ModeBank<6> modes;
        Svf bp1, bp2;
        OnePole lp;
        Random noise;
    };
    // When each kind of event happens in this place.
    struct Schedule {
        Kind kind = Kind::beep;
        float meanSeconds = 10, minSeconds = 1, level = 1, send = 0.5f;
        float countdown = 0;
    };
    Event* spawn(const Schedule&, int delay = 0);   // delay: samples before it starts
    Stereo renderEvent(Event&, float& sendOut);
    float random() { return rng.uniform(); }
    float bipolar() { return rng.bipolar(); }

    double sr = 48000.0;
    Place place = Place::hospital;
    unsigned seed = 1;
    Random rng, noiseL, noiseR;

    // The bed's generators; which ones run, and how, depends on the place.
    Pink pinkL, pinkR;
    Svf airL, airR, ductL, ductR, lapL, lapR;
    float airLevel = 0, ductLevel = 0, lapLevel = 0, brownL = 0, brownR = 0, rumbleLevel = 0;
    OnePole rumbleL, rumbleR;
    float breath = 1, breathTarget = 1, breathCoef = 0.0001f;
    int breathCountdown = 0;
    float lap = 0, lapTarget = 0;
    int lapCountdown = 0;
    double mainsPhase = 0, mainsStep = 0;
    std::array<float, 8> humWeights {};
    float humLevel = 0, buzzLevel = 0, buzz = 1, buzzTarget = 1, buzzCoef = 0.01f;
    int buzzCountdown = 0;
    float motorHz = 0, motorLevel = 0, motorPhase = 0, motorAmPhase = 0, motorAmRate = 0, motorAmDepth = 0;
    std::array<float, 4> motorWeights {};
    float subHz = 0, subLevel = 0, subPhase = 0, subWobble = 0;
    Wander subWander;
    int clackCountdown = 0;
    float clackInterval = 0, clackLevel = 0, clackEnv = 0, clackDecay = 0;
    Svf clackTone;
    // Refrigeration: the compressor switches itself on and off (supermarket).
    bool motorCycles = false;
    float motorGate = 1, motorGateTarget = 1, motorGateCoef = 0.0001f;
    int cycleCountdown = 0;
    // A transformer's or monitor's coil whine (office).
    float whineHz = 0, whineLevel = 0, whinePhase = 0;
    // A cluster of inharmonic tones whose balance never settles (impossible room).
    static constexpr int clusterTones = 6;
    float clusterLevel = 0;
    std::array<float, clusterTones> clusterHz {}, clusterPhase {}, clusterWeight {};
    std::array<Wander, clusterTones> clusterWander {};
    float bedSend = 0.3f;

    float activity = 1;
    int eventCap = 12, spawnCount = 0;
    std::array<Schedule, 6> schedules {};
    int scheduleCount = 0;
    std::array<Event, 12> events {};
};

} // namespace liminal::dsp
