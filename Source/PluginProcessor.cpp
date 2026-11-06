#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

namespace liminal {

LiminalProcessor::LiminalProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "LIMINAL", params::makeLayout()) {   // (the state's tag: kept, so LIMINAL's songs load)
    samples::Bank::get().load();   // the developer's recordings (Milestone 8), on a background thread
    // The mirror (Milestone 5): every parameter the DSP reads, live or morphed between scenes.
    {
        const auto& all = getParameters();
        mirror = std::make_unique<std::atomic<float>[]>(std::size_t(all.size()));
        for (auto& sc : scenes) sc = std::make_unique<std::atomic<float>[]>(std::size_t(all.size()));
        for (auto* p : all) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
            const auto id = ranged->getParameterID();
            const bool discrete = dynamic_cast<juce::AudioParameterBool*>(p) != nullptr || dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr
                                  || dynamic_cast<juce::AudioParameterInt*>(p) != nullptr;
            int role = 0;   // 0 morphed, 1 an instrument's On, 2 an instrument's Level, 3 never morphed
            if (id == params::power || id == params::sceneOn || id == params::sceneMorph) role = 3;
            for (const char* key : params::instrumentKeys) {
                if (id == juce::String(key) + "_on") role = 1;
                if (id == juce::String(key) + "_level") role = 2;
            }
            mirrored.push_back({ ranged, parameters.getRawParameterValue(id), discrete, role, -1 });
        }
        for (std::size_t i = 0; i < mirrored.size(); ++i) {
            mirror[i].store(mirrored[i].raw->load());
            if (mirrored[i].role == 2) {   // a Level morphs with its instrument's On
                const auto onId = mirrored[i].parameter->getParameterID().upToLastOccurrenceOf("_", false, false) + "_on";
                for (std::size_t k = 0; k < mirrored.size(); ++k)
                    if (mirrored[k].parameter->getParameterID() == onId) mirrored[i].partner = int(k);
            }
        }
    }
    power = parameters.getRawParameterValue(params::power);
    master = mirrorOf(params::master);
    instrumentLevel = mirrorOf(params::instrument);
    ambienceLevel = mirrorOf(params::ambience);
    space = mirrorOf(params::space);
    const auto raw = [this](const char* id) { return mirrorOf(id); };
    const char* onIds[] { params::pianoOn, params::synthOn, params::glassOn, params::choirOn, params::boxOn, params::droneOn };
    const char* levelIds[] { params::pianoLevel, params::synthLevel, params::glassLevel, params::choirLevel, params::boxLevel, params::droneLevel };
    for (int i = 0; i < instrumentCount; ++i) {
        auto& slot = slots[std::size_t(i)];
        slot.on = raw(onIds[i]);
        slot.level = raw(levelIds[i]);
        slot.type = mirrorOf(params::slotTypeId(i));
        for (int k = 0; k < params::lib::count; ++k) slot.libParams[std::size_t(k)] = mirrorOf(params::libId(i, params::lib::keys[k]));
        slot.playing = i;   // its own instrument (the type's default)
        slot.instrument = slot.kit->original(i);
    }
    slots[0].distance = raw(params::pianoDistance);
    slots[2].distance = raw(params::glassDistance);
    slots[4].distance = raw(params::boxDistance);
    const char* pianoIds[] { params::pianoAge, params::pianoDetune, params::pianoSoftness, params::pianoNoise, params::pianoDecay };
    for (int i = 0; i < 5; ++i) pianoSettings[i] = raw(pianoIds[i]);
    const char* synthIds[] { params::synthDetune, params::synthWarmth, params::synthDrift, params::synthAttack,
                             params::synthRelease, params::synthWidth, params::synthGlide, params::synthGlideTime,
                             params::synthPatch, params::synthBlend };
    for (int i = 0; i < 10; ++i) synthSettings[i] = raw(synthIds[i]);
    const char* glassIds[] { params::glassFragility, params::glassBrightness, params::glassResonance, params::glassDetune, params::glassDecay };
    for (int i = 0; i < 5; ++i) glassSettings[i] = raw(glassIds[i]);
    const char* choirIds[] { params::choirBreath, params::choirFormant, params::choirDetune, params::choirWidth, params::choirDarkness, params::choirRelease };
    for (int i = 0; i < 6; ++i) choirSettings[i] = raw(choirIds[i]);
    const char* boxIds[] { params::boxWear, params::boxDrift, params::boxMechanism, params::boxBrightness, params::boxDecay };
    for (int i = 0; i < 5; ++i) boxSettings[i] = raw(boxIds[i]);
    const char* droneIds[] { params::droneDensity, params::droneInstability, params::droneHarmonics, params::droneSub,
                             params::droneMovement, params::droneRelease, params::droneAttack };
    for (int i = 0; i < 7; ++i) droneSettings[i] = raw(droneIds[i]);
    for (int i = 0; i < instrumentCount; ++i) {
        auto& slot = slots[std::size_t(i)];
        for (int k = 0; k < params::fx::count; ++k) slot.fx[std::size_t(k)] = mirrorOf(params::fxId(i, params::fx::all[k]));
        slot.keyLow = mirrorOf(params::midiId(i, params::midi::keyLow));
        slot.keyHigh = mirrorOf(params::midiId(i, params::midi::keyHigh));
        slot.velocityVolume = mirrorOf(params::midiId(i, params::midi::velocityVolume));
        slot.velocityFilter = mirrorOf(params::midiId(i, params::midi::velocityFilter));
        for (int k = 0; k < dsp::lim::count; ++k) slot.limParams[std::size_t(k)] = mirrorOf(params::limId(i, dsp::lim::keys[k]));
    }
    liminalityMacro = mirrorOf(params::liminality);
    qualityChoice = mirrorOf(params::quality);
    soloParam = raw(params::solo);
    environmentChoice = raw(params::environment);
    environmentAmount = raw(params::environmentAmount);
    farAway = raw(params::farAway);
    warbleOn = raw(params::vhsWarble);
    characterChoice = raw(params::character);
    evolutionRate = raw(params::evolution);
    evolutionFreeze = raw(params::evolutionFreeze);
    evolutionSeed = raw(params::evolutionSeed);
    macroParams[0] = raw(params::presence);
    macroParams[1] = raw(params::scale);
    macroParams[2] = raw(params::activity);
    macroParams[3] = raw(params::unreal);
    sceneOn = parameters.getRawParameterValue(params::sceneOn);
    sceneMorph = parameters.getRawParameterValue(params::sceneMorph);
}

