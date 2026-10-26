#pragma once
#include "Engine.h"
namespace liminal
{
// Normalised family controls have model-specific mappings, documented in INSTRUMENTS.md.
class Instruments
{
  public:
    void prepare(double rate, const Settings &);
    void setSettings(const Settings &) noexcept;
    void noteOn(int channel, int note, float velocity) noexcept;
    void noteOff(int channel, int note) noexcept;
    void sustain(int channel, bool down) noexcept;
    void bend(int channel, int value) noexcept;
    void allNotesOff(int channel, bool immediate = false) noexcept;
    Stereo tick() noexcept;
    int activeVoices() const noexcept;

  private:
    struct Voice
    {
        int note = -1, channel = 0;
        bool held = false, sustained = false, stealing = false;
        float velocity = 0, envelope = 0, filter = 0, last = 0;
        double age = 0, frequency = 0;
        std::array<double, 12> phase{}, frequencies{};
        std::array<float, 12> amplitudes{}, amplitudeTargets{};
        unsigned control = 0;
        int partials = 0;
        float envelopeCoefficient = 0, filterCoefficient = 1, noiseLevel = 0, drive = 0, gain = 1;
    };
    struct Family
    {
        std::array<Voice, 12> voices;
        std::array<Voice, 12> tails;
        float on = 0, level = 0;
        std::array<float, 6> controls{};
    };
    std::array<Family, 6> families{};
    std::array<bool, 16> pedal{};
    std::array<float, 16> bendTarget{}, bendCurrent{};
    Settings target = defaults();
    double sr = 48000;
    float smoothing = .001f;
    std::uint32_t rng = 12345;
    float noise() noexcept;
    float render(Voice &, int, const std::array<float, 6> &) noexcept;
};
} // namespace liminal
