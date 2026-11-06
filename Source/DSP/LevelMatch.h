#pragma once
// Level matching in the rooms (2026-11-06). Each place's processing of an instrument (its signature,
// the cold filtering, the echoes, the Character's colour) changes how loud that instrument is by a
// different amount: the hospital's bright sheen and flutter lift a pizzicato or a toy piano by several
// dB while it thins a warm synth pad, and the choir and the drone were always quieter than the rest.
// This table brings each of the twenty instruments to the Forgotten Piano's loudness (the default
// instrument, never changed by it) in every place and Character
// at the defaults (ITU-R BS.1770 loudness of one test phrase, recordings; Tests/LoudnessTests.h),
// without touching the instruments themselves: the trim rides on how much the place shapes the
// instrument (Environment Amount x the slot's Environment Influence), so a dry instrument (both at 0)
// is exactly as before. Regenerate with `LiminalTests --calibrate-loudness` (see the DEVLOG).
#include "DSP/Characters.h"
#include "DSP/Environment.h"
#include <algorithm>

namespace liminal::dsp::levelmatch {

inline constexpr int instruments = 20;
#include "DSP/LevelMatchTable.h"   // float table[instruments][placeCount][characterCount] (dB)

// Tests and the calibration itself: the table off.
inline bool& bypassed() {
    static bool off = false;
    return off;
}

// The trim (dB) for `instrument` (0..19) in `place` with Character `character`, at `amount` of the
// place's influence on it (0..1: Environment Amount x Environment Influence).
inline float trimDb(int instrument, int place, int character, float amount) {
    if (bypassed() || instrument < 0 || instrument >= instruments) return 0.0f;
    const int p = std::clamp(place, 0, placeCount - 1), c = std::clamp(character, 0, characterCount - 1);
    return table[instrument][p][c] * std::clamp(amount, 0.0f, 1.0f);
}

} // namespace liminal::dsp::levelmatch