std::atomic<float>* LiminalProcessor::mirrorOf(const juce::String& id) {
    for (std::size_t i = 0; i < mirrored.size(); ++i)
        if (mirrored[i].parameter->getParameterID() == id) return &mirror[i];
    jassertfalse;
    return parameters.getRawParameterValue(id);
}

// Once per block: the values the DSP will use. With Scene Morph off, the live parameters.
// With it on, the morph from A to B: continuous parameters interpolate (in their normalised
// range), discrete ones (place, Character, switches, choices) switch at the half-way point
// (the place cross-fades; switches fade), and an instrument present in only one scene fades
// in or out through its level instead of switching. An unstored scene is the live settings.
void LiminalProcessor::updateMirror() {
    const bool morphing = sceneOn->load() > 0.5f;
    const float t = std::clamp(sceneMorph->load(), 0.0f, 1.0f);
    const auto sceneValue = [&](int which, std::size_t i) {
        return stored[std::size_t(which)].load() ? scenes[std::size_t(which)][i].load() : mirrored[i].parameter->getValue();
    };
    for (std::size_t i = 0; i < mirrored.size(); ++i) {
        const auto& m = mirrored[i];
        if (!morphing || m.role == 3) {
            mirror[i].store(m.raw->load(), std::memory_order_relaxed);
            continue;
        }
        const float a = sceneValue(0, i), b = sceneValue(1, i);
        float value;
        if (m.role == 1) {
            value = a > 0.5f || b > 0.5f ? 1.0f : 0.0f;
        } else if (m.role == 2 && m.partner >= 0) {
            const bool onA = sceneValue(0, std::size_t(m.partner)) > 0.5f, onB = sceneValue(1, std::size_t(m.partner)) > 0.5f;
            const float dbA = onA ? m.parameter->convertFrom0to1(a) : params::minDb, dbB = onB ? m.parameter->convertFrom0to1(b) : params::minDb;
            value = dbA + t * (dbB - dbA);
        } else if (m.discrete) {
            value = m.parameter->convertFrom0to1(t < 0.5f ? a : b);
        } else {
            value = m.parameter->convertFrom0to1(a + t * (b - a));
        }
        mirror[i].store(value, std::memory_order_relaxed);
    }
}

void LiminalProcessor::storeScene(int which) {
    const auto w = std::size_t(which & 1);
    for (std::size_t i = 0; i < mirrored.size(); ++i) scenes[w][i].store(mirrored[i].parameter->getValue());
    stored[w].store(true);
}

int LiminalProcessor::selectedCharacter() const { return std::clamp(int(std::lround(characterChoice->load())), 0, dsp::characterCount - 1); }

dsp::EffectSettings LiminalProcessor::effectSettings(const Slot& slot) const {
    // The pointers are in params::fx::all order.
    const auto v = [&](int k) { return slot.fx[std::size_t(k)]->load(); };
    const auto b = [&](int k) { return v(k) > 0.5f; };
    dsp::EffectSettings s;
    s.mix = v(0); s.influence = v(1); s.order = int(std::lround(v(2)));
    s.driftOn = b(3); s.drift = v(4); s.driftRate = v(5);
    s.tapeOn = b(6); s.tape = v(7); s.tapeTone = v(8); s.tapeHiss = v(9); s.tapeDropouts = v(10);
    s.reverbOn = b(11); s.reverb = v(12);
    s.echoOn = b(13); s.echo = v(14); s.echoMs = v(15); s.echoSync = int(std::lround(v(16))); s.echoFeedback = v(17); s.echoTone = v(18);
    s.chorusOn = b(19); s.chorus = v(20); s.chorusRate = v(21);
    s.filterOn = b(22); s.filter = v(23); s.filterLow = v(24); s.filterHigh = v(25); s.filterRes = v(26);
    s.grainOn = b(27); s.grain = v(28); s.grainSize = v(29); s.grainSpread = v(30); s.grainFreeze = b(31);
    s.velocityFilter = slot.velocityFilter->load();
    return s;
}

// The room's level match for what the slot plays (linear), riding on how much the place shapes it.
float LiminalProcessor::levelTrim(const Slot& slot, int place, int character) const {
    // fx[0]: Effects Mix (0 = the dry instrument), fx[1]: Environment Influence
    const float amount = std::clamp(environmentAmount->load(), 0.0f, 1.0f) * std::clamp(slot.fx[1]->load(), 0.0f, 1.0f) * std::clamp(slot.fx[0]->load(), 0.0f, 1.0f);
    const float db = dsp::levelmatch::trimDb(slot.playing, place, character, amount);
    return std::pow(10.0f, db / 20.0f);
}

dsp::Place LiminalProcessor::selectedPlace() const {
    return dsp::Place(std::clamp(int(std::lround(environmentChoice->load())), 0, dsp::placeCount - 1));
}

// A soloed instrument that is on mutes every other one; soloing one that is off does nothing.
void LiminalProcessor::updateSolo() {
    const int solo = int(std::lround(soloParam->load()));
    const bool valid = solo >= 1 && solo <= instrumentCount && slots[std::size_t(solo - 1)].enabled();
    for (int i = 0; i < instrumentCount; ++i) slots[std::size_t(i)].muted = valid && i != solo - 1;
}

