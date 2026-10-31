# LIMINAL — Environment acoustics research

Research notes on the ten LIMINAL places. Each place gets its physical facts, a confirmed or
corrected "signature" phenomenon, and concrete DSP suggestions.

**How to read the numbers.** Every figure comes from one of three kinds of source, and is
labelled:

- **[src]**: taken from a cited source (paper, standard, manufacturer data, consultancy).
- **[calc]**: computed here from a stated formula (Sabine, 2d/c, mass-air-mass, etc.) using
  assumed dimensions. These are only as good as the assumptions.
- **[est]**: an engineering judgement or rule of thumb. No measurement exists for it.

Speed of sound c = 343 m/s (20 °C) throughout. Method note: the sources were found with web
search. Many publisher hosts (ioa.org.uk, wikipedia, commercial-acoustics.com) could not be
opened from this environment, so several citations rest on the abstract or search snippet,
not the full text. Check the values that matter most against the originals.

---

## 0. General acoustics toolkit

### 0.1 RT60 and its definitions
- RT60 is the time for the sound level to fall 60 dB after the source stops. In practice it is
  extrapolated from a shorter part of the Schroeder decay curve: **T20 = 3·(t₋₂₅ − t₋₅)** and
  **T30 = 2·(t₋₃₅ − t₋₅)** [src: Jens Hee, *Reverberation time measurements*,
  https://jenshee.dk/signalprocessing/rtmeas.pdf]. **EDT** (early decay time) uses the first
  10 dB and tracks perceived reverberance more closely.
- RT is reported per octave band, 125 Hz–4 kHz. A single figure usually means the 500 Hz/1 kHz
  mean [src: ISO 3382-2 scope, https://www.asn.sn/en/node/41823; NTI Audio,
  https://www.nti-audio.com/en/applications/room-building-acoustics/reverberation-time].
- Low bands are hard to measure. Impulsive sources excite them poorly, and ISO 3382-1 requires
  bandwidth × RT > 16 [src: Norsonic,
  https://www.norsonic.com/articles/impulsive-method-for-reverberation-time-measurement].

### 0.2 Sabine and Eyring
- **Sabine:** T = 0.161·V / A, where A = Σ Sᵢαᵢ (m²). **Eyring:** T = 0.161·V / (−S·ln(1−ᾱ)).
  Sabine overestimates RT once ᾱ exceeds roughly 0.2–0.3, and Eyring is preferred there.
  Both assume a diffuse field [src: MetricGate summary of Sabine/Eyring,
  https://metricgate.com/docs/reverberation-time-rt60-sabine-eyring/; Beranek, via
  https://prosoundtraining.com/course_docs/course320/sabineandeyringeq.pdf].
- **Air absorption term:** T = 0.161·V / (A + 4mV), where m is the energy attenuation
  coefficient of air. The term matters in large rooms and at high frequency [src: HyperPhysics,
  https://hyperphysics.gsu.edu/hbase/Acoustic/revmod.html]. In big rooms, doubling the size
  raises HF RT by much less than ×2 because air takes over [src: ICSI/Berkeley lecture slides,
  https://ftp.icsi.berkeley.edu/eecs225d/spr12/slides/acoustics_large.pdf].
- **Where Sabine fails:** in **long enclosures** (corridors, tunnels, platforms) and **flat
  enclosures** (car parks), the field is not diffuse. Measured EDT and RT diverge from
  prediction, especially at long source–receiver distances [src: Nowicka, Warsaw metro,
  https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-3dae7586-2f9b-4ab7-a4b6-76da87b3500b/c/Nowicka.pdf;
  Kang, long enclosures review, https://www.aivc.org/sites/default/files/airbase_10219.pdf].
  In long spaces, RT rises quickly with distance from the source to a maximum and then falls
  slowly, while SPL keeps falling along the length [src: Tang, Kang & Jin, underground
  streets,
  https://www.ioa.org.uk/system/files/proceedings/z_tang_j_kang_h_jin_sound_field_and_soundscape_in_underground_shopping_streets.pdf].
  So **distance itself should change the decay** in the corridor, subway and garage.

### 0.3 Typical absorption coefficients (octave bands 125 / 250 / 500 / 1k / 2k / 4k Hz)
These are representative textbook-type values gathered from several tables [src: Commercial
Acoustics chart, https://commercial-acoustics.com/sound-advice/sound-absorption-coefficient-chart;
UTM CARA chapter, https://people.utm.my/zaitonharon/wp-content/uploads/sites/688/2016/08/CHAPTER-5_COMPUTATION-OF-REVERBERATION-ROOM-BY-USING-CARA.4.pdf;
Sengpiel coefficient calculator, https://sengpielaudio.com/calculator-RT60Coeff.htm; Acta
Acustica model tables, https://acta-acustica.edpsciences.org/articles/aacus/full_html/2024/01/aacus230061/T2.html].
The sources disagree by up to about ×2 on some entries.

| Material | 125 | 250 | 500 | 1k | 2k | 4k | Note |
|---|---|---|---|---|---|---|---|
| Concrete, tile, terrazzo | 0.01 | 0.01 | 0.02 | 0.02 | 0.02 | 0.02 | near-perfect reflector [src] |
| Carpet on concrete | 0.02 | ~0.06 | ~0.14 | ~0.37 | ~0.60 | 0.65 | absorbs highs only [src] |
| "Acoustic tile" (generic, older table) | 0.05 | – | – | 0.56 | – | 0.32 | peaks at mid [src] |
| Modern mineral tile NRC 0.70 / 0.90 | – | – | – | – | – | – | NRC = mean of 250–2k [src: Armstrong via Knauf, https://knauf.com/en-IN/p/product/armstrong-classic-max-24229_0240; OPTRA NRC 0.90, https://knauf.com/en-IN/p/product/armstrong-optra-15mm-29626_0240] |
| Gypsum board on studs | ~0.29 | ~0.10 | ~0.05 | ~0.04 | ~0.07 | ~0.09 | **absorbs lows** (panel resonance) [src: CA chart; Acta table gives 0.29 at 31.5–125 Hz] |
| Glass window | 0.18 | – | – | – | 0.02 | 0.02 | absorbs lows, reflects highs [src: Acta table; MTSU, https://w1.mtsu.edu/faculty/wroberts/teaching/reverb_2.php] |
| Painted plaster ceiling | 0.08 | – | – | – | – | 0.18 | [src: Acta table] |

- Water surface: no tabulated value was found. Physically, water reflects airborne sound almost
  completely: Z_air ≈ 415 rayl and Z_water ≈ 1.48 MRayl (standard values), so less than 0.2 %
  of the energy crosses the surface ([calc], §5). Treat it as α ≈ 0.01 or lower, like glazed
  tile [est].
- NRC is defined as the 250–2000 Hz average rounded to 0.05, so it says nothing about 125 Hz
  or 4 kHz [src: Knauf/Armstrong search summary above]. This matters: ceiling tiles are much
  less absorbent at 125 Hz than their NRC suggests.

### 0.4 Early vs late reflections
- The early/late split is conventionally **50 ms for speech** (C50, D50) and **80 ms for music**
  (C80) [src: McGill MUMT notes, https://caml.music.mcgill.ca/~gary/307/week3/node9.html;
  MetricGate C50/C80, https://metricgate.com/docs/clarity-c50-c80-definition-d50/]. By the
  precedence effect, early reflections fuse with the direct sound and colour it rather than
  being heard as separate events [src: techscience PDF,
  https://file.techscience.com/uploads/attached/file/20191111/20191111065114_96899.pdf]. A
  strong reflection arriving more than about 50 ms after the direct sound tends to be heard
  as a separate echo [est from the above].

### 0.5 Flutter echo
- **Definition:** a train of pulses between two parallel reflecting surfaces. The repetition
  time is **t_r = 2ℓ/c**, which reduces to ℓ/c for a listener at the midpoint. The rate c/2ℓ
  equals the first axial mode, and the axial modes are its harmonics. It needs the parallel
  pair to absorb **less** than the other surfaces, i.e. uneven absorption [src: Esselun BA
  thesis, HAW Hamburg,
  https://reposit.haw-hamburg.de/bitstream/20.500.12738/7920/1/Bachelorarbeit_A_Esselun.pdf;
  akutek, https://akutek.info/research_files/echoes.htm].
- **Perception:** when t_r < 1/20 s (50 ms), the pulses merge into a pitched "flutter tone" at
  c/2ℓ rather than a rattle. Halmrast argues flutter tones tend to settle in the 1–2 kHz
  region, i.e. the train high-passes itself as it bounces
  [src: https://www.ioa.org.uk/system/files/proceedings/t_halmrast_why_do_flutter_echoes_always_end_up_around_1-2_khz.pdf
  (title/abstract only)].
- **Key consequence for LIMINAL:** an **absorptive ceiling does not remove wall-to-wall
  flutter. It makes flutter more likely**, because it creates exactly the uneven absorption the
  definition requires. This is why acoustic ceiling tiles matter in the corridor rooms.

### 0.6 Critical distance and Schroeder frequency
- **Critical distance** d_c ≈ 0.057·√(γV/RT60), where γ is source directivity (1 =
  omnidirectional) [src: Techtile arXiv, https://arxiv.org/pdf/2204.06352; Arup Strutt,
  https://strutt.arup.com/help/Electroacoustics/DCritical.htm; Hearing Review,
  https://hearingreview.com/hearing-loss/health-wellness/critical-distance]. Beyond d_c the
  reverberant field dominates. This is the physical basis of a "distance" knob (DRR falls about
  6 dB per doubling of distance beyond d_c).
- **Schroeder frequency** f_s ≈ 2000·√(T/V): below it, single room modes dominate [src:
  Sonarworks/room-mode calculators, https://www.sonarworks.com/blog/learn/room-modes]. Axial
  mode f = n·c/2L [src: same].

### 0.7 Air absorption vs humidity (ISO 9613-1)
- ISO 9613-1 tabulates pure-tone atmospheric attenuation for 50 Hz–10 kHz, −20 to +50 °C,
  10–100 % RH [src: https://unistore.uni.com/iso-9613-1-1993]. A secondary reproduction of the
  20 °C table gives roughly: **4 kHz: 109 dB/km at 10 % RH, 48.5 at 30 %, 29.4 at 50 %;
  8 kHz: about 166 at 30 %, 104 at 50 %** [src, read from a flattened table, so verify:
  https://kwx.liehendr.com/content/wikibooks_en_all_maxi_2021-03/A/Engineering_Acoustics/Outdoor_Sound_Propagation].
- **Direction:** in the 10–60 % RH range at room temperature, **humid air absorbs high
  frequencies less than dry air** [src: preprints.org room-acoustics air-absorption paper,
  https://preprints.org/manuscript/202409.1192/v1/download; HyperPhysics table, 4 kHz:
  0.038 at 30 % vs 0.024 at 50 %]. Over a *direct* path of tens of metres the effect is small
  (50 m at 29 dB/km ≈ 1.5 dB at 4 kHz [calc]). It matters for the **reverberant tail**, whose
  path length is RT × c (a 3 s tail has travelled about 1 km). So air absorption darkens tails
  in big rooms, not direct sound.
- Converting to the Sabine air term: m ≈ α_dB/km ÷ 4343, so 4 kHz at 50 % RH gives
  m ≈ 0.0067 m⁻¹ and 4m ≈ 0.027 m⁻¹ [calc].

### 0.8 Comb filtering from multiple loudspeakers
- Two equal copies arriving Δt apart give notches at f = (2n+1)/(2Δt), spaced 1/Δt apart.
  Example: 0.894 m path difference = 2.61 ms, first notch about 192 Hz [src: Wikipedia comb
  filter, https://en.wikipedia.org/wiki/Comb_filter; Live Sound Int'l,
  https://digital.livesoundint.com/october-2019/page-52; ProSoundWeb,
  https://www.prosoundweb.com/proper-loudspeaker-placement-how-to-avoid-lobes-and-nulls/].
  Unequal levels make the notches shallower. Every listener position has its own Δt set, so
  **walking through a distributed system sweeps the combs**, which sounds like a slow flange
  [est, follows from the above].
- In distributed systems, path-length differences "disperse the direct sound in time" and
  need delay alignment [src: Start, IOA,
  https://www.ioa.org.uk/system/files/proceedings/e_start_optimisation_of_speech_intelligibility_in_multi-source_environments_using_delay_spread_minimisatio.pdf].
  A row of PA speakers along a platform adds "artificial reverberation" [src: Canadian transit
  study, https://jcaa.caa-aca.ca/index.php/jcaa/article/download/1444/1188/1578].

---

## 1. Hospital corridor

**Geometry & materials.** A long double-loaded corridor, typically about 2.4 m wide in
inpatient areas (bed-movement width) [est], 2.6–2.8 m to a suspended ceiling [est], 30–60 m
runs [est]. Floors are sheet vinyl, rubber or terrazzo. Walls are painted gypsum on studs or
painted block. Ceilings are lay-in mineral tile. Infection control pushes designers away
from carpet and textiles, so sound "bounces off tile, hardwood and steel and spills into
hallways" [src: Walls & Ceilings, https://www.wconline.com/articles/93407-sound-medicine].
Hard glazed tile or polished concrete "creates a lot of flutter echo" [src: tehqeeqat
review citing Bradshaw 2010, https://tehqeeqat.org/downloadpdf/16228]. Some immunosuppressed
wards ban fibrous tiles entirely and so end up with hard, cleanable ceilings [src: Hsu et al.,
ASA 159th, https://acoustics.org/pressroom/httpdocs/159th/hsu.htm].

**Guidance.**
- **FGI Guidelines:** all normally occupied spaces need acoustic finishes that meet a design
  room-average absorption coefficient, estimated from NRC. Suspended tile ceilings are
  allowed in public areas [src: Rockfon FGI summary,
  https://www.rockfon.com/en-us/our-story/articles-and-insights/2024/FGI-Ceiling-Requirements/].
  An older FGI table quotes ceilings at NRC ≥ 0.55 in general, ≥ 0.75 in some spaces
  [src: CAA paper, https://awc.caa-aca.ca/index.php/AWC/AWC14/paper/downloadSuppFile/59/10].
  Evidence-based design advocates NRC ≥ 0.90 [src: Rockfon, as above; vendor source].
- **WHO (1999 Guidelines for Community Noise):** wards 30 dB LAeq at night, 40 dB LAmax.
  Daytime figures are quoted as 30–35 dB. Few hospitals comply [src: WHO via nonoise.org,
  https://nonoise.org/library/whonoise/whonoise.htm; PMC review,
  https://pmc.ncbi.nlm.nih.gov/articles/PMC2443309/]. For LIMINAL this means the corridor is
  **meant to be near-silent**, so a low bed level is realistic.

**RT60.**
- Measured: in Chinese general wards, a sound-absorptive ceiling brought 500 Hz–1 kHz RT in
  patient rooms, **corridors** and nurse stations down by 13–53 % to about **0.3 s** [src: UCL,
  https://discovery.ucl.ac.uk/10178697/1/Acoustic%20treatments%20in%20general%20hospital%20wards%20in%20China_F1.pdf].
  Johns Hopkins (Weinberg 5C): absorbing panels cut RT from **1.2 s to 0.4 s** and noise by
  about 6 dBA [src: Physics Today,
  https://physicstoday.aip.org/news/homemade-panels-significantly-reduce-noise-in-a-busy-hospital-unit].
  Fabric-fibreglass ceiling panels roughly halved corridor RT [src: Acoustics Today 2006,
  https://acousticstoday.org/issues/2006AT/Jan2006/files/basic-html/page47.html].
- Sabine for a 30 × 2.4 × 2.7 m corridor (V = 194 m³) [calc]:
  - At 500 Hz with an NRC ~0.6 tile ceiling (α ≈ 0.55), vinyl floor (0.03) and gypsum walls
    (0.05): A ≈ 50 m², so **RT ≈ 0.6 s**. With a hard ceiling (α 0.05): **RT ≈ 2.2 s**.
  - At 125 Hz with gypsum stud walls (α ≈ 0.29), the walls alone give about 47 m², so
    **RT125 ≈ 0.45–0.6 s**. With painted block walls (α ≈ 0.1) it would be longer.
  - At 4 kHz the tile falls off (about 0.3 in the older table) and the walls and floor stay
    reflective, so HF RT runs **at or slightly above mid** [est].
- So: **tile ceiling, about 0.5–0.9 s mid; hard ceiling, 1.5–2.2 s**. Sabine also
  overestimates the effect of the ceiling here, because the lateral wall-to-wall energy never
  hits it (§0.2, §0.5).

**Early reflections.** Wall-to-wall flutter at **2·2.4/343 = 14.0 ms** (71 Hz rate) [calc].
A 1.8 m corridor gives 10.5 ms [calc]. Floor–ceiling repeats at about 15.7 ms but are damped by
the tile. The far end wall at 20 m returns at 117 ms (an echo). Side doorways and alcoves break
up the flutter train irregularly.

**Coloration.** With gypsum walls the low end is genuinely **thin**: LF RT is short because the
stud walls absorb lows as panels. The developer's "thin low end" is confirmed for gypsum, but
not for block walls. The 71 Hz flutter rate sits at the corridor's first transverse mode, so a
faint pitch at 71/142/213 Hz [calc]. Halmrast's observation puts flutter "sheen" energy around
1–2 kHz [src, abstract], which is where the cold high-mid sheen comes from.

**Through walls / distance.** In a long space, level falls continuously along the corridor and
the tail lengthens with distance (§0.2). Sounds two corridors away arrive LF-heavy through
gypsum partitions (see §6). The required exam-room/corridor isolation is quoted as STC 35
[src: CAA paper above].

**Signature (refined).** **A short, clean flutter between the side walls under a quiet,
absorptive ceiling.** The decay is short overall, but a thin, pitched flutter (about 10–14 ms
period, so a faint 70–95 Hz "buzz" over a 1–2 kHz sheen) rings longer than the rest of the
room. That contrast (dead room, live flutter) is the recognisable hospital sound, more than a
long reverb.

**Implications for the DSP.**
- RT60: 125 Hz **0.5 s**, 500 Hz–1 kHz **0.7 s**, 4 kHz **0.8 s** with tile ceiling. Offer the
  1.5–2.0 s "hard-ceiling/old wing" variant only as a Character [est from calc above].
  The current design (1.5 s low/1.8 s high) is the hard-ceiling case.
- Flutter: a separate feedback comb (or tapped line) at **14.0 ms** (range 10.5–14 ms), with
  its own decay about 1.5–2× the main RT. Band-pass the train **800 Hz–3 kHz**, so it loses
  lows and highs each pass. The current 8.5 ms implies a 1.46 m corridor, which is narrow for
  a hospital.
- Pre-delay 0–3 ms. ERs: first lateral reflections 5–8 ms (source off-centre), then the
  14 ms train. Far end wall at **110–180 ms** (20–30 m), low-passed about 3 kHz.
- Low end: HP about 120–150 Hz on the reverb send (gypsum absorbs lows), shelf −3 dB below
  250 Hz. Gentle +2–3 dB presence at 1.5–3 kHz in the tail.
- Distance: lengthen the tail and lower the direct level with distance.

---

## 2. Shopping mall atrium

**Geometry & materials.** Multi-storey void, e.g. 40 × 20 × 20 m (V ≈ 16 000 m³) [est], glass
roof, stone/tile floor, glass shopfronts, balustrades. Open gallery edges couple to the
corridors and act partly as absorbers [est]. Large volume and hard materials, especially
glass, produce long RT [src: Zhao, Kang & Jin, *Building Simulation* 2017,
https://eprints.whiterose.ac.uk/109149/1/Effects-of-geometry-on-the-sound-field-in-atria-white%20rose.pdf].
A flat skylight gives the longest T30, a sloped one shorter [src: same].

**RT60.**
- Measured in operating malls: **4–5 s at mid frequencies**, the main cause of poor acoustic
  conditions [src: Kanev, *Acoustics* 2021, https://doi.org/10.3390/acoustics3010011]. Malls
  and transport hubs are quoted at 4.0–5.0 s [src: Sheffield review,
  https://www.ioa.org.uk/system/files/proceedings/j_kang_acoustic_comfort_in_non-acoustic_spaces_a_review_of_recent_work_in_sheffield.pdf].
  Porto food courts measured 1.7–3.2 s, against a proposed target of 1.1–1.3 s [src: Carvalho,
  https://paginas.fe.up.pt/%7Ecarvalho/nc16.pdf]. The Galleria mall was "three or four times
  longer" than suitable, with intelligibility collapsing beyond about 7 m [src: JCAA,
  https://jcaa.caa-aca.ca/index.php/jcaa/article/download/855/572].
- **Spectral shape: "the longest reverberation in atria occurs at middle frequencies"** [src:
  Sheffield review]. LF is reduced by glass and light panels (glass α125 ≈ 0.18). HF is
  reduced by air absorption.
- Air absorption [calc]: for V = 16 000 m³ and 4.5 s mid, A ≈ 570 m². At 4 kHz/50 % RH,
  4mV ≈ 430 m², giving **RT4k ≈ 2.6 s**. At 8 kHz, 4mV ≈ 1530 m², giving **RT8k ≈ 1.2 s**.
- The developer's 3–8 s range is fair. **5–6 s and up is the empty/untreated end**, and 7.5 s
  at low frequencies contradicts the mid-peaked shape.

**Early reflections.** Few strong ones: the floor (a few ms) and then balcony fronts and the
opposite gallery at 40–120 ms [calc for 7–20 m extra path]. The fronts form discrete slaps in
a big space. Diffuse, slow-building tail.

**The PA (distant music).**
- Ceiling speakers: 8" 70 V models from one maker span **100 Hz–10 kHz** (HD72W/TFD72WC) or
  **55 Hz–8 kHz** (SD722W, dual-cone). Coaxial models reach **70 Hz–15.5 kHz**. 4" models
  are rated about **100 Hz–10 kHz** (FC104T72), and published specs are inconsistent
  (BT720-4 is quoted as both 60 Hz–12 kHz and 150 Hz–17 kHz). Tolerances (±3 dB vs −10 dB)
  are often unstated [src: AtlasIED, https://atlasied.com/hd72w, https://www.atlasied.com/sd722w,
  https://atlasied.com/fc104t72; avlgear C803AT70,
  https://avlgear.com/collections/commercial-ceiling-speakers/products/atlasied-c803at70-8-inch-in-ceiling-coaxial-speaker-with-5w-70v-transformer;
  goknight BT720-4,
  https://goknight.com/atlas-sound-bt720-4-t47-4-inch-in-ceiling-speaker-with-70v-4-watt-transformer-and-t720-4-baffle/].
  Transformer taps are 0.25–4 W on budget units [src: AtlasIED TFD72WC].
- Small line transformers are known to saturate at low frequency and high drive, so installers
  high-pass 70 V lines [est, general engineering practice; not sourced here]. Saturation adds
  low-order harmonics on bass notes.
- Distributed speakers in a long-RT atrium cause time dispersion, and long-delay echoes are
  audible within a 4.0–4.5 s decay [src: P. Mapp IOA,
  https://www.ioa.org.uk/system/files/publications/P%20MAPP%20DESIGN%20AND%20IMPLEMENTATION%20OF%20P.A.%20SYSTEMS%20FOR%20DIFFICULT%20CONDITIONS.pdf;
  Start, IOA, §0.8].

**Through walls / distance.** "Music in another wing": the listener is far beyond critical
distance (d_c ≈ 0.057·√(16000/4.5) ≈ 3.4 m [calc]), so almost everything heard is
reverberant. The direct HF of the nearest speaker is mostly lost off-axis. Several speakers
arrive smeared over tens of ms.

**Signature (confirmed and refined).** **Music over band-limited ceiling speakers, heard as
reverberant field only, with several speaker arrivals smeared in time.** Keep it, but derive the
band from the speaker (about 100 Hz–10 kHz) and let distance, off-axis loss and the mid-peaked
tail do the further narrowing. A fixed 320 Hz–3.9 kHz band-pass is narrower than any
datasheet. It is defensible only as "speaker + off-axis + distance" combined.

**Implications for the DSP.**
- RT60 (mall "Far Away" path and room): 125 Hz **3.5 s**, 500 Hz–1 kHz **4.5 s**, 4 kHz
  **2.5 s**, 8 kHz ~1.2 s [calc/est]. Empty-mall Character up to 6 s mid.
- Pre-delay **40–80 ms**. Sparse ER taps at about 45, 70, 95, 120 ms (balcony fronts),
  −10 to −16 dB, low-passed to 4 kHz.
- PA chain: HP 2nd order **100–120 Hz**, LP **8–10 kHz**, +2–3 dB bump about 150–200 Hz (small
  cone in a back-can) [est]. Soft-clip only below 200 Hz (transformer).
- Distributed arrivals: 3–5 copies at **0, 12, 23, 37, 55 ms**, decreasing level (−3 to
  −9 dB), each gently low-passed by its distance. Then far-field wet only (direct ≤ −15 dB re
  tail).

---

## 3. School hallway

**Geometry & materials.** About 3–3.6 m wide, 3 m high, 40–80 m long [est]. Floor is terrazzo
or VCT. Walls are painted block, glazed tile or brick, often lined with steel lockers. Ceilings
in many buildings are lay-in tile; older buildings have plaster.

**Guidance.** UK BB93 has a dedicated corridor/stairwell absorption section (§1.7). It was
introduced to reduce reverberation, and compliance is by calculation because there is no
in-situ corridor test [src: BB93 2015,
https://assets.publishing.service.gov.uk/media/5a8170d3e5274a2e8ab54012/BB93_February_2015.pdf;
Building mag, https://www.building.co.uk/focus/the-new-skool-rules-ok/1028200.article].
Equivalent Irish guidance: Class C absorber over an area ≥ the floor area (usually the
ceiling), or ≥ 0.25 m² absorption per m³ of corridor [src: TGD E summary,
https://housebuild-prod.herokuapp.com/construction/building-guidelines/building-regulations-and-other-guidance/reverberation-control].

**RT60.** No measured corridor data found. Sabine for 60 × 3 × 3 m (V = 540 m³) with
terrazzo floor and block walls [calc]:
- **Tile ceiling** (α 0.55 at 500): A ≈ 3.6 + 99 + 7.2 ≈ 110 m², so **RT ≈ 0.8 s**.
- **Hard plaster ceiling:** A ≈ 20 m², so **RT ≈ 4 s** by Sabine. Real long-corridor decay is
  shorter, because of the non-diffuse field and openings to classrooms [est, about 1.5–2.5 s].
- The developer's 1.2–1.6 s is reasonable as a mix [est]. Block walls absorb fewer lows than
  gypsum, so LF RT ≥ mid (a "boxy" sound).

**Early reflections.** Locker-to-locker flutter at **2·3.0/343 = 17.5 ms** (57 Hz); 3.6 m
gives 21 ms [calc]. The developer's 4.6 ms (0.8 m) cannot be a wall-to-wall spacing. It could
stand for the louvre/door-panel scale of the lockers themselves, a diffusion texture rather
than flutter [est].

**Lockers.** Manufacturers confirm steel locker doors rattle and vibrate. "Quiet" lockers add
full-height stiffeners, rubber bumpers and polymer latch parts specifically to stop "metal
rattle noises" [src: Hallowell/Hertz,
https://www.hertzfurniture.com/steel-lockers--hallowell-quiet-lockers--3-wide--1340--pr.html;
Scranton (HDPE vendor claim "3× quieter"),
https://www.scrantonproducts.com/wp-content/uploads/2015/05/Duralife_vs_Metal_Comparison_Sell_Sheet.pdf].
No measured resonance frequencies were found. The banks act as large thin steel panels that
can be driven sympathetically by loud low-mid sound and add a faint ring and buzz [est].

**Tape wobble ("nostalgia").**
- Wow and flutter are measured with a weighting filter **peaking at 4 Hz**, where wobble is most
  objectionable. The filter is shared by IEC 60386, DIN 45507, BS 4847, CCIR 409-3 and AES6.
  Historically wow was below and flutter above about 4 Hz (some sources say 6 Hz). The curve
  presumes flutter above 200 Hz is inaudible [src: Wikipedia, Wow and flutter measurement,
  https://en.wikipedia.org/wiki/Wow_and_flutter_measurement; reeltoreel.nl,
  https://www.reeltoreel.nl/blog/?p=551].
- Typical figures: Sony Walkman WM-DD **0.08 % WRMS**, a later DD 0.13 % DIN. Budget modern
  portable **0.3 % typ.** Measured vintage portables **0.25–0.35 % WRMS**, a good Aiwa 0.10 %.
  Home decks 0.05 % NAB/WRMS vs 0.08 % DIN for the same unit [src: Walkman DD,
  https://en.wikipedia.org/wiki/Walkman_DD_series; stereo2go forum,
  https://www.stereo2go.com/forums/posts/55427/; RTM B-1000EW,
  https://store.recordingthemasters.com/products/c60-walkmann; Harman Kardon HK100M,
  https://support.harmankardon.com/us/en/home-audio/tape-players-recorders/tape-players-recorders-cassette/HK200XM-.html].
- Audibility: above about 0.25 % WRMS, pitch instability is clearly audible on most music [src:
  commercial article, https://hifihub.ai/articles/what-is-wow-and-flutter].
- 0.3 % = **5.2 cents** [calc]. For a sinusoidal speed error of peak fraction p at frequency f,
  the delay-line excursion is p/(2πf). So 0.4 % peak at 1 Hz → 0.64 ms; 0.1 % at 8 Hz →
  0.02 ms [calc].

**Signature (refined).** The physical room signature is the **boxy, mid-forward hallway with a
locker-bank metallic ring and a lateral flutter of about 17–21 ms**. The tape wobble is not an
acoustic property of the hallway. Keep it as this room's one *non-acoustic* signature only if
the "memory" framing is intended, and keep it subtle (≤ 0.3 % WRMS).

**Implications for the DSP.**
- RT60: 125 Hz **1.5 s**, 500 Hz **1.3 s**, 4 kHz **1.0 s** [est from calc range].
- Flutter comb **17.5 ms** (range 17–21 ms), decay about 1.2× main, band 500 Hz–4 kHz.
- Locker ring: 2–3 high-Q resonators in **700 Hz–2 kHz** [est], −24 dB, excited only by the
  wet signal, decay 0.3–0.6 s. Optional rattle: envelope-gated noise burst, very rare.
- Boxiness: +3–5 dB peak about 400–800 Hz in the tail [est]. Pre-delay 2–5 ms.
- Wobble: wow **0.5–2 Hz**, peak depth 0.15–0.3 % (0.3–0.8 ms excursion). Flutter
  **6–12 Hz**, 0.03–0.08 %. Add randomness (sum of 2–3 incommensurate LFOs + slow noise).
  Optional tape LP 10–12 kHz.

---

## 4. Underground parking garage

**Geometry & materials.** Large, flat: e.g. 50 × 30 m per level, **2.2–2.7 m clear height**
[est], column grid about 7.5–8 m (three parking bays) [est]. Bare concrete slab, columns and
walls, α ≈ 0.01–0.02. Bare concrete is "typical of parking garages, unfinished basements"
[src: AppliedCalculator,
https://appliedcalculator.com/physics/acoustics/rt60-reverberation-time-calculator/].
Parked cars, ramps and ventilation openings absorb and scatter.

**RT60.**
- No peer-reviewed garage measurement was found (searches exhausted). Indications:
  - NTI Audio's own clap-test estimate in its parking basement is **about 3 s** [src:
    https://www.nti-audio.com/en/applications/room-building-acoustics/reverberation-time].
  - Bare concrete/metal-clad space worked example: 2.5 s at 500 Hz [src: MechCodex,
    https://mechcodex.com/learn/dynamics-vibration/room-acoustics].
  - Korean apartment courtyards above underground car parks: over 4 s at 500 Hz, with RT
    rising with distance above 500 Hz [src: Yang, Kim & Kang, NCEJ 2013,
    https://ince.publisher.ingentaconnect.com/content/ince/ncej/2013/00000061/00000001/art00001].
    These are outdoor spaces, not garage interiors.
- Sabine for 50 × 30 × 2.6 m (V = 3900 m³, all α = 0.02) gives about **9 s** [calc]. That is
  far too long. A flat space is not diffuse: sound spreads horizontally, columns and cars
  scatter it, and each level is open to ramps. Treat **2.5–3.5 s mid** as realistic [est,
  anchored by the NTI estimate].
- Frequency shape: concrete reflects all bands. LF is longest (no panel absorbers, modes). HF
  is shortened by air absorption over the long tail paths (3 s ≈ 1 km of path, about 30 dB at
  4 kHz at 50 % RH [calc]) and by slightly porous concrete. The developer's 3.4 s low/1.9 s
  high matches this.

**Early reflections.**
- **Floor–ceiling flutter: 2·2.6/343 = 15.2 ms** (2.4 m → 14.0 ms), i.e. an axial series at
  66/132/198 Hz [calc]. Two huge parallel planes make this the strongest early pattern. It
  is what makes a clap in a car park "ring".
- Columns: discrete returns at 2d/c, e.g. 4 m → 23 ms, 8 m → 47 ms, 12 m → 70 ms [calc].
  Columns (about 0.4–0.6 m) reflect mainly above about c/0.5 m ≈ 700 Hz, so the slaps are
  brighter than the tail [est].
- Far walls 25–50 m: 150–300 ms slap-back [calc].

**Coloration/modes.** Schroeder f ≈ 2000·√(3/3900) ≈ 55 Hz [calc], so the room is modal-dense
in general. The vertical axial series (66 Hz family) stands out because it is so strongly
reinforced, the "boom" [est].

**Distance.** Level falls with distance more than in a diffuse room (flat space) [est, by
analogy with long enclosures §0.2]. Distant sources lose highs in the tail.

**Signature (refined).** **Low-ceiling floor–ceiling flutter (about 14–15 ms, a pitched boom
at about 66 Hz and harmonics) plus discrete bright slaps off columns at irregular 20–70 ms.**
Confirmed in spirit. Note the "dark highs" come from the long tail, not from the direct sound.

**Implications for the DSP.**
- RT60: 125 Hz **3.5 s**, 500 Hz **3.0 s**, 4 kHz **1.8 s** (crossover about 800 Hz, as now)
  [est].
- Vertical flutter comb **14.0–15.2 ms**, feedback for about 0.8–1.2 s decay, LP 2 kHz
  (concrete flutter is low-mid). This produces the 66 Hz family; optionally add a +3–4 dB
  peak at **66 Hz and 132 Hz** in the tail [calc].
- Column taps: 6–10 taps between **18 and 75 ms**, irregular, each HP about 500 Hz,
  −8 to −16 dB. Slap-back from the far wall **150–250 ms**, −18 dB, LP 3 kHz.
- Pre-delay 0–2 ms (floor reflection almost immediate). Tail LP sweeping down with decay
  (about 6 kHz → 2.5 kHz over the first second).

---

## 5. Indoor swimming pool

**Geometry & materials.** A 25 m pool in a hall of about 33 × 20 × 7 m (V ≈ 4600 m³) [est].
Ceramic tile on deck and walls, glazing, water surface, often a metal deck roof. Materials
are chosen for moisture resistance, and "concrete, tile, glass, even the water surface itself"
are highly reflective [src: Kinetics Noise,
https://kineticsnoise.com/spaces/swimming-pools-natatoriums; Athletic Business,
https://www.athleticbusiness.com/facilities/aquatics/article/15140446/sound-barriers].

**RT60.** Untreated natatoriums **5–10 s** [src: Kinetics]. 6–8 s typical at 500 Hz and often
higher [src: Aquatics Int'l, https://www.aquaticsintl.com/?p=9604]. Many aquatic centres
reach 8+ s [src: Australasian Leisure Mgmt,
https://www.ausleisure.com.au/news/curbing-the-reverb-in-indoor-pools]. A modelled untreated
high-school pool exceeds 11 s at 500 Hz, and 1.63 s was measured after treatment [src: Eckel,
https://eckelusa.com/wp-content/uploads/2017/07/Fremont-Ross-HS-Natatorium-Case-Study_May-2017.pdf].
The usual target is 1.5–2 s. These are vendor sources but consistent. **The developer's
3–6 s is on the short side for untreated. 3.8–4.4 s fits a partly treated pool.**

**Humidity and air absorption.** Pool halls are warm and humid. Humid air absorbs HF **less**
than dry air (§0.7), so the developer's claim is directionally correct. Size of the effect
[calc, V = 4600 m³, 5 s mid → A ≈ 148 m²]: at 4 kHz, 4mV ≈ 123 m² at 50 % RH (RT4k ≈ 2.7 s)
vs ≈ 206 m² at 30 % RH (RT4k ≈ 2.1 s). So **HF still decays faster than mid**, just less than
in a dry hall. A brighter-than-mid tail (4.4 s high > 3.8 s low) would need an absorptive
LF element (glazing, light roof). Plausible, but not the default.

**Early reflections.** Water is a flat, near-perfect mirror at the level of the source.
Side-wall flutter across 20 m at **117 ms**, end walls (33 m) at **192 ms**: these are
**discrete echoes, not a tone** (>50 ms) [calc]. Floor/water–ceiling at 7 m gives **41 ms** [calc].

**Underwater hearing.**
- Air→water transmission: T = 4Z₁Z₂/(Z₁+Z₂)² ≈ 4·415/1.48×10⁶ ≈ 1.1×10⁻³, i.e. **−29.5 dB**
  [calc, standard impedances]. The developer's "~−30 dB" is confirmed.
- Hearing thresholds underwater are **44–60 dB worse** than in air (250 Hz–4 kHz), and the
  loss grows with frequency, from **about 24 dB at 25 Hz to about 72 dB at 4 kHz** [src:
  DTIC 1956 diver study, https://apps.dtic.mil/sti/pdfs/AD0691403.pdf; review via sonicfield,
  https://labs.sonicfield.org/library/underwater-hearing-thresholds].
- Mechanism: the ear is impedance-matched to air. A flooded canal adds a second mismatch.
  Underwater hearing is largely **bone conduction**, because head tissue impedance is close to
  water's. Dual-path theory gives roughly equal tympanic and bone contributions near 1 kHz
  [src: DTIC AD0691403; auditory.org list, https://www.auditory.org/postings/2006/242.html].
  A wetsuit hood costs a further 25–35 dB above 1 kHz [src: DTIC].
- Implication: from underwater (or with ears submerged), airborne pool sound is **about 30 dB
  down with a steep, frequency-rising loss**. Effectively a low-pass whose top octave is
  40–50 dB below its bottom [calc from the 24→72 dB slope]. The pool's own tail is also
  heard this way. With the head half-submerged, one ear hears the bright room in air while
  the other hears the dark, bone-conducted version [est].

**Signature (refined).** **Long, bright-ish tile tail with discrete wall echoes at about
100–200 ms over a mirror-like water reflection.** "Underwater" makes a strong optional
*state*: a steep tilt (24 dB at 25 Hz rising to 72 dB at 4 kHz) and an overall −30 dB. The
current "surface shimmer" modulation has no acoustic basis found. Keep it as an aesthetic
choice, but very small (as it already is).

**Implications for the DSP.**
- RT60: 125 Hz **4.5 s**, 500 Hz–1 kHz **5.0 s**, 4 kHz **3.0 s** (humid) [calc/est].
  Untreated Character up to 7 s mid.
- Pre-delay 8–15 ms. Discrete ER echoes at **41 ms** (ceiling), **117 ms** and **192 ms**
  (walls), −8 to −14 dB, bright (LP ≥ 8 kHz), with 2–3 repeats of the 117 ms pair.
- Underwater mode: overall −25 to −30 dB, then a shelving tilt about −12 dB/oct above 250 Hz
  (or LP 2-pole at 300–500 Hz plus a shelf), keeping 50–200 Hz. Mono-ish, slightly boosted
  50–150 Hz (bone conduction, "in the head") [est].

---

## 6. Hotel room / corridor

**Geometry & materials.** Room about 4 × 6 × 2.6 m (V ≈ 62 m³) [est]: carpet, bed,
upholstery, heavy curtains, gypsum walls. Corridor about 1.8 m wide, carpeted, with doors
every few metres [est].

**RT60.** Furnished living rooms: historically assumed about 0.5 s; a 1983 average of
**0.29 s**; a typical room with pile carpet and upholstery predicted at **0.26 s** [src: M.
Burgess, IOA,
https://www.ioa.org.uk/system/files/publications/M%20BURGESS%20THE%20EFFECTS%20OF%20CURRENT%20REVERBERATION%20TIMES%20IN%20BRITISH%20LIVING%20ROOMS.pdf].
Carpet alone took one test room from 3.3 to 1.6 s at 1 kHz [src: Penn State,
https://www.acs.psu.edu/drussell/Demos/RT60/RT60.html]. So **0.3–0.5 s for a hotel room**
[est]. Carpet absorbs HF strongly but LF hardly at all (§0.3), so LF RT is longer than HF.
The developer's 0.9 s low is long for a furnished room; **0.5–0.6 s at 125 Hz** is more
likely, plus modes. Hotel corridors: no data found; 0.5–0.8 s [est, developer's figure].

**Modes.** Axial modes at 343/12 = 29 Hz, 343/8 = 43 Hz and 343/5.2 = 66 Hz, plus multiples.
Schroeder f ≈ 2000·√(0.4/62) ≈ 160 Hz [calc]. Below about 160 Hz the room is a set of
resonances.

**Through the wall.**
- Mass law: about +6 dB per doubling of mass or frequency for a single leaf [src: NRC Canada
  CBD-239, https://web.mit.edu/parmstr/Public/NRCan/CanBldgDigests/cbd239_e.html].
- **Mass-air-mass resonance** f₀ ≈ C·√((m₁+m₂)/(m₁m₂d)), with C ≈ 60 for an empty cavity and
  ≈ 43 when filled [src: forum formula, consistent with literature,
  https://johnlsayersarchive.com/viewtopic.php?p=145272]. For two 16 mm gypsum leaves
  (≈ 12.5 kg/m²): **≈ 76 Hz (100 mm cavity), ≈ 107 Hz (50 mm)** [calc]. With wood or heavy
  steel studs the effective resonance is **about double** the formula [src: INTER-NOISE 2019,
  https://www.sea-acustica.es/INTERNOISE_2019/Fchrs/Proceedings/1210.pdf], i.e. **150–210 Hz**.
  Double-stud walls show resonances near 80 Hz [src: JCAA,
  https://jcaa.caa-aca.ca/index.php/jcaa/article/view/3976].
- **Coincidence dip:** gypsum's dip is near 2500 Hz (patent), about 3150 Hz for 12.7 mm board,
  and measured at about 2600 Hz for 15.9 mm Type X [src: Euronoise 2015 (NRC),
  https://www.conforg.fr/euronoise2015/proceedings/data/articles/000262.pdf; USPTO
  https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/11772372].
- Single-stud walls test around **STC 37 (1+1 layers), 41 (2+2)** [src: JCAA,
  https://jcaa.caa-aca.ca/index.php/jcaa/article/download/2404/2153/2543].
- Result: what leaks is **mostly below about 250 Hz**, peaked at the wall resonance, with TL
  rising steeply above. Theory gives about 18 dB/oct for double walls above f₀ [est, textbook
  rule]. A small leak returns around 2.5–3 kHz (coincidence) plus flanking via doors and ducts.

**Signature (confirmed).** **Music from the next room: bass and kick only, with a resonant hump
at about 100–200 Hz, everything above about 500 Hz gone except a faint 2.5–3 kHz trace.** The
room itself should feel dead and close. The existing 480 Hz LP and 165 Hz resonance are well
supported (stud-coupled resonance).

**Implications for the DSP.**
- Room RT60: 125 Hz **0.55 s**, 500 Hz **0.4 s**, 4 kHz **0.3 s**. Pre-delay 1–3 ms. ERs at
  3–12 ms, highs damped (LP 6 kHz on ERs) [est].
- Room modes: 2–3 peaks at **43, 66, 86 Hz** [calc], +2–4 dB, Q 4–8.
- Through-wall: resonant peak **120–180 Hz** (+6 dB, Q 2), then LP 4th order at **250–400 Hz**,
  plus a narrow band at **2.6–3 kHz** at −30 to −35 dB relative to the pass-band. Overall
  −25 to −35 dB (STC 35–40). Add the next room's own short reverb (0.4 s) *before* the wall.
- Corridor variant: RT 0.6 s mid, HF 0.4 s, flutter killed (carpet and doors).

---

## 7. Subway station / tunnel

**Geometry & materials.** Platform 100–150 m long, about 15 m wide, 5–8 m high [est], glazed
tile walls (classic stations), concrete/stone floor, open track bed, tunnel mouths at both ends.

**RT60.**
- London Underground platform: **about 2.5 s at the live end, 1.2 s at the dead end** (mid)
  [src: Barnett 1993, IOA,
  https://www.ioa.org.uk/system/files/publications/PW%20BARNETT%20THE%20USE%20OF%20BINAURAL%20RECORDING%20TECHNIQUES%20IN%20THE%20ASSESSMENT%20OF%20SPEECH%20INTELLIGIBILITY%20USING%20WORD%20S.pdf].
  Porto: the **best**, most ceiling-treated station has an average RT of **3.2 s** (RASTI 0.55),
  so untreated ones are longer [src: Carvalho, FIA 2018,
  https://paginas.fe.up.pt/%7Ecarvalho/fia2018j.pdf]. Iranian stations mean and max above 2 s
  [src: https://karafan.tvu.ac.ir/article_165831_20cc8d9ac5be8434c5f2224570f755a7.pdf].
  An underground rail platform T30 ranged from **2.44 s at 250 Hz to 0.65 s at 8 kHz** [src:
  search summary of arXiv hearing-research scenes, https://arxiv.org/pdf/2111.01237; Munich
  Theresienstraße IRs, https://arxiv.org/pdf/2106.15916].
- Shape: **LF-heavy, steep HF roll-off**, consistent with the developer's 3.6/2.6 s.

**Long-enclosure behaviour.** Level falls along the platform. RT/EDT vary with position and
distance [src: Shield & Yang, IOA,
https://www.ioa.org.uk/system/files/proceedings/bm_shield_ln_yang_improving_speech_intelligibility_in_underground_stations.pdf;
Nowicka]. A smaller cross-section and absorptive floor/ceiling improve STI [src: Ankara
study, https://repository.bilkent.edu.tr/items/b8839276-737f-4bb5-81af-39c18e662a6a]. The
tunnel is a waveguide: its cross-modes start at about c/2W (W ≈ 5 m → 34 Hz) [calc], so
essentially all audio propagates down it with little spreading loss. Energy entering the
tunnel mouth returns only from distant discontinuities (crossovers, the next station) [est].

**PA.** London Underground uses RASTI/STI. A transit design goal is STI 0.45 ("fair"). The row
of platform speakers itself lengthens the perceived reverb [src: Knight & Barnett, IOA,
https://www.ioa.org.uk/system/files/publications/RD%20KNIGHT%20PW%20BARNETT%20RELATIONSHIP%20BETWEEN%20STI%20AND%20PB%20WORD%20SCORE%20INTELLIGIBILITY%20MEASUREMENTS%20IN%20LONDON%20UNDERGROUND%20STATIONS.pdf;
JCAA transit study, §0.8].

**Early reflections.** Platform wall ↔ track wall across about 15 m: **87 ms** flutter-echo
pairs [calc], so discrete. Vault/ceiling at 6 m: 35 ms [calc]. Tunnel returns are late and
asymmetric (one side): 200–600 ms [est from 35–100 m to a discontinuity].

**Signature (confirmed).** **Asymmetric echoes running away down the tunnel on one side,
over a LF-heavy, position-dependent tail.** "Metallic colour" is weakly supported. Glazed
tile and the regular repetition of cross-platform reflections could give it, but no source
measures it. Keep the combs subtle.

**Implications for the DSP.**
- RT60: 125–250 Hz **3.0 s**, 500 Hz–1 kHz **2.5 s**, 4 kHz **1.2 s**, 8 kHz 0.7 s [src-anchored].
- Pre-delay 5–10 ms. Cross-platform echo pair at **87 ms** (and 174 ms), −10 dB, bright.
  Ceiling 35 ms.
- Tunnel: one-sided (pan 70–90 % to one side) delays at **250, 430, 600 ms**, each
  LP-ed progressively (5 → 2 kHz), feedback 0.3, so the echo "runs away".
- LF: +4–6 dB shelf below 120 Hz in the tail, plus very low rumble bed (train) < 80 Hz.

---

## 8. Supermarket

**Geometry & materials.** Single storey, e.g. 60 × 40 × 5–6 m (V ≈ 12 000–14 000 m³) [est].
Glossy vinyl or polished concrete floor; exposed metal deck roof or tile ceiling; long
shelving runs full of packaged goods; refrigerated cases with glass doors.

**RT60.** **No supermarket-specific measurement was found** (searched explicitly). Nearest
data: mall food courts 0.9–3.2 s, with a 1.1–1.3 s target [src: Carvalho; MDPI
mall-workers study, https://www.mdpi.com/2076-3417/10/3/1170]. Goods on shelves (cardboard,
packaging, textiles, bread) add broadband absorption and strong scattering. Shelving
breaks long paths, so the field is diffuse but short. **Estimate 1.0–2.0 s mid** [est]. The
current 2.5/2.9 s (bright) suits a nearly empty store with hard ceiling. A full store is
shorter and less bright, as cardboard and goods absorb highs more than lows [est].

**The ceiling speaker grid.**
- Classic rule: spacing ≈ **2 × (ceiling height − ear height of about 1.2 m)** [src:
  distributed-sound tech note, https://www.scribd.com/document/132042835/Distributed-Sound-Systems;
  TOA speaker guide, https://www.manualslib.com/manual/328138/Toa-Bs-1030.html?page=24].
  For a 5 m ceiling that is about 7.6 m [calc]. The developer's "1.5–2 × ceiling height" is
  close (7.5–10 m), but the ear-height correction is the standard form.
- Comb filtering [calc, 5 m ceiling, 7.6 m spacing]:
  - Directly under a speaker (3.8 m) with the next speaker at 8.5 m: Δ = 4.7 m → **13.7 ms**.
    Notches every 73 Hz. The level difference of 7 dB limits notch depth to about −5 dB.
  - At the quarter-point: 4.25 m vs 6.85 m → **7.6 ms**, notches every 132 Hz.
  - Midway: Δ ≈ 0, no comb.
  - Walking at 1 m/s changes Δ by up to several ms per second, so a **slow sweeping flange**.

**Early reflections.** Floor (glossy) almost immediately; the roof at about 2·5/343 = 29 ms
(floor–ceiling repetition) [calc]; shelving aisles about 2–2.5 m wide → 12–15 ms lateral
repeats [calc], heavily diffused by goods.

**Signature (refined).** **Background music from an overlapping ceiling-speaker grid, with
slowly moving comb filtering as you walk, in a large but softened room.** The PA comb is the
signature, more than reverb length. The speakers here are the same band-limited 8" units as
the mall (about 55/100 Hz–8/10 kHz).

**Implications for the DSP.**
- RT60: 125 Hz **1.8 s**, 500 Hz **1.5 s**, 4 kHz **1.0 s** [est] (empty-store Character
  2.5 s).
- Pre-delay 5–15 ms. Lateral ER taps **12–15 ms** (aisles), roof **29 ms**, diffused (short
  allpass chain).
- Speaker grid: 3–4 delayed copies at **0, 7.6, 13.7, 20 ms** (−0, −4, −7, −9 dB).
  Modulate the delays slowly (**0.03–0.1 Hz**, ±1–3 ms) to emulate walking.
- Speaker band: HP 90–110 Hz, LP 8–10 kHz, slight 2–4 kHz cone peak.

---

## 9. Open-plan office

**Geometry & materials.** Large floor plate, 2.7–3 m to a suspended ceiling [est],
**NRC 0.70–0.90** tiles (e.g. Armstrong CLASSIC Max NRC 0.70, OPTRA NRC 0.90) [src: Knauf/
Armstrong pages, §0.3]. Carpet tiles, screens, workstations.

**RT60.** ODEON's comparison: the same office measured **0.5 s with an acoustic ceiling vs
1.0 s with a concrete ceiling**. Ceiling absorption is "the most important parameter" [src:
https://odeon.dk/learn/articles/open-plan-office-acoustics/]. A Nordic limit is **T ≤ 0.5 s**,
or T ≤ 0.16·h [src: Petersen, BNAM 2018,
https://vbn.aau.dk/ws/files/281954690/2026382_C_M_Petersen_BNAM2018.pdf]. ISO 3382-3
characterises offices by *spatial decay of speech* (D2,S, level drop per distance doubling),
not RT, because the field is not diffuse [src: ISO 3382-3 preview,
https://webstore.ansi.org/preview-pages/ISO/preview_ISO+3382-3-2012.pdf]. The developer's
0.4–0.6 s is confirmed. LF is longer: tile α at 125 Hz is low, so 0.6–0.8 s at 125 Hz is
likely [est].

**Ballast hum.**
- Magnetic ballasts leave the mains frequency unchanged, so the arc current crosses zero
  **120 times per second** (60 Hz mains), causing 120 Hz light flicker [src: LightSearch,
  https://classic.lightsearch.com/resources/lightguides/ballasts.html].
- The audible hum comes from the core and coil assembly. Sound ratings run from **Class A
  (quietest, libraries)** to D/F (loud). Hum depends on ballast count, rating and room
  background [src: Philips/Advance datasheet,
  https://www.assets.lighting.philips.com/is/content/PhilipsLighting/fp913700500931-pss-en_us;
  Howard Lighting,
  https://dev.shineretrofits.com/brands/howard-lighting/ballasts/magnetic-fluorescent-ballasts.html].
- The fundamental is at **2 × mains (120 Hz in 60 Hz countries, 100 Hz in 50 Hz countries)**,
  with harmonics, because magnetostriction strain doesn't depend on flux polarity [est,
  standard transformer-hum physics; a forum claim of 60 Hz found in the search is not
  supported]. Electronic HF ballasts are generally rated A and essentially silent [src:
  LightSearch].

**Through walls / distance.** Spatial decay per distance doubling is the defining quantity,
and screens give local shielding. Speech fades with distance more steeply than in a
reverberant room [src: ODEON].

**Signature (confirmed).** **A dead, close room whose only "sound" is a 120 Hz (or 100 Hz)
ballast hum with harmonics and flicker.** Confirmed. Ensure the hum is 2 × mains, with
harmonic content from magnetostriction and arc buzz.

**Implications for the DSP.**
- RT60: 125 Hz **0.7 s**, 500 Hz **0.45 s**, 4 kHz **0.35 s** [src-anchored/est].
- Pre-delay 2–5 ms. ERs: floor (carpet, LP 3 kHz) and a few screen reflections 4–15 ms,
  −12 dB. No flutter (absorbent ceiling, no parallel hard pair).
- Distance: level drop 6–8 dB per doubling [est]. LP 4–6 kHz on the far source.
- Hum: 120 Hz fundamental (option 100 Hz), harmonics 240/360/480 Hz at −6/−10/−14 dB, odd
  buzz above 1 kHz. Flicker modulation at 120 Hz (implicit). A dying tube stutters at 2–8 Hz.

---

## 10. "Impossible room"

**Perceptual cues of room size.**
- Listeners use **different cues**: some judge room size by direct-to-reverberant ratio, others
  by reverberation time. Energy-based measures carry most of the information [src: Sandvad
  1999 JASA, summarised at https://labs.sonicfield.org/library/auditory-perception-of-reverberant-surroundings].
- Reverberation increases judged room size when level cues are present; even weak reverb gives
  usable size information [src: Kolarik et al. 2017, Cambridge repository,
  https://www.repository.cam.ac.uk/handle/1810/323538].
- Perceived maximum distance correlates with perceived room size, and reverberant rooms make
  sources seem farther [src: Etchemendy et al., Sci. Rep. 2017,
  https://link.springer.com/10.1038/s41598-017-06495-3]. Room size tends to be underestimated
  [src: Maempel & Jentsch 2013,
  https://www.simpk.de/fileadmin/user_upload_sim/Dokumente/Forschung/Themen/Abteilung_III/Publikationen/Maempel_Jentsch_2013_Audio-visual_interaction_of_size_and_distance_perception_in_concert_halls_-_a_preliminary_study.pdf].
- Pre-delay (the gap before the first reflection) scales with room dimension: a 2 ms first
  reflection implies a surface about 0.3 m away [calc, path c·t].

**What mismatched cues do.** With size encoded in DRR, pre-delay, ER density and RT
*independently*, an impossible room sets them to contradict each other:
- **Small-room ERs (1–5 ms, dense) + cathedral tail (7–10 s):** the brain gets "close walls"
  and "huge volume" at once. Listeners vary in which cue they weight (Sandvad), so the room
  reads as unstable [est].
- **High DRR (close, dry) + long RT:** the source sounds near but the space vast, a "floating"
  effect [est].
- **RT that does not fall with frequency** (or rises): physically impossible in large rooms,
  where air absorption always shortens HF (§0.7). A tail that gets *brighter* as it decays
  is a strong "unreal" cue [est, follows from physics].
- **Tail that grows after the note (swell):** no passive room does this.

**Reverse reverb and freeze.**
- Reverse reverb: render the reverb of a note, reverse it so the tail swells into the note
  [src: Attack Magazine, https://www.attackmagazine.com/?p=76069]. In real time it can only
  be approximated (e.g. an envelope that rises over 0.5–2 s on the wet signal, or a
  reverse-buffer grain delay), because true reversal needs the future.
- Freeze: hold the reverb tail indefinitely (feedback = 1, input muted) [src: Soundbridge,
  https://www.soundbridge.io/frozen-reverb-effect]. The timing of engagement determines the
  frozen content.

**Signature (confirmed).** **Contradicting size cues: a tiny room's early reflections with a
cathedral's tail that swells instead of decaying.** The existing design matches the research.
The one cue it could add is a **tail that brightens as it decays**.

**Implications for the DSP.**
- ERs at 1.5–4 ms (dense, 8–12 taps); RT60 **8–10 s**, flat or rising with frequency (e.g.
  125 Hz 7 s, 4 kHz 9 s).
- Swell: wet gain envelope rising over **0.8–1.5 s** after onset (reverse-reverb impression).
- Freeze: feedback → 1.0 with input muted, triggered rarely or by the user. Crossfade 50–100 ms.
- Keep DRR high (direct about +6 dB over the tail) while RT is long. Pitch-shifted reflections
  (±0.3–0.5 semitone) and very slow image drift (0.02–0.05 Hz) remain design choices
  [est, no source].

---

## Summary table

RT60 values are per band at **125 Hz / 500 Hz–1 kHz / 4 kHz**. Labels: [src] measured,
[calc] computed, [est] estimate.

| Room | RT60 low / mid / high (s) | Pre-delay / first reflection | Main ER pattern | Band limits | Signature effect (only this room) |
|---|---|---|---|---|---|
| Hospital corridor | 0.5 / 0.7 / 0.8 (tile ceiling) [calc+src]; 1.5–2.2 mid if hard ceiling | 0–3 ms; lateral 5–8 ms | Wall-to-wall flutter **14 ms** (10.5–14), far end 110–180 ms | Reverb HP ~130 Hz; sheen 1–3 kHz | **Long, thin lateral flutter over an otherwise short, dead decay** |
| Mall atrium | 3.5 / 4.5 / 2.5 [src mid 4–5 s; calc HF] | 40–80 ms | Sparse balcony slaps 45–120 ms, then diffuse | PA 100 Hz–10 kHz, then distance LP | **Band-limited ceiling-speaker music heard as reverberant field, arrivals smeared 0–55 ms** |
| School hallway | 1.5 / 1.3 / 1.0 [est] | 2–5 ms | Locker-to-locker flutter **17.5 ms** (17–21) | Boxy peak 400–800 Hz; ring 0.7–2 kHz | **Locker-bank metallic ring + lateral flutter**; tape wobble (≤0.3 % WRMS, wow 0.5–2 Hz) as non-acoustic memory layer |
| Parking garage | 3.5 / 3.0 / 1.8 [est, ~3 s anchor] | 0–2 ms | Floor–ceiling flutter **14–15 ms**; column slaps 18–75 ms; far wall 150–250 ms | Tail LP 6 → 2.5 kHz; 66/132 Hz boom | **Low-ceiling floor–ceiling flutter "boom" (66 Hz family) + irregular bright column slaps** |
| Swimming pool | 4.5 / 5.0 / 3.0 [src 5–10 s untreated; calc HF] | 8–15 ms | Discrete echoes 41 ms (ceiling), 117/192 ms (walls) | Bright ERs (≥8 kHz); underwater: −30 dB, steep LP | **Discrete 100–200 ms tile echoes over a long tail; optional underwater state (−30 dB, 24 → 72 dB loss)** |
| Hotel room | 0.55 / 0.4 / 0.3 [src 0.26–0.5 rooms] | 1–3 ms | Short, damped 3–12 ms ERs; modes 43/66/86 Hz | Room LP ~6 kHz | **Through-the-wall: resonance 120–180 Hz, LP 250–400 Hz, faint 2.6–3 kHz leak, −25 to −35 dB** |
| Subway station | 3.0 / 2.5 / 1.2 [src] | 5–10 ms | Cross-platform 87 ms pairs; one-sided tunnel echoes 250–600 ms | +LF below 120 Hz; tail LP 2–5 kHz | **One-sided echoes running away down the tunnel** |
| Supermarket | 1.8 / 1.5 / 1.0 [est] | 5–15 ms | Aisle 12–15 ms, roof 29 ms, diffused | PA 100 Hz–10 kHz | **Overlapping speaker-grid comb filter, slowly sweeping (walking flange)** |
| Open-plan office | 0.7 / 0.45 / 0.35 [src 0.5 s] | 2–5 ms | Few weak floor/screen ERs 4–15 ms, no flutter | Far source LP 4–6 kHz | **2 × mains ballast hum (120/100 Hz + harmonics) as the room's "sound", in a dead space** |
| Impossible room | 7 / 8–10 / 9 (rising) [design] | 1.5–4 ms (tiny room) | Dense tiny-room ERs + cathedral tail | Tail brightens over time | **Contradicting size cues: small-room ERs, huge swelling tail (freeze/reverse)** |

---

## What LIMINAL does with this (2026-10-31)

Each place now has an acoustic profile (`Source/DSP/Environment.cpp`, the room) and **one signature effect no other
place uses** (`Source/DSP/Signature.cpp`), applied to each instrument's share of the dry sound (its Influence x
Environment Amount x effects Mix) inside the place's chain, so it cross-fades with the place (~0.75 s). Effects that
were shared by several places' defaults (tape, pitch drift, discrete echoes, a 100 Hz tremolo, tape wobble in five
Characters) were removed from every place whose signature they are not.

| Place | Room changes from this research | Signature (DSP) | Characters (1 / 2 / 3) |
|---|---|---|---|
| Hospital | RT60 1.5/1.8 s -> **1.0/1.15 s** (tile ceilings); first-reflection spacing 8.5 -> **14 ms** (2.4 m corridor) | Flutter: feedback comb at the walls' round trip, 2 x 2.3 m / c = 13.4 ms (L) and 14.3 ms (R), high-passed in the loop; dry high-passed at 170 Hz with a 3.5 kHz sheen | Sterile Silence: narrow, bright / After Hours: wider (18 ms), softer / Something Wrong: the period wanders +-4 % at 0.13 Hz |
| Mall | RT60 7.5/5.2 s -> **5.5/3.8 s** (atria peak in the mids) | Distant PA: 380 Hz-4.2 kHz (24 dB/oct both sides), cone resonance 1.3 kHz and break-up 3.1 kHz, transformer grit, four speakers' arrivals at 95-251 ms, the direct sound almost gone (heard only over the speakers), most of it sent into the atrium tail | Closing Time: a nearer wing / Forgotten: a worn, narrower, grittier speaker / Endless: furthest, more tail |
| School | first reflections no longer a flutter pattern (that is the hospital's); dense and boxy | Tape wobble (wow 0.55 Hz +-9 cents, flutter 6.8 Hz +-3 cents, slow drift; research: <=0.3 % WRMS) and four steel-locker resonators (415, 662, 1096, 1730 Hz, Q 28 - approximate panel modes) | After Class / Childhood Memory: worn tape, warm / Nightmare: slow deep wow, lockers ringing |
| Garage | unchanged RT60 (3.4/1.9 s, within 3.5/3.0/1.8 estimate) | Slap-back: six discrete dark slaps 27-177 ms (pillars and far wall), no feedback, and the 2.5 m ceiling's first mode, 69 Hz (+104 Hz) booming | Vacant / Distant Movement: further, darker / Pursuit: eight slaps, more boom |
| Pool | RT60 3.8/4.4 s -> **4.8/3.6 s** (highs still decay faster); first reflection 41 ms (ceiling) | Underwater layer: steep 520 Hz low-pass (24 dB/oct, after the air-water loss rising with frequency), slow pitch wobble (0.21 / 0.37 Hz) and level wobble, blended | Still Water 22 % / Forgotten Summer 12 % / Submerged 75 % |
| Hotel | RT60 0.9/0.4 s -> **0.6/0.35 s** (furnished rooms 0.26-0.5 s) | Through the wall: mass-air-mass resonance ~95 Hz, mass-law low-pass 300 Hz then 1.4 kHz, a faint ring at the coincidence dip (2.6 kHz), the neighbour's small room; blended with a warm carpeted near sound | Late Night 55 % / Faded Luxury 25 % / Room 404 85 %, the wall ringing |
| Subway | unchanged room | Receding tunnel echoes: six taps 110-1060 ms, each darker, quieter, further to one side and drifting flat (6-31 cents, 0.06-0.13 Hz); a subtle short comb (the rails) on the echoes only (research: keep it subtle) | Last Train: rails ringing / Abandoned: much further, darker / Approaching: from the other tunnel, far echoes louder |
| Supermarket | RT60 2.5/2.9 s -> **1.8/1.3 s** (shelved goods absorb) | Ceiling-speaker grid: six small-speaker copies at 6-47 ms and different positions (comb filtering), their delays shifting slowly (0.03-0.1 Hz: walking under the grid), one bright bounce off the polished floor | Closing Shift: three speakers / Nostalgia: older, narrower, pushed speakers / Fluorescent Nightmare: eight, harsh |
| Office | unchanged (0.5/0.35 s) | Hum infection: amplitude modulation by the full-wave rectified 60 Hz mains (120 Hz and harmonics), deeper when the tube stutters; a little buzz rides on the notes; dead, damped | After Hours / Dream State: softer / Infinite Cubicles: the ballast stutters |
| Impossible | RT60 9.0/7.5 s -> **7.5/9.5 s** (the highs outlast the lows: no real room does that, as suggested) | Wrong acoustics: each half second played backwards and rising into the moment it was played (reversed swells), reflections at +7 and -5 semitones, and now and then the sound freezes (0.2 s held 2.5 s, rare) | Familiar: subtle / Unstable: short reversed swells, frequent freezes / Beyond Reality: long reversed swells |

**Where LIMINAL departs from the research, on purpose:** the school's tape wobble is not an acoustic property (the
research says so) but the developer asked for it as the school's nostalgia; the pool's "light on water" shimmer of
the tail has no acoustic basis but is kept as an aesthetic choice from Direction Update 2; the subway's metallic comb
is kept subtle because no measurement supports it; the hospital's RT60 follows the measurements (short) rather than
Direction Update 2's 1.5-2 s, because the measured corridors with tile ceilings are short and the flutter reads more
clearly over a short decay.

## Sources

General / standards
- ISO 3382-2 scope: https://www.asn.sn/en/node/41823
- ISO 3382-3 preview: https://webstore.ansi.org/preview-pages/ISO/preview_ISO+3382-3-2012.pdf
- ISO 9613-1 listing: https://unistore.uni.com/iso-9613-1-1993
- ISO 9613-1 table (secondary reproduction): https://kwx.liehendr.com/content/wikibooks_en_all_maxi_2021-03/A/Engineering_Acoustics/Outdoor_Sound_Propagation
- Air absorption vs humidity preprint: https://preprints.org/manuscript/202409.1192/v1/download
- HyperPhysics reverberation modelling: https://hyperphysics.gsu.edu/hbase/Acoustic/revmod.html
- Berkeley/ICSI large-room acoustics slides: https://ftp.icsi.berkeley.edu/eecs225d/spr12/slides/acoustics_large.pdf
- Jens Hee, RT measurement: https://jenshee.dk/signalprocessing/rtmeas.pdf
- Norsonic, impulsive RT method: https://www.norsonic.com/articles/impulsive-method-for-reverberation-time-measurement
- NTI Audio RT page: https://www.nti-audio.com/en/applications/room-building-acoustics/reverberation-time
- Sabine/Eyring: https://metricgate.com/docs/reverberation-time-rt60-sabine-eyring/ ; https://prosoundtraining.com/course_docs/course320/sabineandeyringeq.pdf
- Critical distance: https://arxiv.org/pdf/2204.06352 ; https://strutt.arup.com/help/Electroacoustics/DCritical.htm ; https://hearingreview.com/hearing-loss/health-wellness/critical-distance
- Room modes/Schroeder: https://www.sonarworks.com/blog/learn/room-modes
- Early/late, clarity: https://caml.music.mcgill.ca/~gary/307/week3/node9.html ; https://metricgate.com/docs/clarity-c50-c80-definition-d50/ ; https://file.techscience.com/uploads/attached/file/20191111/20191111065114_96899.pdf
- Flutter echo: https://reposit.haw-hamburg.de/bitstream/20.500.12738/7920/1/Bachelorarbeit_A_Esselun.pdf ; https://akutek.info/research_files/echoes.htm ; https://www.ioa.org.uk/system/files/proceedings/t_halmrast_why_do_flutter_echoes_always_end_up_around_1-2_khz.pdf
- Comb filtering: https://en.wikipedia.org/wiki/Comb_filter ; https://digital.livesoundint.com/october-2019/page-52 ; https://www.prosoundweb.com/proper-loudspeaker-placement-how-to-avoid-lobes-and-nulls/
- Absorption tables: https://commercial-acoustics.com/sound-advice/sound-absorption-coefficient-chart ; https://people.utm.my/zaitonharon/wp-content/uploads/sites/688/2016/08/CHAPTER-5_COMPUTATION-OF-REVERBERATION-ROOM-BY-USING-CARA.4.pdf ; https://sengpielaudio.com/calculator-RT60Coeff.htm ; https://acta-acustica.edpsciences.org/articles/aacus/full_html/2024/01/aacus230061/T2.html ; https://w1.mtsu.edu/faculty/wroberts/teaching/reverb_2.php ; https://appliedcalculator.com/physics/acoustics/rt60-reverberation-time-calculator/
- Long enclosures: https://www.aivc.org/sites/default/files/airbase_10219.pdf ; https://www.ioa.org.uk/system/files/proceedings/z_tang_j_kang_h_jin_sound_field_and_soundscape_in_underground_shopping_streets.pdf

Hospital
- UCL, Chinese wards: https://discovery.ucl.ac.uk/10178697/1/Acoustic%20treatments%20in%20general%20hospital%20wards%20in%20China_F1.pdf
- Hsu et al., ASA 159th: https://acoustics.org/pressroom/httpdocs/159th/hsu.htm
- Physics Today, Johns Hopkins panels: https://physicstoday.aip.org/news/homemade-panels-significantly-reduce-noise-in-a-busy-hospital-unit
- Acoustics Today 2006: https://acousticstoday.org/issues/2006AT/Jan2006/files/basic-html/page47.html
- Walls & Ceilings: https://www.wconline.com/articles/93407-sound-medicine
- Flutter/hard surfaces review: https://tehqeeqat.org/downloadpdf/16228
- Rockfon FGI ceiling requirements: https://www.rockfon.com/en-us/our-story/articles-and-insights/2024/FGI-Ceiling-Requirements/
- FGI table (CAA paper): https://awc.caa-aca.ca/index.php/AWC/AWC14/paper/downloadSuppFile/59/10
- WHO Community Noise: https://nonoise.org/library/whonoise/whonoise.htm ; https://pmc.ncbi.nlm.nih.gov/articles/PMC2443309/

Mall / PA / supermarket
- Kanev 2021: https://doi.org/10.3390/acoustics3010011
- Kang Sheffield review: https://www.ioa.org.uk/system/files/proceedings/j_kang_acoustic_comfort_in_non-acoustic_spaces_a_review_of_recent_work_in_sheffield.pdf
- Zhao, Kang & Jin, atria geometry: https://eprints.whiterose.ac.uk/109149/1/Effects-of-geometry-on-the-sound-field-in-atria-white%20rose.pdf
- Carvalho, Porto malls: https://paginas.fe.up.pt/%7Ecarvalho/nc16.pdf
- MDPI mall workers: https://www.mdpi.com/2076-3417/10/3/1170
- Galleria mall (JCAA): https://jcaa.caa-aca.ca/index.php/jcaa/article/download/855/572
- P. Mapp, PA for difficult conditions: https://www.ioa.org.uk/system/files/publications/P%20MAPP%20DESIGN%20AND%20IMPLEMENTATION%20OF%20P.A.%20SYSTEMS%20FOR%20DIFFICULT%20CONDITIONS.pdf
- Start, delay spread: https://www.ioa.org.uk/system/files/proceedings/e_start_optimisation_of_speech_intelligibility_in_multi-source_environments_using_delay_spread_minimisatio.pdf
- AtlasIED HD72W: https://atlasied.com/hd72w ; SD722W: https://www.atlasied.com/sd722w ; FC104T72: https://atlasied.com/fc104t72
- C803AT70: https://avlgear.com/collections/commercial-ceiling-speakers/products/atlasied-c803at70-8-inch-in-ceiling-coaxial-speaker-with-5w-70v-transformer
- BT720-4: https://goknight.com/atlas-sound-bt720-4-t47-4-inch-in-ceiling-speaker-with-70v-4-watt-transformer-and-t720-4-baffle/
- Speaker spacing: https://www.scribd.com/document/132042835/Distributed-Sound-Systems ; https://www.manualslib.com/manual/328138/Toa-Bs-1030.html?page=24

School / tape
- BB93 2015: https://assets.publishing.service.gov.uk/media/5a8170d3e5274a2e8ab54012/BB93_February_2015.pdf
- Building magazine: https://www.building.co.uk/focus/the-new-skool-rules-ok/1028200.article
- Irish TGD E corridor absorption: https://housebuild-prod.herokuapp.com/construction/building-guidelines/building-regulations-and-other-guidance/reverberation-control
- Quiet lockers: https://www.hertzfurniture.com/steel-lockers--hallowell-quiet-lockers--3-wide--1340--pr.html ; https://www.scrantonproducts.com/wp-content/uploads/2015/05/Duralife_vs_Metal_Comparison_Sell_Sheet.pdf
- Wow & flutter measurement: https://en.wikipedia.org/wiki/Wow_and_flutter_measurement ; https://www.reeltoreel.nl/blog/?p=551 ; https://hifihub.ai/articles/what-is-wow-and-flutter
- Walkman/cassette figures: https://en.wikipedia.org/wiki/Walkman_DD_series ; https://www.stereo2go.com/forums/posts/55427/ ; https://store.recordingthemasters.com/products/c60-walkmann ; https://support.harmankardon.com/us/en/home-audio/tape-players-recorders/tape-players-recorders-cassette/HK200XM-.html

Garage
- Yang, Kim & Kang 2013: https://ince.publisher.ingentaconnect.com/content/ince/ncej/2013/00000061/00000001/art00001
- MechCodex example: https://mechcodex.com/learn/dynamics-vibration/room-acoustics

Pool / underwater
- Kinetics: https://kineticsnoise.com/spaces/swimming-pools-natatoriums
- Aquatics International: https://www.aquaticsintl.com/?p=9604
- Australasian Leisure Management: https://www.ausleisure.com.au/news/curbing-the-reverb-in-indoor-pools
- Eckel Fremont Ross: https://eckelusa.com/wp-content/uploads/2017/07/Fremont-Ross-HS-Natatorium-Case-Study_May-2017.pdf
- Athletic Business: https://www.athleticbusiness.com/facilities/aquatics/article/15140446/sound-barriers
- DTIC underwater hearing: https://apps.dtic.mil/sti/pdfs/AD0691403.pdf
- Underwater thresholds review: https://labs.sonicfield.org/library/underwater-hearing-thresholds
- Auditory list, bone conduction: https://www.auditory.org/postings/2006/242.html

Hotel / walls
- Burgess, living-room RT: https://www.ioa.org.uk/system/files/publications/M%20BURGESS%20THE%20EFFECTS%20OF%20CURRENT%20REVERBERATION%20TIMES%20IN%20BRITISH%20LIVING%20ROOMS.pdf
- Penn State RT demo: https://www.acs.psu.edu/drussell/Demos/RT60/RT60.html
- NRC CBD-239: https://web.mit.edu/parmstr/Public/NRCan/CanBldgDigests/cbd239_e.html
- INTER-NOISE 2019 stud resonance: https://www.sea-acustica.es/INTERNOISE_2019/Fchrs/Proceedings/1210.pdf
- Double-stud walls (JCAA): https://jcaa.caa-aca.ca/index.php/jcaa/article/view/3976
- Single-stud STC (JCAA): https://jcaa.caa-aca.ca/index.php/jcaa/article/download/2404/2153/2543
- Euronoise 2015 coincidence: https://www.conforg.fr/euronoise2015/proceedings/data/articles/000262.pdf
- Gypsum coincidence patent: https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/11772372
- MSM formula: https://johnlsayersarchive.com/viewtopic.php?p=145272

Subway
- Nowicka, Warsaw: https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-3dae7586-2f9b-4ab7-a4b6-76da87b3500b/c/Nowicka.pdf
- Carvalho, Porto metro: https://paginas.fe.up.pt/%7Ecarvalho/fia2018j.pdf
- Munich station IRs: https://arxiv.org/pdf/2106.15916 ; https://arxiv.org/pdf/2111.01237
- Ankara metro: https://repository.bilkent.edu.tr/items/b8839276-737f-4bb5-81af-39c18e662a6a
- Iranian stations: https://karafan.tvu.ac.ir/article_165831_20cc8d9ac5be8434c5f2224570f755a7.pdf
- Barnett, London platform: https://www.ioa.org.uk/system/files/publications/PW%20BARNETT%20THE%20USE%20OF%20BINAURAL%20RECORDING%20TECHNIQUES%20IN%20THE%20ASSESSMENT%20OF%20SPEECH%20INTELLIGIBILITY%20USING%20WORD%20S.pdf
- Knight & Barnett, STI: https://www.ioa.org.uk/system/files/publications/RD%20KNIGHT%20PW%20BARNETT%20RELATIONSHIP%20BETWEEN%20STI%20AND%20PB%20WORD%20SCORE%20INTELLIGIBILITY%20MEASUREMENTS%20IN%20LONDON%20UNDERGROUND%20STATIONS.pdf
- Shield & Yang: https://www.ioa.org.uk/system/files/proceedings/bm_shield_ln_yang_improving_speech_intelligibility_in_underground_stations.pdf
- Canadian transit station STI: https://jcaa.caa-aca.ca/index.php/jcaa/article/download/1444/1188/1578

Office / ballast
- ODEON open-plan: https://odeon.dk/learn/articles/open-plan-office-acoustics/
- Petersen BNAM 2018: https://vbn.aau.dk/ws/files/281954690/2026382_C_M_Petersen_BNAM2018.pdf
- Armstrong tiles (Knauf): https://knauf.com/en-IN/p/product/armstrong-classic-max-24229_0240 ; https://knauf.com/en-IN/p/product/armstrong-optra-15mm-29626_0240
- Ballasts: https://classic.lightsearch.com/resources/lightguides/ballasts.html ; https://www.assets.lighting.philips.com/is/content/PhilipsLighting/fp913700500931-pss-en_us ; https://dev.shineretrofits.com/brands/howard-lighting/ballasts/magnetic-fluorescent-ballasts.html

Impossible room / perception
- Sandvad 1999 summary: https://labs.sonicfield.org/library/auditory-perception-of-reverberant-surroundings
- Kolarik et al.: https://www.repository.cam.ac.uk/handle/1810/323538
- Etchemendy et al. 2017: https://link.springer.com/10.1038/s41598-017-06495-3
- Maempel & Jentsch 2013: https://www.simpk.de/fileadmin/user_upload_sim/Dokumente/Forschung/Themen/Abteilung_III/Publikationen/Maempel_Jentsch_2013_Audio-visual_interaction_of_size_and_distance_perception_in_concert_halls_-_a_preliminary_study.pdf
- Reverse reverb: https://www.attackmagazine.com/?p=76069
- Freeze reverb: https://www.soundbridge.io/frozen-reverb-effect
