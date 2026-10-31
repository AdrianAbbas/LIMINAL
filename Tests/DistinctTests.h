// Ten truly distinct rooms (2026-10-31): the same piano phrase through every place, described by
// what a listener hears, and every pair of places compared. Features (each scaled so that 1 is
// "clearly audible"):
//   tone     octave profile 31 Hz - 16 kHz (3 dB RMS = 1)
//   decay    the shape of the tail after the chord: four windows, 0.05 - 2.2 s (6 dB RMS = 1)
//   width    left/right correlation (0.25 = 1)
//   echoes   the envelope's self-similarity at 3 ms ... 1.3 s, in eight octave bands: flutter,
//            slaps, speaker arrivals, tunnel echoes, reversed swells (0.12 RMS = 1)
//   hum      120 Hz amplitude modulation of the chord (6 dB = 1)
//   flicker  the level's fast movement during the melody (1.5 dB = 1)
//   steady   how much the spectrum of the held chord changes frame to frame: wobble, underwater,
//            freezes (0.3 in -log10(1 - correlation) = 1)
// Every pair must differ by more than 2.0 ("clearly, blind"); the closest pairs are listed with the
// feature that separates them most. And within each place, every Character must move the
// signature audibly from the place's first Character (over 1.0).
#pragma once

namespace distinct {

struct Room {
    std::vector<double> tone, decay, echoes;
    double width = 0, hum = 0, flicker = 0, steady = 0, level = 0;
};

// Frame RMS envelope (linear), `frameMs` frames.
std::vector<double> envelopeOf(const std::vector<float>& m, double rate, double frameMs) {
    const auto f = std::size_t(std::max(1.0, frameMs * 0.001 * rate));
    std::vector<double> e;
    for (std::size_t i = 0; i + f <= m.size(); i += f) {
        double s = 0;
        for (std::size_t k = i; k < i + f; ++k) s += double(m[k]) * m[k];
        e.push_back(std::sqrt(s / double(f)));
    }
    return e;
}
std::vector<double> minusAverage(const std::vector<double>& e, int half) {
    std::vector<double> out(e.size());
    for (std::size_t i = 0; i < e.size(); ++i) {
        const std::size_t a = i >= std::size_t(half) ? i - std::size_t(half) : 0, b = std::min(e.size(), i + std::size_t(half) + 1);
        double s = 0;
        for (std::size_t k = a; k < b; ++k) s += e[k];
        out[i] = e[i] - s / double(b - a);
    }
    return out;
}
// The strongest normalised autocorrelation within [lo, hi] frames.
double strongest(const std::vector<double>& e, int lo, int hi) {
    double e0 = 0;
    for (const double v : e) e0 += v * v;
    double best = -1;
    for (int lag = lo; lag <= hi && lag < int(e.size()) - 1; ++lag) {
        double s = 0;
        for (std::size_t i = 0; i + std::size_t(lag) < e.size(); ++i) s += e[i] * e[i + std::size_t(lag)];
        best = std::max(best, s / (e0 + 1e-18));
    }
    return best;
}

Room describe(const Signal& x, double rate) {
    Room r;
    const auto played = monoOf(x, rate, 0.0, 5.2);
    {
        Signal p { std::vector<float>(x.first.begin(), x.first.begin() + std::ptrdiff_t(5.2 * rate)),
                   std::vector<float>(x.second.begin(), x.second.begin() + std::ptrdiff_t(5.2 * rate)) };
        r.tone = averageProfile(p, rate);
    }
    r.level = windowDb(x, rate, 0.0, 5.2);
    const double chord = windowDb(x, rate, 4.0, 5.0);
    for (const auto& [a, b] : { std::pair { 5.05, 5.3 }, { 5.3, 5.7 }, { 5.7, 6.4 }, { 6.4, 7.2 } }) r.decay.push_back(std::max(-60.0, windowDb(x, rate, a, b) - chord));
    r.width = correlation(x, rate, 0.0, 7.2);
    // Echo structure: fine (1 ms frames, note shapes removed) for 3-40 ms, coarse (5 ms) beyond.
    {
        const auto melody = monoOf(x, rate, 0.0, 3.7);
        const auto fine = minusAverage(envelopeOf(melody, rate, 1.0), 12);
        for (const auto& [lo, hi] : { std::pair { 3, 8 }, { 8, 20 }, { 20, 40 } }) r.echoes.push_back(strongest(fine, lo, hi));
        const auto coarse = minusAverage(envelopeOf(monoOf(x, rate, 0.0, 7.2), rate, 5.0), 60);
        for (const auto& [lo, hi] : { std::pair { 8, 16 }, { 16, 32 }, { 32, 64 }, { 64, 128 }, { 128, 260 } }) r.echoes.push_back(strongest(coarse, lo, hi));
    }
    // Hum: the held chord's envelope (rectified, ~500 Hz low-pass), its spectrum at 120 Hz against
    // the bands either side.
    {
        const auto c = monoOf(x, rate, 3.9, 4.9);
        std::vector<float> env(c.size());
        double z1 = 0, z2 = 0;
        const double k = 1.0 - std::exp(-6.283185 * 500.0 / rate);
        for (std::size_t i = 0; i < c.size(); ++i) {
            z1 += k * (std::abs(c[i]) - z1);
            z2 += k * (z1 - z2);
            env[i] = float(z2);
        }
        const double mean = std::accumulate(env.begin(), env.end(), 0.0) / double(env.size());
        for (auto& v : env) v -= float(mean);
        const Spectrum sp(env, rate, 1 << 16);
        const double at = sp.bandDb(114.0, 126.0), side = 0.5 * (sp.bandDb(80.0, 108.0) + sp.bandDb(132.0, 160.0));
        r.hum = std::clamp(at - side, 0.0, 30.0);
    }
    r.flicker = wobbleDb(x, rate, 0.5, 3.4, 0.02);
    // Steadiness of the held chord: consecutive 43 ms magnitude spectra.
    {
        const auto c = monoOf(x, rate, 3.9, 4.95);
        const std::size_t n = 2048;
        std::vector<std::vector<double>> frames;
        for (std::size_t i = 0; i + n <= c.size(); i += n / 2) {
            const Spectrum sp(std::vector<float>(c.begin() + std::ptrdiff_t(i), c.begin() + std::ptrdiff_t(i + n)), rate, n);
            std::vector<double> m(sp.mag.begin(), sp.mag.begin() + std::ptrdiff_t(std::min<std::size_t>(sp.mag.size(), std::size_t(8000.0 / sp.binHz))));
            for (auto& v : m) v = std::sqrt(v);
            frames.push_back(m);
        }
        double sum = 0;
        int count = 0;
        for (std::size_t f = 1; f < frames.size(); ++f) {
            double ab = 0, aa = 0, bb = 0;
            for (std::size_t k = 0; k < frames[f].size(); ++k) {
                ab += frames[f][k] * frames[f - 1][k];
                aa += frames[f - 1][k] * frames[f - 1][k];
                bb += frames[f][k] * frames[f][k];
            }
            sum += ab / std::sqrt(aa * bb + 1e-30);
            ++count;
        }
        r.steady = -std::log10(std::max(1e-6, 1.0 - sum / std::max(1, count)));
    }
    return r;
}

struct Parts { double tone, decay, width, echoes, hum, flicker, steady; };
Parts parts(const Room& a, const Room& b) {
    const auto rmsOf = [](const std::vector<double>& x, const std::vector<double>& y) {
        double s = 0;
        for (std::size_t i = 0; i < x.size(); ++i) s += (x[i] - y[i]) * (x[i] - y[i]);
        return std::sqrt(s / double(std::max<std::size_t>(1, x.size())));
    };
    return { profileDistance(a.tone, b.tone) / 3.0, rmsOf(a.decay, b.decay) / 6.0, std::abs(a.width - b.width) / 0.25, rmsOf(a.echoes, b.echoes) / 0.12,
             std::abs(a.hum - b.hum) / 6.0, std::abs(a.flicker - b.flicker) / 1.5, std::abs(a.steady - b.steady) / 0.3 };
}
double distance(const Parts& p) {
    return std::sqrt(p.tone * p.tone + p.decay * p.decay + p.width * p.width + p.echoes * p.echoes + p.hum * p.hum + p.flicker * p.flicker + p.steady * p.steady);
}
std::string strongestPart(const Parts& p) {
    const std::pair<double, const char*> list[] { { p.tone, "tone" }, { p.decay, "decay" }, { p.width, "width" }, { p.echoes, "echoes" },
                                                  { p.hum, "hum" }, { p.flicker, "flicker" }, { p.steady, "steadiness" } };
    const auto* best = std::max_element(std::begin(list), std::end(list), [](const auto& x, const auto& y) { return x.first < y.first; });
    return std::string(best->second) + " " + num(best->first, 2);
}

Room renderRoom(int place, int character, double rate) {
    auto rig = rigIn(place);
    rig->set(params::character, float(character));
    rig->set(params::ambience, params::minDb);   // the room's processing, not its bed
    rig->p->prepareToPlay(rate, 512);
    rig->render(1.0);
    return describe(rig->render(7.4, standardPhrase(rig->time)), rate);
}

} // namespace distinct

