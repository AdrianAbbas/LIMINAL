#pragma once
// LIMINAL's automatable parameters.
// IDs are stable: append new ones, never rename or reuse an ID (saved sessions and
// host automation refer to them). Names are what hosts and the AU harness see.
#include <juce_audio_processors/juce_audio_processors.h>

namespace liminal::params {

inline constexpr const char* power = "power";           // Power (fades everything in/out)
inline constexpr const char* master = "master";         // Master Level, dB
inline constexpr const char* instrument = "instrument"; // Instrument Level, dB
inline constexpr const char* ambience = "ambience";     // Ambience Level, dB
inline constexpr const char* space = "space";           // Space: how much of the room is heard

// Forgotten Piano (Milestone 1a)
inline constexpr const char* pianoOn = "piano_on";
inline constexpr const char* pianoLevel = "piano_level";
inline constexpr const char* pianoAge = "piano_age";
inline constexpr const char* pianoDetune = "piano_detune";
inline constexpr const char* pianoSoftness = "piano_softness";
inline constexpr const char* pianoNoise = "piano_noise";
inline constexpr const char* pianoDecay = "piano_decay";
inline constexpr const char* pianoDistance = "piano_distance";

// Faded Synth (Milestone 1a)
inline constexpr const char* synthOn = "synth_on";
inline constexpr const char* synthLevel = "synth_level";
inline constexpr const char* synthDetune = "synth_detune";
inline constexpr const char* synthWarmth = "synth_warmth";
inline constexpr const char* synthDrift = "synth_drift";
inline constexpr const char* synthAttack = "synth_attack";
inline constexpr const char* synthRelease = "synth_release";
inline constexpr const char* synthWidth = "synth_width";
inline constexpr const char* synthGlide = "synth_glide";          // off by default
inline constexpr const char* synthGlideTime = "synth_glidetime";

// Glass Memories (Milestone 1a)
inline constexpr const char* glassOn = "glass_on";
inline constexpr const char* glassLevel = "glass_level";
inline constexpr const char* glassFragility = "glass_fragility";
inline constexpr const char* glassBrightness = "glass_brightness";
inline constexpr const char* glassResonance = "glass_resonance";
inline constexpr const char* glassDetune = "glass_detune";
inline constexpr const char* glassDecay = "glass_decay";
inline constexpr const char* glassDistance = "glass_distance";

// Empty Choir (Milestone 1b)
inline constexpr const char* choirOn = "choir_on";
inline constexpr const char* choirLevel = "choir_level";
inline constexpr const char* choirBreath = "choir_breath";
inline constexpr const char* choirFormant = "choir_formant";
inline constexpr const char* choirDetune = "choir_detune";
inline constexpr const char* choirWidth = "choir_width";
inline constexpr const char* choirDarkness = "choir_darkness";
inline constexpr const char* choirRelease = "choir_release";

// Lost Music Box (Milestone 1b)
inline constexpr const char* boxOn = "box_on";
inline constexpr const char* boxLevel = "box_level";
inline constexpr const char* boxWear = "box_wear";
inline constexpr const char* boxDrift = "box_drift";
inline constexpr const char* boxMechanism = "box_mechanism";
inline constexpr const char* boxBrightness = "box_brightness";
inline constexpr const char* boxDecay = "box_decay";
inline constexpr const char* boxDistance = "box_distance";

// Hollow Drone (Milestone 1b)
inline constexpr const char* droneOn = "drone_on";
inline constexpr const char* droneLevel = "drone_level";
inline constexpr const char* droneDensity = "drone_density";
inline constexpr const char* droneInstability = "drone_instability";
inline constexpr const char* droneHarmonics = "drone_harmonics";
inline constexpr const char* droneSub = "drone_sub";
inline constexpr const char* droneMovement = "drone_movement";
inline constexpr const char* droneRelease = "drone_release";
inline constexpr const char* droneAttack = "drone_attack";

// Solo (Milestone 1b): 0 = none, 1..6 = only that instrument is heard (piano, synth,
// glass, choir, music box, drone). Ignored while the soloed instrument is off.
inline constexpr const char* solo = "solo";
inline constexpr int instrumentCount = 6;

// Environment (Milestone 2a): which place, and how strongly it processes the instruments
// (0 = the instruments untouched; the room is still Space).
inline constexpr const char* environment = "environment";
inline constexpr const char* environmentAmount = "env_amount";

// Per-instrument effects (Milestone 3): "<instrument>_fx_<name>", e.g. piano_fx_echo_time.
// Every instrument has the same set. Appended after everything above; never renamed.
inline constexpr const char* instrumentKeys[] { "piano", "synth", "glass", "choir", "box", "drone" };
inline constexpr const char* instrumentLabels[] { "Piano", "Synth", "Glass", "Choir", "Music Box", "Drone" };
namespace fx {
inline constexpr const char* mix = "mix";               // effects dry/wet (0 = the dry instrument)
inline constexpr const char* influence = "influence";   // how much the place shapes this instrument
inline constexpr const char* order = "order";           // effect order (choice)
inline constexpr const char* driftOn = "drift_on";
inline constexpr const char* drift = "drift";
inline constexpr const char* driftRate = "drift_rate";
inline constexpr const char* tapeOn = "tape_on";
inline constexpr const char* tape = "tape";
inline constexpr const char* tapeTone = "tape_tone";
inline constexpr const char* tapeHiss = "tape_hiss";
inline constexpr const char* tapeDropouts = "tape_dropouts";
inline constexpr const char* reverbOn = "reverb_on";
inline constexpr const char* reverb = "reverb";
inline constexpr const char* echoOn = "echo_on";
inline constexpr const char* echo = "echo";
inline constexpr const char* echoTime = "echo_time";
inline constexpr const char* echoSync = "echo_sync";
inline constexpr const char* echoFeedback = "echo_feedback";
inline constexpr const char* echoTone = "echo_tone";
inline constexpr const char* chorusOn = "chorus_on";
inline constexpr const char* chorus = "chorus";
inline constexpr const char* chorusRate = "chorus_rate";
inline constexpr const char* filterOn = "filter_on";
inline constexpr const char* filter = "filter";
inline constexpr const char* filterLow = "filter_lp";
inline constexpr const char* filterHigh = "filter_hp";
inline constexpr const char* filterRes = "filter_res";
inline constexpr const char* grainOn = "grain_on";
inline constexpr const char* grain = "grain";
inline constexpr const char* grainSize = "grain_size";
inline constexpr const char* grainSpread = "grain_spread";
inline constexpr const char* grainFreeze = "grain_freeze";
inline constexpr const char* all[] { mix, influence, order, driftOn, drift, driftRate, tapeOn, tape, tapeTone, tapeHiss, tapeDropouts,
                                     reverbOn, reverb, echoOn, echo, echoTime, echoSync, echoFeedback, echoTone, chorusOn, chorus, chorusRate,
                                     filterOn, filter, filterLow, filterHigh, filterRes, grainOn, grain, grainSize, grainSpread, grainFreeze };
inline constexpr int count = int(sizeof(all) / sizeof(all[0]));
inline const char* syncNames[] { "Free", "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1 bar" };
inline constexpr float syncBeats[] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };
inline const char* orderNames[] { "Standard", "Texture first", "Echo into texture" };
} // namespace fx
// Milestone 5: Character, evolution, macros, scenes.
inline constexpr const char* character = "character";              // 0..2: the place's three Characters
inline constexpr const char* evolution = "evolution";              // evolution rate
inline constexpr const char* evolutionFreeze = "evolution_freeze";
inline constexpr const char* evolutionSeed = "evolution_seed";
inline constexpr const char* presence = "macro_presence";
inline constexpr const char* scale = "macro_scale";
inline constexpr const char* activity = "macro_activity";
inline constexpr const char* unreal = "macro_unreal";
inline constexpr const char* sceneOn = "scene_on";                 // play the morph between scenes A and B
inline constexpr const char* sceneMorph = "scene_morph";           // 0 = A, 1 = B
inline juce::String fxId(int instrument, const char* name) { return juce::String(instrumentKeys[instrument]) + "_fx_" + name; }

inline constexpr float minDb = -60.0f; // the bottom of every level: silence
inline constexpr float maxDb = 6.0f;

// A level parameter's dB value as a gain, with the minimum meaning silence.
inline float levelGain(float db) { return db <= minDb + 0.01f ? 0.0f : juce::Decibels::decibelsToGain(db); }

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

} // namespace liminal::params
