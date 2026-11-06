// Real woodwinds (2026-11-06): the woodwind layer (DSP/Woodwind) and the two instruments that blend one
// in (Elevator Vibraphone + flute, Lounge E-Piano + clarinet). Included by LiminalTests.cpp after
// LoudnessTests.h. Group `woodwind`; `--probe-woodwind` prints the calibration (developer aid);
// `--render-woodwinds f.flac` is the listening render (before / after, dry and in three rooms).
// Every check runs with the recordings when they are there (CI: the CC0 woodwind samples) and again on
// the synthesised stand-ins.
//   in tune: every note of the layer across its range (cents report), alone and through the treatment;
//   no clicks: long held notes across several loop cycles, note-offs, the pedal, the phrase, chords, the
//   control moved while notes sound, a change of instrument;
//   level-matched: the instrument with its woodwind (at the presets' amounts and full) within 1.5 dB of
//   the instrument alone (BS.1770 loudness in the default room, the bed down, as the `loudness` group);
//   the stand-in within 1.5 dB of the recordings; no clipping;
//   distinct: each instrument with its woodwind from all twenty instruments at their defaults;
//   Liminality and the rooms still work on it (the layer detunes with Detune, wobbles with Wow, every
//   room changes it, levels kept).

namespace woodwindtest {

constexpr double rate = 48000.0;
constexpr int testSlot = 1;
struct Changed { int which; int kind; int voice; const char* name; const char* blend; };
const Changed changed[] {
    { dsp::library::originals + dsp::library::elevatorVibraphone, dsp::library::elevatorVibraphone, dsp::woodwind::flute, "Elevator Vibraphone", "flute" },
    { dsp::library::originals + dsp::library::loungeEPiano, dsp::library::loungeEPiano, dsp::woodwind::clarinet, "Lounge E-Piano", "clarinet" },
};
juce::String windId() { return params::libId(testSlot, params::wind::amount); }
juce::String windVoiceId() { return params::libId(testSlot, params::wind::voice); }

// The layer alone (an Instrument of its own), at LIMINALITY 0 (no wander), its treatment on or off.
struct Layer {
    dsp::Woodwind w;
    explicit Layer(int voice, bool recorder = false) {
        w.prepare(rate);
        w.setVoice(voice, recorder);
        w.setProfile(voice == dsp::woodwind::clarinet ? dsp::woodwind::lobbyClarinet() : dsp::woodwind::muzakFlute());
        w.setLiminalPitch({ 0.0f, 0.0f, 0.0f });
    }
    Signal render(double seconds, const std::vector<std::pair<double, std::pair<int, int>>>& notes) {   // (time, (note, velocity: 0 = off))
        const auto total = std::size_t(seconds * rate);
        Signal out { std::vector<float>(total, 0.0f), std::vector<float>(total, 0.0f) };
        std::size_t done = 0, next = 0;
        auto sorted = notes;
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.first < b.first; });
        while (done < total) {
            const int n = int(std::min<std::size_t>(256, total - done));
            while (next < sorted.size() && sorted[next].first * rate < double(done + std::size_t(n))) {
                const auto [note, vel] = sorted[next].second;
                if (vel > 0) w.noteOn(note, float(vel) / 127.0f);
                else w.noteOff(note);
                ++next;
            }
            w.render(out.first.data() + done, out.second.data() + done, n);
            done += std::size_t(n);
        }
        return out;
    }
};

// The instrument alone in the test slot (dry or in the default room), its woodwind at `amount`.
std::unique_ptr<Rig> rigWith(const Changed& c, float amount, bool dryRig = true, int voice = 0) {
    auto rig = std::make_unique<Rig>(rate);
    const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    for (int k = 0; k < 6; ++k) rig->set(ons[k], k == testSlot ? 1.0f : 0.0f);
    if (dryRig) dry(*rig);
    rig->set(params::ambience, params::minDb);
    rig->set(params::slotTypeId(testSlot).toRawUTF8(), float(c.which));
    rig->set(windId().toRawUTF8(), amount);
    rig->set(windVoiceId().toRawUTF8(), float(voice));
    rig->render(0.3);
    return rig;
}

