#pragma once
// Procedural 2.5D environment art (Milestone 4): every place is its own architecture —
// geometry in one-point perspective, materials, fog, its own artificial light — finished
// like a photograph (the Character's colour grade and light level, halation around lights,
// vignette, film grain). Rendered once per place / Character / size into a cached Image,
// never per frame; the animation layer only draws small light glows on top.
//
// If the developer adds a photograph (photographFolder()/<n>.png, Milestone 4), `renderPlace`
// uses it instead of the drawing, with the same finish. Since Direction Update 2 the places with
// rendered light passes (UI/PhotoScene: hospital, pool, office) are shown photographically
// instead; the drawing remains for the other seven and as their film-strip fallback.
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace liminal::ui {

struct Light {
    juce::Rectangle<float> glow;   // the halo around the fixture (logical coordinates)
    juce::Point<float> reflection; // where it shows on the floor (or -1, -1)
    float size = 1;                // how big it looks (for the animation)
};

struct Scene {
    juce::Image image;              // the full-bleed scene (physical pixels)
    juce::Image frosted;            // blurred, darkened copy for panels and the strip
    std::vector<Light> lights;      // light fixtures, near to far
    juce::Colour lightColour { 0xffe8fff2 };   // the colour of this place's light
    int place = 0, character = 0;
};

// `width` x `height` logical pixels times `scale` (Retina = 2). `quick` skips grain and
// halation (thumbnails).
Scene renderPlace(int place, int character, int width, int height, float scale, bool quick = false);

// The old Milestone 0 name, kept for callers: the hospital.
inline Scene renderCorridor(int width, int height, float scale, unsigned = 7) { return renderPlace(0, 0, width, height, scale); }

// Where the developer's own photographs would go (see above).
juce::File photographFolder();

} // namespace liminal::ui
