#pragma once
// Written by tools/levelmatch/fx_levelmatch.py from `TapewornFXTests --calibrate-levels` (do not edit by hand).
// dB per place (hospital .. impossible room) x Character, at the default settings.
namespace liminal::fx::levelmatch {
inline constexpr float table[10][3] {
    { -0.31f, -0.33f, 0.36f },
    { 2.74f, 1.71f, -0.43f },
    { 5.62f, 6.66f, 7.63f },
    { 2.48f, 1.04f, 1.76f },
    { -2.53f, 0.48f, -0.75f },
    { 5.96f, 3.96f, 7.32f },
    { 4.55f, 4.66f, 4.24f },
    { 1.71f, 1.90f, 1.43f },
    { -0.62f, 3.33f, -0.77f },
    { 2.82f, 2.61f, 1.27f },
};
} // namespace liminal::fx::levelmatch
