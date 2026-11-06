// Milestone 8: the instruments on the developer's recordings (Source/Samples, DSP/Sampler.h).
// Included by LiminalTests.cpp inside its anonymous namespace, after the instrument tests.
// Every check runs dry at LIMINALITY 0 unless it says otherwise. Without the recordings the
// group checks only that every stand-in plays (a build without SAMPLES_TOKEN).


bool recordingsReady() {
    samples::Bank::get().load();
    // (2026-11-06: 8 s was too short on CI with every shard loading the bank at once: some groups
    // silently ran on the stand-ins. A build without recordings still returns at once.)
    return samples::Bank::get().waitUntilLoaded(60000);
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

// Clicks after `from` seconds: the liminality group's detector (a second difference > 8x its RMS
// over +/- 5 ms), counting only spikes above -80 dBFS (|d2| > 1e-4): a -97 dB tick of the room
// bed in silence (heard with the stand-ins too) is not a click anyone can hear.
int clicksIn(const Signal& x, double rate, double from) {
    int count = 0;
    for (const auto* ch : { &x.first, &x.second }) {
        const auto& v = *ch;
        std::vector<double> d(v.size(), 0.0);
        for (std::size_t i = 2; i < v.size(); ++i) d[i] = double(v[i]) - 2.0 * v[i - 1] + v[i - 2];
        const auto half = std::size_t(0.005 * rate);
        double sum = 0;
        std::size_t lo = 0, hi = 0;
        for (std::size_t i = std::max(std::size_t(from * rate), half); i + half < v.size(); ++i) {
            while (hi < i + half) { sum += d[hi] * d[hi]; ++hi; }
            while (lo < i - half) { sum -= d[lo] * d[lo]; ++lo; }
            const double local = std::sqrt(std::max(0.0, sum) / double(hi - lo));
            if (std::abs(d[i]) > 8.0 * local + 1.0e-7 && std::abs(d[i]) > 1.0e-4) { ++count; i += half; }
        }
    }
    return count;
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

    // 1b. They honour Liminality's per-note pitch (the hook in DSP/Instrument.h): +50 cents comes out
    // +50, measured over a long held stretch (whole loop cycles: a sung note's own vibrato averages out).
    {
        std::string wrong, all;
        dsp::ForgottenPiano piano;
        dsp::FadedSynth synth;
        dsp::EmptyChoir choir;
        dsp::LostMusicBox box;
        dsp::HollowDrone drone;
        dsp::Instrument* list[] { &piano, &synth, &choir, &box, &drone };
        const char* names[] { "piano", "synth", "choir", "box", "drone" };
        const double from[] { 0.4, 1.0, 1.5, 0.15, 2.0 }, to[] { 2.0, 7.5, 8.5, 1.2, 8.5 };
        for (int i = 0; i < 5; ++i) {
            const auto play = [&](float c) {
                list[i]->prepare(rate);
                list[i]->setLiminalPitch({ 0.0f, 0.0f, 0.0f });
                list[i]->shapeNextNote({ c, 0.0f, 7u });
                list[i]->noteOn(i == 3 ? 81 : 69, 0.8f);
                std::vector<float> l(std::size_t(rate * 9.0), 0.0f), r(l.size(), 0.0f);
                for (std::size_t k = 0; k < l.size(); k += 256) list[i]->render(l.data() + k, r.data() + k, int(std::min<std::size_t>(256, l.size() - k)));
                const Signal x { l, r };
                return double(c) + centsOff(x, rate, from[i], to[i], 440.0 * std::exp2(((i == 3 ? 81 : 69) - 69) / 12.0 + c / 1200.0));
            };
            double p0 = 0, p50 = 0;
            if (i == 2) {   // the voice: three notes averaged, singers together (Detune 0), no ghost
                // (before the remembered treatment, 2026-11-05: its colour leans the reading of a sung,
                // wandering partial by a few cents; the choir's tuning with it is checked in `remembered`)
                struct Untreated { Untreated() { dsp::Remembered::bypassedForTests() = true; } ~Untreated() { dsp::Remembered::bypassedForTests() = false; } } untreated;
                dsp::EmptyChoir::Settings cs;
                cs.detune = 0.0f;
                cs.breath = 0.0f;
                choir.setSettings(cs);
                for (const int n : { 57, 64, 69 }) {
                    const auto at = [&](float c) {
                        list[i]->prepare(rate);
                        list[i]->setLiminalPitch({ 0.0f, 0.0f, 0.0f });
                        list[i]->shapeNextNote({ c, 0.0f, 7u });
                        list[i]->noteOn(n, 0.8f);
                        std::vector<float> l(std::size_t(rate * 9.0), 0.0f), r(l.size(), 0.0f);
                        for (std::size_t k = 0; k < l.size(); k += 256) list[i]->render(l.data() + k, r.data() + k, int(std::min<std::size_t>(256, l.size() - k)));
                        return double(c) + centsOff(Signal { l, r }, rate, from[i], to[i], 440.0 * std::exp2((n - 69) / 12.0 + c / 1200.0));
                    };
                    p0 += at(0.0f) / 3.0;
                    p50 += at(50.0f) / 3.0;
                }
            } else {
                p0 = play(0.0f);
                p50 = play(50.0f);
            }
            const double shift = p50 - p0;
            all += std::string(names[i]) + " " + num(shift, 1) + " (" + num(p0, 1) + ")  ";
            // A sung note wanders by itself (+/- 15 cents) and its loop passes faster when raised: the
            // voice is measured to within 8 cents; the others, through the same reader, within 3.
            if (std::abs(shift - 50.0) > (i == 2 ? 8.0 : 3.0)) wrong += names[i];
        }
        s.check(wrong.empty(), "the recorded instruments apply a note's Liminality pitch (+50 cents -> +50 +/- 3; the voice +/- 8)", all);
    }

    // 2. Velocity layers rise smoothly (piano: three recorded dynamics crossfaded; music box: three).
    for (const auto* id : { params::pianoOn, params::boxOn }) {
        auto rig = soloRig(id);
        rig->set(params::pianoAge, 0.0f);
        rig->set(params::boxWear, 0.0f);
        double worstDrop = 0, worstStep = 0;
        std::string list, whereDrop;
        for (const int note : { 36, 48, 60, 67, 72, 84 }) {
            double last = -200, lastStep = -1;
            for (int vel = 12; vel <= 127; vel += 8) {
                const auto x = playNote(*rig, id == params::boxOn ? note + 12 : note, vel, 0.4, 0.3);
                const double lv = loudestDb(x, rate, 0.0, 0.4, 0.3);
                if (last > -199) {
                    if (last - lv > worstDrop) whereDrop = " (key " + std::to_string(note) + ", velocity " + std::to_string(vel) + ")";
                    worstDrop = std::max(worstDrop, last - lv);
                    // A layer crossfade never jumps: each step close to the one before it (the
                    // velocity curve itself is steep at the bottom, as velocity^1.6 is).
                    if (lastStep > -0.5) worstStep = std::max(worstStep, std::abs((lv - last) - lastStep));
                    lastStep = lv - last;
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
        s.check(worstStep <= 3.0, "velocity layers rise smoothly (no sudden step: each velocity step within 3 dB of the one before)", name + ": largest change " + num(worstStep, 2) + " dB");
    }

    {   // Every key of the piano gets louder with velocity (five velocities; a dynamic that barely
        // sounded in the recording is replaced by its neighbour).
        auto rig = soloRig(params::pianoOn, { { params::pianoAge, 0.0f }, { params::pianoNoise, 0.0f } });
        std::string bad;
        for (int key = 21; key <= 108; ++key) {
            double last = -200;
            for (const int vel : { 20, 45, 76, 100, 124 }) {
                const double lv = loudestDb(playNote(*rig, key, vel, 0.3, 0.05), rate, 0.0, 0.35, 0.3);
                if (lv < last - 1.0) bad += juce::MidiMessage::getMidiNoteName(key, true, true, 4).toStdString() + "@" + std::to_string(vel) + " ";
                last = std::max(last, lv);
                rig->render(0.15, { { rig->time, juce::MidiMessage::allSoundOff(1) } });
            }
        }
        s.check(bad.empty(), "piano: every one of the 88 keys gets louder with velocity (20 / 45 / 76 / 100 / 124)", bad.empty() ? "all 88" : bad);
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
        std::string where;
        for (int note = 60; note <= 100; note += 5) {
            const auto x = playNote(*rig, note, 90, 0.3, 4.0);
            const int n = clicksIn(x, rate, 0.03);
            if (n > 0) {
                std::vector<float> l(x.first);
                std::vector<double> d(l.size(), 0.0);
                for (std::size_t i = 2; i < l.size(); ++i) d[i] = double(l[i]) - 2.0 * l[i - 1] + l[i - 2];
                where += std::to_string(note) + " (" + std::to_string(n) + ") ";
            }
            total += n;
        }
        s.check(total == 0, "music box: no clicks as the tines ring out", std::to_string(total) + (where.empty() ? "" : " at " + where));
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
    BypassRecordings bypass(std::getenv("LIMINAL_PROBE_STANDIN") != nullptr);
    const double rate = 48000.0;
    const juce::String on = juce::String(instrument) + "_on";
    auto rig = soloRig(on.toRawUTF8(), { { params::pianoNoise, 0.0f }, { params::boxMechanism, 0.0f } });
    if (std::strcmp(instrument, "box") == 0)   // the recorded group's sequence before this note
        for (int n = 60; n < note; n += 5) playNote(*rig, n, velocity, 0.3, 4.0);
    const auto x = hold < 0 ? playNote(*rig, note, velocity, -hold, 2.5, true) : playNote(*rig, note, velocity, hold, std::strcmp(instrument, "box") == 0 ? 4.0 : 1.0);
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
            for (std::size_t k = i - 6; k < i + 6; ++k) std::cout << "    " << l[k] << "\n";
            i += half;
        }
    }
    return 0;
}

// The listening render (LiminalTests --render-recorded f.wav): the same piano phrase and chords on
// each instrument (Glass Memories, still synthesised, for comparison), dry; then in three rooms;
// then dry at LIMINALITY 0 / 50 / 100 %. A cue sheet beside it (f.txt).
int renderRecorded(const juce::String& path) {
    const double rate = 48000.0;
    recordingsReady();
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    struct Item { const char* name; const char* on; };
    const Item items[] { { "Forgotten Piano", params::pianoOn }, { "Empty Choir", params::choirOn }, { "Lost Music Box", params::boxOn },
                         { "Faded Synth (Soft Pad)", params::synthOn }, { "Hollow Drone", params::droneOn }, { "Glass Memories (synthesised)", params::glassOn } };
    struct Take { juce::String what; int place; float liminality; };
    const Take takes[] { { "dry", -1, 0.4f }, { "school", 2, 0.4f }, { "hotel", 5, 0.4f }, { "indoor pool", 4, 0.4f },
                         { "dry, LIMINALITY 0 %", -1, 0.0f }, { "dry, LIMINALITY 50 %", -1, 0.5f }, { "dry, LIMINALITY 100 %", -1, 1.0f } };
    std::vector<float> outL, outR;
    juce::String cues = "LIMINAL - Milestone 8 listening render: the developer's recordings.\n"
                        "Each instrument plays the same phrase (E4 G4 C5 B4 A4 G4 F4 E4) and chords (C, Am, F, G):\n"
                        "dry (no room, no ambience) at the default LIMINALITY 40 %, then in three rooms (default Character,\n"
                        "ambience at -12 dB), then dry at LIMINALITY 0, 50 and 100 %.\n"
                        + samples::Bank::get().message() + "\n\n";
    for (const auto& item : items)
        for (const auto& take : takes) {
            std::unique_ptr<Rig> rig;
            {
                AtLiminalityDefault keep;
                if (take.place < 0) rig = limtest::dryRig();
                else {
                    rig = rigIn(take.place);
                    rig->set(params::ambience, -12.0f);
                }
            }
            for (const char* id : { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn })
                rig->set(id, std::strcmp(id, item.on) == 0 ? 1.0f : 0.0f);
            rig->set(params::liminality, take.liminality);
            rig->render(0.3);
            const double at = double(outL.size()) / rate;
            const auto x = rig->render(11.0, recordedPhrase(rig->time + 0.2));
            outL.insert(outL.end(), x.first.begin(), x.first.end());
            outR.insert(outR.end(), x.second.begin(), x.second.end());
            outL.insert(outL.end(), std::size_t(rate), 0.0f);
            outR.insert(outR.end(), std::size_t(rate), 0.0f);
            cues << juce::String(int(at) / 60) << ":" << juce::String(int(at) % 60).paddedLeft('0', 2) << "  " << item.name << " - " << take.what << "\n";
        }
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(new juce::FileOutputStream(file));
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), rate, 2, 24, {}, 0));
    if (writer == nullptr) return 1;
    stream.release();
    const float* channels[] { outL.data(), outR.data() };
    writer->writeFromFloatArrays(channels, 2, int(outL.size()));
    writer.reset();
    file.withFileExtension("txt").replaceWithText(cues);
    std::cout << cues << "wrote " << file.getFullPathName() << "\n";
    return 0;
}

