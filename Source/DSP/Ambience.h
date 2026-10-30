#pragma once
// An environment's own sounds (Direction Update 2): a liminal space is *still*. The ambience is
//   - a steady tonal bed: 60 Hz mains hum with harmonics and slow beating, a fluorescent
//     ballast buzz around 120 Hz with a gritty upper edge, motors, pumps, fans, a clock;
//   - a very quiet room tone: heavily filtered air/HVAC, well below the hum, never breathing;
//   - rare, tiny, distant events: a drip, a door, a ballast tick, a lift chime two floors down.
// No risers, whooshes, swells, noise sweeps or "waves" of broadband noise anywhere.
//
// Events come from one scheduler per place: at the default Activity at most one every 20 s
// (a minimum gap), on average one every 40-90 s; even at Activity 100 % they stay sparse.
// A fluorescent stutter is an event too: the buzz drops out for a few flicks and `lamp()`
// follows it, so the picture's failing tube can flicker with the sound.
//
// `tick` returns the dry sound and, separately, what goes into the room (distant events are
// mostly room). Deterministic for a given seed; fixed storage, no allocation.
#include "DSP/Characters.h"
#include "DSP/Filters.h"
#include "DSP/Instrument.h"
#include <array>

namespace liminal::dsp {

class Ambience {
public:
    enum class Place { hospital, mall, school, garage, pool, hotel, subway, supermarket, office, impossible };
    // The order is Character::extraKind's numbering (DSP/Characters.cpp): append only.
    enum class Kind { monitor, ballastTick, doorClick, paChime, shutter, radiatorPing, locker, bell, carDoor, barrier,
                      drip, ripple, liftChime, doorClose, trainRumble, railTick, freezerSeal, cart, flicker, ghostNote, wrongWall };

    void prepare(double sampleRate);
    void configure(Place, unsigned seed, const Character* = nullptr);   // also resets
    void setActivity(float rate) { activity = std::clamp(rate, 0.0f, 6.0f); }   // events x rate (Activity macro); 0 = none
    void disturb();   // an evolution disturbance: the next event now, if the last one was long enough ago
    int spawned() const { return spawnCount; }
    void setQuality(int q) { eventCap = q == 0 ? 3 : 6; }   // Eco: at most 3 events at once
    void reset();
    struct Out { Stereo dry, send; };
    Out tick();
    float trim = 1.0f;   // overall level (set per environment)
    // The failing tube: 1 = steady, lower while it stutters (the buzz follows it exactly).
    float lamp() const { return lampNow; }
    int flickers() const { return flickerCount; }

private:
    struct Pink {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float next(float w);
    };
    // A steady machine tone: a fundamental and three harmonics, a slow mechanical
    // periodicity, and how it comes and goes.
    struct Drone {
        enum class Gate { always, cycles, sometimes };
        float hz = 0, level = 0;
        std::array<float, 4> weights { 1, 0, 0, 0 };
        float amHz = 0, amDepth = 0;
        Gate gate = Gate::always;
        float onSeconds = 0, offSeconds = 0, fadeSeconds = 2;   // cycles / sometimes
        // state
        float phase = 0, amPhase = 0, g = 1, gTarget = 1, gCoef = 0;
        int countdown = 0;
    };
    // One incidental sound while it plays.
    struct Event {
        Kind kind = Kind::monitor;
        bool active = false;
        int age = 0, length = 0;
        float amp = 0, pan = 0, send = 0, dry = 1;
        float f = 0, rate = 0, phase = 0, phase2 = 0, env = 0, decay = 0, x = 0;
        int repeats = 0, gap = 0, step = 0;
        ModeBank<6> modes;
        Svf bp1;
        OnePole lp, lp2;
        Random noise;
    };
    struct Schedule { Kind kind = Kind::monitor; float weight = 1, level = 1; };
    // delay: samples before it starts; pitch: a fixed frequency for tone events (else chosen)
    Event* spawn(Kind, float level, int delay = 0, float pitch = 0);
    Stereo renderEvent(Event&, float& sendOut);
    void startStutter(float depth);
    void scheduleNext();
    float random() { return rng.uniform(); }
    float bipolar() { return rng.bipolar(); }

    double sr = 48000.0;
    Place place = Place::hospital;
    unsigned seed = 1;
    Random rng, noiseL, noiseR;

    // Room tone: filtered pink noise, constant.
    Pink pinkL, pinkR;
    OnePole airL1, airL2, airR1, airR2;
    float airLevel = 0;
    // Mains hum: harmonics of 60 Hz, each beating slowly against a near twin.
    static constexpr int humPartials = 6;
    double mainsPhase = 0, mainsStep = 0;
    std::array<float, humPartials> humWeights {}, beatPhase {}, beatHz {};
    float humLevel = 0, beatDepth = 0.15f;
    // The ballast: 120 Hz and harmonics (a full-wave shape) and the arc's grit at each restrike.
    float buzzLevel = 0, gritLevel = 0, buzzFlutter = 0, restrike = 0;
    Svf gritL, gritR;
    Wander buzzWander;
    // The failing tube's stutter, shared with the picture.
    float lampNow = 1, lampTarget = 1, lampCoef = 0.01f, stutterDepth = 0;
    int stutterFlicks = 0, stutterCountdown = 0, flickerCount = 0;
    std::array<Drone, 3> drones {};
    // Small steady things: a wall clock, a fountain trickling far away, a coil's whine.
    float clockLevel = 0, clockEnv = 0;
    int clockCountdown = 0, clockBeat = 0;
    Svf clockTone;
    float trickleLevel = 0, trickleEnv = 0;
    Svf trickleA, trickleB;
    float whineHz = 0, whineLevel = 0, whinePhase = 0;
    // The impossible room: a hum that drifts in pitch, and moments of total silence.
    bool humDrifts = false, dropouts = false;
    Wander humWander;
    float humDrift = 0, silenceGain = 1, silenceTarget = 1, silenceFall = 0.01f, silenceRise = 0.001f;
    int silenceCountdown = 0, controlClock = 0;
    float bedSend = 0.3f;

    // The scheduler: one queue per place, a minimum gap, exponential waits above it.
    float activity = 1, characterRate = 1;
    float minGapSeconds = 20, meanGapSeconds = 45;
    double countdown = 0, sinceLast = 1e9;
    int eventCap = 6, spawnCount = 0;
    std::array<Schedule, 6> schedules {};
    int scheduleCount = 0;
    float weightSum = 0;
    std::array<Event, 6> events {};
};

} // namespace liminal::dsp
