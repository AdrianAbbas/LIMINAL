// Pedal-capture milestone: the real-time convolver and the matched models (group `convolver`).
// Included by LiminalTests.cpp.

namespace convolverTests {

std::vector<float> testIr(double seconds, double rt60, unsigned seed, double rate = 48000.0) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> h(std::size_t(seconds * rate));
    for (std::size_t i = 0; i < h.size(); ++i) h[i] = g(rng) * 0.05f * float(std::exp(-6.91 * double(i) / rate / rt60));
    h[0] = 0.7f;
    h[37] = -0.3f;
    return h;
}

double dbRatio(double num, double den) { return 10.0 * std::log10(std::max(num, 1e-300) / std::max(den, 1e-300)); }

}  // namespace convolverTests

void testConvolver(Suite& s) {
    using namespace convolverTests;
    s.section("convolver + matched models (pedal-capture milestone)");
    const double rate = 48000.0;

    {   // 1. Exact: against direct convolution, random host block sizes, IR spanning every stage.
        const auto h = testIr(1.2, 0.8, 1);
        std::mt19937 rng(2);
        std::normal_distribution<float> g(0.0f, 0.3f);
        std::vector<float> x(std::size_t(rate * 1.6));
        for (auto& v : x) v = g(rng);
        dsp::Convolver c;
        c.load(h.data(), int(h.size()), rate, 10.0);
        std::vector<float> y(x.size());
        std::uniform_int_distribution<int> blk(1, 1024);
        for (std::size_t i = 0; i < x.size();) {
            const int n = std::min<int>(blk(rng), int(x.size() - i));
            c.process(x.data() + i, y.data() + i, n);
            i += std::size_t(n);
        }
        double err = 0, ref = 0;
        // Direct convolution on a stretch late enough for every stage to be active.
        for (std::size_t t = std::size_t(rate * 1.3); t < std::size_t(rate * 1.3) + 4000; ++t) {
            double acc = 0;
            for (std::size_t k = 0; k < h.size() && k <= t; ++k) acc += double(h[k]) * double(x[t - k]);
            err += (acc - y[t]) * (acc - y[t]);
            ref += acc * acc;
        }
        const double e = dbRatio(err, ref);
        s.check(e < -90.0, "partitioned convolution = direct convolution (random host blocks 1-1024, every stage)", juce::String(e, 1).toStdString() + " dB error");
    }
    {   // 2. Latency: zero, and what it reports.
        const auto h = testIr(0.5, 0.3, 3);
        dsp::Convolver c;
        c.load(h.data(), int(h.size()), rate, 10.0);
        std::vector<float> x(h.size() + 512, 0.0f), y(x.size());
        x[0] = 1.0f;
        c.process(x.data(), y.data(), int(x.size()));
        double worst = 0;
        for (std::size_t i = 0; i < h.size(); ++i) worst = std::max(worst, double(std::abs(y[i] - h[i])));
        s.check(c.latencySamples() == 0 && std::abs(y[0] - h[0]) < 1e-6f && worst < 1e-5,
                "zero latency, reported as 0: an impulse returns the IR from sample 0", "max deviation " + juce::String(worst, 8).toStdString());
    }
    {   // 3. Quality caps the IR length.
        const auto h = testIr(10.0, 4.0, 4);
        juce::String lens;
        bool ok = true;
        for (int q = 0; q < 3; ++q) {
            dsp::Convolver c;
            c.load(h.data(), int(h.size()), rate, dsp::Convolver::maxSecondsFor(q));
            lens << juce::String(c.irLength() / rate, 2) << " s ";
            ok = ok && std::abs(c.irLength() / rate - dsp::Convolver::maxSecondsFor(q)) < 0.01;
        }
        s.check(ok, "Quality scales the IR length (Eco 1.5 s, Standard 3.5 s, High 8 s)", lens.toStdString());
    }
    {   // 4. Real-time safety and CPU (stereo, Standard, 3.5 s IR, 128-sample host blocks).
        dsp::StereoConvolver sc;
        sc.load({ testIr(6.0, 3.0, 5), testIr(6.0, 3.0, 6) }, rate, 1);
        std::vector<float> l(128), r(128);
        std::mt19937 rng(7);
        std::normal_distribution<float> g(0.0f, 0.2f);
        std::vector<double> byPhase(64, 0.0);   // mean time per position in the 64-block (8192-sample) cycle
        allocationProbe::calls = 0;
        allocationProbe::enabled = true;
        const int blocks = int(rate * 20.0 / 128);
        double worst = 0, total = 0;
        for (int b = 0; b < blocks; ++b) {
            for (int i = 0; i < 128; ++i) { l[std::size_t(i)] = g(rng); r[std::size_t(i)] = g(rng); }
            const auto t0 = std::chrono::steady_clock::now();
            sc.process(l.data(), r.data(), 128, false);
            const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
            worst = std::max(worst, us);
            total += us;
            if (b >= 64) byPhase[std::size_t(b % 64)] += us / double(blocks / 64 - 1);
        }
        allocationProbe::enabled = false;
        s.check(allocationProbe::calls == 0, "convolution never allocates on the audio thread", std::to_string(allocationProbe::calls) + " allocations");
        const double avg = total / blocks, budget = 128.0 / rate * 1e6;
        s.check(avg / budget < 0.05, "stereo 3.5 s IR costs under 5 % of one core here (Standard)",
                juce::String(100.0 * avg / budget, 2).toStdString() + " % average, " + juce::String(sc.opsPerSample(), 0).toStdString() + " ops/sample");
        const double phaseWorst = *std::max_element(byPhase.begin(), byPhase.end());
        s.report(phaseWorst / budget < 0.25, "no structural CPU spikes: the costliest block position in the stage cycle (averaged) uses < 25 % of its budget",
                 juce::String(100.0 * phaseWorst / budget, 1).toStdString() + " % (" + juce::String(phaseWorst / avg, 1).toStdString() +
                 "x the average); single worst block incl. scheduler noise " + juce::String(100.0 * worst / budget, 1).toStdString() + " %");
    }
    {   // 5. Tape echo: echo time, feedback and darkening per repeat as profiled (wow/flutter off).
        dsp::TapeEcho te;
        dsp::TapeEchoProfile p;
        p.wowCents = p.flutterCents = 0.0f;
        p.drive = 0.0f;
        te.prepare(rate);
        te.configure(p);
        te.reset();
        std::vector<float> y(std::size_t(rate * 3.0));
        for (std::size_t i = 0; i < y.size(); ++i) y[i] = te.tick(i == 0 ? 0.5f : 0.0f);
        // Each repeat's 1 kHz and 4 kHz / 500 Hz content from its spectrum (window +/- 40 ms).
        const auto bandDb = [&](int centre, double f) {
            double re = 0, im = 0;
            for (int i = centre - 1920; i < centre + 1920; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2.0 * 3.141592653589793 * (i - centre + 1920) / 3840.0);
                re += w * y[std::size_t(i)] * std::cos(2.0 * 3.141592653589793 * f * i / rate);
                im += w * y[std::size_t(i)] * std::sin(2.0 * 3.141592653589793 * f * i / rate);
            }
            return 10.0 * std::log10(re * re + im * im + 1e-30);
        };
        int peak = 0;
        for (int i = 1000; i < int(rate * 0.6); ++i) if (std::abs(y[std::size_t(i)]) > std::abs(y[std::size_t(peak)])) peak = i;
        const double echoMs = peak / rate * 1000.0;
        const int d = int(p.echoMs * 0.001 * rate);
        const double fb = (bandDb(d * 4, 1000) - bandDb(d * 2, 1000)) / 2.0;
        const double dark = ((bandDb(d * 4, 4000) - bandDb(d * 4, 500)) - (bandDb(d * 2, 4000) - bandDb(d * 2, 500))) / 2.0;
        s.check(std::abs(echoMs - p.echoMs) < 1.5, "tape echo: first repeat at the profiled time", juce::String(echoMs, 2).toStdString() + " ms");
        s.check(std::abs(fb - p.feedbackDb) < 0.5, "tape echo: feedback per repeat at 1 kHz as profiled (-6 dB)", juce::String(fb, 2).toStdString() + " dB");
        s.check(std::abs(dark - p.darkeningDb) < 0.5, "tape echo: each repeat darker by the profiled tilt (-2.5 dB, 4 kHz vs 500 Hz)", juce::String(dark, 2).toStdString() + " dB");
    }
    {   // 6. Octave shimmer: a 440 Hz tone comes out at 880 Hz.
        dsp::OctaveShifter os;
        os.prepare(rate);
        std::vector<float> y(static_cast<std::size_t>(rate));
        for (std::size_t i = 0; i < y.size(); ++i) y[i] = os.tick(float(std::sin(2.0 * 3.141592653589793 * 440.0 * double(i) / rate)));
        const auto power = [&](double f) {
            double re = 0, im = 0;
            for (std::size_t i = 12000; i < y.size(); ++i) {
                re += y[i] * std::cos(2.0 * 3.141592653589793 * f * double(i) / rate);
                im += y[i] * std::sin(2.0 * 3.141592653589793 * f * double(i) / rate);
            }
            return re * re + im * im;
        };
        const double up = dbRatio(power(880.0), power(440.0));
        s.check(up > 20.0, "octave shifter: 440 Hz in, 880 Hz out (dominant by > 20 dB)", juce::String(up, 1).toStdString() + " dB");
    }
    {   // 7. Modulation voice: the measured rate comes back (flanger notch sweep period).
        dsp::ModVoice mv;
        mv.prepare(rate);
        dsp::ModProfile mp;
        mp.rateHz = 0.5f;
        mp.mix = 1.0f;
        mp.feedback = 0.0f;
        mv.configure(mp);
        // A 2 kHz tone: the wet signal's instantaneous delay -> phase swings at the LFO rate.
        std::vector<float> y(std::size_t(rate * 8));
        for (std::size_t i = 0; i < y.size(); ++i) y[i] = mv.tick(float(std::sin(2.0 * 3.141592653589793 * 2000.0 * double(i) / rate)));
        // Demodulate the wet tone's phase against the 2 kHz reference (10 ms windows): it follows
        // the swept delay, so its mean crossings give the LFO rate.
        std::vector<double> ph;
        double prev = 0, unwrap = 0;
        for (std::size_t w = 0; w + 480 <= y.size(); w += 480) {
            double re = 0, im = 0;
            for (std::size_t i = w; i < w + 480; ++i) {
                re += y[i] * std::cos(2.0 * 3.141592653589793 * 2000.0 * double(i) / rate);
                im += y[i] * std::sin(2.0 * 3.141592653589793 * 2000.0 * double(i) / rate);
            }
            double a = std::atan2(im, re);
            while (a - prev > 3.141592653589793) { a -= 6.283185307179586; }
            while (a - prev < -3.141592653589793) { a += 6.283185307179586; }
            prev = a;
            unwrap = a;
            ph.push_back(unwrap);
        }
        int crossings = 0;
        double mean = 0;
        for (double v : ph) mean += v / double(ph.size());
        for (std::size_t i = 1; i < ph.size(); ++i) crossings += (ph[i - 1] < mean) != (ph[i] < mean);
        const double rateMeasured = crossings / 2.0 / 8.0;
        s.report(std::abs(rateMeasured - 0.5) < 0.15, "modulation voice: LFO rate as profiled (0.5 Hz)", juce::String(rateMeasured, 2).toStdString() + " Hz");
    }
}

