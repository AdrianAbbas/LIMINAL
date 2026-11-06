#include "UI/Viewfinder.h"
#include "Brand.h"
#include "DSP/Characters.h"
#include "DSP/Environment.h"
#include "Parameters.h"
#include <cmath>

namespace liminal {

namespace {
using ui::style::ink;
using ui::style::rec;
using ui::style::font;
using ui::style::nowMs;
using ui::style::segmentsFor;
} // namespace

Viewfinder::Viewfinder(juce::AudioProcessorValueTreeState& s, SceneMeters& m, std::atomic<float>& warble)
    : state(s), meters(m), placeStrip(s), characterStrip(s), warbleOut(warble) {
    meterSources = { &meters.ambience, &meters.instrument };
    setLookAndFeel(&look);
    setOpaque(true);
    addAndMakeVisible(placeStrip);
    addAndMakeVisible(characterStrip);
    placeStrip.onChoose = [this] { if (syncPlace()) repaint(); };
    placeStrip.onHover = [this](int place) {   // decode a hovered place ahead, so a click swaps it in at once
        hoverPlace = place;
        hoverTicks = 0;
    };
    characterStrip.onChoose = [this] { if (syncPlace()) repaint(); };
}

void Viewfinder::startViewfinder() {
    syncPlace();
    setSize(defaultWidth, defaultHeight);
    startTimerHz(30);
}

Viewfinder::~Viewfinder() {
    stopTimer();
    warbleOut.store(0.0f);   // nothing on screen: nothing glitches
    setLookAndFeel(nullptr);
}

void Viewfinder::resized() {
    characterStrip.setBounds(52, 88, 390, 20);
    auto strip = stripArea().reduced(32, 0);
    strip.removeFromTop(10);
    placeStrip.setBounds(strip.removeFromTop(52));
    ++pictureSerial;
    scene = {};
    sceneScale = 0;
}

// The place and Character the parameters ask for. The picture, caption, chips and lights follow
// together once the new scene is ready (advanceSwitch); only the very first one is taken at once.
bool Viewfinder::syncPlace() {
    placeStrip.refresh();
    targetPlace = std::clamp(int(std::lround(state.getRawParameterValue(params::environment)->load())), 0, dsp::placeCount - 1);
    targetCharacter = std::clamp(int(std::lround(state.getRawParameterValue(params::character)->load())), 0, dsp::characterCount - 1);
    if (placeShown >= 0) return false;
    placeShown = targetPlace;   // (nothing on screen yet to fade from: the thumbnail stands in until the scene decodes)
    characterShown = targetCharacter;
    grade = gradeFor(placeShown, characterShown);
    characterStrip.show(placeShown, characterShown);
    sceneCommitted();
    return true;
}

// The Character's grade (Milestone 4 adds its light and flicker to the painted scene).
juce::Colour Viewfinder::gradeFor(int place, int character) {
    const auto characterGrade = dsp::character(place, character).grade;
    return juce::Colour(characterGrade != 0 ? characterGrade : placeInfo[place].grade);
}

// Ease-out, on wall time, set once per tick so a frame's paint and its report agree.
float Viewfinder::fadeAlpha() const { return fadeStartMs >= 0 ? fadeNow : 1.0f; }

// The fade starts one frame in: the first frame after the swap already shows a fifth of the new
// scene, so the caption and chips never change over an unchanged picture.
void Viewfinder::startFade() {
    fadeStartMs = nowMs() - ui::FrameGovernor::tickMs;
    const float t = float(ui::FrameGovernor::tickMs / (fadeSeconds * 1000.0));
    fadeNow = 1.0f - (1.0f - t) * (1.0f - t);
}

// A private copy of what is on screen now (mid-fade: the blend), to fade out from. Nothing else
// writes to it - the compositor recycles its own pictures.
void Viewfinder::freezeCurrentFrame() {
    const bool photographic = photoPlace(std::max(0, placeShown));
    const juce::Image& now = !photographic ? scene.image : showingPhoto() ? photoImage : photoPlaceholder;
    const juce::Image& frost = !photographic ? scene.frosted : photoFrosted;
    if (!now.isValid()) {
        fromPicture = {};
        fromFrosted = {};
    } else if (fadeAlpha() < 1.0f && fromPicture.isValid()) {   // a switch during a fade: freeze the blend
        juce::Image blend(juce::Image::ARGB, now.getWidth(), now.getHeight(), false);
        juce::Graphics g(blend);
        g.fillAll(juce::Colours::black);
        g.drawImage(fromPicture, blend.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setOpacity(fadeAlpha());
        g.drawImage(now, blend.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        fromPicture = blend;
        fromFrosted = frost;
    } else {
        fromPicture = now.createCopy();
        fromFrosted = frost;   // (frosted pictures are made new each time, never written again)
    }
    fromPlace = placeShown;
    fromCharacter = characterShown;
}

// The frosted glass of the strip and the pages, from the photograph on screen.
void Viewfinder::makeFrost() {
    const auto small = photoImage.rescaled(std::max(1, photoImage.getWidth() / 16), std::max(1, photoImage.getHeight() / 16), juce::Graphics::mediumResamplingQuality);
    photoFrosted = juce::Image(juce::Image::ARGB, photoImage.getWidth() / 2, photoImage.getHeight() / 2, false);
    juce::Graphics g(photoFrosted);
    g.fillAll(juce::Colours::black);   // opaque (macOS keeps even RGB images as ARGB)
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(small, 0, 0, photoFrosted.getWidth(), photoFrosted.getHeight(), 0, 0, small.getWidth(), small.getHeight());
    g.fillAll(juce::Colour(0x8c0a0e0d));
    photoNeedsFrost = false;
}

// The swap, in one frame: what the paint shows next is the new scene (fading in over the frozen
// last frame), with the new caption, chips, grade and light levels.
void Viewfinder::commitSwitch(int place, int character, juce::RectangleList<int>& dirty) {
    if (place != placeShown) playUntil = clock + 3.5;   // a new place: the deck shows PLAY again for a moment
    placeShown = place;
    characterShown = character;
    grade = gradeFor(place, character);
    characterStrip.show(place, character);
    sceneCommitted();
    shownPulse = {};
    pulse = {};
    requestedPlace = requestedCharacter = -1;
    if (fromPicture.isValid() && !settle) startFade();
    else {
        fadeStartMs = -1;
        fromPicture = {};
        fromFrosted = {};
    }
    dirty.add(getLocalBounds());
}

// Prepares the scene the parameters ask for, off screen, and swaps it in when its first picture is
// complete: a photograph's passes decoded (or kept from before / decoded on hover) and composited
// with the target Character's light; a painted place drawn. Until then the old scene stays.
void Viewfinder::advanceSwitch(juce::RectangleList<int>& dirty) {
    if (!pendingSwitch()) return;
    const int place = targetPlace, character = targetCharacter;
    if (!photoPlace(place)) {   // a painted place: drawn now, on this thread
        freezeCurrentFrame();
        renderPainted(place, character, sceneScale > 0 ? sceneScale : 1.0f);
        if (photo != nullptr) spare = std::move(photo);
        ++pictureSerial;
        photoImage = {};
        photoFrosted = {};
        commitSwitch(place, character, dirty);
        return;
    }
    ui::PhotoScene* s = nullptr;
    if (photo != nullptr && photo->place() == place) s = photo.get();   // a new Character, same place
    else {
        if (incoming == nullptr || incoming->place() != place) {
            if (spare != nullptr && spare->place() == place) std::swap(incoming, spare);   // decoded ahead
            else {
                if (incoming != nullptr && incoming->loaded()) spare = std::move(incoming);
                incoming = std::make_unique<ui::PhotoScene>(place);
            }
            requestedPlace = -1;
        }
        s = incoming.get();
    }
    if (settle) s->ensureLoaded();
    if (s->failed()) {   // no photograph after all: the painted scene
        photoKnown[std::size_t(place)] = 0;
        if (s == incoming.get()) incoming.reset();
        advanceSwitch(dirty);
        return;
    }
    if (!s->loaded()) return;
    const auto install = [&](const juce::Image& picture) {
        if (s != photo.get()) {
            if (photo != nullptr) spare = std::move(photo);
            photo = std::move(incoming);
        }
        ++pictureSerial;
        photoImage = picture;
        photoSwell.assign(photo->passes().size(), 0.0f);
        photoLevelsSent = targetLevels;
        photoPhaseSent = targetPhase;
        photoCharacterSent = character;
        photoVhsSent = vhsStatic();
        makeFrost();
        commitSwitch(place, character, dirty);
    };
    if (requestedPlace != place || requestedCharacter != character) {
        auto look = photoLook(*s, place, character, false);
        look.serial = targetSerial = ++lookSerial;
        targetLevels = look.levels;
        targetPhase = look.causticPhase;
        requestedPlace = place;
        requestedCharacter = character;
        if (settle) {   // tests and snapshots: composited on the spot
            freezeCurrentFrame();
            install(s->renderNow(look));
            return;
        }
        s->request(look);
        return;
    }
    unsigned serial = 0;
    if (!s->peek(serial) || serial < targetSerial) return;   // not the target look yet
    freezeCurrentFrame();   // (before take(): the picture on screen may go back to the compositor)
    juce::Image next;
    if (s->take(next)) install(next);
}

void Viewfinder::mouseMove(const juce::MouseEvent& e) { mouse = e.position; }

// The lights answering the music: each pulse brightens a fixture, its halo and its
// reflection (drawn over the cached scene, only where something changed).
void Viewfinder::paintLights(juce::Graphics& g) {
    const auto count = std::min<std::size_t>(shownPulse.size(), scene.lights.size());
    for (std::size_t i = 0; i < count; ++i) {
        const float p = shownPulse[i];
        const auto& light = scene.lights[i];
        if (p > 0.005f) {
            g.setGradientFill(juce::ColourGradient(scene.lightColour.withAlpha(std::min(0.5f, 0.32f * p)), light.glow.getCentre(), scene.lightColour.withAlpha(0.0f),
                                                   light.glow.getCentre().translated(light.glow.getWidth() * 0.5f, 0), true));
            g.fillEllipse(light.glow);
            if (light.reflection.x >= 0) {
                const auto r = juce::Rectangle<float>(light.glow.getWidth() * 0.4f, light.glow.getHeight() * 0.9f).withCentre(light.reflection);
                g.setGradientFill(juce::ColourGradient(scene.lightColour.withAlpha(std::min(0.35f, 0.22f * p)), light.reflection, scene.lightColour.withAlpha(0.0f),
                                                       light.reflection.translated(0, r.getHeight() * 0.5f), true));
                g.fillEllipse(r);
            }
        } else if (p < -0.005f) {   // a failing tube: its glow dims
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0a0e0d).withAlpha(std::min(0.8f, -p)), light.glow.getCentre(), juce::Colour(0x000a0e0d),
                                                   light.glow.getCentre().translated(light.glow.getWidth() * 0.5f, 0), true));
            g.fillEllipse(light.glow);
        }
    }
}

