#pragma once
#include "Parameters.h"
#include "AudioTypes.h"
#include "environments/Locations.h"
#include <array>
#include <cstdint>
#include <vector>
namespace liminal
{
class Engine
{
  public:
    void prepare(double sampleRate, const Settings &initial = defaults());
    void reset(std::uint32_t randomSeed);
    void setSettings(const Settings &);
    void setTransport(bool playing, double bpm) noexcept;
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    void allNotesOff(bool immediately = false) noexcept;
    Stereo tick(Stereo instrument = {}) noexcept;
    std::uint64_t locationEvents() const { return locations.events(); }
    std::uint64_t structureEvents() const { return structureCount; }
    std::uint64_t distantEvents() const { return distantCount; }
    double reflectionSeconds() const { return erTimes[0] / sr; }
    double decaySeconds() const { return rt60; }

  private:
    struct Voice
    {
        int note = -1;
        float env = 0, velocity = 0;
        double phase = 0;
        bool held = false;
        std::uint64_t age = 0;
    };
    struct Mode
    {
        double phase = 0, freq = 100;
        float amplitude = 0;
    };
    struct Delay
    {
        std::vector<float> data;
        std::size_t write = 0;
        float read(double delay) const noexcept;
        void push(float) noexcept;
        void clear();
    };
    Locations locations;
    double sr = 48000, bpm = 120, rt60 = 2;
    bool playing = true;
    Settings target = defaults(), p = defaults();
    std::array<Voice, 8> voices{};
    std::array<Mode, 6> modes{};
    std::array<Delay, 8> late;
    Delay earlyL, earlyR;
    std::array<double, 6> erTimes{};
    std::array<double, 8> lateTimes{};
    std::array<float, 8> feedback{}, damped{};
    std::array<float, 6> colour{};
    std::array<float, 2> outLP{}, hpPrevIn{}, hpPrevOut{}, distanceLP{};
    std::array<double, 8> phases{};
    std::uint32_t rng = 1, lastSeed = 1;
    std::uint64_t sampleCounter = 0, voiceAge = 0, structureCount = 0, distantCount = 0;
    double evolutionClock = 0, structureWait = 0, distantWait = 0, flickerWait = 0;
    float evolution = 0, evolutionTarget = 0, flickerGain = 1, flickerTarget = 1, distantEnv = 0;
    double distantPhase = 0, distantFrequency = 80;
    float transportGain = 1;
    float smooth = 0, lpAir = .1f, lpDark = .1f, lpMuffle = .1f, lpOutput = .5f, hpOutput = .99f,
          lpDistance = .1f, damping = .1f;
    float random() noexcept;
    float noise() noexcept { return random() * 2 - 1; }
    double osc(int index, double hz) noexcept;
    void coefficients() noexcept;
    Stereo room(Stereo) noexcept;
};
} // namespace liminal
