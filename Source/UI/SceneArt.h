#pragma once
// Procedural 2.5D environment art (Milestone 0: one scene, a vacant hospital corridor).
// The whole image is rendered once per editor size into a cached Image (never per frame):
// perspective geometry, ceiling lights with halation, floor reflections, fog, a cold
// colour grade, vignette and film grain. Milestone 4 builds every environment this way.
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace liminal::ui {

struct Scene {
    juce::Image image;              // the full-bleed scene (physical pixels)
    juce::Image frosted;            // blurred, darkened copy for the control strip
    std::vector<juce::Rectangle<float>> lights;   // ceiling-light glows, logical coordinates
};

// Renders the corridor at `width` x `height` logical pixels times `scale` (Retina = 2).
Scene renderCorridor(int width, int height, float scale, unsigned seed = 7);

} // namespace liminal::ui