void Viewfinder::paint(juce::Graphics& g) {
    paintStarted = nowMs();
    const float scale = std::max(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    // A photographic scene (light passes composited live) where there is one; else the drawing.
    const bool photographic = photoPlace(std::max(0, placeShown));
    const float fade = fadeAlpha();
    const bool fading = fade < 1.0f && fromPicture.isValid();
    if (photographic && placeholderFor != placeShown) {
        ++pictureSerial;
        photoPlaceholder = ui::PhotoScene::thumbnail(placeShown);
        placeholderFor = placeShown;
    }
    if (!photographic && (!scene.image.isValid() || std::abs(scale - sceneScale) > 0.01f || scene.place != placeShown || scene.character != characterShown || sceneVhs != vhsAmountNow))
        renderPainted(std::max(0, placeShown), std::max(0, characterShown), scale);
    const auto bounds = getLocalBounds().toFloat();
    if (photographic) scene.lights.clear();   // (the drawing's lights do not belong to the photograph)
    // Every repaint, however small its region, draws the whole scene under it: a base fill (so a
    // missing picture is never "nothing"), then the picture (opaque; clipped to the region).
    g.fillAll(juce::Colour(0xff0a0e0d));
    const juce::Image& picture = !photographic ? scene.image : showingPhoto() ? photoImage : photoPlaceholder;
    const juce::Image& frosted = !photographic ? scene.frosted : photoFrosted;
    lastPicture = picture;
    frameInfo = { fading ? fromPlace : -1, fading ? fromCharacter : -1, placeShown, characterShown, fading ? fade : 1.0f,
                  placeShown, characterShown, characterStrip.shownPlace(), characterStrip.shownCharacter(), photographic && !showingPhoto() };
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    if (fading) {   // the frozen last frame, the new scene over it
        g.drawImage(fromPicture, bounds, juce::RectanglePlacement::stretchToFit);
        g.setOpacity(fade);
    }
    const bool tape = vhsAmountNow > 0.0f;
    // With the tape look the whole viewfinder repaints at up to 24 fps: the picture is drawn from a
    // copy at screen resolution (made once per new picture), so a frame is a plain copy, not a resample.
    const juce::Image& shown = tape && !fading && picture.isValid() ? screenCopy(picture, scale) : picture;
    if (&shown == &picture) g.drawImage(picture, bounds.translated(tape && !fading ? vhsTape.jitter : 0.0f, 0.0f), juce::RectanglePlacement::stretchToFit);
    else g.drawImageTransformed(shown, juce::AffineTransform::scale(1.0f / scale).translated(vhsTape.jitter, 0.0f));
    g.setOpacity(1.0f);

    if (!photographic && !fading) paintLights(g);   // (a photograph's lights are its passes)
    else {   // a photograph's ceiling lights sit behind the viewfinder's captions: shade the top a little
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x66080b0a), 0, 0, juce::Colour(0x00080b0a), 0, 150, false));
        g.fillRect(bounds.withHeight(150));
    }
    const auto drawFrosted = [&](juce::Rectangle<int> area) {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(area);
        const auto glass = [&](const juce::Image& image) {
            if (image.isValid()) g.drawImage(image, bounds, juce::RectanglePlacement::stretchToFit);
            else g.fillAll(juce::Colour(0xe00a0e0d));
        };
        if (fading) {
            glass(fromFrosted);
            g.setOpacity(fade);
        }
        glass(frosted);
        g.setOpacity(1.0f);
    };
    // The VHS look's animated half: grain, dropouts, the head-switching strip, line wobble and the
    // sparse tracking rolls and tears (drawn over the cached picture; nothing is recomposited).
    if (tape) {
        vhsGrain.prepare(sceneArea().getWidth(), sceneArea().getHeight(), scale, vhsAmountNow);   // (made once per size and amount)
        ui::vhs::paint(g, fading ? juce::Image() : shown, bounds, sceneArea(), vhsTape, vhsAmountNow, vhsWrongNow, &vhsGrain);
    }
    if (!tape && clock < shimmerUntil) {   // Unreal (no VHS look): a band of the picture slips sideways, like worn tape
        const auto band = juce::Rectangle<float>(0, shimmerY, bounds.getWidth(), shimmerH);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(band.toNearestInt());
        g.drawImage(picture, bounds.translated(shimmerShift, 0), juce::RectanglePlacement::stretchToFit);
        g.setColour(ink.withAlpha(0.12f));
        g.fillRect(band.withHeight(1.0f).translated(0, shimmerH * 0.6f));
    }

    if (drawerOpen())   // frosted glass behind the drawer
        drawFrosted(drawerArea());

    // Frosted control strip.
    const auto strip = stripArea();
    drawFrosted(strip);
    g.setColour(ink.withAlpha(0.18f));
    g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1);
    g.setColour(ink.withAlpha(0.08f));
    g.fillRect(strip.getX() + 32, placeStrip.getBottom() + 4, strip.getWidth() - 64, 1);
    // The wordmark, quiet, at the top of the viewfinder.
    g.setColour(ink.withAlpha(0.5f));
    g.setFont(font(12.0f, true).withExtraKerningFactor(0.6f));
    g.drawText(brand::product, juce::Rectangle<int>(getWidth() / 2 - 110, 26, 220, 20), juce::Justification::centred);

    // Viewfinder frame corners and centre mark.
    const auto frame = getLocalBounds().withTrimmedBottom(strip.getHeight()).reduced(18).toFloat();
    g.setColour(ink.withAlpha(0.55f));
    const float l = 26.0f, t = 1.5f;
    for (const auto corner : { frame.getTopLeft(), frame.getTopRight(), frame.getBottomLeft(), frame.getBottomRight() }) {
        const float sx = corner.x < frame.getCentreX() ? 1.0f : -1.0f, sy = corner.y < frame.getCentreY() ? 1.0f : -1.0f;
        g.fillRect(juce::Rectangle<float>(corner, corner.translated(sx * l, sy * t)));
        g.fillRect(juce::Rectangle<float>(corner, corner.translated(sx * t, sy * l)));
    }
    const auto c = frame.getCentre();
    g.setColour(ink.withAlpha(0.22f));
    g.fillRect(c.x - 9, c.y - 0.5f, 18.0f, 1.0f);
    g.fillRect(c.x - 0.5f, c.y - 9, 1.0f, 18.0f);

    // REC and the location caption (with the VHS look: the camcorder's own blocky display).
    const auto recBox = recArea().toFloat();
    if (tape) paintOsd(g, scale);
    else {
        if (recOn) {
            g.setColour(rec);
            g.fillEllipse(juce::Rectangle<float>(10, 10).withCentre({ recBox.getX() + 9, recBox.getCentreY() }));
        }
        g.setColour(ink.withAlpha(0.9f));
        g.setFont(font(13.0f, true));
        g.drawText("REC", recBox.withTrimmedLeft(22), juce::Justification::centredLeft);
    }
    g.setColour(ink.withAlpha(0.6f));
    g.setFont(font(10.5f));
    const auto& where = dsp::profile(dsp::Place(std::clamp(placeShown, 0, dsp::placeCount - 1)));
    g.drawText(juce::String(where.name) + "  /  " + dsp::character(placeShown, std::max(0, characterShown)).name + "  /  " + where.location,
               captionArea().toFloat(), juce::Justification::centredLeft);
    // Evolution: a quiet row of ticks, lit by how much the place is changing right now.
    {
        auto box = activityArea().toFloat();
        g.setColour(ink.withAlpha(0.4f));
        g.setFont(font(8.5f));
        g.drawText("EVOLVING", box.removeFromLeft(62), juce::Justification::centredLeft);
        for (int i = 0; i < 12; ++i) {
            g.setColour(ink.withAlpha(i < activityLit ? 0.65f : 0.15f));
            g.fillRect(box.getX() + float(i) * 6.0f, box.getCentreY() - 2.0f, 3.0f, 4.0f);
        }
    }

    // Time stamp and battery-style meters.
    if (!tape) {
        g.setColour(ink.withAlpha(0.9f));
        g.setFont(font(13.0f));
        g.drawText(clockText, clockArea(), juce::Justification::centredRight);
    }
    auto meters = meterArea().toFloat();
    for (const auto& [name, segments] : { std::pair<const char*, int> { meterNames[0], ambienceSegments }, { meterNames[1], instrumentSegments } }) {
        auto box = meters.removeFromRight(46).withSizeKeepingCentre(40, 12);
        g.setColour(ink.withAlpha(0.7f));
        g.drawRect(box, 1.0f);
        g.fillRect(box.getRight(), box.getCentreY() - 2.5f, 2.5f, 5.0f);
        auto inner = box.reduced(2.5f);
        const float seg = inner.getWidth() / 6.0f;
        for (int i = 0; i < segments; ++i) g.fillRect(inner.getX() + float(i) * seg + 0.5f, inner.getY(), seg - 1.0f, inner.getHeight());
        g.setFont(font(10.0f));
        g.setColour(ink.withAlpha(0.6f));
        g.drawText(name, meters.removeFromRight(40), juce::Justification::centredRight);
        meters.removeFromRight(14);
    }
}

