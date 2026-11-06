// Loudness of all twenty instruments (2026-11-06). Included by LiminalTests.cpp after LibraryTests.h.
// Group `loudness`; `--probe-loudness` prints the table (developer aid).
//   Every instrument at its defaults (LIMINALITY at its default too), alone in a slot, the same test
//   phrase (recordedPhrase: a melody, then four chords) in the same room (the default place, its
//   Character, the room's default Space), the room's own sounds (ambience bed, events) turned down so
//   only the instrument and its reverberation are measured. Loudness: ITU-R BS.1770 (K-weighted,
//   gated, LUFS) over the phrase and its tail; also the plain RMS and the sample peak.
//   Checks: all twenty within +/-1.5 dB of their mean loudness; no clipping (peak below 0 dBFS, also
//   for a loud phrase: velocity 127, six-note chords with the pedal).

namespace loudnesstest {

constexpr double rate = 48000.0;
int slot = 1;   // every instrument in the same slot (the synth's), so only the instrument differs
bool probeDry = false, probeClean = false;
int place = -1, character = -1;   // -1: the defaults   // (developer aid: the probe's other conditions)

// ITU-R BS.1770-4 at 48 kHz: the K-weighting (a high shelf, then a high-pass), 400 ms blocks every
// 100 ms, the absolute gate at -70 LUFS and the relative gate 10 LU below.
double lufs(const Signal& x, double from, double to) {
    struct Biquad {
        double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
        double run(double v) {
            const double y = b0 * v + z1;
            z1 = b1 * v - a1 * y + z2;
            z2 = b2 * v - a2 * y;
            return y;
        }
    };
    const auto weighted = [&](const std::vector<float>& in) {
        Biquad shelf { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        Biquad high { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        std::vector<double> out(in.size());
        for (std::size_t i = 0; i < in.size(); ++i) out[i] = high.run(shelf.run(in[i]));
        return out;
    };
    const auto l = weighted(x.first), r = weighted(x.second);
    const auto a = std::size_t(std::max(0.0, from) * rate), b = std::min(l.size(), std::size_t(to * rate));
    const std::size_t block = std::size_t(0.4 * rate), step = std::size_t(0.1 * rate);
    std::vector<double> z;
    for (std::size_t s = a; s + block <= b; s += step) {
        double sum = 0;
        for (std::size_t i = s; i < s + block; ++i) sum += l[i] * l[i] + r[i] * r[i];
        z.push_back(sum / double(block));
    }
    const auto loud = [](double power) { return -0.691 + 10.0 * std::log10(std::max(power, 1.0e-20)); };
    const auto gated = [&](double gate) {
        double sum = 0;
        int n = 0;
        for (double v : z)
            if (loud(v) > gate) { sum += v; ++n; }
        return n > 0 ? sum / n : 0.0;
    };
    const double absolute = gated(-70.0);
    return loud(gated(loud(absolute) - 10.0));
}

double peakOf(const Signal& x) {
    float p = 0;
    for (std::size_t i = 0; i < x.first.size(); ++i) p = std::max({ p, std::abs(x.first[i]), std::abs(x.second[i]) });
    return toDb(p);
}

// The instrument alone in the slot, everything else at its defaults, the room's own sounds down.
std::unique_ptr<Rig> rigWith(int which, int place = loudnesstest::place, int character = loudnesstest::character) {
    std::unique_ptr<Rig> rig;
    {   // (AtLiminalityDefault is a global flag the Rig reads when it is made: one at a time across threads)
        static std::mutex making;
        const std::lock_guard<std::mutex> lock(making);
        std::optional<AtLiminalityDefault> limDefault;
        if (!probeClean) limDefault.emplace();
        rig = std::make_unique<Rig>(rate);
    }
    if (probeDry) dry(*rig);
    if (std::getenv("PROBE_NOINFLUENCE")) rig->set(params::environmentAmount, 0.0f);
    if (std::getenv("PROBE_NOSPACE")) rig->set(params::space, 0.0f);
    if (auto* v = std::getenv("PROBE_PLACE")) rig->set(params::environment, float(std::atoi(v)));
    if (place >= 0) rig->set(params::environment, float(place));
    if (character >= 0) rig->set(params::character, float(character));
    const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    for (int k = 0; k < 6; ++k) rig->set(ons[k], k == slot ? 1.0f : 0.0f);
    rig->set(params::slotTypeId(slot).toRawUTF8(), float(which));
    rig->set(params::ambience, params::minDb);   // the bed and the place's events, not the room's reverberation
    rig->render(0.5);
    return rig;
}

struct Level { double lufs = 0, rms = 0, peak = -200, loudPeak = -200; };

// A second phrase, not the one the table was calibrated on: slow held notes, then quick staccato
// ones, then a long four-note chord (does the match hold for other playing?).
std::vector<Event> otherPhrase(double t0) {
    std::vector<Event> e;
    const int slow[] { 62, 69, 67, 65 };
    for (int i = 0; i < 4; ++i) {
        e.push_back({ t0 + 1.2 * i, on(slow[i], 75) });
        e.push_back({ t0 + 1.2 * i + 1.1, off(slow[i]) });
    }
    const int quick[] { 72, 74, 76, 74, 72, 71, 69, 71, 72, 67, 64, 60 };
    for (int i = 0; i < 12; ++i) {
        e.push_back({ t0 + 5.0 + 0.18 * i, on(quick[i], 105) });
        e.push_back({ t0 + 5.0 + 0.18 * i + 0.09, off(quick[i]) });
    }
    for (int n : { 57, 60, 64, 69 }) {
        e.push_back({ t0 + 7.5, on(n, 85) });
        e.push_back({ t0 + 10.5, off(n) });
    }
    return e;
}
bool second = false;   // measure() plays otherPhrase()

Level measure(int which, bool loudToo, bool other = second, int place = loudnesstest::place, int character = loudnesstest::character) {
    Level m;
    {
        auto rig = rigWith(which, place, character);
        const auto y = rig->render(11.5, other ? otherPhrase(rig->time + 0.1) : recordedPhrase(rig->time + 0.1));
        m.lufs = lufs(y, 0.0, 11.5);
        m.rms = windowDb(y, rate, 0.1, 10.5);
        m.peak = peakOf(y);
    }
    if (loudToo) {   // as hard as a player can: velocity 127, six-note chords, the pedal down
        auto rig = rigWith(which, place, character);
        std::vector<Event> e;
        const double t0 = rig->time + 0.1;
        e.push_back({ t0, pedal(true) });
        const int chords[][6] { { 36, 48, 55, 60, 64, 67 }, { 33, 45, 52, 57, 60, 64 }, { 41, 53, 60, 65, 69, 72 }, { 43, 55, 62, 67, 71, 74 } };
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 6; ++k) {
                e.push_back({ t0 + 1.0 * c, on(chords[c][k], 127) });
                e.push_back({ t0 + 1.0 * c + 0.9, off(chords[c][k]) });
            }
        e.push_back({ t0 + 4.5, pedal(false) });
        m.loudPeak = peakOf(rig->render(8.0, e));
    }
    return m;
}

} // namespace loudnesstest

// The table (developer aid): recordings and stand-ins.
int probeLoudness() {
    using namespace loudnesstest;
    if (auto* v = std::getenv("PROBE_SLOT")) slot = std::atoi(v);
    probeDry = std::getenv("PROBE_DRY") != nullptr;
    probeClean = std::getenv("PROBE_CLEAN") != nullptr;
    for (const bool recorded : { true, false }) {
        if (recorded && !recordingsReady()) continue;
        BypassRecordings bypass(!recorded);
        std::cout << (recorded ? "recordings" : "stand-ins") << ":\n";
        std::vector<Level> all;
        double mean = 0;
        for (int w = 0; w < params::libraryCount; ++w) {
            all.push_back(measure(w, true));
            mean += all.back().lufs / params::libraryCount;
        }
        for (int w = 0; w < params::libraryCount; ++w)
            std::cout << "  " << juce::String(params::libraryNames[w]).paddedRight(' ', 20) << " " << num(all[std::size_t(w)].lufs, 2) << " LUFS  (" << num(all[std::size_t(w)].lufs - mean, 2)
                      << " vs mean)  rms " << num(all[std::size_t(w)].rms, 2) << "  peak " << num(all[std::size_t(w)].peak, 1) << "  loud peak " << num(all[std::size_t(w)].loudPeak, 1) << "\n";
        std::cout << "  mean " << num(mean, 2) << " LUFS\n";
    }
    return 0;
}

void testLoudness(Suite& s) {
    using namespace loudnesstest;
    s.section("loudness: all twenty instruments level-matched (same phrase, same room, defaults), no clipping");
    const bool ready = recordingsReady();
    // The twenty in one place / Character: their loudness (LUFS) and peaks.
    const auto measureAll = [&](bool loudToo) {
        std::vector<Level> all;
        for (int w = 0; w < params::libraryCount; ++w) all.push_back(measure(w, loudToo));
        return all;
    };
    const auto spread = [](const std::vector<Level>& all, double& mean, std::string& outside) {
        mean = 0;
        for (const auto& m : all) mean += m.lufs / double(all.size());
        double worst = 0;
        for (std::size_t w = 0; w < all.size(); ++w) {
            const double d = all[w].lufs - mean;
            worst = std::max(worst, std::abs(d));
            if (std::abs(d) > 1.5) outside += std::string(params::libraryNames[w]) + " " + num(d, 1) + ", ";
        }
        return worst;
    };
    for (const bool recorded : { true, false }) {
        if (recorded && !ready) continue;
        BypassRecordings bypass(!recorded);
        // The shipped build plays the recordings: that is the set held to the window; the stand-ins
        // (a build without the recordings) are reported, and held to it when they are all there is.
        const bool enforced = recorded || !ready;
        const std::string tag = recorded ? " (recordings)" : " (stand-ins)";
        const auto hold = [&](bool okay, const std::string& name, const std::string& detail) {
            if (enforced) s.check(okay, name, detail);
            else s.report(okay, name, detail);
        };
        // 1. The default room, the test phrase: before (the table off) and now.
        std::vector<Level> before;
        {
            dsp::levelmatch::bypassed() = true;
            before = measureAll(false);
            dsp::levelmatch::bypassed() = false;
        }
        const auto all = measureAll(true);
        double mean = 0, meanBefore = 0;
        std::string outside, outsideBefore;
        const double worst = spread(all, mean, outside), worstBefore = spread(before, meanBefore, outsideBefore);
        double worstPeak = -200, worstLoudPeak = -200;
        for (int w = 0; w < params::libraryCount; ++w) {
            const auto& m = all[std::size_t(w)];
            s.metric(std::string(params::libraryNames[w]) + tag + ": loudness vs the mean of the twenty", m.lufs - mean, " dB");
            std::cout << "  [loudness] " << juce::String(params::libraryNames[w]).paddedRight(' ', 20) << tag << ": before " << num(before[std::size_t(w)].lufs - meanBefore, 2) << " dB, now "
                      << num(m.lufs - mean, 2) << " dB (" << num(m.lufs, 2) << " LUFS, change " << num(m.lufs - before[std::size_t(w)].lufs, 2) << " dB); RMS " << num(m.rms, 2)
                      << " dBFS, peak " << num(m.peak, 1) << " dBFS, loud phrase peak " << num(m.loudPeak, 1) << " dBFS\n";
            worstPeak = std::max(worstPeak, m.peak);
            worstLoudPeak = std::max(worstLoudPeak, m.loudPeak);
        }
        std::cout << "  [loudness] before: worst " << num(worstBefore, 2) << " dB from the mean (" << outsideBefore << ")\n";
        hold(worst <= 1.5, "all twenty instruments within +/-1.5 dB of their mean loudness, the default room, the test phrase" + tag,
             "mean " + num(mean, 1) + " LUFS, worst " + num(worst, 2) + " dB (before: " + num(worstBefore, 2) + ")" + (outside.empty() ? std::string() : "; outside: " + outside));
        s.check(worstPeak < 0.0, "no clipping on the test phrase" + tag, "highest peak " + num(worstPeak, 1) + " dBFS");
        s.check(worstLoudPeak < 0.0, "no clipping on a loud phrase (velocity 127, six-note chords, pedal)" + tag, "highest peak " + num(worstLoudPeak, 1) + " dBFS");
        // 2. Another phrase (not the calibration's), the default room.
        {
            second = true;
            const auto other = measureAll(false);
            second = false;
            double m2 = 0;
            std::string out2;
            const double w2 = spread(other, m2, out2);
            // Register, articulation and velocity change each instrument's loudness on their own (a
            // bass played high, a pad played staccato): the window is the test phrase's; here the
            // spread is reported against it and held to +/-3.5 dB (it was 8 dB before).
            const std::string detail = "worst " + num(w2, 2) + " dB" + (out2.empty() ? std::string() : "; outside +/-1.5: " + out2);
            s.report(w2 <= 1.5, "another phrase (slow notes, staccato, a long chord) within +/-1.5 dB, the default room" + tag, detail);
            hold(w2 <= 3.5, "another phrase (slow notes, staccato, a long chord) within +/-3.5 dB, the default room" + tag, detail);
        }
        // 3. Every place and Character (the test phrase): the table covers them all.
        if (recorded || !ready) {
            // (four threads, one processor each: every place / Character is independent)
            std::vector<std::vector<Level>> results(std::size_t(dsp::placeCount * dsp::characterCount));
            std::atomic<int> next { 0 };
            std::vector<std::thread> workers;
            for (int t = 0; t < 4; ++t)
                workers.emplace_back([&] {
                    for (int k = next++; k < int(results.size()); k = next++)
                        for (int w = 0; w < params::libraryCount; ++w) results[std::size_t(k)].push_back(measure(w, false, false, k / dsp::characterCount, k % dsp::characterCount));
                });
            for (auto& t : workers) t.join();
            double worstAll = 0;
            std::string where;
            for (int k = 0; k < int(results.size()); ++k) {
                double m3 = 0;
                std::string out3;
                const double w3 = spread(results[std::size_t(k)], m3, out3);
                if (w3 > worstAll) { worstAll = w3; where = dsp::character(k / dsp::characterCount, k % dsp::characterCount).name + std::string(": ") + out3; }
            }
            s.check(worstAll <= 1.5, "within +/-1.5 dB in every place and Character (all thirty, the test phrase)" + tag, "worst " + num(worstAll, 2) + " dB " + where);
        }
    }
    // 4. Dry (no Environment Amount): the level match is off, the instruments exactly as before.
    {
        double diff = 0;
        for (int w : { 1, 8, 13 }) {
            auto a = rigWith(w), b = rigWith(w);
            a->set(params::environmentAmount, 0.0f);
            b->set(params::environmentAmount, 0.0f);
            auto ya = a->render(4.0, recordedPhrase(a->time + 0.1));
            dsp::levelmatch::bypassed() = true;
            auto yb = b->render(4.0, recordedPhrase(b->time + 0.1));
            dsp::levelmatch::bypassed() = false;
            for (std::size_t i = 0; i < ya.first.size(); ++i) diff = std::max(diff, double(std::abs(ya.first[i] - yb.first[i])));
        }
        s.check(diff == 0.0, "with Environment Amount at 0 the level match does nothing (bit-exact)", num(diff, 8));
    }
}

// The calibration of DSP/LevelMatch.h: every instrument's loudness in each place and Character with
// the table off, for the places from..to (several processes share the work); the lines are merged
// into Source/DSP/LevelMatchTable.h by tools/levelmatch.py.
int calibrateLoudness(int from, int to) {
    using namespace loudnesstest;
    dsp::levelmatch::bypassed() = true;
    std::cout << "recordings " << (recordingsReady() ? "yes" : "no") << "\n";
    for (int p = std::max(0, from); p <= std::min(dsp::placeCount - 1, to); ++p)
        for (int c = 0; c < dsp::characterCount; ++c) {
            place = p;
            character = c;
            for (int w = 0; w < params::libraryCount; ++w) std::cout << "LOUDNESS " << w << " " << p << " " << c << " " << juce::String(measure(w, false).lufs, 3) << std::endl;
        }
    return 0;
}

