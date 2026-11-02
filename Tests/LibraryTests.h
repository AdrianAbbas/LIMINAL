// The instrument library (2026-11-02): the fourteen new instruments, each in a slot (DSP/Library).
// Included by LiminalTests.cpp after RecordedTests.h. Every check runs dry at LIMINALITY 0, with the
// recordings when they are there (CI) and again on the synthesised stand-ins.
//   plays from MIDI in a slot that held another instrument; in tune across its range; no clicks
//   (notes, chords, the release, the pedal, a change of instrument while notes sound); level-matched to
//   the Forgotten Piano on one phrase (and the recordings to their stand-in); distinct from all the
//   other nineteen (a feature distance between every pair).

namespace librarytest {

constexpr double rate = 48000.0;
constexpr int testSlot = 1;   // the synth's slot holds each instrument in turn

struct Case {
    int kind;                 // 0..13 (dsp::library::Kind)
    int low, high;            // the range checked for pitch
    double hold;              // seconds held for pitch
    bool sustained;
    std::vector<std::pair<const char*, float>> tuning;   // controls for the pitch check (Age 0, ...)
};

std::vector<Case> cases() {
    using namespace dsp::library;
    const char* colour = "synth_lib_colour";
    return {
        { oldGuitar, 40, 79, 0.9, false, {} },
        { fadedStrings, 55, 88, 1.6, true, {} },
        { hallwayPizzicato, 55, 88, 0.6, false, {} },
        { toyPiano, 60, 96, 0.5, false, { { colour, 0.0f } } },   // Broken detunes some rods on purpose
        { loungeEPiano, 36, 84, 1.0, false, {} },
        { elevatorVibraphone, 53, 89, 1.0, false, {} },
        { mallChimes, 60, 96, 1.0, false, {} },
        { soloVoice, 40, 72, 1.8, true, {} },
        { hummedMemory, 40, 67, 1.8, true, {} },
        { cassetteStrings, 36, 84, 1.6, true, {} },
        { arcadeLead, 48, 84, 1.2, true, {} },
        { basementBass, 28, 60, 1.2, true, {} },
        { warpedOrgan, 36, 84, 1.2, true, {} },
        { bowedGlass, 48, 96, 2.0, true, {} },
    };
}

// A dry rig with only `slot` on, holding instrument `which` (0..19).
std::unique_ptr<Rig> slotRig(int slot, int which, std::initializer_list<std::pair<const char*, float>> settings = {}) {
    auto rig = std::make_unique<Rig>(rate);
    const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    solo(*rig, ons[slot]);
    rig->set(params::slotTypeId(slot).toRawUTF8(), float(which));
    rig->set(params::libId(slot, "age").toRawUTF8(), 0.5f);
    for (const auto& [id, value] : settings) rig->set(id, value);
    rig->render(0.05);
    return rig;
}
std::unique_ptr<Rig> libraryRig(int kind, std::initializer_list<std::pair<const char*, float>> settings = {}) {
    return slotRig(testSlot, kind + dsp::library::originals, settings);
}
std::unique_ptr<Rig> rigFor(int which) {   // any of the twenty, in its own slot for the originals
    return which < dsp::library::originals ? slotRig(which, which) : libraryRig(which - dsp::library::originals);
}

// A single held note: attack, held part and release, for the envelope features.
struct Features { std::vector<double> v; };
Features featuresOf(int which) {
    auto rig = rigFor(which);
    const auto phrase = rig->render(9.5, recordedPhrase(rig->time + 0.1));
    auto one = rigFor(which);
    const auto note = playNote(*one, 60, 100, 1.5, 1.5);
    Features f;
    const auto mono = monoOf(phrase, rate, 0.1, 9.5);
    const Spectrum sp(mono, rate, 1 << 18);
    // Spectral balance: the centroid and the share of five bands.
    f.v.push_back(std::log2(std::max(50.0, sp.centroid(30, 16000))) * 2.0);
    const double total = sp.bandDb(30, 16000);
    for (const auto& [lo, hi] : { std::pair { 30.0, 200.0 }, { 200.0, 700.0 }, { 700.0, 2000.0 }, { 2000.0, 5000.0 }, { 5000.0, 16000.0 } })
        f.v.push_back(std::max(-40.0, sp.bandDb(lo, hi) - total) / 6.0);
    // The note's envelope (20 ms frames, dB): how fast it speaks, how it holds, how it lets go.
    const auto frames = [&](double from, double to) {
        std::vector<double> e;
        for (double t = from; t + 0.02 <= to; t += 0.02) e.push_back(windowDb(note, rate, t, t + 0.02));
        return e;
    };
    const auto held = frames(0.0, 1.5), after = frames(1.5, 3.0);
    const double peak = *std::max_element(held.begin(), held.end());
    int rise = 0;
    while (rise < int(held.size()) && held[std::size_t(rise)] < peak - 3.0) ++rise;
    f.v.push_back(std::log2(1.0 + double(rise)) * 1.5);                                   // attack
    f.v.push_back(std::clamp(peak - held.back(), 0.0, 60.0) / 5.0);                      // decay while held
    int gone = 0;
    while (gone < int(after.size()) && after[std::size_t(gone)] > held.back() - 25.0) ++gone;
    f.v.push_back(std::log2(1.0 + double(gone)) * 1.5);                                  // release
    // Movement while held (tremolo, a pad's wobble): the spread of the level over the held part.
    double mean = 0, var = 0;
    int n = 0;
    for (std::size_t i = 20; i < held.size(); ++i) { mean += held[i]; ++n; }
    mean /= std::max(1, n);
    for (std::size_t i = 20; i < held.size(); ++i) var += (held[i] - mean) * (held[i] - mean);
    f.v.push_back(std::min(6.0, std::sqrt(var / std::max(1, n))) / 1.0);
    // Inharmonic or noisy: the share of the held note's energy that is not near a harmonic of C4.
    {
        const auto m = monoOf(note, rate, 0.2, 1.4);
        const Spectrum s2(m, rate, 1 << 16);
        double harmonic = 0, all = 0;
        const double f0 = 261.63;
        for (std::size_t i = std::size_t(60 / s2.binHz); i < std::min(s2.mag.size(), std::size_t(12000 / s2.binHz)); ++i) {
            const double hz = double(i) * s2.binHz, p = s2.mag[i] * s2.mag[i];
            const double h = std::round(hz / f0);
            all += p;
            if (h >= 1 && std::abs(1200.0 * std::log2(hz / (h * f0))) < 35.0) harmonic += p;
        }
        f.v.push_back((1.0 - harmonic / std::max(1e-30, all)) * 6.0);
    }
    return f;
}
double distanceOf(const Features& a, const Features& b) {
    double d = 0;
    for (std::size_t i = 0; i < a.v.size(); ++i) d += (a.v[i] - b.v[i]) * (a.v[i] - b.v[i]);
    return std::sqrt(d);
}

} // namespace librarytest

