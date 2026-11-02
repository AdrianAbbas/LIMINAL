#!/usr/bin/env python3
"""LIMINAL's additions to the prepared recordings (run after prepare.py, on its output).

prepare.py (copied unchanged in spirit from LIGHT SEEKER) slices, tunes and levels the
recordings. The Forgotten Piano needs a few more measurements, written into the mapping
files so the plug-in does no analysis on its own:

  piano.json (short hits soft / medium / full), per note:
    onset      where the hammer strikes (samples)
    damper     where the recorded damper starts to stop the note (samples)
    hand       the handover point: where the held note crosses into the long sweep
    handDb     the short note's level around the handover (dB)
    longPos    the matching position in that key's long note (aligned on the waveform:
               the lag, within one period, where the two recordings agree best)
  piano-long.json (the long sweep, medium), per note:
    onset, damper, handDb-style levels
    tail       where the recording hands over to its resynthesised natural decay, and that
               decay: up to 32 partials (frequency ratio to the root, amplitude, T60), measured
               on the last held second before the damper. The recordings were held about
               2.6 s; a piano rings much longer, so the tail carries on decaying as measured.
  both:
    stretchCents  per key: the piano's own stretch tuning, a smooth curve through every
                  note's measured pitch, with its overall offset removed (A3-C5 average 0):
                  the plug-in tunes each note to key + stretch (overall tuning corrected,
                  stretch kept; each note's own deviation from the curve is reported).

Prints a short report and adds "liminal" to report.json.
Usage: liminal.py <prepared dir>
"""
import json
import math
import os
import sys

import numpy as np
import soundfile as sf

RATE = 44100


def env_db(x, hop=441):
    n = len(x) // hop
    e = np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1) + 1e-12)
    return 20 * np.log10(e)


def find_onset(x):
    peak = np.abs(x).max()
    i = int(np.argmax(np.abs(x) > 0.25 * peak))
    return max(0, i - 64)


def find_damper(x, onset, earliest):
    """The point where the note stops decaying naturally: from `earliest` on, the first 20 ms
    frame after which the level falls 12 dB more in the next 150 ms than the natural slope
    measured over the 300 ms before it."""
    hop = 441 * 2
    e = env_db(x[onset:], hop)
    start = int(earliest * RATE / hop)
    for i in range(max(start, 16), len(e) - 8):
        slope = (e[i] - e[i - 15]) / 15.0          # dB per frame, natural
        drop = e[i + 7] - e[i]
        if drop < 7 * slope - 12:
            return onset + i * hop
    return onset + len(e) * hop


def click_times(x, start=0):
    """Clicks in a recording (second-difference spikes, > 8x the local RMS over +/- 5 ms), after
    sample `start`: noises of the key, the damper or the room inside a note."""
    d = np.zeros_like(x)
    d[2:] = x[2:] - 2 * x[1:-1] + x[:-2]
    h = int(0.005 * RATE)
    c = np.concatenate([[0.0], np.cumsum(d.astype(np.float64) ** 2)])
    idx = np.arange(max(h, start), len(x) - h)
    if len(idx) == 0:
        return []
    local = np.sqrt((c[idx + h] - c[idx - h]) / (2 * h))
    hits = idx[np.abs(d[idx]) > 8 * local + 1e-7]
    out, last = [], -10 * h
    for i in hits:
        if i - last > h:
            out.append(int(i))
            last = i
    return out


def best_lag(a, b, centre, period, width):
    """Lag (within +/- one period) where b[centre + lag + k] matches a best."""
    best, best_c = 0, -2.0
    span = int(min(period, 1200))
    for lag in range(-span, span + 1, 1 if period < 200 else 2):
        s = centre + lag
        if s < 0 or s + width > len(b):
            continue
        seg = b[s:s + width]
        c = float(np.dot(a, seg) / (np.linalg.norm(a) * np.linalg.norm(seg) + 1e-12))
        if c > best_c:
            best, best_c = lag, c
    return best, best_c


