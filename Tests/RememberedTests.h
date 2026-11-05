// The remembered treatment (2026-11-05, patch notes 2-4, 6, 7; group `remembered`). Included by
// LiminalTests.cpp after TreatmentProbe.h. With the recordings (as in CI):
//   - Empty Choir, Solo Voice, Faded Strings, Hallway Pizzicato and Arcade Lead at their defaults have
//     clearly less high-frequency energy and softer attacks than before the patch (the numbers of the
//     code before the patch, measured with the same probe by scripts/build-before.sh, are below), at the
//     same level; Cassette Strings is 2.5 dB louder and otherwise the same;
//   - the rooms still clearly change the choir and the solo voice (every pair of the ten places apart);
//   - Solo Voice: in tune across its range, its loops click-free on long held notes, a legato line has no
//     gap and no second attack.

namespace rememberedtest {

// `LiminalTests --probe-treatment` on the code before the patch (13351aa, Linux, recordings + captures):
// hf (above 4 kHz against the whole, dB), attack (an isolated note's first 60 ms against its body, dB),
// the phrase's level (dB).
struct Before { int which; double hf, attack, level; };
constexpr Before before[] {
    { 3, -39.14, -24.13, -30.83 },    // Empty Choir
    { 13, -44.58, -16.36, -24.41 },   // Solo Voice
    { 7, -27.00, -19.17, -24.41 },    // Faded Strings
    { 8, -14.75, 10.46, -24.37 },     // Hallway Pizzicato
    { 16, -26.57, -0.09, -25.09 },    // Arcade Lead
    { 15, -86.10, -14.76, -24.41 },   // Cassette Strings (+2.5 dB asked)
    { 5, -33.32, -19.52, -28.54 },    // Hollow Drone (its voice layer is one of four: report only)
};

} // namespace rememberedtest