void testLibraryPresets(Suite& s);

void testLibrary(Suite& s) {
    s.section("library (the fourteen new instruments, in a slot)");
    using namespace librarytest;
    const bool ready = recordingsReady();
    // The reference: the Forgotten Piano in its own slot, on the same phrase.
    double pianoDb = 0;
    {
        auto rig = slotRig(0, 0);
        const auto x = rig->render(9.5, recordedPhrase(rig->time + 0.1));
        pianoDb = windowDb(x, rate, 0.1, 9.5);
    }
    for (const bool recorded : { true, false }) {
        if (recorded && !ready) {
            s.report(false, "recordings present (the recorded half of this group needs them)", samples::Bank::get().message().toStdString());
            continue;
        }
        BypassRecordings bypass(!recorded);
        const std::string tag = recorded ? " (recordings)" : " (stand-in)";
        int plays = 0, slotChanged = 0;
        for (const auto& c : cases()) {
            const auto& info = dsp::library::info(c.kind + dsp::library::originals);
            const std::string name = info.name;
            const bool hasRecordings = std::strlen(info.sets) > 0;
            if (recorded && !hasRecordings) continue;   // synthesised only: checked once, below
            // Plays from MIDI in the slot.
            auto rig = libraryRig(c.kind, { { "synth_lib_age", 0.0f } });
            if (rig->p->playingIn(testSlot) != c.kind + dsp::library::originals) ++slotChanged;
            if (recorded && !rig->p->slotRecorded(testSlot)) s.check(false, name + ": plays the recordings", samples::Bank::get().message().toStdString());
            const int mid = (c.low + c.high) / 2;
            const auto x = playNote(*rig, mid, 100, c.hold, 1.0);
            plays += windowDb(x, rate, 0.05, c.hold) > -50.0 ? 1 : 0;
            if (windowDb(x, rate, 0.05, c.hold) <= -50.0) s.check(false, name + tag + ": plays from MIDI", num(windowDb(x, rate, 0.05, c.hold)) + " dB");
            // In tune across the range (Age 0, LIMINALITY 0).
            {
                auto t = libraryRig(c.kind, { { "synth_lib_age", 0.0f } });
                for (const auto& [id, v] : c.tuning) t->set(id, v);
                double worst = 0;
                std::string list;
                for (int note = c.low; note <= c.high; note += 4) {
                    const double expect = 440.0 * std::exp2((note - 69) / 12.0);
                    const auto y = playNote(*t, note, 96, c.hold, 0.6);
                    const double from = c.sustained ? c.hold * 0.45 : 0.05, to = c.sustained ? c.hold : std::min(c.hold, 0.6);
                    const double off = centsOff(y, rate, from, to, expect);
                    worst = std::max(worst, std::abs(off));
                    list += juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString() + " " + num(off, 0) + ", ";
                    t->render(0.3);
                }
                std::cout << "  [cents] " << name << tag << ": " << list << "\n";
                s.check(worst <= 25.0, name + tag + ": in tune from MIDI across its range (within 25 cents)", "worst " + num(worst, 1) + " cents");
            }
            // No clicks: the phrase (melody, chords), a held chord released, the pedal.
            {
                auto t = libraryRig(c.kind);
                auto events = recordedPhrase(t->time + 0.1);
                const double at = t->time + 10.0;
                events.push_back({ at, pedal(true) });
                for (int n : { 48, 55, 60, 64, 67, 71 }) events.push_back({ at + 0.01 * (n % 3), on(n, 70 + n % 40) });
                for (int n : { 48, 55, 60, 64, 67, 71 }) events.push_back({ at + 2.5, off(n) });
                events.push_back({ at + 3.5, pedal(false) });
                const auto y = t->render(16.0, events);
                const int clicks = clicksIn(y, rate, 0.05);
                s.check(clicks == 0, name + tag + ": no clicks (phrase, chords, release, pedal)", std::to_string(clicks) + " clicks");
            }
            // Level-matched to the piano (the phrase), within 3 dB.
            {
                auto t = libraryRig(c.kind);
                const auto y = t->render(9.5, recordedPhrase(t->time + 0.1));
                const double db = windowDb(y, rate, 0.1, 9.5) - pianoDb;
                s.metric(name + tag + ": phrase level vs the piano", db, " dB");
                s.check(std::abs(db) <= 3.0, name + tag + ": level-matched to the Forgotten Piano (within 3 dB on one phrase)", num(db, 1) + " dB");
            }
        }
        if (!recorded) s.check(plays == int(cases().size()), "every new instrument plays from MIDI in a slot" + tag, std::to_string(plays) + " of " + std::to_string(cases().size()));
        s.check(slotChanged == 0, "the slot holds the chosen instrument" + tag);
    }
    // The recordings matched to their stand-in (the same phrase), within 1.5 dB.
    if (ready) {
        for (const auto& c : cases()) {
            const auto& info = dsp::library::info(c.kind + dsp::library::originals);
            if (std::strlen(info.sets) == 0) continue;
            double db[2];
            for (const bool recorded : { true, false }) {
                BypassRecordings bypass(!recorded);
                auto t = libraryRig(c.kind);
                const auto y = t->render(9.5, recordedPhrase(t->time + 0.1));
                db[recorded ? 0 : 1] = windowDb(y, rate, 0.1, 9.5);
            }
            s.check(std::abs(db[0] - db[1]) <= 1.5, std::string(info.name) + ": the recordings level-matched to the stand-in (within 1.5 dB)", num(db[0] - db[1], 1) + " dB");
        }
    }
    // A change of instrument while notes sound: the old one lets go (a short fade), the new one plays; no clicks.
    {
        auto rig = slotRig(testSlot, 1);
        std::vector<Event> e;
        for (int n : { 48, 55, 60, 64 }) e.push_back({ rig->time + 0.1, on(n, 100) });
        auto first = rig->render(0.8, e);
        int clicks = 0, wrong = 0;
        for (int which = 6; which < 20; ++which) {
            rig->set(params::slotTypeId(testSlot).toRawUTF8(), float(which));
            std::vector<Event> next;
            for (int n : { 52, 59, 67 }) next.push_back({ rig->time + 0.02, on(n, 90) });
            for (int n : { 52, 59, 67 }) next.push_back({ rig->time + 0.5, off(n) });
            const auto y = rig->render(0.7, next);
            clicks += clicksIn(y, rate, 0.0);
            if (rig->p->playingIn(testSlot) != which) ++wrong;
        }
        s.check(wrong == 0, "changing a slot's instrument takes effect at once (fourteen changes while playing)");
        s.check(clicks == 0, "changing a slot's instrument while notes sound is click-free (the old one fades out)", std::to_string(clicks) + " clicks");
    }
    // Every slot can hold any instrument (each new one once in each slot): it plays, through that slot's chain.
    {
        int silent = 0;
        for (int slot = 0; slot < params::instrumentCount; ++slot) {
            const int which = 6 + (slot * 3) % 14;
            auto rig = slotRig(slot, which);
            const auto y = playNote(*rig, 62, 100, 0.8, 0.3);
            if (windowDb(y, rate, 0.05, 0.8) < -50.0 || rig->p->playingIn(slot) != which) ++silent;
        }
        s.check(silent == 0, "every slot can hold a new instrument (and plays it)", std::to_string(silent) + " silent");
    }
    // Distinct: every one of the fourteen from all nineteen others (features of one phrase and one note).
    {
        std::vector<Features> f;
        for (int which = 0; which < 20; ++which) f.push_back(featuresOf(which));
        double closest = 1e9;
        std::string closestPair;
        std::vector<std::tuple<double, int, int>> pairs;
        for (int a = 6; a < 20; ++a)
            for (int b = 0; b < 20; ++b) {
                if (b == a || (b >= 6 && b < a)) continue;
                const double d = distanceOf(f[std::size_t(a)], f[std::size_t(b)]);
                pairs.push_back({ d, a, b });
                if (d < closest) { closest = d; closestPair = std::string(params::libraryNames[a]) + " / " + params::libraryNames[b]; }
            }
        std::sort(pairs.begin(), pairs.end());
        for (std::size_t k = 0; k < std::min<std::size_t>(6, pairs.size()); ++k)
            std::cout << "  [distinct] " << params::libraryNames[std::get<1>(pairs[k])] << " / " << params::libraryNames[std::get<2>(pairs[k])] << ": " << num(std::get<0>(pairs[k]), 2) << "\n";
        s.metric("closest pair (feature distance)", closest);
        s.check(closest >= 1.5, "every new instrument is distinct from the other nineteen (closest pair's feature distance >= 1.5)", closestPair + ": " + num(closest, 2));
    }
    testLibraryPresets(s);
    // CPU: six new instruments at once (one in each slot), sixteen held notes and the pedal, dry.
    {
        Rig rig(rate);
        const int held[] { 6, 7, 13, 11, 9, 18 };   // guitar, strings, voice, vibraphone, toy piano, organ
        const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
        for (int i = 0; i < params::instrumentCount; ++i) {
            rig.set(ons[i], 1.0f);
            rig.set(params::slotTypeId(i).toRawUTF8(), float(held[i]));
        }
        dry(rig);
        std::vector<Event> e { { rig.time, pedal(true) } };
        for (int k = 0; k < 16; ++k) e.push_back({ rig.time + 0.01 * k, on(40 + 3 * k, 90) });
        const auto t0 = std::chrono::steady_clock::now();
        rig.render(10.0, e);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        s.metric("six new instruments x 16 held notes + pedal, CPU (one core, dry)", seconds / 10.0 * 100.0, " %");
        s.check(seconds / 10.0 < 0.6, "six new instruments with sixteen held notes each run well within real time (under 60 % of one core)", num(seconds * 10.0, 1) + " %");
    }
}

