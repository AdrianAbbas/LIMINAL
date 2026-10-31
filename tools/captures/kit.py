"""The capture-kit sequence, as described by the developer (2026-10-31).

    1 kHz level tone, 3 sync clicks, two 20 Hz-20 kHz exponential sweeps (15 s, -12 dBFS) each
    followed by 20 s of silence, an impulse, noise bursts (50 ms / 200 ms / 1 s), a quieter sweep
    at -24 dBFS, then 3 end clicks.

The *order and contents* come from the developer; the *gaps* between sections are not known (the
kit file itself was not uploaded). `analyze.py` therefore finds every section in a recording by
detection, and only uses the nominal offsets below for two things: the time from the first sync
click to each sweep (alignment) and the click-to-click spacing (clock drift). Pass the real kit
file with `--kit` and those are measured from it instead of taken from here.
"""
import numpy as np

SR = 48000
F1, F2, SWEEP_S, TAIL_S = 20.0, 20000.0, 15.0, 20.0


def db(x):
    return 10 ** (x / 20.0)


def sweep(level_dbfs=-12.0, sr=SR, seconds=SWEEP_S, f1=F1, f2=F2):
    """Exponential (Farina) sweep with 20 ms raised-cosine fades."""
    t = np.arange(int(seconds * sr)) / sr
    L = seconds / np.log(f2 / f1)
    x = np.sin(2 * np.pi * f1 * L * (np.exp(t / L) - 1.0))
    fade = int(0.02 * sr)
    w = np.ones_like(x)
    w[:fade] = 0.5 - 0.5 * np.cos(np.pi * np.arange(fade) / fade)
    w[-fade:] = w[:fade][::-1]
    return (x * w * db(level_dbfs)).astype(np.float64)


def layout(sr=SR):
    """Assumed section times in seconds (see the module note)."""
    s = {}
    t = 1.0
    s["tone"] = (t, t + 3.0); t += 3.0 + 2.0
    s["clicks"] = [t, t + 0.5, t + 1.0]; t += 1.0 + 3.0
    s["sweep1"] = t; t += SWEEP_S + TAIL_S
    s["sweep2"] = t; t += SWEEP_S + TAIL_S
    s["impulse"] = t; t += 8.0
    s["bursts"] = []
    for d in (0.05, 0.2, 1.0):
        s["bursts"].append((t, t + d)); t += d + 6.0
    s["sweep_quiet"] = t; t += SWEEP_S + TAIL_S
    s["end_clicks"] = [t, t + 0.5, t + 1.0]; t += 1.0 + 3.0
    s["length"] = t
    return s


def sequence(sr=SR, seed=1):
    lay = layout(sr)
    x = np.zeros(int(lay["length"] * sr))
    i = lambda sec: int(round(sec * sr))
    a, b = lay["tone"]
    n = np.arange(i(b) - i(a))
    tone = np.sin(2 * np.pi * 1000.0 * n / sr) * db(-18.0)
    f = int(0.01 * sr); tone[:f] *= np.linspace(0, 1, f); tone[-f:] *= np.linspace(1, 0, f)
    x[i(a):i(b)] = tone
    for c in lay["clicks"] + lay["end_clicks"]:
        x[i(c)] = db(-6.0)
    for k, lev in (("sweep1", -12.0), ("sweep2", -12.0), ("sweep_quiet", -24.0)):
        sw = sweep(lev, sr)
        x[i(lay[k]):i(lay[k]) + len(sw)] = sw
    x[i(lay["impulse"])] = db(-6.0)
    rng = np.random.default_rng(seed)
    for a, b in lay["bursts"]:
        nb = rng.standard_normal(i(b) - i(a)) * db(-18.0)
        f = int(0.002 * sr); nb[:f] *= np.linspace(0, 1, f); nb[-f:] *= np.linspace(1, 0, f)
        x[i(a):i(b)] = nb
    return x, lay


if __name__ == "__main__":
    import sys
    from scipy.io import wavfile
    x, _ = sequence()
    wavfile.write(sys.argv[1] if len(sys.argv) > 1 else "kit-reference-48k.wav", SR, x.astype(np.float32))
