#include "DSP/Library.h"

namespace liminal::dsp {

namespace library {
const Info& info(int instrument) {
    static const Info table[total] {
        { "Forgotten Piano", "PIANO", "", "A neglected upright in an empty building", "piano, piano-long" },
        { "Faded Synth", "SYNTH", "", "A worn analog synthesizer", "analog synth patches" },
        { "Glass Memories", "GLASS", "", "Struck and rubbed glass, half-heard", "" },
        { "Empty Choir", "CHOIR", "", "A choir remembered in an empty building", "voice sweeps, choir stacks" },
        { "Lost Music Box", "MUSIC BOX", "", "A worn music box: kalimba tines and the piano's top", "kalimba, piano-long" },
        { "Hollow Drone", "DRONE", "", "Held voices, bowed strings and a pad, hollowed out", "voice drones, violin, pad, static bass" },
        { "Old Guitar", "GUITAR", "OTHER ROOM", "A classical guitar heard from another room", "guitar" },
        { "Faded Strings", "STRINGS", "SECTION", "Bowed violin sustain: a faded string section", "violin-sustain" },
        { "Hallway Pizzicato", "PIZZICATO", "COL LEGNO", "Plucked and tapped violin, sparse, echoing down a hall", "violin-pizz, violin-collegno" },
        { "Toy Piano", "TOY PIANO", "BROKEN", "A small, slightly broken toy piano", "piano, kalimba" },
        { "Lounge E-Piano", "E-PIANO", "BELL", "A soft electric piano in a hotel lobby", "" },
        { "Elevator Vibraphone", "VIBRAPHONE", "TREMOLO", "Muzak vibraphone with a slow motor tremolo", "" },
        { "Mall Chimes", "CHIMES", "PA", "Public-address chime tones over an empty concourse", "" },
        { "Solo Voice", "VOICE", "VOWEL", "One distant singer, a soft ooh and aah", "voice-ooh, voice-aah" },
        { "Hummed Memory", "HUM", "MEMORY", "A soft hum, half-remembered", "voice-mm" },
        { "Cassette Strings", "CASSETTE", "TAPE WEAR", "A soft analog pad played from a worn tape", "mlg-softpad-long" },
        { "Arcade Lead", "ARCADE", "GRIT", "An empty arcade's game-console lead", "mlg-square-long, mlg-cyberpunk-long" },
        { "Basement Bass", "BASS", "FLOOR", "A dull bass from the floor below", "mlg-staticbass-long" },
        { "Warped Organ", "ORGAN", "WOW", "An old combo organ: wow, and notes that are dying", "" },
        { "Bowed Glass", "BOWED GLASS", "BOW", "A glassy bowed pad", "" },
    };
    return table[std::clamp(instrument, 0, total - 1)];
}
} // namespace library

namespace {
using namespace library;
float expRange(float x, float octaves) { return std::exp2(octaves * (x - 0.5f)); }   // x = 0.5 -> 1

// What each instrument does with its envelope (seconds at the controls' middle).
struct Design {
    float attack;     // the onset (Softness stretches it)
    float held;       // a decay while held (0 = none: it sustains or decays by itself)
    float release;    // after the key (Decay stretches it)
    float toneHz;     // the per-voice low-pass at Tone 0.5, velocity 1
    float velocity;   // 1 = full velocity response, 0 = none
    bool sustained;   // a held sound (pads, bows, voices, organ)
};
Design design(int kind) {
    switch (kind) {
    case oldGuitar: return { 0.002f, 0.0f, 0.28f, 6500.0f, 1.0f, false };
    case fadedStrings: return { 0.16f, 0.0f, 0.75f, 5200.0f, 0.6f, true };
    case hallwayPizzicato: return { 0.0035f, 0.0f, 0.9f, 7000.0f, 1.0f, false };
    case toyPiano: return { 0.0015f, 1.1f, 0.22f, 9000.0f, 1.0f, false };
    case loungeEPiano: return { 0.002f, 4.5f, 0.3f, 5200.0f, 1.0f, false };
    case elevatorVibraphone: return { 0.0015f, 6.0f, 0.4f, 9000.0f, 0.9f, false };
    case mallChimes: return { 0.003f, 0.0f, 2.4f, 12000.0f, 0.7f, false };
    case soloVoice: return { 0.14f, 0.0f, 0.9f, 5000.0f, 0.6f, true };
    case hummedMemory: return { 0.32f, 0.0f, 1.4f, 2600.0f, 0.5f, true };
    case cassetteStrings: return { 0.3f, 0.0f, 1.2f, 4200.0f, 0.5f, true };
    case arcadeLead: return { 0.004f, 0.0f, 0.12f, 4200.0f, 0.6f, true };
    case basementBass: return { 0.006f, 9.0f, 0.25f, 2200.0f, 0.8f, true };
    case warpedOrgan: return { 0.006f, 0.0f, 0.08f, 7000.0f, 0.3f, true };
    default: return { 0.8f, 0.0f, 1.8f, 9000.0f, 0.5f, true };   // bowed glass
    }
}
// The recordings each instrument plays (first, second); nullptr = none.
std::pair<const char*, const char*> setsOf(int kind) {
    switch (kind) {
    case oldGuitar: return { "guitar", nullptr };
    case fadedStrings: return { "violin-sustain", nullptr };
    case hallwayPizzicato: return { "violin-pizz", "violin-collegno" };
    case toyPiano: return { "piano", "kalimba" };
    case soloVoice: return { "voice-ooh", "voice-aah" };
    case hummedMemory: return { "voice-mm", nullptr };
    case cassetteStrings: return { "mlg-softpad-long", nullptr };
    case arcadeLead: return { "mlg-square-long", "mlg-cyberpunk-long" };
    case basementBass: return { "mlg-staticbass-long", nullptr };
    default: return { nullptr, nullptr };
    }
}
float pulseWave(float p, float w, float inc) {
    float q = p + 1.0f - w;
    q -= std::floor(q);
    return blepSaw(p, inc) - blepSaw(q, inc) + (2.0f * w - 1.0f);
}
} // namespace

float Library::trimDb(int kind, bool recorded) {
    //                         guitar strings pizz  toy    ep     vib    chimes voice  hum    tape   arcade bass   organ  glass
    static const float standIn[] { 8.6f, -7.7f, 16.1f, 2.1f, -10.8f, -8.4f, -8.0f, -4.4f, -10.8f, -9.1f, -17.4f, -12.3f, -18.1f, -14.6f };
    static const float rec[] { 4.1f, 5.2f, 4.6f, 12.8f, -10.8f, -8.4f, -8.0f, 2.0f, 3.8f, 1.8f, 3.5f, 8.0f, -18.1f, -14.6f };
    const int k = std::clamp(kind, 0, kinds - 1);
    return recorded ? rec[k] : standIn[k];
}

void Library::setKind(int k) {
    k = std::clamp(k, 0, kinds - 1);
    if (k == kindNow) return;
    kindNow = k;
    reset();
}

void Library::prepare(double sampleRate) {
    sr = std::clamp(sampleRate, 8000.0, 384000.0);
    echoL.assign(std::size_t(0.6 * sr) + 4, 0.0f);
    echoR.assign(echoL.size(), 0.0f);
    reset();
}

void Library::reset() {
    for (auto& v : pool.voices) {
        v.note = -1;
        v.fade = 1;
        v.stolen = false;
    }
    pool.pedal = false;
    pool.counter = 0;
    rng = {};
    rng.state = 0x2F6B1D35u ^ std::uint32_t(kindNow * 7919);
    bend = 1.0f;
    clock = 0;
    wow = {};
    wowCents = 0;
    lfoPhase = flutterPhase = 0;
    for (auto& b : postL) b.clear();
    for (auto& b : postR) b.clear();
    std::fill(echoL.begin(), echoL.end(), 0.0f);
    std::fill(echoR.begin(), echoR.end(), 0.0f);
    echoWrite = 0;
    echoLp[0] = echoLp[1] = crushOut[0] = crushOut[1] = 0;
    updatePost();
}

bool Library::recordedNow() const { return setsOf(kindNow).first != nullptr && recordings(setsOf(kindNow).first) != nullptr; }

void Library::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff(note); return; }
    auto& v = pool.allocate(note, voices(polyphony), sr, 0.02, [this](const Voice& x) { return level(x); });
    pool.begin(v, note);
    v.shape = takeShape();
    startVoice(v, note, std::clamp(velocity, 0.0f, 1.0f));
}

