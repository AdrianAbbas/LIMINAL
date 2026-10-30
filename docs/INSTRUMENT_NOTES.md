# Instrument notes (Milestone 1)

How LIMINAL's instruments are built, and the acoustics behind the numbers. Code:
`Source/DSP/Instrument.h` (shared voice handling, the mode bank), one file pair per
instrument in `Source/DSP/`, and `Source/DSP/Distance.h`. Tests: `Tests/InstrumentTests.h`
(`LiminalTests --only piano,synth,glass`).

How sure each number is:
- *(web)* — checked during this milestone against the source named (often through a
  search summary: this environment's network policy blocks most publisher and university
  pages, e.g. speech.kth.se, so full papers could not be opened).
- *(ref)* — from standard references (Fletcher & Rossing, *The Physics of Musical
  Instruments*; Askenfelt (ed.), *Five Lectures on the Acoustics of the Piano*, KTH) as
  remembered, not re-checked here. Verify against a recording first.
- *(estimate)* — chosen by reasoning to fit the facts above; no measurement behind it.

WORLDBUILDER's `docs/INSTRUMENT_NOTES.md` (AdrianAbbas/Worldbuilder) was read first; its
modal-object and tine tables are the starting point for Milestone 1b (Music Box), and its
lesson "decays are T60s, not time constants" applies throughout: every decay below is
the time to fall 60 dB.

**No forced pitch glides.** No instrument here starts a note off its pitch or slides into
it. Pitch instability (Age, Drift, Detune) is a small wander *around* the note (a few
cents, sub-hertz) that starts wherever it is, and per-key/per-note offsets are fixed.
The only slide is the Faded Synth's optional Glide, off by default. The tests check the
pitch at the start of a note against a second later for every instrument.

---

## Forgotten Piano

**Model** (`dsp::ForgottenPiano`): modal synthesis of a struck stiff-string unison.

