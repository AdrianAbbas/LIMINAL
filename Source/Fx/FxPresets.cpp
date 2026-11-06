#include "Fx/FxPresets.h"
#include "Fx/FxParameters.h"

namespace liminal::fx::presets {

namespace {
// Places (the Room parameter) and their Characters, as numbers.
enum Place { hospital, mall, school, garage, pool, hotel, subway, supermarket, office, impossible };
using V = std::vector<std::pair<juce::String, float>>;
// The common part of every preset: where, which Character, how strong, how much room, LIMINALITY.
V where(Place p, int character, float intensity, float space, float liminality, float mix = 1.0f) {
    return { { fxp::environment, float(p) }, { fxp::character, float(character) }, { fxp::intensity, intensity },
             { fxp::space, space }, { fxp::liminality, liminality }, { fxp::mix, mix } };
}
V operator+(V a, const V& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
// One Liminality control ("detune", "wear", ...).
std::pair<juce::String, float> lim(const char* key, float v) { return { juce::String("fx_lim_") + key, v }; }
} // namespace

const std::vector<Preset>& factory() {
    static const std::vector<Preset> all {
        // Vocals
        { "Mall at 3 AM", Source::vocals, "A voice in the empty atrium, endless soft decay, the old PA's chorus",
          where(mall, 2, 0.8f, 0.6f, 0.45f, 0.9f) + V { lim("warble", 0.5f), lim("ghost", 0.5f), { fxp::ambience, -20.0f } } },
        { "Answering Machine", Source::vocals, "A message left years ago: a small worn speaker, through the wall",
          where(hotel, 0, 0.75f, 0.12f, 0.55f) + V { { fxp::farAway, 0.9f }, lim("wear", 0.85f), lim("wow", 0.55f), lim("fade", 0.15f), { fxp::ambience, -24.0f } } },
        { "Fluorescent Corridor", Source::vocals, "Cold hospital corridor, the tube humming over the voice",
          where(hospital, 0, 0.75f, 0.5f, 0.4f) + V { lim("detune", 0.55f), { fxp::ambience, -18.0f } } },
        { "Room 404", Source::vocals, "Through the door left ajar: ghosts of the line, notes that come out sour",
          where(hotel, 2, 0.8f, 0.4f, 0.6f) + V { lim("ghost", 0.75f), lim("sour", 0.6f), lim("drift", 0.5f) } },
        { "Dream State", Source::vocals, "An office that goes on forever, a voice half remembered",
          where(office, 1, 0.75f, 0.55f, 0.55f) + V { lim("warble", 0.6f), lim("fade", 0.5f) } },
        // Guitar
        { "Hallway Guitar", Source::guitar, "An abandoned school hallway: boxy, a tight slap-back, worn tape",
          where(school, 0, 0.75f, 0.42f, 0.4f) + V { lim("wow", 0.55f) } },
        { "Distant Movement", Source::guitar, "Concrete levels of a parking garage, something moving far off",
          where(garage, 1, 0.8f, 0.55f, 0.4f) + V { lim("ghost", 0.5f), lim("timing", 0.5f) } },
        { "Last Train Home", Source::guitar, "An empty platform, the tunnel's long echo, sagging pitch",
          where(subway, 0, 0.8f, 0.55f, 0.45f) + V { lim("drift", 0.5f) } },
        { "Forgotten Summer", Source::guitar, "A tiled pool hall in the off-season, shimmering tail, tape wow",
          where(pool, 1, 0.75f, 0.55f, 0.5f) + V { lim("wow", 0.6f), lim("wear", 0.55f) } },
        // Keys
        { "Hotel Lobby Piano", Source::keys, "Faded luxury: carpet, a soft short room, an old tape of the lobby",
          where(hotel, 1, 0.7f, 0.5f, 0.45f) + V { lim("wear", 0.55f), lim("detune", 0.5f) } },
        { "Night Shift Keys", Source::keys, "The hospital after hours, dim, monitors somewhere",
          where(hospital, 1, 0.75f, 0.55f, 0.4f) + V { { fxp::ambience, -16.0f } } },
        { "Infinite Cubicles", Source::keys, "Synths and pads in an open plan that never ends",
          where(office, 2, 0.8f, 0.6f, 0.5f) + V { lim("ghost", 0.6f), lim("timing", 0.55f) } },
        { "Supermarket Muzak", Source::keys, "Tired muzak on the frozen aisle's speakers, a mild chorus, mains hum",
          where(supermarket, 0, 0.75f, 0.35f, 0.55f) + V { lim("wow", 0.65f), lim("wear", 0.6f), { fxp::farAway, 0.4f } } },
        // Drums
        { "Empty Pool Drums", Source::drums, "Drums in a drained pool: bright tiles, distinct echoes, a long tail",
          where(pool, 0, 0.85f, 0.6f, 0.25f) + V { lim("timing", 0.3f), lim("detune", 0.2f), lim("sour", 0.1f) } },
        { "Parking Level B3", Source::drums, "Concrete: heavy low-mids, hard early reflections",
          where(garage, 0, 0.85f, 0.6f, 0.25f) + V { lim("sour", 0.1f), lim("detune", 0.2f) } },
        { "Tunnel Snare", Source::drums, "The abandoned platform: metallic ringing, the tunnel's echo",
          where(subway, 1, 0.8f, 0.55f, 0.3f) + V { lim("sour", 0.1f) } },
        // Mix
        { "Worn VHS Mix", Source::mix, "The whole mix played back from a worn tape: wow, flutter, dropouts, a little room",
          where(supermarket, 1, 0.45f, 0.18f, 0.6f) + V { lim("wear", 0.85f), lim("wow", 0.75f), lim("warble", 0.5f), lim("detune", 0.35f),
                                                          lim("ghost", 0.2f), lim("sour", 0.15f), lim("fade", 0.25f), { fxp::ambience, -30.0f } } },
        { "Childhood Tape", Source::mix, "A school recording found years later: wow and tape wear",
          where(school, 1, 0.55f, 0.3f, 0.6f) + V { lim("wow", 0.7f), lim("wear", 0.65f), { fxp::ambience, -26.0f } } },
        { "Distant Radio", Source::mix, "Over the PA in another wing of the mall: band-limited, tinny, far off",
          where(mall, 0, 0.8f, 0.5f, 0.5f) + V { { fxp::farAway, 1.0f }, lim("wear", 0.6f), lim("wow", 0.5f), { fxp::ambience, -22.0f } } },
        { "Clean Room", Source::mix, "Gentle: the hospital's quiet room on the whole mix, a hint of LIMINALITY",
          where(hospital, 0, 0.5f, 0.3f, 0.25f, 0.7f) + V { { fxp::ambience, -40.0f } } },
        // Any
        { "Fading Memory", Source::any, "It fades away and comes back, a little more wrong each time",
          where(pool, 2, 0.75f, 0.55f, 0.6f) + V { lim("fade", 0.9f), lim("ghost", 0.6f), lim("warble", 0.55f) } },
        { "Something Wrong", Source::any, "The hospital when the alarms start: the pitch slips, the lights stutter",
          where(hospital, 2, 0.8f, 0.5f, 0.7f) + V { lim("sour", 0.7f), lim("drift", 0.6f) } },
        { "Fluorescent Nightmare", Source::any, "The supermarket after closing, wrong in every aisle",
          where(supermarket, 2, 0.85f, 0.45f, 0.65f) + V { lim("warble", 0.6f), lim("sour", 0.5f) } },
        { "Submerged", Source::any, "Under the pool's surface: slow warble, everything fading",
          where(pool, 2, 0.85f, 0.65f, 0.55f) + V { lim("warble", 0.75f), lim("fade", 0.55f) } },
        { "Beyond Reality", Source::any, "The room that should not exist: shifted partials, a rising tail",
          where(impossible, 2, 0.85f, 0.6f, 0.6f) + V { { fxp::unreal, 0.55f }, lim("ghost", 0.6f) } },
        { "Unstable", Source::any, "The impossible room shifting under your feet",
          where(impossible, 1, 0.8f, 0.55f, 0.55f) + V { lim("drift", 0.6f), lim("warble", 0.55f) } },
    };
    return all;
}

std::vector<int> inSource(Source s) {
    std::vector<int> out;
    const auto& all = factory();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].source == s) out.push_back(int(i));
    return out;
}

} // namespace liminal::fx::presets