void Library::startVoice(Voice& v, int note, float velocity) {
    const auto& st = settings;
    const auto d = design(kindNow);
    v.velocity = velocity;
    v.hz = noteHz(float(note));
    v.age = 0;
    v.amp = 0;
    v.attacking = true;
    v.scoop = v.sag = v.sagRate = 0;
    v.burst = 0;
    v.mix = st.colour;
    v.wobble.start(rng);
    v.noise.state = rng.state ^ 0x9E3779B9u ^ std::uint32_t(note * 2654435761u);
    if (v.noise.state == 0) v.noise.state = 1;
    v.lpL.z = v.lpR.z = 0;
    // Envelope: Softness stretches the onset (x0.25 .. x4), Decay the release and any held decay.
    const float attack = d.attack * expRange(st.softness, 4.0f);
    v.attackCoef = float(1.0 - std::pow(0.3 / 1.3, 1.0 / (std::max(0.0005, double(attack)) * sr)));
    v.releaseCoef = t60(d.release * expRange(st.decay, 3.0f), sr);
    v.heldCoef = d.held > 0.0f ? t60(d.held * expRange(st.decay, 3.0f) * std::pow(261.6f / v.hz, 0.25f), sr) : 1.0f;
    // Velocity: the instrument's own response.
    const float curve = 0.12f + 0.88f * std::pow(velocity, 1.5f);
    v.gain = (1.0f - d.velocity) * 0.55f + d.velocity * curve;
    // Pan: alternate sides, wider with Width.
    const float side = (pool.counter % 2 == 0 ? 1.0f : -1.0f) * (0.08f + 0.55f * st.width) * (0.4f + 0.6f * rng.uniform());
    const auto g = panGains(side);
    v.panL = g.l;
    v.panR = g.r;
    // Tone: Tone x4 / x0.25, velocity brightens, darker as the key goes down for most.
    v.tone = std::clamp(d.toneHz * expRange(st.tone, 4.0f) * (0.55f + 0.45f * velocity), 150.0f, 0.45f * float(sr));
    v.lpL.setHz(v.tone, sr);
    v.lpR.setHz(v.tone, sr);
    // Age: a fixed per-key mistuning (the instrument's own, not Liminality's).
    const float keyCents = (keyHash(note, 31 + kindNow) - 0.5f) * 2.0f * st.age * st.age * 9.0f;
    v.shape.cents += keyCents;
    if (kindNow == hummedMemory) v.scoop = -(12.0f + 50.0f * st.colour);
    if (kindNow == soloVoice) v.scoop = -8.0f;
    if (kindNow == warpedOrgan && keyHash(note, 77 + int(pool.counter % 5)) < 0.08f + 0.45f * st.age * st.colour) {
        v.sagRate = -(4.0f + 10.0f * rng.uniform()) * (0.4f + st.colour);   // this note is dying: it sags
        v.heldCoef = t60(14.0 + 10.0 * rng.uniform(), sr);
    }
    v.engine = Engine::recorded;
    if (!startRecorded(v, note, velocity)) startSynth(v, note, velocity);
    control(v, 0.0f);
}