double lufsOf(const Signal& y) { return loudnesstest::lufs(y, 0.0, double(y.first.size()) / rate); }

// The phrase's loudness in the default room (as the `loudness` group: the bed down).
double phraseLufs(const Changed& c, float amount, int place = -1, int character = -1) {
    auto rig = rigWith(c, amount, false);
    if (place >= 0) rig->set(params::environment, float(place));
    if (character >= 0) rig->set(params::character, float(character));
    if (place >= 0 || character >= 0) { rig->set(params::ambience, params::minDb); rig->render(1.0); }
    const auto y = rig->render(11.5, recordedPhrase(rig->time + 0.1));
    return lufsOf(y);
}

// The centre pitch of a held note in cents from `expectHz`, over [from, to]: the mean of short frames'
// pitches (60 ms frames every 30 ms, each read as `centsOff` does). Over whole cycles of the flute's soft
// vibrato (4.9 Hz, +/-9 cents) this is the pitch it sits on; one long spectrum of a vibrato tone is not
// (its upper harmonics' sidebands pull the peaks: up to 13 cents of error for a tone exactly in tune,
// none with the vibrato off - checked 2026-11-06).
double centrePitch(const Signal& y, double from, double to, double expectHz) {
    double sum = 0;
    int n = 0;
    for (double t = from; t + 0.06 <= to + 1e-9; t += 0.03) { sum += centsOff(y, rate, t, t + 0.06, expectHz); ++n; }
    return n > 0 ? sum / n : 0.0;
}

// Held notes across the layer's range, every 3 semitones: cents of each (the vibrato averages out).
std::vector<std::pair<int, double>> layerCents(int voice, bool treated) {
    dsp::Woodwind::bypassTreatmentForTests() = !treated;
    std::vector<std::pair<int, double>> out;
    Layer probe(voice);
    const int lo = int(std::ceil(probe.w.lowNote() + 2.0f)), hi = int(std::floor(probe.w.highNote() - 2.0f));
    for (int note = lo; note <= hi; note += 3) {
        Layer l(voice);
        const auto y = l.render(3.4, { { 0.0, { note, 90 } }, { 3.0, { note, 0 } } });
        out.push_back({ note, centrePitch(y, 1.2, 1.2 + 7.0 / 4.9, 440.0 * std::exp2((note - 69) / 12.0)) });
    }
    dsp::Woodwind::bypassTreatmentForTests() = false;
    return out;
}

} // namespace woodwindtest