// The instrument library (2026-11-02): the slot plays another of the twenty. The one it played lets go of
// every note at once (a short fade, never a cut) and keeps rendering until it is silent; held-back
// Liminality notes are dropped. Everything after the voices (Liminality, Distance, effects, room) is the
// slot's and carries on. Audio thread (also prepareToPlay); no allocation.
void LiminalProcessor::selectInstrument(Slot& slot, int which) {
    which = std::clamp(which, 0, params::libraryCount - 1);
    if (which == slot.playing && slot.instrument != nullptr) return;
    auto& kit = *slot.kit;
    const auto forget = [&](const dsp::Instrument* in) {   // no longer fading (it plays again, or is cut)
        for (auto& f : slot.fading)
            if (f.instrument == in) f = {};
    };
    dsp::Instrument* next = nullptr;
    if (dsp::library::isOriginal(which)) {
        next = kit.original(which);
    } else {
        const int kind = which - dsp::library::originals;
        dsp::Library* pick = nullptr;
        for (auto& l : kit.library)   // the same instrument still fading out: it simply plays on
            if (&l != slot.instrument && slot.isFading(&l) && l.kind() == kind) pick = &l;
        if (pick == nullptr)
            for (auto& l : kit.library)
                if (&l != slot.instrument && !slot.isFading(&l)) { pick = &l; break; }
        if (pick == nullptr) {   // every instance busy (three changes within a second): the oldest fade is cut short
            auto& oldest = slot.fading[0].instrument != nullptr ? slot.fading[0] : slot.fading[1];
            pick = static_cast<dsp::Library*>(oldest.instrument);
            pick->reset();
            oldest = {};
        }
        pick->setKind(kind);
        next = pick;
    }
    const bool wasFading = slot.isFading(next);
    forget(next);
    if (slot.instrument != nullptr && slot.instrument != next) {
        slot.lim.flush(*slot.instrument);
        slot.instrument->allSoundOff();
        Slot::Fading* free = nullptr;
        for (auto& f : slot.fading)
            if (f.instrument == nullptr) { free = &f; break; }
        if (free == nullptr) {   // both busy: cut the older one short
            slot.fading[0].instrument->reset();
            slot.fading[0] = slot.fading[1];
            free = &slot.fading[1];
        }
        *free = { slot.instrument, -1, 1 };
    }
    if (next != slot.instrument) {
        if (!wasFading) next->reset();   // an idle instrument starts clean (one still fading plays on)
        next->setQuality(std::clamp(qualityNow, 0, 2));
        next->setLiminalPitch(slot.instrument != nullptr ? slot.instrument->liminalPitch() : dsp::LiminalPitch {});
    }
    slot.instrument = next;
    slot.playing = which;
}

bool LiminalProcessor::slotRecorded(int slot) const {
    const auto& s = slots[std::size_t(std::clamp(slot, 0, instrumentCount - 1))];
    if (auto* lib = dynamic_cast<const dsp::Library*>(s.instrument)) return lib->recordedNow();
    return samples::Bank::get().ready() && s.playing != 2;   // the originals: all but Glass Memories play recordings when there are any
}

bool LiminalProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void LiminalProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    preparedRate = sampleRate;
    // The recordings load in about a second; waiting here means the first notes are recorded ones.
    samples::Bank::get().waitUntilLoaded(5000);
    updateMirror();
    updateSettings();
    warble.prepare(sampleRate);
    qualityNow = -1;
    updateSolo();
    for (int i = 0; i < instrumentCount; ++i) {
        auto& slot = slots[std::size_t(i)];
        slot.kit->forEach([&](dsp::Instrument& in) { in.prepare(sampleRate); });
        slot.fading = {};
        selectInstrument(slot, std::clamp(int(std::lround(slot.type->load())), 0, params::libraryCount - 1));
        slot.stage.setAmount(slot.distance != nullptr ? slot.distance->load() : 0.0f);
        slot.stage.prepare(sampleRate);
        slot.rack.prepare(sampleRate, i);
        slot.settings = effectSettings(slot);
        slot.rack.setSettings(slot.settings, environmentAmount->load(), hostBpm);
        slot.rack.setPlace(selectedPlace(), selectedCharacter());
        slot.gain.coef = float(1.0 - std::exp(-1.0 / (0.02 * sampleRate)));
        slot.share.coef = float(1.0 - std::exp(-1.0 / (0.02 * sampleRate)));
        slot.share.reset(slot.rack.placeShare());
        slot.gain.reset(slot.targetGain());
        slot.trim.coef = float(1.0 - std::exp(-1.0 / (0.05 * sampleRate)));
        slot.trim.reset(levelTrim(slot, int(selectedPlace()), selectedCharacter()));
        slot.running = slot.enabled();
        slot.lim.setSeed(std::uint32_t(evolutionSeed->load()) * 2654435761u + 1u);
        slot.lim.prepare(sampleRate, i);
    }
    updateLiminality(int(selectedPlace()), selectedCharacter());
    for (auto& slot : slots) slot.lim.reset();   // starts on the current amounts
    environments.setAmount(environmentAmount->load());
    samplePosition = 0;
    limClock = 0;
    hostWasPlaying = false;
    lastSeed = evolutionSeed->load();
    captures.build(sampleRate);
    environments.setCaptures(&captures);
    environments.prepare(sampleRate, selectedPlace(), selectedCharacter(), unsigned(lastSeed));
    applyQuality();
    juce::ignoreUnused(samplesPerBlock);
    // 20 ms smoothing for levels; Power fades over about 60 ms.
    const float levelCoef = float(1.0 - std::exp(-1.0 / (0.02 * sampleRate)));
    for (auto* s : { &masterGain, &instrumentGain, &ambienceGain, &spaceMix, &presenceDry, &presenceRoom }) s->coef = levelCoef;
    presenceDry.reset(1.0f);
    presenceRoom.reset(1.0f);
    powerGain.coef = float(1.0 - std::exp(-1.0 / (0.012 * sampleRate)));
    powerGain.reset(power->load() > 0.5f ? 1.0f : 0.0f);
    masterGain.reset(params::levelGain(master->load()));
    instrumentGain.reset(params::levelGain(instrumentLevel->load()));
    ambienceGain.reset(params::levelGain(ambienceLevel->load()));
    spaceMix.reset(space->load());
}