bool Library::startRecorded(Voice& v, int note, float velocity) {
    const auto [firstId, secondId] = setsOf(kindNow);
    if (firstId == nullptr) return false;
    const auto* a = recordings(firstId);
    if (a == nullptr) return false;
    // Beyond the recordings' range (an octave past the last note) the stand-in plays.
    if (float(note) < a->lowRoot - 12.0f || float(note) > a->highRoot + 12.0f) return false;
    const auto& st = settings;
    const auto* b = secondId != nullptr ? recordings(secondId) : nullptr;
    for (auto& r : v.r) r.on = false;
    v.rCents = { 0, 0, 0 };
    const float w = 0.1f + 0.6f * st.width;
    const auto setPan = [&](int k, float p) { const auto g = panGains(p); v.rL[std::size_t(k)] = g.l; v.rR[std::size_t(k)] = g.r; };
    const auto start = [&](int k, const samples::Instrument* set, float vel, float pick, double from = 1.0) {
        if (set == nullptr) return false;
        const auto* z = set->find(note, vel, pick);
        if (z == nullptr) return false;
        auto& r = v.r[std::size_t(k)];
        if (!r.start(z, set->rate, sr, float(note), from)) return false;
        r.gain = dbGain(-21.0f - z->loudnessDb);   // every note at one loudness; velocity is the curve
        return true;
    };
    const float pick = rng.uniform();
    bool ok = false;
    switch (kindNow) {
    case oldGuitar:
        ok = start(0, a, velocity, pick);
        setPan(0, v.panL > v.panR ? -0.3f * w : 0.3f * w);
        break;
    case fadedStrings: {   // three players: different takes, a little apart, entering a little late
        const float apart = 3.0f + 11.0f * st.colour;
        ok = start(0, a, 0.5f, 0.15f);
        if (ok && st.colour > 0.02f) {
            start(1, a, 0.5f, 0.5f);
            start(2, a, 0.5f, 0.85f);
            v.r[1].wait = int(0.021 * sr);
            v.r[2].wait = int(0.037 * sr);
            v.rCents = { 0.0f, -apart, apart * 0.8f };
            const float k = st.colour;
            v.r[0].gain *= 1.0f - 0.35f * k;
            v.r[1].gain *= 0.75f * k;
            v.r[2].gain *= 0.7f * k;
        }
        setPan(0, 0.0f);
        setPan(1, -w);
        setPan(2, w);
        break;
    }
    case hallwayPizzicato:
        ok = start(0, a, velocity, pick);
        if (ok && b != nullptr && st.colour > 0.02f && start(1, b, velocity, rng.uniform())) v.r[1].gain *= 1.6f * st.colour;
        setPan(0, v.panL > v.panR ? -0.4f * w : 0.4f * w);
        setPan(1, v.panL > v.panR ? 0.3f * w : -0.3f * w);
        break;
    case toyPiano: {
        ok = start(0, a, velocity, 0.0f);
        if (ok && b != nullptr && start(1, b, velocity, 0.0f)) v.r[1].gain *= 0.55f;
        // Broken: some rods are out, a second strike rings a little apart.
        const float broken = keyHash(note, 404) < 0.2f + 0.6f * st.colour ? st.colour : 0.15f * st.colour;
        if (ok && broken > 0.02f && start(2, a, velocity, 0.0f)) {
            v.r[2].gain *= 0.5f * broken;
            v.rCents[2] = 18.0f + 30.0f * keyHash(note, 405);
        }
        setPan(0, 0.0f);
        setPan(1, -0.3f * w);
        setPan(2, 0.3f * w);
        break;
    }
    case soloVoice:
        ok = start(0, a, 0.25f, 0.0f);
        if (ok && b != nullptr) start(1, b, 0.25f, 0.0f);
        setPan(0, 0.0f);
        setPan(1, 0.0f);
        break;
    case hummedMemory:
        ok = start(0, a, 0.25f, 0.0f);
        setPan(0, 0.0f);
        break;
    case cassetteStrings: {
        ok = start(0, a, 0.5f, 0.0f);
        const auto* z = v.r[0].zone;
        if (ok && start(1, a, 0.5f, 0.0f, z != nullptr && z->loops() ? double(z->loopStart) * 0.5 + 1.0 : 1.0)) {
            v.rCents = { -4.0f - 6.0f * st.width, 4.0f + 6.0f * st.width, 0.0f };
            v.r[0].gain *= 0.72f;
            v.r[1].gain *= 0.72f;
        }
        setPan(0, -w);
        setPan(1, w);
        break;
    }
    case arcadeLead:
        ok = start(0, a, 0.5f, 0.0f);
        if (ok && b != nullptr) start(1, b, 0.5f, 0.0f);
        setPan(0, 0.0f);
        setPan(1, 0.0f);
        break;
    case basementBass:
        ok = start(0, a, 0.5f, 0.0f);
        setPan(0, 0.0f);
        break;
    default:
        break;
    }
    if (!ok) {
        for (auto& r : v.r) r.on = false;
        return false;
    }
    v.engine = Engine::recorded;
    v.stereo = true;
    v.gain *= dbGain(trimDb(kindNow, true) - trimDb(kindNow, false));   // the stand-in's trim is applied to both below
    return true;
}