// What the composite bakes in: the amount, the colour cast in small steps (a new composite every
// few seconds as it drifts, not every frame) and how wrong it is (in fifths).
ui::vhs::Static Viewfinder::vhsStatic() const {
    if (vhsAmountNow <= 0.0f) return {};
    return { vhsAmountNow, std::round(vhsTape.drift * 10.0f) / 10.0f, std::round(vhsWrongNow * 5.0f) / 5.0f };
}

// A painted place (or the developer's own photograph), with the tape look baked in.
void Viewfinder::renderPainted(int place, int character, float scale) {
    ++pictureSerial;
    scene = ui::renderPlace(place, character, getWidth(), getHeight(), scale);
    sceneScale = scale;
    sceneVhs = vhsAmountNow;
    if (vhsAmountNow > 0.0f) ui::vhs::bakeImage(scene.image, { vhsAmountNow, 0.0f, std::round(vhsWrongNow * 5.0f) / 5.0f });
}

const juce::Image& Viewfinder::osdImage(int slot, const juce::String& text, juce::Colour colour, float scale) {
    auto& o = osd[std::size_t(slot)];
    if (o.text != text || std::abs(o.scale - scale) > 0.01f || o.colour != colour.getARGB() || !o.image.isValid()) {
        o.text = text;
        o.scale = scale;
        o.colour = colour.getARGB();
        o.image = ui::vhs::osdText(text, colour, 2.0f, scale);
    }
    return o.image;
}

