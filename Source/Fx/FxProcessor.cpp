#include "Fx/FxProcessor.h"
#include "Fx/FxEditor.h"
#include "Fx/FxLevelMatch.h"
#include <cmath>
#include <limits>

namespace liminal::fx {

namespace {
float dbGain(float db) { return std::pow(10.0f, db / 20.0f); }
float coefFor(double seconds, double sr) { return float(1.0 - std::exp(-1.0 / (seconds * sr))); }
// The world's safety: untouched below -1 dBFS, never above 0 dBFS (the dry share is never limited).
float safety(float x) {
    const float a = std::abs(x);
    if (a <= 0.89f) return x;
    return std::copysign(0.89f + 0.11f * std::tanh((a - 0.89f) / 0.11f), x);
}
} // namespace

FxProcessor::FxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "TAPEWORN_FX", fxp::makeLayout()) {
    bypassParameter = parameters.getParameter(fxp::bypass);
    bypassOn = raw(fxp::bypass);
    mixParam = raw(fxp::mix);
    inputParam = raw(fxp::input);
    outputParam = raw(fxp::output);
    intensityParam = raw(fxp::intensity);
    placeParam = raw(fxp::environment);
    characterParam = raw(fxp::character);
    spaceParam = raw(fxp::space);
    roomSoundParam = raw(fxp::ambience);
    macroParams[0] = raw(fxp::presence);
    macroParams[1] = raw(fxp::scale);
    macroParams[2] = raw(fxp::activity);
    macroParams[3] = raw(fxp::unreal);
    evolutionRate = raw(fxp::evolution);
    evolutionFreeze = raw(fxp::evolutionFreeze);
    evolutionSeed = raw(fxp::evolutionSeed);
    farAway = raw(fxp::farAway);
    qualityParam = raw(fxp::quality);
    warbleOn = raw(fxp::vhsWarble);
    liminalityMacro = raw(fxp::liminality);
    for (int k = 0; k < dsp::lim::count; ++k) limParams[std::size_t(k)] = raw(fxp::limId(k));
    // The place's processing of the input: the place's own layer only (the instrument's player
    // effects all at 0), at Influence 100 %, through its Effects Mix 100 %.
    rackSettings = {};
    setLatencySamples(latencyFor(48000.0));
    abStore[0] = abStore[1] = snapshot();
}

bool FxProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Stereo, mono, and mono in / stereo out (a mono track's insert in many hosts).
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    return (in == stereo && out == stereo) || (in == mono && out == mono) || (in == mono && out == stereo);
}

dsp::Place FxProcessor::selectedPlace() const { return dsp::Place(std::clamp(int(std::lround(placeParam->load())), 0, dsp::placeCount - 1)); }
int FxProcessor::selectedCharacter() const { return std::clamp(int(std::lround(characterParam->load())), 0, dsp::characterCount - 1); }

void FxProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    rate = sampleRate;
    latency = latencyFor(sampleRate);
    setLatencySamples(latency);
    int n = 1;
    while (n < latency + 8) n <<= 1;
    dryLineL.assign(std::size_t(n), 0.0f);
    dryLineR.assign(std::size_t(n), 0.0f);
    dryMask = n - 1;
    dryWrite = 0;
    lim.setSeed(std::uint32_t(evolutionSeed->load()) * 2654435761u + 7u);
    lim.prepare(sampleRate, latency);
    rack.prepare(sampleRate, 0);
    const float amount = std::clamp(intensityParam->load(), 0.0f, 1.0f);
    rack.setSettings(rackSettings, amount, 120.0);
    rack.setPlace(selectedPlace(), selectedCharacter());
    rack.reset();
    lastSeed = evolutionSeed->load();
    captures.build(sampleRate);
    environments.setCaptures(&captures);
    environments.setAmount(amount);
    environments.prepare(sampleRate, selectedPlace(), selectedCharacter(), unsigned(lastSeed));
    qualityNow = -1;
    samplePosition = limClock = 0;
    hostWasPlaying = false;
    const float levelCoef = coefFor(0.02, sampleRate);
    for (auto* s : { &inGain, &outGain, &mixGain, &spaceMix, &presenceDry, &presenceRoom, &share }) s->coef = levelCoef;
    bypassStep = float(1.0 / (0.025 * sampleRate));
    trim.coef = coefFor(0.03, sampleRate);
    roomSound.coef = levelCoef;
    updateBlockSettings();
    for (auto* s : { &inGain, &outGain, &mixGain, &bypassGain, &spaceMix, &trim, &roomSound, &presenceDry, &presenceRoom, &share }) s->reset(s->target);
    bypassPos = bypassGain.target;
    trimNow = trim.current;
    limiterGain = 1.0f;
    limiterAttack = coefFor(0.0003, sampleRate);
    limiterRelease = coefFor(0.12, sampleRate);
    share.reset(rack.placeShare());
    fastCoef = coefFor(0.004, sampleRate);
    liveGlide = float(1.0 - std::exp(-64.0 / (0.3 * sampleRate)));
    slowCoef = coefFor(0.15, sampleRate);
    releaseCoef = coefFor(0.06, sampleRate);
    presenceUp = coefFor(0.3, sampleRate);
    presenceDown = coefFor(3.0, sampleRate);
    envFast = envSlow = presence = 0;
    sinceOnset = 1;
    {   // K-weighting at this rate (the standard's two stages, by their analog prototypes)
        const double kh = std::tan(3.141592653589793 * 1681.974450955533 / sampleRate), q = 0.7071752369554196;
        const double vh = std::pow(10.0, 3.999843853973347 / 20.0), vb = std::pow(vh, 0.4996667741545416);
        const double kl = std::tan(3.141592653589793 * 38.13547087602444 / sampleRate), ql = 0.5003270373238773;
        for (std::size_t i = 0; i < kWeight.size(); i += 2) {
            kWeight[i].normalise(vh + vb * kh / q + kh * kh, 2.0 * (kh * kh - vh), vh - vb * kh / q + kh * kh, 1.0 + kh / q + kh * kh, 2.0 * (kh * kh - 1.0), 1.0 - kh / q + kh * kh);
            kWeight[i + 1].normalise(1.0, -2.0, 1.0, 1.0 + kl / ql + kl * kl, 2.0 * (kl * kl - 1.0), 1.0 - kl / ql + kl * kl);
            kWeight[i].clear();
            kWeight[i + 1].clear();
        }
    }
    matchGainCoefStart = coefFor(0.15, sampleRate);
    matchGainCoefFast = coefFor(0.25, sampleRate);
    matchGainCoef = coefFor(0.6, sampleRate);
    matchCoef = coefFor(8.0, sampleRate);
    matchIn = matchOut = matchSumIn = matchSumOut = 0;
    matchGain = matchTarget = 1;
    matchSamples = 0;
    levelOpen = levelStarting = levelChanged = false;
    updateBlockSettings();
    lim.reset();
}