void Library::startSynth(Voice& v, int note, float velocity) {
    const auto& st = settings;
    const float hz = v.hz;
    v.stereo = false;
    const auto setModes = [&](std::initializer_list<float> ratios, std::initializer_list<float> amps, std::initializer_list<float> held,
                              std::initializer_list<float> damped, float brightTilt) {
        v.modes.clear();
        int k = 0;
        auto ra = ratios.begin();
        auto am = amps.begin();
        auto he = held.begin();
        auto da = damped.begin();
        for (; ra != ratios.end(); ++ra, ++am, ++he, ++da, ++k) {
            const float radians = twoPi * hz * *ra / float(sr);
            const float tilt = std::pow(*ra, -brightTilt);
            const float hold = *he > 500.0f ? 1.0f : t60(*he, sr);
            const float damp = *da > 500.0f ? 1.0f : t60(*da, sr);
            v.modes.set(k, radians < 0.95f * pi ? *am * tilt : 0.0f, 0.0f, radians, hold, damp, v.panL, v.panR);
        }
        v.engine = Engine::modal;
    };
    const float decayScale = expRange(st.decay, 3.0f);
    const float mallet = 0.25f + 1.1f * st.softness - 0.5f * velocity;   // softer mallets lose the upper partials
    switch (kindNow) {
    case oldGuitar:
    case hallwayPizzicato: {
        v.engine = Engine::pluck;
        const float period = float(sr) / hz;
        v.delay = v.delayTarget = std::clamp(period - 1.0f, 2.0f, float(lineSize - 4));
        const bool pizz = kindNow == hallwayPizzicato;
        const float seconds = (pizz ? 0.55f : 3.4f) * decayScale * std::pow(110.0f / hz, 0.35f);
        v.loss = std::pow(10.0f, -3.0f * period / (seconds * float(sr)));
        v.bright = std::clamp(0.24f - 0.16f * velocity * expRange(st.tone, 1.0f) * 0.7f + 0.06f * st.softness, 0.04f, 0.25f);
        std::fill(v.line.begin(), v.line.end(), 0.0f);
        // The pluck: one period of soft noise, a pluck position (a comb at 1/7), darker when soft.
        const int length = std::clamp(int(period), 2, lineSize - 2);
        float lp = 0, prev = 0;
        const float c = 0.25f + 0.6f * velocity;
        for (int i = 0; i < length; ++i) {
            lp += c * (v.noise.bipolar() - lp);
            v.line[std::size_t(i)] = lp;
        }
        const int pos = std::max(1, length / 7);
        for (int i = length - 1; i >= pos; --i) v.line[std::size_t(i)] -= 0.8f * v.line[std::size_t(i - pos)];
        float mean = 0;
        for (int i = 0; i < length; ++i) mean += v.line[std::size_t(i)];
        mean /= float(length);
        for (int i = 0; i < length; ++i) { v.line[std::size_t(i)] = 0.6f * (v.line[std::size_t(i)] - mean); prev = v.line[std::size_t(i)]; }
        (void) prev;
        v.write = length;
        v.z1 = v.z2 = 0;
        if (pizz) v.burst = 0.25f + 0.6f * st.colour;   // the wood of the bow (col legno) / a finger on the string
        break;
    }
    case toyPiano:
        setModes({ 1.0f, 3.93f, 9.6f, 1.0f + 0.012f * (0.3f + st.colour) }, { 1.0f, 0.42f, 0.18f, 0.35f * (0.2f + st.colour) },
                 { 1.6f * decayScale, 0.45f * decayScale, 0.2f * decayScale, 1.2f * decayScale }, { 0.18f, 0.08f, 0.05f, 0.15f }, mallet * 0.6f);
        v.burst = 0.5f;   // the hammer's thunk
        break;
    case elevatorVibraphone:
        setModes({ 1.0f, 3.98f, 9.92f }, { 1.0f, 0.2f, 0.06f }, { 6.0f * decayScale, 1.5f * decayScale, 0.5f }, { 0.3f, 0.12f, 0.05f }, mallet);
        break;
    case mallChimes:
        setModes({ 1.0f, 2.0f, 2.99f, 4.16f, 5.43f }, { 1.0f, 0.5f, 0.32f, 0.16f, 0.08f },
                 { 3.6f * decayScale, 2.4f * decayScale, 1.7f * decayScale, 1.0f, 0.7f }, { 1.4f, 1.0f, 0.8f, 0.5f, 0.4f }, mallet * 0.5f);
        break;
    case warpedOrgan: {
        const float t = st.tone;
        setModes({ 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f }, { 0.45f, 1.0f, 0.65f, 0.35f * (0.3f + t), 0.3f * (0.2f + t), 0.15f * t },
                 { 1000, 1000, 1000, 1000, 1000, 1000 }, { 1000, 1000, 1000, 1000, 1000, 1000 }, 0.0f);
        v.burst = 0.25f + 0.35f * (1.0f - st.softness);   // the key click
        break;
    }
    case bowedGlass:
        setModes({ 1.0f, 1.0f + 0.0021f * (1.0f + st.width), 2.004f, 2.76f, 5.4f }, { 1.0f, 0.55f, 0.3f, 0.14f * (0.4f + st.tone), 0.05f * st.tone },
                 { 1000, 1000, 1000, 1000, 1000 }, { 1000, 1000, 1000, 1000, 1000 }, 0.0f);
        break;
    case loungeEPiano: {
        v.engine = Engine::tine;
        v.pc = v.pm = v.pt = 0;
        v.index = (0.06f + 0.30f * velocity) * expRange(st.tone, 1.5f);
        v.indexDecay = t60(0.9 * decayScale, sr);
        v.bell = (0.02f + 0.3f * st.colour) * (0.3f + 0.7f * velocity);
        v.bellDecay = t60(0.12, sr);
        break;
    }
    default: {   // analog / formant voices
        v.engine = Engine::analog;
        for (auto& p : v.ph) p = v.noise.uniform();
        v.det = { 0, 0, 0 };
        v.filter.clear();
        for (auto& f : v.formant) f.clear();
        v.vibPhase = v.noise.uniform();
        v.vibRate = 4.8f + 0.8f * v.noise.uniform();
        if (kindNow == fadedStrings) v.det = { -9.0f * (0.3f + st.colour), 7.0f * (0.3f + st.colour), 0.0f };
        if (kindNow == cassetteStrings) v.det = { -5.0f - 5.0f * st.width, 5.0f + 5.0f * st.width, -1200.0f };
        if (kindNow == basementBass) v.det = { 0.0f, -1200.0f, 0.0f };
        break;
    }
    }
    (void) note;
}

