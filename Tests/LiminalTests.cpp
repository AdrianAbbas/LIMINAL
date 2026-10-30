// Offline tests through the real processBlock. Exit code 0 only if every check passes.
//   LiminalTests                       run everything
//   LiminalTests --only a,b            run only some groups (smoke, midi, levels, ...)
//   LiminalTests --snapshot f.png      also write an image of the editor (2x scale)
//   LiminalTests --render f.wav        render each instrument in the corridor, then dry (listening aid)
//   LiminalTests --render-places f.wav the same phrase in each environment (listening aid)
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <new>
#include <string>
#include <vector>

// Count C++ allocations on the calling thread while enabled (from WORLDBUILDER's tests).
namespace allocationProbe {
thread_local bool enabled = false;
thread_local std::size_t calls = 0;
void mark() noexcept {
    if (enabled) ++calls;
}
}
void* operator new(std::size_t n) {
    allocationProbe::mark();
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace liminal;

namespace {

struct Suite {
    int passed = 0, failed = 0;
    bool check(bool okay, const std::string& name, const std::string& detail = {}) {
        okay ? ++passed : ++failed;
        std::cout << (okay ? "PASS " : "FAIL ") << name << (detail.empty() ? "" : " :: " + detail) << "\n";
        return okay;
    }
    void section(const char* title) { std::cout << "[test] " << title << std::endl; }
    void metric(const std::string& name, double value, const char* unit = "") { std::cout << "  [metric] " << name << " = " << value << unit << "\n"; }
};

double toDb(double x) { return 20.0 * std::log10(std::max(x, 1.0e-12)); }
std::string db(double x) { return juce::String(toDb(x), 1).toStdString() + " dB"; }

struct Event { double at; juce::MidiMessage message; };

// A processor driven like a host: fixed blocks, MIDI at sample positions.
struct Rig {
    std::unique_ptr<LiminalProcessor> p = std::make_unique<LiminalProcessor>();
    double rate;
    int block;
    double time = 0;
    explicit Rig(double sampleRate = 48000.0, int blockSize = 512, bool mono = false) : rate(sampleRate), block(blockSize) {
        if (mono) p->setBusesLayout({ {}, { juce::AudioChannelSet::mono() } });
        p->setRateAndBufferSizeDetails(rate, block);
        p->prepareToPlay(rate, block);
    }
    void set(const char* id, float value) {
        auto* param = p->parameters.getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
    }
    // Renders `seconds`; events are at absolute times (seconds since the rig started).
    // Returns left and right (right == left for mono).
    std::pair<std::vector<float>, std::vector<float>> render(double seconds, std::vector<Event> events = {}, int blockOverride = 0) {
        const int channels = p->getTotalNumOutputChannels();
        const int size = blockOverride > 0 ? blockOverride : block;
        const auto total = std::size_t(seconds * rate);
        std::vector<float> l, r;
        l.reserve(total);
        r.reserve(total);
        juce::AudioBuffer<float> buffer(channels, size);
        juce::MidiBuffer midi;
        std::size_t done = 0;
        while (done < total) {
            const int n = int(std::min<std::size_t>(std::size_t(size), total - done));
            buffer.setSize(channels, n, false, false, true);
            midi.clear();
            const double start = time, end = time + double(n) / rate;
            for (const auto& e : events)
                if (e.at >= start && e.at < end) midi.addEvent(e.message, std::clamp(int((e.at - start) * rate), 0, n - 1));
            p->processBlock(buffer, midi);
            for (int i = 0; i < n; ++i) {
                l.push_back(buffer.getSample(0, i));
                r.push_back(buffer.getSample(channels > 1 ? 1 : 0, i));
            }
            done += std::size_t(n);
            time = end;
        }
        return { l, r };
    }
};

double rms(const std::vector<float>& l, const std::vector<float>& r, std::size_t from = 0, std::size_t to = 0) {
    to = to == 0 ? l.size() : std::min(to, l.size());
    double sum = 0;
    for (std::size_t i = from; i < to; ++i) sum += 0.5 * (double(l[i]) * l[i] + double(r[i]) * r[i]);
    return to > from ? std::sqrt(sum / double(to - from)) : 0.0;
}
double rms(const std::pair<std::vector<float>, std::vector<float>>& s) { return rms(s.first, s.second); }
bool finite(const std::pair<std::vector<float>, std::vector<float>>& s) {
    for (std::size_t i = 0; i < s.first.size(); ++i)
        if (!std::isfinite(s.first[i]) || !std::isfinite(s.second[i]) || std::abs(s.first[i]) > 1.0f || std::abs(s.second[i]) > 1.0f) return false;
    return true;
}
float peak(const std::pair<std::vector<float>, std::vector<float>>& s) {
    float m = 0;
    for (std::size_t i = 0; i < s.first.size(); ++i) m = std::max({ m, std::abs(s.first[i]), std::abs(s.second[i]) });
    return m;
}

juce::MidiMessage on(int note, int velocity = 100, int channel = 1) { return juce::MidiMessage::noteOn(channel, note, juce::uint8(velocity)); }
juce::MidiMessage off(int note, int channel = 1) { return juce::MidiMessage::noteOff(channel, note); }
juce::MidiMessage pedal(bool down) { return juce::MidiMessage::controllerEvent(1, 64, down ? 127 : 0); }

// The instrument alone: ambience off, room off, no environment processing.
void dry(Rig& rig) {
    rig.set(params::ambience, params::minDb);
    rig.set(params::space, 0.0f);
    rig.set(params::environmentAmount, 0.0f);
    rig.render(0.2);
}

// Fundamental frequency by normalised autocorrelation (lags for 150-2000 Hz), refined
// by parabolic interpolation.
double frequency(const std::vector<float>& x, double rate) {
    const int n = int(std::min<std::size_t>(x.size(), 8192)) / 2;
    const int minLag = int(rate / 2000.0), maxLag = std::min(int(rate / 150.0), n - 1);
    std::vector<double> score(std::size_t(maxLag + 2), 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag) {
        double num = 0, a = 0, b = 0;
        for (int i = 0; i < n; ++i) {
            num += double(x[std::size_t(i)]) * x[std::size_t(i + lag)];
            a += double(x[std::size_t(i)]) * x[std::size_t(i)];
            b += double(x[std::size_t(i + lag)]) * x[std::size_t(i + lag)];
        }
        score[std::size_t(lag)] = num / std::sqrt(std::max(1.0e-20, a * b));
    }
    // The first peak above 0.9 of the best one (avoids octave-down errors).
    double best = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) best = std::max(best, score[std::size_t(lag)]);
    for (int lag = minLag + 1; lag <= maxLag; ++lag) {
        const double y0 = score[std::size_t(lag - 1)], y1 = score[std::size_t(lag)], y2 = score[std::size_t(lag + 1)];
        if (y1 >= 0.9 * best && y1 >= y0 && y1 >= y2) {
            const double curve = y0 - 2 * y1 + y2;
            const double shift = std::abs(curve) < 1.0e-12 ? 0.0 : 0.5 * (y0 - y2) / curve;
            return rate / (double(lag) + shift);
        }
    }
    return 0.0;
}

#include "InstrumentTests.h"
#include "EnvironmentTests.h"

//==============================================================================
void testParameters(Suite& s) {
    s.section("parameters");
    LiminalProcessor p;
    const auto& list = p.getParameters();
    s.check(list.size() >= 5, "the skeleton parameters and the instruments'", std::to_string(list.size()));
    const char* names[] { "Power", "Master Level", "Instrument Level", "Ambience Level", "Space" };
    const char* ids[] { params::power, params::master, params::instrument, params::ambience, params::space };
    for (int i = 0; i < 5; ++i) {
        auto* param = p.parameters.getParameter(ids[i]);
        s.check(param != nullptr && param->getName(64) == names[i] && param->isAutomatable(), std::string("parameter ") + names[i]);
    }
    s.check(p.parameters.getParameter(params::master)->getText(0.0f, 16) == "-inf", "Master Level minimum reads -inf");
}

void testSmoke(Suite& s) {
    s.section("smoke");
    Rig rig;
    const auto bed = rig.render(3.0);
    const double bedRms = rms(bed.first, bed.second, std::size_t(rig.rate));
    s.check(finite(bed), "default output finite and within +/-1");
    s.check(toDb(bedRms) > -55.0 && toDb(bedRms) < -32.0, "ambience plays without MIDI, as a quiet bed (-55..-32 dBFS)", db(bedRms));
    {   // The AU harness gates on > -45 dBFS in 1-1.5 s windows: keep every 1 s window clear of it.
        Rig quiet;
        const auto minute = quiet.render(40.0);
        double lowest = 1;
        for (std::size_t at = std::size_t(quiet.rate); at + std::size_t(quiet.rate) <= minute.first.size(); at += std::size_t(quiet.rate))
            lowest = std::min(lowest, rms(minute.first, minute.second, at, at + std::size_t(quiet.rate)));
        s.check(toDb(lowest) > -44.0, "every 1 s of the default ambience stays above -44 dBFS (harness gate -45)", db(lowest));
    }
    const auto note = rig.render(1.0, { { rig.time + 0.01, on(60, 100) }, { rig.time + 0.9, off(60) } });
    s.check(rms(note) > bedRms * 4.0, "a MIDI note is clearly audible over the ambience", db(rms(note)) + " vs " + db(bedRms));
    s.check(finite(note), "note output finite");
    rig.render(3.0);
    const auto after = rig.render(1.0);
    s.check(rig.p->meters.voices.load() == 0, "the note has finished 3 s after note-off");
    s.check(rms(after) < bedRms * 1.6, "back to the ambience bed after the release", db(rms(after)));

    // State round trip into a fresh instance.
    rig.set(params::master, -7.5f);
    rig.set(params::space, 0.8f);
    rig.set(params::power, 0.0f);
    juce::MemoryBlock state;
    rig.p->getStateInformation(state);
    LiminalProcessor fresh;
    fresh.setStateInformation(state.getData(), int(state.getSize()));
    bool same = true;
    for (auto* param : rig.p->getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            same &= std::abs(ranged->getValue() - fresh.parameters.getParameter(ranged->getParameterID())->getValue()) < 1.0e-5f;
    s.check(same, "state restores every parameter in a fresh instance");
    const char junk[] = "not a LIMINAL state";
    fresh.setStateInformation(junk, int(sizeof(junk)));
    s.check(std::abs(fresh.parameters.getParameter(params::master)->getValue() - rig.p->parameters.getParameter(params::master)->getValue()) < 1.0e-5f,
            "a foreign state is ignored");
}

void testMidi(Suite& s) {
    s.section("midi");
    {
        Rig rig;
        dry(rig);
        const double t = rig.time;
        const auto single = rig.render(1.0, { { t + 0.01, on(60, 90) } });
        rig.render(0.01, { { rig.time, off(60) } });
        rig.render(3.0);
        const double t2 = rig.time;
        const auto chord = rig.render(1.0, { { t2 + 0.01, on(48, 90) }, { t2 + 0.01, on(55, 90) }, { t2 + 0.01, on(60, 90) }, { t2 + 0.01, on(64, 90) } });
        s.check(rig.p->meters.voices.load() == 4, "a four-note chord sounds four voices", std::to_string(rig.p->meters.voices.load()));
        s.check(rms(chord) > rms(single) * 1.4, "the chord is louder than one note", db(rms(chord)) + " vs " + db(rms(single)));
        s.check(peak(chord) < 0.95f && finite(chord), "chord does not clip", juce::String(peak(chord)).toStdString());
        rig.render(0.5, { { rig.time, off(48) }, { rig.time, off(55) }, { rig.time, off(60) }, { rig.time, off(64) } });
        rig.render(1.5);
        s.check(rig.p->meters.voices.load() == 0, "note-off releases every chord note (no stuck notes)");
    }
    {   // Velocity.
        Rig rig;
        dry(rig);
        const auto soft = rig.render(0.6, { { rig.time + 0.01, on(60, 30) } });
        rig.render(0.01, { { rig.time, off(60) } });
        rig.render(2.0);
        const auto loud = rig.render(0.6, { { rig.time + 0.01, on(60, 120) } });
        s.check(toDb(rms(loud)) - toDb(rms(soft)) > 10.0, "velocity 120 is over 10 dB louder than velocity 30",
                db(rms(loud)) + " vs " + db(rms(soft)));
    }
    {   // Sustain pedal.
        Rig rig;
        dry(rig);
        rig.render(0.5, { { rig.time + 0.01, on(60) }, { rig.time + 0.05, pedal(true) }, { rig.time + 0.2, off(60) } });
        const auto held = rig.render(0.5);
        s.check(rig.p->meters.voices.load() == 1 && toDb(rms(held)) > -45.0, "the sustain pedal holds a released note", db(rms(held)));
        rig.render(0.01, { { rig.time, pedal(false) } });
        rig.render(1.5);
        s.check(rig.p->meters.voices.load() == 0, "lifting the pedal releases it");
    }
    {   // Pitch bend: +2 semitones at full bend. A high note, so the fundamental dominates the crossings.
        Rig rig;
        dry(rig);
        const double t = rig.time;
        const auto straight = rig.render(0.5, { { t + 0.001, on(81, 100) } });
        const double f0 = frequency(std::vector<float>(straight.first.begin() + 2400, straight.first.end()), rig.rate);
        rig.render(0.01, { { rig.time, juce::MidiMessage::pitchWheel(1, 16383) } });
        const auto bent = rig.render(0.4);
        const double f1 = frequency(bent.first, rig.rate);
        s.check(std::abs(f0 / 880.0 - 1.0) < 0.01, "A5 sounds at 880 Hz (within 1 %)", juce::String(f0, 2).toStdString() + " Hz");
        s.check(std::abs(f1 / f0 - std::pow(2.0, 2.0 / 12.0)) < 0.01, "full pitch bend raises it 2 semitones", juce::String(f1, 2).toStdString() + " Hz");
        rig.render(0.01, { { rig.time, juce::MidiMessage::pitchWheel(1, 8192) } });
        const double f2 = frequency(rig.render(0.4).first, rig.rate);
        s.check(std::abs(f2 / f0 - 1.0) < 0.01, "centring the wheel returns to pitch (no glide by default)", juce::String(f2, 2).toStdString() + " Hz");
    }
    {   // Voice stealing and a storm of notes: no clicks, no stuck notes.
        Rig rig;
        dry(rig);
        std::vector<Event> events;
        juce::Random random(42);
        for (int i = 0; i < 300; ++i) {
            const int note = 36 + random.nextInt(48);
            const double at = rig.time + 0.005 * i;
            events.push_back({ at, on(note, 40 + random.nextInt(80)) });
            events.push_back({ at + 0.05 + 0.4 * random.nextDouble(), off(note) });
        }
        const auto storm = rig.render(2.5, events);
        float jump = 0;
        for (std::size_t i = 1; i < storm.first.size(); ++i) jump = std::max(jump, std::abs(storm.first[i] - storm.first[i - 1]));
        s.metric("largest sample-to-sample step in the storm", jump);
        s.check(finite(storm) && peak(storm) < 1.0f, "300 overlapping notes stay finite and unclipped", juce::String(peak(storm)).toStdString());
        s.check(jump < 0.5f * peak(storm), "no clicks when voices are stolen (largest step under half the peak)",
                juce::String(jump).toStdString() + " vs peak " + juce::String(peak(storm)).toStdString());
        rig.render(2.0);
        s.check(rig.p->meters.voices.load() == 0, "every storm note has ended 2 s after the last note-off");
    }
    {   // All Notes Off / All Sound Off.
        Rig rig;
        dry(rig);
        rig.render(0.3, { { rig.time, pedal(true) }, { rig.time, on(40) }, { rig.time, on(52) } });
        rig.render(0.01, { { rig.time, juce::MidiMessage::allNotesOff(1) } });
        rig.render(1.5);
        s.check(rig.p->meters.voices.load() == 0, "All Notes Off releases held and sustained notes");
        rig.render(0.3, { { rig.time, on(40) }, { rig.time, on(52) } });
        rig.render(0.01, { { rig.time, juce::MidiMessage::allSoundOff(1) } });
        const auto quiet = rig.render(0.1);
        s.check(rig.p->meters.voices.load() == 0 && rms(quiet) < 1.0e-4, "All Sound Off silences at once", db(rms(quiet)));
    }
}

void testLevels(Suite& s) {
    s.section("levels");
    // The brief: at default the instrument sits about 10-15 dB over the ambience.
    {
        Rig rig;
        rig.render(2.0);
        const auto bed = rig.render(4.0);
        Rig played;
        played.set(params::ambience, params::minDb);
        played.render(2.0);
        std::vector<Event> phrase;
        const int notes[] { 64, 67, 71, 72, 71, 67 };
        for (int i = 0; i < 6; ++i) {
            phrase.push_back({ played.time + 0.6 * i + 0.01, on(notes[i], 80) });
            phrase.push_back({ played.time + 0.6 * i + 0.55, off(notes[i]) });
        }
        const auto melody = played.render(4.0, phrase);
        const double gap = toDb(rms(melody)) - toDb(rms(bed));
        s.metric("mezzo-forte melody over ambience", gap, " dB");
        s.metric("ambience bed at default", toDb(rms(bed)), " dBFS");
        s.check(gap >= 10.0 && gap <= 16.0, "a mezzo-forte melody sits 10-16 dB over the ambience at default", juce::String(gap, 1).toStdString() + " dB");
    }
    {
        Rig rig;
        dry(rig);
        const auto ref = rig.render(0.5, { { rig.time + 0.001, on(60) } });
        rig.render(0.01, { { rig.time, off(60) } });
        rig.render(2.0);
        rig.set(params::instrument, -12.0f);
        rig.render(0.2);
        const auto lower = rig.render(0.5, { { rig.time + 0.001, on(60) } });
        const double change = toDb(rms(lower)) - toDb(rms(ref));
        s.check(std::abs(change + 12.0) < 1.5, "Instrument Level -12 dB lowers the instrument 12 dB", juce::String(change, 2).toStdString());
    }
    {
        Rig rig;
        rig.render(3.0);
        const auto ref = rig.render(2.0);
        rig.set(params::master, -12.0f);
        rig.render(0.3);
        const auto lower = rig.render(2.0);
        const double change = toDb(rms(lower)) - toDb(rms(ref));
        s.check(std::abs(change + 12.0) < 2.0, "Master Level -12 dB lowers the output about 12 dB", juce::String(change, 2).toStdString());
        rig.set(params::master, params::minDb);
        rig.render(0.3);
        s.check(rms(rig.render(0.5)) < 1.0e-6, "Master Level at minimum is silent");
    }
    {
        Rig rig;
        rig.set(params::ambience, params::minDb);
        rig.render(3.0);
        s.check(rms(rig.render(1.0)) < 1.0e-6, "Ambience Level at minimum leaves silence without notes");
    }
    {
        Rig a, b;
        a.set(params::space, 0.0f);
        b.set(params::space, 1.0f);
        dry(a);
        b.set(params::ambience, params::minDb);
        b.render(0.2);
        a.render(0.3, { { a.time, on(60) }, { a.time + 0.2, off(60) } });
        b.render(0.3, { { b.time, on(60) }, { b.time + 0.2, off(60) } });
        const auto tailA = a.render(0.8), tailB = b.render(0.8);
        s.check(rms(tailB) > rms(tailA) * 3.0, "Space adds an audible room tail after the note", db(rms(tailB)) + " vs " + db(rms(tailA)));
    }
}

void testPower(Suite& s) {
    s.section("power");
    Rig rig;
    rig.render(1.0, { { 0.2, on(60) } });
    rig.set(params::power, 0.0f);
    rig.render(0.2);
    const auto off = rig.render(1.0, { { rig.time + 0.1, on(64) } });
    s.check(rms(off) == 0.0, "Power off is exact silence after the fade (notes too)");
    rig.set(params::power, 1.0f);
    rig.render(0.3);
    s.check(toDb(rms(rig.render(1.0))) > -55.0, "Power on brings the ambience back");
}

void testRates(Suite& s) {
    s.section("rates");
    for (const double rate : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 }) {
        Rig rig(rate, 512);
        const auto out = rig.render(1.5, { { 0.5, on(60) }, { 0.5, on(67) }, { 1.2, off(60) }, { 1.2, off(67) } });
        s.check(finite(out) && toDb(rms(out)) > -40.0, "audible and finite at " + std::to_string(int(rate)) + " Hz", db(rms(out)));
    }
    for (const int size : { 1, 7, 64, 333, 1024, 4096 }) {
        Rig rig(48000.0, 512);   // 4096 is larger than announced: must still work
        const auto out = rig.render(0.8, { { 0.3, on(60) } }, size);
        s.check(finite(out) && toDb(rms(out)) > -40.0, "block size " + std::to_string(size), db(rms(out)));
    }
    // MIDI timing is sample-accurate inside a block.
    {
        Rig a(48000.0, 512), b(48000.0, 64);
        dry(a);
        dry(b);
        const double t = a.time;
        auto x = a.render(0.2, { { t + 0.05, on(60) } });
        auto y = b.render(0.2, { { t + 0.05, on(60) } });
        double diff = 0;
        for (std::size_t i = 0; i < x.first.size(); ++i) diff = std::max(diff, double(std::abs(x.first[i] - y.first[i])));
        s.check(diff < 1.0e-5, "the same note at the same time renders identically with 512 and 64 sample blocks", juce::String(diff).toStdString());
    }
    Rig mono(48000.0, 512, true);
    const auto out = mono.render(1.0, { { 0.3, on(60) } });
    s.check(mono.p->getTotalNumOutputChannels() == 1 && finite(out) && toDb(rms(out)) > -40.0, "mono output works", db(rms(out)));
}

void testDeterminism(Suite& s) {
    s.section("determinism");
    Rig a, b;
    const std::vector<Event> events { { 0.5, on(60) }, { 0.7, on(64) }, { 1.5, off(60) }, { 1.6, off(64) } };
    const auto x = a.render(3.0, events), y = b.render(3.0, events);
    s.check(x == y, "two instances render identical output from the same input");
}

void testRealtime(Suite& s) {
    s.section("realtime");
    Rig rig;
    rig.render(0.5);
    std::vector<Event> events;
    for (int i = 0; i < 16; ++i) events.push_back({ rig.time + 0.01, on(36 + 3 * i, 90) });
    events.push_back({ rig.time + 0.02, pedal(true) });
    events.push_back({ rig.time + 0.03, juce::MidiMessage::pitchWheel(1, 12000) });
    // Allocation guard: the buffers are allocated outside; only processBlock is probed.
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (const auto& e : events) midi.addEvent(e.message, 10);
    allocationProbe::calls = 0;
    allocationProbe::enabled = true;
    rig.p->processBlock(buffer, midi);
    juce::MidiBuffer none;
    const auto start = std::chrono::steady_clock::now();
    const int blocks = int(10.0 * 48000.0 / 512.0);
    for (int i = 0; i < blocks; ++i) rig.p->processBlock(buffer, none);
    allocationProbe::enabled = false;
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    s.check(allocationProbe::calls == 0, "processBlock never allocates (16 notes, pedal, bend)", std::to_string(allocationProbe::calls) + " allocations");
    s.metric("CPU, 16 sounding notes + ambience + room, % of one core at 48 kHz (report-only)", 100.0 * seconds / 10.0, " %");
}

void testEditor(Suite& s, const juce::String& snapshot) {
    s.section("editor");
    LiminalProcessor p;
    p.prepareToPlay(48000.0, 512);
    for (int i = 0; i < 6; ++i) {
        const auto start = std::chrono::steady_clock::now();
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (i == 0) {
            s.check(editor->getWidth() == LiminalEditor::defaultWidth && editor->getHeight() == LiminalEditor::defaultHeight, "editor has its size");
            s.check(image.getWidth() == 2 * LiminalEditor::defaultWidth, "editor renders at 2x (Retina)");
            // The scene is a picture, not a flat fill: sample brightness spread.
            float lo = 1, hi = 0;
            for (int y = 10; y < image.getHeight(); y += 37)
                for (int x = 10; x < image.getWidth(); x += 41) {
                    const float b = image.getPixelAt(x, y).getBrightness();
                    lo = std::min(lo, b);
                    hi = std::max(hi, b);
                }
            s.check(hi - lo > 0.4f, "the editor shows a scene (brightness range > 0.4)", juce::String(hi - lo, 2).toStdString());
            s.check(editor->getNumChildComponents() >= 5, "the editor has its controls");
            if (snapshot.isNotEmpty()) {
                juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(snapshot);
                file.getParentDirectory().createDirectory();
                file.deleteFile();
                juce::FileOutputStream out(file);
                juce::PNGImageFormat().writeImageToStream(image, out);
                std::cout << "  snapshot written to " << file.getFullPathName() << "\n";
            }
        }
        if (i == 0) {   // The instruments drawer: opens, shows each instrument's controls, paints quickly.
            auto* lm = dynamic_cast<LiminalEditor*>(editor.get());
            s.check(lm != nullptr, "the editor is LIMINAL's");
            if (lm != nullptr) {
                lm->showInstruments(true);
                bool fine = lm->instrumentDrawer().isVisible() && lm->instrumentDrawer().getNumChildComponents() >= 9;
                double worst = 0;
                for (int k = 0; k < InstrumentDrawer::count; ++k) {
                    lm->instrumentDrawer().focus(k);
                    const auto t0 = std::chrono::steady_clock::now();
                    const auto shot = lm->createComponentSnapshot(lm->getLocalBounds(), true, 2.0f);
                    worst = std::max(worst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
                    fine = fine && shot.isValid();
                    if (snapshot.isNotEmpty()) {
                        const char* names[] { "piano", "synth", "glass", "choir", "box", "drone" };
                        juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(snapshot).getSiblingFile(juce::String("editor-") + names[k] + ".png");
                        file.deleteFile();
                        juce::FileOutputStream out(file);
                        juce::PNGImageFormat().writeImageToStream(shot, out);
                    }
                }
                s.check(fine, "the Instruments drawer opens and shows each instrument's page");
                s.check(lm->instrumentsToggle().getToggleState(), "the INSTRUMENTS button is lit while its drawer is open");
                s.metric("full 2x repaint with the drawer open (worst of six)", worst, " ms");
                lm->instrumentDrawer().focus(0);
                lm->showInstruments(false);
                s.check(!lm->instrumentsToggle().getToggleState(), "and unlit when it closes");
                // The strip names the instruments that are on; clicking a name solos it.
                auto& active = lm->activeInstruments();
                s.check(active.text() == "PIANO", "the strip shows the active instrument (PIANO by default)", active.text().toStdString());
                p.parameters.getParameter(params::glassOn)->setValueNotifyingHost(1.0f);
                active.refresh();
                s.check(active.text() == "PIANO + GLASS", "and every instrument that is on (PIANO + GLASS)", active.text().toStdString());
                active.clickName(2);
                s.check(std::lround(p.parameters.getRawParameterValue(params::solo)->load()) == 3, "clicking GLASS solos the glass");
                if (snapshot.isNotEmpty()) {
                    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(snapshot).getSiblingFile("editor-solo.png");
                    file.deleteFile();
                    juce::FileOutputStream out(file);
                    juce::PNGImageFormat().writeImageToStream(lm->createComponentSnapshot(lm->getLocalBounds(), true, 2.0f), out);
                }
                active.clickName(2);
                s.check(std::lround(p.parameters.getRawParameterValue(params::solo)->load()) == 0, "clicking it again clears the solo");
                p.parameters.getParameter(params::glassOn)->setValueNotifyingHost(0.0f);
                // The environment drawer: open it, pick each place, snapshot it.
                lm->showEnvironment(true);
                s.check(lm->environmentDrawer().isVisible() && !lm->instrumentDrawer().isVisible() && lm->environmentToggle().getToggleState(),
                        "the ENVIRONMENT drawer opens (and the instruments drawer closes)");
                bool picked = true;
                for (int place = 0; place < dsp::placeCount; ++place) {
                    lm->environmentDrawer().choose(place);
                    picked &= std::lround(p.parameters.getRawParameterValue(params::environment)->load()) == place;
                    if (snapshot.isNotEmpty()) {
                        juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(snapshot).getSiblingFile(
                            juce::String("editor-place-") + juce::String(place + 1) + ".png");
                        file.deleteFile();
                        juce::FileOutputStream out(file);
                        juce::PNGImageFormat().writeImageToStream(lm->createComponentSnapshot(lm->getLocalBounds(), true, 2.0f), out);
                    }
                }
                s.check(picked, "clicking a place selects it");
                lm->environmentDrawer().choose(0);
                lm->showEnvironment(false);
            }
        }
        s.metric("editor open + first 2x paint", ms, " ms");
        if (i == 5) s.check(ms < 1500.0, "editor opens and paints within 1.5 s (software renderer)", juce::String(ms, 0).toStdString() + " ms");
        p.editorBeingDeleted(editor.get());
    }
    // The governor's rules (from WORLDBUILDER): one frame at a time, late ticks skipped.
    ui::FrameGovernor g;
    double elapsed = 0;
    s.check(g.beginFrame(0, elapsed), "governor: first frame drawn");
    s.check(!g.beginFrame(33, elapsed), "governor: no new frame while one is pending");
    g.painted(5, 40);
    s.check(g.beginFrame(66, elapsed), "governor: next frame after painting");
    g.painted(40, 70);
    s.check(g.frameInterval() > 1, "governor: expensive frames are spaced out");
}

// Each of the six instruments plays the same phrase and chord: first in the corridor
// (default Space and ambience), then completely dry. Listening aid; CI uploads it.
int renderPhrase(const juce::String& path) {
    Rig rig;
    rig.render(1.0);
    Signal out;
    const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) dry(rig);
        for (const char* which : ons) {
            for (const char* id : ons) rig.set(id, std::strcmp(id, which) == 0 ? 1.0f : 0.0f);
            std::vector<Event> events;
            const int notes[] { 64, 67, 71, 72, 71, 67, 64, 60, 59, 60 };
            double t = rig.time + 0.5;
            for (int i = 0; i < 10; ++i, t += (i % 4 == 0 ? 1.0 : 0.5)) {
                events.push_back({ t, on(notes[i], 60 + (i * 7) % 40) });
                events.push_back({ t + 0.4, off(notes[i]) });
            }
            for (int note : { 45, 52, 57, 60 }) {
                events.push_back({ t + 0.5, on(note, 70) });
                events.push_back({ t + 3.5, off(note) });
            }
            const auto part = rig.render(t + 6.5 - rig.time, events);
            out.first.insert(out.first.end(), part.first.begin(), part.first.end());
            out.second.insert(out.second.end(), part.second.begin(), part.second.end());
        }
    }
    juce::AudioBuffer<float> buffer(2, int(out.first.size()));
    for (std::size_t i = 0; i < out.first.size(); ++i) {
        buffer.setSample(0, int(i), out.first[i]);
        buffer.setSample(1, int(i), out.second[i]);
    }
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    auto writer = juce::WavAudioFormat().createWriterFor(
        stream, juce::AudioFormatWriterOptions {}.withSampleRate(rig.rate).withNumChannels(2).withBitsPerSample(24));
    if (writer == nullptr) return 1;
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    std::cout << "wrote " << file.getFullPathName() << "\n";
    return 0;
}

