#!/usr/bin/env python3
"""Prepares woodwind samples (one note per file) for TAPEWORN's sample bank (2026-11-06).

Two sources, both outside this repository (nothing they hold is committed here):
  * the free CC0 library "VSCO 2 Community Edition" (docs/LICENSES/README.md): its sustained
    soft flute (Woodwinds/Flute/susNV, the v1 takes) and sustained soft clarinet
    (Woodwinds/Clarinet/susLong, the v1 takes), prepared as the sets `ww-flute` and
    `ww-clarinet`;
  * the developer's own alto recorder, when it exists in the private Samples repository: a folder
    named `alto-recorder` (or `recorder`, any depth) of one-note WAV/MP3 files whose names carry
    the note played (`C5.wav`, `recorder F#4 soft.wav`, ...), prepared as `alto-recorder`.
    Missing is normal (not recorded yet): the set is simply not written.

    python3 -I tools/sample-prep/woodwinds.py --vsco <VSCO-2-CE checkout> --out build/samples
    python3 -I tools/sample-prep/woodwinds.py --recorder <Samples checkout> --out build/samples

Per note: mono (the two microphones summed), 44.1 kHz, a 25 Hz high-pass (DC, rumble), the
silence before the onset trimmed (5 ms kept, faded in), the pitch measured (YIN over the
sustain, refined on the harmonics; the file name only says roughly where it is - VSCO names the
flute an octave low), a sustain loop (prepare.py's search: matched waveforms, cross-faded and
levelled) and the release point (where the player stops: the recorded tail plays on note-off),
the end trimmed where it falls 60 dB below its peak. One gain per set (its loudest peak at
-1 dBFS); mapping written by prepare.py's `finish_set` (key ranges halfway to the neighbours),
and the set added to the output's index.json (created if there is none: a build without the
developer's recordings still gets the woodwinds).
"""

import argparse
import glob
import json
import os
import re
import sys

import numpy as np
import soundfile as sf
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare as P  # noqa: E402  (the shared helpers: RATE, yin, refine, find_loop, crossfade_loop, finish_set)

RATE = P.RATE

SETS = [
    {"id": "ww-flute", "name": "Flute, sustained, soft, no vibrato (VSCO 2 CE, CC0)", "kind": "bowed",
     "glob": "Woodwinds/Flute/susNV/*_v1_*.wav", "source": "vsco"},
    {"id": "ww-clarinet", "name": "Clarinet, sustained, soft (VSCO 2 CE, CC0)", "kind": "bowed",
     "glob": "Woodwinds/Clarinet/susLong/*_v1_*.wav", "source": "vsco"},
]
RECORDER = {"id": "alto-recorder", "name": "Alto recorder (the developer's own)", "kind": "bowed", "source": "recorder"}

NOTE_IN_NAME = re.compile(r"(?<![A-Za-z])([A-G])([#b]|s(?=\d))?(-?\d)(?!\d)")


def label_of(path):
    """The note a file's name carries ('C#4', 'Cs4', 'Db4'), or None."""
    base = os.path.splitext(os.path.basename(path))[0]
    for m in NOTE_IN_NAME.finditer(base):
        acc = m.group(2) or ""
        acc = "#" if acc == "s" else acc
        try:
            P.midi_of(m.group(1) + acc + m.group(3))
            return m.group(1) + acc + m.group(3)
        except ValueError:
            continue
    return None


def load_note(path):
    x, rate = sf.read(path, always_2d=True, dtype="float64")
    x = x.mean(axis=1)
    if rate != RATE:
        x = P.to_rate(x, rate) if hasattr(P, "to_rate") else signal.resample_poly(x, RATE, rate)
    sos = signal.butter(2, 25.0, "highpass", fs=RATE, output="sos")
    return signal.sosfiltfilt(sos, x - np.mean(x))