// New notes go to every enabled instrument; everything else (note-offs, pedal, bend,
// panic) to every instrument still sounding, so nothing is left hanging.
void LiminalProcessor::handleMidi(const juce::MidiMessage& m) {
    for (auto& slot : slots) {
        auto& in = *slot.instrument;
        if (m.isNoteOn()) {
            const int note = m.getNoteNumber();
            const bool inRange = note >= int(std::lround(slot.keyLow->load())) && note <= int(std::lround(slot.keyHigh->load()));
            if (slot.enabled() && inRange) {
                if (!slot.running) slot.rack.reset();   // starts fresh, on its current settings
                slot.running = true;
                // Velocity to Volume: 1 = the instrument's own response, 0 = every note alike.
                const float v = 0.75f + slot.velocityVolume->load() * (m.getFloatVelocity() - 0.75f);
                // Through Liminality: the note's detune, sourness, unevenness and timing.
                slot.lim.noteOn(in, note, std::clamp(v, 0.01f, 1.0f), samplePosition, limClock);
                meters.notes.fetch_add(1, std::memory_order_relaxed);
                meters.velocity.store(m.getFloatVelocity(), std::memory_order_relaxed);
                slot.rack.noteVelocity(m.getFloatVelocity());
            }
        } else if (!slot.running) {
            continue;
        } else if (m.isNoteOff()) {
            slot.lim.noteOff(in, m.getNoteNumber(), samplePosition);   // follows its note's delay
        } else if (m.isSustainPedalOn()) {
            in.sustain(true);
        } else if (m.isSustainPedalOff()) {
            in.sustain(false);
        } else if (m.isPitchWheel()) {
            in.pitchBend(m.getPitchWheelValue());
        } else if (m.isAllSoundOff()) {
            slot.lim.flush(in);
            in.allSoundOff();
        } else if (m.isAllNotesOff() || m.isResetAllControllers()) {
            slot.lim.flush(in);
            in.allNotesOff();
        }
    }
}

// Quality (Milestone 6): applied when it changes. Instruments: new notes only; the room's
// modulation glides out; the ambience caps simultaneous events; granular caps grains.
void LiminalProcessor::applyQuality() {
    const int q = std::clamp(int(std::lround(qualityChoice->load())), 0, 2);
    if (q == qualityNow) return;
    qualityNow = q;
    for (auto& slot : slots) {
        slot.kit->forEach([&](dsp::Instrument& in) { in.setQuality(q); });
        slot.rack.setQuality(q);
    }
    environments.setQuality(q);
}

void LiminalProcessor::updateSettings() {
    dsp::ForgottenPiano::Settings p;
    p.age = pianoSettings[0]->load();
    p.detune = pianoSettings[1]->load();
    p.softness = pianoSettings[2]->load();
    p.noise = pianoSettings[3]->load();
    p.decay = pianoSettings[4]->load();
    for (auto& slot : slots) slot.kit->piano.setSettings(p);
    dsp::FadedSynth::Settings f;
    f.detune = synthSettings[0]->load();
    f.warmth = synthSettings[1]->load();
    f.drift = synthSettings[2]->load();
    f.attack = synthSettings[3]->load();
    f.release = synthSettings[4]->load();
    f.width = synthSettings[5]->load();
    f.glide = synthSettings[6]->load() > 0.5f;
    f.glideTime = synthSettings[7]->load();
    f.patch = juce::roundToInt(synthSettings[8]->load());
    f.blend = synthSettings[9]->load();
    for (auto& slot : slots) slot.kit->synth.setSettings(f);
    dsp::GlassMemories::Settings g;
    g.fragility = glassSettings[0]->load();
    g.brightness = glassSettings[1]->load();
    g.resonance = glassSettings[2]->load();
    g.detune = glassSettings[3]->load();
    g.decay = glassSettings[4]->load();
    for (auto& slot : slots) slot.kit->glass.setSettings(g);
    dsp::EmptyChoir::Settings c;
    c.breath = choirSettings[0]->load();
    c.formant = choirSettings[1]->load();
    c.detune = choirSettings[2]->load();
    c.width = choirSettings[3]->load();
    c.darkness = choirSettings[4]->load();
    c.release = choirSettings[5]->load();
    for (auto& slot : slots) slot.kit->choir.setSettings(c);
    dsp::LostMusicBox::Settings b;
    b.wear = boxSettings[0]->load();
    b.drift = boxSettings[1]->load();
    b.mechanism = boxSettings[2]->load();
    b.brightness = boxSettings[3]->load();
    b.decay = boxSettings[4]->load();
    for (auto& slot : slots) slot.kit->box.setSettings(b);
    dsp::HollowDrone::Settings d;
    d.density = droneSettings[0]->load();
    d.instability = droneSettings[1]->load();
    d.harmonics = droneSettings[2]->load();
    d.sub = droneSettings[3]->load();
    d.movement = droneSettings[4]->load();
    d.release = droneSettings[5]->load();
    d.attack = droneSettings[6]->load();
    for (auto& slot : slots) slot.kit->drone.setSettings(d);
    // The library's six controls (2026-11-02), per slot.
    for (auto& slot : slots) {
        dsp::Library::Settings l;
        const auto v = [&](int k) { return slot.libParams[std::size_t(k)]->load(); };
        l.tone = v(0);
        l.age = v(1);
        l.softness = v(2);
        l.decay = v(3);
        l.width = v(4);
        l.colour = v(5);
        for (auto& lib : slot.kit->library) lib.setSettings(l);
    }
}