void Library::control(Voice& v, float tick) {
    const auto& st = settings;
    v.age += tick;
    const float wander = this->wander();
    // Pitch: Liminality's shape, the instrument's own movement (scaled by Liminality's wander), the
    // scoop at the start (a voice finding the note), a dying organ note's sag.
    float c = shapeCents(v.shape, tick);
    const float own = (wowCents + v.wobble.tick(rng, 0.3f, std::max(tick, 1.0e-4f)) * st.age * 3.0f) * wander;
    c += own;
    if (v.scoop != 0.0f) {
        const float x = std::clamp(v.age / (kindNow == hummedMemory ? 0.45f : 0.18f), 0.0f, 1.0f);
        c += v.scoop * (1.0f - x) * (1.0f - x);
    }
    if (v.sagRate != 0.0f) v.sag = std::max(-45.0f, v.sag + v.sagRate * tick / 4.0f);
    c += v.sag * wander;   // (a dying note's sag is the instrument's own movement: none at LIMINALITY 0)
    // Vibrato (voices and bows, after a moment; the arcade's lead later and faster).
    float vib = 0;
    if (kindNow == soloVoice || kindNow == fadedStrings || kindNow == arcadeLead || kindNow == hummedMemory) {
        v.vibPhase += tick * (kindNow == arcadeLead ? 6.2f : v.vibRate);
        v.vibPhase -= std::floor(v.vibPhase);
        const float delayIn = kindNow == arcadeLead ? 0.35f : 0.45f;
        const float grow = std::clamp((v.age - delayIn) / 0.6f, 0.0f, 1.0f);
        const float depth = kindNow == arcadeLead ? 22.0f : kindNow == hummedMemory ? 5.0f : 11.0f;
        vib = depth * grow * std::sin(twoPi * v.vibPhase) * (v.engine == Engine::recorded && kindNow != arcadeLead ? 0.35f : 1.0f);
    }
    const float ratio = bend * cents(c + vib);
    switch (v.engine) {
    case Engine::recorded:
        for (std::size_t k = 0; k < v.r.size(); ++k)
            if (v.r[k].on) v.r[k].retune(ratio * cents(v.rCents[k]));
        break;
    case Engine::modal:
        v.modes.retune(ratio, v.releasing());
        break;
    case Engine::tine:
        v.incC = std::min(0.45f, v.hz * ratio / float(sr));
        v.incM = v.incC * 1.0007f;
        v.incT = std::min(0.45f, v.incC * 14.0f);
        break;
    case Engine::pluck:
        v.delayTarget = std::clamp(float(sr) / (v.hz * ratio) - 1.0f, 2.0f, float(lineSize - 4));
        if (v.releasing()) {   // the hand damps the string
            const float period = float(sr) / v.hz;
            v.loss = std::min(v.loss, std::pow(10.0f, -3.0f * period / (0.12f * expRange(st.decay, 2.0f) * float(sr))));
        }
        break;
    case Engine::analog: {
        const float base = v.hz * ratio / float(sr);
        for (std::size_t k = 0; k < 3; ++k) v.inc[k] = std::min(0.45f, base * cents(v.det[k]));
        const float bright = expRange(st.tone, 3.0f) * (0.6f + 0.6f * v.velocity);
        float cutoff = 2000.0f, q = 0.6f;
        switch (kindNow) {
        case fadedStrings: cutoff = 2600.0f * bright; break;
        case soloVoice: cutoff = 3800.0f * bright; break;
        case hummedMemory: cutoff = 520.0f * bright; q = 0.8f; break;
        case cassetteStrings: cutoff = 1700.0f * bright; q = 0.75f; break;
        case arcadeLead: cutoff = std::min(3400.0f, 2600.0f * bright); q = 0.5f; break;   // (a console's soft edges: no sharper than a few samples)
        case basementBass: cutoff = (380.0f + 900.0f * std::exp(-v.age / 0.12f)) * bright; q = 0.9f; break;
        default: break;
        }
        v.filter.set(std::clamp(cutoff, 60.0f, 0.42f * float(sr)), q, sr);
        // Formants: the voice's vowel (ooh .. aah), the hum's nasal m, the strings' wooden body.
        struct F { float hz, q, gain; };
        F f[3] { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 } };
        if (kindNow == soloVoice) {
            const float m = st.colour;
            f[0] = { 300.0f + 420.0f * m, 6.0f, 1.0f };
            f[1] = { 870.0f + 280.0f * m, 8.0f, 0.35f + 0.2f * m };
            f[2] = { 2240.0f + 250.0f * m, 10.0f, 0.1f + 0.12f * m };
        } else if (kindNow == hummedMemory) {
            f[0] = { 250.0f, 4.0f, 1.0f };
            f[1] = { 2600.0f, 8.0f, 0.04f };
        } else if (kindNow == fadedStrings) {
            f[0] = { 290.0f, 2.0f, 0.7f };
            f[1] = { 1100.0f, 2.5f, 0.5f };
            f[2] = { 2700.0f, 3.0f, 0.3f };
        }
        for (int k = 0; k < 3; ++k) {
            if (f[k].gain > 0.0f) v.formant[k].set(f[k].hz, f[k].q, sr);
            v.formantGain[k] = f[k].gain;
        }
        break;
    }
    }
    // Recorded voices: the vowel / patch crossfade; the per-voice tone.
    v.mix = st.colour;
    float toneHz = v.tone;
    if (kindNow == fadedStrings) toneHz *= 1.0f - 0.55f * st.age;   // faded: the highs go first
    if (kindNow == basementBass) toneHz *= expRange(1.0f - st.colour, 2.0f);
    v.lpL.setHz(std::clamp(toneHz, 80.0f, 0.45f * float(sr)), sr);
    v.lpR.c = v.lpL.c;
}

