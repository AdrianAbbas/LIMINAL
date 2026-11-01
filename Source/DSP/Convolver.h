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
// Everything is allocated in load() / prepareLayout() (message thread); process(),
// setSpectra() and setLengthCap() never allocate or lock.
//
// Shared spectra: an IR's partition spectra (ConvolverSpectra) are computed once, for a fixed
// layout length, and any number of Convolvers prepared with that layout can play them - a room
// chain swaps IRs on the audio thread by pointer. The Quality setting caps the length in real
// time by giving each partition a gain (a raised-cosine fade over the last 20 %; partitions past
// the cap are skipped, so the CPU falls with it) - see maxSecondsFor().
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace liminal::dsp {

// One IR's frequency-domain partitions for a given layout (see Convolver::analyse).
struct ConvolverSpectra {
    int length = 0, layoutLength = 0;
    std::vector<float> head;                  // the first headSize taps, reversed
    std::vector<std::vector<float>> stages;   // per stage: parts x (fftSize + 2)
};

class Convolver {
public:
    static constexpr int headSize = 128;
    static constexpr int growth = 4, maxPartition = 8192;

    // Quality: 0 Eco, 1 Standard, 2 High.
    static double maxSecondsFor(int quality) { return quality <= 0 ? 2.0 : quality == 1 ? 6.0 : 8.0; }

    // Not real-time safe: builds the partitions. `ir` is one channel. Lengths beyond `maxSeconds`
    // are faded out over the last 5 % (no abrupt cut).
    void load(const float* ir, int length, double sampleRate, double maxSeconds);
    // Shared use (not real-time safe): the layout for IRs up to `layoutLength` samples, and an
    // IR's spectra for that layout.
    void prepareLayout(int layoutLength);
    static std::unique_ptr<ConvolverSpectra> analyse(const float* ir, int length, int layoutLength);
    // Real-time safe. `s` must outlive its use and match this layout; nullptr = silence. Resets.
    void setSpectra(const ConvolverSpectra* s);
    // Real-time safe: the audible length (seconds), faded over its last 20 %.
    void setLengthCap(double seconds, double sampleRate);
    // Real-time safe: reshape the decay - partition gains follow exp(-k t) (k in 1/s; > 0 shorter,
    // < 0 longer), a staircase as fine as the partitions (2.7 ms early, 170 ms late).
    void setDecayShape(double kPerSecond, double sampleRate);
    void reset();
    // In-place safe (in == out allowed). Real-time safe.
    void process(const float* in, float* out, int samples);
    int latencySamples() const { return 0; }
    int irLength() const { return spectra == nullptr ? 0 : std::min(spectra->length, capSamples); }
    // Average multiply-adds per sample (complex MACs counted as 4), for the CPU report.
    double opsPerSample() const;

private:
    struct Stage {
        int index = 0, partition = 0, offset = 0, parts = 0, slices = 0, fftSize = 0, fftSlice = 0, ifftSlice = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> gains;     // per partition (the length cap); 0 = skipped
        bool active = true;           // any partition audible (else the whole stage is skipped)
        std::vector<float> fdl;       // parts x (fftSize + 2): the input spectra, newest at `head`
        std::vector<float> frame;     // 2 x fftSize (JUCE's real FFT work space)
        std::vector<float> acc;       // fftSize + 2
        int fdlHead = 0, macDone = 0;
    };
    void runSlice(Stage&, int slice);
    void mac(Stage&, int fromPart, int toPart);

    struct Layout { int partition, offset, parts; };
    static std::vector<Layout> layoutFor(int length);
    void updateGains();
    const ConvolverSpectra* spectra = nullptr;
    std::unique_ptr<ConvolverSpectra> own;   // load()'s own IR
    int layoutLength = 0, capSamples = 1 << 30;
    double decayK = 0.0;   // per sample
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