// On the fixed 64-sample control grid (counted from prepareToPlay, independent of the host's
// blocks): the place's housekeeping, the macros and evolution into the chains and the racks.
void LiminalProcessor::controlTick() {
    environments.maintain();
    environments.setLive(liveNow);
    const auto& chain = environments.current();
    const auto& evo = chain.evolution();
    const auto& flavour = dsp::unrealFlavour(int(chain.place()));
    const float u = liveNow.macros.unreal * dsp::character(int(chain.place()), chain.characterIndex()).unreal;
    effectLive.drift = u * flavour.drift + 0.15f * std::abs(evo.harmonic());
    effectLive.tape = u * flavour.tape;
    effectLive.grain = u * flavour.grain;
    effectLive.chorus = u * flavour.chorus;
    effectLive.colour = { u * flavour.shiftMix, u * flavour.wanderPan, u * flavour.wowCents + 3.0f * std::abs(evo.harmonic()), 0.8f * evo.spectral(),
                          farAway->load() };
    for (auto& slot : slots) slot.rack.setSettings(slot.settings, environmentAmount->load(), hostBpm, effectLive);
    for (auto& slot : slots) slot.lim.control(*slot.instrument, limClock);   // Liminality's pitch and audio side
}

// Liminality's amounts per instrument: its own, scaled by the LIMINALITY macro, nudged by the place
// and its Character (as much as Environment Amount and the instrument's Environment Influence allow).
void LiminalProcessor::updateLiminality(int place, int character) {
    const float macro = liminalityMacro->load(), env = environmentAmount->load();
    for (auto& slot : slots) {
        dsp::LiminalAmounts own {};
        for (std::size_t k = 0; k < own.size(); ++k) own[k] = slot.limParams[k]->load();
        slot.lim.setAmounts(dsp::liminalEffective(own, macro, place, character, env * std::clamp(slot.settings.influence, 0.0f, 1.0f)));
    }
}

void LiminalProcessor::renderSegment(float* left, float* right, int n) {
    if (samplePosition % controlPeriod == 0) controlTick();
    samplePosition += std::uint64_t(n);
    limClock += std::uint64_t(n);
    float* il = scratchL;
    float* ir = scratchR;
    std::fill(il, il + n, 0.0f);
    std::fill(ir, ir + n, 0.0f);
    std::fill(sendL, sendL + n, 0.0f);
    std::fill(sendR, sendR + n, 0.0f);
    std::fill(echoInL, echoInL + n, 0.0f);
    std::fill(echoInR, echoInR + n, 0.0f);
    std::fill(dryL, dryL + n, 0.0f);   // the share of the instruments the place's signature takes
    std::fill(dryR, dryR + n, 0.0f);
    for (auto& slot : slots) {
        if (!slot.running) continue;
        std::fill(slotL, slotL + n, 0.0f);
        std::fill(slotR, slotR + n, 0.0f);
        std::fill(slotSendL, slotSendL + n, 0.0f);
        std::fill(slotSendR, slotSendR + n, 0.0f);
        slot.instrument->render(slotL, slotR, n);
        for (auto& f : slot.fading) {   // what it played before a change: its notes fading, then its own tail
            if (f.instrument == nullptr) continue;
            if (f.tail < 0) {
                f.instrument->render(slotL, slotR, n);
                if (f.instrument->activeVoices() == 0) f.tail = f.tailLength = int(0.5 * preparedRate);
            } else {
                float tl[chunk] {}, tr[chunk] {};
                f.instrument->render(tl, tr, n);
                for (int i = 0; i < n; ++i) {
                    const float g = float(std::max(0, f.tail - i)) / float(f.tailLength);
                    slotL[i] += tl[i] * g * g;
                    slotR[i] += tr[i] * g * g;
                }
                f.tail -= n;
                if (f.tail <= 0) f = {};
            }
        }
        slot.lim.process(slotL, slotR, n);   // Liminality: after the voices, before Distance and the room
        slot.stage.process(slotL, slotR, slotSendL, slotSendR, n);
        // The instrument's own effects (and the place's processing of it); the rack makes
        // the room and echo sends. Farther instruments send more to the room.
        std::fill(slotSendL, slotSendL + n, 0.0f);
        std::fill(slotSendR, slotSendR + n, 0.0f);
        std::fill(slotEchoL, slotEchoL + n, 0.0f);
        std::fill(slotEchoR, slotEchoR + n, 0.0f);
        const float sendFactor = (1.0f + 1.5f * (slot.distance != nullptr ? slot.distance->load() : 0.0f)) * dsp::library::roomSend(slot.playing);
        slot.rack.process(slotL, slotR, slotSendL, slotSendR, slotEchoL, slotEchoR, sendFactor, n);
        slot.share.target = slot.rack.placeShare();
        for (int i = 0; i < n; ++i) {
            const float g = slot.gain.next() * slot.trim.next(), share = slot.share.next();
            il[i] += slotL[i] * g;
            ir[i] += slotR[i] * g;
            dryL[i] += slotL[i] * g * share;
            dryR[i] += slotR[i] * g * share;
            sendL[i] += slotSendL[i] * g;
            sendR[i] += slotSendR[i] * g;
            echoInL[i] += slotEchoL[i] * g;
            echoInR[i] += slotEchoR[i] * g;
        }
        // Switched off and faded out: stop it (and forget its notes) until enabled again.
        if (!slot.enabled() && slot.gain.current < 1.0e-5f) {
            slot.instrument->reset();
            for (auto& f : slot.fading) {
                if (f.instrument != nullptr) f.instrument->reset();
                f = {};
            }
            slot.rack.reset();
            slot.lim.reset();
            slot.running = false;
        }
    }
    for (int i = 0; i < n; ++i) {
        const float ig = instrumentGain.next();
        il[i] *= ig;
        ir[i] *= ig;
        dryL[i] *= ig;
        dryR[i] *= ig;
        const float roomGain = presenceRoom.next();
        sendL[i] *= ig * roomGain;
        sendR[i] *= ig * roomGain;
        echoInL[i] *= ig * roomGain;   // the place's echo recedes too as the instruments come near
        echoInR[i] *= ig * roomGain;
        instrumentSum += double(il[i] * il[i] + ir[i] * ir[i]) * 0.5;
        ambienceGains[i] = ambienceGain.next();
        spaces[i] = spaceMix.next();
    }
    // The environment: the instruments' processing, echoes, room and ambience.
    std::fill(mixL, mixL + n, 0.0f);
    std::fill(mixR, mixR + n, 0.0f);
    std::fill(ambiencePower, ambiencePower + n, 0.0f);
    for (int i = 0; i < n; ++i) {   // the instruments, dry (a little less of them as the room grows)
        const float keep = (1.0f - 0.45f * spaces[i]) * presenceDry.next();
        mixL[i] = il[i] * keep;
        mixR[i] = ir[i] * keep;
        dryL[i] *= keep;
        dryR[i] *= keep;
    }
    environments.process(sendL, sendR, echoInL, echoInR, dryL, dryR, ambienceGains, spaces, mixL, mixR, ambiencePower, n);
    for (int i = 0; i < n; ++i) {
        const float p = powerGain.next(), m = masterGain.next() * p;
        float l = mixL[i] * m;
        float r = mixR[i] * m;
        // Soft safety limit above -3 dBFS; never above 0 dBFS.
        const auto soft = [](float x) {
            const float a = std::abs(x);
            if (a <= 0.7f) return x;
            return std::copysign(0.7f + 0.3f * std::tanh((a - 0.7f) / 0.3f), x);
        };
        l = soft(l);
        r = soft(r);
        if (p < 1.0e-6f) l = r = 0.0f;   // Power off is exact silence once faded
        left[i] = l;
        right[i] = r;
        ambienceSum += double(ambiencePower[i]);
        outputSum += double(l * l + r * r) * 0.5;
    }
}

void LiminalProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    updateMirror();
    powerGain.target = power->load() > 0.5f ? 1.0f : 0.0f;
    masterGain.target = params::levelGain(master->load());
    instrumentGain.target = params::levelGain(instrumentLevel->load());
    ambienceGain.target = params::levelGain(ambienceLevel->load());
    spaceMix.target = space->load();
    updateSettings();
    for (auto& slot : slots) selectInstrument(slot, std::clamp(int(std::lround(slot.type->load())), 0, params::libraryCount - 1));
    updateSolo();
    applyQuality();
    environments.setAmount(environmentAmount->load());
    const auto place = selectedPlace();
    const int characterIndex = selectedCharacter();
    environments.select(place, characterIndex);
    if (evolutionSeed->load() != lastSeed) {   // a new seed: evolution starts again from it
        lastSeed = evolutionSeed->load();
        environments.setEvolutionSeed(unsigned(lastSeed));
        for (auto& slot : slots) slot.lim.setSeed(std::uint32_t(lastSeed) * 2654435761u + 1u);
        environments.resetEvolution();
    }
    if (evolutionResetRequest.exchange(false)) environments.resetEvolution();
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition()) {
            if (const auto bpm = position->getBpm()) hostBpm = std::clamp(*bpm, 20.0, 400.0);
            // Liminality's clock follows the song while it plays, so a replay or a bounce of the same
            // passage has the same detuned notes, drift and wow.
            // Only a real jump of the timeline is a relocation (a cycle, a click in the ruler, play
            // from elsewhere): hosts report their position with rounding (GarageBand derives it from
            // the beat position), and a sample or two of disagreement, block after block, used to
            // reseed every instrument and Liminality mid-note - clicks, a jumping chorus, notes that
            // seemed to hang (2026-11-05). Within the tolerance the stage keeps its own count.
            const bool playing = position->getIsPlaying();
            if (playing)
                if (const auto at = position->getTimeInSamples()) {
                    const auto host = std::uint64_t(std::max<std::int64_t>(0, *at));
                    const auto gap = host > limClock ? host - limClock : limClock - host;
                    const auto tolerance = std::uint64_t(std::max(256.0, relocateTolerance * preparedRate));
                    if (gap > (hostWasPlaying ? tolerance : 0)) {
                        limClock = host;
                        for (std::size_t i = 0; i < slots.size(); ++i) {
                            slots[i].lim.relocate(limClock, slots[i].running && slots[i].instrument->activeVoices() > 0);
                            slots[i].instrument->reseed(std::uint32_t(limClock * 0x9E3779B97F4A7C15ull >> 32) ^ std::uint32_t(i * 0x85EBCA77u));
                        }
                    }
                }
            hostWasPlaying = playing;
        }
    // The macros (Milestone 5). Presence brings the instruments forward (and the place's
    // sounds back); Scale, Activity and Unreal act on the place, each in its own way.
    const dsp::Macros macros { macroParams[0]->load(), macroParams[1]->load(), macroParams[2]->load(), macroParams[3]->load() };
    const auto& response = dsp::macroResponse(int(place));
    const float presence = (macros.presence - 0.5f) * response.presence;
    presenceDry.target = std::pow(10.0f, 12.0f * presence / 20.0f);
    presenceRoom.target = std::pow(10.0f, -8.0f * presence / 20.0f);
    liveNow.macros = macros;
    liveNow.evolutionRate = evolutionRate->load();
    liveNow.frozen = evolutionFreeze->load() > 0.5f;
    liveNow.ambienceGain = std::pow(10.0f, -6.0f * presence / 20.0f);
    for (auto& slot : slots) {
        slot.settings = effectSettings(slot);
        slot.rack.setSettings(slot.settings, environmentAmount->load(), hostBpm, effectLive);
        slot.rack.setPlace(place, characterIndex);
        slot.gain.target = slot.targetGain();
        slot.trim.target = levelTrim(slot, int(place), characterIndex);
        if (!slot.running) slot.trim.reset(slot.trim.target);   // (silent: it starts at the right level)
        if (slot.distance != nullptr) slot.stage.setAmount(slot.distance->load());
        if (slot.enabled() && !slot.running) {
            slot.rack.reset();
            slot.running = true;
        }
    }
    updateLiminality(int(place), characterIndex);

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    instrumentSum = ambienceSum = outputSum = 0;
    // Rendered in chunks of at most `chunk` samples, split at each MIDI event's position.
    auto renderRange = [&](int from, int to) {
        float monoRight[chunk];
        while (from < to) {
            // Notes held back by Liminality's Timing start at their exact sample.
            std::uint64_t due = std::numeric_limits<std::uint64_t>::max();
            for (auto& slot : slots) {
                if (!slot.running) continue;
                slot.lim.fireDue(*slot.instrument, samplePosition);
                due = std::min(due, slot.lim.nextDue());
            }
            // Never across a step of the 64-sample control grid (see controlTick), nor past a held-back note.
            int n = std::min({ chunk, to - from, controlPeriod - int(samplePosition % controlPeriod) });
            if (due > samplePosition && due - samplePosition < std::uint64_t(n)) n = int(due - samplePosition);
            if (right != nullptr) {
                renderSegment(left + from, right + from, n);
            } else {
                renderSegment(left + from, monoRight, n);
                for (int i = 0; i < n; ++i) left[from + i] = 0.5f * (left[from + i] + monoRight[i]);
            }
            from += n;
        }
    };
    int position = 0;
    for (const auto metadata : midi) {
        const int at = std::clamp(metadata.samplePosition, 0, total);
        renderRange(position, at);
        position = at;
        handleMidi(metadata.getMessage());
    }
    renderRange(position, total);
    // The VHS look's optional tape warble (off by default), following the picture's glitches.
    warble.process(left, right, total, warbleOn->load() > 0.5f, vhsWarble.load(std::memory_order_relaxed));
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, total);

    if (total > 0) {
        meters.instrument.store(float(std::sqrt(instrumentSum / total)), std::memory_order_relaxed);
        meters.ambience.store(float(std::sqrt(ambienceSum / total)), std::memory_order_relaxed);
        meters.output.store(float(std::sqrt(outputSum / total)), std::memory_order_relaxed);
    }
    int voices = 0;
    for (const auto& slot : slots)
        if (slot.running) voices += slot.instrument->activeVoices() + slot.fadingVoices();
    meters.voices.store(voices, std::memory_order_relaxed);
    int held = 0;
    for (const auto& slot : slots)
        if (slot.running) held += slot.instrument->heldVoices();
    meters.held.store(held, std::memory_order_relaxed);
    meters.place.store(int(environments.place()), std::memory_order_relaxed);
    meters.character.store(environments.current().characterIndex(), std::memory_order_relaxed);
    meters.events.store(environments.current().ambienceEvents(), std::memory_order_relaxed);
    {   // the darkest the failing tube got since the editor last looked (it exchanges this with 1)
        const float lamp = environments.current().lamp();
        if (lamp < meters.lamp.load(std::memory_order_relaxed)) meters.lamp.store(lamp, std::memory_order_relaxed);
        meters.flickers.store(environments.current().flickers(), std::memory_order_relaxed);
    }
    meters.activity.store(environments.current().evolution().movement(), std::memory_order_relaxed);
}

