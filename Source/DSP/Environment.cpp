#include "DSP/Environment.h"
#include <cmath>

namespace liminal::dsp {

namespace {
// The ten environments (2a: the first five, 2b: the rest; 2b also widened the first five
// so every pair is further apart than any instrument control). Why each number is what it
// is: docs/ENVIRONMENTS.md.
Profile makeHospital() {
    Profile p { "VACANT HOSPITAL", "EAST WING, LEVEL 2", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // a long, hard, sterile corridor: flutter between the walls
    r.rt60Low = 2.4f; r.rt60High = 1.5f; r.crossoverHz = 1800.0f; r.size = 1.0f; r.preDelayMs = 22.0f;
    r.diffusion = 0.45f; r.modulation = 0.15f; r.pattern = Space::Pattern::flutter; r.reflections = 12;
    r.firstMs = 6.0f; r.spacingMs = 11.0f; r.reflectionDecay = 0.8f; r.reflectionLevel = 0.45f; r.width = 1.6f;
    r.lowCutHz = 160.0f; r.highCutHz = 6000.0f;
    auto& c = p.colour;   // thin, filtered, a cold ringing resonance, unsteady, set back and wide
    c.highPassHz = 300.0f; c.lowPassHz = 2100.0f; c.lowPassQ = 0.8f; c.steep = true; c.peakHz = 1900.0f; c.peakQ = 5.0f; c.peakDb = 11.0f;
    c.peak2Hz = 520.0f; c.peak2Q = 1.4f; c.peak2Db = -3.0f;
    c.driftCents = 12.0f; c.wowHz = 0.17f; c.wowCents = 5.0f;
    c.width = 2.4f; c.dryDb = -1.5f;
    auto& e = p.echo;     // distant stereo reflections from the corridor's far end
    e.leftMs = 241.0f; e.rightMs = 367.0f; e.feedback = 0.42f; e.level = 0.5f; e.lowPassHz = 2600.0f; e.highPassHz = 450.0f;
    p.wetDb = 0.0f;
    return p;
}
Profile makeMall() {
    Profile p { "EMPTY SHOPPING MALL", "CENTRAL ATRIUM, AFTER HOURS", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // enormous and diffuse: late, soft, endless
    r.rt60Low = 7.5f; r.rt60High = 3.8f; r.crossoverHz = 1200.0f; r.size = 2.4f; r.preDelayMs = 70.0f;
    r.diffusion = 0.85f; r.modulation = 0.6f; r.pattern = Space::Pattern::sparse; r.reflections = 6;
    r.firstMs = 40.0f; r.spacingMs = 22.0f; r.reflectionDecay = 0.75f; r.reflectionLevel = 0.2f; r.width = 1.2f;
    r.lowCutHz = 50.0f; r.highCutHz = 5500.0f;
    auto& c = p.colour;   // washed out: soft highs, a wide chorus wash, far away
    c.highPassHz = 50.0f; c.lowPassHz = 4800.0f; c.shelfHz = 2000.0f; c.shelfDb = -12.0f;
    c.peak2Hz = 200.0f; c.peak2Q = 0.7f; c.peak2Db = 3.0f;
    c.chorusHz = 0.37f; c.chorusCents = 9.0f; c.chorusMix = 0.7f; c.driftCents = 3.0f;
    c.width = 1.6f; c.dryDb = -6.0f;
    p.wetDb = 4.5f;
    p.ambienceTrim = 0.6f;
    return p;
}
Profile makeSchool() {
    Profile p { "ABANDONED SCHOOL", "B CORRIDOR, NEAR ROOM 14", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // a narrow hallway: short, dark, tight flutter
    r.rt60Low = 1.5f; r.rt60High = 0.6f; r.crossoverHz = 900.0f; r.size = 0.55f; r.preDelayMs = 6.0f;
    r.diffusion = 0.35f; r.modulation = 0.1f; r.pattern = Space::Pattern::flutter; r.reflections = 14;
    r.firstMs = 3.0f; r.spacingMs = 6.5f; r.reflectionDecay = 0.82f; r.reflectionLevel = 0.55f; r.width = 0.05f;
    r.lowCutHz = 200.0f; r.highCutHz = 4000.0f;
    auto& c = p.colour;   // nostalgic: detuned, worn tape, boxy, narrow
    c.highPassHz = 420.0f; c.lowPassHz = 3000.0f; c.peakHz = 1050.0f; c.peakQ = 1.0f; c.peakDb = 8.0f;
    c.wowHz = 0.55f; c.wowCents = 9.0f; c.flutterHz = 6.5f; c.flutterCents = 4.5f;
    c.chorusHz = 0.23f; c.chorusCents = 12.0f; c.chorusMix = 0.45f;
    c.drive = 0.6f; c.width = 0.0f; c.dryDb = 0.0f;
    auto& e = p.echo;     // the opposite wall, close: a short dark slap
    e.leftMs = 41.0f; e.rightMs = 42.0f; e.feedback = 0.32f; e.level = 0.42f; e.lowPassHz = 2500.0f; e.highPassHz = 250.0f; e.pingPong = false;
    p.wetDb = -1.0f;
    return p;
}
Profile makeGarage() {
    Profile p { "UNDERGROUND PARKING GARAGE", "LEVEL P3, ROW F", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // concrete: strong, dense early reflections, heavy lows, dull highs
    r.rt60Low = 4.2f; r.rt60High = 1.3f; r.crossoverHz = 600.0f; r.size = 1.6f; r.preDelayMs = 10.0f;
    r.diffusion = 0.3f; r.modulation = 0.0f; r.pattern = Space::Pattern::dense; r.reflections = 16;
    r.firstMs = 4.0f; r.spacingMs = 5.0f; r.reflectionDecay = 0.88f; r.reflectionLevel = 0.85f; r.width = 0.4f;
    r.lowCutHz = 30.0f; r.highCutHz = 3000.0f;
    auto& c = p.colour;   // heavy low-mid boom, a concrete box resonance, dull top, close and narrow
    c.highPassHz = 32.0f; c.lowPassHz = 2400.0f; c.lowPassQ = 0.8f; c.steep = true;
    c.peakHz = 165.0f; c.peakQ = 0.8f; c.peakDb = 10.0f; c.peak2Hz = 330.0f; c.peak2Q = 1.5f; c.peak2Db = 4.0f;
    c.combHz = 118.0f; c.comb2Hz = 157.0f; c.combFeedback = 0.6f; c.combMix = 0.25f; c.combDamp = 0.6f;
    c.width = 0.55f; c.dryDb = -10.0f;
    auto& e = p.echo;     // the far wall of the bay
    e.leftMs = 101.0f; e.rightMs = 107.0f; e.feedback = 0.25f; e.level = 0.3f; e.lowPassHz = 1400.0f; e.highPassHz = 70.0f; e.pingPong = false;
    p.wetDb = -1.0f;
    p.ambienceTrim = 0.24f;
    return p;
}
Profile makePool() {
    Profile p { "ABANDONED INDOOR POOL", "MAIN HALL, DEEP END", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // tiles and water: long and bright
    r.rt60Low = 4.6f; r.rt60High = 4.4f; r.crossoverHz = 3000.0f; r.size = 1.9f; r.preDelayMs = 32.0f;
    r.diffusion = 0.7f; r.modulation = 0.35f; r.pattern = Space::Pattern::sparse; r.reflections = 9;
    r.firstMs = 18.0f; r.spacingMs = 12.0f; r.reflectionDecay = 0.85f; r.reflectionLevel = 0.6f; r.width = 1.4f;
    r.lowCutHz = 110.0f; r.highCutHz = 15000.0f;
    auto& c = p.colour;   // bright, glassy, spacious
    c.highPassHz = 220.0f; c.shelfHz = 3500.0f; c.shelfDb = 12.0f; c.peakHz = 3200.0f; c.peakQ = 1.0f; c.peakDb = 3.0f;
    c.width = 1.6f; c.dryDb = -4.0f;
    auto& e = p.echo;     // distinct echoes off the far tiled wall
    e.leftMs = 173.0f; e.rightMs = 291.0f; e.feedback = 0.5f; e.level = 0.55f; e.lowPassHz = 9000.0f; e.highPassHz = 250.0f;
    p.wetDb = 2.0f;
    return p;
}
Profile makeHotel() {
    Profile p { "EMPTY HOTEL", "FOURTH FLOOR, ROOM 412", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // carpet, curtains, soft furniture: short and very dark
    r.rt60Low = 1.1f; r.rt60High = 0.3f; r.crossoverHz = 500.0f; r.size = 0.7f; r.preDelayMs = 14.0f;
    r.diffusion = 0.8f; r.modulation = 0.2f; r.pattern = Space::Pattern::sparse; r.reflections = 5;
    r.firstMs = 9.0f; r.spacingMs = 9.0f; r.reflectionDecay = 0.6f; r.reflectionLevel = 0.25f; r.width = 0.8f;
    r.lowCutHz = 60.0f; r.highCutHz = 1800.0f;
    auto& c = p.colour;   // heard through a wall from the next room: muffled, warm, worn tape
    c.highPassHz = 70.0f; c.lowPassHz = 700.0f; c.lowPassQ = 0.6f; c.steep = true;
    c.peak2Hz = 240.0f; c.peak2Q = 0.9f; c.peak2Db = 4.0f;
    c.wowHz = 0.4f; c.wowCents = 6.0f; c.flutterHz = 7.5f; c.flutterCents = 2.5f;
    c.drive = 0.45f; c.width = 0.75f; c.dryDb = 1.0f;
    p.wetDb = -3.0f;
    p.ambienceTrim = 0.9f;
    return p;
}
Profile makeSubway() {
    Profile p { "DESERTED SUBWAY STATION", "PLATFORM 2, LAST TRAIN GONE", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // a tiled platform opening into tunnels: reflections run away down the tube
    r.rt60Low = 4.5f; r.rt60High = 2.2f; r.crossoverHz = 1000.0f; r.size = 2.0f; r.preDelayMs = 25.0f;
    r.diffusion = 0.5f; r.modulation = 0.25f; r.pattern = Space::Pattern::tunnel; r.reflections = 14;
    r.firstMs = 12.0f; r.spacingMs = 18.0f; r.reflectionDecay = 0.86f; r.reflectionLevel = 0.6f; r.width = 0.6f;
    r.lowCutHz = 35.0f; r.highCutHz = 6000.0f;
    auto& c = p.colour;   // a bass resonance, metallic ringing, pitch that sometimes sags
    c.highPassHz = 40.0f; c.lowPassHz = 7500.0f; c.peakHz = 72.0f; c.peakQ = 1.3f; c.peakDb = 11.0f;
    c.peak2Hz = 2600.0f; c.peak2Q = 1.2f; c.peak2Db = 6.0f;
    c.combHz = 412.0f; c.comb2Hz = 587.0f; c.combFeedback = 0.86f; c.combMix = 0.4f; c.combDamp = 0.12f;
    c.driftCents = 16.0f; c.wowHz = 0.09f; c.wowCents = 6.0f;
    c.width = 0.4f; c.dryDb = -4.0f;
    auto& e = p.echo;     // the long tunnel echo, metallic, wavering
    e.leftMs = 520.0f; e.rightMs = 610.0f; e.feedback = 0.52f; e.level = 0.75f; e.lowPassHz = 3200.0f; e.highPassHz = 380.0f; e.pingPong = false;
    e.modHz = 0.21f; e.modMs = 1.2f;
    p.wetDb = 1.0f;
    p.ambienceTrim = 0.8f;
    return p;
}
Profile makeSupermarket() {
    Profile p { "EMPTY SUPERMARKET", "AISLE 7, FROZEN FOODS", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // a low ceiling grid over hard floors: regular, artificial reflections
    r.rt60Low = 1.0f; r.rt60High = 0.8f; r.crossoverHz = 2500.0f; r.size = 0.9f; r.preDelayMs = 8.0f;
    r.diffusion = 0.25f; r.modulation = 0.0f; r.pattern = Space::Pattern::grid; r.reflections = 12;
    r.firstMs = 9.0f; r.spacingMs = 17.0f; r.reflectionDecay = 0.9f; r.reflectionLevel = 0.55f; r.width = 1.0f;
    r.lowCutHz = 300.0f; r.highCutHz = 9000.0f;
    auto& c = p.colour;   // thin and nostalgic, a mild chorus, humming with the electrics
    c.highPassHz = 480.0f; c.lowPassHz = 5200.0f; c.peakHz = 1700.0f; c.peakQ = 1.2f; c.peakDb = 4.0f;
    c.chorusHz = 0.62f; c.chorusCents = 6.0f; c.chorusMix = 0.4f; c.wowHz = 0.3f; c.wowCents = 3.0f;
    c.tremoloHz = 100.0f; c.tremoloDepth = 0.35f;
    c.width = 1.05f; c.dryDb = 2.5f;
    auto& e = p.echo;     // shelving and cold cases: a bright, short repeat
    e.leftMs = 63.0f; e.rightMs = 71.0f; e.feedback = 0.2f; e.level = 0.2f; e.lowPassHz = 6000.0f; e.highPassHz = 600.0f;
    p.wetDb = -1.0f;
    p.ambienceTrim = 1.0f;
    return p;
}
Profile makeOffice() {
    Profile p { "ENDLESS OFFICE", "OPEN PLAN, FLOOR 31 (OF 31?)", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // the same bay of ceiling tiles forever: repeating reflections, no edges
    r.rt60Low = 3.4f; r.rt60High = 3.0f; r.crossoverHz = 2000.0f; r.size = 1.5f; r.preDelayMs = 40.0f;
    r.diffusion = 1.0f; r.modulation = 0.9f; r.pattern = Space::Pattern::grid; r.reflections = 16;
    r.firstMs = 20.0f; r.spacingMs = 23.0f; r.reflectionDecay = 0.97f; r.reflectionLevel = 0.3f; r.width = 1.6f;
    r.lowCutHz = 220.0f; r.highCutHz = 4500.0f;
    auto& c = p.colour;   // soft, distant, synthetic: moving spectral notches and a slow wash
    c.highPassHz = 300.0f; c.lowPassHz = 3800.0f; c.peak2Hz = 750.0f; c.peak2Q = 0.6f; c.peak2Db = -10.0f;
    c.shelfHz = 3000.0f; c.shelfDb = 2.0f;
    c.combHz = 640.0f; c.combMix = 0.9f; c.notch = true; c.combSweepHz = 0.07f; c.combSweep = 0.5f;
    c.chorusHz = 0.21f; c.chorusCents = 10.0f; c.chorusMix = 0.55f;
    c.width = 1.8f; c.dryDb = 3.0f;
    auto& e = p.echo;     // a modulated, smeared delay
    e.leftMs = 330.0f; e.rightMs = 445.0f; e.feedback = 0.5f; e.level = 0.45f; e.lowPassHz = 2400.0f; e.highPassHz = 300.0f;
    e.modHz = 0.33f; e.modMs = 5.0f; e.diffuse = 0.6f;
    p.wetDb = 3.5f;
    p.ambienceTrim = 0.9f;
    return p;
}
Profile makeImpossible() {
    Profile p { "IMPOSSIBLE ROOM", "THE ROOM THAT WAS NOT THERE YESTERDAY", {}, {}, {}, 0, 1 };
    auto& r = p.room;   // reflections that grow instead of fade, a tail that rises in pitch and never ends
    r.rt60Low = 11.0f; r.rt60High = 8.0f; r.crossoverHz = 1500.0f; r.size = 2.3f; r.preDelayMs = 45.0f;
    r.diffusion = 0.9f; r.modulation = 1.0f; r.pattern = Space::Pattern::sparse; r.reflections = 12;
    r.firstMs = 30.0f; r.spacingMs = 26.0f; r.reflectionDecay = 1.14f; r.reflectionLevel = 0.12f; r.width = 1.6f;
    r.lowCutHz = 40.0f; r.highCutHz = 12000.0f;
    auto& c = p.colour;   // dissonant: every partial shifted off the harmonic series; the image will not stay still
    c.highPassHz = 30.0f; c.lowPassHz = 12000.0f; c.peakHz = 1480.0f; c.peakQ = 6.0f; c.peakDb = 6.0f;
    c.peak2Hz = 90.0f; c.peak2Q = 0.9f; c.peak2Db = 9.0f; c.shelfHz = 5000.0f; c.shelfDb = 4.0f;
    c.shiftHz = -17.0f; c.shiftMix = 0.45f;
    c.combHz = 233.0f; c.comb2Hz = 329.6f; c.combFeedback = 0.75f; c.combMix = 0.2f; c.combDamp = 0.3f;
    c.combSweepHz = 0.05f; c.combSweep = 0.15f;
    c.driftCents = 8.0f; c.wanderPan = 0.85f;
    c.width = 1.4f; c.dryDb = -6.5f;
    auto& e = p.echo;     // each echo comes back a tritone higher
    e.leftMs = 410.0f; e.rightMs = 590.0f; e.feedback = 0.55f; e.level = 0.45f; e.lowPassHz = 3500.0f; e.highPassHz = 200.0f;
    e.shiftSemis = 6.0f; e.modHz = 0.13f; e.modMs = 2.0f;
    p.wetDb = -1.0f;
    p.shimmerSemis = 12.0f; p.shimmerFeedback = 0.3f;
    p.ambienceTrim = 1.0f;
    return p;
}
const std::array<Profile, placeCount> profiles { makeHospital(), makeMall(), makeSchool(), makeGarage(), makePool(),
                                                makeHotel(), makeSubway(), makeSupermarket(), makeOffice(), makeImpossible() };

// Delay-line swing (samples) for a sinusoidal pitch wobble of `cents` at `hz`.
float swing(float cents, float hz, double sr) {
    return cents <= 0.0f ? 0.0f : float(cents * 0.693147 / (1200.0 * 6.283185 * std::max(0.05f, hz)) * sr);
}
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }
} // namespace

const Profile& profile(Place p) { return profiles[std::size_t(std::clamp(int(p), 0, placeCount - 1))]; }

Profile profileFor(Place place, int index) {
    Profile p = profile(place);
    const Character& ch = character(int(place), index);
    auto& r = p.room;
    r.rt60Low *= ch.decay;
    r.rt60High *= ch.decay;
    r.highCutHz = std::min(18000.0f, r.highCutHz * ch.roomBright);
    p.wetDb += ch.wetDb;
    auto& c = p.colour;
    c.lowPassHz = std::min(22000.0f, c.lowPassHz * ch.lowPass);
    c.driftCents = std::max(0.0f, c.driftCents + ch.driftCents);
    c.wowCents = std::max(0.0f, c.wowCents + ch.wowCents);
    if (ch.chorusMix > 0.0f) c.chorusCents = std::max(c.chorusCents, 8.0f);
    c.chorusMix = std::clamp(c.chorusMix + ch.chorusMix, 0.0f, 0.9f);
    if (ch.shiftMix > 0.0f && c.shiftHz == 0.0f) c.shiftHz = 13.0f;
    c.shiftMix = std::clamp(c.shiftMix + ch.shiftMix, 0.0f, 0.8f);
    c.wanderPan = std::clamp(c.wanderPan + ch.wanderPan, 0.0f, 1.0f);
    if (ch.tremolo > 0.0f && c.tremoloHz == 0.0f) c.tremoloHz = 100.0f;
    c.tremoloDepth = std::clamp(c.tremoloDepth + ch.tremolo, 0.0f, 0.8f);
    c.dryDb += ch.dryDb;
    p.ambienceTrim *= ch.bed;
    return p;
}

//==============================================================================
void PitchShift::prepare(double sampleRate) {
    window = float(0.07 * sampleRate);   // 70 ms grains
    line.assign(std::size_t(window) + 8, 0.0f);
    reset();
}

void PitchShift::reset() {
    std::fill(line.begin(), line.end(), 0.0f);
    write = 0;
    phase = 0;
}

void PitchShift::setSemitones(float semis) { step = (1.0f - std::pow(2.0f, semis / 12.0f)) / std::max(1.0f, window); }

float PitchShift::tick(float x) {
    const int size = int(line.size());
    if (size < 8) return 0.0f;
    line[std::size_t(write)] = x;
    phase += step;
    phase -= std::floor(phase);
    float out = 0;
    for (const float offset : { 0.0f, 0.5f }) {   // two heads half a grain apart, Hann-crossfaded
        float ph = phase + offset;
        if (ph >= 1.0f) ph -= 1.0f;
        float pos = float(write) - (2.0f + ph * window);
        if (pos < 0.0f) pos += float(size);
        const int i0 = int(pos);
        const float f = pos - float(i0);
        const float y = line[std::size_t(i0 % size)] + f * (line[std::size_t((i0 + 1) % size)] - line[std::size_t(i0 % size)]);
        const float s = sinCycle(0.5f * ph);
        out += y * s * s;
    }
    if (++write >= size) write = 0;
    return out;
}

//==============================================================================
void FrequencyShift::reset() {
    // Olli Niemitalo's 90-degree allpass pair (wideband, about 20 Hz .. 20 kHz at 48 kHz).
    constexpr float a[] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    constexpr float b[] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
    for (int k = 0; k < 4; ++k) {
        re[std::size_t(k)] = { a[k] * a[k] };
        im[std::size_t(k)] = { b[k] * b[k] };
    }
    imDelay = 0;
    ready = true;
}

float FrequencyShift::tick(float x, float cosine, float sine) {
    if (!ready) reset();
    const auto chain = [](std::array<Section, 4>& sections, float v) {
        for (auto& s : sections) {
            const float y = s.a * (v + s.y2) - s.x2;
            s.x2 = s.x1;
            s.x1 = v;
            s.y2 = s.y1;
            s.y1 = y;
            v = y;
        }
        return v;
    };
    const float i = chain(re, x);
    const float q = imDelay;
    imDelay = chain(im, x);
    return i * cosine - q * sine;
}

//==============================================================================
void Colour::prepare(double sampleRate) {
    sr = sampleRate;
    lineL.assign(std::size_t(0.06 * sr) + 4, 0.0f);
    lineR.assign(lineL.size(), 0.0f);
    combL.assign(std::size_t(0.06 * sr) + 4, 0.0f);
    combR.assign(combL.size(), 0.0f);
    amountCoef = float(1.0 - std::exp(-1.0 / (0.02 * sr)));
    reset();
}

void Colour::configure(const Profile::Colour& colour, unsigned seed) {
    c = colour;
    rng.state = 0xB5297A4Du ^ (seed * 0x68E31DA4u);
    drift.start(rng);
    pan.start(rng);
    const auto delayFor = [&](float hz) { return hz > 0.0f ? std::clamp(float(sr) / hz, 2.0f, float(combL.size()) * 0.6f) : 2.0f; };
    combD1 = delayFor(c.combHz);
    combD2 = delayFor(c.comb2Hz > 0.0f ? c.comb2Hz : c.combHz);
    combLpL.setHz(float(sr) * 0.5f * (1.0f - std::clamp(c.combDamp, 0.0f, 0.95f)) * 0.25f + 200.0f, sr);
    combLpR.c = combLpL.c;
    reset();
}

void Colour::reset() {
    for (auto* b : { &hpL, &hpR, &peakL, &peakR, &peak2L, &peak2R, &shelfL, &shelfR, &lpL, &lpR, &lp2L, &lp2R }) b->clear();
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    std::fill(combL.begin(), combL.end(), 0.0f);
    std::fill(combR.begin(), combR.end(), 0.0f);
    combLpL.z = combLpR.z = 0;
    shiftL.reset();
    shiftR.reset();
    write = clock = combWrite = 0;
    wowPhase = flutterPhase = tremoloPhase = sweepPhase = shiftPhase = 0;
    chorusPhase = 0.25f;
    driftNow = panNow = 0;
    filtersAt = -1;
    updateFilters();
}

void Colour::updateFilters() {
    const float a = amount;
    liveLowAt = live.lowOctaves;
    const double lowPassHz = double(c.lowPassHz) * std::pow(2.0, -double(live.lowOctaves));
    const double nyquistSafe = 0.45 * sr;
    hpL.highPass(10.0 * std::pow(double(c.highPassHz) / 10.0, double(a)), 0.707, sr);
    lpL.lowPass(std::min(nyquistSafe, 22000.0 * std::pow(std::min(1.0, lowPassHz / 22000.0), double(a))), 0.707 + (c.lowPassQ - 0.707) * a, sr);
    if (c.steep) lp2L.lowPass(std::min(nyquistSafe, 22000.0 * std::pow(std::min(1.0, lowPassHz / 22000.0), double(a))), 0.707, sr);
    else lp2L.identity();
    peakL.peak(c.peakHz, c.peakQ, c.peakDb * a, sr);
    peak2L.peak(c.peak2Hz, c.peak2Q, c.peak2Db * a, sr);
    shelfL.highShelf(c.shelfHz, c.shelfDb * a, sr);
    // Same coefficients on the right.
    const auto copy = [](Biquad& to, const Biquad& from) { to.b0 = from.b0; to.b1 = from.b1; to.b2 = from.b2; to.a1 = from.a1; to.a2 = from.a2; };
    copy(hpR, hpL);
    copy(lpR, lpL);
    copy(lp2R, lp2L);
    copy(peakR, peakL);
    copy(peak2R, peak2L);
    copy(shelfR, shelfL);
    filtersAt = a;
}

void Colour::process(float* left, float* right, int n) {
    const int size = int(lineL.size());
    const auto read = [](const std::vector<float>& line, int at, float delay) {
        const int length = int(line.size());
        float pos = float(at) - delay;
        while (pos < 0.0f) pos += float(length);
        const int i0 = int(pos);
        const float f = pos - float(i0);
        const float y0 = line[std::size_t(i0 % length)], y1 = line[std::size_t((i0 + 1) % length)];
        return y0 + f * (y1 - y0);
    };
    const float fs = float(sr);
    for (int i = 0; i < n; ++i) {
        if (clock == 0) {   // control rate: filters follow the amount, the drift and the image wander
            if (std::abs(amount - filtersAt) > 0.0005f || std::abs(live.lowOctaves - liveLowAt) > 0.01f) updateFilters();
            nearness = std::pow(10.0f, c.dryDb * amount / 20.0f);
            const float k = float(1.0 - std::exp(-32.0 / (0.15 * sr)));
            live.shiftMix += k * (liveTarget.shiftMix - live.shiftMix);
            live.wanderPan += k * (liveTarget.wanderPan - live.wanderPan);
            live.wowCents += k * (liveTarget.wowCents - live.wowCents);
            live.lowOctaves += k * (liveTarget.lowOctaves - live.lowOctaves);
            driftNow = drift.tick(rng, 0.3f, float(32.0 / sr));
            panNow = pan.tick(rng, 0.25f, float(32.0 / sr));
            clock = 32;
        }
        --clock;
        amount += amountCoef * (amountTarget - amount);
        if (std::abs(amountTarget - amount) < 1.0e-3f) amount = amountTarget;
        const float a = amount;
        float l = left[i], r = right[i];
        if (a <= 0.0f && filtersAt <= 0.0f) {   // untouched: Milestone 1's sound
            lineL[std::size_t(write)] = l;
            lineR[std::size_t(write)] = r;
            if (++write >= size) write = 0;
            bypassed = true;
            continue;
        }
        if (bypassed) {   // coming back: the filters forget what they held before
            for (auto* b : { &hpL, &hpR, &peakL, &peakR, &peak2L, &peak2R, &shelfL, &shelfR, &lpL, &lpR, &lp2L, &lp2R }) b->clear();
            bypassed = false;
        }
        l = lp2L.tick(lpL.tick(shelfL.tick(peak2L.tick(peakL.tick(hpL.tick(l))))));
        r = lp2R.tick(lpR.tick(shelfR.tick(peak2R.tick(peakR.tick(hpR.tick(r))))));
        if (c.combMix > 0.0f) {   // the place's own resonators (or moving notches)
            float d1 = combD1, d2 = combD2;
            if (c.combSweep > 0.0f) {
                sweepPhase += c.combSweepHz / fs;
                if (sweepPhase >= 1.0f) sweepPhase -= 1.0f;
                const float m = 1.0f + c.combSweep * 0.5f * sinCycle(sweepPhase);
                d1 *= m;
                d2 *= m;
            }
            const float m = c.combMix * a;
            if (c.notch) {   // feed-forward: x - delayed x, notches every combHz
                const float nl = l - 0.5f * (read(combL, combWrite, d1) + read(combL, combWrite, d2 * 1.5f));
                const float nr = r - 0.5f * (read(combR, combWrite, d1 * 1.01f) + read(combR, combWrite, d2 * 1.52f));
                combL[std::size_t(combWrite)] = l;
                combR[std::size_t(combWrite)] = r;
                l += m * (nl - l);
                r += m * (nr - r);
            } else {         // feedback: the signal rings at the comb's pitches
                const float fb = c.combFeedback;
                const float yl = combLpL.lp(0.5f * (read(combL, combWrite, d1) + read(combL, combWrite, d2)));
                const float yr = combLpR.lp(0.5f * (read(combR, combWrite, d1 * 1.004f) + read(combR, combWrite, d2 * 0.997f)));
                combL[std::size_t(combWrite)] = l + fb * yl;
                combR[std::size_t(combWrite)] = r + fb * yr;
                l += m * yl * 1.5f;
                r += m * yr * 1.5f;
            }
            if (++combWrite >= int(combL.size())) combWrite = 0;
        }
        // Pitch: wow, flutter and a random drift, as a delay that swings around a centre.
        wowPhase += c.wowHz / fs;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += c.flutterHz / fs;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        chorusPhase += c.chorusHz / fs;
        if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
        const float wow = swing((c.wowCents + live.wowCents) * a, c.wowHz, sr), flutter = swing(c.flutterCents * a, c.flutterHz, sr),
                    wander = swing(c.driftCents * a, 0.3f, sr);
        const float centre = wow + flutter + wander;
        const float d = centre + wow * sinCycle(wowPhase) + flutter * sinCycle(flutterPhase) + wander * driftNow;
        lineL[std::size_t(write)] = l;
        lineR[std::size_t(write)] = r;
        l = read(lineL, write, d);
        r = read(lineR, write, d);
        if (c.chorusMix > 0.0f) {   // a second, detuned player a few ms behind (opposite swing per side)
            const float depth = swing(c.chorusCents * a, c.chorusHz, sr);
            const float base = centre + depth + float(0.007 * sr) * a;
            float other = chorusPhase + 0.5f;
            if (other >= 1.0f) other -= 1.0f;
            const float cl = read(lineL, write, base + depth * sinCycle(chorusPhase));
            const float cr = read(lineR, write, base + depth * sinCycle(other));
            const float m = c.chorusMix * a;
            l = l * (1.0f - 0.45f * m) + cl * 0.65f * m;
            r = r * (1.0f - 0.45f * m) + cr * 0.65f * m;
        }
        if (++write >= size) write = 0;
        const float shiftMixNow = std::clamp(c.shiftMix + live.shiftMix, 0.0f, 0.9f);
        if (shiftMixNow > 0.0f) {   // a shifted copy: partials no longer in harmonic ratio
            shiftPhase += (c.shiftHz != 0.0f ? c.shiftHz : 13.0f) / fs;
            shiftPhase -= std::floor(shiftPhase);
            float q = shiftPhase + 0.25f;
            if (q >= 1.0f) q -= 1.0f;
            const float cs = sinCycle(q), sn = sinCycle(shiftPhase);
            const float m = shiftMixNow * a;
            l = l * (1.0f - 0.5f * m) + m * shiftL.tick(l, cs, sn);
            r = r * (1.0f - 0.5f * m) + m * shiftR.tick(r, cs, -sn);   // the other way on the right: the sides disagree
        }
        if (c.drive > 0.0f) {   // tape-like: loud passages round off, quiet ones pass
            const float g = 1.0f + 8.0f * c.drive * a;
            l = softClip(l * g) / g;
            r = softClip(r * g) / g;
        }
        if (c.tremoloDepth > 0.0f) {   // the mains getting into everything
            tremoloPhase += c.tremoloHz / fs;
            if (tremoloPhase >= 1.0f) tremoloPhase -= 1.0f;
            const float t = 1.0f - c.tremoloDepth * a * (0.5f + 0.5f * sinCycle(tremoloPhase));
            l *= t;
            r *= t;
        }
        const float w = 1.0f + (c.width - 1.0f) * a;
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
        l = mid + side;
        r = mid - side;
        const float wanderNow = std::clamp(c.wanderPan + live.wanderPan, 0.0f, 1.0f);
        if (wanderNow > 0.0f) {   // the image will not stay where it is
            const float p = std::clamp(wanderNow * a * panNow * 1.6f, -1.0f, 1.0f);
            const float gl = std::sqrt(0.5f * (1.0f - p)) * 1.41421f, gr = std::sqrt(0.5f * (1.0f + p)) * 1.41421f;
            l *= gl;
            r *= gr;
        }
        left[i] = l * nearness;
        right[i] = r * nearness;
    }
}

//==============================================================================
void Echo::prepare(double sampleRate) {
    sr = sampleRate;
    lineL.assign(std::size_t(1.2 * sr) + 4, 0.0f);
    lineR.assign(lineL.size(), 0.0f);
    apLineL.assign(std::size_t(0.03 * sr) + 4, 0.0f);
    apLineR.assign(apLineL.size(), 0.0f);
    shiftL.prepare(sr);
    shiftR.prepare(sr);
    reset();
}

void Echo::configure(const Profile::Echo& echo) {
    e = echo;
    const int size = int(lineL.size());
    delayL = std::clamp(int(e.leftMs * 0.001 * sr), 1, size - 1);
    delayR = std::clamp(int(e.rightMs * 0.001 * sr), 1, size - 1);
    lpL.setHz(e.lowPassHz, sr);
    lpR.setHz(e.lowPassHz, sr);
    hpL.setHz(e.highPassHz, sr);
    hpR.setHz(e.highPassHz, sr);
    modSamples = float(std::clamp(e.modMs, 0.0f, 20.0f) * 0.001 * sr);
    apLength = std::clamp(int(0.0173 * sr), 1, int(apLineL.size()) - 1);
    boostShift = 0;
    shiftL.setSemitones(e.shiftSemis);
    shiftR.setSemitones(e.shiftSemis);
    reset();
}

void Echo::reset() {
    std::fill(lineL.begin(), lineL.end(), 0.0f);
    std::fill(lineR.begin(), lineR.end(), 0.0f);
    std::fill(apLineL.begin(), apLineL.end(), 0.0f);
    std::fill(apLineR.begin(), apLineR.end(), 0.0f);
    shiftL.reset();
    shiftR.reset();
    write = apWrite = 0;
    modPhase = 0;
    lpL.z = lpR.z = hpL.z = hpR.z = 0;
}

void Echo::setBoost(float level, float feedback, float shiftSemis) {
    boostLevel = level;
    boostFeedback = feedback;
    if (shiftSemis != boostShift) {
        boostShift = shiftSemis;
        shiftL.setSemitones(e.shiftSemis + boostShift);
        shiftR.setSemitones(e.shiftSemis + boostShift);
    }
}

Stereo Echo::tick(Stereo in, float amount) {
    const int size = int(lineL.size());
    float yl, yr;
    if (modSamples > 0.0f) {   // the delay wavers: each repeat slightly off pitch, then back
        modPhase += e.modHz / float(sr);
        if (modPhase >= 1.0f) modPhase -= 1.0f;
        const float m = modSamples * (1.0f + sinCycle(modPhase));
        const auto at = [&](const std::vector<float>& line, float delay) {
            float pos = float(write) - delay;
            while (pos < 0.0f) pos += float(size);
            const int i0 = int(pos);
            const float f = pos - float(i0);
            return line[std::size_t(i0 % size)] + f * (line[std::size_t((i0 + 1) % size)] - line[std::size_t(i0 % size)]);
        };
        yl = at(lineL, float(delayL) + m);
        yr = at(lineR, float(delayR) + modSamples * 2.0f - m);
    } else {
        yl = lineL[std::size_t((write - delayL + size) % size)];
        yr = lineR[std::size_t((write - delayR + size) % size)];
    }
    // The loop's band-pass: each repeat darker and thinner, like a real far wall.
    float fl = hpL.hp(lpL.lp(yl)), fr = hpR.hp(lpR.lp(yr));
    if (e.diffuse > 0.0f) {   // each repeat smeared into a soft cloud
        const float g = std::clamp(e.diffuse, 0.0f, 0.7f);
        const float dl = apLineL[std::size_t(apWrite)], dr = apLineR[std::size_t(apWrite)];
        const float vl = fl + g * dl, vr = fr + g * dr;
        apLineL[std::size_t(apWrite)] = vl;
        apLineR[std::size_t(apWrite)] = vr;
        if (++apWrite >= apLength) apWrite = 0;
        fl = dl - g * vl;
        fr = dr - g * vr;
    }
    float backL = fl, backR = fr;
    if (e.shiftSemis + boostShift != 0.0f) {   // what comes back is not what was played
        backL = shiftL.tick(fl);
        backR = shiftR.tick(fr);
    }
    const float fb = std::min(0.85f, e.feedback + boostFeedback);
    const float level = (e.level + boostLevel) * amount;
    const float mono = 0.5f * (in.l + in.r);
    lineL[std::size_t(write)] = (e.pingPong ? mono : in.l) * level + fb * (e.pingPong ? backR : backL);
    lineR[std::size_t(write)] = (e.pingPong ? 0.0f : in.r * level) + fb * (e.pingPong ? backL : backR);
    if (++write >= size) write = 0;
    return { fl, fr };
}

//==============================================================================
void EnvironmentChain::prepare(double sampleRate) {
    sr = sampleRate;
    echo.prepare(sr);
    room.prepare(sr);
    ambience.prepare(sr);
    shimmer.prepare(sr);
    sbufL.assign(1024, 0.0f);
    sbufR.assign(1024, 0.0f);
    ebufL.assign(1024, 0.0f);
    ebufR.assign(1024, 0.0f);
    amountCoef = float(1.0 - std::exp(-1.0 / (0.02 * sr)));
    liveCoef = float(1.0 - std::exp(-1.0 / (0.03 * sr)));
    ambienceCoef = float(1.0 - std::exp(-1.0 / (0.6 * sr)));   // about 2 s to fade across
    killStep = float(1.0 / (0.03 * sr));
    quietLimit = int(sr);
    evo.prepare(sr);
    configure(current, characterNow, 1, evolutionSeedNow);
}

void EnvironmentChain::configure(Place place, int characterIndex, unsigned seed, unsigned evolutionSeed) {
    current = place;
    characterNow = std::clamp(characterIndex, 0, characterCount - 1);
    shaped = profileFor(place, characterNow);
    p = &shaped;
    evolutionSeedNow = evolutionSeed;
    resetEvolution();
    controlClock = 0;
    wetLive = wetLiveTarget = 1;
    ambienceLive = 1;
    echo.configure(p->echo);
    room.configure(p->room, seed + unsigned(place) * 7919u);
    room.reset();
    ambience.trim = p->ambienceTrim;
    ambience.configure(Ambience::Place(int(place)), seed, &character(int(place), characterNow));
    shimmer.setSemitones(p->shimmerSemis != 0.0f ? p->shimmerSemis : 12.0f);
    shimmer.reset();
    shimmerIn = 0;
    wetGain = dbGain(p->wetDb);
    quietFor = 0;
    killing = false;
    outFade = 1;
}

void EnvironmentChain::resetEvolution() {
    controlClock = 0;
    evo.start(evolutionSeedNow * 2654435761u + unsigned(current) * 97u + unsigned(characterNow) * 13u);
}

// The macros, Unreal and evolution, applied to the place's shared part once per block.
void EnvironmentChain::setLive(const Live& l) {
    liveSettings = l;
    const auto& r = macroResponse(int(current));
    const auto& f = unrealFlavour(int(current));
    const auto& ch = character(int(current), characterNow);
    const float u = std::clamp(l.macros.unreal, 0.0f, 1.0f) * ch.unreal;
    // Scale: the room grows or shrinks (decay and level); evolution moves it a little.
    const float scale = (l.macros.scale - 0.5f) * 2.0f * r.scale;
    room.setDecayScale(std::pow(2.0f, scale + 0.3f * evo.spatial()));
    wetLiveTarget = std::pow(10.0f, (4.0f * scale + 1.5f * evo.spatial()) / 20.0f);
    echo.setBoost(0.8f * u * f.echo, 0.3f * u * f.echo, u * f.echoShift);
    shimmerExtra = u * f.shimmer;
    // Activity: more or fewer events; the bed breathes more.
    ambience.setActivity(std::pow(2.0f, (l.macros.activity - 0.5f) * 3.0f * r.activity));
    ambienceLive = l.ambienceGain;
}

void EnvironmentChain::process(const float* sendL, const float* sendR, const float* echoInL, const float* echoInR, const float* ambienceGain,
                               const float* space, float* outL, float* outR, float* ambienceOut, int n) {
    n = std::min(n, int(sbufL.size()));
    for (int i = 0; i < n; ++i) {
        live += liveCoef * (liveTarget - live);
        sbufL[std::size_t(i)] = sendL[i] * live;
        sbufR[std::size_t(i)] = sendR[i] * live;
        ebufL[std::size_t(i)] = echoInL[i] * live;
        ebufR[std::size_t(i)] = echoInR[i] * live;
    }
    const auto& ch = character(int(current), characterNow);
    const float activity = std::pow(2.0f, (liveSettings.macros.activity - 0.5f) * 2.0f * macroResponse(int(current)).activity);
    for (int i = 0; i < n; ++i) {
        if (--controlClock <= 0) {   // the place evolves (64-sample control rate)
            controlClock = 64;
            evo.tick(float(64.0 / sr), liveSettings.evolutionRate, ch.evolutionSpeed, ch.evolutionDepth, activity, liveSettings.frozen);
            if (evo.takeDisturbance() && liveTarget > 0.0f) ambience.disturb();
        }
        wetLive += 0.0005f * (wetLiveTarget - wetLive);
        amount += amountCoef * (amountTarget - amount);
        if (std::abs(amountTarget - amount) < 1.0e-3f) amount = amountTarget;
        ambienceFade += ambienceCoef * (liveTarget - ambienceFade);
        if (killing) outFade = std::max(0.0f, outFade - killStep);
        const float ag = ambienceGain[i] * ambienceFade * ambienceLive * (1.0f + 0.2f * evo.spectral());
        const auto amb = ambience.tick();
        const Stereo send { sbufL[std::size_t(i)], sbufR[std::size_t(i)] };
        const Stereo echoed = echo.tick({ ebufL[std::size_t(i)], ebufR[std::size_t(i)] }, 1.0f);
        const Stereo wet = room.tick({ send.l + 0.5f * echoed.l + amb.send.l * ag + shimmerIn, send.r + 0.5f * echoed.r + amb.send.r * ag + shimmerIn });
        if (p->shimmerFeedback + shimmerExtra > 0.0f)   // the tail, shifted up, fed back in: it climbs as it fades
            shimmerIn = std::min(0.6f, p->shimmerFeedback + shimmerExtra) * amount * shimmer.tick(0.5f * (wet.l + wet.r));
        const float sp = space[i];
        const float keep = 1.0f - 0.45f * sp;
        const float ambL = amb.dry.l * ag, ambR = amb.dry.r * ag;
        const float l = (keep * ambL + echoed.l + sp * wetGain * wetLive * wet.l) * outFade;
        const float r = (keep * ambR + echoed.r + sp * wetGain * wetLive * wet.r) * outFade;
        outL[i] += l;
        outR[i] += r;
        ambienceOut[i] += 0.5f * (ambL * ambL + ambR * ambR);
        quietFor = std::abs(l) + std::abs(r) < 3.0e-5f ? quietFor + 1 : 0;
    }
}

//==============================================================================
void Environments::prepare(double sampleRate, Place initial, int character, unsigned seed) {
    evolutionSeed = seed;
    for (auto& c : chains) c.prepare(sampleRate);
    changes = 0;
    active = 0;
    pending = -1;
    chains[0].configure(initial, character, 1, evolutionSeed);
    chains[0].setAmount(amount);
    chains[0].snapAmount();
    chains[0].setLive(true);
    chains[0].snapLive();
    chains[0].setLive(liveNow);
    chains[1].setLive(false);
    chains[1].snapLive();
    running = { true, false };
}

void Environments::setAmount(float a) {
    amount = std::clamp(a, 0.0f, 1.0f);
    for (auto& c : chains) c.setAmount(amount);
}

void Environments::setLive(const EnvironmentChain::Live& l) {
    liveNow = l;
    for (int c = 0; c < 2; ++c)
        if (running[std::size_t(c)]) chains[std::size_t(c)].setLive(l);
}

void Environments::select(Place place, int character) {
    if (pending >= 0) { pending = int(place); pendingCharacter = character; return; }   // starts once the other chain is clear
    const auto& now = chains[std::size_t(active)];
    if (place == now.place() && character == now.characterIndex()) return;
    const int other = 1 - active;
    if (running[std::size_t(other)] && !chains[std::size_t(other)].silent()) {
        chains[std::size_t(other)].kill();   // still ringing from an earlier change: fade it quickly first
        pending = int(place);
        pendingCharacter = character;
        return;
    }
    start(place, character);
}

void Environments::start(Place place, int character) {
    const int other = 1 - active;
    auto& next = chains[std::size_t(other)];
    next.configure(place, character, ++changes + 1, evolutionSeed);
    next.setAmount(amount);
    next.snapAmount();
    next.setLive(false);
    next.snapLive();
    next.setLive(true);   // fades in
    next.setLive(liveNow);
    chains[std::size_t(active)].setLive(false);   // stops hearing the instruments; its tail rings out
    running[std::size_t(other)] = true;
    active = other;
}

void Environments::process(const float* sendL, const float* sendR, const float* echoInL, const float* echoInR, const float* ambienceGain,
                           const float* space, float* outL, float* outR, float* ambienceOut, int n) {
    for (int c = 0; c < 2; ++c)
        if (running[std::size_t(c)]) chains[std::size_t(c)].process(sendL, sendR, echoInL, echoInR, ambienceGain, space, outL, outR, ambienceOut, n);
}

// Retiring a faded chain and starting a waiting place happen only here, on the processor's
// fixed control grid, so they land on the same sample whatever the host's block size.
void Environments::maintain() {
    const int other = 1 - active;
    auto& old = chains[std::size_t(other)];
    if (running[std::size_t(other)] && (old.silent() || old.killed())) running[std::size_t(other)] = false;
    if (pending >= 0 && !running[std::size_t(other)]) {
        const auto place = Place(pending);
        pending = -1;
        if (place != chains[std::size_t(active)].place() || pendingCharacter != chains[std::size_t(active)].characterIndex()) start(place, pendingCharacter);
    }
}

} // namespace liminal::dsp