// Once per block: the parameters' values as the DSP's targets.
void FxProcessor::updateBlockSettings() {
    const float intensity = std::clamp(intensityParam->load(), 0.0f, 1.0f);
    const auto place = selectedPlace();
    const int character = selectedCharacter();
    inGain.target = dbGain(inputParam->load());
    outGain.target = dbGain(outputParam->load());
    mixGain.target = std::clamp(mixParam->load(), 0.0f, 1.0f);
    bypassGain.target = bypassOn->load() > 0.5f ? 1.0f : 0.0f;
    // Intensity: the place's processing (Environment Amount) and, with it, how much of its room
    // (Space x Intensity / default: the default Intensity gives Space as set).
    spaceMix.target = std::clamp(spaceParam->load() * intensity / fxp::defaultIntensity, 0.0f, 1.0f);
    roomSound.target = params::levelGain(roomSoundParam->load());
    trimDbNow = levelmatch::trimDb(int(place), character, intensity);
    if (int(place) != matchPlace || character != matchCharacter) {   // another room: the automatic part settles again (from where it was)
        matchPlace = int(place);
        matchCharacter = character;
        matchSamples = 0;
        matchSumIn = matchSumOut = 0;   // (the new room's own loudness from now on)
        matchTarget = 1.0f;             // (from the new room's calibrated level, gliding)
        levelChanged = matchPlace >= 0 && samplePosition > 0;
    }
    trim.target = dbGain(trimDbNow);
    // Presence (as the instrument's): the input nearer, the room further, and the other way round.
    const dsp::Macros macros { macroParams[0]->load(), macroParams[1]->load(), macroParams[2]->load(), macroParams[3]->load() };
    const float p = (macros.presence - 0.5f) * dsp::macroResponse(int(place)).presence;
    presenceDry.target = std::pow(10.0f, 12.0f * p / 20.0f);
    presenceRoom.target = std::pow(10.0f, -8.0f * p / 20.0f);
    liveNow.macros = macros;
    liveNow.evolutionRate = evolutionRate->load();
    liveNow.frozen = evolutionFreeze->load() > 0.5f;
    liveNow.ambienceGain = std::pow(10.0f, -6.0f * p / 20.0f);
    environments.setAmount(intensity);
    environments.select(place, character);
    rack.setSettings(rackSettings, intensity, 120.0, effectLive);
    rack.setPlace(place, character);
    // LIMINALITY: the effect's own amounts x the macro, nudged by the place (as much as Intensity allows).
    dsp::LiminalAmounts own {};
    for (std::size_t k = 0; k < own.size(); ++k) own[k] = limParams[k]->load();
    lim.setAmounts(dsp::liminalEffective(own, liminalityMacro->load(), int(place), character, intensity));
    lim.setVhsWarble(warbleOn->load() > 0.5f ? vhsWarble.load(std::memory_order_relaxed) : 0.0f);
    const int q = std::clamp(int(std::lround(qualityParam->load())), 0, 2);
    if (q != qualityNow) {
        qualityNow = q;
        rack.setQuality(q);
        environments.setQuality(q);
    }
    if (evolutionSeed->load() != lastSeed) {   // a new seed: the evolution and the randomness start again from it
        lastSeed = evolutionSeed->load();
        environments.setEvolutionSeed(unsigned(lastSeed));
        environments.resetEvolution();
        lim.setSeed(std::uint32_t(lastSeed) * 2654435761u + 7u);
    }
}

// On the fixed 64-sample control grid: the place's housekeeping, the macros and evolution into the
// chain and the rack, LIMINALITY's control tick.
void FxProcessor::controlTick() {
    environments.maintain();
    environments.setLive(liveNow);
    const auto& chain = environments.current();
    const auto& evo = chain.evolution();
    const auto& flavour = dsp::unrealFlavour(int(chain.place()));
    const float u = liveNow.macros.unreal * dsp::character(int(chain.place()), chain.characterIndex()).unreal;
    // What Unreal and the evolution do to the place's processing. The new place's evolution starts
    // from its own seed, so at a change of place these glide over (~0.3 s) instead of jumping (a
    // jump in the filters' and the pitch's movement was a tick on a held tone).
    dsp::EffectLive want;
    want.drift = u * flavour.drift + 0.15f * std::abs(evo.harmonic());
    want.tape = u * flavour.tape;
    want.grain = u * flavour.grain;
    want.chorus = u * flavour.chorus;
    want.colour = { u * flavour.shiftMix, u * flavour.wanderPan, u * flavour.wowCents + 3.0f * std::abs(evo.harmonic()), 0.8f * evo.spectral(), farAway->load() };
    const float k = liveGlide;
    const auto glide = [k](float& v, float t) { v += k * (t - v); };
    glide(effectLive.drift, want.drift);
    glide(effectLive.tape, want.tape);
    glide(effectLive.grain, want.grain);
    glide(effectLive.chorus, want.chorus);
    glide(effectLive.colour.shiftMix, want.colour.shiftMix);
    glide(effectLive.colour.wanderPan, want.colour.wanderPan);
    glide(effectLive.colour.wowCents, want.colour.wowCents);
    glide(effectLive.colour.lowOctaves, want.colour.lowOctaves);
    effectLive.colour.far = want.colour.far;   // (the parameter itself; the Colour glides it)
    rack.setSettings(rackSettings, std::clamp(intensityParam->load(), 0.0f, 1.0f), 120.0, effectLive);
    lim.control(limClock);
    // The level match's automatic part: since a start (or a change of room) it compares the totals
    // heard so far, after eight seconds the last ~8 s; it moves only while something plays.
    levelOpen = automaticLevel && envSlow > 0.003f;   // (above about -50 dBFS)
    levelReference = lim.intendedGain();
    levelStarting = matchSamples < std::uint64_t(8.0 * rate);
    if (!automaticLevel) {
        matchTarget = 1.0f;
        return;
    }
    if (levelStarting) {   // (the running totals hand over to the slow follower at the end)
        matchIn = matchSumIn;
        matchOut = matchSumOut;
    }
    if (levelOpen) matchSamples += controlPeriod;
    // (From a start it waits for half a second of sound - the room builds up - and stays within
    // +/-4 dB of the room's calibrated level for the first two seconds, then +/-12 dB. After a change
    // of room, with the sound already playing, it answers after a fifth of a second, within +/-9 dB:
    // a source far from the calibration's average - a bass in the hotel - is not left too loud.)
    const auto wait = std::uint64_t((levelChanged ? 0.2 : 0.5) * rate);
    if (levelOpen && matchSamples > wait && matchOut > 1.0e-15) {
        const float limit = levelChanged ? (matchSamples < std::uint64_t(2.0 * rate) ? 2.8f : 4.0f) : matchSamples < std::uint64_t(2.0 * rate) ? 1.585f : 4.0f;
        matchTarget = std::clamp(float(std::sqrt(matchIn / matchOut)), 1.0f / limit, limit);
    }
}

