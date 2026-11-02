// Milestone 8: the instruments on the developer's recordings (Source/Samples, DSP/Sampler.h).
// Included by LiminalTests.cpp inside its anonymous namespace, after the instrument tests.
// Every check runs dry at LIMINALITY 0 unless it says otherwise. Without the recordings the
// group checks only that every stand-in plays (a build without SAMPLES_TOKEN).

struct BypassRecordings {   // the stand-ins (synthesis), as a build without the recordings
    explicit BypassRecordings(bool on = true) { samples::Bank::bypassForTests(on); }
    ~BypassRecordings() { samples::Bank::bypassForTests(false); }
};

bool recordingsReady() {
    samples::Bank::get().load();
    return samples::Bank::get().waitUntilLoaded(8000);
}

// Pitch of a note in cents from `expectHz`: what its first six harmonics agree on (each harmonic's
// peak within +/- 60 cents, weighted by its power), as tools/sample-prep measures the recordings
// (a synth patch's oscillators, a piano's lowest strings: the fundamental alone misleads).
double centsOff(const Signal& x, double rate, double from, double to, double expectHz) {
    const auto mono = monoOf(x, rate, from, to);
    Spectrum sp(mono, rate, 1 << 17);
    double num = 0, den = 0;
    for (int h = 1; h <= 6; ++h) {
        if (expectHz * h > 0.45 * rate) break;
        const auto [hz, mag] = sp.peakNear(expectHz * h, 60);
        const double w = mag * mag;
        num += w * centsBetween(hz / h, expectHz);
        den += w;
    }
    return den > 0 ? num / den : 0.0;
}

// Clicks after `from` seconds (second-difference spikes; the liminality group's detector).
int clicksIn(const Signal& x, double rate, double from) {
    return limtest::clicks(x.first, rate, std::size_t(from * rate)) + limtest::clicks(x.second, rate, std::size_t(from * rate));
}

// The loudest 300 ms window's level in [from, to].
double loudestDb(const Signal& x, double rate, double from, double to, double window = 0.3) {
    double best = -200;
    for (double t = from; t + window <= to; t += 0.05) best = std::max(best, windowDb(x, rate, t, t + window));
    return best;
}

// One phrase (the listening render's): melody then chords, for levels.
std::vector<Event> recordedPhrase(double t0) {
    std::vector<Event> e;
    const int melody[] { 64, 67, 72, 71, 69, 67, 65, 64 };
    for (int i = 0; i < 8; ++i) {
        e.push_back({ t0 + 0.5 * i, on(melody[i], 90) });
        e.push_back({ t0 + 0.5 * i + 0.45, off(melody[i]) });
    }
    const int chords[][3] { { 60, 64, 67 }, { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 } };
    for (int c = 0; c < 4; ++c)
        for (int k = 0; k < 3; ++k) {
            e.push_back({ t0 + 4.0 + 1.2 * c, on(chords[c][k], 80) });
            e.push_back({ t0 + 4.0 + 1.2 * c + 1.1, off(chords[c][k]) });
        }
    return e;
}

struct RecordedCase {
    const char* name;
    const char* onId;
    int low, high;   // the range checked (every 4 semitones)
    double hold;     // seconds held when measuring pitch
    std::vector<std::pair<const char*, float>> settings;
};

