#include "Presets.h"
#include "DSP/Characters.h"
#include "DSP/Environment.h"
#include "Parameters.h"

namespace liminal::presets {

namespace {
using Values = std::vector<std::pair<juce::String, float>>;
using namespace params;

const char* instrumentOn[] { pianoOn, synthOn, glassOn, choirOn, boxOn, droneOn };
const char* instrumentLevel[] { pianoLevel, synthLevel, glassLevel, choirLevel, boxLevel, droneLevel };

// Only these instruments on (each at a level in dB).
Values playing(std::initializer_list<std::pair<int, float>> on) {
    Values v;
    for (int i = 0; i < instrumentCount; ++i) {
        float level = -100.0f;
        for (const auto& [k, db] : on)
            if (k == i) level = db;
        v.push_back({ instrumentOn[i], level > -100.0f ? 1.0f : 0.0f });
        if (level > -100.0f) v.push_back({ instrumentLevel[i], level });
    }
    return v;
}
Values operator+(Values a, const Values& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
std::pair<juce::String, float> fx(int instrument, const char* name, float value) { return { fxId(instrument, name), value }; }

std::vector<Preset> build() {
    std::vector<Preset> list;
    // Environments: each place with its first Character.
    for (int p = 0; p < dsp::placeCount; ++p) {
        const auto& prof = dsp::profile(dsp::Place(p));
        list.push_back({ prof.name, Category::environment, -1, prof.location, { { environment, float(p) }, { character, 0.0f } } });
    }
    // Characters: every place's three.
    for (int p = 0; p < dsp::placeCount; ++p)
        for (int c = 0; c < dsp::characterCount; ++c) {
            const auto& ch = dsp::character(p, c);
            Values v { { environment, float(p) }, { character, float(c) } };
            // Each Character's suggested way to play it.
            const float unrealAmount = c == 2 ? 0.25f : 0.0f;
            v.push_back({ unreal, unrealAmount });
            v.push_back({ evolution, c == 0 ? 0.3f : c == 1 ? 0.45f : 0.65f });
            list.push_back({ juce::String(dsp::profile(dsp::Place(p)).name) + "  -  " + ch.name, Category::character, -1, ch.mood, v });
        }
    // Instruments: the six on their own, then twenty variations.
    const char* names[] { "FORGOTTEN PIANO", "FADED SYNTH", "GLASS MEMORIES", "EMPTY CHOIR", "LOST MUSIC BOX", "HOLLOW DRONE" };
    for (int i = 0; i < instrumentCount; ++i) list.push_back({ names[i], Category::instrument, i, "The instrument as it comes", playing({ { i, 0.0f } }) });
    const auto variation = [&](const char* name, int i, const char* description, Values v) {
        const Values all = playing({ { i, 0.0f } }) + v;
        list.push_back({ name, Category::instrument, i, description, all });
    };
    variation("FELT UPRIGHT", 0, "Soft hammers, close, mechanical", { { pianoSoftness, 0.8f }, { pianoNoise, 0.45f }, { pianoAge, 0.2f }, { pianoDecay, 0.5f } });
    variation("DETUNED PARLOUR", 0, "A neglected piano: keys that disagree", { { pianoAge, 0.85f }, { pianoDetune, 0.7f }, fx(0, fx::drift, 0.15f) });
    variation("DISTANT REHEARSAL ROOM", 0, "Heard from down the hall", { { pianoDistance, 0.8f }, fx(0, fx::reverb, 0.9f) });
    variation("WORN TAPE PIANO", 0, "A cassette of someone practising", { { pianoAge, 0.5f }, fx(0, fx::tape, 0.65f), fx(0, fx::tapeHiss, 0.6f), fx(0, fx::tapeDropouts, 0.3f) });
    variation("WARM PAD", 1, "Slow, soft, wide", { { synthAttack, 0.8f }, { synthRelease, 2.5f }, { synthWarmth, 0.7f }, { synthWidth, 0.85f } });
    variation("VHS LEAD", 1, "A melody off an old tape", { { synthDetune, 0.3f }, { synthDrift, 0.55f }, fx(1, fx::tape, 0.5f), fx(1, fx::chorus, 0.4f) });
    variation("DOTTED ECHO PLUCK", 1, "Short notes with a synced echo", { { synthAttack, 0.003f }, { synthRelease, 0.25f }, { synthWarmth, 0.25f },
                                                                          fx(1, fx::echo, 0.5f), fx(1, fx::echoSync, 3.0f), fx(1, fx::echoFeedback, 0.45f) });
    variation("WINE GLASS RIM", 2, "A finger round the rim", { { glassResonance, 0.9f }, { glassDecay, 0.8f }, { glassBrightness, 0.4f } });
    variation("CRACKED CRYSTAL", 2, "Brittle, beating, fragile", { { glassFragility, 0.9f }, { glassBrightness, 0.75f } });
    variation("FAR AWAY CHIMES", 2, "Glass in another room", { { glassDistance, 0.7f }, fx(2, fx::echo, 0.4f), fx(2, fx::reverb, 0.8f) });
    variation("OOH CHOIR", 3, "Closed vowels, warm", { { choirFormant, 0.1f }, { choirDarkness, 0.4f } });
    variation("BREATHY GHOSTS", 3, "More breath than voice", { { choirBreath, 0.9f }, { choirDarkness, 0.35f }, { choirWidth, 0.8f } });
    variation("BRIGHT EE", 3, "Open, wide, bright", { { choirFormant, 0.9f }, { choirWidth, 0.9f }, { choirDarkness, 0.0f } });
    variation("SMEARED CHOIR", 3, "Voices dissolving into grains", { fx(3, fx::grain, 0.6f), fx(3, fx::grainSize, 0.9f), fx(3, fx::grainSpread, 0.4f) });
    variation("NEW MUSIC BOX", 4, "Just wound, in tune", { { boxWear, 0.0f }, { boxDrift, 0.0f }, { boxBrightness, 0.7f } });
    variation("BROKEN MUSIC BOX", 4, "Missing teeth, a tired spring", { { boxWear, 0.9f }, { boxDrift, 0.8f }, { boxMechanism, 0.8f } });
    variation("BOX IN THE ATTIC", 4, "Muffled by dust and distance", { { boxDistance, 0.6f }, fx(4, fx::tape, 0.6f), fx(4, fx::tapeTone, 0.2f) });
    variation("SUB HUM", 5, "Low and nearly pure", { { droneHarmonics, 0.1f }, { droneSub, 1.0f }, { droneMovement, 0.2f } });
    variation("BRIGHT SWARM", 5, "Many voices, moving", { { droneDensity, 0.9f }, { droneHarmonics, 0.8f }, { droneMovement, 0.8f } });
    variation("SLOW TIDE", 5, "Very long rise and fall, smeared", { { droneAttack, 6.0f }, { droneRelease, 10.0f }, { droneMovement, 0.3f }, fx(5, fx::grain, 0.4f) });
    // Scenes: everything together.
    const auto scene = [&](const char* name, const char* description, int place, int ch, Values v) {
        const Values all = Values { { environment, float(place) }, { character, float(ch) } } + v;
        list.push_back({ name, Category::scene, -1, description, all });
    };
    using P = dsp::Place;
    scene("FORGOTTEN CHILDHOOD", "A music box and a soft piano in a classroom you half remember", int(P::school), 1,
          playing({ { 4, -3.0f }, { 0, -6.0f } }) + Values { { pianoSoftness, 0.7f }, { boxWear, 0.5f }, { presence, 0.6f }, fx(4, fx::tape, 0.3f) });
    scene("THE LAST PATIENT", "Night shift: a piano, a low drone, the monitors", int(P::hospital), 1,
          playing({ { 0, 0.0f }, { 5, -12.0f } }) + Values { { droneSub, 0.7f }, { droneHarmonics, 0.2f }, { unreal, 0.15f } });
    scene("CLOSING TIME", "Shutters down, a warm synth and a piano in the atrium", int(P::mall), 0,
          playing({ { 1, -6.0f }, { 0, -3.0f } }) + Values { { synthAttack, 0.6f }, { synthRelease, 2.0f }, { synthWarmth, 0.6f }, { space, 0.5f } });
    scene("DREAM OF AN EMPTY POOL", "Glass and an ooh choir over still water", int(P::pool), 0,
          playing({ { 2, 0.0f }, { 3, -8.0f } }) + Values { { choirFormant, 0.15f }, { scale, 0.7f }, { glassResonance, 0.6f } });
    scene("CORRIDOR WITHOUT END", "The hospital corridor goes on and on", int(P::hospital), 0,
          playing({ { 0, 0.0f }, { 2, -6.0f } }) + Values { { scale, 0.9f }, { space, 0.6f }, fx(0, fx::echo, 0.3f), fx(0, fx::echoTime, 620.0f), { presence, 0.35f } });
    scene("MALL AFTER MIDNIGHT", "The atrium has no end; a drone, a far synth", int(P::mall), 2,
          playing({ { 5, -6.0f }, { 1, -9.0f } }) + Values { { unreal, 0.3f }, { synthAttack, 1.5f }, { synthRelease, 4.0f }, { droneMovement, 0.6f } });
    scene("MEMORIES OF SUMMER", "A sunlit pool, a VHS melody, a music box", int(P::pool), 1,
          playing({ { 1, -4.0f }, { 4, -6.0f } }) + Values { { synthDrift, 0.5f }, fx(1, fx::tape, 0.4f), fx(1, fx::chorus, 0.3f) });
    scene("SOMETHING BELOW THE STATION", "A sub drone and dark voices; something is coming", int(P::subway), 2,
          playing({ { 5, -3.0f }, { 3, -10.0f } }) + Values { { droneSub, 0.9f }, { choirDarkness, 0.8f }, { activity, 0.8f } });
    scene("ROOMS THAT SHOULDN'T EXIST", "Choir and glass where the walls should be", int(P::impossible), 2,
          playing({ { 3, -6.0f }, { 2, -3.0f } }) + Values { { unreal, 0.6f }, { evolution, 0.7f }, { choirFormant, 0.6f } });
    scene("HOME, BUT WRONG", "A detuned piano and a broken music box behind a hotel door", int(P::hotel), 2,
          playing({ { 0, 0.0f }, { 4, -8.0f } }) + Values { { pianoAge, 0.8f }, { pianoDetune, 0.6f }, { boxWear, 0.8f }, { boxDrift, 0.7f } });
    scene("NIGHT SHIFT", "A warm pad and glass under the fluorescent hum", int(P::hospital), 1,
          playing({ { 1, -6.0f }, { 2, -4.0f } }) + Values { { synthAttack, 0.8f }, { synthRelease, 2.5f }, { synthWarmth, 0.75f } });
    scene("LEVEL P3", "A distant piano and the concrete's own hum", int(P::garage), 1,
          playing({ { 0, 0.0f }, { 5, -14.0f } }) + Values { { pianoDistance, 0.6f }, { droneHarmonics, 0.15f } });
    scene("ROOM 412", "A felt piano through the wall at 3 a.m.", int(P::hotel), 0,
          playing({ { 0, 0.0f } }) + Values { { pianoSoftness, 0.8f }, { pianoNoise, 0.4f } });
    scene("FLUORESCENT AISLES", "Synth and drone, the tubes screaming", int(P::supermarket), 2,
          playing({ { 1, -4.0f }, { 5, -8.0f } }) + Values { { unreal, 0.4f }, { synthDetune, 0.4f } });
    scene("SATURDAY 1996", "A music box and a synth through worn tape", int(P::supermarket), 1,
          playing({ { 4, -4.0f }, { 1, -6.0f } }) + Values { fx(4, fx::tape, 0.4f), fx(1, fx::tape, 0.4f) });
    scene("UNDERWATER LULLABY", "A music box and a choir under the surface", int(P::pool), 2,
          playing({ { 4, 0.0f }, { 3, -8.0f } }) + Values { { choirFormant, 0.1f } });
    scene("AFTER THE BELL", "An upright in an empty corridor", int(P::school), 0,
          playing({ { 0, 0.0f } }) + Values { { pianoAge, 0.45f } });
    scene("THE LIFT IS COMING", "Piano and glass in the lobby", int(P::hotel), 1,
          playing({ { 0, -2.0f }, { 2, -6.0f } }) + Values { { scale, 0.7f } });
    scene("OVERTIME FOREVER", "A choir and a pad dissolving in the open plan", int(P::office), 1,
          playing({ { 3, -6.0f }, { 1, -8.0f } }) + Values { { synthAttack, 1.0f }, { synthRelease, 3.0f }, fx(3, fx::grain, 0.3f) });
    scene("FAMILIAR WALLS", "Almost a normal room: piano and glass", int(P::impossible), 0,
          playing({ { 0, 0.0f }, { 2, -6.0f } }) + Values { { evolution, 0.7f } });
    return list;
}
} // namespace

const std::vector<Preset>& factory() {
    static const std::vector<Preset> list = build();
    return list;
}

std::vector<int> inCategory(Category c) {
    std::vector<int> out;
    for (std::size_t i = 0; i < factory().size(); ++i)
        if (factory()[i].category == c) out.push_back(int(i));
    return out;
}

} // namespace liminal::presets
