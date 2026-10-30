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

inline constexpr float minDb = -60.0f; // the bottom of every level: silence
inline constexpr float maxDb = 6.0f;

// A level parameter's dB value as a gain, with the minimum meaning silence.
inline float levelGain(float db) { return db <= minDb + 0.01f ? 0.0f : juce::Decibels::decibelsToGain(db); }

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

} // namespace liminal::params