void FxProcessor::renderSegment(const float* inL, const float* inR, float* outL, float* outR, int n, bool mono) {
    if (samplePosition % controlPeriod == 0) controlTick();
    samplePosition += std::uint64_t(n);
    limClock += std::uint64_t(n);
    // The input (at the Input level) into the world; the raw input into the dry line.
    float dl[chunk], dr[chunk];
    for (int i = 0; i < n; ++i) {
        const float g = inGain.next();
        bufL[i] = inL[i] * g;
        bufR[i] = inR[i] * g;
        dryLineL[std::size_t(dryWrite)] = inL[i];
        dryLineR[std::size_t(dryWrite)] = inR[i];
        dl[i] = dryLineL[std::size_t((dryWrite - latency) & dryMask)];
        dr[i] = dryLineR[std::size_t((dryWrite - latency) & dryMask)];
        dryWrite = (dryWrite + 1) & dryMask;
        // Attacks and the input's presence (from the input as it goes in).
        const float level = std::max(std::abs(bufL[i]), std::abs(bufR[i]));
        envFast += (level > envFast ? fastCoef : releaseCoef) * (level - envFast);
        envSlow += slowCoef * (level - envSlow);
        sinceOnset += 1.0f / float(rate);
        if (envFast > 0.006f && envFast > 2.2f * envSlow + 0.002f && sinceOnset > 0.09f) {
            sinceOnset = 0;
            const float strength = std::clamp((juce::Decibels::gainToDecibels(envFast, -100.0f) + 40.0f) / 34.0f, 0.0f, 1.0f);
            lim.onset(strength);
            meters.notes.fetch_add(1, std::memory_order_relaxed);
            meters.velocity.store(strength, std::memory_order_relaxed);
        }
        const float here = std::clamp((juce::Decibels::gainToDecibels(envSlow, -100.0f) + 60.0f) / 20.0f, 0.0f, 1.0f);
        presence += (here > presence ? presenceUp : presenceDown) * (here - presence);
        inSum += double(bufL[i] * bufL[i] + bufR[i] * bufR[i]) * 0.5;
    }
    // The level match's automatic part: the input as it comes in - with the level changes LIMINALITY
    // makes on purpose (Fade, Unevenness) - against the room's output (below). K-weighted energy, over
    // everything heard since a start or a change of room (so it settles within a second or two), then
    // over the last eight seconds or so; the make-up only moves while something plays.
    // (Decided on the 64-sample control grid - levelOpen, levelStarting - so the sound does not
    // depend on the host's block size.)
    const bool starting = levelStarting;
    if (automaticLevel) {
        const float g = levelReference;
        for (int i = 0; i < n; ++i) {
            const float a = weigh(0, bufL[i]) * g, b = weigh(1, bufR[i]) * g;
            const double e = double(a * a + b * b);
            if (starting) matchSumIn += e;
            matchIn += double(matchCoef) * (e - matchIn);
        }
    }
    // LIMINALITY (pitch line: the latency), then the place's processing with its sends.
    lim.process(bufL, bufR, n);
    std::fill(sendL, sendL + n, 0.0f);
    std::fill(sendR, sendR + n, 0.0f);
    std::fill(echoL, echoL + n, 0.0f);
    std::fill(echoR, echoR + n, 0.0f);
    rack.process(bufL, bufR, sendL, sendR, echoL, echoR, 1.0f, n);
    lim.processSends(sendL, sendR, echoL, echoR, n);
    share.target = rack.placeShare();
    // The room's level match (its calibrated trim x the automatic make-up): on the near sound and on
    // what the place being heard adds; a place fading out keeps the level it had (its tail is not
    // turned up or down by the next room's match).
    const float kg = levelStarting ? (levelChanged ? matchGainCoefStart : matchGainCoefFast) : matchGainCoef;
    for (int i = 0; i < n; ++i) {
        matchGain += kg * (matchTarget - matchGain);
        madeUp[i] = matchGain;
        trimNow += trim.coef * (trim.next() - trimNow);   // (two one-poles in a row: the new room's level comes in without a kink)
        roomGains[i] = trimNow * matchGain;
    }
    for (int i = 0; i < n; ++i) {
        spaces[i] = spaceMix.next();
        const float keep = (1.0f - 0.45f * spaces[i]) * presenceDry.next(), roomGain = presenceRoom.next(), sh = share.next();
        mixL[i] = bufL[i] * keep * roomGains[i];
        mixR[i] = bufR[i] * keep * roomGains[i];
        dryL[i] = bufL[i] * keep * sh;
        dryR[i] = bufR[i] * keep * sh;
        sendL[i] *= roomGain;
        sendR[i] *= roomGain;
        echoL[i] *= roomGain;
        echoR[i] *= roomGain;
        ambGains[i] = roomSound.next() * presence;   // the room's own sounds, only while something plays
        ambPower[i] = 0;
    }
    environments.process(sendL, sendR, echoL, echoR, dryL, dryR, ambGains, spaces, mixL, mixR, ambPower, n, roomGains);
    if (automaticLevel) {
        // (What comes out carries the make-up already: measured without it, the room's own level.)
        for (int i = 0; i < n; ++i) {
            const float unmade = 1.0f / std::max(1.0e-3f, madeUp[i]);
            const float ml = mono ? 0.5f * (mixL[i] + mixR[i]) : mixL[i], mr = mono ? ml : mixR[i];   // (mono: what is heard, folded)
            const float a = weigh(2, ml * unmade), b = weigh(3, mr * unmade);
            const double e = double(a * a + b * b);
            if (starting) matchSumOut += e;
            matchOut += double(matchCoef) * (e - matchOut);
        }
    } else {
        matchTarget = 1.0f;
    }
    for (int i = 0; i < n; ++i) {
        bypassPos = bypassGain.target > bypassPos ? std::min(bypassGain.target, bypassPos + bypassStep) : std::max(bypassGain.target, bypassPos - bypassStep);
        const float b = bypassPos * bypassPos * (3.0f - 2.0f * bypassPos);   // 25 ms along an S-curve: no kink at either end
        const float m = mixGain.next(), o = outGain.next();
        // The world's peaks: a gain-riding limiter (attack ~0.3 ms, release ~120 ms) keeps them under
        // -1 dBFS; the soft safety after it only rounds what still gets through.
        const float peakNow = std::max(std::abs(mixL[i]), std::abs(mixR[i])) * limiterGain;
        if (peakNow > limiterCeiling) limiterGain += limiterAttack * (limiterCeiling / std::max(peakNow, 1.0e-9f) * limiterGain - limiterGain);
        else limiterGain += limiterRelease * (1.0f - limiterGain);
        const float wl = safety(mixL[i] * limiterGain), wr = safety(mixR[i] * limiterGain);
        float l = (dl[i] + m * (wl - dl[i])) * o;
        float r = (dr[i] + m * (wr - dr[i])) * o;
        l += b * (dl[i] - l);   // Bypass: the input itself (delayed by the latency, as everything is)
        r += b * (dr[i] - r);
        if (b >= 1.0f) { l = dl[i]; r = dr[i]; }
        outL[i] = l;
        outR[i] = r;
        ambSum += double(ambPower[i]);
        outSum += double(l * l + r * r) * 0.5;
    }
}

void FxProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int total = buffer.getNumSamples();
    const int inputs = std::min(getTotalNumInputChannels(), buffer.getNumChannels());
    const int outputs = std::min(getTotalNumOutputChannels(), buffer.getNumChannels());
    updateBlockSettings();
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition()) {
            // The randomness follows the song while it plays (a bounce of the same passage sounds the same).
            const bool playing = position->getIsPlaying();
            if (playing)
                if (const auto at = position->getTimeInSamples()) {
                    const auto host = std::uint64_t(std::max<std::int64_t>(0, *at));
                    const auto gap = host > limClock ? host - limClock : limClock - host;
                    if (gap > (hostWasPlaying ? std::uint64_t(std::max(256.0, 0.1 * rate)) : 0)) {
                        limClock = host;
                        lim.relocate(limClock);
                    }
                }
            hostWasPlaying = playing;
        }
    if (inputs < 1 || outputs < 1) return;
    float* left = buffer.getWritePointer(0);
    float* right = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
    const float* inRight = inputs > 1 ? buffer.getReadPointer(1) : left;   // (mono in: the same on both sides)
    inSum = ambSum = outSum = 0;
    float inL[chunk], inR[chunk], outL[chunk], outR[chunk];
    int from = 0;
    while (from < total) {
        const int n = std::min({ chunk, total - from, controlPeriod - int(samplePosition % controlPeriod) });
        std::copy(left + from, left + from + n, inL);
        std::copy(inRight + from, inRight + from + n, inR);
        renderSegment(inL, inR, outL, outR, n, right == nullptr);
        if (right != nullptr) {
            std::copy(outL, outL + n, left + from);
            std::copy(outR, outR + n, right + from);
        } else {   // mono out: the world in stereo, folded back
            for (int i = 0; i < n; ++i) left[from + i] = 0.5f * (outL[i] + outR[i]);
        }
        from += n;
    }
    for (int ch = std::max(1, outputs); ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, total);
    if (total > 0) {
        meters.instrument.store(float(std::sqrt(inSum / total)), std::memory_order_relaxed);
        meters.ambience.store(float(std::sqrt(ambSum / total)), std::memory_order_relaxed);
        meters.output.store(float(std::sqrt(outSum / total)), std::memory_order_relaxed);
    }
    const auto& chain = environments.current();
    meters.place.store(int(environments.place()), std::memory_order_relaxed);
    meters.character.store(chain.characterIndex(), std::memory_order_relaxed);
    meters.events.store(chain.ambienceEvents(), std::memory_order_relaxed);
    const float lamp = chain.lamp();
    if (lamp < meters.lamp.load(std::memory_order_relaxed)) meters.lamp.store(lamp, std::memory_order_relaxed);
    meters.flickers.store(chain.flickers(), std::memory_order_relaxed);
    meters.activity.store(chain.evolution().movement(), std::memory_order_relaxed);
}