def tail_modes(x, root, end, onset):
    """Partials of the held note just before `end` - fade (two 0.4 s windows), their decay rates;
    the tail's crossfade ends at `end`."""
    f0 = 440.0 * 2 ** ((root - 69) / 12)
    w = int(0.4 * RATE)
    fade = int(0.3 * RATE)
    at = end - fade
    b0, a0 = at - w, at - 2 * w
    if a0 < onset + int(0.12 * RATE):
        return None
    win = np.hanning(w)
    nfft = 1 << int(math.ceil(math.log2(w * 4)))
    A = np.abs(np.fft.rfft(x[a0:a0 + w] * win, nfft)) * 2 / win.sum()
    B = np.abs(np.fft.rfft(x[b0:b0 + w] * win, nfft)) * 2 / win.sum()
    freqs = np.fft.rfftfreq(nfft, 1 / RATE)
    modes = []
    taken = np.zeros_like(B, dtype=bool)
    floor = B.max() * 10 ** (-60 / 20)
    order = np.argsort(B)[::-1]
    guard = max(2, int(0.45 * f0 / (RATE / nfft)))
    for k in order:
        if len(modes) >= 32 or B[k] < floor:
            break
        f = freqs[k]
        if f < 0.85 * f0 or f > 16000 or taken[k]:
            continue
        lo, hi = max(0, k - guard), min(len(B), k + guard + 1)
        taken[lo:hi] = True
        if B[k] < B[max(0, k - 1)] or B[k] < B[min(len(B) - 1, k + 1)]:
            continue
        if 0 < k < len(B) - 1:
            y0, y1, y2 = np.log(B[k - 1] + 1e-12), np.log(B[k] + 1e-12), np.log(B[k + 1] + 1e-12)
            d = float(np.clip(0.5 * (y0 - y2) / (y0 - 2 * y1 + y2 + 1e-12), -0.5, 0.5))
        else:
            d = 0.0
        fk = (k + d) * RATE / nfft
        ampb = B[k]
        ampa = A[max(0, k - 2):k + 3].max()
        rate = (20 * math.log10(ampb + 1e-12) - 20 * math.log10(ampa + 1e-12)) / (w / RATE)   # dB per second
        t60 = float(np.clip(60.0 / max(0.4, -rate) if rate < 0 else 40.0, 0.6, 40.0))
        # Amplitude at the tail's start (window B's middle is 0.2 s earlier).
        amp = ampb * 10 ** (-60.0 / t60 * 0.2 / 20)
        modes.append([round(fk / f0, 5), float(amp), round(t60, 2)])
    modes.sort(key=lambda m: m[0])
    seg = x[b0:b0 + w]
    level = math.sqrt(float(np.mean(seg ** 2)))
    power = math.sqrt(sum(0.5 * (m[1] * 10 ** (60.0 / m[2] * 0.2 / 20)) ** 2 for m in modes)) + 1e-12
    scale = level / power
    for m in modes:
        m[1] = round(m[1] * scale, 6)
    return {"at": int(at), "fade": fade, "modes": modes}


