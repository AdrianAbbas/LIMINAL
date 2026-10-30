#pragma once
// AMBIENCE EXPORT (Milestone 6): renders the place's own sounds to a WAV file, offline, with
// the same engine as live playback (a second processor restored from the live state, the
// instruments switched off). Runs on a worker thread, never the audio callback.
// Deterministic: the same state, length and rate give the same file, sample for sample
// (the place starts from its seeds, as a freshly loaded song does).
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

namespace liminal::exporter {

struct Options {
    double seconds = 60.0;
    double sampleRate = 48000.0;
    bool room = true;   // false: the dry stem (Space at 0: the bed and events without the room)
};

// `progress` is called now and then with 0..1; returning false cancels.
bool renderAmbience(const juce::MemoryBlock& state, const Options&, const juce::File&, std::function<bool(float)> progress = {});

// Renders on its own thread; the editor polls `progress()` and `finished()`.
class Job final : private juce::Thread {
public:
    Job(juce::MemoryBlock state, Options, juce::File);
    ~Job() override;
    float progress() const { return done.load(); }
    bool finished() const { return complete.load(); }
    bool succeeded() const { return ok.load(); }
    const juce::File& file() const { return target; }

private:
    void run() override;
    juce::MemoryBlock state;
    Options options;
    juce::File target;
    std::atomic<float> done { 0 };
    std::atomic<bool> complete { false }, ok { false };
};

} // namespace liminal::exporter