// The same phrase (piano, then the piano with the choir) in each place, ambience on,
// 12 s per place, with a place change mid-phrase at the end. Listening aid; CI uploads it.
int renderPlaces(const juce::String& path) {
    Signal out;
    for (int place = 0; place < dsp::placeCount; ++place) {
        auto rig = rigIn(place);
        rig->set(params::choirOn, 1.0f);
        rig->set(params::choirLevel, -8.0f);
        rig->render(1.0);
        const auto part = rig->render(12.0, standardPhrase(rig->time + 0.5));
        out.first.insert(out.first.end(), part.first.begin(), part.first.end());
        out.second.insert(out.second.end(), part.second.begin(), part.second.end());
    }
    {   // Changing place while a chord rings: hospital -> pool -> garage.
        auto rig = rigIn(0);
        const double t = rig->time;
        std::vector<Event> events;
        for (int note : { 48, 55, 60, 64 }) {
            events.push_back({ t + 0.2, on(note, 80) });
            events.push_back({ t + 5.0, off(note) });
        }
        events.push_back({ t + 0.2, pedal(true) });
        const auto a = rig->render(2.0, events);
        setPlace(*rig, 4);
        const auto b = rig->render(3.0, events);
        setPlace(*rig, 3);
        const auto c = rig->render(5.0, events);
        for (const auto* part : { &a, &b, &c }) {
            out.first.insert(out.first.end(), part->first.begin(), part->first.end());
            out.second.insert(out.second.end(), part->second.begin(), part->second.end());
        }
    }
    juce::AudioBuffer<float> buffer(2, int(out.first.size()));
    for (std::size_t i = 0; i < out.first.size(); ++i) {
        buffer.setSample(0, int(i), out.first[i]);
        buffer.setSample(1, int(i), out.second[i]);
    }
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    auto writer = juce::WavAudioFormat().createWriterFor(
        stream, juce::AudioFormatWriterOptions {}.withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(24));
    if (writer == nullptr) return 1;
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    std::cout << "wrote " << file.getFullPathName() << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String snapshot;
    juce::StringArray only;
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]);
        if (arg == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (arg == "--only" && i + 1 < argc) only = juce::StringArray::fromTokens(argv[++i], ",", "");
        else if (arg == "--render" && i + 1 < argc) return renderPhrase(argv[++i]);
        else if (arg == "--render-places" && i + 1 < argc) return renderPlaces(argv[++i]);
    }
    const auto run = [&](const char* name) { return only.isEmpty() || only.contains(name); };
    Suite s;
    if (run("parameters")) testParameters(s);
    if (run("smoke")) testSmoke(s);
    if (run("midi")) testMidi(s);
    if (run("levels")) testLevels(s);
    if (run("power")) testPower(s);
    if (run("rates")) testRates(s);
    if (run("determinism")) testDeterminism(s);
    if (run("piano")) testPiano(s);
    if (run("synth")) testSynth(s);
    if (run("glass")) testGlass(s);
    if (run("choir")) testChoir(s);
    if (run("box")) testBox(s);
    if (run("drone")) testDrone(s);
    if (run("instruments")) testInstruments(s);
    if (run("environments")) testEnvironments(s);
    if (run("realtime")) testRealtime(s);
    if (run("editor")) testEditor(s, snapshot);
    std::cout << "\n" << s.passed << " passed, " << s.failed << " failed" << std::endl;
    return s.failed == 0 ? 0 : 1;
}