// The factory presets on the new instruments: at least twenty, each loads the instrument into its slot and plays.
void testLibraryPresets(Suite& s) {
    using namespace librarytest;
    int using_ = 0, silent = 0, wrongSlot = 0;
    std::set<int> kinds;
    for (const auto& preset : presets::factory()) {
        std::vector<std::pair<int, int>> held;   // slot, instrument
        for (const auto& [id, value] : preset.values)
            for (int slot = 0; slot < params::instrumentCount; ++slot)
                if (id == params::slotTypeId(slot) && value >= 6.0f) held.push_back({ slot, int(std::lround(value)) });
        if (held.empty()) continue;
        ++using_;
        Rig rig(rate);
        rig.p->applyPreset(preset);
        rig.render(0.1);
        const auto y = rig.render(4.0, recordedPhrase(rig.time + 0.1));
        for (const auto& [slot, type] : held) {
            kinds.insert(type);
            if (rig.p->playingIn(slot) != type) ++wrongSlot;
        }
        bool finite = true;
        for (float v : y.first) finite = finite && std::isfinite(v);
        if (!finite || windowDb(y, rate, 0.2, 4.0) < -50.0) {
            ++silent;
            std::cout << "  silent preset: " << preset.name << " " << num(windowDb(y, rate, 0.2, 4.0)) << " dB\n";
        }
    }
    s.check(using_ >= 20, "at least twenty factory presets use the new instruments", std::to_string(using_) + " presets");
    s.check(int(kinds.size()) == 14, "every new instrument has a factory preset", std::to_string(kinds.size()) + " of 14");
    s.check(wrongSlot == 0 && silent == 0, "each of them loads its instruments into their slots and plays (its place, its Character)",
            std::to_string(wrongSlot) + " wrong slots, " + std::to_string(silent) + " silent");
}

