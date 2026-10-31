"""Effect-capture analysis: quality check, impulse responses and measurements.

    python3 -I tools/captures/analyze.py <capture.wav> <out-dir> [--kit kit-reference.wav]
        [--static <static-capture.wav>]

For one take of the capture-kit sequence (see kit.py) through one effect patch this:
  1. finds the sections (sync clicks by onset, sweeps by matched filter);
  2. corrects the recorder's clock drift from the start/end click spacing (needs the kit layout);
  3. quality-checks every sweep segment (sweep + its 20 s tail) for clicks, dropouts and hiss
     bursts - detected on the raw signal AND on the residual after predicting the segment from
     the *other* sweep's response, which exposes glitches hidden under a loud sweep;
  4. estimates the noise floor (level + spectrum) from the quietest stretches of the tails;
  5. deconvolves the clean sweep(s) into an impulse response (both clean: averaged, +3 dB SNR;
     one clean: that one; neither: the less damaged, clicks repaired, flagged for re-recording),
     aligned to the sync clicks, truncated where it meets the noise floor, with a fade;
  6. measures: EQ (1/3-octave), decay per octave band (Schroeder, noise-compensated), echo time,
     feedback per repeat and repeat darkening, wow/flutter and amplitude modulation (1 kHz tone),
     harmonic distortion at both sweep levels, level dependence (quiet vs loud sweep), and the
     noise-burst gains.
Writes `<out>/ir.wav` (float32, all channels) and `<out>/report.json`.
Pass `--static` with the take of the same patch at minimum modulation: the IR then comes from
that take, the modulation measurements from the main one.
"""
import json
import os
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit  # noqa: E402

SR = kit.SR
EPS = 1e-12


def _plain(o):
    return o.item() if hasattr(o, "item") else str(o)


def to_db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), EPS))