// Developer aid: each instrument's peak and RMS in the workflow group's 16-note pedalled chord.
int probeChord() {
    recordingsReady();
    for (const bool bypass : { false, true }) {
        BypassRecordings b(bypass);
        for (const char* id : { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn }) {
            Rig rig(48000.0);
            for (const char* o : { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn }) rig.set(o, std::strcmp(o, id) == 0 ? 1.0f : 0.0f);
            std::vector<Event> chord { { 0.05, pedal(true) } };
            for (int note = 48; note < 64; ++note) chord.push_back({ 0.05, on(note, 100) });
            const auto x = rig.render(10.0, chord);
            std::cout << (bypass ? "stand-in " : "recorded ") << id << ": peak " << peak(x) << " rms " << toDb(rms(x)) << " dB\n";
        }
        auto rig = soloRig(params::boxOn);
        for (const int n : { 48, 55, 60, 67, 72, 84 }) {
            const auto x = playNote(*rig, n, 100, 0.3, 3.0);
            std::cout << "  box " << n << ": peak " << peak(x) << " first 0.3 s " << windowDb(x, 48000.0, 0, 0.3) << " 1-3 s " << windowDb(x, 48000.0, 1, 3) << "\n";
            rig->render(0.1, { { rig->time, juce::MidiMessage::allSoundOff(1) } });
        }
    }
    return 0;
}