// The camcorder's display, burned into the picture: REC (blinking dot), the tape speed, the deck's
// PLAY for a few seconds, the date and time. Blocky, glowing a little, slightly off the frame's
// grid, and it jumps with the picture when the tape jitters. Cached per text (the clock: once a
// second).
void Viewfinder::paintOsd(juce::Graphics& g, float scale) {
    const float px = 2.0f, margin = 2.0f * px;
    const float jx = vhsTape.jitter + 1.0f, top = 30.5f;
    const juce::Colour white(0xfff4f2ea);
    g.setOpacity(1.0f);   // (images are drawn with the current colour's alpha)
    const auto put = [&](const juce::Image& image, float x) {
        g.drawImage(image, juce::Rectangle<float>(x - margin, top - margin, float(image.getWidth()) / scale, float(image.getHeight()) / scale), juce::RectanglePlacement::stretchToFit);
    };
    if (recOn) put(osdImage(0, "*", rec, scale), 31.0f + jx);
    put(osdImage(1, "REC", white, scale), 47.0f + jx);
    put(osdImage(2, "SP", white.withAlpha(0.85f), scale), 112.0f + jx);
    if (clock < playUntil) put(osdImage(3, "PLAY >", white, scale), 586.0f + jx);
    if (osdClock.isEmpty()) osdClock = juce::Time::getCurrentTime().formatted("%b.%d %Y  %H:%M:%S").toUpperCase();
    put(osdImage(4, osdClock, white, scale), float(clockArea().getRight()) - ui::vhs::osdWidth(osdClock, px) + jx);
}