// A host's own bypass (when it does not use the Bypass parameter): the input, delayed by the same
// latency, so the track stays in time.
void FxProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    midi.clear();
    const int total = buffer.getNumSamples();
    const int inputs = std::min(getTotalNumInputChannels(), buffer.getNumChannels());
    const int outputs = std::min(getTotalNumOutputChannels(), buffer.getNumChannels());
    if (dryLineL.empty() || inputs < 1 || outputs < 1) return;
    float* left = buffer.getWritePointer(0);
    float* right = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
    const float* inRight = inputs > 1 ? buffer.getReadPointer(1) : left;
    for (int i = 0; i < total; ++i) {
        dryLineL[std::size_t(dryWrite)] = left[i];
        dryLineR[std::size_t(dryWrite)] = inRight[i];
        left[i] = dryLineL[std::size_t((dryWrite - latency) & dryMask)];
        if (right != nullptr) right[i] = dryLineR[std::size_t((dryWrite - latency) & dryMask)];
        dryWrite = (dryWrite + 1) & dryMask;
    }
}

//==============================================================================
// Presets, A/B, undo.
void FxProcessor::setParameter(const juce::String& id, float value) {
    if (auto* p = parameters.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    }
}

void FxProcessor::resetToDefaults() {
    for (auto* p : getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
        const auto id = ranged->getParameterID();
        if (id == fxp::bypass || id == fxp::vhs || id == fxp::vhsWarble) continue;   // (listening / viewing preferences)
        if (ranged->getValue() == ranged->getDefaultValue()) continue;
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost(ranged->getDefaultValue());
        ranged->endChangeGesture();
    }
}