void testWoodwind(Suite& s) {
    s.section("woodwind (real woodwinds: the flute and clarinet layers, Elevator Vibraphone, Lounge E-Piano)");
    using namespace woodwindtest;
    const bool ready = recordingsReady();
    const bool wwReady = ready && dsp::recordings("ww-flute") != nullptr && dsp::recordings("ww-clarinet") != nullptr;
    s.report(wwReady, "the woodwind recordings are loaded (ww-flute, ww-clarinet)", samples::Bank::get().message().toStdString());
    for (const bool recorded : { true, false }) {
        if (recorded && !wwReady) continue;
        BypassRecordings bypass(!recorded);
        const std::string tag = recorded ? " (recordings)" : " (stand-in)";
        for (const auto& c : changed) {
            const std::string name = std::string(c.name) + " + " + c.blend + tag;
            // The layer plays what it should.
            {
                Layer l(c.voice);
                s.check(l.w.recordedNow() == recorded, name + ": the layer plays " + (recorded ? "the recordings" : "its stand-in"));
            }
            // In tune: every note of its range, bare and through its treatment (LIMINALITY 0).
            for (const bool treated : { false, true }) {
                const auto list = layerCents(c.voice, treated);
                double worst = 0;
                std::string text;
                for (const auto& [note, cents] : list) {
                    worst = std::max(worst, std::abs(cents));
                    text += juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString() + " " + num(cents, 1) + ", ";
                }
                std::cout << "  [cents] " << c.blend << tag << (treated ? " treated" : " bare") << ": " << text << "\n";
                s.metric(std::string(c.blend) + tag + (treated ? " treated" : " bare") + ": worst cents", worst, " cents");
                s.check(worst <= 10.0, std::string(c.blend) + tag + (treated ? " through its treatment" : "") + ": every note in tune (within 10 cents)", "worst " + num(worst, 1) + " cents over " + std::to_string(list.size()) + " notes");
            }
            // Never clean or crisp: through its treatment the layer's top (4-16 kHz, against its whole) is far
            // below the bare sample's, and it is heard at a distance (its reflections ring on after the note).
            {
                const auto play = [&](bool treated) {
                    dsp::Woodwind::bypassTreatmentForTests() = !treated;
                    Layer l(c.voice);
                    auto y = l.render(3.0, { { 0.0, { 72, 100 } }, { 2.0, { 72, 0 } } });
                    dsp::Woodwind::bypassTreatmentForTests() = false;
                    return y;
                };
                const auto bare = play(false), treated = play(true);
                const auto top = [&](const Signal& y) {
                    const Spectrum sp(monoOf(y, rate, 0.3, 1.9), rate, 1 << 16);
                    return sp.bandDb(4000, 16000) - sp.bandDb(60, 16000);
                };
                const double darker = top(treated) - top(bare);
                std::cout << "  [top] " << name << ": bare " << num(top(bare), 1) << " dB, treated " << num(top(treated), 1) << " dB\n";
                s.check(top(treated) <= -55.0 && darker <= -6.0, name + ": never clean or crisp - through its treatment almost nothing above 4 kHz (under -55 dB of the whole, at least 6 dB under the bare sample)",
                        "treated " + num(top(treated), 1) + " dB, " + num(darker, 1) + " dB against the bare sample");
            }
            // Through the slot: the instrument with its woodwind follows MIDI pitch (the layer's notes
            // fold into its range by octaves) - A4 held: the instrument and the layer agree.
            {
                auto rig = rigWith(c, 1.0f);
                const auto y = playNote(*rig, 69, 96, 2.5, 0.5);
                const double off = centsOff(y, rate, 1.2, 2.4, 440.0);
                s.check(std::abs(off) <= 10.0, name + ": in tune through the slot (A4 held, the instrument and its woodwind)", num(off, 1) + " cents");
            }
            // The blend is there: the held part after the instrument has decayed carries the woodwind.
            {
                auto a = rigWith(c, 0.0f), b = rigWith(c, 1.0f);
                const auto ya = playNote(*a, 72, 90, 4.0, 0.5), yb = playNote(*b, 72, 90, 4.0, 0.5);
                const double held = windowDb(yb, rate, 2.5, 4.0) - windowDb(ya, rate, 2.5, 4.0);
                s.check(held >= 6.0, name + ": the woodwind is clearly heard (a held note 2.5-4 s on, against the instrument alone)", "+" + num(held, 1) + " dB");
            }
            // At 0 the instrument is exactly as before: no layer voices start.
            {
                auto rig = rigWith(c, 0.0f);
                rig->render(9.5, recordedPhrase(rig->time + 0.1));
                const auto* lib = rig->p->libraryIn(testSlot);
                s.check(lib != nullptr && lib->woodwindLayer().activeVoices() == 0, name + ": at 0 no woodwind voice ever starts (the instrument alone, exactly as before)");
            }
            // No clicks: long held notes (several loop cycles of the recordings), across the range.
            {
                auto rig = rigWith(c, 1.0f);
                std::vector<Event> e;
                const double t0 = rig->time + 0.1;
                const int notes[] { 50, 57, 62, 69, 74, 81, 88 };
                for (int k = 0; k < 7; ++k) { e.push_back({ t0 + 0.05 * k, on(notes[k], 70 + 7 * k) }); e.push_back({ t0 + 20.0 + 0.3 * k, off(notes[k]) }); }
                const auto y = rig->render(24.0, e);
                // (from 0.5 s: the seven bars' / tines' own strikes at the start are the instrument's onset,
                // the same without the woodwind; what is checked is the loops and the note-offs)
                const int clicks = clicksIn(y, rate, 0.5);
                s.check(clicks == 0, name + ": no clicks while seven notes are held 20 s (loops) and let go", std::to_string(clicks) + " clicks");
                s.check(finite(y), name + ": finite and unclipped (seven held notes)", num(toDb(peak(y)), 1) + " dBFS");
            }
            // No clicks: the phrase, chords with the pedal, quick notes, the control moved while playing.
            {
                auto rig = rigWith(c, 1.0f);
                auto e = recordedPhrase(rig->time + 0.1);
                const double at = rig->time + 10.0;
                e.push_back({ at, pedal(true) });
                for (int n : { 48, 55, 60, 64, 67, 71 }) e.push_back({ at + 0.01 * (n % 3), on(n, 70 + n % 40) });
                for (int n : { 48, 55, 60, 64, 67, 71 }) e.push_back({ at + 2.5, off(n) });
                e.push_back({ at + 3.5, pedal(false) });
                for (int k = 0; k < 24; ++k) { e.push_back({ at + 5.0 + 0.11 * k, on(60 + (k * 5) % 19, 100) }); e.push_back({ at + 5.0 + 0.11 * k + 0.06, off(60 + (k * 5) % 19) }); }
                const auto y = rig->render(19.0, e);
                int clicks = clicksIn(y, rate, 0.05);
                // The control moved while notes sound (0 -> 1 -> 0.3 -> 0 in steps, as automation).
                std::vector<Event> held;
                for (int n : { 60, 64, 67 }) held.push_back({ rig->time + 0.02, on(n, 90) });
                for (int n : { 60, 64, 67 }) held.push_back({ rig->time + 3.9, off(n) });
                rig->set(windId().toRawUTF8(), 0.0f);
                Signal moved;
                for (int k = 0; k < 8; ++k) {
                    if (k > 0) rig->set(windId().toRawUTF8(), std::array<float, 8> { 0, 1, 0.5f, 1, 0.3f, 0, 0.8f, 0 }[std::size_t(k)]);
                    const auto part = rig->render(0.5, k == 0 ? held : std::vector<Event> {});
                    moved.first.insert(moved.first.end(), part.first.begin(), part.first.end());
                    moved.second.insert(moved.second.end(), part.second.begin(), part.second.end());
                }
                clicks += clicksIn(moved, rate, 0.05);
                s.check(clicks == 0, name + ": no clicks (phrase, chords and pedal, quick notes, the control moved while notes sound)", std::to_string(clicks) + " clicks");
                rig->render(3.0);
                s.check(rig->p->libraryIn(testSlot)->woodwindLayer().heldVoices() == 0, name + ": no hung woodwind notes after all that");
            }
            // Level-matched (realistic loudness: the phrase in the default room, BS.1770), at the presets'
            // amount and at full, against the instrument alone; no clipping when played hard.
            {
                const double alone = phraseLufs(c, 0.0f);
                for (const float amount : { 0.6f, 1.0f }) {
                    const double d = phraseLufs(c, amount) - alone;
                    s.metric(name + ": loudness with the woodwind at " + num(amount * 100.0, 0) + " % vs alone", d, " LU");
                    s.check(std::abs(d) <= 1.5, name + ": level-matched with the woodwind at " + num(amount * 100.0, 0) + " % (within 1.5 LU of the instrument alone, default room)", num(d, 2) + " LU");
                }
                auto rig = rigWith(c, 1.0f, false);
                std::vector<Event> e;
                const double t0 = rig->time + 0.1;
                e.push_back({ t0, pedal(true) });
                const int chords[][6] { { 36, 48, 55, 60, 64, 67 }, { 41, 53, 60, 65, 69, 72 } };
                for (int k = 0; k < 2; ++k)
                    for (int n : chords[k]) { e.push_back({ t0 + 1.5 * k, on(n, 127) }); e.push_back({ t0 + 1.5 * k + 1.4, off(n) }); }
                e.push_back({ t0 + 3.2, pedal(false) });
                const auto y = rig->render(6.0, e);
                s.check(peak(y) < 1.0f, name + ": no clipping played hard (velocity 127, six-note chords, pedal)", num(toDb(peak(y)), 1) + " dBFS");
            }
        }
    }
    // The stand-in level-matched to the recordings (the instrument with its woodwind at full).
    if (wwReady)
        for (const auto& c : changed) {
            double l[2];
            for (const bool recorded : { true, false }) {
                BypassRecordings bypass(!recorded);
                l[recorded ? 0 : 1] = phraseLufs(c, 1.0f);
            }
            s.check(std::abs(l[0] - l[1]) <= 1.5, std::string(c.name) + " + " + c.blend + ": the stand-in level-matched to the recordings (within 1.5 LU)", num(l[1] - l[0], 2) + " LU");
        }
    // Distinct: each instrument with its woodwind from all twenty at their defaults (the library group's features).
    {
        std::vector<librarytest::Features> all;
        for (int which = 0; which < 20; ++which) all.push_back(librarytest::featuresOf(which));
        for (const auto& c : changed) {
            const auto f = librarytest::featuresOf(c.which, 1.0f);
            double closest = 1e9;
            std::string other;
            for (int which = 0; which < 20; ++which) {
                if (which == c.which) continue;
                const double d = librarytest::distanceOf(f, all[std::size_t(which)]);
                if (d < closest) { closest = d; other = params::libraryNames[which]; }
            }
            const double self = librarytest::distanceOf(f, all[std::size_t(c.which)]);
            std::cout << "  [distinct] " << c.name << " + " << c.blend << ": closest " << other << " " << num(closest, 2) << " (itself without: " << num(self, 2) << ")\n";
            s.check(closest >= 1.5, std::string(c.name) + " + " + c.blend + ": still distinct from the other nineteen (feature distance >= 1.5)", other + ": " + num(closest, 2));
        }
    }
    // Liminality reaches the layer: Detune's note shape and the instrument-wide Wow move its pitch; at
    // LIMINALITY 100 % the instrument with its woodwind is finite, click-free, level kept.
    for (const auto& c : changed) {
        const std::string name = std::string(c.name) + " + " + c.blend;
        {
            dsp::Woodwind::bypassTreatmentForTests() = true;
            Layer a(c.voice), b(c.voice);
            b.w.shapeNextNote({ 30.0f, 0.0f, 7u });
            const auto ya = a.render(2.5, { { 0.0, { 69, 90 } } });
            const auto yb = b.render(2.5, { { 0.0, { 69, 90 } } });
            const double moved = centsOff(yb, rate, 0.9, 2.4, 440.0) - centsOff(ya, rate, 0.9, 2.4, 440.0);
            s.check(std::abs(moved - 30.0) <= 4.0, name + ": Liminality's note shape detunes the woodwind (+30 cents asked)", num(moved, 1) + " cents");
            Layer w(c.voice);
            w.w.setLiminalPitch({ -25.0f, 0.0f, 1.0f });
            const auto yw = w.render(2.5, { { 0.0, { 69, 90 } } });
            const double drift = centsOff(yw, rate, 0.9, 2.4, 440.0) - centsOff(ya, rate, 0.9, 2.4, 440.0);
            s.check(std::abs(drift + 25.0) <= 4.0, name + ": Liminality's drift / wow moves the woodwind (-25 cents asked)", num(drift, 1) + " cents");
            dsp::Woodwind::bypassTreatmentForTests() = false;
        }
        {
            double lufs[2];
            Signal ys[2];
            for (int k = 0; k < 2; ++k) {
                auto rig = rigWith(c, 1.0f, false);
                rig->set(params::liminality, k == 0 ? 0.0f : 1.0f);
                rig->render(0.5);
                ys[k] = rig->render(11.5, recordedPhrase(rig->time + 0.1));
                lufs[k] = lufsOf(ys[k]);
            }
            const auto a = monoOf(ys[0], rate, 0.5, 10.0), b = monoOf(ys[1], rate, 0.5, 10.0);
            double diff = 0, ref = 0;
            for (std::size_t i = 0; i < a.size(); ++i) { diff += double(a[i] - b[i]) * (a[i] - b[i]); ref += double(a[i]) * a[i]; }
            const double change = 10.0 * std::log10(std::max(1e-12, diff / std::max(1e-12, ref)));
            s.check(finite(ys[1]) && clicksIn(ys[1], rate, 0.05) == 0, name + ": LIMINALITY 100 % finite and click-free");
            s.check(change > -6.0, name + ": LIMINALITY 100 % clearly changes it (residual against 0 %)", num(change, 1) + " dB");
            s.check(std::abs(lufs[1] - lufs[0]) <= 3.0, name + ": LIMINALITY 100 % keeps the level (within 3 LU)", num(lufs[1] - lufs[0], 2) + " LU");
        }
        // The rooms: three places, each changes it, the woodwind's blend keeps the level in each.
        for (const int place : { int(dsp::Place::school), int(dsp::Place::hotel), int(dsp::Place::impossible) }) {
            const double alone = phraseLufs(c, 0.0f, place), with = phraseLufs(c, 1.0f, place);
            s.check(std::abs(with - alone) <= 1.5, name + " in " + dsp::profile(dsp::Place(place)).name + ": level-matched (within 1.5 LU of the instrument alone)", num(with - alone, 2) + " LU");
        }
    }
    // The Recorder voice: until the developer's alto recorder is recorded it plays the flute (for both
    // instruments); once recorded, the recorder.
    {
        auto rig = rigWith(changed[1], 1.0f, true, 1);
        const auto y = playNote(*rig, 72, 96, 2.0, 0.5);
        const auto* lib = rig->p->libraryIn(testSlot);
        const bool rec = lib != nullptr && lib->woodwindLayer().recorderRecorded();
        s.check(lib != nullptr && lib->woodwindLayer().voice() == dsp::woodwind::clarinet && windowDb(y, rate, 0.5, 2.0) > -50.0,
                std::string("the Recorder voice plays (") + (rec ? "the developer's alto recorder" : "not recorded yet: the flute") + ")");
    }
    // A preset with the woodwind loads and plays it.
    {
        int found = 0;
        for (const auto& preset : presets::factory())
            for (const auto& [id, value] : preset.values)
                if (id.endsWith(juce::String("_lib_") + params::wind::amount) && value > 0.0f) { ++found; break; }
        s.check(found >= 2, "factory presets use the woodwind blends (MUZAK WITH FLUTE, LOBBY CLARINET)", std::to_string(found) + " presets");
    }
}

