#pragma once
// The audio thread's view for the editor (2026-11-06: shared by VACANCY and VACANCY FX).
#include <atomic>

namespace liminal {

// What the viewfinder reads from the audio thread (written once per block, read by the editor).
struct SceneMeters {
    std::atomic<float> instrument { 0 }, ambience { 0 }, output { 0 }; // RMS, linear (FX: input, room sound, output)
    std::atomic<int> place { 0 };   // the environment being heard
    std::atomic<int> character { 0 };
    std::atomic<float> activity { 0 };   // evolution movement + events (0..1), for the interface
    std::atomic<int> events { 0 };      // the place's incidental sounds so far (the lights answer them)
    std::atomic<int> notes { 0 };       // note-ons so far (FX: attacks in the input) - the lights answer them
    std::atomic<float> velocity { 0 };  // the latest note-on's velocity (0..1): how much the lights swell
    std::atomic<float> lamp { 1 };      // the failing tube's lowest brightness since the editor took it (1 = steady)
    std::atomic<int> flickers { 0 };    // its stutters so far: the picture's tube flickers with the hum
};

} // namespace liminal
