# Sample preparation (LIMINAL Milestone 8)

Copied from LIGHT SEEKER's `tools/sample-prep` (itself WORLDBUILDER's; those repositories were
only read): `prepare.py`, `sweeps.py` unchanged, `manifest.json` rewritten for LIMINAL (only the
sets its instruments play; everything else listed in `unused` with the reason), plus
**`liminal.py`**, run after `prepare.py` on its output: the Forgotten Piano's measurements
(each note's onset and damper point, the short->long handover aligned on the waveform, the
velocity layers' correlation, the natural-decay tail resynthesised from the last held second,
noises inside held notes, and the piano's stretch-tuning curve). CI: `scripts/ci-fetch-samples.sh`
(one clone of the private repository for the recordings and the effect captures).

    python3 tools/sample-prep/prepare.py --source ../samples --out build/samples
    python3 tools/sample-prep/liminal.py build/samples
    cmake -S . -B build ...     # LIMINAL_SAMPLES_DIR defaults to build/samples

The text below is the tool's own documentation from LIGHT SEEKER / WORLDBUILDER.

## The tool (from LIGHT SEEKER)

`prepare.py` turns the developer's recordings (MP3s and WAVs of the private
`AdrianAbbas/Samples` repository, in its root or in folders such as `01-voice/`; a
manifest entry names a file by its path or, if it moved, by its name anywhere under the
repository; WAVs at other rates are resampled to 44.1 kHz) into what WORLDBUILDER's sampler loads: mono 16-bit
FLAC notes and one mapping file per instrument. The recordings and the prepared notes
never go into this public repository: CI fetches the recordings with a token
(`scripts/ci-fetch-samples.sh`) and the output lives in the build folder.

    pip install -r tools/sample-prep/requirements.txt
    git clone https://github.com/AdrianAbbas/Samples ../samples      # needs access
    python3 tools/sample-prep/prepare.py --source ../samples --out build/samples
    cmake -S . -B build -DLS_SAMPLES_DIR=build/samples ...

`manifest.json` says what each recording contains (from the Samples repository's
`notes.txt`); the script checks it against the audio and prints a report (also written
to `report.json` in the output). Rerun it whenever recordings are added or changed; a new
recording needs a manifest entry.

What it does, per recording:

1. **MP3 padding**: GarageBand's MP3s carry Apple's `iTunSMPB` tag (encoder delay,
   padding, sample count); the LAME header is used when there is one instead. The
   encoder delay plus the decoder's 529 samples are taken off the start, the padding off
   the end, and trailing digital silence is dropped.
2. **Noise reduction (gentle)**: the room tone's spectrum, fitted per third of an octave to
   the recording's own quiet gaps, comes off with a soft spectral gate (at most 9 dB,
   smoothed so it does not warble).
3. **Slicing**: onsets from the level's rise; the expected notes are assigned in order by
   how clearly each expected pitch sounds in each onset (handling noises and finger clicks
   fall out and never shift the labels after them).
4. **Pitch**: YIN within 8 semitones of the label (strings ringing in sympathy can be louder
   than a high note), refined on the harmonics, octave-checked; the sampler is tuned to
   what was played. Steady tones well below a note (other strings ringing in sympathy) are
   notched out, and notes above 250 Hz get a gentle high-pass at half their pitch.
5. **Levels**: velocity layers recorded at different input gains are levelled by their
   noise floors (the room is the same in all of them), then each instrument gets one gain
   (its loudest peak at -1 dBFS), so the layers keep their dynamics.
6. **Loops and release tails** for sustained notes: the best-matching loop in the sustain
   (crossfaded) and the point where the bow lifts (the recorded tail plays on release).
7. **Output**: `<id>/<note>-<layer>-<take>.flac`, `<id>.json` (key ranges, velocity layers,
   round-robins, loop points, release points, loudness), `index.json`, `report.json`.

