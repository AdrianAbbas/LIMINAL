#pragma once
#include "Profiles.h"
#include "dsp/AudioTypes.h"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <numeric>
namespace liminal
{
class Locations
{
  public:
    void prepare(double rate, const Settings &);
    void reset(unsigned seed);
    Stereo ambience(const Settings &) noexcept;
    Stereo music(Stereo, const Settings &) noexcept;
    float amount() const noexcept
    {
        return std::clamp(std::accumulate(weights.begin(), weights.end(), 0.f), 0.f, 1.f);
    }
    std::uint64_t events() const noexcept { return eventCount; }

  private:
    struct State
    {
        unsigned rng = 1;
        double phase[6]{}, age = 0, wait = 1, cycleWait = 20;
        float low[3]{}, musicalLow[2]{}, event = 0, eventSmooth = 0, eventLoss = .999f, eventHz = 500,
                                         pan = 0, cycle = 1, cycleTarget = 1, airCoefficient = .02f,
                                         musicCoefficient = .1f;
    };
    std::array<State, 10> states{};
    std::array<float, 10> weights{};
    std::vector<Stereo> delay;
    std::size_t position = 0;
    double sr = 48000, clock = 0;
    float smooth = .001f;
    std::uint64_t samples = 0, eventCount = 0;
    float random(State &) noexcept;
    Stereo read(double samples) const noexcept;
};
} // namespace liminal