void Library::updatePost() {
    const auto& st = settings;
    const double s = sr;
    for (int k = 0; k < 2; ++k) { postL[k].identity(); postR[k].identity(); }
    switch (kindNow) {
    case oldGuitar:   // from another room: the wall takes the highs
        postL[0].lowPass(9000.0 * std::pow(1500.0 / 9000.0, double(st.colour)), 0.6, s);
        postL[1].highPass(70.0 + 60.0 * st.colour, 0.7, s);
        break;
    case mallChimes:  // through the PA's horns
        postL[0].highPass(180.0 + 420.0 * st.colour, 0.7, s);
        postL[1].peak(1900.0, 0.9, 2.0 + 7.0 * st.colour, s);
        break;
    case basementBass:  // through the floor
        postL[0].lowPass(2400.0 * std::pow(260.0 / 2400.0, double(st.colour)), 0.75, s);
        postL[1].peak(68.0, 1.2, 1.0 + 4.0 * st.colour, s);
        break;
    case toyPiano:    // small: no low end
        postL[0].highPass(240.0 + 160.0 * st.age, 0.7, s);
        break;
    case cassetteStrings:  // a worn tape: highs gone, a little head bump
        postL[0].lowPass(11000.0 * std::pow(2200.0 / 11000.0, double(st.colour)), 0.7, s);
        postL[1].peak(110.0, 0.8, 1.5 * st.colour, s);
        break;
    case soloVoice:
    case hummedMemory:
        postL[0].highPass(90.0, 0.7, s);
        postL[1].lowPass(7500.0 * std::pow(0.5, double(st.age)), 0.6, s);
        break;
    case warpedOrgan:
        postL[0].lowPass(9000.0 * std::pow(0.4, double(st.age)), 0.6, s);
        break;
    default:
        break;
    }
    for (int k = 0; k < 2; ++k) {   // the right channel: the same coefficients, its own state
        postR[k].b0 = postL[k].b0; postR[k].b1 = postL[k].b1; postR[k].b2 = postL[k].b2;
        postR[k].a1 = postL[k].a1; postR[k].a2 = postL[k].a2;
    }
}

