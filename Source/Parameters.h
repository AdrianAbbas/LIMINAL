#pragma once
// LIMINAL's automatable parameters (Milestone 0 skeleton set).
// IDs are stable: append new ones, never rename or reuse an ID (saved sessions and
// host automation refer to them). Names are what hosts and the AU harness see.
#include <juce_audio_processors/juce_audio_processors.h>

namespace liminal::params {

inline constexpr const char* power = "power";           // Power (fades everything in/out)
inline constexpr const char* master = "master";         // Master Level, dB
inline constexpr const char* instrument = "instrument"; // Instrument Level, dB
inline constexpr const char* ambience = "ambience";     // Ambience Level, dB
inline constexpr const char* space = "space";           // Space: how much of the room is heard

inline constexpr float minDb = -60.0f; // the bottom of every level: silence
inline constexpr float maxDb = 6.0f;

// A level parameter's dB value as a gain, with the minimum meaning silence.
inline float levelGain(float db) { return db <= minDb + 0.01f ? 0.0f : juce::Decibels::decibelsToGain(db); }

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

} // namespace liminal::params