// The probe (developer aid): each new instrument's phrase level against the piano, and features.
int probeLibrary() {
    using namespace librarytest;
    recordingsReady();
    double pianoDb = 0;
    {
        auto rig = slotRig(0, 0);
        pianoDb = windowDb(rig->render(9.5, recordedPhrase(rig->time + 0.1)), rate, 0.1, 9.5);
    }
    for (const bool recorded : { true, false }) {
        BypassRecordings bypass(!recorded);
        for (const auto& c : cases()) {
            auto t = libraryRig(c.kind);
            const auto y = t->render(9.5, recordedPhrase(t->time + 0.1));
            std::cout << (recorded ? "rec  " : "syn  ") << dsp::library::info(c.kind + 6).name << ": " << num(windowDb(y, rate, 0.1, 9.5) - pianoDb, 1)
                      << " dB vs piano, peak " << num(toDb(std::max(*std::max_element(y.first.begin(), y.first.end()), -*std::min_element(y.first.begin(), y.first.end()))), 1)
                      << " dBFS, recorded=" << t->p->slotRecorded(testSlot) << "\n";
        }
    }
    return 0;
}

// Developer aid: where the click detector fires for one instrument (kind 0..13, recorded 0/1, or kind -1:
// the instrument-change sequence).
int probeLibraryClicks(int kind, bool recorded) {
    using namespace librarytest;
    recordingsReady();
    BypassRecordings bypass(!recorded);
    Signal y;
    if (kind >= 0) {
        auto t = libraryRig(kind);
        auto events = recordedPhrase(t->time + 0.1);
        const double at = t->time + 10.0;
        events.push_back({ at, pedal(true) });
        for (int n : { 48, 55, 60, 64, 67, 71 }) events.push_back({ at + 0.01 * (n % 3), on(n, 70 + n % 40) });
        for (int n : { 48, 55, 60, 64, 67, 71 }) events.push_back({ at + 2.5, off(n) });
        events.push_back({ at + 3.5, pedal(false) });
        y = t->render(16.0, events);
    } else {
        auto rig = slotRig(testSlot, 1);
        std::vector<Event> e;
        for (int n : { 48, 55, 60, 64 }) e.push_back({ rig->time + 0.1, on(n, 100) });
        y = rig->render(0.8, e);
        for (int which = 6; which < 20; ++which) {
            rig->set(params::slotTypeId(testSlot).toRawUTF8(), float(which));
            std::vector<Event> next;
            for (int n : { 52, 59, 67 }) next.push_back({ rig->time + 0.02, on(n, 90) });
            for (int n : { 52, 59, 67 }) next.push_back({ rig->time + 0.5, off(n) });
            const auto z = rig->render(0.7, next);
            y.first.insert(y.first.end(), z.first.begin(), z.first.end());
            y.second.insert(y.second.end(), z.second.begin(), z.second.end());
        }
    }
    for (const auto* ch : { &y.first, &y.second }) {
    const auto& v = *ch;
    std::cout << (ch == &y.first ? "left\n" : "right\n");
    std::vector<double> d(v.size(), 0.0);
    for (std::size_t i = 2; i < v.size(); ++i) d[i] = double(v[i]) - 2.0 * v[i - 1] + v[i - 2];
    const auto half = std::size_t(0.005 * rate);
    for (std::size_t i = half; i + half < v.size(); ++i) {
        double sum = 0;
        for (std::size_t k = i - half; k < i + half; ++k) sum += d[k] * d[k];
        const double local = std::sqrt(sum / double(2 * half));
        if (std::abs(d[i]) > 8.0 * local + 1.0e-7 && std::abs(d[i]) > 1.0e-4) {
            std::cout << "click at " << num(double(i) / rate, 4) << " s: d2 " << d[i] << " local " << local << " x " << v[i - 2] << " " << v[i - 1] << " " << v[i] << " " << v[i + 1] << "\n";
            i += half;
        }
    }
    }
    return 0;
}