def load(path):
    sr, x = wavfile.read(path)
    if x.dtype.kind == "i":
        x = x.astype(np.float64) / float(np.iinfo(x.dtype).max)
    elif x.dtype.kind == "u":
        x = (x.astype(np.float64) - 128.0) / 128.0
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1:
        x = x[:, None]
    if sr != SR:
        g = np.gcd(int(sr), SR)
        x = signal.resample_poly(x, SR // g, int(sr) // g, axis=0)
    return x


# ----------------------------------------------------------------------------- detection

def envelope(m, ms=1.0):
    n = max(1, int(ms * 1e-3 * SR))
    return np.sqrt(signal.oaconvolve(m * m, np.ones(n) / n, mode="same").clip(0))


def onsets(m, start, stop, min_gap_s=0.3, rise_db=20.0):
    """Sharp onsets (sync clicks, the impulse) in [start, stop) samples: the envelope jumps by
    `rise_db` over the preceding 30 ms. Returns refined onset samples."""
    seg = m[start:stop]
    if len(seg) < SR // 10:
        return []
    hp = signal.sosfilt(signal.butter(4, 2000, "hp", fs=SR, output="sos"), seg)
    e = envelope(hp, 0.5)
    before = np.maximum(signal.oaconvolve(e, np.ones(int(0.03 * SR)) / (0.03 * SR), mode="full")[:len(e)], EPS)
    lag = int(0.002 * SR)
    ratio = to_db(e[lag:] / before[:-lag])
    cand = np.flatnonzero((ratio > rise_db) & (e[lag:] > 0.01 * e.max())) + lag
    out = []
    for c in cand:
        if out and c - out[-1] < min_gap_s * SR:
            continue
        w = seg[max(0, c - lag * 2):c + lag]
        pk = np.argmax(np.abs(w)) + max(0, c - lag * 2)
        a = np.abs(seg[max(0, pk - lag * 2):pk + 1])
        first = np.flatnonzero(a > 0.2 * np.abs(seg[pk]))
        out.append(max(0, pk - lag * 2) + (first[0] if len(first) else len(a) - 1))
    return [start + o for o in out]


def click_triple(cands, spacing=None, last=False):
    """Three equally spaced onsets (the sync clicks), ignoring their echoes. With `spacing`
    (from the kit) the gaps must match it within 2 ms; otherwise any equal spacing >= 0.1 s."""
    best = None
    c = sorted(cands)
    for i in range(len(c)):
        for j in range(i + 1, len(c)):
            d = c[j] - c[i]
            if d < 0.1 * SR or (spacing and abs(d - spacing * SR) > 0.002 * SR):
                continue
            for k in range(j + 1, len(c)):
                if abs((c[k] - c[j]) - d) <= 0.002 * SR:
                    t = [c[i], c[j], c[k]]
                    if best is None or (t[0] > best[0] if last else t[0] < best[0]):
                        best = t
    return best or []


def deconvolve(y, ref, n_ir=None):
    """Regularised spectral division (20 Hz - 20 kHz). Returns the full circular result: the
    linear IR at the start, harmonic IRs wrapped to the end."""
    n = 1 << int(np.ceil(np.log2(len(y) + len(ref))))
    Y = np.fft.rfft(y, n, axis=0)
    X = np.fft.rfft(ref, n)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    band = (f >= kit.F1 * 1.05) & (f <= kit.F2 * 0.97)
    p = np.abs(X) ** 2
    reg = np.where(band, 1e-6, 1e3) * p[band].max()
    # Raised-cosine taper at the band edges (20-25 Hz, 17.5-20 kHz): no brickwall ringing.
    taper = np.clip((f - 20.0) / 5.0, 0, 1) * np.clip((20000.0 - f) / 2500.0, 0, 1)
    taper = 0.5 - 0.5 * np.cos(np.pi * taper)
    H = (np.conj(X) / (p + reg)) * taper
    h = np.fft.irfft(Y * (H[:, None] if Y.ndim == 2 else H), n, axis=0)
    return h if n_ir is None else h[:n_ir]


def locate_sweeps(m):
    """Matched filter against the reference sweep: the three biggest response peaks >= 25 s apart."""
    ref = kit.sweep(-12.0)
    h = signal.oaconvolve(m, ref[::-1], mode="full")[len(ref) - 1:]
    env = np.abs(h)
    peaks = []
    order = np.argsort(env)[::-1]
    for i in order[:200000]:
        if all(abs(i - p) > 25 * SR for p in peaks):
            peaks.append(i)
        if len(peaks) == 3:
            break
    peaks.sort()
    return peaks, [float(env[p]) for p in peaks]


# ----------------------------------------------------------------------------- glitches

def glitches(x, start, stop, residual=None, thresh=25.0):
    """Clicks/dropouts in x[start:stop] (any channel): second-difference outliers against a local
    robust scale; exact-repeat runs (dropped buffers); hiss bursts in the quiet part."""
    found = []
    for name, sig in (("raw", x), ("residual", residual)):
        if sig is None:
            continue
        seg = sig[start:stop]
        d2 = np.abs(np.diff(seg, 2, axis=0)).max(axis=1)
        blk = int(0.005 * SR)
        nb = len(d2) // blk
        if nb < 6:
            continue
        med = np.median(d2[:nb * blk].reshape(nb, blk), axis=1)
        scale = signal.medfilt(med, 9)
        floor = max(1e-7, 1e-5 * float(np.abs(seg).max()))   # ignore anything 100 dB below the peak
        scale = np.maximum(np.repeat(scale, blk), floor)
        scale = np.concatenate([scale, np.full(len(d2) - len(scale), scale[-1])])
        hits = np.flatnonzero(d2 > thresh * scale)
        found += _group(hits + start + 1, "click", name)
    # Dropped/duplicated buffers: >= 16 identical consecutive samples while the signal is live.
    seg = x[start:stop]
    same = np.all(np.diff(seg, axis=0) == 0, axis=1)
    live = envelope(np.abs(seg).max(axis=1), 5.0) > 1e-4
    run = 0
    for i, s in enumerate(same):
        run = run + 1 if s else 0
        if run == 16 and live[i]:
            found.append({"at": (start + i - 15) / SR, "len_ms": None, "kind": "dropout", "seen_in": "raw"})
    return _merge(found)


def _group(idx, kind, src):
    out = []
    if len(idx) == 0:
        return out
    a = b = idx[0]
    for i in idx[1:]:
        if i - b > int(0.002 * SR):
            out.append({"at": a / SR, "len_ms": (b - a + 1) / SR * 1e3, "kind": kind, "seen_in": src})
            a = i
        b = i
    out.append({"at": a / SR, "len_ms": (b - a + 1) / SR * 1e3, "kind": kind, "seen_in": src})
    return out


def _merge(g):
    g.sort(key=lambda e: e["at"])
    out = []
    for e in g:
        if out and e["at"] - out[-1]["at"] < 0.01:
            rank = {"dropout": 0, "click": 1, "hiss burst": 2}
            if rank[e["kind"]] < rank[out[-1]["kind"]]:
                out[-1]["kind"] = e["kind"]; out[-1]["len_ms"] = e["len_ms"]
            out[-1]["seen_in"] = "both" if out[-1]["seen_in"] != e["seen_in"] else e["seen_in"]
            continue
        out.append(dict(e))
    return out


def hiss_bursts(x, start, stop):
    """20 ms windows of high-band (> 4 kHz) energy 12 dB above the stretch's median, in a stretch
    that should only be decaying."""
    seg = signal.sosfilt(signal.butter(4, 4000, "hp", fs=SR, output="sos"), x[start:stop].mean(axis=1))
    w = int(0.02 * SR)
    nb = len(seg) // w
    if nb < 10:
        return []
    e = to_db(np.sqrt((seg[:nb * w].reshape(nb, w) ** 2).mean(axis=1)))
    base = signal.medfilt(e, 25)
    out = []
    for i in np.flatnonzero(e - base > 12.0):
        out.append({"at": (start + i * w) / SR, "len_ms": 20.0, "kind": "hiss burst", "seen_in": "raw"})
    return _merge(out)


def repair_clicks(x, events, max_ms=1.0, order=32):
    """Least-squares AR interpolation over short clicks (<= max_ms). Returns the repaired copy and
    the events it repaired."""
    y = x.copy()
    done = []
    for e in events:
        if e["kind"] != "click" or e["len_ms"] is None or e["len_ms"] > max_ms:
            continue
        a = int(e["at"] * SR) - 2
        n = int(np.ceil(e["len_ms"] * 1e-3 * SR)) + 4
        ctx = 8 * order
        if a - ctx < 0 or a + n + ctx > len(y):
            continue
        for ch in range(y.shape[1]):
            seg = y[a - ctx:a + n + ctx, ch].copy()
            known = np.ones(len(seg), bool); known[ctx:ctx + n] = False
            # AR coefficients from the clean context (Burg-free: covariance least squares).
            c = np.concatenate([seg[:ctx], seg[ctx + n:]])
            A = np.array([c[i:i + order][::-1] for i in range(len(c) - order)])
            coef = np.linalg.lstsq(A, c[order:], rcond=None)[0]
            # Prediction-error filter; solve for the missing samples minimising the residual.
            pe = np.concatenate([[1.0], -coef])
            L = len(seg) - order
            Mtx = np.zeros((L, len(seg)))
            for r in range(L):
                Mtx[r, r:r + order + 1] = pe[::-1]
            Au, Ak = Mtx[:, ~known], Mtx[:, known]
            seg[~known] = np.linalg.lstsq(Au, -Ak @ seg[known], rcond=None)[0]
            y[a - ctx:a + n + ctx, ch] = seg
        done.append(e)
    return y, done


# ----------------------------------------------------------------------------- measurements

OCT = [125, 250, 500, 1000, 2000, 4000, 8000]


def band(x, fc, frac=1.0):
    lo, hi = fc * 2 ** (-0.5 * frac), min(fc * 2 ** (0.5 * frac), SR * 0.45)
    return signal.sosfiltfilt(signal.butter(4, [lo, hi], "bp", fs=SR, output="sos"), x, axis=0)


def noise_floor(x, regions):
    """Quietest 0.5 s windows within the given (start, stop) regions: RMS dBFS and an octave
    spectrum."""
    m = np.sqrt((x ** 2).mean(axis=1))
    w = SR // 2
    best = None
    for a, b in regions:
        for s in range(a, b - w, w // 2):
            r = np.sqrt(np.mean(m[s:s + w] ** 2))
            if best is None or r < best[0]:
                best = (r, s)
    if best is None:
        return None
    seg = x[best[1]:best[1] + w]
    spec = {str(f): round(float(to_db(np.sqrt(np.mean(band(seg, f) ** 2)))), 1) for f in OCT}
    return {"rms_dbfs": round(float(to_db(best[0])), 1), "at_s": best[1] / SR, "octaves_dbfs": spec,
            "rms": float(best[0])}


def truncate_point(h, floor_rms):
    """After the last 50 ms block whose envelope is above the noise floor + 3 dB (an echo's
    silent gaps between repeats don't end it)."""
    m = np.sqrt((h ** 2).mean(axis=1))
    w = int(0.05 * SR)
    nb = len(m) // w
    e = np.sqrt((m[:nb * w].reshape(nb, w) ** 2).mean(axis=1))
    lim = floor_rms * 10 ** (3 / 20.0)
    above = np.flatnonzero(e > lim)
    return min(nb, above[-1] + 2) * w if len(above) else w


def schroeder_rt(h, floor_rms, fc):
    b = band(h, fc).mean(axis=1) if h.ndim == 2 else band(h, fc)
    end = truncate_point(b[:, None], floor_rms * 0.5)
    start = int(max(0.02, 5.0 / fc) * SR)   # past the direct sound (and the band filter's ring)
    e = b[start:end] ** 2
    # Noise compensation: subtract the floor's energy per sample before integrating.
    e = np.maximum(e - (floor_rms * 0.3) ** 2, 0)
    sch = np.cumsum(e[::-1])[::-1]
    if sch[0] <= 0:
        return None
    d = 10 * np.log10(np.maximum(sch / sch[0], 1e-30))
    i5, i25 = np.argmax(d <= -5), np.argmax(d <= -25)
    if i25 <= i5:
        return None
    t = np.arange(i5, i25) / SR
    slope = np.polyfit(t, d[i5:i25], 1)[0]
    rt = -60.0 / slope if slope < 0 else None
    return round(float(rt), 3) if rt is not None and rt < 30.0 and (i25 - i5) > 0.01 * SR else None


def third_octave_eq(h):
    n = 1 << int(np.ceil(np.log2(len(h))))
    H = np.abs(np.fft.rfft(h.mean(axis=1), n))
    f = np.fft.rfftfreq(n, 1 / SR)
    out = {}
    centres = 1000 * 2 ** (np.arange(-17, 14) / 3.0)
    ref = None
    for fc in centres:
        sel = (f >= fc * 2 ** (-1 / 6)) & (f < fc * 2 ** (1 / 6))
        if sel.any():
            v = float(to_db(np.sqrt(np.mean(H[sel] ** 2))))
            out[f"{fc:.0f}"] = v
            if abs(fc - 1000) < 1:
                ref = v
    return {k: round(v - ref, 1) for k, v in out.items()}


def echoes(h, max_s=2.0):
    """Echo time from the envelope's autocorrelation; per-repeat level and darkening."""
    m = h.mean(axis=1)
    env = envelope(m, 2.0)[: int(max_s * 4 * SR)]
    skip = int(0.01 * SR)   # past the direct sound
    le = to_db(env[skip:])
    live = le > le.max() - 70
    t = np.arange(len(le))
    fit = np.polyval(np.polyfit(t[live], le[live], 1), t)
    e = np.where(live, le - fit, 0.0)
    e -= e[live].mean() if live.any() else 0
    ac = signal.fftconvolve(e, e[::-1], mode="full")[len(e) - 1:]
    ac /= ac[0] + EPS
    lo, hi = int(0.03 * SR), int(max_s * SR)
    lag = lo + int(np.argmax(ac[lo:hi]))
    if ac[lag] < 0.4:
        return {"periodic": False, "autocorr": round(float(ac[lag]), 2)}
    # Refine: find the repeat peaks near k*lag after the first wet arrival.
    reps = []
    tau = lag
    first = int(np.argmax(env[lo // 2: int(tau * 1.5)])) + lo // 2
    if env[first] < 0.3 * env.max():
        first = lag
    for k in range(0, 12):
        c = first + k * tau
        a, b = c - tau // 4, c + tau // 4
        if b >= len(m):
            break
        w = m[a:b]
        lv = to_db(np.sqrt(np.mean(band(w, 1000) ** 2)))   # 1 kHz octave: the loop's filters barely act here
        # Spot tilt (1/6-octave bands): a whole octave drifts toward its lower edge as repeats darken.
        lo_b = to_db(np.sqrt(np.mean(band(w, 500, 1 / 6) ** 2)))
        hi_b = to_db(np.sqrt(np.mean(band(w, 4000, 1 / 6) ** 2)))
        pk = a + int(np.argmax(np.abs(signal.hilbert(w))))
        reps.append((pk, lv, hi_b - lo_b))
    reps = [r for r in reps if r[1] > reps[0][1] - 50]
    if len(reps) < 3:
        return {"periodic": False, "autocorr": round(float(ac[lag]), 2)}
    pks = np.array([r[0] for r in reps]); lv = np.array([r[1] for r in reps]); tilt = np.array([r[2] for r in reps])
    k = np.arange(len(reps))
    tau_s = float(np.polyfit(k, pks, 1)[0] / SR)
    return {"periodic": True, "echo_ms": round(tau_s * 1e3, 2), "first_ms": round(pks[0] / SR * 1e3, 2),
            "feedback_db_per_repeat_1k": round(float(np.polyfit(k, lv, 1)[0]), 2),
            "darkening_db_per_repeat": round(float(np.polyfit(k, tilt, 1)[0]), 2),
            "repeats_measured": int(len(reps)), "autocorr": round(float(ac[lag]), 2)}


def modulation(x, a, b, label):
    """Frequency (cents) and amplitude modulation of the 1 kHz tone in [a, b) samples."""
    m = x[a:b].mean(axis=1)
    if len(m) < SR // 2:
        return None
    bp = signal.sosfiltfilt(signal.butter(4, [800, 1250], "bp", fs=SR, output="sos"), m)
    z = signal.hilbert(bp)
    trim = int(0.05 * SR)
    z = z[trim:-trim]
    inst = np.diff(np.unwrap(np.angle(z))) * SR / (2 * np.pi)
    dec = 48
    lp = signal.sosfiltfilt(signal.butter(4, 40, fs=SR, output="sos"), inst)[::dec]
    amp = signal.sosfiltfilt(signal.butter(4, 40, fs=SR, output="sos"), np.abs(z))[::dec]
    fs2 = SR / dec
    cents = 1200 * np.log2(np.maximum(lp, 1) / np.median(lp))
    out = {"segment": label, "seconds": round(len(m) / SR, 2)}
    for name, sig, unit in (("fm", cents, "cents"), ("am", to_db(amp / np.median(amp)), "dB")):
        s = signal.detrend(sig)
        n = len(s)
        win = np.hanning(n)
        F = np.abs(np.fft.rfft(s * win, 8 * n)) / (win.sum() / 2)
        f = np.fft.rfftfreq(8 * n, 1 / fs2)
        res = {}
        for nm, lo, hi in (("wow", 0.1, 4.0), ("flutter", 4.0, 20.0)):
            sel = (f >= max(lo, 2.0 / (n / fs2))) & (f < hi)
            if not sel.any():
                continue
            i = np.flatnonzero(sel)[np.argmax(F[sel])]
            res[nm] = {"rate_hz": round(float(f[i]), 3), f"depth_{unit}": round(float(F[i]), 3)}
        res[f"rms_{unit}"] = round(float(np.std(s)), 3)
        out[name] = res
    out["lowest_rate_resolvable_hz"] = round(2.0 / (len(m) / SR), 2)
    return out


def harmonics(hfull, n_lin, level_dbfs):
    L = kit.SWEEP_S / np.log(kit.F2 / kit.F1)
    n = len(hfull)
    lin = hfull[:n_lin]
    e_lin = np.sum(lin ** 2)
    res = {}
    tot = 0.0
    for k in range(2, 6):
        d = int(round(L * np.log(k) * SR))
        span = int(round(L * (np.log(k + 1) - np.log(k)) * SR * 0.9))
        a = n - d - span // 8
        seg = hfull[a:a + min(span, n_lin)]
        ek = float(np.sum(seg ** 2))
        tot += ek
        res[f"h{k}_db"] = round(float(10 * np.log10(ek / e_lin + 1e-20)), 1)
    res["thd_percent"] = round(float(100 * np.sqrt(tot / e_lin)), 3)
    res["sweep_dbfs"] = level_dbfs
    return res


def analyze(path, out_dir, kit_path=None, static_path=None):
    os.makedirs(out_dir, exist_ok=True)
    x = load(path)
    rep = {"file": os.path.basename(path), "channels": int(x.shape[1]), "seconds": round(len(x) / SR, 2)}
    m = x.mean(axis=1)
    peak = float(np.abs(x).max())
    rep["peak_dbfs"] = round(float(to_db(peak)), 2)
    rep["clipped_samples"] = int(np.sum(np.abs(x) >= 0.999))

    # Nominal layout (from the kit reference if given).
    if kit_path:
        ref = load(kit_path).mean(axis=1)
        sp, _ = locate_sweeps(ref)
        ref_on = click_triple(onsets(ref, 0, sp[0] - SR // 2, 0.02))
        ref_end = click_triple(onsets(ref, sp[2] + int((kit.SWEEP_S + 2) * SR), len(ref), 0.02))
        nominal = {"clicks": [o / SR for o in ref_on], "end_clicks": [o / SR for o in ref_end],
                   "sweeps": [s / SR for s in sp]}
        rep["layout"] = "measured from the kit reference"
    else:
        lay = kit.layout()
        nominal = {"clicks": lay["clicks"], "end_clicks": lay["end_clicks"],
                   "sweeps": [lay["sweep1"], lay["sweep2"], lay["sweep_quiet"]]}
        rep["layout"] = "assumed (kit.py) - pass --kit with the real kit file"

    drift = 1.0
    peaks, heights = locate_sweeps(m)
    if len(peaks) < 3:
        rep["error"] = "could not find three sweeps"
        json.dump(rep, open(os.path.join(out_dir, "report.json"), "w"), indent=1, default=_plain)
        return rep
    rep["sweep_peak_ratio_db"] = round(float(to_db(heights[2] / max(heights[0], heights[1]))), 1)

    spacing = nominal["clicks"][1] - nominal["clicks"][0]
    clicks = click_triple(onsets(m, 0, peaks[0] - SR // 2, 0.02), spacing)
    ends = click_triple(onsets(m, peaks[2] + int((kit.SWEEP_S + 2) * SR), len(m), 0.02), spacing)
    rep["sync_clicks_s"] = [round(c / SR, 4) for c in clicks]
    rep["end_clicks_s"] = [round(c / SR, 4) for c in ends]
    if len(clicks) == 3 and len(ends) == 3:
        nominal_span = nominal["end_clicks"][0] - nominal["clicks"][0]
        measured = (ends[0] - clicks[0]) / SR
        ppm = (measured / nominal_span - 1.0) * 1e6
        rep["clock_drift_ppm"] = round(float(ppm), 1)
        # The recorder's clock ran at 48 kHz x (1 + drift): rather than resampling the recording
        # (FFT resampling of a long take smeared the responses), the reference sweeps are
        # generated at the recorder's rate - exact for an analytic sweep.
        if abs(ppm) < 2000:
            drift = 1 + ppm * 1e-6
            rep["drift_corrected"] = abs(ppm) > 2.0
        starts = [int(round(clicks[0] + (s - nominal["clicks"][0]) * SR * drift)) for s in nominal["sweeps"]]
        rep["alignment"] = "sync clicks"
    else:
        # No usable clicks: the sweep starts from the matched filter (zero = the IR's first arrival).
        starts = []
        for p in peaks:
            starts.append(p)
        rep["alignment"] = "first arrival (sync clicks not found)"
        rep.setdefault("warnings", []).append("sync clicks not found: aligned on the first arrival")

    seg_len = int((kit.SWEEP_S + kit.TAIL_S) * SR)
    n_ir = int(kit.TAIL_S * SR)
    refs = [kit.sweep(lv, SR * drift) for lv in (-12.0, -12.0, -24.0)]

    def seg(i):
        a = max(0, starts[i])
        y = x[a:a + seg_len]
        if len(y) < seg_len:
            y = np.vstack([y, np.zeros((seg_len - len(y), x.shape[1]))])
        return y

    full = [deconvolve(seg(i), refs[i]) for i in range(3)]
    lin = [f[:n_ir] for f in full]

    # Residuals: each loud sweep predicted from the other's response.
    resid = [None, None, None]
    for i, j in ((0, 1), (1, 0)):
        pred = signal.oaconvolve(refs[i][:, None], lin[j], mode="full", axes=0)[:seg_len]
        if len(pred) < seg_len:
            pred = np.vstack([pred, np.zeros((seg_len - len(pred), pred.shape[1]))])
        r = np.zeros_like(x)
        a = max(0, starts[i])
        r[a:a + seg_len] = (seg(i) - pred)[: len(r) - a]
        resid[i] = r

    # Noise floor: the last 5 s of each tail.
    tails = [(starts[i] + seg_len - 5 * SR, starts[i] + seg_len) for i in range(3)]
    nf = noise_floor(x, tails)
    rep["noise_floor"] = {k: v for k, v in nf.items() if k != "rms"} if nf else None
    floor_rms = nf["rms"] if nf else 1e-5

    qc = []
    for i, nm in enumerate(("sweep1", "sweep2", "sweep_quiet")):
        a = max(0, starts[i])
        b = min(len(x), a + seg_len - SR // 10)   # stop short of the next section
        g = glitches(x, a, b, resid[i])
        g += hiss_bursts(x, a + int((kit.SWEEP_S + 5) * SR), b)
        qc.append({"segment": nm, "start_s": round(a / SR, 3), "events": _merge(g)})
    # A residual-only hit in one loud sweep at the same relative time as a hit in the other is
    # the other's glitch, carried in by the cross-prediction.
    for i, j in ((0, 1), (1, 0)):
        rel_j = [e["at"] - qc[j]["start_s"] for e in qc[j]["events"] if e["seen_in"] != "residual"]
        qc[i]["events"] = [e for e in qc[i]["events"] if not (e["seen_in"] == "residual" and
                           any(abs(e["at"] - qc[i]["start_s"] - r) < 0.003 for r in rel_j))]
    for q in qc:
        q["clean"] = len(q["events"]) == 0
    tone_a = int((nominal["clicks"][0] - 4.5) * SR) if rep["alignment"] == "sync clicks" else 0
    tone_b = clicks[0] - int(0.2 * SR) if clicks else peaks[0] - 2 * SR
    tone_a = max(0, min(tone_a + (clicks[0] - int(nominal["clicks"][0] * SR) if clicks else 0), tone_b))
    gt = glitches(x, tone_a, tone_b)
    x_rep, repaired = repair_clicks(x, gt)
    qc.append({"segment": "tone + pre-roll", "events": gt, "repaired": len(repaired),
               "clean": len(gt) == 0})
    rep["quality"] = qc

    # Which loud sweeps make the IR.
    c1, c2 = qc[0]["clean"], qc[1]["clean"]
    if c1 and c2:
        h = 0.5 * (lin[0] + lin[1]); used = "average of both sweeps"
        rep["sweep_consistency_db"] = round(float(to_db(np.sqrt(np.sum((lin[0] - lin[1]) ** 2) / np.sum(lin[0] ** 2)))), 1)
    elif c1 or c2:
        k = 0 if c1 else 1
        h = lin[k]; used = f"sweep {k + 1} only (sweep {2 - k} damaged)"
    else:
        k = int(np.argmin([len(qc[0]["events"]), len(qc[1]["events"])]))
        xr, _ = repair_clicks(x, qc[k]["events"], max_ms=2.0)
        a = max(0, starts[k])
        h = deconvolve(xr[a:a + seg_len], refs[k], n_ir); used = f"sweep {k + 1}, clicks repaired (both damaged)"
        rep["rerecord"] = True
    rep["ir_from"] = rep["sweep_choice"] = used
    if not qc[2]["clean"]:
        rep.setdefault("warnings", []).append("quiet sweep damaged: level-dependence measurement unreliable")

    # IR from the static take if given (modulated patches).
    if static_path:
        st = analyze(static_path, os.path.join(out_dir, "static"), kit_path)
        sh = load(os.path.join(out_dir, "static", "ir.wav"))
        rep["ir_source"] = "static take: " + os.path.basename(static_path)
        h = h_meas = sh
        rep["ir_from"] = "static take: " + st.get("ir_from", "?")
        if st.get("rerecord"):
            rep["rerecord"] = True
        rep["measured_on"] = "static take (EQ, decay, echoes, distortion); this take (modulation)"
    else:
        h_meas = h
        rep["ir_source"] = "this take"

    cut = min(len(h), truncate_point(h, floor_rms))
    hh = h[:cut].copy()
    fade = min(int(0.05 * SR), cut // 4)
    hh[cut - fade:] *= np.linspace(1, 0, fade)[:, None] ** 2
    rep["ir_length_s"] = round(cut / SR, 3)
    wavfile.write(os.path.join(out_dir, "ir.wav"), SR, hh.astype(np.float32))

    meas = {}
    meas["eq_third_octave_db_re_1k"] = third_octave_eq(h_meas[:cut])
    meas["rt60_s"] = {str(f): schroeder_rt(h_meas[:cut], floor_rms, f) for f in OCT}
    meas["echo"] = echoes(h_meas[:cut])
    # The same from the -24 dBFS sweep: less loop saturation, whose distortion products otherwise
    # land in other repeats' windows. Use this one to fit a model when the two disagree.
    meas["echo_quiet_sweep"] = echoes(lin[2][:cut])
    early = h_meas[: int(0.005 * SR)]
    meas["direct_to_total_db"] = round(float(10 * np.log10(np.sum(early ** 2) / (np.sum(h_meas[:cut] ** 2) + EPS))), 1)
    meas["harmonics_loud"] = harmonics(0.5 * (full[0] + full[1]).mean(axis=1) if c1 and c2 else full[0 if c1 else 1].mean(axis=1), n_ir, -12)
    meas["harmonics_quiet"] = harmonics(full[2].mean(axis=1), n_ir, -24)
    meas["level_dependence_db"] = {str(f): round(float(
        to_db(np.sqrt(np.mean(band(lin[2][:cut], f) ** 2))) - to_db(np.sqrt(np.mean(band(h_meas[:cut], f) ** 2)))), 2)
        for f in OCT}
    # Tone: wow/flutter on the tone itself and on what follows it (wet only), when long enough.
    tone_end = tone_b
    on = envelope(signal.sosfiltfilt(signal.butter(4, [900, 1100], "bp", fs=SR, output="sos"),
                                     x_rep[tone_a:tone_end].mean(axis=1)), 20.0)
    live = np.flatnonzero(on > on.max() * 0.3)
    if len(live):
        ta, tb = tone_a + live[0], tone_a + live[-1]
        meas["tone_modulation"] = modulation(x_rep, ta, tb, "tone")
        meas["tone_level_dbfs"] = round(float(to_db(np.sqrt(np.mean(x_rep[ta:tb] ** 2)))), 1)
    # Noise bursts: between the second sweep's tail and the quiet sweep.
    ba, bb = starts[1] + seg_len + int(1.0 * SR), starts[2] - SR // 2
    if bb > ba:
        e = envelope(x[ba:bb].mean(axis=1), 5.0)
        live = e > max(floor_rms * 30, e.max() * 0.05)
        edges = np.flatnonzero(np.diff(live.astype(int)))
        bursts = []
        for s0, s1 in zip(edges[::2], edges[1::2]):
            d = (s1 - s0) / SR
            if d < 0.03:
                continue
            w = x[ba + s0: ba + s1]
            first = w[: int(0.04 * SR)]
            lastw = w[-max(1, int(min(0.5, d / 3) * SR)):]
            bursts.append({"length_s": round(d, 3), "first_40ms_dbfs": round(float(to_db(np.sqrt(np.mean(first ** 2)))), 1),
                           "end_dbfs": round(float(to_db(np.sqrt(np.mean(lastw ** 2)))), 1)})
        meas["noise_bursts"] = bursts
    if static_path:
        for k in ("harmonics_loud", "harmonics_quiet", "level_dependence_db", "noise_bursts"):
            if k in st.get("measurements", {}):
                meas[k] = st["measurements"][k]
    rep["measurements"] = meas
    json.dump(rep, open(os.path.join(out_dir, "report.json"), "w"), indent=1, default=_plain)
    return rep


def compare(dry, wet, ir):
    """How close `dry * ir` is to the reamped `wet`: alignment, level, 1/3-octave spectral
    difference and envelope (50 ms) difference, in dB."""
    sim = signal.oaconvolve(dry, ir, mode="full", axes=0)[: len(wet)]
    a, b = sim.mean(axis=1), wet.mean(axis=1)
    lag = int(np.argmax(signal.correlate(b[: 10 * SR], a[: 10 * SR], mode="full", method="fft"))) - (min(len(a), 10 * SR) - 1)
    a = np.roll(a, lag)
    gain = to_db(np.sqrt(np.mean(b ** 2)) / (np.sqrt(np.mean(a ** 2)) + EPS))
    a *= 10 ** (gain / 20)
    lv = []
    for fc in 1000 * 2 ** (np.arange(-15, 13) / 3.0):
        lv.append((float(to_db(np.sqrt(np.mean(band(b, fc, 1 / 3) ** 2)))), float(to_db(np.sqrt(np.mean(band(a, fc, 1 / 3) ** 2))))))
    top = max(v[0] for v in lv)
    spec = [w - s for w, s in lv if w > top - 40]   # bands where the music is, not the noise floor
    ea, eb = to_db(envelope(a, 50.0)[:: SR // 20]), to_db(envelope(b, 50.0)[:: SR // 20])
    live = eb > eb.max() - 40
    return {"lag_ms": round(lag / SR * 1e3, 2), "level_db": round(float(gain), 2),
            "spectral_rms_db": round(float(np.sqrt(np.mean(np.square(spec)))), 2),
            "envelope_rms_db": round(float(np.sqrt(np.mean((ea[live] - eb[live]) ** 2))), 2)}


if __name__ == "__main__":
    args = sys.argv[1:]
    kitp = statp = None
    if "--kit" in args:
        i = args.index("--kit"); kitp = args[i + 1]; del args[i:i + 2]
    if "--static" in args:
        i = args.index("--static"); statp = args[i + 1]; del args[i:i + 2]
    r = analyze(args[0], args[1], kitp, statp)
    print(json.dumps({k: v for k, v in r.items() if k != "quality"}, indent=1))
    for q in r.get("quality", []):
        print(q["segment"], "clean" if q["clean"] else f"{len(q['events'])} events: {q['events'][:4]}")
