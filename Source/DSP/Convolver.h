#pragma once
// Real-time convolution for captured impulse responses (pedal-capture milestone).
//
// Non-uniform partitioned convolution with zero added latency:
//   - a direct-form FIR "head" for the first `headSize` taps (128) - sample by sample, so the
//     output starts in the same sample as the input (latencySamples() == 0, tested);
//   - then frequency-domain stages (uniformly partitioned overlap-save inside each stage), each
//     stage's partition 4x the previous one: 128, 512, 2048, 8192. Stage s starts at IR offset
//     2 x its partition size, which gives it one whole partition period of slack: its input FFT,
//     its multiply-adds and its inverse FFT are spread over that period in 128-sample slices, so
//     no audio callback ever pays for a big FFT all at once (no CPU spikes).
// Everything is allocated in load() (message thread); process() never allocates or locks.
// The IR length is capped by the Quality setting (Eco / Standard / High) - see maxSecondsFor().
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace liminal::dsp {

class Convolver {
public:
    static constexpr int headSize = 128;
    static constexpr int growth = 4, maxPartition = 8192;

    // Quality: 0 Eco, 1 Standard, 2 High.
    static double maxSecondsFor(int quality) { return quality <= 0 ? 1.5 : quality == 1 ? 3.5 : 8.0; }

    // Not real-time safe: builds the partitions. `ir` is one channel. Lengths beyond `maxSeconds`
    // are faded out over the last 5 % (no abrupt cut).
    void load(const float* ir, int length, double sampleRate, double maxSeconds);
    void reset();
    // In-place safe (in == out allowed). Real-time safe.
    void process(const float* in, float* out, int samples);
    int latencySamples() const { return 0; }
    int irLength() const { return length; }
    // Average multiply-adds per sample (complex MACs counted as 4), for the CPU report.
    double opsPerSample() const;

private:
    struct Stage {
        int partition = 0, offset = 0, parts = 0, slices = 0, fftSize = 0, fftSlice = 0, ifftSlice = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> filters;   // parts x (fftSize + 2), interleaved complex
        std::vector<float> fdl;       // parts x (fftSize + 2): the input spectra, newest at `head`
        std::vector<float> frame;     // 2 x fftSize (JUCE's real FFT work space)
        std::vector<float> acc;       // fftSize + 2
        int fdlHead = 0, macDone = 0;
    };
    void runSlice(Stage&, int slice);
    void mac(Stage&, int fromPart, int toPart);

    int length = 0;
    std::vector<float> head;          // the first headSize taps, reversed for the dot product
    std::vector<float> headHistory;   // 2 x headSize, mirrored ring (contiguous dot product)
    int headPos = 0;
    std::vector<Stage> stages;
    std::vector<float> inRing, outRing;
    int inMask = 0, outMask = 0;
    long long time = 0;               // samples processed
    int blockFill = 0;
};

// A stereo IR (mono in -> L/R, or L->L and R->R): two Convolvers.
class StereoConvolver {
public:
    void load(const std::vector<std::vector<float>>& channels, double sampleRate, int quality);
    void reset() { for (auto& c : ch) c.reset(); }
    // Mono-summed input feeds both IR channels when `monoIn` (a pedal: mono in, stereo out).
    void process(float* left, float* right, int samples, bool monoIn);
    bool loaded() const { return ok; }
    double opsPerSample() const { return ch[0].opsPerSample() + ch[1].opsPerSample(); }
    int irLength() const { return ch[0].irLength(); }

private:
    Convolver ch[2];
    std::vector<float> scratch;
    bool ok = false;
};

}  // namespace liminal::dsp