// The calibration (developer aid): the layer alone vs the instrument alone, and the stand-in vs the
// recordings, on the phrase in the default room.
int probeWoodwind() {
    using namespace woodwindtest;
    recordingsReady();
    if (std::getenv("WW_CALIBRATE")) {   // --calibrate-woodwind: the table for Source/DSP/WoodwindLevelTable.h
        dsp::levelmatch::windBypassed() = true;
        std::string out = "// Generated by `LiminalTests --calibrate-woodwind` (Source/DSP/LevelMatch.h, windTrimDb). Do not edit by hand.\n"
                          "// dB: [Elevator Vibraphone + flute, Lounge E-Piano + clarinet] x place (dsp::Place order) x Character.\n"
                          "inline constexpr float windTable[2][placeCount][characterCount] {\n";
        for (const auto& c : changed) {
            out += "    { ";
            for (int place = 0; place < dsp::placeCount; ++place) {
                out += "{ ";
                for (int ch = 0; ch < dsp::characterCount; ++ch) {
                    const double d = phraseLufs(c, 0.0f, place, ch) - phraseLufs(c, 1.0f, place, ch);
                    out += juce::String(d, 2).toStdString() + "f" + (ch + 1 < dsp::characterCount ? ", " : " ");
                }
                out += place + 1 < dsp::placeCount ? "}, " : "} ";
            }
            out += "},\n";
        }
        out += "};\n";
        dsp::levelmatch::windBypassed() = false;
        std::cout << out;
        return 0;
    }
    if (std::getenv("WW_PLACES")) {
        for (const auto& c : changed)
            for (int place = 0; place < dsp::placeCount; ++place) {
                const double a = phraseLufs(c, 0.0f, place), b = phraseLufs(c, 1.0f, place);
                std::cout << c.name << " in " << dsp::profile(dsp::Place(place)).name << ": alone " << num(a, 2) << " with " << num(b - a, 2) << " LU\n";
            }
        return 0;
    }
    if (std::getenv("WW_CLICKS")) {
        for (const auto& c : changed)
            for (const float amount : { 0.0f, 1.0f }) {
                auto rig = rigWith(c, amount);
                std::vector<Event> e;
                const double t0 = rig->time + 0.1;
                const int notes[] { 50, 57, 62, 69, 74, 81, 88 };
                for (int k = 0; k < 7; ++k) { e.push_back({ t0 + 0.05 * k, on(notes[k], 70 + 7 * k) }); e.push_back({ t0 + 20.0 + 0.3 * k, off(notes[k]) }); }
                const auto y = rig->render(24.0, e);
                // where: the detector, printing times
                for (const auto* ch : { &y.first }) {
                    const auto& v = *ch;
                    std::vector<double> d(v.size(), 0.0);
                    for (std::size_t i = 2; i < v.size(); ++i) d[i] = double(v[i]) - 2.0 * v[i - 1] + v[i - 2];
                    const auto half = std::size_t(0.005 * rate);
                    double sum = 0;
                    std::size_t lo = 0, hi = 0;
                    for (std::size_t i = half; i + half < v.size(); ++i) {
                        while (hi < i + half) { sum += d[hi] * d[hi]; ++hi; }
                        while (lo < i - half) { sum -= d[lo] * d[lo]; ++lo; }
                        const double local = std::sqrt(std::max(0.0, sum) / double(hi - lo));
                        if (std::abs(d[i]) > 8.0 * local + 1.0e-7 && std::abs(d[i]) > 1.0e-4) { std::cout << c.name << " amount " << amount << ": click at " << num(double(i) / rate - t0 + rig->time - 24.0, 3) << " s (" << num(std::abs(d[i]) / local, 1) << "x, d2 " << d[i] << ")\n"; i += half; }
                    }
                }
                std::cout << c.name << " amount " << amount << ": " << clicksIn(y, rate, 0.05) << " clicks\n";
            }
        return 0;
    }
    for (const auto& c : changed) {
        for (const bool recorded : { true, false }) {
            BypassRecordings bypass(!recorded);
            const double alone = phraseLufs(c, 0.0f), full = phraseLufs(c, 1.0f), six = phraseLufs(c, 0.6f);
            std::cout << c.name << (recorded ? " recorded" : " stand-in") << ": alone " << num(alone, 2) << " LUFS, wind 60 % " << num(six - alone, 2)
                      << " LU, wind 100 % " << num(full - alone, 2) << " LU\n";
        }
    }
    return 0;
}
