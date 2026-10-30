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