| Aspect | What the model does | Basis |
|---|---|---|
| Partials | Up to 32, at `f_n = n f0 sqrt(1 + B n^2)`; none above 16 kHz or 0.45 x the sample rate | Stiff-string inharmonicity *(ref; formula confirmed (web): DAFx 2004 inharmonicity paper)* |
| Stiffness B | 2.5e-4 at A0, 1.5e-4 at E2 (wound strings), 4e-4 at C4, 2.5e-3 at C6, 1.5e-2 at C8, log-interpolated; x(1 + 0.5 Age) | Bass "1e-4 to 1e-3" and "about 0.0002 in the bass" *(web: auditory.org list; arXiv 1203.5101)*; the arXiv paper's "up to 0.4 in the treble" was not used (implausibly high for C8 — it would put the 2nd partial 5 semitones sharp); mid/treble values *(ref: Fletcher & Rossing)*. Test: C6's 2nd partial measures 6.4 cents sharp, the 4th 32 cents. |
| Two-stage decay | Each partial = a "prompt" mode plus a slower "aftersound" pair (the unison's normal modes) | Weinreich: strings of a unison coupled through the bridge; the in-phase mode decays fast (prompt sound), the out-of-phase one slowly (aftersound); mistuning between the strings shapes the envelope, beating, and double decay *(web: J.O. Smith, PASP "Coupled Strings"; Southampton/Valiente 2024; Baxandall, Brown & Gough, IOA)*. JOS notes that just summing detuned strings beats but does **not** give a true two-stage decay — hence separate decay rates for the prompt and aftersound modes here. |
| Prompt / aftersound split | Aftersound 30 % of the amplitude (+ softness, age, treble); prompt T60 = aftersound / 5 for the fundamental, closer for upper partials and in the treble | Ratio *(estimate, consistent with "fast onset decay... usually within the first second", web: kevinhunt.com.au)*. Measured C4: 33 dB/s over the first half second, 5.4 dB/s in the aftersound. |
| Unison mistuning | 0.6 cents x (0.5..1.5) per key at Age 0 / Detune 0, up to ~15 cents at Detune 1; some keys of an aged piano 4x off (false beats); single bass strings only a third of it (two polarisations) | Typical cents not found online *(estimate)*; "slight mistuning between strings" *(web: IOA)* |
| Sustain (T60) | Fundamental `24 s x (110 Hz / f0)^0.7`, 1.2–45 s (C4 ~13 s, C7 ~3 s); per partial `1/T = 1/T0 + 0.18 (f/1 kHz)^1.5` | Higher partials and higher notes die faster *(ref; web: ICASSP 2015 decay study confirms per-partial linear-in-dB decay and that dynamics barely change decay rate)*; numbers *(estimate)* |
| Hammer | Force pulse of contact time `tau` low-passes the spectrum: corner `1.8 / tau`, slope steeper for soft blows | Contact ~4 ms in the bass to <1 ms in the treble; varies ~+/-20 % from pp to ff; harder blows shorter *(web: KTH "string contact" lecture summary; Hall & Askenfelt)*. Felt is a nonlinear spring, force ~ compression^p, p 2.2–3.5 for used hammers, 1.5–2.8 new *(web: Russell, "The piano hammer as a nonlinear spring", PSU)* — modelled as the velocity-dependent slope. Test: centroid 522 Hz at velocity 35, 685 Hz at 120. |
| Strike point | Comb `|sin(n pi beta)|`, beta = 0.12 +/- 0.02 per key | Hammers strike at about 1/7–1/9 of the string *(ref)* |
| Dampers | Release T60 0.12 s (treble) to 0.47 s (bass), x(1 + 2.5 Age) for worn felts; no dampers from F6 (MIDI 89) up | Top ~1.5 octaves undamped *(ref; web: Pianoteq forum notes "a point in the high register where there is no more damper")* |
| Sustain pedal | Lifts the dampers; six open strings (A1 C2 E2 G2 A2 C3, comb filters, T60 4 s) ring in sympathy while it is down | Sympathetic resonance of undamped strings *(ref)*; string choice *(estimate)* |
| Mechanical noise | Key-bed thump (95–135 Hz knock), hammer knock (1.4–2.3 kHz noise), damper "thup" (380–580 Hz) on release, pedal thump and squeak | "The mechanical action of the pedals or dampers also makes a noise that has become part of the piano's tone" *(web: historical listening-test report, univie.ac.at)*; frequencies *(estimate)* |
| Age | Keys out of tune with one another (+/- up to 6 cents), wider unisons and some false-beating keys, uneven voicing (+/-50 %) and sustain, duller soundboard (-35 % sustain), worn dampers, 0.3–2.8 cents pitch wander | *(estimate)* — the "forgotten" character |
| Softness | Longer contact (x up to 2.2), steeper roll-off, more aftersound (as with the una corda: the pedal "lengthens the decay relative to the attack", web: Southampton) | |
| Polyphony | 20 sounding notes + 6 fading; a restruck note fades in 30 ms; stealing the quietest in 5 ms | |

**Not modelled** (yet): longitudinal string modes and phantom partials (the bass's
"metallic" bite), the soundboard's own impulse response, duplex scaling, hammer
re-bounce at fortissimo. Milestone 8 can layer the developer's piano recordings.

**Sources (web):** J.O. Smith, *Physical Audio Signal Processing*, "Coupled Strings"
(https://ccrma.stanford.edu/~jos/pasp/Coupled_Strings.html); Valiente et al. 2024, J. Phys.
Conf. Ser. 2909 012036 (https://eprints.soton.ac.uk/499445/); Baxandall, Brown & Gough,
*Vibrations of piano strings and coupling to the soundboard*, IOA
(https://www.ioa.org.uk/); D. Russell, *The piano hammer as a nonlinear spring*
(https://www.acs.psu.edu/drussell/piano/nonlinearhammer.html); KTH, *Five lectures on the
acoustics of the piano* (https://www.speech.kth.se/music/5_lectures/ — summary only);
Cheng, Dixon & Mauch, ICASSP 2015, *Modelling the decay of piano sounds*
(https://webspace.eecs.qmul.ac.uk/s.e.dixon/pub/2015/ChengDixonMauch-ICASSP2015-Decay.pdf);
DAFx 2004 inharmonicity estimation (https://dafx.de/paper-archive/2004/P_212.PDF);
*Entropy-based tuning of musical instruments* (https://arxiv.org/pdf/1203.5101).

---

## Faded Synth

**Model** (`dsp::FadedSynth`): subtractive "virtual analog" with the faults of old
electronics kept on purpose.

| Aspect | What the model does | Basis |
|---|---|---|
| Oscillators | Two sawtooths and a pulse (levels 0.5 / 0.5 / 0.35), band-limited with PolyBLEP, free-running (random phase per note) | Standard analog-polysynth voice *(ref)*; PolyBLEP: Välimäki & Huovilainen anti-aliasing *(ref)* |
| Detune | Saws at -/+ half the spread, the pulse at +0.15; spread 0–24 cents (`24 x^1.5`, default 5 cents) | *(estimate)*; test: beating doubles from Detune 0 to 1 |
| Drift | Each oscillator wanders on its own (~0.25 Hz, up to 7 cents at Drift 1), a shared slow wow (0.6 Hz, up to 6 cents at Drift 1, quadratic so the default is subtle), cutoff wander up to 15 % | Listeners notice fluctuations around a cent or less; "2 cents at 1 kHz ~ 0.1 Hz"; VCO and ladder-filter cutoff drift with temperature *(web: synth-diy list "Analogue Drift" thread; Analogue Systems RS-100 manual)*. Default Drift 0.3: ~2 cents per oscillator, 0.5 cents of wow. Test: 0.6 -> 1.9 cents RMS. |
| Slow modulation | Pulse width 0.5 +/- 0.18 at 0.22–0.42 Hz per voice; the same LFO moves the cutoff +/-4 % | *(estimate)* |
| Warm saturation | `tanh`-like (Padé) input stage, drive 1–4 with Warmth, small bias for even harmonics | OTA/ladder filters have a tanh term in their transfer function: soft saturation; Moog ladder and OTA input stages behave similarly; cutoff shifts with drive *(web: synth-diy "best VC drive approach" thread on the Juno-106 filter)* |
| Filter | Four one-pole stages (24 dB/octave) with feedback 0.55 (soft resonance); cutoff 9 kHz (Warmth 0) to 500 Hz (Warmth 1), key tracking `(f/261.6)^0.6`, envelope 0.8–2.4 octaves with velocity (decays to 15 % in ~1.2 s) | Ladder topology *(ref: Moog; Stilson & Smith)*; values *(estimate)*. Test: Warmth 0 -> 1 halves the centroid. |
| Envelopes | RC-style: attack charges toward 1.3 and stops at 1 (the analog curve), decay to 0.8 over ~1.6 s, release T60 = Release; Attack 1 ms–4 s, Release 10 ms–6 s | *(ref: analog ADSR behaviour)* |
| Width / chorus | Voices alternate left/right (up to +/-0.6), and a bucket-brigade-style chorus: 3.5 +/- 1.8 ms at 0.55 Hz, darkened, cross-fed, the right sweep offset by up to half a cycle; mix 12–50 % | Juno chorus: MN3009 BBD, "only a very limited range of the chip"; the delay range 0.64–12.8 ms is the chip's; clone presets list I+II at ~1 Hz, 8 % depth; emulations sweep 0–1.2 Hz *(web: SoundForce Chorus 6 manual via ModularGrid; Plugin Boutique)*. Roland's own figures were not found. |
| Glide | Optional (`Synth Glide`, **off by default**), 10 ms–2 s, from the previous note | Brief: any slide into pitch must be an option, default off |
| Polyphony | 12 + 4 fading; restrike 12 ms; stealing 5 ms | |

**Sources (web):** synth-diy mailing list, "Analogue Drift" (https://synth-diy.org/pipermail/synth-diy/2012-November/141794.html)
and "best VC drive approach" (https://synth-diy.org/pipermail/synth-diy/2022-February/170149.html);
Analogue Systems RS-100 manual (https://www.analoguesystems.co.uk/images/AS_manuals/RS100S.pdf);
SoundForce Chorus 6 (https://modulargrid.net/e/soundforce-chorus-6-); Feedback Chorus
(https://www.pluginboutique.com/products/17837-Feedback-Chorus).

---

## Glass Memories

**Model** (`dsp::GlassMemories`): modal synthesis of a struck glass, with an optional
rubbed-rim layer.

| Aspect | What the model does | Basis |
|---|---|---|
| Mode ratios | (2,0) … (7,0) bending modes at **1 : 1.86 : 3.25 : 4.95 : 7.0 : 9.35** of the played pitch | A modal struck-glass model uses 1206, 2240, 3919, 5965 Hz = 1 : 1.86 : 3.25 : 4.95 *(web: ChucK book listing 7.10, via search summary; the page itself is blocked here)*. Rossing's glass harmonica: the (5,0) mode at 5997 Hz sits next to the 5th harmonic of (2,0) (5990 Hz), and (6,0) at 8423 Hz next to its 7th (8380 Hz), i.e. (5,0) ≈ 5.0 and (6,0) ≈ 7.04 x (2,0) *(web: Rossing, "Acoustics of the glass harmonica", JASA 95(2) 1994, 1106–1111, via summary)* — consistent with 4.95 and 7.0. The (7,0) value 9.35 is extrapolated from the spacing *(estimate)*. WORLDBUILDER's table (1 : 2.32 : 4.25 : 6.63, from a Csound table, unverified) was **not** used: it disagrees with both sources. "The ratios of the resonance frequencies are different for every glass" *(web: BU GK-12 wine glass page)* — hence the per-strike irregularity with Fragility. Test: measured 1.860, 3.250, 4.951. |
| Doublets | Every mode is two modes 0.08–1.5 Hz apart (scaled by `sqrt(f / 1 kHz)` and Fragility^2), panned apart | Real glasses are not perfectly round; the degenerate mode pairs split and beat *(ref: Rossing; web: "frequency matching of orthogonal wineglass modes", IUCC; WORLDBUILDER notes on singing-bowl doublets)*; split values *(estimate)* |
| Strike | Raised-cosine pulse 4 ms (soft mallet) to 0.16 ms (hard tick), from Brightness and velocity; spectrum `1/sqrt(1 + (1.5 f T)^4)`; plus a few ms of high-passed noise (the tick) with Brightness x Fragility | "Tapping excites a number of vibrational modes" while rubbing excites (2,0) *(web: Terwagne & Bush, singing-bowl paper, arXiv 1106.5657)*; raised-cosine strike from WORLDBUILDER's modal objects; numbers *(estimate)* |
| Ring | Lowest mode T60 1.2–20 s (Decay; default ~7 s around A5, `(880/f)^0.3` by pitch), upper modes `ratio^-0.8` shorter | Glass has very high Q: a 1 s decay is ~0.3 Hz bandwidth *(web: Dartmouth problem set; Q ≈ π f τ)*; values *(estimate)* |
| Release | Key up: a finger on the rim, ring x0.4 (at least 0.4 s); the pedal keeps it ringing | *(estimate)*; long tails stay musical in sparse melodies without smearing fast ones |
| Resonance | While held, the rubbed-rim sound swells in (~0.35 s): the lowest mode and its 2nd and 3rd harmonics (1 / 0.22 / 0.08), doublets, with a 3 Hz stick-slip unevenness; fades 0.8 s after release. Also lengthens the lowest mode's ring up to 2x | "Rubbing with a wet finger tends to excite only the (2,0) mode and its harmonics through a stick-slip process" *(web: BU wine glass page; Terwagne & Bush)* |
| Fragility | Wider doublets (shimmer), +/-30 % strike-to-strike mode strengths, +/-0.6 % per mode in ratio, a louder tick, and two short "crack" partials (2.4–2.7x and 5.6–6.2x) | *(estimate)* — the "fragile" character |
| Detune | Each strike off by up to +/-15 cents (random, fixed for the note), then a +/-6 cent wander at ~0.4 Hz; never a slide | Brief: adjustable pitch instability, no forced glides |
| Polyphony | 16 + 6 fading; a restruck note fades in 40 ms | |

**Sources (web):** Rossing, *Acoustics of the glass harmonica*, JASA 95 (1994)
(summary; ASA press-room lay version: https://acoustics.org/pressroom/httpdocs/147th/Rossing_Harmonicas1.htm);
BU GK-12, *Wine glass* (https://www.bu.edu/gk12/miked/wineglass.html); Terwagne & Bush,
*Tibetan singing bowls* (https://arxiv.org/pdf/1106.5657); Arane et al., *Coupling between two
singing wineglasses* (https://arxiv.org/pdf/0901.0656); ChucK, *Programming for Musicians and
Digital Artists*, listing 7.10 (https://chuck.cs.princeton.edu/doc/examples/book/digital-artists/chapter7/Listing7.10.ck — summary only);
Dartmouth Math 5 problem set (https://math.dartmouth.edu/~m5f11/7.pdf).

---

## Milestone 1b instruments (quick versions)

The developer's direction for 1b: functional, distinct and musically usable, not polished —
most instruments will later be replaced or layered with the developer's own samples
(Milestone 8), and the environment is meant to be the main source of sound transformation.
So these three use well-known textbook models with sensible numbers and no deep research.

### Empty Choir

Source-filter synthesis: four sawtooth "singers" per note (PolyBLEP, one-pole spectral
tilt of roughly -6 dB/oct above 0.9–2.7 kHz depending on velocity, plus breath noise) into
four band-pass formants per ear. Formant table (Hz / bandwidth Hz / level dB), alto-ish,
after the commonly quoted Peterson & Barney averages and the Csound manual's vowel
formant table *(ref; levels adjusted by ear-free judgement, estimate)*:

| vowel | F1 | F2 | F3 | F4 |
|---|---|---|---|---|
| oo | 350/60/0 | 600/80/-10 | 2400/120/-28 | 3300/150/-32 |
| oh | 450/70/0 | 800/80/-6 | 2830/100/-24 | 3500/130/-30 |
| ah | 800/80/0 | 1150/90/-4 | 2900/120/-20 | 3900/130/-30 |
| eh | 400/60/0 | 1600/80/-10 | 2700/120/-12 | 3300/150/-20 |
| ee | 300/50/0 | 2000/100/-10 | 2900/120/-10 | 3500/130/-16 |

Vibrato 4.6–5.4 Hz, 5 cents, starting after 0.5 s (choral vibrato is typically 5–6 Hz;
a choir's blend makes it shallow) *(estimate)*. Right-ear formants 2 % higher (a different
throat per side). No speech, no consonants.

### Lost Music Box

A comb tooth is a clamped-free (cantilever) bar: Euler–Bernoulli bending modes at
1 : 6.267 : 17.55 of the fundamental *(ref: standard beam result, β·L = 1.875, 4.694,
7.855)*. Real teeth carry a lead tip mass and are tuned, so the upper ratios vary
*(ref, qualitative)*; here they vary ±1.5 % per tooth with Wear. A faint 2nd and 3rd
harmonic (7 % and 2.5 %) stand for the large-amplitude nonlinearity *(estimate)*. Ring
0.5–6 s around A5, shorter for higher modes (t60 ∝ ratio^-0.6 / ^-0.7) *(estimate)*.
Wear: a fixed mistuning per tooth (up to ±28 cents), a few "dead" teeth (short, dull),
uneven levels and a slowly beating doublet (a bent tooth). Pitch Drift: the spring motor —
one shared slow wander (±18 cents x amount, about 0.2 Hz) plus a small 2.5 Hz flutter.
Mechanism: the pin's click (15 ms band-passed noise) and the clockwork's irregular ticks.
Teeth have no dampers here: a note rings out whether the key is held or not.

### Hollow Drone

Subtractive: 2–7 free-running oscillators (Density, the last fading in), each morphing
from a near-sine to a PolyBLEP sawtooth (Harmonics), spread up to ±18 cents, each with its
own slow drift (Instability, up to ±23 cents); a fifth and an octave above come in with
Harmonics; a sine an octave below and, above Sub 50 %, two octaves below. A resonant
state-variable low-pass per ear (cutoff 1.5–15.5x the note, Q 1.2–4.2) sweeps up to ±1.2
octaves with Movement, which also circles each oscillator through the stereo field.
Attack 5 ms–12 s (default 1.2 s), release 50 ms–20 s (default 4 s). Level normalised by
1/sqrt(stack) so Density makes it fuller rather than much louder.

---

## Distance (Piano, Glass and Music Box)

`dsp::Distance`: direct sound down to -7 dB, two one-pole low-passes from 18 kHz down to
900 Hz, four early reflections (7–17 ms, cross-fed, 30 %) and up to 2.5x more send to the
room. Air absorbs high frequencies more with distance *(ref)*; in an empty building the
far source is also heard around corners and through doors, which is why it darkens far
more than open air would *(estimate)*. Transparent at 0.