std::vector<RecordedCase> recordedCases() {
    return {
        { "piano", params::pianoOn, 21, 108, 1.2, { { params::pianoDetune, 0.0f }, { params::pianoAge, 0.0f } } },
        { "choir", params::choirOn, 40, 76, 2.5, { { params::choirDetune, 0.0f } } },
        { "music box", params::boxOn, 60, 100, 0.8, { { params::boxWear, 0.0f }, { params::boxDrift, 0.0f } } },
        { "synth (soft pad)", params::synthOn, 36, 84, 1.5, { { params::synthDetune, 0.0f }, { params::synthPatch, 1.0f } } },
        { "synth (pulse)", params::synthOn, 36, 84, 1.5, { { params::synthDetune, 0.0f }, { params::synthPatch, 2.0f } } },
        { "synth (square)", params::synthOn, 36, 84, 1.5, { { params::synthDetune, 0.0f }, { params::synthPatch, 3.0f } } },
        { "synth (cyberpunk)", params::synthOn, 36, 84, 1.5, { { params::synthDetune, 0.0f }, { params::synthPatch, 4.0f } } },
        { "synth (static bass)", params::synthOn, 28, 72, 1.5, { { params::synthDetune, 0.0f }, { params::synthPatch, 5.0f } } },
        { "drone", params::droneOn, 36, 72, 4.0, { { params::droneInstability, 0.0f } } },
    };
}