const juce::Image& Viewfinder::screenCopy(const juce::Image& picture, float scale) {
    const int pw = juce::roundToInt(float(getWidth()) * scale), ph = juce::roundToInt(float(getHeight()) * scale);
    if (screenSerial != pictureSerial || screenSource != &picture || screenPicture.getWidth() != pw || screenPicture.getHeight() != ph) {
        if (screenPicture.getWidth() != pw || screenPicture.getHeight() != ph) screenPicture = juce::Image(juce::Image::ARGB, pw, ph, false);
        if (!ui::vhs::resize(picture, screenPicture)) {   // (a painted scene is RGB: through Graphics)
            juce::Graphics g(screenPicture);
            g.fillAll(juce::Colour(0xff0a0e0d));
            g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
            g.drawImage(picture, juce::Rectangle<float>(0, 0, float(pw), float(ph)), juce::RectanglePlacement::stretchToFit);
        }
        screenSerial = pictureSerial;
        screenSource = &picture;
    }
    return screenPicture;
}

void Viewfinder::settlePicture() {
    if (photo != nullptr && photo->place() == placeShown && photo->ensureLoaded()) {
        const auto look = photoLook(*photo, std::max(0, placeShown), std::max(0, characterShown), false);
        ++pictureSerial;
        photoImage = photo->renderNow(look);
        photoLevelsSent = look.levels;
        photoPhaseSent = look.causticPhase;
        photoCharacterSent = characterShown;
        photoVhsSent = look.vhs;
        makeFrost();
    }
    repaint();
}