void Library::renderVoice(Voice& v, float* outL, float* outR, int m, const float* trem) {
    const auto& st = settings;
    const bool releasing = v.releasing();
    const float trim = dbGain(trimDb(kindNow, false));
    if (releasing && v.engine == Engine::recorded && !v.r[0].released)
        for (auto& r : v.r) r.release();
    const float w1 = std::sin(1.5707963f * std::clamp(v.mix, 0.0f, 1.0f)), w0 = std::cos(1.5707963f * std::clamp(v.mix, 0.0f, 1.0f));
    float peak = 0;
    for (int i = 0; i < m; ++i) {
        // The envelope.
        if (releasing) {
            v.amp *= v.releaseCoef;
        } else if (v.attacking) {
            v.amp += v.attackCoef * (1.3f - v.amp);
            if (v.amp >= 1.0f) { v.amp = 1.0f; v.attacking = false; }
        } else {
            v.amp *= v.heldCoef;
        }
        float l = 0, r = 0;
        switch (v.engine) {
        case Engine::recorded: {
            float y[3];
            for (std::size_t k = 0; k < 3; ++k) y[k] = v.r[k].on ? v.r[k].tick() : 0.0f;
            if (kindNow == soloVoice || kindNow == arcadeLead) { y[0] *= w0; y[1] *= w1; }
            for (std::size_t k = 0; k < 3; ++k) { l += y[k] * v.rL[k]; r += y[k] * v.rR[k]; }
            if (kindNow == cassetteStrings) { l = softClip(1.6f * l) / 1.6f; r = softClip(1.6f * r) / 1.6f; }
            break;
        }
        case Engine::modal: {
            const auto s = v.modes.tick();
            l = s.l;
            r = s.r;
            if (v.burst > 1.0e-4f) {   // hammer thunk / key click
                const float n = v.noise.bipolar() * v.burst * (kindNow == warpedOrgan ? 0.05f : 0.09f);
                v.burst *= kindNow == warpedOrgan ? 0.985f : 0.996f;
                l += n * v.panL;
                r += n * v.panR;
            }
            if (kindNow == bowedGlass) {   // the bow: a little rosin noise on the glass, more with Bow
                const float n = v.noise.bipolar() * 0.0012f * (0.2f + 1.6f * st.colour);
                l += n;
                r += n;
            }
            break;
        }
        case Engine::tine: {
            v.pc += v.incC; v.pc -= std::floor(v.pc);
            v.pm += v.incM; v.pm -= std::floor(v.pm);
            v.pt += v.incT; v.pt -= std::floor(v.pt);
            float p = v.pc + v.index * sinCycle(v.pm) + v.bell * sinCycle(v.pt);
            p -= std::floor(p);
            const float y = sinCycle(p);
            v.index = 0.03f + (v.index - 0.03f) * v.indexDecay;
            v.bell *= v.bellDecay;
            l = y * v.panL;
            r = y * v.panR;
            break;
        }
        case Engine::pluck: {
            v.delay += 0.02f * (v.delayTarget - v.delay);
            float pos = float(v.write) - v.delay;
            if (pos < 0.0f) pos += float(lineSize);
            const int a = int(pos);
            const float f = pos - float(a);
            const float x = v.line[std::size_t(a & (lineSize - 1))] * (1.0f - f) + v.line[std::size_t((a + 1) & (lineSize - 1))] * f;
            const float b = v.bright;
            const float y = b * x + (1.0f - 2.0f * b) * v.z1 + b * v.z2;
            v.z2 = v.z1;
            v.z1 = x;
            v.line[std::size_t(v.write)] = y * v.loss;
            v.write = (v.write + 1) & (lineSize - 1);
            float out = x;
            if (v.burst > 1.0e-4f) { out += v.noise.bipolar() * v.burst * 0.12f; v.burst *= 0.993f; }
            l = out * v.panL;
            r = out * v.panR;
            break;
        }
        case Engine::analog: {
            float x = 0;
            switch (kindNow) {
            case arcadeLead: x = 0.7f * pulseWave(v.ph[0], 0.25f - 0.12f * st.colour, v.inc[0]); break;
            case basementBass: x = 0.55f * blepSaw(v.ph[0], v.inc[0]) + 0.7f * sinCycle(v.ph[1]); break;
            case cassetteStrings: x = 0.4f * (blepSaw(v.ph[0], v.inc[0]) + blepSaw(v.ph[1], v.inc[1])) + 0.25f * blepSaw(v.ph[2], v.inc[2]); break;
            case fadedStrings: x = 0.33f * (blepSaw(v.ph[0], v.inc[0]) + blepSaw(v.ph[1], v.inc[1]) + blepSaw(v.ph[2], v.inc[2])); break;
            default: x = blepSaw(v.ph[0], v.inc[0]); break;   // the voices: a glottal-ish saw
            }
            for (std::size_t k = 0; k < 3; ++k) { v.ph[k] += v.inc[k]; v.ph[k] -= std::floor(v.ph[k]); }
            if (kindNow == soloVoice || kindNow == hummedMemory) x += v.noise.bipolar() * 0.05f;   // breath
            v.filter.tick(x);
            float y = v.filter.low;
            if (v.formantGain[0] > 0.0f) {
                float f = 0;
                for (int k = 0; k < 3; ++k)
                    if (v.formantGain[k] > 0.0f) { v.formant[k].tick(kindNow == fadedStrings ? y : x); f += v.formant[k].band * v.formant[k].k * v.formantGain[k]; }
                y = kindNow == fadedStrings ? 0.6f * y + 0.8f * f : (kindNow == hummedMemory ? 0.5f * y + f : f);
            }
            l = y * v.panL;
            r = y * v.panR;
            break;
        }
        }
        float g = v.amp * v.gain * trim * trem[i];
        if (kindNow == hummedMemory) {   // half-remembered: the hum breathes in and out
            g *= 1.0f - 0.35f * st.colour * (0.5f + 0.5f * std::sin(twoPi * (0.17f * v.age + 0.3f * float(v.note))));
        }
        if (v.stolen) {
            v.fade = std::max(0.0f, v.fade - v.fadeStep);
            g *= v.fade;
        }
        l = v.lpL.lp(l) * g;
        r = v.lpR.lp(r) * g;
        if (kindNow == toyPiano || kindNow == hallwayPizzicato || kindNow == oldGuitar) {   // a pluck's first instant rounded off (as the music box's pin)
            constexpr float ceiling = 0.3f;
            l = ceiling * softClip(l / ceiling);
            r = ceiling * softClip(r / ceiling);
        }
        outL[i] += l;
        outR[i] += r;
        peak = std::max(peak, std::abs(l) + std::abs(r));
    }
    // Voices end only at the end of a control period (the block size never matters).
    if (m == clock) {
        bool silent = false;
        switch (v.engine) {
        case Engine::recorded: silent = !v.r[0].on && !v.r[1].on && !v.r[2].on; break;
        case Engine::modal: silent = !v.attacking && v.modes.peak() * v.amp * v.gain < 2.0e-5f; break;
        default: silent = !v.attacking && v.age > 0.05f && peak < 1.0e-6f; break;
        }
        if (!v.stolen && ((releasing && v.amp < 1.0e-4f) || silent || v.amp < 1.0e-6f * float(!v.attacking))) v.steal(0.004, sr);
        if (v.stolen && v.fade <= 0.0f) v.note = -1;
    }
}