// Developer aid: the liminality group's macro-jump check, with the click times, per instrument alone.
int probeJump() {
    recordingsReady();
    BypassRecordings bypass(std::getenv("LIMINAL_PROBE_STANDIN") != nullptr);
    using namespace limtest;
    const double rate = 48000.0;
    for (int only = -1; only < params::instrumentCount; ++only) {
        auto rig = dryRig();
        allInstruments(*rig);
        if (only >= 0)
            for (int i = 0; i < params::instrumentCount; ++i) rig->set((juce::String(params::instrumentKeys[i]) + "_on").toRawUTF8(), i == only ? 1.0f : 0.0f);
        allLim(*rig, 0.4f);
        for (int i = 0; i < params::instrumentCount; ++i) setLim(*rig, i, dsp::lim::wear, 0.0f);
        if (const char* off = std::getenv("LIMINAL_PROBE_OFF"))
            for (int i = 0; i < params::instrumentCount; ++i) setLim(*rig, i, std::atoi(off), 0.0f);
        rig->render(0.1);
        std::vector<Event> chord { { rig->time + 0.01, on(57, 70) }, { rig->time + 0.01, on(64, 70) }, { rig->time + 0.01, pedal(true) } };
        rig->render(0.3, chord);
        std::vector<float> outL;
        for (int k = 0; k < 40; ++k) {
            rig->set(params::liminality, k % 2 == 0 ? 1.0f : 0.0f);
            const auto x = rig->render(0.1);
            outL.insert(outL.end(), x.first.begin(), x.first.end());
        }
        std::cout << (only < 0 ? "all" : params::instrumentKeys[only]) << ": " << clicks(outL, rate, 0) << "\n";
        {
            std::vector<double> d(outL.size(), 0.0);
            for (std::size_t i = 2; i < outL.size(); ++i) d[i] = double(outL[i]) - 2.0 * outL[i - 1] + outL[i - 2];
            const auto half = std::size_t(0.005 * rate);
            for (std::size_t i = half; i + half < outL.size(); ++i) {
                double sum = 0;
                for (std::size_t k = i - half; k < i + half; ++k) sum += d[k] * d[k];
                const double local = std::sqrt(sum / double(2 * half));
                if (std::abs(d[i]) > 8.0 * local + 1.0e-7) { std::cout << "   at " << double(i) / rate + 0.4 << " s after the chord, d2 " << d[i] << ", local " << local << ", level " << outL[i] << "\n"; i += half; }
            }
        }
    }
    return 0;
}
