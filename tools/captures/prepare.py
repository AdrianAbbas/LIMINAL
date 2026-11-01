"""Turns the developer's effect captures into the plug-in's capture assets.

    python3 -I tools/captures/prepare.py <Samples checkout> <out dir>

Runs analyze.py on each take it needs (by role, found from the file names), then writes into
<out>:
  darkhall.wav    the dark reverb + delay patch's response (wet only, unit energy, <= 8 s, 24-bit)
  shimmer.wav     the shimmer patch's response from its static ("0 rate") take
  chorusverb.wav  the chorus-modulator + reverb patch's response (mono)
  profiles.json   decay times per IR, the tape-echo profile, the modulation profile, the
                  shimmer's octave level, and where each came from
  report/         every analysis report (QC, measurements)
Nothing here is committed: the captures are private (CI fetches them with SAMPLES_TOKEN).
"""
import json
import os
import sys

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze  # noqa: E402

SR = analyze.SR
MAX_S = 8.0

# role -> (keywords in the sequence take's name, static take keywords or None)
ROLES = {
    "darkhall": (("abyssal",), None),
    "shimmer": (("shimmer",), ("0 rate", "static")),
    "chorusverb": (("wobbly", "chorus"), None),
    "tape": (("tape delay", "tape"), None),
}


def find_takes(root):
    seq = [f for f in sorted(os.listdir(root)) if f.lower().endswith(".wav") and " 48k" in f.lower()]
    kit_file = next((os.path.join(root, f) for f in sorted(os.listdir(root)) if f.lower().startswith("01-pedal-capture-sequence")), None)
    out = {}
    for role, (keys, static_keys) in ROLES.items():
        cands = [f for f in seq if any(k in f.lower() for k in keys)]
        main = [f for f in cands if not static_keys or not any(s in f.lower() for s in static_keys)]
        stat = [f for f in cands if static_keys and any(s in f.lower() for s in static_keys)]
        if main:
            out[role] = (os.path.join(root, main[0]), os.path.join(root, stat[0]) if stat else None)
    return out, kit_file


def wet_only(h):
    """Removes the pedal's own dry path (the room has its dry): zero up to 1 ms after the direct
    peak (the strongest sample in the first 15 ms), fading back in over 0.5 ms."""
    m = np.abs(h).max(axis=1)
    p = int(np.argmax(m[: int(0.015 * SR)]))
    cut = p + int(0.001 * SR)
    fade = int(0.0005 * SR)
    g = np.ones(len(h))
    g[:cut] = 0.0
    g[cut:cut + fade] = np.linspace(0, 1, fade)
    return h * g[:, None], p / SR * 1e3


def write24(path, h):
    peak = np.abs(h).max()
    data = np.clip(np.round(h / peak * 0.98 * 8388607.0), -8388608, 8388607).astype(np.int32) << 8
    wavfile.write(path, SR, data)
    return float(peak / 0.98)


def main(root, out):
    os.makedirs(os.path.join(out, "report"), exist_ok=True)
    takes, kit_file = find_takes(root)
    prof = {"sampleRate": SR, "irs": {}, "source": {}, "kit": os.path.basename(kit_file) if kit_file else None}
    reports = {}
    for role, (main_take, static) in takes.items():
        rep = analyze.analyze(main_take, os.path.join(out, "report", role), kit_file, static)
        reports[role] = rep
        prof["source"][role] = {"take": os.path.basename(main_take), "static": os.path.basename(static) if static else None,
                                "ir_from": rep.get("ir_from"), "quality": [{"segment": q["segment"], "clean": q["clean"], "events": len(q["events"])} for q in rep["quality"]]}
        if role == "tape":
            continue   # the tape echo is a model (its profile below), not a convolution
        h = analyze.load(os.path.join(out, "report", role, "ir.wav"))[: int(MAX_S * SR)]
        h, direct_ms = wet_only(h)
        h /= np.sqrt(np.sum(h ** 2) / h.shape[1])   # unit energy per channel
        fade = int(0.05 * SR)
        h[-fade:] *= np.linspace(1, 0, fade)[:, None] ** 2
        write24(os.path.join(out, role + ".wav"), h)
        rt = rep["measurements"]["rt60_s"]
        mids = [rt[k] for k in ("500", "1000") if rt.get(k)]
        prof["irs"][role] = {"file": role + ".wav", "channels": int(h.shape[1]), "seconds": round(len(h) / SR, 3),
                             "rt60": rt, "rt60Mid": round(float(np.mean(mids)), 3) if mids else None,
                             "directMs": round(direct_ms, 2)}
    if "tape" in reports:
        prof["tape"] = analyze.tape_profile(reports["tape"]["measurements"])
    if "chorusverb" in reports:
        fm = (reports["chorusverb"]["measurements"].get("tone_tail_modulation") or {}).get("fm", {})
        w = fm.get("wow", {})
        rate, cents = w.get("rate_hz", 0.8), w.get("depth_cents", 8.0)
        # Delay modulation depth that gives that pitch deviation: c ln2 / 1200 / (2 pi f) seconds.
        prof["mod"] = {"rateHz": rate, "depthCents": cents, "depthMs": round(cents * 0.000577623 / (2 * np.pi * rate) * 1e3, 3)}
    if "shimmer" in reports:
        h2 = reports["shimmer"]["measurements"]["harmonics_loud"]["h2_db"]
        prof["shimmer"] = {"octaveDb": h2, "note": "octave-up energy relative to the linear response (where a sweep puts it: the 2nd-harmonic slot)"}
    json.dump(prof, open(os.path.join(out, "profiles.json"), "w"), indent=1, default=analyze._plain)
    print(json.dumps(prof, indent=1, default=analyze._plain))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