//==============================================================================
// Presets.
void LiminalProcessor::setParameter(const juce::String& id, float value) {
    if (auto* p = parameters.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    }
}

void LiminalProcessor::resetToDefaults() {
    for (const auto& m : mirrored) {
        const auto id = m.parameter->getParameterID();
        if (id == params::power || id == params::vhs || id == params::vhsWarble) continue;   // (viewing / listening preferences, not the sound's design)
        m.parameter->beginChangeGesture();
        m.parameter->setValueNotifyingHost(m.parameter->getDefaultValue());
        m.parameter->endChangeGesture();
    }
}

// INIT: a clean default, as a new instance has it.
void LiminalProcessor::initPreset() {
    undoable(initName, [this] {
        resetToDefaults();
        requestEvolutionReset();
        currentPresetName = initName;
    });
}

LiminalProcessor::Snapshot LiminalProcessor::snapshot() const {
    Snapshot s;
    s.values.reserve(mirrored.size());
    for (const auto& m : mirrored) s.values.push_back(m.parameter->getValue());
    s.name = currentPresetName;
    return s;
}

void LiminalProcessor::restore(const Snapshot& s) {
    for (std::size_t i = 0; i < mirrored.size() && i < s.values.size(); ++i) {
        auto* p = mirrored[i].parameter;
        if (p->getValue() == s.values[i]) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost(s.values[i]);
        p->endChangeGesture();
    }
    currentPresetName = s.name;
}

namespace {
struct PresetStep final : juce::UndoableAction {
    PresetStep(std::function<void(bool)> apply) : applyFn(std::move(apply)) {}
    bool perform() override { applyFn(true); return true; }    // (the first time: already done)
    bool undo() override { applyFn(false); return true; }
    std::function<void(bool)> applyFn;
};
}

void LiminalProcessor::undoable(const juce::String& name, const std::function<void()>& load) {
    auto before = snapshot();
    load();
    auto after = snapshot();
    if (after.values == before.values && after.name == before.name) return;   // nothing changed: nothing to undo
    presetUndo.beginNewTransaction(name);
    presetUndo.perform(new PresetStep([this, before = std::move(before), after = std::move(after)](bool forward) { restore(forward ? after : before); }), name);
}

bool LiminalProcessor::undoPreset() { return presetUndo.undo(); }
bool LiminalProcessor::redoPreset() { return presetUndo.redo(); }

// What a preset resets before it applies its values depends on its group: a scene sets
// everything; a place or Character preset only the place's controls; an instrument preset
// only the instruments (the chosen one back to its defaults).
void LiminalProcessor::applyPreset(const presets::Preset& preset) {
    undoable(preset.name, [this, &preset] { applyPresetNow(preset); });
}