void testRemembered(Suite& s) {
    s.section("remembered (2026-11-05): the voices, strings, pizzicato and arcade lead remembered, not recorded");
    using namespace rememberedtest;
    if (!recordingsReady()) {
        s.report(false, "recordings present (the before/after numbers are for the recordings)", samples::Bank::get().message().toStdString());
        return;
    }
    for (const auto& b : before) {
        const auto m = treatment::measure(b.which);
        const std::string name = params::libraryNames[b.which];
        std::cout << "  " << name << ": high frequencies " << juce::String(b.hf, 1) << " -> " << juce::String(m.hfDb, 1) << " dB, attack "
                  << juce::String(b.attack, 1) << " -> " << juce::String(m.attackDb, 1) << " dB, level " << juce::String(b.level, 2) << " -> "
                  << juce::String(m.levelDb, 2) << " dB\n";
        if (b.which == 15) {   // Cassette Strings: only louder
            s.check(std::abs(m.levelDb - b.level - 2.5) < 0.5, name + ": 2.5 dB louder than before (+/- 0.5)", juce::String(m.levelDb - b.level, 2).toStdString() + " dB");
            s.check(std::abs(m.hfDb - b.hf) < 2.0 && std::abs(m.attackDb - b.attack) < 1.0, name + ": otherwise as before (its tone and attack)");
            continue;
        }
        if (b.which == 5) {
            s.report(m.hfDb <= b.hf + 0.2, name + ": no brighter than before (the voice layer treated)", juce::String(m.hfDb - b.hf, 2).toStdString() + " dB");
            s.check(std::abs(m.levelDb - b.level) < 1.0, name + ": at its level (within 1 dB)", juce::String(m.levelDb - b.level, 2).toStdString() + " dB");
            continue;
        }
        s.check(m.hfDb < b.hf - 6.0, name + ": at least 6 dB less energy above 4 kHz than before", juce::String(m.hfDb - b.hf, 1).toStdString() + " dB");
        s.check(m.attackDb < b.attack - 4.0, name + ": a softer attack than before (its first 60 ms at least 4 dB lower against its body)",
                juce::String(m.attackDb - b.attack, 1).toStdString() + " dB");
        s.check(std::abs(m.levelDb - b.level) < 1.0, name + ": at the level it had (within 1 dB; the level match to the piano stands)", juce::String(m.levelDb - b.level, 2).toStdString() + " dB");
    }

    // The rooms still clearly change the choir and the solo voice: every pair of places apart.
    {
        const char* placeNames[] { "hospital", "mall", "school", "garage", "pool", "hotel", "subway", "supermarket", "office", "impossible room" };
        for (const int which : { 3, 13 }) {
            std::vector<Character> cs;
            for (int place = 0; place < dsp::placeCount; ++place) {
                auto rig = rigIn(place);
                rig->set(params::ambience, params::minDb);
                const int slot = which < 6 ? which : 1;
                const char* ons[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
                for (int i = 0; i < params::instrumentCount; ++i) rig->set(ons[i], i == slot ? 1.0f : 0.0f);
                rig->set(params::slotTypeId(slot).toRawUTF8(), float(which));
                rig->render(1.0);
                cs.push_back(characterOf(rig->render(7.0, standardPhrase(rig->time)), librarytest::rate));
            }
            double nearest = 1e9;
            std::string pair;
            for (int a = 0; a < dsp::placeCount; ++a)
                for (int b = a + 1; b < dsp::placeCount; ++b) {
                    const double d = characterDistance(cs[std::size_t(a)], cs[std::size_t(b)]);
                    if (d < nearest) { nearest = d; pair = std::string(placeNames[a]) + " / " + placeNames[b]; }
                }
            s.check(nearest > 1.0, std::string(params::libraryNames[which]) + ": every pair of the ten places still sounds different (closest over 1.0)",
                    pair + " " + juce::String(nearest, 2).toStdString());
        }
    }

    // Solo Voice: tuning across its range, loops on long held notes, legato.
    {
        const double rate = librarytest::rate;
        double worst = 0;
        int worstNote = 0, clicky = 0;
        for (int note = 43; note <= 72; note += 3) {
            auto rig = librarytest::rigFor(13);
            const auto x = playNote(*rig, note, 90, 4.0, 1.0);
            // (the median of the first three harmonics' peaks: a sung note's partials scatter a few cents)
            const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const Spectrum sp(monoOf(x, rate, 1.0, 3.8), rate, 1 << 18);
            double hs[3];
            for (int h = 1; h <= 3; ++h) hs[h - 1] = centsBetween(sp.peakNear(h * f0, 60.0).first, h * f0);
            std::sort(std::begin(hs), std::end(hs));
            const double c = hs[1];
            std::cout << "  Solo Voice " << note << ": " << juce::String(c, 1) << " cents\n";
            if (std::abs(c) > std::abs(worst)) { worst = c; worstNote = note; }
            clicky += clicksIn(x, rate, 0.3) != 0;
        }
        s.check(std::abs(worst) < 12.0, "Solo Voice: every held note G2-C5 in tune (within 12 cents, its own wobble included)",
                "worst " + juce::String(worst, 1).toStdString() + " cents at " + std::to_string(worstNote));
        s.check(clicky == 0, "Solo Voice: 4 s held notes across its range loop without a click", std::to_string(clicky) + " notes with clicks");
        // A legato line (each note overlapping the next by 40 ms): no gap and no second attack between notes.
        auto rig = librarytest::rigFor(13);
        std::vector<Event> e;
        const int line[] { 60, 62, 64, 67, 65, 64, 62, 60 };
        const double t0 = rig->time + 0.1;
        for (int i = 0; i < 8; ++i) {
            e.push_back({ t0 + 0.6 * i, on(line[i], 85) });
            e.push_back({ t0 + 0.6 * i + 0.64, off(line[i]) });
        }
        const auto x = rig->render(6.0, e);
        if (const char* dir = std::getenv("LIMINAL_WRITE_PROBE")) writeAudio(x, juce::File(dir).getChildFile("legato.wav"));
        // The lowest 20 ms around each change against the two notes' bodies either side (the voice's own
        // level moves with its vowel by several dB within a note, so only a real hole counts).
        double deepest = 0;
        for (int i = 1; i < 8; ++i) {
            const double at = 0.1 + 0.6 * i;
            const double around = 10.0 * std::log10(0.5 * (std::pow(10.0, windowDb(x, rate, at - 0.35, at - 0.05) / 10.0)
                                                          + std::pow(10.0, windowDb(x, rate, at + 0.2, at + 0.5) / 10.0)));
            for (double t = at - 0.05; t < at + 0.2; t += 0.02) deepest = std::min(deepest, windowDb(x, rate, t, t + 0.04) - around);
        }
        s.check(deepest > -9.0, "Solo Voice: a legato line has no hole between notes (no 40 ms more than 9 dB below the notes either side)",
                juce::String(deepest, 1).toStdString() + " dB");
        s.check(clicksIn(x, rate, 0.2) == 0, "Solo Voice: the legato line is click-free");
    }
}
