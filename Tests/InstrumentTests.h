// Milestone 1 instrument tests: each model is checked against the acoustic facts it is
// built from (docs/INSTRUMENT_NOTES.md), dry (no ambience, no room). Included by
// LiminalTests.cpp inside its anonymous namespace, after Rig and the helpers.

using Signal = std::pair<std::vector<float>, std::vector<float>>;

std::vector<float> monoOf(const Signal& s, double rate, double from, double to) {
    const auto a = std::min(s.first.size(), std::size_t(from * rate)), b = std::min(s.first.size(), std::size_t(to * rate));
    std::vector<float> m;
    m.reserve(b - a);
    for (std::size_t i = a; i < b; ++i) m.push_back(0.5f * (s.first[i] + s.second[i]));
    return m;
}
double windowDb(const Signal& s, double rate, double from, double to) {
    return toDb(rms(s.first, s.second, std::size_t(from * rate), std::size_t(to * rate)));
}

// Magnitude spectrum (Hann window, zero-padded to a power of two >= minSize).
struct Spectrum {
    std::vector<double> mag;
    double binHz = 1;
    Spectrum(const std::vector<float>& x, double rate, std::size_t minSize = 1 << 15) {
        std::size_t n = 1;
        while (n < std::max(minSize, x.size())) n <<= 1;
        std::vector<std::complex<double>> a(n);
        for (std::size_t i = 0; i < x.size(); ++i)
            a[i] = x[i] * (0.5 - 0.5 * std::cos(2.0 * 3.141592653589793 * double(i) / double(x.size() - 1)));
        for (std::size_t i = 1, j = 0; i < n; ++i) {   // iterative radix-2 FFT
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (std::size_t len = 2; len <= n; len <<= 1) {
            const auto w = std::polar(1.0, -2.0 * 3.141592653589793 / double(len));
            for (std::size_t i = 0; i < n; i += len) {
                std::complex<double> u(1.0);
                for (std::size_t k = 0; k < len / 2; ++k, u *= w) {
                    const auto t = a[i + k + len / 2] * u;
                    a[i + k + len / 2] = a[i + k] - t;
                    a[i + k] += t;
                }
            }
        }
        mag.resize(n / 2);
        for (std::size_t i = 0; i < n / 2; ++i) mag[i] = std::abs(a[i]);
        binHz = rate / double(n);
    }
    // The strongest peak within +/- `centsWidth` of hz (parabolic refinement), and its level.
    std::pair<double, double> peakNear(double hz, double centsWidth = 50) const {
        const auto lo = std::size_t(std::max(1.0, hz * std::exp2(-centsWidth / 1200.0) / binHz));
        const auto hi = std::min(mag.size() - 2, std::size_t(hz * std::exp2(centsWidth / 1200.0) / binHz) + 1);
        std::size_t best = lo;
        for (std::size_t i = lo; i <= hi; ++i)
            if (mag[i] > mag[best]) best = i;
        const double y0 = std::log(mag[best - 1] + 1e-30), y1 = std::log(mag[best] + 1e-30), y2 = std::log(mag[best + 1] + 1e-30);
        const double curve = y0 - 2 * y1 + y2;
        const double shift = std::abs(curve) < 1e-12 ? 0.0 : 0.5 * (y0 - y2) / curve;
        return { (double(best) + shift) * binHz, mag[best] };
    }
    double centroid(double from = 30, double to = 16000) const {
        double num = 0, den = 0;
        for (std::size_t i = std::size_t(from / binHz); i < std::min(mag.size(), std::size_t(to / binHz)); ++i) {
            const double p = mag[i] * mag[i];
            num += p * double(i) * binHz;
            den += p;
        }
        return den > 0 ? num / den : 0.0;
    }
    // Energy (dB) in a band, relative to nothing in particular: compare two of them.
    double bandDb(double from, double to) const {
        double e = 0;
        for (std::size_t i = std::size_t(from / binHz); i < std::min(mag.size(), std::size_t(to / binHz)); ++i) e += mag[i] * mag[i];
        return 10.0 * std::log10(e + 1e-30);
    }
};

double centsBetween(double a, double b) { return 1200.0 * std::log2(a / b); }

// Only one instrument on, every control at its default.
void solo(Rig& rig, const char* onId) {
    for (const char* id : { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn })
        rig.set(id, std::strcmp(id, onId) == 0 ? 1.0f : 0.0f);
    dry(rig);
}

// A single note: held `hold` seconds, then `tail` seconds of release.
Signal playNote(Rig& rig, int note, int velocity, double hold, double tail, bool pedalDown = false) {
    const double t = rig.time;
    std::vector<Event> events { { t, on(note, velocity) }, { t + hold, off(note) } };
    if (pedalDown) events.insert(events.begin(), { t, pedal(true) });
    auto out = rig.render(hold + tail, events);
    if (pedalDown) rig.render(0.01, { { rig.time, pedal(false) } });
    return out;
}

// The pitch (autocorrelation) of a stretch of the note, in Hz.
double pitchAt(const Signal& s, double rate, double from, double to) { return frequency(monoOf(s, rate, from, to), rate); }

void testPiano(Suite& s) {
    s.section("piano (Forgotten Piano)");
    const double rate = 48000.0;
    const auto fresh = [&](std::initializer_list<std::pair<const char*, float>> settings = {}) {
        auto rig = std::make_unique<Rig>(rate);
        solo(*rig, params::pianoOn);
        for (const auto& [id, value] : settings) rig->set(id, value);
        return rig;
    };
    {   // In tune at Age 0 / Detune 0; stiff-string stretch of the upper partials.
        auto rig = fresh({ { params::pianoAge, 0.0f }, { params::pianoDetune, 0.0f } });
        const auto a4 = playNote(*rig, 69, 100, 1.5, 0.1);
        const double f = pitchAt(a4, rate, 0.2, 0.6);
        // The stretched partials pull the heard (and autocorrelated) pitch a few cents sharp.
        s.check(std::abs(centsBetween(f, 440.0)) < 6.0, "A4 in tune at Age 0, Detune 0 (within 6 cents)", juce::String(f, 2).toStdString() + " Hz");
        const auto c6 = playNote(*rig, 84, 100, 1.5, 0.1);
        const Spectrum sp(monoOf(c6, rate, 0.05, 1.05), rate, 1 << 17);
        const double f1 = sp.peakNear(1046.5).first, f2 = sp.peakNear(2093.0).first, f4 = sp.peakNear(4186.0, 80).first;
        const double stretch2 = centsBetween(f2, 2 * f1), stretch4 = centsBetween(f4, 4 * f1);
        s.check(stretch2 > 2.0 && stretch2 < 12.0 && stretch4 > stretch2 * 2.5, "C6 partials are stretched (string stiffness): 2nd 2-12 cents sharp, 4th much more",
                juce::String(stretch2, 1).toStdString() + " / " + juce::String(stretch4, 1).toStdString() + " cents");
    }
    {   // Two-stage decay: the prompt sound falls much faster than the aftersound.
        auto rig = fresh();
        const auto c4 = playNote(*rig, 60, 100, 6.0, 0.1);
        const double early = (windowDb(c4, rate, 0.05, 0.15) - windowDb(c4, rate, 0.5, 0.6)) / 0.45;
        const double late = (windowDb(c4, rate, 3.0, 3.2) - windowDb(c4, rate, 5.0, 5.2)) / 2.0;
        s.metric("C4 decay, prompt / aftersound", early, " dB/s");
        s.metric("C4 decay, aftersound", late, " dB/s");
        s.check(early > 2.0 * late && late > 0.3, "two-stage decay: the first half second falls over twice as fast as the aftersound",
                juce::String(early, 1).toStdString() + " vs " + juce::String(late, 1).toStdString() + " dB/s");
        // The fundamental's own frequency, first quarter second vs a second later (Age 0:
        // an aged piano has keys whose unison is badly off, which beats but does not glide).
        auto young = fresh({ { params::pianoAge, 0.0f } });
        const auto c4y = playNote(*young, 60, 100, 1.5, 0.1);
        const double f = Spectrum(monoOf(c4y, rate, 0.0, 0.25), rate, 1 << 17).peakNear(261.6).first;
        const double g = Spectrum(monoOf(c4y, rate, 1.0, 1.25), rate, 1 << 17).peakNear(261.6).first;
        s.check(std::abs(centsBetween(f, g)) < 3.0, "no pitch glide: the fundamental starts where it stays (within 3 cents)",
                juce::String(centsBetween(f, g), 2).toStdString() + " cents (" + juce::String(f, 2).toStdString() + " / " + juce::String(g, 2).toStdString() + " Hz)");
    }
    {   // Velocity: louder and brighter.
        auto rig = fresh();
        const auto soft = playNote(*rig, 60, 35, 0.6, 0.1), hard = playNote(*rig, 60, 120, 0.6, 0.1);
        const double softC = Spectrum(monoOf(soft, rate, 0.0, 0.3), rate).centroid(), hardC = Spectrum(monoOf(hard, rate, 0.0, 0.3), rate).centroid();
        s.metric("C4 spectral centroid, velocity 35", softC, " Hz");
        s.metric("C4 spectral centroid, velocity 120", hardC, " Hz");
        s.check(hardC > 1.3 * softC, "a harder strike is brighter (centroid +30 %)", juce::String(softC, 0).toStdString() + " -> " + juce::String(hardC, 0).toStdString());
        const double range = windowDb(hard, rate, 0, 0.3) - windowDb(soft, rate, 0, 0.3);
        s.check(range > 15.0, "velocity 35 to 120 spans over 15 dB", juce::String(range, 1).toStdString() + " dB");
    }
    {   // Dampers: below F6 a released note stops; the top keys have no dampers.
        auto rig = fresh();
        const auto c4 = playNote(*rig, 60, 100, 1.0, 1.0);
        const double drop = windowDb(c4, rate, 0.9, 1.0) - windowDb(c4, rate, 1.4, 1.5);
        s.check(drop > 25.0, "C4's damper stops it within half a second of release (over 25 dB)", juce::String(drop, 1).toStdString() + " dB");
        // Released early vs held throughout: the same where there is no damper (or the pedal).
        const auto c7 = playNote(*rig, 96, 100, 0.3, 0.8), c7held = playNote(*rig, 96, 100, 1.1, 0.0);
        const double ring = windowDb(c7held, rate, 0.8, 1.0) - windowDb(c7, rate, 0.8, 1.0);
        s.check(std::abs(ring) < 3.0, "C7 (no damper) rings on after release as if held", juce::String(ring, 1).toStdString() + " dB");
        const auto pedalled = playNote(*rig, 60, 100, 0.3, 1.0, true), c4held = playNote(*rig, 60, 100, 1.3, 0.0);
        const double pedalDrop = windowDb(c4held, rate, 0.9, 1.2) - windowDb(pedalled, rate, 0.9, 1.2);
        s.check(std::abs(pedalDrop) < 3.0, "the sustain pedal keeps a released C4 ringing as if held", juce::String(pedalDrop, 1).toStdString() + " dB");
    }
    {   // Each control is heard.
        const auto attack = [&](std::initializer_list<std::pair<const char*, float>> settings, int note = 60) {
            auto rig = fresh(settings);
            return playNote(*rig, note, 100, 2.5, 0.6);
        };
        const auto plain = attack({ { params::pianoSoftness, 0.0f } }), soft = attack({ { params::pianoSoftness, 1.0f } });
        const double c0 = Spectrum(monoOf(plain, rate, 0, 0.3), rate).centroid(), c1 = Spectrum(monoOf(soft, rate, 0, 0.3), rate).centroid();
        s.check(c1 < 0.8 * c0, "Softness darkens the tone", juce::String(c0, 0).toStdString() + " -> " + juce::String(c1, 0).toStdString() + " Hz");

        const auto quiet = attack({ { params::pianoNoise, 0.0f } }), noisy = attack({ { params::pianoNoise, 1.0f } });
        const auto quiet5 = attack({ { params::pianoNoise, 0.0f } }, 72), noisy5 = attack({ { params::pianoNoise, 1.0f } }, 72);
        const double n0 = windowDb(quiet5, rate, 2.5, 2.56), n1 = windowDb(noisy5, rate, 2.5, 2.56);
        const double k0 = Spectrum(monoOf(quiet5, rate, 0, 0.06), rate).bandDb(40, 180), k1 = Spectrum(monoOf(noisy5, rate, 0, 0.06), rate).bandDb(40, 180);
        s.check(k1 > k0 + 2.0 && n1 > n0, "Mechanical Noise adds the key-bed thump and the damper's sound",
                "thump +" + juce::String(k1 - k0, 1).toStdString() + " dB, release +" + juce::String(n1 - n0, 1).toStdString() + " dB");

        const auto shortD = attack({ { params::pianoDecay, 0.0f } }), longD = attack({ { params::pianoDecay, 1.0f } });
        const double d0 = windowDb(shortD, rate, 0.05, 0.1) - windowDb(shortD, rate, 2.3, 2.4);
        const double d1 = windowDb(longD, rate, 0.05, 0.1) - windowDb(longD, rate, 2.3, 2.4);
        s.check(d0 > d1 + 6.0, "Decay lengthens the sustain", juce::String(d0, 1).toStdString() + " vs " + juce::String(d1, 1).toStdString() + " dB lost in 2.3 s");

        // Detune: the unison's beating (envelope wobble over the aftersound) grows.
        const auto wobble = [&](const Signal& x) {
            std::vector<double> frames;
            for (double t = 1.5; t < 3.9; t += 0.02) frames.push_back(windowDb(x, rate, t, t + 0.02));
            double sum = 0;   // deviation from a straight-line decay
            const double slope = (frames.back() - frames.front()) / double(frames.size() - 1);
            for (std::size_t i = 0; i < frames.size(); ++i) sum += std::abs(frames[i] - (frames.front() + slope * double(i)));
            return sum / double(frames.size());
        };
        const auto sustained = [&](float detune) {
            auto rig = fresh({ { params::pianoDetune, detune }, { params::pianoAge, 0.0f } });
            return playNote(*rig, 57, 100, 4.0, 0.1);
        };
        const auto tuned = sustained(0.0f), honky = sustained(1.0f);
        s.check(wobble(honky) > 1.5 * wobble(tuned), "Detune makes the unison beat",
                juce::String(wobble(tuned), 2).toStdString() + " -> " + juce::String(wobble(honky), 2).toStdString() + " dB wobble");

        // Age: keys drift out of tune with one another.
        const auto spread = [&](float age) {
            auto rig = fresh({ { params::pianoAge, age }, { params::pianoDetune, 0.0f } });
            double sum = 0;
            for (int note : { 57, 60, 62, 64, 65, 67, 69, 71 }) {
                const auto x = playNote(*rig, note, 100, 0.5, 0.05);
                sum += std::abs(centsBetween(pitchAt(x, rate, 0.1, 0.4), dsp::noteHz(float(note))));
            }
            return sum / 8.0;
        };
        const double young = spread(0.0f), old = spread(1.0f);
        s.check(old > young + 1.0, "Age puts the keys out of tune with one another",
                juce::String(young, 2).toStdString() + " -> " + juce::String(old, 2).toStdString() + " cents average");

        const auto near = attack({}), far = attack({ { params::pianoDistance, 1.0f } });
        const double l0 = windowDb(near, rate, 0, 0.5), l1 = windowDb(far, rate, 0, 0.5);
        const double b0 = Spectrum(monoOf(near, rate, 0, 0.5), rate).centroid(), b1 = Spectrum(monoOf(far, rate, 0, 0.5), rate).centroid();
        s.check(l1 < l0 - 3.0 && b1 < 0.9 * b0, "Distance makes it quieter and duller",
                juce::String(l1 - l0, 1).toStdString() + " dB, centroid " + juce::String(b0, 0).toStdString() + " -> " + juce::String(b1, 0).toStdString());
    }
    {   // Across the keyboard: every key sounds, none much louder than its neighbours.
        auto rig = fresh();
        double worst = 0, previous = 0;
        bool allFinite = true;
        for (int note = 21; note <= 108; note += 3) {
            const auto x = playNote(*rig, note, 90, 0.4, 0.05);
            allFinite = allFinite && finite(x);
            const double level = windowDb(x, rate, 0, 0.4);
            if (note > 21) worst = std::max(worst, std::abs(level - previous));
            previous = level;
        }
        s.check(allFinite, "A0 to C8 all render finite and unclipped");
        s.check(worst < 6.0, "neighbouring keys (a minor third apart) within 6 dB of each other", juce::String(worst, 1).toStdString() + " dB");
    }
}

// Envelope wobble: mean deviation (dB) of 10 ms frames from a straight-line trend.
double wobbleDb(const Signal& x, double rate, double from, double to, double frame = 0.01) {
    std::vector<double> frames;
    for (double t = from; t < to; t += frame) frames.push_back(windowDb(x, rate, t, t + frame));
    double sum = 0;
    const double slope = (frames.back() - frames.front()) / double(frames.size() - 1);
    for (std::size_t i = 0; i < frames.size(); ++i) sum += std::abs(frames[i] - (frames.front() + slope * double(i)));
    return sum / double(frames.size());
}
// Left/right correlation (1 = mono).
double correlation(const Signal& x, double rate, double from, double to) {
    double lr = 0, ll = 0, rr = 0;
    for (std::size_t i = std::size_t(from * rate); i < std::min(x.first.size(), std::size_t(to * rate)); ++i) {
        lr += double(x.first[i]) * x.second[i];
        ll += double(x.first[i]) * x.first[i];
        rr += double(x.second[i]) * x.second[i];
    }
    return lr / std::sqrt(std::max(1e-30, ll * rr));
}

void testSynth(Suite& s) {
    s.section("synth (Faded Synth)");
    const double rate = 48000.0;
    const auto fresh = [&](std::initializer_list<std::pair<const char*, float>> settings = {}) {
        auto rig = std::make_unique<Rig>(rate);
        solo(*rig, params::synthOn);
        for (const auto& [id, value] : settings) rig->set(id, value);
        return rig;
    };
    {
        auto rig = fresh();
        const auto a4 = playNote(*rig, 69, 100, 1.0, 0.1);
        const double f = pitchAt(a4, rate, 0.3, 0.7);
        s.check(std::abs(centsBetween(f, 440.0)) < 4.0, "A4 in tune (within 4 cents, default drift)", juce::String(f, 2).toStdString() + " Hz");
        s.check(finite(a4), "output finite and within +/-1");
        const double early = pitchAt(a4, rate, 0.012, 0.06);
        s.check(std::abs(centsBetween(early, f)) < 4.0, "no pitch glide: the note starts on its pitch", juce::String(centsBetween(early, f), 2).toStdString() + " cents");
    }
    {   // Sustains while held (unlike the piano and glass), and releases.
        auto rig = fresh();
        const auto c4 = playNote(*rig, 60, 100, 2.5, 1.5);
        const double held = windowDb(c4, rate, 2.0, 2.4) - windowDb(c4, rate, 0.3, 0.7);
        s.check(held > -4.0, "a held note sustains (within 4 dB after 2 s)", juce::String(held, 1).toStdString() + " dB");
        s.check(windowDb(c4, rate, 3.5, 4.0) < windowDb(c4, rate, 2.0, 2.4) - 50.0, "released, it fades away (default release 0.6 s)");
        s.check(rig->p->meters.voices.load() == 0, "no voice left after the release");
    }
    {   // Legato from C4 to E4: no glide by default; with Glide on, it slides.
        const auto legato = [&](bool glide) {
            auto rig = fresh({ { params::synthGlide, glide ? 1.0f : 0.0f }, { params::synthGlideTime, 0.3f }, { params::synthRelease, 0.02f } });
            const double t = rig->time;
            return rig->render(1.2, { { t, on(60) }, { t + 0.5, on(64) }, { t + 0.5, off(60) }, { t + 1.1, off(64) } });
        };
        const auto plain = legato(false), slide = legato(true);
        const double target = dsp::noteHz(64.0f);
        const double p0 = pitchAt(plain, rate, 0.51, 0.56), p1 = pitchAt(slide, rate, 0.51, 0.56), settled = pitchAt(slide, rate, 1.0, 1.1);
        s.check(std::abs(centsBetween(p0, target)) < 6.0, "Glide off (default): the next note starts on its pitch", juce::String(centsBetween(p0, target), 1).toStdString() + " cents");
        s.check(centsBetween(p1, target) < -60.0 && std::abs(centsBetween(settled, target)) < 6.0, "Glide on: it slides up from the previous note, then settles",
                juce::String(centsBetween(p1, target), 1).toStdString() + " cents at the start");
    }
    {   // Chords.
        auto rig = fresh();
        const double t = rig->time;
        const auto chord = rig->render(2.5, { { t, on(48, 90) }, { t, on(55, 90) }, { t, on(60, 90) }, { t, on(64, 90) }, { t, on(67, 90) },
                                               { t + 1.5, off(48) }, { t + 1.5, off(55) }, { t + 1.5, off(60) }, { t + 1.5, off(64) }, { t + 1.5, off(67) } });
        s.check(finite(chord) && peak(chord) < 0.9f, "a five-note chord is unclipped", juce::String(peak(chord), 2).toStdString());
        s.check(windowDb(chord, rate, 0.5, 1.4) > -30.0, "the chord is full (over -30 dBFS)", juce::String(windowDb(chord, rate, 0.5, 1.4), 1).toStdString());
    }
    {   // Velocity: louder and brighter.
        auto rig = fresh();
        const auto soft = playNote(*rig, 60, 35, 0.6, 0.6), hard = playNote(*rig, 60, 120, 0.6, 0.6);
        const double range = windowDb(hard, rate, 0.05, 0.5) - windowDb(soft, rate, 0.05, 0.5);
        const double c0 = Spectrum(monoOf(soft, rate, 0.05, 0.5), rate).centroid(), c1 = Spectrum(monoOf(hard, rate, 0.05, 0.5), rate).centroid();
        s.check(range > 8.0 && c1 > 1.2 * c0, "velocity 35 to 120: over 8 dB louder and brighter",
                juce::String(range, 1).toStdString() + " dB, centroid " + juce::String(c0, 0).toStdString() + " -> " + juce::String(c1, 0).toStdString());
    }
    {   // Each control is heard.
        const auto note = [&](std::initializer_list<std::pair<const char*, float>> settings, double hold = 2.0) {
            auto rig = fresh(settings);
            return playNote(*rig, 60, 100, hold, 0.3);
        };
        const auto bright = note({ { params::synthWarmth, 0.0f } }), warm = note({ { params::synthWarmth, 1.0f } });
        const double c0 = Spectrum(monoOf(bright, rate, 0.5, 1.5), rate).centroid(), c1 = Spectrum(monoOf(warm, rate, 0.5, 1.5), rate).centroid();
        s.check(c1 < 0.6 * c0, "Warmth darkens it", juce::String(c0, 0).toStdString() + " -> " + juce::String(c1, 0).toStdString() + " Hz");

        const auto still = note({ { params::synthDetune, 0.0f }, { params::synthDrift, 0.0f } }), spread = note({ { params::synthDetune, 1.0f }, { params::synthDrift, 0.0f } });
        s.check(wobbleDb(spread, rate, 0.5, 1.9) > 2.0 * wobbleDb(still, rate, 0.5, 1.9) + 0.1, "Detune makes the oscillators beat",
                juce::String(wobbleDb(still, rate, 0.5, 1.9), 2).toStdString() + " -> " + juce::String(wobbleDb(spread, rate, 0.5, 1.9), 2).toStdString() + " dB");

        const auto pitchSpread = [&](float drift) {
            const auto x = note({ { params::synthDrift, drift }, { params::synthDetune, 0.0f } }, 4.0);
            std::vector<double> p;
            for (double t = 0.3; t < 3.9; t += 0.3) p.push_back(centsBetween(pitchAt(x, rate, t, t + 0.12), 261.63));
            const double mean = std::accumulate(p.begin(), p.end(), 0.0) / double(p.size());
            double sum = 0;
            for (double v : p) sum += (v - mean) * (v - mean);
            return std::sqrt(sum / double(p.size()));
        };
        const double d0 = pitchSpread(0.0f), d1 = pitchSpread(1.0f);
        s.check(d1 > d0 + 0.8 && d1 < 15.0, "Drift makes the tuning wander (slowly, a few cents)",
                juce::String(d0, 2).toStdString() + " -> " + juce::String(d1, 2).toStdString() + " cents RMS");

        const auto fast = note({ { params::synthAttack, 0.002f } }), slow = note({ { params::synthAttack, 1.0f } });
        const double a0 = windowDb(fast, rate, 0.02, 0.08) - windowDb(fast, rate, 1.5, 1.9), a1 = windowDb(slow, rate, 0.02, 0.08) - windowDb(slow, rate, 1.5, 1.9);
        s.check(a1 < a0 - 15.0, "Attack slows the onset", juce::String(a0, 1).toStdString() + " vs " + juce::String(a1, 1).toStdString() + " dB at 50 ms");

        const auto shortR = note({ { params::synthRelease, 0.05f } }), longR = note({ { params::synthRelease, 4.0f } });
        const double r0 = windowDb(shortR, rate, 2.15, 2.3), r1 = windowDb(longR, rate, 2.15, 2.3);
        s.check(r1 > r0 + 20.0, "Release lengthens the tail", juce::String(r0, 1).toStdString() + " vs " + juce::String(r1, 1).toStdString() + " dB");

        const auto chord = [&](float width) {
            auto rig = fresh({ { params::synthWidth, width } });
            const double t = rig->time;
            return rig->render(1.5, { { t, on(55) }, { t, on(60) }, { t, on(64) }, { t + 1.4, off(55) }, { t + 1.4, off(60) }, { t + 1.4, off(64) } });
        };
        const double w0 = correlation(chord(0.0f), rate, 0.2, 1.4), w1 = correlation(chord(1.0f), rate, 0.2, 1.4);
        s.check(w1 < w0 - 0.15, "Width widens the stereo image", "L/R correlation " + juce::String(w0, 2).toStdString() + " -> " + juce::String(w1, 2).toStdString());
    }
}

void testGlass(Suite& s) {
    s.section("glass (Glass Memories)");
    const double rate = 48000.0;
    const auto fresh = [&](std::initializer_list<std::pair<const char*, float>> settings = {}) {
        auto rig = std::make_unique<Rig>(rate);
        solo(*rig, params::glassOn);
        for (const auto& [id, value] : settings) rig->set(id, value);
        return rig;
    };
    {   // The measured wine-glass modes, not harmonics.
        auto rig = fresh({ { params::glassDetune, 0.0f }, { params::glassFragility, 0.0f }, { params::glassBrightness, 0.8f } });
        const auto a4 = playNote(*rig, 69, 110, 2.0, 0.1);
        const Spectrum sp(monoOf(a4, rate, 0.02, 1.02), rate, 1 << 17);
        const auto [f1, m1] = sp.peakNear(440.0);
        s.check(std::abs(centsBetween(f1, 440.0)) < 3.0, "A4's lowest mode at 440 Hz (Detune 0)", juce::String(f1, 2).toStdString() + " Hz");
        bool modesOk = true;
        std::string found;
        for (double r : { 1.86, 3.25, 4.95 }) {
            const auto [f, m] = sp.peakNear(440.0 * r, 40);
            modesOk = modesOk && std::abs(f / (440.0 * r) - 1.0) < 0.01 && toDb(m / m1) > -40.0;
            found += juce::String(f / f1, 3).toStdString() + " ";
        }
        s.check(modesOk, "modes at 1.86, 3.25 and 4.95 times the lowest (within 1 %, above -40 dB)", found);
        const double atTwo = sp.peakNear(880.0, 30).second, atMode = sp.peakNear(818.4, 30).second;
        s.check(toDb(atMode / atTwo) > 15.0, "inharmonic: the 1.86 mode is over 15 dB above anything at the octave",
                juce::String(toDb(atMode / atTwo), 1).toStdString() + " dB");
        const double f = Spectrum(monoOf(a4, rate, 0.0, 0.25), rate, 1 << 17).peakNear(440.0).first;
        const double g = Spectrum(monoOf(a4, rate, 1.0, 1.25), rate, 1 << 17).peakNear(440.0).first;
        s.check(std::abs(centsBetween(f, g)) < 2.0, "no pitch glide: the lowest mode starts where it stays", juce::String(centsBetween(f, g), 2).toStdString() + " cents");
    }
    {   // Long, resonant ring; release damps it (a finger on the rim), the pedal does not.
        auto rig = fresh({ { params::glassResonance, 0.0f } });
        const auto held = playNote(*rig, 76, 100, 3.0, 0.1);
        const double ring = windowDb(held, rate, 0.05, 0.25) - windowDb(held, rate, 2.6, 2.9);
        s.check(ring < 30.0, "a struck E5 still rings after nearly 3 s (under 30 dB down)", juce::String(ring, 1).toStdString() + " dB");
        const auto released = playNote(*rig, 76, 100, 0.5, 2.5);
        const double damp = windowDb(held, rate, 1.6, 1.9) - windowDb(released, rate, 1.6, 1.9);
        s.check(damp > 6.0, "releasing the key damps it", juce::String(damp, 1).toStdString() + " dB quieter than held");
        const auto pedalled = playNote(*rig, 76, 100, 0.5, 2.5, true);
        s.check(std::abs(windowDb(held, rate, 1.6, 1.9) - windowDb(pedalled, rate, 1.6, 1.9)) < 2.0, "the sustain pedal lets it ring as if held");
        s.check(finite(held) && finite(released), "output finite and within +/-1");
    }
    {   // Velocity.
        auto rig = fresh();
        const auto soft = playNote(*rig, 72, 35, 0.6, 0.1), hard = playNote(*rig, 72, 120, 0.6, 0.1);
        const double range = windowDb(hard, rate, 0, 0.4) - windowDb(soft, rate, 0, 0.4);
        const double c0 = Spectrum(monoOf(soft, rate, 0, 0.2), rate).centroid(), c1 = Spectrum(monoOf(hard, rate, 0, 0.2), rate).centroid();
        s.check(range > 15.0 && c1 > 1.15 * c0, "velocity 35 to 120: over 15 dB louder and brighter",
                juce::String(range, 1).toStdString() + " dB, centroid " + juce::String(c0, 0).toStdString() + " -> " + juce::String(c1, 0).toStdString());
    }
    {   // Each control is heard.
        const auto note = [&](std::initializer_list<std::pair<const char*, float>> settings, int pitch = 72, double hold = 3.0) {
            auto rig = fresh(settings);
            return playNote(*rig, pitch, 100, hold, 0.2);
        };
        const auto dull = note({ { params::glassBrightness, 0.0f } }), bright = note({ { params::glassBrightness, 1.0f } });
        const double b0 = Spectrum(monoOf(dull, rate, 0, 0.3), rate).centroid(), b1 = Spectrum(monoOf(bright, rate, 0, 0.3), rate).centroid();
        s.check(b1 > 1.4 * b0, "Brightness brings out the upper modes", juce::String(b0, 0).toStdString() + " -> " + juce::String(b1, 0).toStdString() + " Hz");

        const auto clean = note({ { params::glassFragility, 0.0f }, { params::glassDetune, 0.0f }, { params::glassResonance, 0.0f } });
        const auto fragile = note({ { params::glassFragility, 1.0f }, { params::glassDetune, 0.0f }, { params::glassResonance, 0.0f } });
        const double w0 = wobbleDb(clean, rate, 0.3, 2.8, 0.02), w1 = wobbleDb(fragile, rate, 0.3, 2.8, 0.02);
        s.check(w1 > 1.5 * w0, "Fragility makes the mode doublets beat", juce::String(w0, 2).toStdString() + " -> " + juce::String(w1, 2).toStdString() + " dB wobble");

        const auto dry0 = note({ { params::glassResonance, 0.0f } }), sing = note({ { params::glassResonance, 1.0f } });
        const double r0 = windowDb(dry0, rate, 2.5, 2.9), r1 = windowDb(sing, rate, 2.5, 2.9);
        s.check(r1 > r0 + 6.0, "Resonance: the held glass sings on", juce::String(r0, 1).toStdString() + " -> " + juce::String(r1, 1).toStdString() + " dB at 2.5 s");
        const double swell = windowDb(sing, rate, 1.0, 1.2) - windowDb(sing, rate, 0.05, 0.1);
        s.check(swell > -12.0, "the singing swells in rather than starting at once (no click)", juce::String(swell, 1).toStdString() + " dB");

        const auto shortD = note({ { params::glassDecay, 0.0f }, { params::glassResonance, 0.0f } }), longD = note({ { params::glassDecay, 1.0f }, { params::glassResonance, 0.0f } });
        const double d0 = windowDb(shortD, rate, 0.05, 0.2) - windowDb(shortD, rate, 2.0, 2.3), d1 = windowDb(longD, rate, 0.05, 0.2) - windowDb(longD, rate, 2.0, 2.3);
        s.check(d0 > d1 + 15.0, "Decay lengthens the ring", juce::String(d0, 1).toStdString() + " vs " + juce::String(d1, 1).toStdString() + " dB lost in 2 s");

        const auto spreadOf = [&](float detune) {
            auto rig = fresh({ { params::glassDetune, detune }, { params::glassFragility, 0.0f } });
            double sum = 0;
            for (int k = 0; k < 8; ++k) {
                const auto x = playNote(*rig, 69, 100, 0.6, 0.05);
                sum += std::abs(centsBetween(Spectrum(monoOf(x, rate, 0.05, 0.55), rate, 1 << 16).peakNear(440.0, 60).first, 440.0));
            }
            return sum / 8.0;
        };
        const double t0 = spreadOf(0.0f), t1 = spreadOf(1.0f);
        s.check(t1 > t0 + 3.0 && t1 < 25.0, "Detune: each strike lands a little off pitch", juce::String(t0, 2).toStdString() + " -> " + juce::String(t1, 2).toStdString() + " cents average");

        // Duller: the share of energy above 3 kHz (the upper modes) falls.
        const auto near = note({}), far = note({ { params::glassDistance, 1.0f } });
        const double l0 = windowDb(near, rate, 0, 0.5), l1 = windowDb(far, rate, 0, 0.5);
        const Spectrum sn(monoOf(near, rate, 0, 0.5), rate), sf(monoOf(far, rate, 0, 0.5), rate);
        const double h0 = sn.bandDb(3000, 16000) - sn.bandDb(30, 16000), h1 = sf.bandDb(3000, 16000) - sf.bandDb(30, 16000);
        s.check(l1 < l0 - 3.0 && h1 < h0 - 6.0, "Distance makes it quieter and duller",
                juce::String(l1 - l0, 1).toStdString() + " dB, energy above 3 kHz " + juce::String(h0, 1).toStdString() + " -> " + juce::String(h1, 1).toStdString() + " dB");
    }
}

// The three together: distinct with no room at all, levels that match, safe to combine.
// A rig with only one instrument on (dry), with some of its controls set.
std::unique_ptr<Rig> soloRig(const char* onId, std::initializer_list<std::pair<const char*, float>> settings = {}) {
    auto rig = std::make_unique<Rig>(48000.0);
    solo(*rig, onId);
    for (const auto& [id, value] : settings) rig->set(id, value);
    rig->render(0.05);
    return rig;
}
std::string num(double x, int places = 1) { return juce::String(x, places).toStdString(); }

void testChoir(Suite& s) {
    s.section("choir (Empty Choir)");
    const double rate = 48000.0;
    const auto note = [&](std::initializer_list<std::pair<const char*, float>> settings, int key = 57, double hold = 2.0, double tail = 0.2) {
        auto rig = soloRig(params::choirOn, settings);
        return playNote(*rig, key, 100, hold, tail);
    };
    {
        auto rig = soloRig(params::choirOn);
        const auto a3 = playNote(*rig, 57, 100, 2.0, 7.0);
        const double f = pitchAt(a3, rate, 0.25, 0.45);
        s.check(std::abs(centsBetween(f, 220.0)) < 10.0, "A3 in tune (within 10 cents: detuned singers)", num(f, 2) + " Hz");
        s.check(finite(a3) && peak(a3) < 0.9f, "output finite and unclipped", num(peak(a3), 2));
        const double rise = windowDb(a3, rate, 0.8, 1.2) - windowDb(a3, rate, 0.0, 0.05);
        s.check(rise > 8.0, "slow attack: 8 dB or more louder at 1 s than in the first 50 ms", num(rise) + " dB");
        s.check(windowDb(a3, rate, 1.6, 2.0) - windowDb(a3, rate, 0.8, 1.2) > -3.0, "a held note sustains");
        const double tail = windowDb(a3, rate, 3.0, 3.3) - windowDb(a3, rate, 1.6, 2.0);
        s.check(tail < -6.0 && tail > -50.0, "default release (2.5 s): still audible 1 s after key-up, fading", num(tail) + " dB");
        s.check(rig->p->meters.voices.load() == 0, "the voice ends after its release");
    }
    const auto centroid = [&](const Signal& x) { return Spectrum(monoOf(x, rate, 0.8, 1.8), rate).centroid(); };
    {
        const auto oo = note({ { params::choirFormant, 0.0f } }), ah = note({ { params::choirFormant, 0.5f } }), ee = note({ { params::choirFormant, 1.0f } });
        const auto f2 = [&](const Signal& x) { const Spectrum q(monoOf(x, rate, 0.8, 1.8), rate); return q.bandDb(1800, 2600) - q.bandDb(200, 1000); };
        s.check(centroid(ah) > centroid(oo) * 1.3, "Formant moves the vowel: 'ah' brighter than 'oo'", num(centroid(oo), 0) + " -> " + num(centroid(ah), 0) + " Hz");
        s.check(f2(ee) > f2(oo) + 10.0, "and 'ee' has its high second formant (1.8-2.6 kHz)", num(f2(oo)) + " -> " + num(f2(ee)) + " dB");
    }
    const auto band = [&](const Signal& x, double lo, double hi) { return Spectrum(monoOf(x, rate, 0.8, 1.8), rate).bandDb(lo, hi) - Spectrum(monoOf(x, rate, 0.8, 1.8), rate).bandDb(100, 1000); };
    const double airLo = band(note({ { params::choirBreath, 0.0f } }), 4000, 9000), airHi = band(note({ { params::choirBreath, 1.0f } }), 4000, 9000);
    s.check(airHi > airLo + 6.0, "Breath adds air above 4 kHz", num(airLo) + " -> " + num(airHi) + " dB");
    // Darkness, measured above the first formant (the centroid is pinned by F1).
    const double light = band(note({ { params::choirDarkness, 0.0f } }), 1500, 6000), dark = band(note({ { params::choirDarkness, 1.0f } }), 1500, 6000);
    s.check(dark < light - 10.0, "Darkness darkens (less energy at 1.5-6 kHz)", num(light) + " -> " + num(dark) + " dB");
    const double narrow = correlation(note({ { params::choirWidth, 0.0f } }), rate, 0.8, 1.8), wide = correlation(note({ { params::choirWidth, 1.0f } }), rate, 0.8, 1.8);
    s.check(wide < narrow - 0.15, "Width spreads the singers (L/R correlation falls)", num(narrow, 2) + " -> " + num(wide, 2));
    const double still = wobbleDb(note({ { params::choirDetune, 0.0f } }), rate, 0.8, 1.8, 0.02), beating = wobbleDb(note({ { params::choirDetune, 1.0f } }), rate, 0.8, 1.8, 0.02);
    s.check(beating > still * 1.3, "Detune makes the singers beat against each other", num(still, 2) + " -> " + num(beating, 2) + " dB");
    {
        auto rig = soloRig(params::choirOn, { { params::choirRelease, 0.1f } });
        playNote(*rig, 57, 100, 1.0, 1.0);
        s.check(rig->p->meters.voices.load() == 0, "a short Release ends the note quickly");
    }
}

// The strongest spectral peak near `nominal` (the music box's 6.27 x mode misleads the
// autocorrelation pitch detector).
double spectralPitch(const Signal& x, double rate, double from, double to, double nominal) {
    return Spectrum(monoOf(x, rate, from, to), rate, 1 << 17).peakNear(nominal, 120).first;
}

void testBox(Suite& s) {
    s.section("box (Lost Music Box)");
    const double rate = 48000.0;
    {
        auto rig = soloRig(params::boxOn, { { params::boxWear, 0.0f }, { params::boxDrift, 0.0f } });
        const auto a5 = playNote(*rig, 81, 100, 0.3, 2.7);
        const double f = spectralPitch(a5, rate, 0.1, 0.5, 880.0);
        s.check(std::abs(centsBetween(f, 880.0)) < 3.0, "A5 in tune with Wear and Pitch Drift at 0", num(f, 2) + " Hz");
        const double early = spectralPitch(a5, rate, 0.0, 0.08, 880.0);
        s.check(std::abs(centsBetween(early, f)) < 5.0, "no pitch glide: the pluck starts on its pitch", num(centsBetween(early, f), 2) + " cents");
        s.check(windowDb(a5, rate, 1.8, 2.0) - windowDb(a5, rate, 0.05, 0.15) < -12.0, "a short metallic pluck: it decays while held");
        s.check(finite(a5) && peak(a5) < 0.9f, "output finite and unclipped", num(peak(a5), 2));
        rig->render(4.0);
        s.check(rig->p->meters.voices.load() == 0, "the note rings out and ends");
        const Spectrum sp(monoOf(a5, rate, 0.01, 0.3), rate, 1 << 16);
        const double beam = toDb(sp.peakNear(880.0 * 6.267, 25).second / std::max(1e-12, sp.peakNear(880.0 * 6.0, 15).second));
        s.check(beam > 10.0, "the tooth's 2nd bending mode (6.27 x) stands out, not the 6th harmonic", num(beam) + " dB");
    }
    {   // Wear mistunes the teeth, each by its own fixed amount.
        const auto spread = [&](float wear) {
            double sum = 0;
            for (int key = 72; key < 84; ++key) {
                auto rig = soloRig(params::boxOn, { { params::boxWear, wear }, { params::boxDrift, 0.0f } });
                const auto x = playNote(*rig, key, 100, 0.3, 0.0);
                const double c = centsBetween(spectralPitch(x, rate, 0.02, 0.3, dsp::noteHz(float(key))), dsp::noteHz(float(key)));
                sum += c * c;
            }
            return std::sqrt(sum / 12.0);
        };
        const double worn = spread(1.0f), fresh = spread(0.0f);
        s.check(worn > 6.0 && fresh < 2.0, "Wear puts the teeth out of tune with each other", num(fresh, 2) + " -> " + num(worn, 2) + " cents RMS");
    }
    {   // Pitch Drift: the spring motor wanders.
        const auto wander = [&](float drift) {
            auto rig = soloRig(params::boxOn, { { params::boxWear, 0.0f }, { params::boxDrift, drift } });
            std::vector<double> c;
            for (int i = 0; i < 8; ++i) {
                const auto x = playNote(*rig, 81, 100, 0.25, 0.25);
                c.push_back(centsBetween(spectralPitch(x, rate, 0.02, 0.25, 880.0), 880.0));
            }
            const auto [lo, hi] = std::minmax_element(c.begin(), c.end());
            return *hi - *lo;
        };
        const double steady = wander(0.0f), drifting = wander(1.0f);
        s.check(drifting > 4.0 && steady < 1.0, "Pitch Drift makes the notes wander over time", num(steady, 2) + " -> " + num(drifting, 2) + " cents range");
    }
    const auto play = [&](std::initializer_list<std::pair<const char*, float>> settings) {
        auto rig = soloRig(params::boxOn, settings);
        return playNote(*rig, 76, 100, 0.2, 1.8);
    };
    const auto centroid = [&](const Signal& x) { return Spectrum(monoOf(x, rate, 0.0, 0.12), rate).centroid(); };
    const double dull = centroid(play({ { params::boxBrightness, 0.0f } })), bright = centroid(play({ { params::boxBrightness, 1.0f } }));
    s.check(bright > dull * 1.3, "Brightness brings out the upper modes", num(dull, 0) + " -> " + num(bright, 0) + " Hz");
    const auto shortRing = play({ { params::boxDecay, 0.0f } }), longRing = play({ { params::boxDecay, 1.0f } });
    s.check(windowDb(longRing, rate, 1.0, 1.4) - windowDb(shortRing, rate, 1.0, 1.4) > 15.0, "Decay lengthens the ring");
    const auto quiet = play({ { params::boxMechanism, 0.0f } }), noisy = play({ { params::boxMechanism, 1.0f } });
    const double click = Spectrum(monoOf(noisy, rate, 0.0, 0.02), rate).bandDb(6000, 10500) - Spectrum(monoOf(quiet, rate, 0.0, 0.02), rate).bandDb(6000, 10500);
    s.check(click > 3.0, "Mechanism adds the pin's click", num(click) + " dB");
    const auto near = play({ { params::boxDistance, 0.0f } }), far = play({ { params::boxDistance, 1.0f } });
    s.check(toDb(rms(far)) < toDb(rms(near)) - 2.0 && centroid(far) < centroid(near), "Distance: quieter and duller");
}

void testDrone(Suite& s) {
    s.section("drone (Hollow Drone)");
    const double rate = 48000.0;
    {
        auto rig = soloRig(params::droneOn, { { params::droneDensity, 0.0f }, { params::droneInstability, 0.0f }, { params::droneSub, 0.0f } });
        const auto a3 = playNote(*rig, 57, 100, 4.0, 0.1);
        const double f = pitchAt(a3, rate, 2.5, 3.0);
        s.check(std::abs(centsBetween(f, 220.0)) < 6.0, "A3 in tune (Density, Instability and Sub at 0)", num(f, 2) + " Hz");
        const double rise = windowDb(a3, rate, 2.5, 3.0) - windowDb(a3, rate, 0.0, 0.1);
        s.check(rise > 12.0, "long default attack (1.2 s): swells in", num(rise) + " dB");
    }
    {
        auto rig = soloRig(params::droneOn);
        const auto c3 = playNote(*rig, 48, 100, 4.0, 2.0);
        s.check(finite(c3) && peak(c3) < 0.9f, "output finite and unclipped", num(peak(c3), 2));
        s.check(windowDb(c3, rate, 3.5, 4.0) - windowDb(c3, rate, 2.5, 3.0) > -3.0, "a held note sustains");
        const double tail = windowDb(c3, rate, 5.0, 5.5) - windowDb(c3, rate, 3.5, 4.0);
        s.check(tail > -25.0 && tail < -2.0, "long default release (4 s): still sounding a second after key-up", num(tail) + " dB");
        rig->render(16.0);
        s.check(rig->p->meters.voices.load() == 0, "the voice ends after its release");
    }
    const auto hold = [&](std::initializer_list<std::pair<const char*, float>> settings, int key = 45) {
        auto rig = soloRig(params::droneOn, settings);
        return playNote(*rig, key, 100, 5.0, 0.0);
    };
    const auto spectrum = [&](const Signal& x) { return Spectrum(monoOf(x, rate, 3.0, 5.0), rate, 1 << 17); };
    {
        const auto none = spectrum(hold({ { params::droneSub, 0.0f } })), lots = spectrum(hold({ { params::droneSub, 1.0f } }));
        const double gain = toDb(lots.peakNear(55.0, 40).second) - toDb(none.peakNear(55.0, 40).second);
        s.check(gain > 15.0, "Sub adds the octave below (and two below)", num(gain) + " dB at 55 Hz");
    }
    {
        const double lo = spectrum(hold({ { params::droneHarmonics, 0.0f } })).centroid(), hi = spectrum(hold({ { params::droneHarmonics, 1.0f } })).centroid();
        s.check(hi > lo * 1.5, "Harmonics opens the filter and adds upper partials", num(lo, 0) + " -> " + num(hi, 0) + " Hz");
    }
    {
        const double still = correlation(hold({ { params::droneMovement, 0.0f } }), rate, 3.0, 5.0), moving = correlation(hold({ { params::droneMovement, 1.0f } }), rate, 3.0, 5.0);
        s.check(moving < still - 0.1, "Movement widens and moves the stereo image", num(still, 2) + " -> " + num(moving, 2));
    }
    {
        // Energy beside the fundamental (4-25 cents off) against energy on it (A3, 220 Hz).
        const auto spread = [&](float density) {
            const auto q = spectrum(hold({ { params::droneDensity, density }, { params::droneInstability, 0.0f } }, 57));
            const double f = 220.0;
            const auto c = [&](double x) { return f * std::exp2(x / 1200.0); };
            return 10.0 * std::log10(std::pow(10.0, q.bandDb(c(-25), c(-4)) / 10.0) + std::pow(10.0, q.bandDb(c(4), c(25)) / 10.0)) - q.bandDb(c(-3), c(3));
        };
        const double thin = spread(0.0f), thick = spread(1.0f);
        s.check(thick > thin + 6.0, "Density stacks detuned oscillators around the pitch", num(thin) + " -> " + num(thick) + " dB beside the fundamental");
    }
    {
        auto rig = soloRig(params::droneOn, { { params::droneAttack, 0.01f }, { params::droneRelease, 0.1f } });
        const auto x = playNote(*rig, 45, 100, 1.0, 1.0);
        s.check(windowDb(x, rate, 0.05, 0.15) - windowDb(x, rate, 0.8, 1.0) > -3.0 && rig->p->meters.voices.load() == 0, "short Attack and Release: immediate and quick");
    }
}

void testInstruments(Suite& s) {
    s.section("instruments (all six)");
    const double rate = 48000.0;
    struct Item { const char* on; const char* name; };
    const Item items[] { { params::pianoOn, "piano" }, { params::synthOn, "synth" }, { params::glassOn, "glass" },
                         { params::choirOn, "choir" }, { params::boxOn, "music box" }, { params::droneOn, "drone" } };
    {
        Rig rig(rate);
        bool others = false;
        for (const char* id : { params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn })
            others |= rig.p->parameters.getRawParameterValue(id)->load() > 0.5f;
        s.check(rig.p->parameters.getRawParameterValue(params::pianoOn)->load() > 0.5f && !others, "by default the Forgotten Piano is on, the others off");
    }
    struct Features { double sustain, inharmonic, centroid, melody, held; };
    std::vector<Features> f;
    for (const auto& item : items) {
        Rig rig(rate);
        solo(rig, item.on);
        const auto x = playNote(rig, 60, 100, 2.2, 0.3);
        const Spectrum sp(monoOf(x, rate, 0.02, 1.02), rate, 1 << 17);
        const double fund = sp.peakNear(261.63).second;
        // How much sits at the glass's 1.86 mode versus at the octave.
        const double inharmonic = toDb(sp.peakNear(261.63 * 1.86, 30).second / std::max(1e-12, sp.peakNear(261.63 * 2.0, 30).second));
        Features q { windowDb(x, rate, 1.9, 2.1) - windowDb(x, rate, 0.1, 0.3), inharmonic, sp.centroid(), 0, windowDb(x, rate, 1.0, 2.0) };
        juce::ignoreUnused(fund);
        // A mezzo-forte phrase, as in the levels test.
        const double t = rig.time;
        std::vector<Event> phrase;
        const int notes[] { 60, 64, 67, 72, 67, 64, 62, 60 };
        for (int i = 0; i < 8; ++i) {
            phrase.push_back({ t + 0.4 * i, on(notes[i], 80) });
            phrase.push_back({ t + 0.4 * i + 0.35, off(notes[i]) });
        }
        const auto melody = rig.render(3.6, phrase);
        q.melody = toDb(rms(melody));
        s.check(finite(melody) && peak(melody) < 0.9f, std::string(item.name) + ": a phrase is unclipped", juce::String(peak(melody), 2).toStdString());
        s.metric(std::string(item.name) + " C4: level change 0.2 s -> 2 s held", q.sustain, " dB");
        s.metric(std::string(item.name) + " C4: 1.86 mode vs octave", q.inharmonic, " dB");
        s.metric(std::string(item.name) + " C4: spectral centroid", q.centroid, " Hz");
        s.metric(std::string(item.name) + " mezzo-forte phrase", q.melody, " dBFS");
        f.push_back(q);
    }
    s.check(f[1].sustain > f[0].sustain + 6.0 && f[1].sustain > f[2].sustain + 4.0, "the synth sustains, the piano and glass decay",
            juce::String(f[0].sustain, 1).toStdString() + " / " + juce::String(f[1].sustain, 1).toStdString() + " / " + juce::String(f[2].sustain, 1).toStdString() + " dB");
    s.check(f[2].inharmonic > 15.0 && f[0].inharmonic < -10.0 && f[1].inharmonic < -10.0, "only the glass is inharmonic (strong 1.86 mode, weak octave)",
            juce::String(f[0].inharmonic, 1).toStdString() + " / " + juce::String(f[1].inharmonic, 1).toStdString() + " / " + juce::String(f[2].inharmonic, 1).toStdString() + " dB");
    {
        // The slow pads (choir, drone) never reach full level in a quick phrase: their held
        // note is compared with the piano's phrase instead.
        std::string levels;
        bool close = true;
        for (std::size_t i = 0; i < f.size(); ++i) {
            const bool pad = i == 3 || i == 5;
            const double level = pad ? f[i].held : f[i].melody;
            close &= std::abs(level - f[0].melody) < 6.0;
            levels += (i ? " / " : "") + num(level) + (pad ? " (held)" : "");
        }
        s.check(close, "each instrument within 6 dB of the piano's phrase", levels + " dBFS");
    }
    {   // The six are told apart by sustain, inharmonicity and brightness together.
        double nearest = 1e9;
        std::string pair;
        for (std::size_t a = 0; a < f.size(); ++a)
            for (std::size_t b = a + 1; b < f.size(); ++b) {
                const double d = std::abs(f[a].sustain - f[b].sustain) / 6.0 + std::abs(f[a].inharmonic - f[b].inharmonic) / 10.0
                               + std::abs(std::log2(f[a].centroid / f[b].centroid)) / 0.5;
                if (d < nearest) { nearest = d; pair = std::string(items[a].name) + " vs " + items[b].name; }
            }
        s.check(nearest > 1.0, "every pair of instruments differs clearly (sustain / inharmonicity / brightness)", "closest: " + pair + " " + num(nearest, 2));
    }

    {   // All three at once: chords, the pedal, bends, a storm; nothing stuck, nothing clipped.
        Rig rig(rate);
        dry(rig);
        for (const auto& item : items) rig.set(item.on, 1.0f);
        std::vector<Event> events;
        const double t = rig.time;
        events.push_back({ t, pedal(true) });
        for (int i = 0; i < 200; ++i) {
            const int note = 36 + (i * 7) % 48;
            events.push_back({ t + 0.01 * i, on(note, 40 + (i * 13) % 87) });
            events.push_back({ t + 0.01 * i + 0.15, off(note) });
        }
        events.push_back({ t + 1.0, juce::MidiMessage::pitchWheel(1, 16383) });
        events.push_back({ t + 1.5, juce::MidiMessage::pitchWheel(1, 8192) });
        events.push_back({ t + 2.5, pedal(false) });
        const auto out = rig.render(4.0, events);
        s.check(finite(out) && peak(out) <= 1.0f, "all six together through a 200-note storm: finite, held under 0 dBFS by the safety limiter", juce::String(peak(out), 2).toStdString());
        rig.render(20.0);
        s.check(rig.p->meters.voices.load() == 0, "every voice of all six has ended afterwards (no stuck notes)", std::to_string(rig.p->meters.voices.load()));
    }
    {   // Solo: only the soloed instrument is heard; soloing one that is off changes nothing.
        const auto level = [&](float soloValue, bool glassOn) {
            Rig rig(rate);
            dry(rig);
            rig.set(params::glassOn, glassOn ? 1.0f : 0.0f);
            rig.set(params::pianoOn, 1.0f);
            rig.set(params::solo, soloValue);
            rig.render(0.1);
            return playNote(rig, 72, 100, 0.6, 0.0);
        };
        const auto both = level(0, true), glassOnly = level(3, true), pianoOnly = level(1, true), offSolo = level(3, false);
        Rig ref(rate);
        solo(ref, params::glassOn);
        ref.render(0.1);   // same start time as the others (the control clock runs from the start)
        const auto glassRef = playNote(ref, 72, 100, 0.6, 0.0);
        double diff = 0;
        for (std::size_t i = 4800; i < glassOnly.first.size(); ++i) diff = std::max(diff, double(std::abs(glassOnly.first[i] - glassRef.first[i])));
        s.check(diff < 1.0e-4, "soloing the glass: exactly the glass alone (piano muted)", num(diff * 1e6, 1) + " e-6");
        s.check(rms(pianoOnly) < rms(both) && rms(pianoOnly) > 0.0, "soloing the piano mutes the glass");
        s.check(std::abs(toDb(rms(offSolo)) - toDb(rms(level(0, false)))) < 0.01, "soloing an instrument that is off changes nothing");
    }
    {   // Switching an instrument off fades it and stops it.
        Rig rig(rate);
        solo(rig, params::synthOn);
        rig.render(0.5, { { rig.time, on(60) } });
        rig.set(params::synthOn, 0.0f);
        const auto after = rig.render(0.3);
        s.check(rms(rig.render(0.1)) < 1.0e-5 && rig.p->meters.voices.load() == 0, "switching the synth off silences it (no stuck note)");
        juce::ignoreUnused(after);
    }
    for (const auto& item : items) {   // Block size does not change the sound.
        Rig a(rate, 512), b(rate, 64);
        solo(a, item.on);
        solo(b, item.on);
        // Event times half a sample past whole samples, so both rigs place them identically.
        const double t = a.time + 0.5 / rate;
        const std::vector<Event> events { { t + 0.05, on(60) }, { t + 0.1, on(67) }, { t + 0.4, off(60) } };
        const auto x = a.render(0.6, events);
        const auto y = b.render(0.6, events);
        double diff = 0;
        for (std::size_t i = 0; i < x.first.size(); ++i) diff = std::max(diff, double(std::abs(x.first[i] - y.first[i])));
        s.check(diff < 1.0e-5, std::string(item.name) + ": identical output with 512- and 64-sample blocks", juce::String(diff).toStdString());
    }
    {   // Real-time safety and cost with all three playing.
        Rig rig(rate);
        dry(rig);
        for (const auto& item : items) rig.set(item.on, 1.0f);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int note = 48; note < 64; ++note) midi.addEvent(on(note, 100), 0);
        midi.addEvent(pedal(true), 0);
        allocationProbe::calls = 0;
        const auto start = std::chrono::steady_clock::now();
        const int blocks = int(10.0 * rate / 512);
        for (int i = 0; i < blocks; ++i) {
            allocationProbe::enabled = true;
            rig.p->processBlock(buffer, midi);
            allocationProbe::enabled = false;
            midi.clear();
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        s.check(allocationProbe::calls == 0, "processBlock never allocates with all six instruments (16 notes each)", std::to_string(allocationProbe::calls));
        s.metric("CPU, 16 notes on all six instruments + ambience, % of one core at 48 kHz (report-only)", 100.0 * seconds / 10.0, " %");
    }
}
