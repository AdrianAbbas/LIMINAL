#include "DSP/Captures.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#if LIMINAL_HAS_CAPTURES
#include "CaptureData.h"
#endif

namespace liminal::dsp {

namespace {

struct Variant {
    int id;
    const char* file;
    float targetRt60;   // 0 = as captured
};
// Which capture each variant comes from, and the decay the rooms need (docs/PEDAL_CAPTURES.md).
constexpr Variant variants[] = {
    { captureDarkHall, "darkhall", 0.0f },
    { captureDarkHallShort, "darkhall", 3.0f },
    { captureShimmer, "shimmer", 4.5f },
    { captureChorusVerb, "chorusverb", 0.0f },
};

juce::AudioBuffer<float> readWav(const juce::MemoryBlock& data, double& fileRate) {
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::MemoryInputStream(data, false), true));
    juce::AudioBuffer<float> b;
    if (r == nullptr) return b;
    fileRate = r->sampleRate;
    b.setSize(int(r->numChannels), int(r->lengthInSamples));
    r->read(&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}

}  // namespace

const ConvolverSpectra* CaptureBank::spectra(int ir, int channel) const {
    if (!ok || ir < 0 || ir >= captureIrCount) return nullptr;
    const auto& pair = irs[std::size_t(ir)];
    return pair[std::size_t(stereo[std::size_t(ir)] ? std::clamp(channel, 0, 1) : 0)].get();
}

bool CaptureBank::build(double sampleRate) {
#if LIMINAL_HAS_CAPTURES
    if (ok && rate == sampleRate) return true;
    return buildFromData(sampleRate, [](const juce::String& name) {
        int size = 0;
        const auto resource = name.replaceCharacter('.', '_');
        if (const char* d = CaptureData::getNamedResource(resource.toRawUTF8(), size)) return juce::MemoryBlock(d, std::size_t(size));
        return juce::MemoryBlock();
    });
#else
    juce::ignoreUnused(sampleRate);
    ok = false;
    return false;
#endif
}

bool CaptureBank::buildFrom(double sampleRate, const juce::File& folder) {
    return buildFromData(sampleRate, [folder](const juce::String& name) {
        juce::MemoryBlock m;
        folder.getChildFile(name).loadFileAsData(m);
        return m;
    });
}

bool CaptureBank::buildFromData(double sampleRate, const std::function<juce::MemoryBlock(const juce::String&)>& fetch) {
    ok = false;
    rate = sampleRate;
    const auto json = fetch("profiles.json");
    if (json.getSize() == 0) return false;
    const auto prof = juce::JSON::parse(json.toString());
    if (!prof.isObject()) return false;

    layout = int(layoutSeconds * sampleRate);
    for (const auto& v : variants) {
        const auto info = prof["irs"][v.file];
        double fileRate = 48000.0;
        const auto buf = readWav(fetch(juce::String(v.file) + ".wav"), fileRate);
        if (buf.getNumSamples() == 0) return false;
        const double ratio = fileRate / sampleRate;   // input samples per output sample
        const int outLen = std::min(layout, int(buf.getNumSamples() / ratio));
        const float rtNow = float(double(info["rt60Mid"]));
        // Decay shaping: amplitude x exp(-6.91 t (1/T_new - 1/T_old)) moves RT60 from T_old to T_new.
        const double k = (v.targetRt60 > 0.0f && rtNow > v.targetRt60) ? 6.907755 * (1.0 / v.targetRt60 - 1.0 / rtNow) : 0.0;
        stereo[std::size_t(v.id)] = buf.getNumChannels() > 1;
        rts[std::size_t(v.id)] = k > 0.0 ? v.targetRt60 : rtNow;
        for (int ch = 0; ch < std::min(2, buf.getNumChannels()); ++ch) {
            std::vector<float> h(std::size_t(outLen), 0.0f);
            if (std::abs(ratio - 1.0) < 1e-9) {
                std::copy(buf.getReadPointer(ch), buf.getReadPointer(ch) + outLen, h.begin());
            } else {
                juce::LagrangeInterpolator interp;
                interp.process(ratio, buf.getReadPointer(ch), h.data(), outLen);
                // Keep the IR's energy per second at the new rate (a resampled impulse response
                // sums ratio x as many samples).
                for (auto& x : h) x *= float(ratio);
            }
            if (k > 0.0)
                for (int i = 0; i < outLen; ++i) h[std::size_t(i)] *= float(std::exp(-k * double(i) / sampleRate));
            irs[std::size_t(v.id)][std::size_t(ch)] = Convolver::analyse(h.data(), outLen, layout);
        }
    }

    const auto t = prof["tape"];
    if (t.isObject()) {
        tapeProfile.echoMs = float(double(t["echoMs"]));
        tapeProfile.feedbackDb = float(double(t["feedbackDb"]));
        tapeProfile.darkeningDb = float(double(t["darkeningDb"]));
        tapeProfile.wowHz = float(double(t["wowHz"]));
        tapeProfile.wowCents = float(double(t["wowCents"]));
        tapeProfile.flutterHz = float(double(t["flutterHz"]));
        tapeProfile.flutterCents = float(double(t["flutterCents"]));
        tapeProfile.drive = float(double(t["drive"]));
    }
    const auto m = prof["mod"];
    if (m.isObject()) {
        modProfile.rateHz = float(double(m["rateHz"]));
        modProfile.depthMs = float(double(m["depthMs"]));
    }
    const auto sh = prof["shimmer"];
    if (sh.isObject()) octaveDb = float(double(sh["octaveDb"]));
    ok = true;
    return true;
}

}  // namespace liminal::dsp