void Library::render(float* left, float* right, int n) {
    const auto& st = settings;
    int done = 0;
    while (done < n) {
        if (clock == 0) {
            const float tick = float(controlPeriod / sr);
            // The instrument's own wow (a tape, an organ's tired motor, a memory).
            float depth = 0, rate = 0.5f;
            if (kindNow == cassetteStrings) { depth = 2.0f + 11.0f * st.colour; rate = 0.7f; }
            if (kindNow == warpedOrgan) { depth = 2.0f + 20.0f * st.colour; rate = 0.45f; }
            if (kindNow == hummedMemory) { depth = 3.0f + 6.0f * st.colour; rate = 0.25f; }
            flutterPhase += tick * 7.5f;
            flutterPhase -= std::floor(flutterPhase);
            wowCents = wow.tick(rng, rate, tick) * depth + (kindNow == cassetteStrings ? 1.5f * st.colour * std::sin(twoPi * flutterPhase) : 0.0f);
            for (auto& v : pool.voices)
                if (v.active()) control(v, tick);
            updatePost();
            clock = controlPeriod;
        }
        const int m = std::min(n - done, clock);
        float bufL[controlPeriod] {}, bufR[controlPeriod] {}, trem[controlPeriod];
        // The vibraphone's motor: a slow tremolo on everything (Tremolo: speed and depth).
        const float tRate = kindNow == elevatorVibraphone ? 1.2f + 4.4f * st.colour : 0.0f;
        const float tDepth = kindNow == elevatorVibraphone ? 0.12f + 0.5f * st.colour : 0.0f;
        for (int i = 0; i < m; ++i) {
            if (tRate > 0.0f) {
                lfoPhase += tRate / float(sr);
                if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                trem[i] = 1.0f - tDepth * (0.5f + 0.5f * sinCycle(lfoPhase));
            } else {
                trem[i] = 1.0f;
            }
        }
        for (auto& v : pool.voices)
            if (v.active()) renderVoice(v, bufL, bufR, m, trem);
        const int size = int(echoL.size());
        for (int i = 0; i < m; ++i) {
            float l = bufL[i], r = bufR[i];
            for (int k = 0; k < 2; ++k) { l = postL[k].tick(l); r = postR[k].tick(r); }
            if (kindNow == mallChimes) { const float d = 1.0f + 1.5f * st.colour; l = softClip(d * l) / d; r = softClip(d * r) / d; }
            if (kindNow == arcadeLead && st.age > 0.02f) {   // the console's converter: fewer bits as it ages
                const float steps = std::exp2(13.0f - 7.0f * st.age);
                // The rounding error comes in with the level (no steps out of silence), then the converter's
                // own output filter (its steps never sharper than a few samples).
                const auto bits = [&](float x) { const float q = std::round(x * steps) / steps; const float k = std::min(1.0f, std::abs(x) * steps * 0.03f); return x + (q - x) * k * k; };
                const float smooth = std::min(1.0f, 2.0f * 9000.0f / float(sr));
                crushOut[0] += smooth * (bits(l) - crushOut[0]);
                crushOut[1] += smooth * (bits(r) - crushOut[1]);
                l = crushOut[0];
                r = crushOut[1];
            }
            // A hallway's echo (the pizzicato, sparse and echoing) or the room next door (the guitar).
            if (kindNow == hallwayPizzicato || kindNow == oldGuitar) {
                const bool hall = kindNow == hallwayPizzicato;
                const float dl = float(hall ? 0.23 : 0.013) * float(sr), dr = float(hall ? 0.29 : 0.019) * float(sr);
                const auto tap = [&](const std::vector<float>& line, float d) {
                    int p = echoWrite - int(d);
                    if (p < 0) p += size;
                    return line[std::size_t(p)];
                };
                const float el = tap(echoL, dl), er = tap(echoR, dr);
                echoLp[0] += 0.3f * (el - echoLp[0]);
                echoLp[1] += 0.3f * (er - echoLp[1]);
                const float fb = hall ? 0.32f : 0.0f;
                echoL[std::size_t(echoWrite)] = l + fb * echoLp[1];
                echoR[std::size_t(echoWrite)] = r + fb * echoLp[0];
                if (++echoWrite >= size) echoWrite = 0;
                const float mix = hall ? 0.22f : 0.18f + 0.3f * st.colour;
                l += mix * echoLp[1];
                r += mix * echoLp[0];
            }
            left[done + i] += l;
            right[done + i] += r;
        }
        clock -= m;
        done += m;
    }
}

} // namespace liminal::dsp
