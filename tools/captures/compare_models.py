"""Emulations vs the developer's reamped phrases (pedal-capture milestone, Part 3).

    python3 -I tools/captures/compare_models.py <Samples checkout> <prepared captures dir> <LiminalTests binary> <out.json>

For each patch with reamped phrases: the dry phrases (Notes.wav) through
  - the captured IR as measured (dry path included): what a convolution alone can do;
  - for the tape delay, also dry + the plug-in's TapeEcho with the measured profile, at the
    captured first repeat's level;
and the difference to the real reamped recording (analyze.compare: level, 1/3-octave spectrum,
50 ms envelope). For the shimmer the modulated and the static ("0 rate") phrases are both compared.
"""
import json
import os
import subprocess
import sys
import tempfile

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze  # noqa: E402

SR = analyze.SR


def main(samples, prepared, binary, out_path):
    dry = analyze.load(os.path.join(samples, "Notes.wav"))
    rep = {}
    files = sorted(f for f in os.listdir(samples) if f.lower().endswith(".wav") and " phrases" in f.lower())
    for f in files:
        name = f[: f.lower().index(" phrases")].strip()
        role = next((r for r, keys in (("darkhall", ("abyssal",)), ("shimmer", ("shimmer",)), ("chorusverb", ("wobbly",)),
                                       ("tape", ("tape delay",))) if any(k in name.lower() for k in keys)), None)
        wet = analyze.load(os.path.join(samples, f))
        res = {"role": role}
        ir_dir = os.path.join(prepared, "report", role)
        ir = analyze.load(os.path.join(ir_dir, "ir.wav")) if os.path.exists(os.path.join(ir_dir, "ir.wav")) else None
        if ir is not None:
            res["captured IR"] = analyze.compare(dry, wet, ir)
        if role == "tape":
            r = json.load(open(os.path.join(ir_dir, "report.json")))
            wet_db = r["measurements"]["echo"].get("first_repeat_re_direct_db", -18.0)

            def render(d, wet_db=wet_db):
                with tempfile.TemporaryDirectory() as tmp:
                    src, dst = os.path.join(tmp, "in.wav"), os.path.join(tmp, "out.wav")
                    wavfile.write(src, SR, d.astype(np.float32))
                    subprocess.run([binary, "--capture-model", f"tape-measured:{os.path.join(prepared, 'profiles.json')}@{wet_db}", src, dst],
                                   cwd=tmp, check=True, stdout=subprocess.DEVNULL)
                    return analyze.load(dst).mean(axis=1)
            res["tape model (measured profile)"] = analyze.compare(dry, wet, render=render)
        rep[f] = res
        print(f, json.dumps(res, default=analyze._plain)[:600])
    json.dump(rep, open(out_path, "w"), indent=1, default=analyze._plain)


if __name__ == "__main__":
    main(*sys.argv[1:5])