bool Viewfinder::photoPlace(int place) {
    auto& known = photoKnown[std::size_t(std::clamp(place, 0, 9))];
    if (known < 0) known = ui::PhotoScene::available(place) ? 1 : 0;
    return known == 1;
}

// A photograph's look under a place's Character: every light group's level (the Character's light
// from the manifest; the failing tube following the hum), the grade, the caustics' phase. `live`
// adds what the music and the mouse do to the lights on screen (the swells).
ui::PhotoScene::Look Viewfinder::photoLook(const ui::PhotoScene& s, int place, int character, bool live) const {
    auto look = s.defaultLook();
    const auto& passes = s.passes();
    for (std::size_t i = 0; i < passes.size(); ++i) {
        float level = passes[i].defaultLevel * passes[i].level(character);   // how the Character lights this group
        if (passes[i].flickers(character)) level *= 0.02f + 0.98f * photoLamp * photoLamp * photoLamp;
        if (live && i < photoSwell.size()) {
            level *= 1.0f + photoSwell[i];
            const auto a = passes[i].anchor;   // the light under the mouse warms a little
            if (a.x >= 0 && mouse.getDistanceFrom({ a.x * float(getWidth()), a.y * float(getHeight()) }) < 90.0f) level *= 1.08f;
        }
        look.levels[i] = std::max(0.0f, level);
    }
    look.light = dsp::character(place, character).light;
    look.grade = gradeFor(place, character).getARGB();
    look.character = character;
    look.vhs = vhsStatic();
    look.causticLevel *= s.causticCharacter(character);
    if (s.hasCaustics() && !lightsStill) {   // two steps per caustic frame: smooth, and no composite every tick
        const double steps = 2.0 * double(s.causticFrames());
        look.causticPhase = float(std::floor(std::fmod(clock * double(s.causticFps()) / double(s.causticFrames()), 1.0) * steps) / steps);
    } else look.causticPhase = photoPhaseSent < 0 ? 0.0f : photoPhaseSent;
    return look;
}

// The photographic scene on screen, one frame: the lights answer the music (notes swell the
// nearest bank), the place's events, the failing tube (the hum's stutter, from the audio thread)
// and the mouse; the caustics drift on their loop. The compositing runs on the scene's own
// thread; this only asks for a look and picks up finished pictures. Changes of place or
// Character are advanceSwitch's: while one is being prepared in this same scene, it waits.
void Viewfinder::animatePhoto(bool still, juce::RectangleList<int>& dirty) {
    const int place = std::max(0, placeShown), character = std::max(0, characterShown);
    if (!photoPlace(place)) {
        if (photo != nullptr) {
            spare = std::move(photo);
            ++pictureSerial;
            photoImage = {};
            photoFrosted = {};
            dirty.add(getLocalBounds());
        }
        return;
    }
    if (photo == nullptr || photo->place() != place) {   // (the first place: nothing was on screen)
        photo = incoming != nullptr && incoming->place() == place ? std::move(incoming) : std::make_unique<ui::PhotoScene>(place);
        ++pictureSerial;
        photoImage = {};
        photoFrosted = {};
        photoLevelsSent.clear();
        photoSwell.clear();
        photoCharacterSent = -1;
    }
    if (pendingSwitch() && targetPlace == place) return;   // a new Character is being composited in this scene
    const bool settleNow = settle && !photoImage.isValid();
    if (settleNow && photo->ensureLoaded()) {   // tests and snapshots: the first picture, composited now
        ++pictureSerial;
        photoImage = photo->renderNow(photoLook(*photo, place, character, false));
        photoNeedsFrost = true;
    }
    if (!photo->loaded()) return;
    const auto& passes = photo->passes();
    if (photoSwell.size() != passes.size()) photoSwell.assign(passes.size(), 0.0f);
    const int notes = meters.notes.load(), events = meters.events.load();
    // The failing tube follows the hum: the darkest it got since the last frame.
    const float lamp = meters.lamp.exchange(1.0f);
    photoLamp = lamp < photoLamp ? lamp : photoLamp + 0.6f * (lamp - photoLamp);
    if (!still) {
        if (notes != photoNotesSeen) {   // the nearest bank of lights swells with the note
            for (std::size_t i = 0; i < passes.size(); ++i)
                if (passes[i].kind != "base" && passes[i].kind != "failing" && passes[i].level(character) > 0.05f) {
                    photoSwell[i] = std::max(photoSwell[i], 0.08f + 0.22f * meters.velocity.load());
                    break;
                }
        }
        if (events != photoEventsSeen && passes.size() > 1)   // the place's own sounds: a light wavers
            photoSwell[std::size_t(1 + random.nextInt(int(passes.size()) - 1))] -= 0.18f;
    }
    photoNotesSeen = notes;
    photoEventsSeen = events;
    const auto look = photoLook(*photo, place, character, !still);
    for (auto& swell : photoSwell) {
        swell *= 0.88f;
        if (std::abs(swell) < 0.01f) swell = 0.0f;   // (a 1 % swell is invisible: stop compositing for it)
    }
    bool changed = character != photoCharacterSent || photoLevelsSent.size() != look.levels.size() || std::abs(look.causticPhase - photoPhaseSent) > 1.0e-4f
                   || !(look.vhs == photoVhsSent);   // the tape look (its colour cast drifts in small steps)
    // Only a light whose intensity really moved (over 2 % of its level, about the smallest step the
    // eye sees on a whole light group) asks for a new picture.
    // With the VHS look's soft picture, 4 % (each picture there also costs a copy to screen size).
    const float step = vhsAmountNow > 0.0f ? 0.04f : 0.02f;
    for (std::size_t i = 0; !changed && i < look.levels.size(); ++i)
        changed = std::abs(look.levels[i] - photoLevelsSent[i]) > std::max(0.004f, step * std::max(look.levels[i], photoLevelsSent[i]));
    if (changed && !settleNow) photo->request(look);
    if (changed || settleNow) {
        photoLevelsSent = look.levels;
        photoPhaseSent = look.causticPhase;
        photoCharacterSent = character;
        photoVhsSent = look.vhs;
    }
    juce::Image next;
    const bool first = !photoImage.isValid();
    if (photo->take(next)) {
        if (first && photoPlaceholder.isValid() && placeholderFor == place && !settle) {   // the thumbnail that stood in fades out
            fromPicture = photoPlaceholder;
            fromFrosted = {};
            fromPlace = place;
            fromCharacter = character;
            startFade();
        }
        ++pictureSerial;
        photoImage = next;
        photoNeedsFrost = photoNeedsFrost || first;
        dirty.add(getLocalBounds().withBottom(stripArea().getY()));
    }
    if (photoNeedsFrost && photoImage.isValid()) {
        makeFrost();
        dirty.add(getLocalBounds());
    }
}