def measure_pitch(x, start, label_midi):
    """MIDI pitch over the steady part of the note (the file name only says roughly where)."""
    seg = x[start + int(0.4 * RATE):start + int(1.6 * RATE)]
    if len(seg) < int(0.3 * RATE):
        seg = x[start:]
    lo = P.noteHz(label_midi - 14) if hasattr(P, "noteHz") else 440.0 * 2 ** ((label_midi - 14 - 69) / 12)
    hi = 440.0 * 2 ** ((label_midi + 16 - 69) / 12)
    frames = [(f, a) for f, a in P.yin(seg, fmin=max(40.0, lo), fmax=min(4000.0, hi)) if a < 0.2]
    if not frames:
        return None, 1.0
    f0 = float(np.median([f for f, _ in frames]))
    f0 = P.refine(seg, f0, 6)
    return 69 + 12 * np.log2(f0 / 440.0), float(np.median([a for _, a in frames]))


def prepare_note(path, spec, report):
    x = load_note(path)
    label = label_of(path)
    if label is None:
        report["problems"].append(f"{spec['id']}: no note in the name '{os.path.basename(path)}' (skipped)")
        return None
    env = P.envelope(x)
    peak_db = float(env.max())
    above = np.nonzero(env > peak_db - 40)[0]
    if len(above) == 0:
        return None
    start = max(0, int(above[0]) * P.ENV_HOP - int(0.005 * RATE))
    x = x[start:]
    fade = int(0.005 * RATE)
    x[:fade] *= np.linspace(0.0, 1.0, fade)
    # The end: where it falls 60 dB below its peak (and 20 ms faded).
    env = P.envelope(x)
    last = np.nonzero(env > peak_db - 60)[0]
    end = min(len(x), (int(last[-1]) + 1) * P.ENV_HOP + int(0.02 * RATE))
    x = x[:end].copy()
    tail = int(0.02 * RATE)
    x[-tail:] *= np.linspace(1.0, 0.0, tail)
    midi_label = P.midi_of(label)
    root, aperiodic = measure_pitch(x, 0, midi_label)
    if root is None:
        report["problems"].append(f"{spec['id']} {label}: no steady pitch found (skipped)")
        return None
    # VSCO names the flute an octave below where it sounds: the measured pitch decides; the label
    # moves by whole octaves to it.
    label_now = P.name_of(round(root))
    if (round(root) - midi_label) % 12 != 0:
        report["warnings"].append(f"{spec['id']} {label}: sounds at {root:.2f} (MIDI), not an octave of its name: mapped as {label_now}")
    zone = {"audio": x, "label": label_now, "root": round(float(root), 3), "layer": 0, "rr": 0,
            "velocity": [0.0, 1.0], "source": os.path.basename(path), "at": 0.0}
    region = P.sustain_region(x)
    f0 = 440.0 * 2 ** ((root - 69) / 12)
    loop = P.find_loop(x, f0, region) if region else None
    note_report = {"file": os.path.basename(path), "label": label, "mapped": label_now, "pitch": round(float(root), 2),
                   "cents": round(100 * float(root - round(root)), 1), "aperiodicity": round(aperiodic, 3),
                   "seconds": round(len(x) / RATE, 2)}
    if loop:
        s, e, cost, _ = loop
        zone["audio"] = P.crossfade_loop(x, s, e, int(0.08 * RATE))
        zone["loop"] = [int(s), int(e)]
        zone["release"] = int(region[1])
        note_report.update({"loop": [round(s / RATE, 3), round(e / RATE, 3)], "loopCost": round(cost, 4), "release": round(region[1] / RATE, 3)})
    else:
        report["warnings"].append(f"{spec['id']} {label}: no sustain loop found (it plays once, as recorded)")
    report["notes"].setdefault(spec["id"], []).append(note_report)
    return zone


