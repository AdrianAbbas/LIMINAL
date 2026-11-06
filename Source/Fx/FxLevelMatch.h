#pragma once
// TAPEWORN FX's level match (2026-11-06): every room and Character brings the input back to the
// loudness it came in with (at Mix 100 %, the default settings), so changing rooms compares the
// places, not their levels. dB per place x Character, measured (ITU-R BS.1770, gated) over the
// three test sources (guitar, vocal, a full mix) by `TapewornFXTests --calibrate-levels`
// (tools/levelmatch/fx_levelmatch.py writes the table). The trim rides on Intensity (0 = none).
#include "Fx/FxLevelMatchTable.h"
#include "Fx/FxParameters.h"
#include <algorithm>

namespace liminal::fx::levelmatch {

inline float trimDb(int place, int character, float intensity) {
    const float db = table[std::clamp(place, 0, 9)][std::clamp(character, 0, 2)];
    return db * std::clamp(intensity / fxp::defaultIntensity, 0.0f, 1.0f / fxp::defaultIntensity);
}

} // namespace liminal::fx::levelmatch