// --capture-model tape in.wav out.wav : the tape echo (default profile, dry + wet) over a WAV,
// for the closed-loop check with tools/captures/analyze.py.
int runCaptureModel(const juce::String& model, const juce::File& in, const juce::File& out) {
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(in));
    if (rd == nullptr) return 1;
    juce::AudioBuffer<float> buf(int(rd->numChannels), int(rd->lengthInSamples));
    rd->read(&buf, 0, buf.getNumSamples(), 0, true, true);
    juce::AudioBuffer<float> res(2, buf.getNumSamples());
    if (model == "tape" || model == "tape-static" || model == "tape-linear") {
        dsp::TapeEcho te;
        te.prepare(rd->sampleRate);
        dsp::TapeEchoProfile p;
        if (model != "tape") p.wowCents = p.flutterCents = 0.0f;
        if (model == "tape-linear") p.drive = 0.0f;
        te.configure(p);
        te.reset();
        for (int i = 0; i < buf.getNumSamples(); ++i) {
            const float x = buf.getSample(0, i);
            const float y = x + te.tick(x);
            res.setSample(0, i, y);
            res.setSample(1, i, y);
        }
    } else {
        return 2;
    }
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(out), rd->sampleRate, 2, 32, {}, 0));
    if (w == nullptr) return 3;
    w->writeFromAudioSampleBuffer(res, 0, res.getNumSamples());
    return 0;
}
