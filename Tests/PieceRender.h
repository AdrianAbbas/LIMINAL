// The four-part test piece (2026-11-05), a listening render before and after the patch notes.
// Included by LiminalTests.cpp after LibraryRender.h. `--render-piece file.flac`: the piece on
// Faded Synth, Empty Choir (in the hospital), Solo Voice, Faded Strings, Hallway Pizzicato, Arcade
// Lead and the instrument in the old Hummed Memory slot (Distant Radio since this patch), each at the
// product defaults (LIMINALITY 40 %), in the default place unless named, ambience at -12 dB.
//   1. Pattern   (0-8 s)   sixteenth-note arpeggios over Am F C G, staccato and legato in turn
//   2. Chorale   (8-16 s)  four-part chords, two seconds each, with the pedal
//   3. Melody    (16-26 s) a slow legato melody (each note overlapping the next by 30 ms)
//   4. Repeats   (26-34 s) a held low drone under repeated eighth notes and a fast run
// The same file is written by the code before the patch (scripts/ci-render-before.sh in CI), so the
// two can be compared piece for piece.

std::vector<Event> fourPartPiece(double t0) {
    std::vector<Event> e;
    const auto note = [&](double at, double length, int key, int velocity) {
        e.push_back({ t0 + at, on(key, velocity) });
        e.push_back({ t0 + at + length, off(key) });
    };
    // 1. Pattern.
    const int roots[] { 57, 53, 48, 55 };
    const int minor[] { 57, 53, 48, 55 };
    for (int bar = 0; bar < 4; ++bar) {
        const int r = roots[bar];
        const int third = r == minor[0] ? 3 : 4;
        const int shape[] { 0, third, 7, 12, 7, third, 0, 12 };
        for (int i = 0; i < 16; ++i) {
            const double at = bar * 2.0 + i * 0.125;
            const double length = bar % 2 == 0 ? 0.05 : 0.14;   // staccato, then legato
            note(at, length, r + 12 + shape[i % 8], 70 + (i % 4 == 0 ? 25 : 0));
        }
    }
    // 2. Chorale (pedal down through each chord, changed at each).
    const int chords[][4] { { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 48, 55, 64, 67 }, { 43, 55, 62, 71 } };
    for (int c = 0; c < 4; ++c) {
        const double at = 8.0 + 2.0 * c;
        e.push_back({ t0 + at - 0.02, pedal(false) });
        e.push_back({ t0 + at + 0.05, pedal(true) });
        for (int k = 0; k < 4; ++k) note(at, 1.7, chords[c][k], 72 + 6 * k);
    }
    e.push_back({ t0 + 15.9, pedal(false) });
    // 3. Melody.
    const std::pair<int, double> melody[] { { 64, 1.0 }, { 67, 0.5 }, { 69, 0.5 }, { 72, 1.5 }, { 71, 0.5 }, { 69, 1.0 }, { 67, 1.0 },
                                            { 65, 0.5 }, { 64, 0.5 }, { 62, 1.0 }, { 64, 2.0 } };
    double t = 16.0;
    for (const auto& [key, beats] : melody) {
        note(t, beats * 0.9 + 0.03, key, 80);
        t += beats * 0.9;
    }
    // 4. Repeats over a drone.
    note(26.0, 7.5, 48, 70);
    for (int i = 0; i < 8; ++i) note(26.0 + 0.25 * i, 0.12, 64, 85);
    for (int i = 0; i < 8; ++i) note(28.0 + 0.25 * i, 0.2, i % 2 == 0 ? 67 : 64, 80);
    const int run[] { 60, 62, 64, 65, 67, 69, 71, 72, 74, 72, 71, 69, 67, 65, 64, 62, 60 };
    for (int i = 0; i < 17; ++i) note(30.5 + 0.09 * i, 0.08, run[i], 75);
    note(32.2, 1.3, 60, 80);
    note(32.2, 1.3, 64, 70);
    return e;
}

int renderPiece(const juce::String& path) {
    const double rate = 48000.0;
    recordingsReady();
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    struct Take { int which; int place; const char* what; };
    const Take takes[] { { 1, -1, "Faded Synth" }, { 3, 0, "Empty Choir, in the hospital" }, { 13, -1, "Solo Voice" }, { 7, -1, "Faded Strings" },
                         { 8, -1, "Hallway Pizzicato" }, { 16, -1, "Arcade Lead" }, { 14, -1, "the Hummed Memory slot" } };
    Signal out;
    juce::String cues = "LIMINAL - the four-part test piece (pattern, chorale, melody, repeats over a drone).\n"
                        "Product defaults (LIMINALITY 40 %), ambience -12 dB, the default place unless named.\n"
                        + samples::Bank::get().message() + "\n\n";
    for (const auto& take : takes) {
        std::unique_ptr<Rig> rig;
        {
            AtLiminalityDefault keep;
            rig = std::make_unique<Rig>(rate);
        }
        if (take.place >= 0) {
            auto fresh = rigIn(take.place);
            rig.swap(fresh);
            rig->set(params::liminality, 0.4f);
        }
        rig->set(params::ambience, -12.0f);
        const int slot = take.which < 6 ? take.which : 1;
        const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
        for (int i = 0; i < params::instrumentCount; ++i) rig->set(ons[i], i == slot ? 1.0f : 0.0f);
        rig->set(params::slotTypeId(slot).toRawUTF8(), float(take.which));
        rig->render(0.5);
        const double at = double(out.first.size()) / rate;
        const auto x = rig->render(38.0, fourPartPiece(rig->time + 0.2));
        out.first.insert(out.first.end(), x.first.begin(), x.first.end());
        out.second.insert(out.second.end(), x.second.begin(), x.second.end());
        out.first.insert(out.first.end(), std::size_t(1.0 * rate), 0.0f);
        out.second.insert(out.second.end(), std::size_t(1.0 * rate), 0.0f);
        cues << juce::String(int(at) / 60) << ":" << juce::String(int(at) % 60).paddedLeft('0', 2) << "  " << params::libraryNames[take.which]
             << " (" << take.what << ")" << (rig->p->slotRecorded(slot) ? "" : "  (stand-in: no recordings)") << "\n";
    }
    double peak = 0;
    for (const auto* ch : { &out.first, &out.second })
        for (float v : *ch) peak = std::max(peak, double(std::abs(v)));
    cues << "\npeak " << juce::String(toDb(peak), 1) << " dBFS\n";
    if (!writeAudio(out, file)) return 1;
    file.withFileExtension("txt").replaceWithText(cues);
    std::cout << cues;
    return 0;
}