def prepare_set(spec, files, out, report):
    zones = []
    for path in sorted(files):
        z = prepare_note(path, spec, report)
        if z is not None:
            zones.append(z)
    # Two files on one note (a second take): round-robins.
    seen = {}
    for z in zones:
        z["rr"] = seen.get(z["label"], 0)
        seen[z["label"]] = z["rr"] + 1
    if not zones:
        return None
    peak = max(float(np.abs(z["audio"]).max()) for z in zones)
    gain = 10 ** (-1 / 20) / max(peak, 1e-9)
    spec_full = dict(spec, layers=[{"name": "sus", "velocity": [0.0, 1.0]}], endStretch=7)
    rep = {"warnings": report["warnings"]}
    mapping = P.finish_set(spec_full, zones, gain, out, rep)
    report["sets"][spec["id"]] = {"notes": len(mapping["zones"]), "loops": sum(1 for z in mapping["zones"] if "loopStart" in z),
                                  "range": [mapping["zones"][0]["label"], mapping["zones"][-1]["label"]],
                                  "seconds": round(sum(z["length"] for z in mapping["zones"]) / RATE, 1)}
    return mapping


def add_to_index(out, ids):
    path = os.path.join(out, "index.json")
    index = {"version": 1, "instruments": []}
    if os.path.exists(path):
        with open(path) as f:
            index = json.load(f)
    for sid in ids:
        if sid not in index["instruments"]:
            index["instruments"].append(sid)
    with open(path, "w") as f:
        json.dump(index, f, indent=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--vsco", help="a VSCO-2-CE checkout (at least Woodwinds/ and LICENSE)")
    ap.add_argument("--recorder", help="the Samples checkout: an `alto-recorder` folder of one-note files, if recorded")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    report = {"sets": {}, "notes": {}, "problems": [], "warnings": []}
    done = []
    if args.vsco:
        lic = os.path.join(args.vsco, "LICENSE")
        if not os.path.exists(lic) or not open(lic, encoding="utf-8", errors="replace").read().startswith("CC0 1.0 Universal"):
            print("woodwinds: the library's LICENSE is missing or not CC0 1.0: not prepared", file=sys.stderr)
            return 2
        for spec in SETS:
            files = glob.glob(os.path.join(args.vsco, spec["glob"]))
            if not files:
                report["problems"].append(f"{spec['id']}: no files match {spec['glob']}")
                continue
            if prepare_set(spec, files, args.out, report):
                done.append(spec["id"])
    if args.recorder:
        files = []
        for d in glob.glob(os.path.join(args.recorder, "**", "*"), recursive=True):
            if os.path.isdir(d) and os.path.basename(d).lower().replace(" ", "-") in ("alto-recorder", "recorder"):
                files += [f for f in glob.glob(os.path.join(d, "*")) if f.lower().endswith((".wav", ".mp3", ".flac", ".aif", ".aiff"))]
        if files and prepare_set(RECORDER, files, args.out, report):
            done.append(RECORDER["id"])
        elif not files:
            print("woodwinds: no alto recorder recorded yet (no `alto-recorder` folder): the Recorder voice plays the flute")
    add_to_index(args.out, done)
    with open(os.path.join(args.out, "woodwinds-report.json"), "w") as f:
        json.dump(report, f, indent=1)
    for sid, s in report["sets"].items():
        print(f"Prepared: {s['notes']} notes ({sid}), {s['range'][0]}..{s['range'][1]}, {s['loops']} loops, {s['seconds']} s")
    for sid, notes in report["notes"].items():
        for n in notes:
            print(f"   {sid:12} {n['file']:40} -> {n['mapped']:4} pitch {n['pitch']:7.2f} ({n['cents']:+5.1f} c)"
                  + (f" loop {n['loop']} cost {n['loopCost']}" if "loop" in n else " (no loop)"))
    for p in report["problems"]:
        print(f" problem: {p}")
    for w in report["warnings"]:
        print(f" warning: {w}")
    return 1 if report["problems"] and not done else 0


if __name__ == "__main__":
    sys.exit(main())
