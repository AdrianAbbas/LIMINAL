#pragma once
// An environment's room (Milestone 2): early reflections from the room's own geometry,
// input diffusion, then an eight-line feedback delay network (Householder mixing) with a
// two-band decay (separate RT60 below and above a crossover), slowly modulated line
// lengths and an output band limit. Everything that differs between environments is in
// `Space::Shape`; buffers are sized for the largest shape in prepare(), so configure()
// never allocates and can run on the audio thread.
#include "DSP/Filters.h"
#include "DSP/Stereo.h"
#include <array>
#include <vector>

namespace liminal::dsp {

class Space {
public:
    // How the first reflections are laid out.
    enum class Pattern { flutter, dense, sparse, tunnel, grid };
    struct Shape {
        float rt60Low = 2.4f, rt60High = 1.6f, crossoverHz = 1500.0f;
        float size = 1.0f;            // scales the delay lines (0.5 .. 2.5): bigger = sparser, slower build-up
        float preDelayMs = 18.0f;
        float diffusion = 0.5f;       // 0..1: how smeared the onset is
        float modulation = 0.2f;      // 0..1: slow line-length wobble (smoother, lusher tails)
        Pattern pattern = Pattern::sparse;
        int reflections = 8;          // up to 16
        float firstMs = 10.0f, spacingMs = 10.0f, reflectionDecay = 0.8f, reflectionLevel = 0.5f;
        float width = 1.0f;           // tail stereo width
        float lowCutHz = 40.0f, highCutHz = 12000.0f;
        float lateLevel = 1.0f;       // the tail's level relative to the reflections
    };
    static constexpr float maxSize = 2.5f, maxPreDelayMs = 150.0f, maxReflectionMs = 400.0f;

    void prepare(double sampleRate);
    void configure(const Shape&, unsigned seed);   // then reset() for a clean start
    void reset();
    Stereo tick(Stereo in);   // wet only: reflections + tail
    void setDecayScale(float scale);   // the Scale macro: RT60 x scale, without touching the line lengths
    void setQuality(int q) { lean = q == 0; }   // Eco: no line modulation (glided out), at most 8 reflections

private:
    static constexpr int lines = 8, diffusers = 4, maxTaps = 16;
    struct Line {
        std::vector<float> data;
        int write = 0, length = 1;
        float gLow = 0.5f, gHigh = 0.5f, lp = 0, phase = 0, rate = 0.3f;
    };
    struct Allpass {
        std::vector<float> data;
        int write = 0, length = 1;
    };
    double sr = 48000.0;
    Shape shape;
    std::array<Line, lines> delays;
    std::array<Allpass, diffusers> diffuse;
    float diffuseCoef = 0.5f;
    std::vector<float> early;   // input history for pre-delay and reflections (mono)
    int earlyWrite = 0, preDelay = 1;
    std::array<int, maxTaps> tapDelay {};
    std::array<float, maxTaps> tapL {}, tapR {};
    int taps = 0;
    float crossover = 0.1f, modSamples = 0;
    OnePole lowCutL, lowCutR, highCutL, highCutR;
    float lowCutHz = 40.0f, highCutHz = 12000.0f;
    float decayScale = 1.0f, modNow = 0;
    bool lean = false;
    void updateDecay();
};

} // namespace liminal::dsp
