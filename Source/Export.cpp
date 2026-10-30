#include "Export.h"
#include "PluginProcessor.h"

namespace liminal::exporter {

bool renderAmbience(const juce::MemoryBlock& state, const Options& o, const juce::File& file, std::function<bool(float)> progress) {
    const double rate = std::clamp(o.sampleRate, 22050.0, 192000.0);
    const int block = 512;
    LiminalProcessor p;
    p.setStateInformation(state.getData(), int(state.getSize()));
    const auto set = [&](const juce::String& id, float value) {
        if (auto* param = p.parameters.getParameter(id)) param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    for (const char* key : params::instrumentKeys) set(juce::String(key) + "_on", 0.0f);   // the place only
    set(params::sceneOn, 0.0f);
    set(params::power, 1.0f);
    if (!o.room) set(params::space, 0.0f);
    p.setRateAndBufferSizeDetails(rate, block);
    p.prepareToPlay(rate, block);
    const auto total = juce::int64(o.seconds * rate);
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    if (static_cast<juce::FileOutputStream*>(stream.get())->failedToOpen()) return false;
    auto writer = juce::WavAudioFormat().createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
    if (writer == nullptr) return false;
    juce::AudioBuffer<float> buffer(2, block);
    juce::MidiBuffer none;
    for (juce::int64 at = 0; at < total; at += block) {
        const int n = int(std::min<juce::int64>(block, total - at));
        buffer.setSize(2, n, false, false, true);
        p.processBlock(buffer, none);
        if (!writer->writeFromAudioSampleBuffer(buffer, 0, n)) return false;
        if (progress && (at / block) % 64 == 0 && !progress(float(double(at) / double(total)))) return false;
    }
    if (progress) progress(1.0f);
    return true;
}

Job::Job(juce::MemoryBlock s, Options o, juce::File f) : juce::Thread("LIMINAL ambience export"), state(std::move(s)), options(o), target(std::move(f)) {
    startThread(juce::Thread::Priority::low);
}

Job::~Job() { stopThread(10000); }

void Job::run() {
    ok = renderAmbience(state, options, target, [this](float x) {
        done = x;
        return !threadShouldExit();
    });
    complete = true;
}

} // namespace liminal::exporter