def main():
    out = sys.argv[1]
    report_path = os.path.join(out, "report.json")
    report = json.load(open(report_path)) if os.path.exists(report_path) else {}
    lines = []
    sp = os.path.join(out, "piano.json")
    lp = os.path.join(out, "piano-long.json")
    if not (os.path.exists(sp) and os.path.exists(lp)):
        print("liminal.py: no piano sets, nothing to add")
        return
    short = json.load(open(sp))
    long_ = json.load(open(lp))
    audio = {}

    def load(z):
        if z["file"] not in audio:
            y, _ = sf.read(os.path.join(out, z["file"]), dtype="float32")
            audio[z["file"]] = y if y.ndim == 1 else y.mean(axis=1)
        return audio[z["file"]]

    # Stretch tuning: a smooth curve through the long notes' measured pitches.
    keys = np.array([round(z["root"]) for z in long_["zones"]], dtype=float)
    cents = np.array([(z["root"] - round(z["root"])) * 100 for z in long_["zones"]])
    use = np.ones_like(keys, dtype=bool)
    for _ in range(3):
        coef = np.polyfit(keys[use] - 64, cents[use], 3)
        resid = cents - np.polyval(coef, keys - 64)
        sd = max(3.0, float(np.std(resid[use])))
        use = np.abs(resid) < 2.5 * sd
    all_keys = np.arange(21, 109)
    curve = np.polyval(coef, all_keys - 64)
    mid = (all_keys >= 57) & (all_keys <= 72)
    offset = float(np.mean(curve[mid]))
    curve = curve - offset
    stretch = {str(int(k)): round(float(c), 2) for k, c in zip(all_keys, curve)}
    off_curve = sorted(((abs(r), int(k), float(r)) for k, r in zip(keys, resid)), reverse=True)
    lines.append(f"piano tuning: overall offset {offset:+.1f} cents corrected; stretch kept: A0 {stretch['21']:+.1f}, "
                 f"A2 {stretch['45']:+.1f}, A6 {stretch['93']:+.1f}, C8 {stretch['108']:+.1f} cents")
    lines.append("piano notes furthest from the stretch curve (corrected to it): "
                 + ", ".join(f"{k} {r:+.0f}c" for _, k, r in off_curve[:8]))

    # The long notes: onset, damper, natural-decay tail.
    by_key = {}
    held = []
    noisy = []
    for z in long_["zones"]:
        x = load(z)
        on = find_onset(x)
        dmp = find_damper(x, on, 1.2)
        z["onset"] = on
        z["damper"] = int(min(dmp, len(x) - 1))
        z["key"] = int(round(z["root"]))
        z.pop("tail", None)
        z.pop("bad", None)
        # The tail takes over before the damper, and before any noise inside the held note.
        clicks = [c for c in click_times(x, on + int(0.03 * RATE)) if c < z["damper"]]
        end = z["damper"] - int(0.05 * RATE)
        if clicks:
            end = min(end, clicks[0] - int(0.01 * RATE))
            noisy.append(f"{z['label']} at {(clicks[0] - on) / RATE:.2f} s")
        if end - on < int(0.75 * RATE) and z["key"] < 96:
            z["bad"] = True   # too short or noisy early: the neighbouring key's long note plays
        t = tail_modes(x, z["root"], end, on)
        if t and not z.get("bad"):
            z["tail"] = t
        held.append((z["damper"] - on) / RATE)
        by_key[z["key"]] = z
    lines.append(f"piano long notes: held {min(held):.2f}-{max(held):.2f} s (median {np.median(held):.2f}) before the damper; "
                 f"{sum(1 for z in long_['zones'] if 'tail' in z)}/{len(long_['zones'])} have a measured natural-decay tail")

    # The short hits: onset, damper, handover into the long note.
    aligned = []
    weak = []
    for z in short["zones"]:
        x = load(z)
        on = find_onset(x)
        dmp = find_damper(x, on, 0.12)
        z["onset"] = on
        z["damper"] = int(min(dmp, len(x) - 1))
        key = int(round(z["root"]))
        z["key"] = key
        lz = by_key.get(key)
        # A long note the recording cut short: the nearest well-held neighbour (the plug-in
        # plays it shifted to the key, see longKey).
        def good(q):
            return q is not None and not q.get("bad") and ("tail" in q or len(load(q)) - q["onset"] > int(0.9 * RATE))
        if not good(lz) and key < 96:
            for d in (1, -1, 2, -2):
                if good(by_key.get(key + d)):
                    lz = by_key[key + d]
                    break
        if lz is None:
            continue
        z["longKey"] = lz["key"]
        lx = load(lz)
        t = int(np.clip((dmp - on) / RATE - 0.1, 0.06, 0.28) * RATE)
        # Before any noise the short recording has before its damper (a key or pedal sound).
        sc = [c for c in click_times(x, on + int(0.03 * RATE)) if c < dmp]
        if sc:
            t = int(max(0.05 * RATE, min(t, sc[0] - on - int(0.06 * RATE))))
            if sc[0] - on < int(0.12 * RATE):
                noisy.append(f"{z['label']}-{short['layers'][z['layer']]} (short) at {(sc[0] - on) / RATE:.2f} s")
        hand = on + t
        f0 = 440.0 * 2 ** ((z["root"] - 69) / 12)
        period = RATE / f0
        half = int(max(512, min(2048, 1.5 * period)))
        half = min(half, hand)
        width = 2 * half
        a = x[hand - half:hand + half]
        centre = lz["onset"] + t - half
        lag, corr = best_lag(a, lx, centre, period, width)
        if corr < -1:   # nothing to compare with (the long note ends first)
            lag, corr = 0, 0.0
        z["hand"] = int(hand)
        z["longPos"] = int(lz["onset"] + t + lag)
        z["handDb"] = round(float(20 * np.log10(np.sqrt(np.mean(a ** 2)) + 1e-12)), 2)
        b = lx[max(0, z["longPos"] - half):z["longPos"] + half]
        z["longDb"] = round(float(20 * np.log10(np.sqrt(np.mean(b ** 2)) + 1e-12)), 2) if len(b) else z["handDb"]
        z["handCorr"] = round(corr, 3)
        aligned.append(corr)
        if corr < 0.5:
            weak.append(f"{z['label']}-{short['layers'][z['layer']]} ({corr:.2f})")
        if lz["key"] != key:
            weak.append(f"{z['label']}-{short['layers'][z['layer']]} (long note from {lz['label']})")
    lines.append("piano notes with a noise inside the held part (the tail or the handover comes before it; worth re-recording if early): "
                 + (", ".join(noisy) if noisy else "none"))
    lines.append(f"piano handover short -> long: waveform match median {float(np.median(aligned)) if aligned else 0:.2f}, "
                 f"{sum(1 for c in aligned if c < 0.5)} of {len(aligned)} under 0.5"
                 + (": " + ", ".join(weak[:12]) if weak else ""))
    # Crossfaded velocity layers (the plug-in starts each layer at its onset): how much each
    # layer's first 300 ms correlates with the next layer up, so the crossfade can keep the
    # level (two strikes of one string are partly in phase: an equal-power mix would swell or dip).
    by_layer = {(z["layer"], z["key"]): z for z in short["zones"]}
    rhos = []
    for z in short["zones"]:
        up = by_layer.get((z["layer"] + 1, z["key"]))
        if up is None:
            continue
        a, b = load(z), load(up)
        n = int(0.3 * RATE)
        sa, sb = a[max(0, z["onset"] - 48):][:n], b[max(0, up["onset"] - 48):][:n]
        n = min(len(sa), len(sb))
        rho = float(np.dot(sa[:n], sb[:n]) / (np.linalg.norm(sa[:n]) * np.linalg.norm(sb[:n]) + 1e-12))
        z["corrUp"] = round(rho, 3)
        z["rms300"] = round(float(20 * np.log10(np.sqrt(np.mean(sa[:n] ** 2)) + 1e-12)), 2)
        up["rms300"] = round(float(20 * np.log10(np.sqrt(np.mean(sb[:n] ** 2)) + 1e-12)), 2)
        rhos.append(rho)
    if rhos:
        lines.append(f"piano velocity layers: correlation with the next layer up {min(rhos):+.2f} to {max(rhos):+.2f} (median {np.median(rhos):+.2f}); the crossfade corrects for it")
    for m in (short, long_):
        m["stretchCents"] = stretch
    json.dump(short, open(sp, "w"), indent=1, allow_nan=False)
    json.dump(long_, open(lp, "w"), indent=1, allow_nan=False)
    report["liminal"] = lines
    json.dump(report, open(report_path, "w"), indent=1)
    for line in lines:
        print(line)


if __name__ == "__main__":
    main()