void Viewfinder::paintOverChildren(juce::Graphics&) {
    const double now = nowMs();
    frames.painted(now - paintStarted, now);
}

void Viewfinder::timerCallback() {
    double elapsed = 0;
    const double now = nowMs();
    if (!frames.beginFrame(now, elapsed)) return;
    clock += elapsed;
    if (fadeStartMs >= 0) {   // the cross-fade's progress this frame
        const float t = float(std::clamp((now - fadeStartMs) / (fadeSeconds * 1000.0), 0.0, 1.0));
        fadeNow = 1.0f - (1.0f - t) * (1.0f - t);
        if (t >= 1.0f) {   // over: the new scene alone (one last full repaint)
            fadeStartMs = -1;
            fromPicture = {};
            fromFrosted = {};
            repaint();
        }
    }
    // Slow frames for two seconds: stop the light's flicker (the cheapest animation to lose).
    const bool reduced = frames.slowFor(now);
    juce::RectangleList<int> dirty;

    const bool nextRec = std::fmod(clock, 1.2) < 0.75;
    if (nextRec != recOn) {
        recOn = nextRec;
        dirty.add(recArea());
    }
    const auto text = juce::Time::getCurrentTime().formatted("%b %d %Y   %H:%M:%S").toUpperCase();
    if (text != clockText) {
        clockText = text;
        osdClock = juce::Time::getCurrentTime().formatted("%b.%d %Y  %H:%M:%S").toUpperCase();
        dirty.add(clockArea());
    }
    // Meters fall slowly, rise at once.
    instrumentShown = std::max(meterSources[1]->load(), instrumentShown * 0.82f);
    ambienceShown = std::max(meterSources[0]->load(), ambienceShown * 0.82f);
    const int inst = segmentsFor(instrumentShown), amb = segmentsFor(ambienceShown);
    if (inst != instrumentSegments || amb != ambienceSegments) {
        instrumentSegments = inst;
        ambienceSegments = amb;
        dirty.add(meterArea());
    }
    // The lights: notes make them swell, the place's own sounds make one flicker, the
    // Character decides how unreliable the tubes are, the mouse warms the nearest one.
    // Eco quality (or slow frames) keeps them still.
    const bool eco = std::lround(state.getRawParameterValue(params::quality)->load()) == 0;
    const int notes = meters.notes.load(), events = meters.events.load();
    const auto lightCount = std::min<std::size_t>(pulse.size(), scene.lights.size());
    if (!reduced && !eco && lightCount > 0) {
        if (notes != notesSeen)
            for (std::size_t i = 0; i < lightCount; ++i) pulse[i] = std::max(pulse[i], 0.9f - 0.06f * float(i));
        if (events != eventsSeen) pulse[std::size_t(random.nextInt(int(lightCount)))] = 0.7f;
        const float flicker = dsp::character(std::max(0, placeShown), std::max(0, characterShown)).flicker;
        if (clock >= nextFlicker) {
            nextFlicker = clock + (1.0 + 8.0 * random.nextDouble()) / (0.3 + 1.5 * flicker);
            pulse[std::size_t(random.nextInt(int(lightCount)))] = -0.85f;
        }
        for (std::size_t i = 0; i < lightCount; ++i) {
            float target = pulse[i];
            const auto& glow = scene.lights[i].glow;
            if (glow.getWidth() < 400.0f && glow.contains(mouse)) target = std::max(target, 0.25f);
            if (std::abs(target - shownPulse[i]) > 0.01f) {
                shownPulse[i] = target;
                dirty.add(glow.getSmallestIntegerContainer().expanded(2));
                if (scene.lights[i].reflection.x >= 0)
                    dirty.add(juce::Rectangle<float>(glow.getWidth() * 0.4f, glow.getHeight() * 0.9f).withCentre(scene.lights[i].reflection).getSmallestIntegerContainer().expanded(2));
            }
            pulse[i] *= pulse[i] < 0.0f ? 0.7f : 0.86f;   // a flicker is short; a swell fades
            if (std::abs(pulse[i]) < 0.01f) pulse[i] = 0.0f;
        }
        // Unreal: now and then the picture slips like worn tape.
        const float u = state.getRawParameterValue(params::unreal)->load();
        if (u > 0.35f && clock >= nextShimmer && vhsAmountNow <= 0.0f) {
            shimmerY = float(40 + random.nextInt(std::max(1, stripArea().getY() - 80)));
            shimmerH = 6.0f + 18.0f * random.nextFloat();
            shimmerShift = (random.nextBool() ? 1.0f : -1.0f) * (4.0f + 10.0f * u);
            shimmerUntil = clock + 0.12 + 0.15 * random.nextDouble();
            nextShimmer = clock + (2.0 + 6.0 * random.nextDouble()) / u;
            dirty.add(juce::Rectangle<int>(0, int(shimmerY) - 1, getWidth(), int(shimmerH) + 2));
        } else if (shimmerUntil > 0 && clock >= shimmerUntil) {
            shimmerUntil = 0;
            dirty.add(juce::Rectangle<int>(0, int(shimmerY) - 1, getWidth(), int(shimmerH) + 2));
        }
    } else if (std::any_of(shownPulse.begin(), shownPulse.end(), [](float v) { return v != 0.0f; })) {
        shownPulse = {};
        pulse = {};
        dirty.add(getLocalBounds().withBottom(stripArea().getY()));
    }
    notesSeen = notes;
    eventsSeen = events;
    // A place hovered in the film strip for ~130 ms is decoded ahead (kept as the spare scene).
    if (hoverPlace >= 0 && ++hoverTicks == 4 && hoverPlace != placeShown && photoPlace(hoverPlace) && (incoming == nullptr || incoming->place() != hoverPlace)
        && (spare == nullptr || spare->place() != hoverPlace))
        spare = std::make_unique<ui::PhotoScene>(hoverPlace);
    lightsStill = reduced || eco;
    {   // The VHS look: the tape moves on (its own <= 24 fps clock); a new tape frame repaints the
        // viewfinder. Unreal and the "wrong" Characters make it struggle more.
        const int level = std::clamp(int(std::lround(state.getRawParameterValue(params::vhs)->load())), 0, 2);
        if (level != vhsLevel) {
            vhsLevel = level;
            vhsAmountNow = ui::vhs::amountOf(level);
            photoNeedsFrost = true;
            dirty.add(getLocalBounds());
        }
        const float unrealNow = state.getRawParameterValue(params::unreal)->load();
        vhsWrongNow = ui::vhs::wrongOf(unrealNow, ui::vhs::wrongness(std::max(0, placeShown), std::max(0, characterShown)));
        if (vhsAmountNow > 0.0f && vhsTape.advance(elapsed, vhsAmountNow, vhsWrongNow, reduced || eco ? 12.0 : 24.0, fadeStartMs >= 0))
            dirty.add(sceneArea());
        warbleOut.store(vhsAmountNow > 0.0f ? vhsTape.warble : 0.0f, std::memory_order_relaxed);
    }
    syncPlace();
    advanceSwitch(dirty);
    animatePhoto(reduced || eco, dirty);
    if (fadeStartMs >= 0) dirty.add(getLocalBounds());   // the cross-fade: the whole picture and its frosted glass, every frame
    {   // the evolution indicator: at most a few repaints a second of a tiny area
        const int lit = std::clamp(int(std::lround(meters.activity.load() * 12.0f)), 0, 12);
        if (lit != activityLit) {
            activityLit = lit;
            dirty.add(activityArea());
        }
    }
    placeStrip.refresh();
    characterStrip.refresh();
    refreshControls();   // the plug-in's own controls follow the parameters
    lastDirty = dirty;
    if (dirty.isEmpty()) {
        frames.painted(0.0, now);   // nothing to draw: the frame is done
        return;
    }
    for (const auto& r : dirty) repaint(r);
}

} // namespace liminal