FxProcessor::Snapshot FxProcessor::snapshot() const {
    Snapshot s;
    for (auto* p : getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
        s.values.push_back(ranged->getParameterID() == fxp::bypass ? -1.0f : ranged->getValue());   // (Bypass is not part of a setting)
    }
    s.name = currentPresetName;
    return s;
}

void FxProcessor::restore(const Snapshot& s) {
    const auto& all = getParameters();
    for (int i = 0; i < all.size() && std::size_t(i) < s.values.size(); ++i) {
        if (s.values[std::size_t(i)] < 0.0f) continue;
        auto* p = all[i];
        if (p->getValue() == s.values[std::size_t(i)]) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost(s.values[std::size_t(i)]);
        p->endChangeGesture();
    }
    currentPresetName = s.name;
}

namespace {
struct Step final : juce::UndoableAction {
    explicit Step(std::function<void(bool)> f) : apply(std::move(f)) {}
    bool perform() override { apply(true); return true; }
    bool undo() override { apply(false); return true; }
    std::function<void(bool)> apply;
};
} // namespace

void FxProcessor::undoable(const juce::String& name, const std::function<void()>& change) {
    auto before = snapshot();
    const int abBefore = ab;
    change();
    auto after = snapshot();
    const int abAfter = ab;
    if (after.values == before.values && after.name == before.name && abAfter == abBefore) return;
    undoManager.beginNewTransaction(name);
    undoManager.perform(new Step([this, before = std::move(before), after = std::move(after), abBefore, abAfter](bool forward) {
        restore(forward ? after : before);
        ab = forward ? abAfter : abBefore;
    }), name);
}

bool FxProcessor::undo() { return undoManager.undo(); }
bool FxProcessor::redo() { return undoManager.redo(); }

void FxProcessor::initPreset() {
    undoable(initName, [this] {
        resetToDefaults();
        currentPresetName = initName;
    });
}

void FxProcessor::applyPreset(const presets::Preset& preset) {
    undoable(preset.name, [this, &preset] {
        resetToDefaults();
        for (const auto& [id, value] : preset.values) setParameter(id, value);
        currentPresetName = preset.name;
    });
}

void FxProcessor::showAB(int which) {
    which = std::clamp(which, 0, 1);
    if (which == ab) return;
    undoable(which == 0 ? "A" : "B", [this, which] {
        abStore[std::size_t(ab)] = snapshot();   // what is shown now is kept in its slot
        abStored[std::size_t(ab)] = true;
        ab = which;
        if (abStored[std::size_t(which)]) restore(abStore[std::size_t(which)]);   // (never stored: B starts as a copy of A)
        abStored[std::size_t(which)] = true;
    });
}

void FxProcessor::copyAB() {
    abStore[std::size_t(1 - ab)] = snapshot();
    abStored[std::size_t(1 - ab)] = true;
}