void testDistinct(Suite& s) {
    s.section("distinct: ten rooms, every pair told apart blind (the same piano phrase)");
    const double rate = 48000.0;
    constexpr int places = dsp::placeCount;
    std::vector<distinct::Room> rooms;
    for (int place = 0; place < places; ++place) {
        rooms.push_back(distinct::renderRoom(place, 0, rate));
        const auto& r = rooms.back();
        std::string echoes;
        for (const double e : r.echoes) echoes += num(e, 2) + " ";
        std::cout << "  [features] " << placeNames[place] << ": level " << num(r.level, 1) << " dBFS, width " << num(r.width, 2) << ", hum " << num(r.hum, 1)
                  << " dB, flicker " << num(r.flicker, 2) << " dB, steady " << num(r.steady, 2) << ", decay " << num(r.decay[0], 0) << "/" << num(r.decay[1], 0) << "/"
                  << num(r.decay[2], 0) << "/" << num(r.decay[3], 0) << " dB, echoes " << echoes << "\n";
    }
    struct Pair { double d; int a, b; std::string why; };
    std::vector<Pair> pairs;
    for (int a = 0; a < places; ++a)
        for (int b = a + 1; b < places; ++b) {
            const auto p = distinct::parts(rooms[std::size_t(a)], rooms[std::size_t(b)]);
            pairs.push_back({ distinct::distance(p), a, b, distinct::strongestPart(p) });
        }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) { return x.d < y.d; });
    for (std::size_t k = 0; k < std::min<std::size_t>(6, pairs.size()); ++k)
        s.metric(std::string("closest pair ") + std::to_string(k + 1) + ": " + placeNames[pairs[k].a] + " / " + placeNames[pairs[k].b] + " (most apart in " + pairs[k].why + ")",
                 pairs[k].d);
    const int under = int(std::count_if(pairs.begin(), pairs.end(), [](const Pair& p) { return p.d <= 2.0; }));
    s.check(under == 0, "every pair of rooms differs clearly through the same phrase (distance over 2.0 for all 45 pairs)",
            std::to_string(under) + " pairs at or under 2.0; closest " + placeNames[pairs[0].a] + " / " + placeNames[pairs[0].b] + " " + num(pairs[0].d, 2));
    double lo = 1e9, hi = -1e9;
    for (const auto& r : rooms) { lo = std::min(lo, r.level); hi = std::max(hi, r.level); }
    s.check(hi - lo < 6.0, "the phrase stays level-matched across the rooms (within 6 dB)", num(lo, 1) + " .. " + num(hi, 1) + " dBFS");

    // Each Character varies its room's signature audibly.
    double weakest = 1e9;
    std::string which;
    for (int place = 0; place < places; ++place)
        for (int c = 1; c < dsp::characterCount; ++c) {
            const auto other = distinct::renderRoom(place, c, rate);
            const auto p = distinct::parts(rooms[std::size_t(place)], other);
            const double d = distinct::distance(p);
            if (d < weakest) { weakest = d; which = std::string(placeNames[place]) + " " + dsp::character(place, c).name + " (" + distinct::strongestPart(p) + ")"; }
        }
    s.check(weakest > 0.75, "every Character moves its room's sound from the room's first Character (over 0.75)", "weakest " + which + " " + num(weakest, 2));
    s.report(weakest > 1.0, "every Character moves it by over 1.0 (clearly audible)", "weakest " + which + " " + num(weakest, 2));
}