void LiminalProcessor::applyPresetNow(const presets::Preset& preset) {
    using presets::Category;
    const auto resetIds = [&](const juce::StringArray& ids) {
        for (const auto& id : ids)
            if (auto* p = parameters.getParameter(id)) p->setValueNotifyingHost(p->getDefaultValue());
    };
    switch (preset.category) {
    case Category::scene:
        resetToDefaults();
        break;
    case Category::environment:
    case Category::character:
        resetIds({ params::environment, params::character, params::environmentAmount, params::presence, params::scale, params::activity, params::unreal,
                   params::evolution, params::evolutionFreeze, params::farAway });
        break;
    case Category::instrument: {
        juce::StringArray ids { params::solo };
        for (const char* key : params::instrumentKeys) ids.add(juce::String(key) + "_on");
        if (preset.instrument >= 0)
            for (const auto& m : mirrored) {
                const auto id = m.parameter->getParameterID();
                if (id.startsWith(juce::String(params::instrumentKeys[preset.instrument]) + "_")) ids.add(id);
            }
        resetIds(ids);
        break;
    }
    case Category::user:
        break;
    }
    for (const auto& [id, value] : preset.values) setParameter(id, value);
    currentPresetName = preset.name;
}

int LiminalProcessor::getNumPrograms() { return 1 + int(presets::inCategory(presets::Category::scene).size()); }

const juce::String LiminalProcessor::getProgramName(int index) {
    const auto scenes = presets::inCategory(presets::Category::scene);
    if (index <= 0 || index > int(scenes.size())) return "Init";
    return presets::factory()[std::size_t(scenes[std::size_t(index - 1)])].name;
}

void LiminalProcessor::setCurrentProgram(int index) {
    const auto scenes = presets::inCategory(presets::Category::scene);
    currentProgram = std::clamp(index, 0, int(scenes.size()));
    if (currentProgram == 0) {
        initPreset();
    } else {
        applyPreset(presets::factory()[std::size_t(scenes[std::size_t(currentProgram - 1)])]);
    }
}

// User presets: one XML file each, every parameter by ID (scenes A/B are part of the song,
// not of a preset).
juce::File LiminalProcessor::userPresetFolder() { return brand::appDataFolder().getChildFile("Presets"); }
juce::File LiminalProcessor::legacyUserPresetFolder() { return brand::legacyAppDataFolder().getChildFile("Presets"); }

// The presets saved with LIMINAL are copied over once (the old files are left where they were).
void LiminalProcessor::carryOverLegacyPresets() {
    const auto old = legacyUserPresetFolder();
    if (!old.isDirectory()) return;
    const auto folder = userPresetFolder();
    for (const auto& f : old.findChildFiles(juce::File::findFiles, false, juce::String("*") + brand::legacyPresetExtension)) {
        const auto target = folder.getChildFile(f.getFileNameWithoutExtension() + brand::presetExtension);
        if (target.existsAsFile() || !folder.createDirectory()) continue;
        f.copyFileTo(target);
    }
}

juce::File LiminalProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = juce::File::createLegalFileName(name.trim().isEmpty() ? juce::String("Untitled") : name.trim());
    auto folder = userPresetFolder();
    if (!folder.createDirectory()) return {};
    juce::XmlElement xml(brand::presetTag);
    xml.setAttribute("name", clean);
    xml.setAttribute("stateVersion", stateVersion);
    for (const auto& m : mirrored) {
        const auto id = m.parameter->getParameterID();
        if (id == params::power || id == params::sceneOn || id == params::sceneMorph) continue;
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", id);
        e->setAttribute("value", double(m.parameter->convertFrom0to1(m.parameter->getValue())));
    }
    const auto file = folder.getChildFile(clean + brand::presetExtension);
    if (!xml.writeTo(file)) return {};
    currentPresetName = clean;
    return file;
}

bool LiminalProcessor::loadUserPreset(const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !(xml->hasTagName(brand::presetTag) || xml->hasTagName(brand::legacyPresetTag))) return false;   // (LIMINAL's presets too)
    const auto name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
    undoable(name, [&] {
        resetToDefaults();   // parameters the preset does not know (added later) take their defaults
        for (auto* e : xml->getChildWithTagNameIterator("PARAM")) setParameter(e->getStringAttribute("id"), float(e->getDoubleAttribute("value")));
        currentPresetName = name;
    });
    return true;
}

juce::Array<juce::File> LiminalProcessor::userPresets() const {
    carryOverLegacyPresets();
    auto files = userPresetFolder().findChildFiles(juce::File::findFiles, false, juce::String("*") + brand::presetExtension + ";*" + brand::legacyPresetExtension);
    files.sort();
    return files;
}

juce::AudioProcessorEditor* LiminalProcessor::createEditor() { return new LiminalEditor(*this); }

void LiminalProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("stateVersion", stateVersion);
    xml->setAttribute("uiScale", double(uiScale.load()));
    auto* scenesXml = xml->createNewChildElement("SCENES");   // by parameter ID, so later versions can add parameters
    for (int w = 0; w < 2; ++w) {
        if (!stored[std::size_t(w)].load()) continue;
        auto* scene = scenesXml->createNewChildElement(w == 0 ? "A" : "B");
        for (std::size_t i = 0; i < mirrored.size(); ++i) scene->setAttribute(mirrored[i].parameter->getParameterID(), double(scenes[std::size_t(w)][i].load()));
    }
    copyXmlToBinary(*xml, destData);
}

void LiminalProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;   // not ours: ignore
    uiScale.store(float(std::clamp(xml->getDoubleAttribute("uiScale", 1.0), 0.85, 1.5)));
    for (int w = 0; w < 2; ++w) stored[std::size_t(w)].store(false);
    if (auto* scenesXml = xml->getChildByName("SCENES")) {
        for (int w = 0; w < 2; ++w) {
            auto* scene = scenesXml->getChildByName(w == 0 ? "A" : "B");
            if (scene == nullptr) continue;
            for (std::size_t i = 0; i < mirrored.size(); ++i) {
                const auto id = mirrored[i].parameter->getParameterID();
                // A parameter the scene does not know (added later): its default.
                scenes[std::size_t(w)][i].store(float(scene->getDoubleAttribute(id, double(mirrored[i].parameter->getDefaultValue()))));
            }
            stored[std::size_t(w)].store(true);
        }
        xml->removeChildElement(scenesXml, true);
    }
    parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

} // namespace liminal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new liminal::LiminalProcessor(); }
