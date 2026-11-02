#pragma once
// The twenty instruments as line icons (2026-11-02), in the viewfinder's look: thin pale strokes with
// round ends, one weight for all of them, drawn as paths in a square so they stay sharp at any size
// (the shelf, the picker, the tabs; readable from 14 px). No logos: each is the object itself.
// The caller sets the colour; `instrument` is 0..19 (params::libraryNames).
#include <juce_gui_basics/juce_gui_basics.h>

namespace liminal::ui {

// Draws the icon centred in `area` (as a square). `weight` scales the stroke (1 = the standard weight).
void drawInstrumentIcon(juce::Graphics&, int instrument, juce::Rectangle<float> area, float weight = 1.0f);

} // namespace liminal::ui