// The rooms still matter most (the developer's rule since 1b): for each new instrument, its closest pair
// of places against its strongest control swept end to end (in the hospital), as the environments group
// measures the originals. Every pair of places must clearly differ; the control comparison is checked on
// the closest pair of places (enforced where it holds for the originals' measure, see DEVLOG).
void testLibraryRooms(Suite& s) {
    s.section("library: the rooms change every new instrument more than its controls");
    recordingsReady();
    using namespace librarytest;
    const char* placeNames[] { "hospital", "mall", "school", "garage", "pool", "hotel", "subway", "supermarket", "office", "impossible room" };
    int notDistinct = 0, controlWins = 0;
    double worstRatio = 1e9;
    std::string worst;
    for (const auto& c : cases()) {
        const int which = c.kind + 6;
        const auto render = [&](int place, const char* control, float normalised) {
            auto rig = rigIn(place);
            rig->set(params::ambience, params::minDb);
            rig->set(params::pianoOn, 0.0f);
            rig->set(params::synthOn, 1.0f);
            rig->set(params::slotTypeId(testSlot).toRawUTF8(), float(which));
            if (control != nullptr) rig->p->parameters.getParameter(control)->setValueNotifyingHost(normalised);
            rig->render(1.0);
            return characterOf(rig->render(7.0, standardPhrase(rig->time)), rate);
        };
        std::vector<Character> cs;
        for (int place = 0; place < dsp::placeCount; ++place) cs.push_back(render(place, nullptr, 0.0f));
        double nearest = 1e9, mean = 0;
        int pairs = 0;
        std::string pair;
        for (int a = 0; a < dsp::placeCount; ++a)
            for (int b = a + 1; b < dsp::placeCount; ++b) {
                const double d = characterDistance(cs[std::size_t(a)], cs[std::size_t(b)]);
                mean += d;
                ++pairs;
                if (d < nearest) { nearest = d; pair = std::string(placeNames[a]) + " / " + placeNames[b]; }
            }
        mean /= double(pairs);
        double strongest = 0;
        std::string control;
        for (int k = 0; k < params::lib::count; ++k) {
            const auto id = params::libId(testSlot, params::lib::keys[k]);
            const double d = characterDistance(render(0, id.toRawUTF8(), 0.0f), render(0, id.toRawUTF8(), 1.0f));
            if (d > strongest) { strongest = d; control = k == 5 ? dsp::library::info(which).colour : params::lib::names[k]; }
        }
        const std::string name = params::libraryNames[which];
        std::cout << "  [rooms] " << name << ": closest places " << pair << " " << num(nearest, 2) << ", mean " << num(mean, 2) << "; strongest control " << control << " "
                  << num(strongest, 2) << "\n";
        if (nearest <= 1.0) ++notDistinct;
        if (mean <= strongest) ++controlWins;
        if (mean / std::max(1e-9, strongest) < worstRatio) { worstRatio = mean / std::max(1e-9, strongest); worst = name + ": rooms " + num(mean, 2) + " vs " + control + " " + num(strongest, 2); }
        s.report(nearest > strongest, name + ": even the closest pair of places changes it more than its strongest control", pair + " " + num(nearest, 2) + " vs " + control + " " + num(strongest, 2));
    }
    s.check(notDistinct == 0, "every new instrument sounds different in every pair of places", std::to_string(notDistinct) + " not");
    s.check(controlWins == 0, "for every new instrument a change of place (on average) changes it more than any of its controls end to end", "closest margin: " + worst);
}

// Developer aid: every new instrument with each control at 0 and at 1: finite? level?
int probeExtremes() {
    using namespace librarytest;
    recordingsReady();
    for (const auto& c : cases())
        for (int k = 0; k < params::lib::count; ++k)
            for (const float v : { 0.0f, 1.0f }) {
                auto rig = libraryRig(c.kind);
                rig->p->parameters.getParameter(params::libId(testSlot, params::lib::keys[k]))->setValueNotifyingHost(v);
                const auto y = rig->render(7.0, standardPhrase(rig->time + 0.1));
                bool finite = true;
                for (float x : y.first) finite = finite && std::isfinite(x);
                if (!finite || windowDb(y, rate, 0.1, 7.0) < -60.0)
                    std::cout << params::libraryNames[c.kind + 6] << " " << params::lib::names[k] << "=" << v << ": finite " << finite << " level " << num(windowDb(y, rate, 0.1, 7.0)) << "\n";
            }
    return 0;
}
