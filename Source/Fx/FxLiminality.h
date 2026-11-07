#pragma once
// LIMINALITY for audio (VACANCY FX, 2026-11-06). The instrument's stage (DSP/Liminality) acts on
// notes - a detuned key, a sour note, a note held back - and on the instrument's output. An effect
// has no notes, so here every control is done to the audio itself:
//
//   Detune       a subtle constant pitch shift (flat, like a slightly slow tape; up to ~28 cents at
//                100 %, the two sides a few cents apart)
//   Drift        the pitch wanders away over seconds and comes back (the instrument's own curve)
//   Wow & Flutter  tape wow (~0.6-0.9 Hz) and flutter (~7-12 Hz) (the instrument's own curve)
//   Warble       each side wavers on its own, seasick; plus the instrument's slow swell and faint
//                chorus copy, growing back after each attack
//   Ghost        a faint copy, delayed (~55-95 ms) and detuned a constant few cents (the instrument's)
//   Wear         tape / VHS degradation: lost highs and lows, soft saturation, hiss and crackle under
//                the playing, the occasional dropout, level-matched (the instrument's)
//   Timing       the delayed and wet paths (the room and echo sends) smeared and jittered: their
//                delay jumps (cross-faded, never glided: no pitch artefacts) between 0 and ~45 ms,
//                with a little diffusion
//   Unevenness   slow fluctuation of level (up to about +/-3.5 dB) and tone (a tilt)
//   Sour Notes   occasional short pitch sags (30-60 cents down, a quarter to half a second), most
//                often right on an attack, as if that note came out sour
//   Fade         the sound occasionally fades away (and darkens), then returns
//
// The pitch controls share one pitch line per side: a delay read at a moving position (its speed
// is the pitch), centred on the plug-in's reported latency, so the shifted sound stays in time
// with the track on average. When the read position reaches the edge of its +/-range it jumps back
// to the centre, cross-faded over ~25 ms. At every amount 0 the line settles exactly on the centre
// (an integer delay: the input delayed by the latency, bit for bit) and the rest is bypassed.
// All randomness is a pure function of (seed, the stage's clock); fixed storage (prepare), no
// allocation or locks in the audio path.
#include "DSP/Filters.h"
#include "DSP/Liminality.h"
#include <array>
#include <cstdint>
#include <vector>

namespace liminal::fx {

// A mono delay read at a moving position (the pitch), wrapping back to the centre with a cross-fade.
class PitchLine {
public:
    void prepare(int centreSamples, int rangeSamples, int crossfadeSamples);
    void reset();
    // `ratio`: the playback speed now (1 = unshifted).
    float process(float x, double ratio);
    double delay() const { return d1; }
    bool atCentre() const { return !fading && d1 == double(centre); }
    int wraps() const { return wrapCount; }

private:
    float read(double delay) const;
    std::vector<float> line;
    int mask = 0, write = 0, centre = 0, range = 0, fadeLength = 1, fadePos = 0, wrapCount = 0, searchLength = 256;
    double d1 = 0, d2 = 0;
    bool fading = false;
};

// Two delay heads that jump (cross-faded) between random delays: Timing on one send channel.
class Jitter {
public:
    void prepare(int maxSamples, int crossfadeSamples);
    void reset();
    void setTarget(int samples) { next = samples; }
    float process(float x);
    bool idle() const { return !fading && now == 0 && next == 0; }

private:
    std::vector<float> line;
    int mask = 0, write = 0, now = 0, from = 0, next = 0, fadeLength = 1, fadePos = 0;
    bool fading = false;
};

class AudioLiminality {
public:
    // `centre`: the plug-in's latency in samples (the pitch line's centre).
    void prepare(double sampleRate, int centre);
    void reset();
    void setAmounts(const dsp::LiminalAmounts& effective) { target = effective; lim.setAmounts(effective); }
    void setSeed(std::uint32_t s) { seed = s; lim.setSeed(s); }
    void setVhsWarble(float w) { vhsWarble = w; }   // the picture's tape struggling (0..1): a faint warble with it
    void relocate(std::uint64_t clock);
    // Every 64 samples, on the processor's grid.
    void control(std::uint64_t clock);
    void onset(float strength);   // an attack in the input (0..1)
    // The main path, in place (at most 64 samples, never across a control tick): pitch line,
    // Ghost / Warble / Wear, Unevenness, Fade. Delays the sound by the centre (the latency).
    void process(float* left, float* right, int n);
    // Timing, in place, on the room (and echo) sends.
    void processSends(float* roomL, float* roomR, float* echoL, float* echoR, int n);

    // For tests and the interface.
    float centsNow(int side) const { return side == 0 ? centsL : centsR; }
    float sagNow() const { return sag; }
    int sags() const { return sagCount; }
    int fades() const { return fadeCount; }
    float fadeNow() const { return fadeGain; }
    // The level changes the stage makes on purpose (Fade, Unevenness): the level match leaves them alone.
    float intendedGain() const { return quiet ? 1.0f : fadeGain * levelGain; }
    bool fading() const { return fadeStage != FadeStage::idle; }
    int wraps() const { return pitchL.wraps() + pitchR.wraps(); }
    int dropouts() const { return lim.dropouts(); }
    bool bypassed() const { return quiet; }
    const dsp::LiminalAmounts& amounts() const { return lim.amounts(); }

private:
    std::uint32_t salt(std::uint32_t k) const { return dsp::lim::hash(seed, k); }
    double sr = 48000.0;
    int centre = 960;
    std::uint32_t seed = 1;
    dsp::LiminalAmounts target {};
    dsp::Liminality lim;   // the instrument's audio side: Ghost, Warble (swell, chorus), Wear, dropouts
    PitchLine pitchL, pitchR;
    std::array<Jitter, 4> jitter;
    std::array<dsp::Biquad, 8> smear;   // Timing's diffusion: two allpasses per send channel
    float smearMix = 0, smearTarget = 0;
    std::uint64_t clockNow = 0;
    // Pitch, per sample interpolated from the control ticks.
    float centsL = 0, centsR = 0, stepL = 0, stepR = 0, vhsWarble = 0;
    // Sour Notes: the sag's envelope.
    enum class SagStage { idle, down, hold, up };
    SagStage sagStage = SagStage::idle;
    float sag = 0, sagDepth = 0, sagRate = 0, sinceSag = 100, sinceOnset = 100;
    int sagHold = 0, sagCount = 0;
    // Unevenness and Fade (per-sample smoothed gains, a tilt and a closing low-pass).
    float levelGain = 1, levelStep = 0, tilt = 0, tiltStep = 0;
    dsp::OnePole tiltL, tiltR;
    enum class FadeStage { idle, down, hold, up };
    FadeStage fadeStage = FadeStage::idle;
    float fadeGain = 1, fadeDepth = 1, fadeSpeed = 0, untilFade = 10, fadeHold = 0, fadeLpNow = 1, fadeLpStep = 0;
    float fl1 = 0, fl2 = 0, fr1 = 0, fr2 = 0;
    int fadeCount = 0;
    bool quiet = true;
};

} // namespace liminal::fx
