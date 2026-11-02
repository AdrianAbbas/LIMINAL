// The instrument library's listening render (2026-11-02): `LiminalTests --render-library f.flac`.
// The same phrase (E4 G4 C5 B4 A4 G4 F4 E4, then C and Am) on all twenty instruments, each in the
// synth's slot (the originals in their own), dry and in three rooms (school, hotel, indoor pool;
// default Character, ambience at -12 dB), at the default LIMINALITY 40 %. A cue sheet (.txt) beside it.
// Included by LiminalTests.cpp after IconTests.h (it uses writeAudio).

int renderLibrary(const juce::String& path) {
    const double rate = 48000.0;
    recordingsReady();
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    const auto phrase = [](double t0) {
        std::vector<Event> e;
        const int melody[] { 64, 67, 72, 71, 69, 67, 65, 64 };
        for (int i = 0; i < 8; ++i) {
            e.push_back({ t0 + 0.4 * i, on(melody[i], 90) });
            e.push_back({ t0 + 0.4 * i + 0.36, off(melody[i]) });
        }
        const int chords[][3] { { 60, 64, 67 }, { 57, 60, 64 } };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k) {
                e.push_back({ t0 + 3.3 + 1.3 * c, on(chords[c][k], 80) });
                e.push_back({ t0 + 3.3 + 1.3 * c + 1.2, off(chords[c][k]) });
            }
        return e;
    };
    struct Take { const char* what; int place; };
    const Take takes[] { { "dry", -1 }, { "school", 2 }, { "hotel", 5 }, { "indoor pool", 4 } };
    Signal out;
    juce::String cues = "LIMINAL - the instrument library: the same phrase on all twenty instruments.\n"
                        "Melody E4 G4 C5 B4 A4 G4 F4 E4, then the chords C and Am; dry (no room, no ambience), then in the\n"
                        "school, the hotel and the indoor pool (default Character, ambience at -12 dB); LIMINALITY 40 % (the default).\n"
                        "The new instruments play in the synth's slot, the original six in their own.\n"
                        + samples::Bank::get().message() + "\n\n";
    for (int which = 0; which < params::libraryCount; ++which) {
        const int slot = which < 6 ? which : 1;
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
            const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
            for (int i = 0; i < params::instrumentCount; ++i) rig->set(ons[i], i == slot ? 1.0f : 0.0f);
            rig->set(params::slotTypeId(slot).toRawUTF8(), float(which));
            rig->render(0.3);
            const double at = double(out.first.size()) / rate;
            const auto x = rig->render(7.4, phrase(rig->time + 0.1));
            out.first.insert(out.first.end(), x.first.begin(), x.first.end());
            out.second.insert(out.second.end(), x.second.begin(), x.second.end());
            out.first.insert(out.first.end(), std::size_t(0.6 * rate), 0.0f);
            out.second.insert(out.second.end(), std::size_t(0.6 * rate), 0.0f);
            const bool recorded = rig->p->slotRecorded(slot);
            cues << juce::String(int(at) / 60) << ":" << juce::String(int(at) % 60).paddedLeft('0', 2) << "  " << params::libraryNames[which] << " - " << take.what
                 << (recorded ? "" : which == 2 || which == 10 || which == 11 || which == 12 || which >= 18 ? "  (synthesised)" : "  (stand-in: no recordings)") << "\n";
        }
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
