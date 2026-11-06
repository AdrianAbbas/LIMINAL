#pragma once
// TAPEWORN FX's automatable parameters (2026-11-06). IDs are stable: append, never rename.
// The place's parameters share their IDs (and meaning) with the instrument - environment,
// character, space, ambience, the four macros, evolution, far_away, quality, vhs, vhs_warble and
// liminality - so the editor's shared parts (UI/Widgets, UI/Viewfinder) bind to either plug-in.
// The effect's own ones start with "fx_".
#include "DSP/Liminality.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace liminal::fxp {

inline constexpr const char* bypass = "fx_bypass";        // click-free (a 25 ms cross-fade to the input)
inline constexpr const char* mix = "fx_mix";              // dry / wet: 0 = the input untouched, 1 = only TAPEWORN's world
inline constexpr const char* input = "fx_input";          // Input level, dB (into the world; the dry share keeps the input's own level)
inline constexpr const char* output = "fx_output";        // Output level, dB
inline constexpr const char* intensity = "fx_intensity";  // how strongly the place transforms the input
// The place (shared IDs).
using params::environment;
using params::character;
using params::space;        // how much of the room is heard (x Intensity)
using params::ambience;     // Room Sound: the place's own bed and events, only while something plays
using params::presence;
using params::scale;
using params::activity;
using params::unreal;
using params::evolution;
using params::evolutionFreeze;
using params::evolutionSeed;
using params::farAway;
using params::quality;
using params::vhs;
using params::vhsWarble;
using params::liminality;   // the LIMINALITY macro: 0 clean, 40 % the amounts as set, 100 % 2.5x them
// The ten Liminality controls, "fx_lim_<key>" (dsp::lim::keys).
inline juce::String limId(int control) { return juce::String("fx_lim_") + dsp::lim::keys[control]; }
// The effect's own default amounts (with LIMINALITY at its default, 40 %).
inline constexpr float limDefaults[dsp::lim::count] { 0.45f, 0.40f, 0.45f, 0.40f, 0.40f, 0.45f, 0.40f, 0.40f, 0.35f, 0.35f };
inline constexpr float minDb = -24.0f, maxDb = 24.0f;   // Input / Output

inline constexpr float defaultMix = 1.0f;
inline constexpr float defaultIntensity = 0.7f;
inline constexpr float defaultSpace = 0.45f;
inline constexpr float defaultRoomSound = -12.0f;

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

} // namespace liminal::fxp