int FxProcessor::getNumPrograms() { return 1 + int(presets::factory().size()); }
const juce::String FxProcessor::getProgramName(int index) {
    if (index <= 0 || index > int(presets::factory().size())) return "Init";
    return presets::factory()[std::size_t(index - 1)].name;
}
void FxProcessor::setCurrentProgram(int index) {
    currentProgram = std::clamp(index, 0, int(presets::factory().size()));
    if (currentProgram == 0) initPreset();
    else applyPreset(presets::factory()[std::size_t(currentProgram - 1)]);
}

juce::File FxProcessor::userPresetFolder() { return brand::appDataFolder().getChildFile("Presets"); }

juce::File FxProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = juce::File::createLegalFileName(name.trim().isEmpty() ? juce::String("Untitled") : name.trim());
    auto folder = userPresetFolder();
    if (!folder.createDirectory()) return {};
    juce::XmlElement xml(brand::presetTag);
    xml.setAttribute("name", clean);
    xml.setAttribute("stateVersion", stateVersion);
    for (auto* p : getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
        if (ranged->getParameterID() == fxp::bypass) continue;
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", ranged->getParameterID());
        e->setAttribute("value", double(ranged->convertFrom0to1(ranged->getValue())));
    }
    const auto file = folder.getChildFile(clean + brand::presetExtension);
    if (!xml.writeTo(file)) return {};
    currentPresetName = clean;
    return file;
}

bool FxProcessor::loadUserPreset(const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(brand::presetTag)) return false;
    const auto name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
    undoable(name, [&] {
        resetToDefaults();
        for (auto* e : xml->getChildWithTagNameIterator("PARAM")) setParameter(e->getStringAttribute("id"), float(e->getDoubleAttribute("value")));
        currentPresetName = name;
    });
    return true;
}

juce::Array<juce::File> FxProcessor::userPresets() const {
    auto files = userPresetFolder().findChildFiles(juce::File::findFiles, false, juce::String("*") + brand::presetExtension);
    files.sort();
    return files;
}

juce::AudioProcessorEditor* FxProcessor::createEditor() { return new FxEditor(*this); }

void FxProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("stateVersion", stateVersion);
    xml->setAttribute("uiScale", double(uiScale.load()));
    xml->setAttribute("preset", currentPresetName);
    xml->setAttribute("ab", ab);
    // The other of A/B (the shown one is the parameters themselves), by parameter ID.
    if (abStored[std::size_t(1 - ab)]) {
        auto* other = xml->createNewChildElement("AB");
        const auto& all = getParameters();
        const auto& s = abStore[std::size_t(1 - ab)];
        other->setAttribute("name", s.name);
        for (int i = 0; i < all.size() && std::size_t(i) < s.values.size(); ++i)
            if (s.values[std::size_t(i)] >= 0.0f)
                other->setAttribute(dynamic_cast<juce::RangedAudioParameter*>(all[i])->getParameterID(), double(s.values[std::size_t(i)]));
    }
    copyXmlToBinary(*xml, destData);
}

void FxProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    uiScale.store(float(std::clamp(xml->getDoubleAttribute("uiScale", 1.0), 0.85, 1.5)));
    currentPresetName = xml->getStringAttribute("preset", initName);
    ab = std::clamp(xml->getIntAttribute("ab", 0), 0, 1);
    abStored = { false, false };
    if (auto* other = xml->getChildByName("AB")) {
        Snapshot s;
        s.name = other->getStringAttribute("name", initName);
        for (auto* p : getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
            const auto id = ranged->getParameterID();
            s.values.push_back(id == fxp::bypass ? -1.0f : float(other->getDoubleAttribute(id, double(ranged->getDefaultValue()))));
        }
        abStore[std::size_t(1 - ab)] = s;
        abStored[std::size_t(1 - ab)] = true;
        xml->removeChildElement(other, true);
    }
    parameters.replaceState(juce::ValueTree::fromXml(*xml));
    abStore[std::size_t(ab)] = snapshot();
    abStored[std::size_t(ab)] = true;
}

} // namespace liminal::fx

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new liminal::fx::FxProcessor(); }