void testRecorded(Suite& s) {
    s.section("recorded (the developer's recordings)");
    const double rate = 48000.0;
    const bool ready = recordingsReady();
    std::cout << "  " << samples::Bank::get().message() << "\n";
    // Stand-ins (no recordings): every instrument still plays, in tune.
    {
        BypassRecordings bypass;
        for (const auto& c : recordedCases()) {
            auto rig = soloRig(c.onId);
            for (const auto& [id, v] : c.settings) rig->set(id, v);
            const int note = (c.low + c.high) / 2;
            const auto x = playNote(*rig, note, 100, c.hold, 0.5);
            s.check(windowDb(x, rate, 0.05, c.hold) > -60.0, std::string("stand-in plays without the recordings: ") + c.name,
                    num(windowDb(x, rate, 0.05, c.hold)) + " dB");
        }
    }
    if (!ready) {
        s.report(false, "recordings present (the rest of this group needs them)", samples::Bank::get().message().toStdString());
        return;
    }
    const auto* pianoSet = samples::Bank::get().instrument("piano");
    s.check(pianoSet != nullptr && samples::Bank::get().instrument("piano-long") != nullptr, "the piano's short and long recordings are loaded");

    // 1. In tune from MIDI across each instrument's range.
    for (const auto& c : recordedCases()) {
        auto rig = soloRig(c.onId);
        for (const auto& [id, v] : c.settings) rig->set(id, v);
        rig->render(0.1);
        double worst = 0;
        std::string list;
        for (int note = c.low; note <= c.high; note += 4) {
            const bool piano = std::strcmp(c.onId, params::pianoOn) == 0;
            const double stretch = piano && pianoSet != nullptr && pianoSet->hasStretch ? pianoSet->stretchCents[std::size_t(std::clamp(note, 0, 127))] : 0.0;
            const double expect = 440.0 * std::exp2((note - 69 + stretch / 100.0) / 12.0);
            const auto x = playNote(*rig, note, 96, c.hold, piano ? 0.6 : 0.8);
            const double from = piano ? 0.15 : c.hold * 0.4, to = c.hold;
            const double off = centsOff(x, rate, from, to, expect);
            worst = std::max(worst, std::abs(off));
            list += juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString() + " " + num(off, 0) + (piano ? " (stretch " + num(stretch, 0) + ")" : "") + ", ";
            rig->render(0.4);
        }
        std::cout << "  [cents] " << c.name << ": " << list << "\n";
        s.check(worst <= 25.0, std::string("in tune from MIDI across the range (within 25 cents): ") + c.name, "worst " + num(worst, 1) + " cents");
    }

    // 2. Velocity layers rise smoothly (piano: three recorded dynamics crossfaded; music box: three).
    for (const auto* id : { params::pianoOn, params::boxOn }) {
        auto rig = soloRig(id);
        rig->set(params::pianoAge, 0.0f);
        rig->set(params::boxWear, 0.0f);
        double worstDrop = 0, worstStep = 0;
        std::string list, whereDrop;
        for (const int note : { 36, 48, 60, 67, 72, 84 }) {
            double last = -200;
            for (int vel = 12; vel <= 127; vel += 8) {
                const auto x = playNote(*rig, id == params::boxOn ? note + 12 : note, vel, 0.4, 0.3);
                const double lv = loudestDb(x, rate, 0.0, 0.4, 0.3);
                if (last > -199) {
                    if (last - lv > worstDrop) whereDrop = " (key " + std::to_string(note) + ", velocity " + std::to_string(vel) + ")";
                    worstDrop = std::max(worstDrop, last - lv);
                    worstStep = std::max(worstStep, lv - last);
                }
                if (note == 60) list += num(lv, 1) + " ";
                last = lv;
                // Silence before the next one (a tine has no damper: it would still ring).
                rig->render(0.25, { { rig->time, juce::MidiMessage::allSoundOff(1) } });
            }
        }
        const std::string name = id == params::pianoOn ? "piano" : "music box";
        std::cout << "  [velocity] " << name << " C4 levels (vel 12..124): " << list << "\n";
        s.check(worstDrop <= 1.0, "velocity layers rise smoothly (never quieter for a harder hit): " + name, "largest drop " + num(worstDrop, 2) + " dB" + whereDrop);
        s.check(worstStep <= 6.0, "velocity layers rise smoothly (no jump between neighbouring velocities): " + name, "largest step " + num(worstStep, 2) + " dB");
    }

    // 3. No clicks: the piano's short->long handover and its natural-decay tail (held 4 s), at
    // note-off (damper), with the pedal; the looped instruments over 8 s and their release.
    {
        auto rig = soloRig(params::pianoOn, { { params::pianoNoise, 0.0f } });
        int total = 0;
        std::string where;
        for (int note = 24; note <= 104; note += 5)
            for (const int vel : { 40, 80, 120 }) {
                const auto x = playNote(*rig, note, vel, 4.0, 1.0);
                const int n = clicksIn(x, rate, 0.04);
                if (n > 0) where += std::to_string(note) + "/" + std::to_string(vel) + " ";
                total += n;
                rig->render(0.3);
            }
        s.check(total == 0, "piano: no clicks through the handover, the decay tail and the damper (17 keys x 3 velocities)",
                std::to_string(total) + (where.empty() ? "" : " at " + where));
        // Staccato (let go before the handover) and the pedal.
        total = 0;
        for (int note = 30; note <= 100; note += 7) {
            total += clicksIn(playNote(*rig, note, 90, 0.08, 0.6), rate, 0.04);
            total += clicksIn(playNote(*rig, note, 90, 0.5, 2.5, true), rate, 0.04);
        }
        s.check(total == 0, "piano: no clicks on staccato notes or with the sustain pedal", std::to_string(total));
    }
    for (const auto& c : recordedCases()) {
        if (std::strcmp(c.onId, params::pianoOn) == 0 || std::strcmp(c.onId, params::boxOn) == 0) continue;
        auto rig = soloRig(c.onId);
        for (const auto& [id, v] : c.settings) rig->set(id, v);
        const double t = rig->time;
        std::vector<Event> e;
        for (const int n : { 48, 55, 60, 64 }) { e.push_back({ t + 0.01, on(n, 90) }); e.push_back({ t + 8.0, off(n) }); }
        const auto x = rig->render(12.0, e);
        const int n = clicksIn(x, rate, 0.3);
        s.check(n == 0, std::string("no clicks at the loop points or the release (chord held 8 s): ") + c.name, std::to_string(n));
        // Steady while held: no dips from the loops.
        double lo = 1e9, hi = -1e9;
        for (double a = 2.0; a + 0.5 <= 7.5; a += 0.5) { const double d = windowDb(x, rate, a, a + 0.5); lo = std::min(lo, d); hi = std::max(hi, d); }
        s.report(hi - lo <= 6.0, std::string("held chord stays steady (2-7.5 s, 0.5 s windows within 6 dB): ") + c.name, num(hi - lo, 1) + " dB");
    }
    {   // music box: rings out, no click at the end
        auto rig = soloRig(params::boxOn, { { params::boxMechanism, 0.0f } });
        int total = 0;
        for (int note = 60; note <= 100; note += 5) total += clicksIn(playNote(*rig, note, 90, 0.3, 4.0), rate, 0.03);
        s.check(total == 0, "music box: no clicks as the tines ring out", std::to_string(total));
    }

    // 4. Level-matched: each recorded instrument within 1.5 dB of its stand-in (the standard phrase),
    // and with each other as the instruments group asks of the stand-ins (within 6 dB of the piano's
    // phrase; the slow pads, choir and drone, by a held chord).
    std::vector<std::pair<std::string, double>> levels, standLevels;
    for (const auto* id : { params::pianoOn, params::synthOn, params::choirOn, params::boxOn, params::droneOn }) {
        const bool pad = id == params::choirOn || id == params::droneOn;
        const auto measure = [&](bool bypass, bool held) {
            BypassRecordings b(bypass);
            auto rig = soloRig(id);
            if (held) {
                const double t = rig->time + 0.05;
                std::vector<Event> e;
                for (const int n : { 60, 64, 67 }) { e.push_back({ t, on(n, 80) }); e.push_back({ t + 4.0, off(n) }); }
                return windowDb(rig->render(4.5, e), rate, 2.0, 4.0);
            }
            return windowDb(rig->render(10.0, recordedPhrase(rig->time + 0.05)), rate, 0.0, 10.0);
        };
        const double rec = measure(false, false), stand = measure(true, false);
        levels.push_back({ id, pad ? measure(false, true) : rec });
        standLevels.push_back({ id, pad ? measure(true, true) : stand });
        std::cout << "  [level] " << id << ": recorded " << num(rec, 2) << " dB, stand-in " << num(stand, 2) << " dB\n";
        s.check(std::abs(rec - stand) <= 1.5, std::string("level-matched to its stand-in (within 1.5 dB): ") + id, num(rec - stand, 2) + " dB");
    }
    bool close = true;
    std::string list;
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const double rec = levels[i].second - levels[0].second, stand = standLevels[i].second - standLevels[0].second;
        close &= std::abs(rec) <= std::max(6.0, std::abs(stand) + 1.0);
        list += levels[i].first + " " + num(rec, 1) + " (stand-in " + num(stand, 1) + ")  ";
    }
    s.check(close, "the recorded instruments are level-matched with each other (within 6 dB of the piano, or no further than the stand-ins; pads held)", list);
}