**Synth sweeps** (`sweeps.py`, Milestone 15b; manifest sets with `"kind": "synth"`): the
Korg Minilogue patches, each a WAV of all 37 keys played lowest to highest, one note at a
time, once with short hits and once with long (held) hits. Per recording:

1. **Decode**: 24-bit WAV, dual mono (one channel's worth), a 10 Hz high-pass for DC (the
   lowest patch goes down to 16 Hz). No noise reduction (a line input).
2. **Slicing** by the gaps between notes: a start is a rise out of the quiet before it by
   22 dB, relaxed step by step until all 37 are found (a pad's release can overlap the
   next note); with too many, the smallest rises (a pulse inside a note) go first.
3. **Pitch**: a first pass (YIN down to 14 Hz) for every note; the sweep's order then
   gives each note its key (the median offset, so the octave switch and the patch's own
   octaves do not matter), and each note is measured within 3 semitones of its key: the
   pitch its first six harmonics agree on (each at its power-weighted centroid, weighted
   by power), since a patch's oscillators are not one harmonic series. Every note
   is mapped at its own pitch; the sampler tunes it by the cents it is out. Notes the
   first pass heard elsewhere, more than 25 cents out or wandering are reported.
4. **Long hits**: the release point (the knee of the final fall), a loop early in the
   hold (up to 1.2 s; waveform and envelope matched so a pulsing patch keeps its pulse;
   crossfaded and levelled), and the rest of the hold cut out: each note is its attack,
   its loop and its recorded release.
5. **Trim and level**: every note ends once it has fallen 50 dB under its peak; one gain
   per patch for both articulations (they were recorded at the same gain), loudest
   peak at -1 dBFS.
6. **Mapping**: `mlg-<patch>-short.json` and `mlg-<patch>-long.json`, every note on its own
   key; only the lowest and highest notes stretch a fifth to fill the ends of the range
   (the sampler folds anything further by octaves). The report checks that the short
   hits really are the shorter (the static-y bass's two files are named the other way
   round: the manifest assigns them by what they contain).

**The recordings update** (LIGHT SEEKER, after Direction Update 1):

- **Pitch from the audio** (`"pitchFrom": "audio"`, the voice): the names say what was
  meant, the audio decides where a note goes. Each sung note is found by the gaps between
  notes (a voice swells and breathes inside a note), its pitch measured on its steady
  middle (YIN over 55-1100 Hz, then an octave check both ways on the spectrum) and its
  label taken from what is held in its loop. The report lists every note named
  differently from how it sounds (all of the voice files are named an octave below what
  was sung). A one-pitch file (a choir stack, a drone) gets one label, the median of its
  takes; a take more than a semitone from the others is not used.
- **Tracked chromatic sweeps** (`"pitchFrom": "track"`, the piano): every key from
  `lowest` up; each onset's pitch is looked for near the key after the previous one, so
  a repeated or skipped key never shifts the labels; a second onset inside a ringing bass
  note (10 dB under the key's strongest) is not a take, and `oneTake` keeps one per key.
- **Layers**: entries with the same `name` are one velocity layer (two files of one
  dynamic: extra round-robins); `into` copies a file's notes into several layers (the
  falsetto sweeps play at every velocity); `pick` takes a range of a file's notes
  (`fileTakes` in all: the banjo's 5th-string file holds its soft, medium and hard hits).
- **Levels**: `levelByFloor: false` keeps the dynamics of files recorded at one gain;
  `gainGroup` gives several sets one gain (the voice's sweeps, stacks and drones keep their
  levels relative to each other).
- **Textures** (`"kind": "texture"`, the breaths): unpitched stretches, one take each,
  every key. `fx: true` also copies the set into `<out>/fx/` with its own `index.json`:
  what the effect's bundle carries (its Atmosphere layer).
- `scripts/ci-fetch-samples.sh` prints `Prepared: N notes (<set>)` for every set.
