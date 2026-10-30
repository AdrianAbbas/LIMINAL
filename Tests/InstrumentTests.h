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
    for (const char* id : { params::pianoOn })
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