// Developer aid: where a note's clicks are (LiminalTests --probe-clicks <note> <velocity> <hold>).
int probeClicks(const char* instrument, int note, int velocity, double hold) {
    recordingsReady();
    const double rate = 48000.0;
    const juce::String on = juce::String(instrument) + "_on";
    auto rig = soloRig(on.toRawUTF8(), { { params::pianoNoise, 0.0f } });
    const auto x = hold < 0 ? playNote(*rig, note, velocity, -hold, 2.5, true) : playNote(*rig, note, velocity, hold, 1.0);
    const auto& l = x.first;
    std::vector<double> d(l.size(), 0.0);
    for (std::size_t i = 2; i < l.size(); ++i) d[i] = double(l[i]) - 2.0 * l[i - 1] + l[i - 2];
    const auto half = std::size_t(0.005 * rate);
    for (std::size_t i = std::size_t(0.04 * rate); i + half < l.size(); ++i) {
        double sum = 0;
        for (std::size_t k = i - half; k < i + half; ++k) sum += d[k] * d[k];
        const double local = std::sqrt(sum / double(2 * half));
        if (std::abs(d[i]) > 8.0 * local + 1.0e-7) {
            std::cout << "click at " << double(i) / rate << " s, d2 " << d[i] << " local " << local << " level " << l[i] << "\n";
            i += half;
        }
    }
    return 0;
}
