# Effect captures (pedal-capture milestone)

No pedal brand names appear in the product or the UI. In this document the five units are
referred to by their role: **dark hall** (reverb), **astral shimmer** (reverb / shimmer),
**tape echo** (tape-delay emulator), **fuzz-mod visitor** (modulated drive / filter), **flanger**.
(The developer's message names the physical units; the captures will live in the private
Samples repository, never in this public one.)

## Part 1 — inventory (2026-10-31, `AdrianAbbas/Samples` @ `476bfe1`, full history checked)

**No effect captures were found.** The private Samples repository has a single branch
(`main`), five commits, and 25 files; none is a pedal capture, no file is named
`01-pedal-capture-sequence-48k.wav`, and nothing was ever added and later deleted.
No other repository on the developer's account (LIMINAL, Worldbuilder, Dungeon-Crawler,
Dark-Fantasy-Plugin, elsewhere-audio-site, STA130-HW) holds them either.

What *is* in Samples (none of it matches the capture plan):

| Files | What they are (from `notes.txt` and names) | Capture plan match |
|---|---|---|
| `Soft/Medium/Hard 1 (no pick).mp3` | classical guitar, 15-note sweep E2…A#5, three dynamics | none — dry instrument, not reamped through a pedal |
| `Soft/Medium/Hard Hits.mp3`, `Varying Sweep…mp3`, `Wood tapping…mp3` | kalimba | none |
| `Open sustain 1.mp3`, `pizzicato 1/2.mp3`, `hitting bow wood….mp3` | violin | none |
| `Room tone.mp3` | room tone | none (could serve as a noise-floor reference for the recording chain only) |
| ten `* synth * hits sweep*.wav` | Minilogue patches, "dry, line input, **no pedals**" | none |

The kit's **dry guitar phrases** (the source that was to be reamped) are not there either,
so the reamped-phrase comparison in Part 3 has neither side.

## What the pipeline expects (drop the captures here, in Samples)

```
effects/<unit>/<patch>[-static]/01-pedal-capture-sequence-48k.wav   # the kit sequence through the patch
effects/<unit>/<patch>[-static]/phrase-<n>.wav                       # dry phrases reamped through it
effects/dry/phrase-<n>.wav                                           # the same phrases dry
effects/dry/01-pedal-capture-sequence-48k.wav                        # loopback (interface only, no pedal) - strongly recommended
```

`<unit>` is free text (`dark-hall`, `shimmer`, `tape-echo`, `visitor`, `flanger`); the tools
read folder names, not brand names. Any layout works as long as each take's file name says
sequence vs phrase; the inventory step lists anything it can't classify.

A **loopback take** of the sequence (interface out → in, no pedal) lets the tools remove the
interface's own response and latency exactly; without it they assume a flat chain and align
on the sync clicks only.

## Status of the milestone's parts

| Part | Status |
|---|---|
| 1 Inventory | done: nothing to inventory (above) |
| 2 Quality check | tools built and validated on a synthetic capture with known glitches (`tools/captures/`); **no real file checked** |
| 3 IRs, measurements, engine, models | analysis tools validated on synthetic units with known answers; real-time convolution engine built and tested in the plugin; **no IR or measurement of the real units exists** |
| 3 Comparison with reamped phrases | blocked: no phrases |
| 4 Rooms | blocked: not changed (no captured material to put in them) |
| 5 Listening render | blocked: before = after |
