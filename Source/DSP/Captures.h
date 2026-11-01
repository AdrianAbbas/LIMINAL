#pragma once
// The developer's effect captures (pedal-capture milestone), as the rooms use them.
//
// Built from assets that tools/captures/prepare.py makes out of the private Samples repository
// (impulse responses + measured profiles). They are embedded only when the build has them
// (LIMINAL_HAS_CAPTURES; CI fetches them with the SAMPLES_TOKEN secret) - never committed here.
// Without them every room sounds exactly as before.
//
// The bank is built in prepareToPlay (it allocates): each IR is resampled to the host rate, given
// the decay a room needs (an exponential envelope: the captured colour and texture, a shorter
// tail), and analysed once into partition spectra that both room chains share.
#include "DSP/CaptureModels.h"
#include "DSP/Convolver.h"
#include <array>
#include <functional>
#include <limits>
#include <memory>

namespace liminal::dsp {

// IR variants the rooms use (see Profile::Capture in DSP/Environment.h).
enum CaptureIr : int {
    captureNone = -1,
    captureDarkHall = 0,        // the dark reverb + delay patch, as captured (RT60 ~6.6 s mid)
    captureDarkHallShort,       // the same, its tail shortened to ~3 s (the garage)
    captureShimmer,             // the shimmer patch's static take, shortened to ~4.5 s (the pool)
    captureChorusVerb,          // the chorus-modulator + reverb patch, as captured (~6.3 s; mono)
    captureIrCount
};

class CaptureBank {
public:
    static constexpr double layoutSeconds = 8.0;

    // Not real-time safe. Returns false (and stays empty) when the build has no captures.
    bool build(double sampleRate);
    bool available() const { return ok; }
    int layoutLength() const { return layout; }
    // Per channel (a mono capture gives the same spectra to both); nullptr if missing.
    const ConvolverSpectra* spectra(int ir, int channel) const;
    float rt60(int ir) const { return ir >= 0 && ir < captureIrCount ? rts[std::size_t(ir)] : 0.0f; }   // mid RT60 after shaping
    const TapeEchoProfile& tape() const { return tapeProfile; }
    const ModProfile& mod() const { return modProfile; }
    float shimmerOctaveDb() const { return octaveDb; }

    // Tests: build the bank from a folder of prepared assets instead of the embedded ones.
    bool buildFrom(double sampleRate, const juce::File& folder);
    // Tests and the before/after render: switch the captures off everywhere (the old sound).
    static bool& disabled() { static bool d = false; return d; }
    // Level matching (tests only): every room's IR at this level instead of its own; NaN = off.
    static float& irLevelOverrideDb() { static float d = std::numeric_limits<float>::quiet_NaN(); return d; }

private:
    bool buildFromData(double sampleRate, const std::function<juce::MemoryBlock(const juce::String&)>& fetch);
    bool ok = false;
    double rate = 0;
    int layout = 0;
    std::array<std::array<std::unique_ptr<ConvolverSpectra>, 2>, captureIrCount> irs;
    std::array<bool, captureIrCount> stereo {};
    std::array<float, captureIrCount> rts {};
    TapeEchoProfile tapeProfile;
    ModProfile modProfile;
    float octaveDb = -60.0f;
};

}  // namespace liminal::dsp