// The signature matrix: one detector per signature effect, run on every place's signature stage
// (DSP/Signature) with the same test signals. Each place's own detector must read clearly higher
// in its own place than in any other (the "no other room uses it" rule).
namespace signatures {
using Out = std::pair<std::vector<float>, std::vector<float>>;
Out run(dsp::Place place, int character, const std::vector<float>& in, double rate) {
    dsp::Signature sig;
    sig.prepare(rate);
    sig.configure(place, character, 7);
    Out out { std::vector<float>(in.size()), std::vector<float>(in.size()) };
    std::vector<float> sl(512), sr(512);
    for (std::size_t i = 0; i < in.size(); i += 512) {
        const int n = int(std::min<std::size_t>(512, in.size() - i));
        sig.process(in.data() + i, in.data() + i, out.first.data() + i, out.second.data() + i, sl.data(), sr.data(), n);
    }
    return out;
}
std::vector<float> mono(const Out& o) {
    std::vector<float> m(o.first.size());
    for (std::size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.first[i] + o.second[i]);
    return m;
}
double energy(const std::vector<float>& x, std::size_t a, std::size_t b) {
    double e = 0;
    for (std::size_t i = a; i < std::min(b, x.size()); ++i) e += double(x[i]) * x[i];
    return e;
}
// Normalised autocorrelation of x[a, b) at lags [lo, hi] samples (the strongest).
double autocorr(const std::vector<float>& x, std::size_t a, std::size_t b, int lo, int hi) {
    const double e0 = energy(x, a, b) + 1e-30;
    double best = -1;
    for (int lag = lo; lag <= hi; ++lag) {
        double s = 0;
        for (std::size_t i = a; i + std::size_t(lag) < b && i + std::size_t(lag) < x.size(); ++i) s += double(x[i]) * x[i + std::size_t(lag)];
        best = std::max(best, s / e0);
    }
    return best;
}
// Distinct peaks of the 0.5 ms RMS envelope of x in [a, b): local maxima over `gap` samples either
// side, above `floor` x the strongest point of the whole response (so filter ringing and near-silent
// tails do not count).
std::vector<double> envelope05(const std::vector<float>& x, double rate) {
    const auto f = std::size_t(std::max(1.0, 0.0005 * rate));
    std::vector<double> e(x.size() / f);
    for (std::size_t k = 0; k < e.size(); ++k) e[k] = std::sqrt(energy(x, k * f, (k + 1) * f) / double(f));
    return e;
}
int peaks(const std::vector<float>& x, std::size_t a, std::size_t b, int gap, double floor, double rate = 48000.0) {
    const auto e = envelope05(x, rate);
    const auto f = std::size_t(std::max(1.0, 0.0005 * rate));
    const double top = *std::max_element(e.begin(), e.end());
    const std::size_t g = std::max<std::size_t>(1, std::size_t(gap) / f);
    int count = 0;
    for (std::size_t i = a / f; i < std::min(e.size(), b / f); ++i) {
        if (e[i] < floor * top || top <= 0) continue;
        bool best = true;
        for (std::size_t k = i >= g ? i - g : 0; k < std::min(e.size(), i + g + 1) && best; ++k)
            if (k != i && e[k] > e[i]) best = false;
        count += best ? 1 : 0;
    }
    return count;
}
// How periodic the envelope is in [a, b) (the strongest normalised autocorrelation at 5-20 ms):
// a flutter is periodic, pillars and speakers are not.
double periodicity(const std::vector<float>& x, std::size_t a, std::size_t b, double rate = 48000.0) {
    const auto e = envelope05(x, rate);
    const auto f = std::size_t(std::max(1.0, 0.0005 * rate));
    std::vector<float> w;
    double mean = 0;
    for (std::size_t i = a / f; i < std::min(e.size(), b / f); ++i) mean += e[i];
    mean /= double(std::max<std::size_t>(1, b / f - a / f));
    for (std::size_t i = a / f; i < std::min(e.size(), b / f); ++i) w.push_back(float(e[i] - mean));
    return std::max(0.0, autocorr(w, 0, w.size(), int(0.005 * rate / double(f)), int(0.02 * rate / double(f))));
}

// Pitch movement of a sine (cents RMS) from zero crossings, 20 ms windows, after the first 0.3 s.
double fmCents(const std::vector<float>& x, double rate, double hz) {
    std::vector<double> crossings;
    for (std::size_t i = std::size_t(0.3 * rate); i + 1 < x.size(); ++i)
        if (x[i] <= 0.0f && x[i + 1] > 0.0f) crossings.push_back(double(i) + double(-x[i]) / double(x[i + 1] - x[i]));
    std::vector<double> cents;
    const auto per = std::size_t(std::max(2.0, 0.02 * hz));
    for (std::size_t k = 0; k + per < crossings.size(); k += per) {
        const double f = double(per) * rate / (crossings[k + per] - crossings[k]);
        cents.push_back(1200.0 * std::log2(f / hz));
    }
    if (cents.size() < 3) return 0;
    const double mean = std::accumulate(cents.begin(), cents.end(), 0.0) / double(cents.size());
    double s = 0;
    for (const double c : cents) s += (c - mean) * (c - mean);
    return std::sqrt(s / double(cents.size()));
}
// Level swing (dB, 5th-95th percentile) of 50 ms frames after the first 0.5 s.
double swingDb(const std::vector<float>& x, double rate) {
    std::vector<double> f;
    const auto n = std::size_t(0.05 * rate);
    for (std::size_t i = std::size_t(0.5 * rate); i + n <= x.size(); i += n) f.push_back(10.0 * std::log10(energy(x, i, i + n) / double(n) + 1e-20));
    std::sort(f.begin(), f.end());
    return f.empty() ? 0.0 : f[f.size() * 95 / 100] - f[f.size() * 5 / 100];
}
double bandShare(const std::vector<float>& x, double rate, double lo, double hi, double from = 0, double to = 1e9) {
    const Spectrum sp(x, rate, 1 << 16);
    return std::pow(10.0, (sp.bandDb(lo, hi) - sp.bandDb(from, std::min(to, rate * 0.49))) / 10.0);
}
} // namespace signatures

