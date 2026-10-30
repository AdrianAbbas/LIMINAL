#pragma once
// The Milestone 0 room: an eight-line feedback delay network (Householder mixing,
// damped feedback, a short pre-delay) tuned as a long, hard corridor (RT60 about 2.4 s).
// Placeholder for Milestone 2/3's environment-specific spaces. Buffers are allocated in
// prepare(), never while rendering.
#include "DSP/Stereo.h"
#include <array>
#include <vector>

namespace liminal::dsp {

class Space {
public:
    void prepare(double sampleRate);
    void reset();
    Stereo tick(Stereo in);   // returns the wet signal only

private:
    static constexpr int lines = 8;
    struct Line {
        std::vector<float> data;
        int write = 0, length = 1;
        float gain = 0.5f, lp = 0;
    };
    std::array<Line, lines> delays;
    std::vector<float> pre;
    int preWrite = 0, preLength = 1;
    float damping = 0.5f;
};

} // namespace liminal::dsp
