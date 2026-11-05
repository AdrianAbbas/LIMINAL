// What the patch notes 2-4 and 7 ask to change, measured (2026-11-05): how much high-frequency energy
// an instrument has and how sharp its attacks are, at its defaults, dry, LIMINALITY 0 (the instrument
// itself), on the library's phrase (melody then chords). Included by LiminalTests.cpp after
// LibraryTests.h; it uses only what the tests had before the patch, so scripts/build-before.sh compiles
// it into the code before the patch too (`--probe-treatment` prints the same numbers there: the
// baseline the group `remembered` compares against).

namespace treatment {

struct Measure {
    double hfDb = 0;       // energy above 4 kHz against the whole (dB)
    double airDb = 0;      // above 8 kHz (the crispness of consonants, breath, a bow's or pick's edge)
    double attackDb = 0;   // attack: an isolated note's first 60 ms against its body (dB; lower = softer)
    double levelDb = 0;    // the phrase's level
};

Measure measure(int which) {
    auto rig = librarytest::rigFor(which);
    const double rate = librarytest::rate;
    const double t0 = rig->time + 0.1;
    const auto x = rig->render(9.5, recordedPhrase(t0));
    if (const char* dir = std::getenv("LIMINAL_WRITE_PROBE"))
        writeAudio(x, juce::File(dir).getChildFile("probe-" + juce::String(which) + ".wav"));
    Measure m;
    const Spectrum sp(monoOf(x, rate, 0.1, 9.5), rate, 1 << 18);
    const double total = sp.bandDb(30, 20000);
    m.hfDb = sp.bandDb(4000, 20000) - total;
    m.airDb = sp.bandDb(8000, 20000) - total;
    m.levelDb = windowDb(x, rate, 0.1, 9.5);
    // The attack, on three isolated notes (A3, E4, A4), each from silence in a fresh rig: how much of the
    // note is already there in its first 60 ms, against its body (200 ms - 1.2 s), in dB. A softer,
    // slower attack (or a pluck rounded off) is lower. The median of the three.
    std::vector<double> onsets;
    for (const int key : { 57, 64, 69 }) {
        auto one = librarytest::rigFor(which);
        const auto note = playNote(*one, key, 100, 1.2, 0.2);
        if (const char* dir = std::getenv("LIMINAL_WRITE_PROBE"))
            writeAudio(note, juce::File(dir).getChildFile("note-" + juce::String(which) + "-" + juce::String(key) + ".wav"));
        onsets.push_back(windowDb(note, rate, 0.0, 0.06) - windowDb(note, rate, 0.2, 1.2));
    }
    std::sort(onsets.begin(), onsets.end());
    m.attackDb = onsets[1];
    return m;
}

// The instruments the patch notes name (the slot of Hummed Memory, Distant Radio since the patch).
constexpr int named[] { 3, 13, 7, 8, 16, 5, 15, 14 };

} // namespace treatment

int probeTreatment() {
    recordingsReady();
    for (const int which : treatment::named) {
        const auto m = treatment::measure(which);
        std::cout << "probe " << which << " " << params::libraryNames[which] << ": hf " << juce::String(m.hfDb, 2) << " dB, air " << juce::String(m.airDb, 2)
                  << " dB, attack " << juce::String(m.attackDb, 2) << " dB, level " << juce::String(m.levelDb, 2) << " dB\n";
    }
    return 0;
}

int probeVoicePitch() {
    recordingsReady();
    const double rate = librarytest::rate;
    for (int note = 43; note <= 72; note += 3) {
        auto rig = librarytest::rigFor(13);
        const auto x = playNote(*rig, note, 90, 4.0, 0.5);
        const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        const Spectrum sp(monoOf(x, rate, 1.0, 3.8), rate, 1 << 18);
        std::cout << note << ":";
        for (int h = 1; h <= 4; ++h) {
            const auto pk = sp.peakNear(h * f0, 60.0);
            std::cout << "  h" << h << " " << juce::String(centsBetween(pk.first, h * f0), 1) << "c " << juce::String(20 * std::log10(pk.second + 1e-12), 0) << "dB";
        }
        std::cout << "\n";
    }
    return 0;
}
