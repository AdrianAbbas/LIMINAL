#include "DSP/Characters.h"
#include <cmath>

namespace liminal::dsp {

namespace {
// Ambience::Kind indices used for a Character's extra event.
enum Extra { monitor = 0, ballastTick, doorClick, paChime, shutter, radiatorPing, locker, bell, carDoor, barrier,
             drip, ripple, liftChime, doorClose, trainRumble, railTick, freezerSeal, cart, flicker, ghostNote, wrongWall };

// Field order (designated initialisers must follow it): name, mood, recommended,
// eventRate, eventWeight, extraKind, extraMean, bed, bedTone, hum, decay, roomBright, wetDb,
// lowPass, driftCents, wowCents, chorusMix, shiftMix, wanderPan, tremolo, dryDb,
// fxDrift, fxTape, fxChorus, fxGrain, fxEcho, fxReverb, evolutionSpeed, evolutionDepth,
// unreal, grade, light, flicker.
const std::array<std::array<Character, characterCount>, 10> table { {
    {{   // Hospital: monitor, ballast tick, a far door, the tube flickering
        { .name = "STERILE SILENCE", .mood = "Clean, bright, almost nothing moves", .recommended = "PIANO + GLASS",
          .eventRate = 0.8f, .bedTone = 1.15f, .hum = 1.2f, .decay = 0.95f, .roomBright = 1.2f, .lowPass = 1.15f,
          .evolutionSpeed = 0.6f, .evolutionDepth = 0.5f, .unreal = 0.8f, .grade = 0x1ab0f0f0, .light = 1.2f },
        { .name = "AFTER HOURS", .mood = "The night shift: dim, monitors somewhere", .recommended = "PIANO + DRONE",
          .eventRate = 1.0f, .eventWeight = { 1.8f, 1, 0.7f, 1, 1 }, .bed = 1.25f, .bedTone = 0.7f, .hum = 0.6f, .decay = 1.15f, .roomBright = 0.7f, .wetDb = 1.0f,
          .lowPass = 0.75f, .wowCents = 3, .fxTape = 0.15f, .fxReverb = 0.2f, .grade = 0x28203048, .light = 0.7f, .flicker = 0.2f },
        { .name = "SOMETHING WRONG", .mood = "Too many alarms, the lights stutter, pitch slips", .recommended = "GLASS + DRONE",
          .eventRate = 2.0f, .eventWeight = { 2.5f, 1, 1.5f, 1, 1 }, .extraKind = flicker, .extraMean = 9, .hum = 1.8f, .decay = 1.3f,
          .driftCents = 10, .shiftMix = 0.2f, .wanderPan = 0.3f, .fxDrift = 0.2f, .fxGrain = 0.15f,
          .evolutionSpeed = 1.6f, .evolutionDepth = 1.6f, .unreal = 1.5f, .grade = 0x2c50ff60, .light = 0.8f, .flicker = 0.7f },
    }},
    {{   // Mall: the PA chime, a shutter far off
        { .name = "CLOSING TIME", .mood = "The last shoppers far off, shutters coming down", .recommended = "SYNTH + PIANO",
          .eventRate = 1.2f, .eventWeight = { 1, 1.5f, 1.5f, 1, 1 }, .extraKind = doorClose, .extraMean = 14, .hum = 1.2f,
          .grade = 0x26ffc27a, .light = 1.0f },
        { .name = "FORGOTTEN", .mood = "Years empty: dust, faded colour, nothing running", .recommended = "MUSIC BOX + PIANO",
          .eventRate = 0.45f, .bed = 0.95f, .bedTone = 0.6f, .hum = 0.4f, .decay = 1.3f, .roomBright = 0.6f, .lowPass = 0.6f,
          .wowCents = 5, .fxTape = 0.3f, .grade = 0x32c8a070, .light = 0.6f },
        { .name = "ENDLESS", .mood = "The atrium goes on forever; everything far away", .recommended = "DRONE + CHOIR",
          .eventRate = 0.8f, .bed = 1.45f, .bedTone = 0.7f, .hum = 0.6f, .decay = 1.9f, .wetDb = 3.0f, .dryDb = -3.0f, .fxGrain = 0.2f, .fxReverb = 0.4f,
          .evolutionSpeed = 0.5f, .evolutionDepth = 1.5f, .unreal = 1.3f, .grade = 0x22e0e0ff, .light = 0.85f },
    }},
    {{   // School: radiator, lockers, a far bell, the tubes
        { .name = "AFTER CLASS", .mood = "Everyone just left; the building settles", .recommended = "PIANO + MUSIC BOX",
          .eventWeight = { 1.5f, 1.3f, 1, 1, 1 }, .bedTone = 0.85f, .grade = 0x2ae8c060, .light = 1.0f },
        { .name = "CHILDHOOD MEMORY", .mood = "Warm, hazy, half-remembered voices", .recommended = "MUSIC BOX + GLASS",
          .eventRate = 0.6f, .eventWeight = { 0.4f, 0.4f, 1, 0.5f, 1 }, .extraKind = bell, .extraMean = 10, .bed = 1.3f, .bedTone = 1.2f,
          .decay = 1.25f, .lowPass = 0.7f, .wowCents = 6, .chorusMix = 0.2f, .fxTape = 0.3f, .fxChorus = 0.1f,
          .grade = 0x32ffd890, .light = 1.25f },
        { .name = "NIGHTMARE", .mood = "The corridor stretches; nothing stays in tune", .recommended = "CHOIR + DRONE",
          .eventRate = 1.8f, .eventWeight = { 2, 1.5f, 1, 2, 1 }, .extraKind = ghostNote, .extraMean = 8, .bed = 0.75f, .hum = 1.6f, .decay = 1.5f,
          .driftCents = 14, .shiftMix = 0.1f, .wanderPan = 0.4f, .dryDb = -2.0f, .fxDrift = 0.3f, .fxGrain = 0.2f,
          .evolutionSpeed = 1.5f, .evolutionDepth = 1.5f, .unreal = 1.6f, .grade = 0x34ff3030, .light = 0.5f, .flicker = 0.5f },
    }},
    {{   // Garage: a car door far off, the barrier, a drip, the lamps
        { .name = "VACANT", .mood = "Nobody here; the fans and the concrete", .recommended = "PIANO + DRONE",
          .eventRate = 0.7f, .bed = 0.75f, .hum = 0.8f, .grade = 0x30ff7a1e, .light = 0.9f },
        { .name = "DISTANT MOVEMENT", .mood = "Cars on other levels, a lift, something far off", .recommended = "SYNTH + DRONE",
          .eventRate = 1.3f, .eventWeight = { 2, 1, 1, 1, 1 }, .extraKind = doorClose, .extraMean = 20, .bed = 1.35f, .bedTone = 1.3f, .decay = 1.15f, .wetDb = 1.0f,
          .fxEcho = 0.1f, .grade = 0x30ff9a3e, .light = 1.0f },
        { .name = "PURSUIT", .mood = "Engines, doors, footsteps you cannot place", .recommended = "SYNTH + DRONE",
          .eventRate = 2.4f, .eventWeight = { 2.5f, 2.5f, 1.5f, 1.5f, 1 }, .extraKind = wrongWall, .extraMean = 5, .hum = 1.3f, .decay = 0.8f,
          .dryDb = 2.0f, .fxDrift = 0.1f, .fxEcho = 0.2f, .evolutionSpeed = 2.0f, .evolutionDepth = 1.4f, .unreal = 1.2f,
          .grade = 0x3aff4a10, .light = 0.7f, .flicker = 0.8f },
    }},
    {{   // Pool: a drip into still water, very rarely a ripple
        { .name = "STILL WATER", .mood = "Flat water, a drip now and then", .recommended = "GLASS + PIANO",
          .eventRate = 0.7f, .grade = 0x2a38c4e8, .light = 1.0f },
        { .name = "FORGOTTEN SUMMER", .mood = "Sun through the skylights, echoes of voices", .recommended = "SYNTH + MUSIC BOX",
          .eventRate = 1.4f, .extraKind = ripple, .extraMean = 8, .bedTone = 1.3f, .decay = 0.8f, .wetDb = -2.0f, .wowCents = 7,
          .chorusMix = 0.25f, .fxTape = 0.35f, .fxChorus = 0.2f, .grade = 0x32ffe6a0, .light = 1.3f },
        { .name = "SUBMERGED", .mood = "Under the surface: muffled, slow, enveloping", .recommended = "DRONE + CHOIR",
          .eventRate = 0.7f, .bed = 1.3f, .bedTone = 0.4f, .decay = 1.5f, .roomBright = 0.3f, .lowPass = 0.22f, .wowCents = 8,
          .chorusMix = 0.4f, .fxChorus = 0.3f, .fxGrain = 0.2f, .unreal = 1.3f, .grade = 0x44104090, .light = 0.5f },
    }},
    {{   // Hotel: the lift's chime below, a door down the hall
        { .name = "LATE NIGHT", .mood = "Everyone asleep behind their doors", .recommended = "PIANO + SYNTH",
          .eventRate = 0.8f, .grade = 0x2cd29a64, .light = 0.8f },
        { .name = "FADED LUXURY", .mood = "The lobby: marble, gold, a lounge that closed", .recommended = "PIANO + GLASS",
          .eventRate = 0.6f, .eventWeight = { 0.5f, 1.5f, 0.5f, 0.5f, 1 }, .extraKind = paChime, .extraMean = 12, .bed = 1.3f, .bedTone = 1.4f, .hum = 0.7f, .decay = 1.6f, .roomBright = 2.0f,
          .wetDb = 2.0f, .lowPass = 1.8f, .chorusMix = 0.15f, .fxTape = 0.2f, .grade = 0x32ffcf70, .light = 1.0f },
        { .name = "ROOM 404", .mood = "The room that should not be on this floor", .recommended = "CHOIR + DRONE",
          .eventRate = 1.6f, .eventWeight = { 1, 2, 2, 2, 1 }, .extraKind = wrongWall, .extraMean = 9, .bed = 0.7f, .hum = 1.7f, .lowPass = 0.8f, .driftCents = 10,
          .shiftMix = 0.15f, .wanderPan = 0.3f, .fxDrift = 0.2f, .fxGrain = 0.15f, .evolutionSpeed = 1.5f, .evolutionDepth = 1.5f,
          .unreal = 1.5f, .grade = 0x32b0c040, .light = 0.5f, .flicker = 0.5f },
    }},
    {{   // Subway: a train that never arrives, rails ticking, a tube
        { .name = "LAST TRAIN", .mood = "It just left; the rails are still singing", .recommended = "SYNTH + PIANO",
          .eventRate = 1.2f, .eventWeight = { 1.5f, 1, 1.2f, 1.2f, 1 }, .grade = 0x28a6c46a, .light = 0.9f },
        { .name = "ABANDONED", .mood = "Closed for years: leaks, dead power, dark tunnels", .recommended = "DRONE + GLASS",
          .eventRate = 0.4f, .eventWeight = { 0.3f, 1, 0.7f, 0.7f, 1 }, .extraKind = drip, .extraMean = 3, .bed = 0.8f, .hum = 0.4f,
          .decay = 1.35f, .fxTape = 0.2f, .grade = 0x34406040, .light = 0.5f },
        { .name = "APPROACHING", .mood = "Something is coming down the tunnel", .recommended = "SYNTH + DRONE",
          .eventRate = 1.8f, .eventWeight = { 3, 2, 1, 1, 1 }, .extraKind = railTick, .extraMean = 7, .bed = 1.4f, .decay = 0.9f, .fxEcho = 0.15f,
          .evolutionSpeed = 2.0f, .evolutionDepth = 1.5f, .unreal = 1.2f, .grade = 0x30e0f0ff, .light = 1.1f, .flicker = 0.7f },
    }},
    {{   // Supermarket: a freezer seal, a trolley settling, the tubes
        { .name = "CLOSING SHIFT", .mood = "Lights half off, a till still beeping", .recommended = "SYNTH + MUSIC BOX",
          .eventRate = 1.1f, .eventWeight = { 1.5f, 1, 1, 1, 1 }, .bedTone = 1.4f, .hum = 1.4f, .grade = 0x24d6f2ff, .light = 1.1f },
        { .name = "NOSTALGIA", .mood = "A 1990s Saturday, through worn tape", .recommended = "MUSIC BOX + SYNTH",
          .eventRate = 0.6f, .extraKind = paChime, .extraMean = 10, .bed = 1.4f, .bedTone = 0.7f, .hum = 0.55f, .lowPass = 0.8f, .wowCents = 6, .chorusMix = 0.25f,
          .fxTape = 0.35f, .grade = 0x32ffe0b0, .light = 1.2f },
        { .name = "FLUORESCENT NIGHTMARE", .mood = "The tubes are screaming; everything hums", .recommended = "SYNTH + DRONE",
          .eventRate = 1.6f, .eventWeight = { 2, 1, 2, 2, 1 }, .extraKind = flicker, .extraMean = 10, .hum = 2.4f, .driftCents = 8,
          .shiftMix = 0.1f, .tremolo = 0.35f, .fxDrift = 0.15f, .evolutionSpeed = 1.4f, .evolutionDepth = 1.4f, .unreal = 1.4f,
          .grade = 0x3cc0ffc0, .light = 1.5f, .flicker = 1.0f },
    }},
    {{   // Office: almost nothing; the ballast stutters
        { .name = "AFTER HOURS", .mood = "Screens asleep, the HVAC never stops", .recommended = "SYNTH + PIANO",
          .eventRate = 0.8f, .bedTone = 1.3f, .hum = 1.6f, .grade = 0x22c4dcb4, .light = 0.8f },
        { .name = "DREAM STATE", .mood = "Soft focus; the room breathes", .recommended = "CHOIR + SYNTH",
          .eventRate = 0.5f, .bed = 1.3f, .bedTone = 0.7f, .hum = 0.55f, .decay = 1.5f, .wetDb = 2.0f, .lowPass = 0.7f, .chorusMix = 0.3f, .dryDb = -2.0f,
          .fxChorus = 0.2f, .fxGrain = 0.25f, .evolutionSpeed = 0.6f, .evolutionDepth = 1.6f, .grade = 0x32d0b0ff, .light = 1.1f },
        { .name = "INFINITE CUBICLES", .mood = "Every aisle the same, every sound repeated", .recommended = "SYNTH + GLASS",
          .eventRate = 1.5f, .eventWeight = { 2, 2, 1, 1, 1 }, .extraKind = doorClick, .extraMean = 8, .hum = 1.35f, .decay = 1.3f, .fxEcho = 0.25f,
          .unreal = 1.3f, .grade = 0x2ab0d0c0, .light = 1.0f, .flicker = 0.3f },
    }},
    {{   // Impossible room: a note nobody played, a knock from no wall
        { .name = "FAMILIAR", .mood = "Almost a normal room. Almost.", .recommended = "PIANO + GLASS",
          .eventRate = 0.8f, .decay = 0.9f, .shiftMix = -0.12f, .wanderPan = -0.25f, .unreal = 0.7f, .grade = 0x24b27aff, .light = 1.0f },
        { .name = "UNSTABLE", .mood = "Nothing holds still: pitch, space, light", .recommended = "GLASS + DRONE",
          .eventRate = 1.3f, .driftCents = 10, .wanderPan = 0.2f, .evolutionSpeed = 1.8f, .evolutionDepth = 1.8f, .unreal = 1.2f,
          .grade = 0x32ff60d0, .light = 0.9f, .flicker = 0.5f },
        { .name = "BEYOND REALITY", .mood = "The walls are gone; only the reverb remains", .recommended = "CHOIR + DRONE + GLASS",
          .eventRate = 1.6f, .decay = 1.4f, .wetDb = 2.0f, .shiftMix = 0.25f, .fxDrift = 0.2f, .fxGrain = 0.3f,
          .evolutionSpeed = 1.4f, .evolutionDepth = 2.0f, .unreal = 1.8f, .grade = 0x3ae0c0ff, .light = 1.3f, .flicker = 0.3f },
    }},
} };

const std::array<UnrealFlavour, 10> flavours { {
    { .shiftMix = 0.5f, .wanderPan = 0.3f, .wowCents = 14, .grain = 0.2f, .echo = 0.4f, .shimmer = 0.3f },               // hospital: wrong pitch, ghostly tail
    { .wanderPan = 0.5f, .grain = 0.6f, .chorus = 0.3f, .echo = 0.2f, .shimmer = 0.45f },                                 // mall: the atrium dissolves upwards
    { .wanderPan = 0.7f, .wowCents = 25, .drift = 0.8f, .tape = 0.9f, .grain = 0.2f, .chorus = 0.6f },                    // school: pitch instability, degradation, movement
    { .shiftMix = 0.35f, .wowCents = 6, .grain = 0.4f, .echo = 0.9f, .echoShift = -12 },                                  // garage: the concrete answers back, an octave down
    { .wanderPan = 0.2f, .echo = 1.0f, .shimmer = 0.45f, .echoShift = 7 },                                                // pool: unnatural reflective echoes
    { .shiftMix = 0.2f, .wanderPan = 0.6f, .tape = 0.9f, .grain = 0.3f },                                                 // hotel: the tape and the walls warp
    { .wowCents = 10, .drift = 0.7f, .echo = 0.6f, .shimmer = 0.4f, .echoShift = -5 },                                    // subway: the tunnel bends the sound
    { .shiftMix = 0.6f, .chorus = 0.8f, .echo = 0.2f },                                                                   // supermarket: electrical, synthetic
    { .wanderPan = 0.5f, .grain = 0.9f, .echo = 0.6f, .echoShift = 12 },                                                  // office: fragments, repeats that climb
    { .shiftMix = 0.6f, .wanderPan = 0.8f, .drift = 0.6f, .grain = 0.6f, .echo = 0.8f, .shimmer = 0.6f, .echoShift = 12 }, // impossible: all of it
} };

const std::array<MacroResponse, 10> responses { {
    { 1.0f, 1.0f, 1.0f }, { 0.8f, 1.4f, 0.8f }, { 1.2f, 0.6f, 1.2f }, { 1.0f, 1.2f, 1.3f }, { 0.9f, 1.3f, 0.8f },
    { 1.3f, 0.5f, 0.9f }, { 1.0f, 1.3f, 1.4f }, { 1.1f, 0.8f, 1.1f }, { 1.9f, 1.2f, 1.0f }, { 1.0f, 1.6f, 1.5f },
} };
} // namespace

const Character& character(int place, int index) {
    return table[std::size_t(std::clamp(place, 0, 9))][std::size_t(std::clamp(index, 0, characterCount - 1))];
}
const UnrealFlavour& unrealFlavour(int place) { return flavours[std::size_t(std::clamp(place, 0, 9))]; }
const MacroResponse& macroResponse(int place) { return responses[std::size_t(std::clamp(place, 0, 9))]; }

void Evolution::start(unsigned seed) {
    rng.state = 0x7F4A7C15u ^ (seed * 0x9E3779B1u);
    a.start(rng);
    b.start(rng);
    c.start(rng);
    spectralNow = spatialNow = harmonicNow = movementNow = 0;
    nextDisturbance = 12.0f + 20.0f * rng.uniform();
    disturbance = false;
}

void Evolution::tick(float seconds, float rate, float speed, float depth, float activity, bool frozen) {
    if (frozen) {   // everything holds where it is
        movementNow *= 0.999f;
        return;
    }
    // A change of direction every ~100 s at rate 0, every ~5 s at rate 1.
    const float hz = 0.01f * std::pow(20.0f, std::clamp(rate, 0.0f, 1.0f)) * speed * activity;
    const float before = spectralNow + spatialNow + harmonicNow;
    spectralNow = depth * a.tick(rng, hz, seconds);
    spatialNow = depth * b.tick(rng, hz * 0.7f, seconds);
    harmonicNow = depth * c.tick(rng, hz * 1.3f, seconds);
    const float change = std::abs(spectralNow + spatialNow + harmonicNow - before) / std::max(1.0e-6f, seconds);
    movementNow += 0.02f * (std::min(1.0f, change / (3.0f * hz + 1.0e-4f)) - movementNow);
    // Disturbances: a Poisson process whose rate follows the evolution rate and activity.
    nextDisturbance -= seconds * (0.3f + 1.7f * rate) * speed * activity;
    if (nextDisturbance <= 0.0f) {
        disturbance = true;
        nextDisturbance = 8.0f - 25.0f * std::log(std::max(1.0e-4f, rng.uniform()));
    }
}

} // namespace liminal::dsp