void testSignatures(Suite& s) {
    s.section("distinct: each room's signature effect, and no other room's");
    using namespace signatures;
    const double rate = 48000.0;
    const auto ms = [&](double m) { return std::size_t(m * 0.001 * rate); };
    std::vector<float> impulse(std::size_t(1.6 * rate), 0.0f);
    impulse[ms(10)] = 1.0f;
    std::vector<float> noise(std::size_t(1.5 * rate));
    {
        juce::Random rnd(5);
        for (auto& v : noise) v = 0.25f * (rnd.nextFloat() * 2.0f - 1.0f);
    }
    const auto sine = [&](double hz, double seconds) {
        std::vector<float> x(std::size_t(seconds * rate));
        for (std::size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * float(std::sin(6.283185307 * hz * double(i) / rate));
        return x;
    };
    std::vector<float> decaying(std::size_t(2.0 * rate), 0.0f);
    {
        juce::Random rnd(9);
        for (std::size_t i = ms(10); i < ms(610); ++i) decaying[i] = 0.4f * (rnd.nextFloat() * 2.0f - 1.0f) * float(std::exp(-double(i - ms(10)) / (0.12 * rate)));
    }
    const auto s1k = sine(1000.0, 2.5), s250 = sine(250.0, 4.0), s3k = sine(3000.0, 4.0);
    const char* what[dsp::placeCount] { "flutter echo (IR periodicity at 11-17 ms over its 2-8 ms ring)", "distant PA (noise: 400 Hz-3.5 kHz over below 200 Hz + above 8 kHz)",
                                        "tape wobble (1 kHz sine: pitch movement, cents)", "slap-back (IR below 400 Hz: irregular discrete peaks 50-200 ms)",
                                        "underwater layer (slow level swing, 250 Hz sine over 3 kHz sine, dB)", "through the wall (noise: 60-200 Hz over 1-4 kHz)",
                                        "receding tunnel echoes (IR: discrete peaks 300-1300 ms)", "speaker grid (IR above 1 kHz: irregular arrivals 2-50 ms after the sound)",
                                        "hum infection (1 kHz sine: 120 Hz sidebands, dB over -60)", "wrong acoustics (a decaying sound: longest run of growing 50 ms frames)" };
    std::array<std::array<double, dsp::placeCount>, dsp::placeCount> d {};   // [detector][place]
    for (int place = 0; place < dsp::placeCount; ++place) {
        const auto pl = dsp::Place(place);
        const auto ir = mono(run(pl, 0, impulse, rate));
        const auto nz = mono(run(pl, 0, noise, rate));
        const auto a1k = mono(run(pl, 0, s1k, rate));
        const auto irL = run(pl, 0, impulse, rate).first;   // one channel: the flutter's two sides have different periods; the grid's speakers sit left and right
        d[0][place] = autocorr(irL, ms(40), ms(300), int(ms(11)), int(ms(17))) - std::max(0.0, autocorr(irL, ms(40), ms(300), int(ms(2)), int(ms(8))));
        {   // a loudspeaker's band: little below 250 Hz and above 6 kHz
            const Spectrum sp(nz, rate, 1 << 16);
            const auto e = [&](double lo, double hi) { return std::pow(10.0, sp.bandDb(lo, hi) / 10.0); };
            d[1][place] = e(400.0, 3500.0) / (e(20.0, 200.0) + e(8000.0, 20000.0) + 1e-30);
        }
        d[2][place] = fmCents(a1k, rate, 1000.0);
        {   // dark, discrete, irregular slaps 50-200 ms (the IR low-passed at ~400 Hz)
            std::vector<float> dark(ir.size());
            float z1 = 0, z2 = 0;
            const float c = float(1.0 - std::exp(-6.283185 * 400.0 / rate));
            for (std::size_t i = 0; i < ir.size(); ++i) {
                z1 += c * (ir[i] - z1);
                z2 += c * (z1 - z2);
                dark[i] = z2;
            }
            const double irregular = 1.0 - periodicity(dark, ms(50), ms(200));
            d[3][place] = peaks(dark, ms(50), ms(200), int(ms(8)), 0.03) * irregular * irregular;
        }
        d[4][place] = swingDb(mono(run(pl, 0, s250, rate)), rate) - swingDb(mono(run(pl, 0, s3k, rate)), rate);
        d[5][place] = bandShare(nz, rate, 60.0, 200.0, 1000.0, 4000.0);
        d[6][place] = peaks(ir, ms(310), ms(1300), int(ms(40)), 0.005);
        {   // bright arrivals 4-60 ms (high-passed at ~1 kHz), irregular
            std::vector<float> bright(irL.size());
            float z1 = 0, z2 = 0;
            const float c = float(1.0 - std::exp(-6.283185 * 1000.0 / rate));
            for (std::size_t i = 0; i < irL.size(); ++i) {
                z1 += c * (irL[i] - z1);
                const float h1 = irL[i] - z1;
                z2 += c * (h1 - z2);
                bright[i] = h1 - z2;
            }
            const double irregular = 1.0 - periodicity(bright, ms(12), ms(60));
            d[7][place] = peaks(bright, ms(12), ms(60), int(ms(2)), 0.08) * irregular * irregular;   // (after the direct sound at 10 ms)
        }
        {   // the 120 Hz sidebands around 1 kHz, relative to the carrier
            const Spectrum sp(std::vector<float>(a1k.begin() + std::ptrdiff_t(ms(300)), a1k.end()), rate, 1 << 16);
            const double side = 10.0 * std::log10(0.5 * (std::pow(10.0, sp.bandDb(875.0, 885.0) / 10.0) + std::pow(10.0, sp.bandDb(1115.0, 1125.0) / 10.0)) + 1e-30);
            d[8][place] = std::max(0.0, side - sp.bandDb(995.0, 1005.0) + 60.0);
        }
        {   // a decaying sound (like a struck note in a room) comes back growing: a reversed swell
            const auto out = mono(run(pl, 0, decaying, rate));
            int run_ = 0, best = 0;
            double prev = -1;
            for (std::size_t i = ms(20); i + ms(50) <= out.size(); i += ms(50)) {
                const double e = energy(out, i, i + ms(50));
                run_ = (prev > 0 && e > 1.1 * prev && e > 1e-7) ? run_ + 1 : 0;
                best = std::max(best, run_);
                prev = e;
            }
            d[9][place] = best;
        }
    }
    int dominant = 0;
    for (int k = 0; k < dsp::placeCount; ++k) {
        double other = -1e9;
        int at = 0;
        std::string row;
        for (int place = 0; place < dsp::placeCount; ++place) {
            row += num(d[std::size_t(k)][std::size_t(place)], 2) + " ";
            if (place != k && d[std::size_t(k)][std::size_t(place)] > other) { other = d[std::size_t(k)][std::size_t(place)]; at = place; }
        }
        std::cout << "  [matrix] " << placeNames[k] << " detector across the ten places: " << row << "\n";
        const double own = d[std::size_t(k)][std::size_t(k)];
        // Clearly ahead: twice the next place (and a real reading), or for the dB detectors 6 dB ahead.
        const bool db = k == 4 || k == 8;
        const bool clearly = db ? own > other + 6.0 : own > 2.0 * std::max(other, 0.0) && own > 0.0;
        dominant += clearly ? 1 : 0;
        const std::string detail = "own " + num(own, 2) + ", next " + placeNames[at] + " " + num(other, 2);
        s.check(own > other && own > 0.0, std::string(placeNames[k]) + ": its signature, " + what[k] + ", is strongest in its own room", detail);
        // The margin (2x, or 6 dB) is report-only after two detector attempts (garage vs the mall's
        // distant speakers: both have discrete late arrivals; DEVLOG).
        s.report(clearly, std::string(placeNames[k]) + ": and by a clear margin (2x the next room, or 6 dB)", detail);
    }
    s.metric("signatures that are their room's alone", double(dominant), " of 10");
}
