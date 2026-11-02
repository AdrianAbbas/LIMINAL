#!/usr/bin/env python3
"""Prepares the developer's recordings for WORLDBUILDER's sampler (Milestone 15).

Reads the MP3s of the private Samples repository and writes, per instrument, mono
16-bit FLAC notes and a mapping file (key ranges, velocity layers, round-robins, loop
points, release tails) that the plugin's sampler loads. Nothing it writes belongs in
the public repository: the output goes to a build folder.

    python3 tools/sample-prep/prepare.py --source ../samples --out build/samples

Steps, for every recording:
 1. Decode, and take off the MP3 encoder's padding: GarageBand's MP3s carry Apple's
    iTunSMPB tag (encoder delay, padding, sample count); the LAME header's delay and
    padding are used instead when there is one. The decoder's own 529-sample delay is
    taken off with the encoder's.
 2. Gentle noise reduction: the room tone's noise spectrum, fitted per third of an
    octave to the recording's own quiet gaps, is taken off with a soft spectral gate
    (at most 9 dB, smoothed over time so it does not warble).
 3. Slice: onsets from the level's rise, the expected number picked by strength and
    tone (handling noises and finger clicks fall out), each note running until it fades
    into the floor or the next one starts.
 4. Detect each note's pitch (YIN, refined on its harmonics) and check it against the
    label: the sampler is tuned to what was played, the label only says where it goes.
 5. Trim (keeping the attack and the natural tail), take off DC, fade the ends, and
    normalise each instrument with one gain (so the velocity layers keep their dynamics).
 6. Sustained notes: a sustain loop (searched for the best match, crossfaded) and the
    point where the bow lifts (the recorded release tail).
 7. Write the FLACs, the mapping files and a report of everything it could not match.

Needs numpy, scipy and soundfile (libsndfile 1.1+ decodes MP3).

The Korg Minilogue sweeps (WAV, one patch per recording, "kind": "synth") are prepared by
sweeps.py: sliced by key, each note at its own detected pitch, loops for the long hits.
"""

import argparse
import json
import math
import os
import re
import struct
import sys

import numpy as np
import soundfile as sf
from scipy import signal

RATE = 44100
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
DECODER_DELAY = 529


def midi_of(name):
    m = re.fullmatch(r"([A-G])([#b]?)(-?\d)", name)
    if not m:
        raise ValueError(f"bad note name {name}")
    pc = NOTE_NAMES.index(m.group(1)) + (1 if m.group(2) == "#" else (-1 if m.group(2) == "b" else 0))
    return 12 * (int(m.group(3)) + 1) + pc


def name_of(midi):
    m = int(round(midi))
    return f"{NOTE_NAMES[m % 12]}{m // 12 - 1}"


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def k_weight(x, rate=RATE):
    """ITU-R BS.1770's K-weighting (a high shelf, then a high-pass) at any rate."""
    k = math.tan(math.pi * 1681.974450955533 / rate)
    q = 0.7071752369554196
    vh = 10 ** (3.999843853973347 / 20)
    vb = vh ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    shelf_b = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
    shelf_a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    k = math.tan(math.pi * 38.13547087602444 / rate)
    q = 0.5003270373238773
    a0 = 1 + k / q + k * k
    hp_a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    return signal.lfilter([1, -2, 1], hp_a, signal.lfilter(shelf_b, shelf_a, x))


def note_loudness(audio):
    """How loud a note sounds (LUFS of one channel): its loudest 400 ms, K-weighted,
    within the first 1.5 s (a bow takes a while to speak)."""
    y = k_weight(audio[:int(1.9 * RATE)])
    win = int(0.4 * RATE)
    if len(y) <= win:
        return float(-0.691 + 10 * np.log10(np.mean(y ** 2) + 1e-24))
    # A running mean by cumulative sums (a 17640-tap convolution crawls on denormals in
    # the faded tails).
    total = np.cumsum(np.concatenate(([0.0], y ** 2)))
    power = ((total[win:] - total[:-win]) / win)[:int(1.5 * RATE)]
    return float(-0.691 + 10 * np.log10(max(power.max(), 0.0) + 1e-24))


# ---------------------------------------------------------------------------
# 1. Decoding and the MP3 padding

def mp3_gapless(path):
    """(skip, count) from the iTunSMPB tag or the LAME header, or None."""
    with open(path, "rb") as f:
        head = f.read(1 << 18)
    # iTunSMPB in an ID3v2 COMM frame: " 00000000 DELAY PADDING COUNT ..."
    m = re.search(rb"iTunSMPB\x00+ ?([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{16})", head)
    if m:
        delay, padding, count = int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)
        return delay + DECODER_DELAY, count, f"iTunSMPB (encoder delay {delay}, padding {padding})"
    # A LAME/Xing header in the first frame.
    offset = 0
    if head[:3] == b"ID3":
        offset = 10 + ((head[6] << 21) | (head[7] << 14) | (head[8] << 7) | head[9])
    lame = head.find(b"LAME", offset, offset + 400)
    xing = max(head.find(b"Xing", offset, offset + 200), head.find(b"Info", offset, offset + 200))
    if lame >= 0 and xing >= 0:
        d = head[lame + 21:lame + 24]
        delay, padding = (d[0] << 4) | (d[1] >> 4), ((d[1] & 15) << 8) | d[2]
        frames = struct.unpack(">I", head[xing + 8:xing + 12])[0] if head[xing + 7] & 1 else 0
        if frames:
            return delay + DECODER_DELAY, frames * 1152 - delay - padding, f"LAME header (delay {delay}, padding {padding})"
    return None


AUDIO_EXT = (".mp3", ".wav", ".aif", ".aiff", ".flac")
_FILES = {}


def scan(source):
    """Every recording under the source, in subfolders too (`.git` and hidden folders
    skipped): relative path -> absolute path. Recordings used to sit in the root as MP3s;
    newer ones arrive as WAVs and in folders (e.g. `01-voice/`)."""
    out = {}
    for d, dirs, files in os.walk(source):
        dirs[:] = sorted(x for x in dirs if not x.startswith("."))
        for f in sorted(files):
            if f.lower().endswith(AUDIO_EXT) and not f.startswith("."):
                p = os.path.join(d, f)
                out[os.path.relpath(p, source).replace(os.sep, "/")] = p
    _FILES[os.path.abspath(source)] = out
    return out


def resolve(source, name):
    """The recording a manifest names: its relative path, or else its file name anywhere
    under the source (a recording moved into a folder is still found). None if missing."""
    files = _FILES.get(os.path.abspath(source)) or scan(source)
    if name in files:
        return files[name]
    hits = [p for rel, p in files.items() if os.path.basename(rel) == os.path.basename(name)]
    return hits[0] if hits else None


def used_name(source, path):
    return os.path.relpath(path, source).replace(os.sep, "/")


def to_rate(audio, rate):
    """Recordings at another rate (48 kHz interfaces) are resampled to the sampler's."""
    if rate == RATE:
        return audio
    g = math.gcd(RATE, int(rate))
    return signal.resample_poly(audio, RATE // g, int(rate) // g, axis=0)


def load(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    audio = to_rate(audio, rate)
    gap = mp3_gapless(path) if path.lower().endswith(".mp3") else None
    note = "no gapless information (nothing trimmed)"
    if gap:
        skip, count, note = gap
        audio = audio[skip:skip + count]
    # The recordings are dual mono; check, then keep one channel's worth.
    left, right = audio[:, 0], audio[:, -1]
    live = np.abs(left) > 1e-4
    corr = float(np.corrcoef(left[live], right[live])[0, 1]) if live.sum() > 1000 and audio.shape[1] > 1 else 1.0
    mono = audio.mean(axis=1)
    # Trailing digital silence (GarageBand exported past the end of the take).
    nz = np.nonzero(np.abs(mono) > 0)[0]
    end = int(nz[-1]) + 1 if len(nz) else 0
    return mono[:end], note, corr, len(mono) - end


# ---------------------------------------------------------------------------
# 2. Noise reduction

NFFT, HOP = 2048, 512


def stft(x):
    _, _, z = signal.stft(x, RATE, window="hann", nperseg=NFFT, noverlap=NFFT - HOP, boundary="even", padded=True)
    return z


def istft(z, length):
    _, x = signal.istft(z, RATE, window="hann", nperseg=NFFT, noverlap=NFFT - HOP, boundary=True)
    return x[:length]


def band_edges():
    # Thirds of an octave from 20 Hz, as FFT bins.
    freqs = np.fft.rfftfreq(NFFT, 1.0 / RATE)
    edges, f = [], 20.0
    while f < RATE / 2:
        edges.append(f)
        f *= 2 ** (1 / 3)
    return freqs, np.array(edges + [RATE / 2])


def room_profile(room):
    p = np.abs(stft(room)) ** 2
    live = p.sum(axis=0) > 1e-12
    return np.median(p[:, live], axis=1)


def fitted_profile(z, room):
    """The room tone's spectrum, scaled per third-octave to this file's quiet frames."""
    p = np.abs(z) ** 2
    energy = p.sum(axis=0)
    live = energy > 1e-12
    quiet = live & (energy <= np.percentile(energy[live], 15))
    if quiet.sum() < 20:
        return room * np.median(np.median(p[:, live], axis=1) / np.maximum(room, 1e-20)), 0
    gaps = np.median(p[:, quiet], axis=1)
    freqs, edges = band_edges()
    ratio = np.ones_like(room)
    for lo, hi in zip(edges[:-1], edges[1:]):
        sel = (freqs >= lo) & (freqs < hi)
        if sel.any():
            ratio[sel] = np.median(gaps[sel] / np.maximum(room[sel], 1e-20))
    # Below the first band: the first band's ratio.
    ratio[freqs < edges[0]] = ratio[(freqs >= edges[0])][0]
    return room * ratio, int(quiet.sum())


def denoise(x, room, max_db=9.0, over=1.6):
    z = stft(x)
    noise, quiet_frames = fitted_profile(z, room)
    p = np.abs(z) ** 2
    floor = 10 ** (-max_db / 20)
    gain = np.sqrt(np.maximum(1.0 - over * noise[:, None] / np.maximum(p, 1e-20), 0.0))
    gain = np.maximum(gain, floor)
    # Smooth across frequency (3 bins) and time (fast attack, ~60 ms release) so the gate
    # does not leave musical noise.
    gain = signal.convolve2d(gain, np.ones((3, 1)) / 3, mode="same", boundary="symm")
    release = math.exp(-HOP / (0.06 * RATE))
    out = gain.copy()
    for t in range(1, out.shape[1]):
        out[:, t] = np.maximum(gain[:, t], release * out[:, t - 1] + (1 - release) * gain[:, t])
    y = istft(z * out, len(x))
    return y, quiet_floor(x), quiet_floor(y), quiet_frames


def quiet_floor(x):
    """The level of the quiet gaps (the 10th percentile of 5 ms frames), dBFS."""
    env = envelope(x)
    return float(np.percentile(env[env > -200], 10))


# ---------------------------------------------------------------------------
# 3. Onsets and slicing

ENV_HOP = 220   # 5 ms


def envelope(x):
    n = len(x) // ENV_HOP
    frames = x[:n * ENV_HOP].reshape(n, ENV_HOP)
    return db(np.sqrt((frames ** 2).mean(axis=1)))


def onsets(env, bowed):
    """Candidate onsets: the level rising sharply out of a quieter stretch."""
    look = 60 if bowed else 20          # how far back the quieter stretch is (frames)
    rise = 14.0 if bowed else 12.0
    floor = np.percentile(env, 10)
    cands = []
    i = look
    while i < len(env) - 1:
        before = env[max(0, i - look):i].min()
        if env[i] - before >= rise and env[i] > floor + 18:
            # The peak within the next 300 ms (bowed: 1 s).
            j = min(len(env), i + (200 if bowed else 60))
            peak = int(i + np.argmax(env[i:j]))
            # Walk back to where it left the quieter level (+3 dB).
            start = i
            while start > 0 and env[start - 1] > before + 3 and i - start < look:
                start -= 1
            cands.append((start, peak, float(env[peak])))
            i = peak + (40 if bowed else 20)
        else:
            i += 1
    return cands, floor


def yin(x, fmin=55.0, fmax=2000.0, frame=2048, hop=512, threshold=0.15):
    """f0 and aperiodicity per frame."""
    tmax, tmin = int(RATE / fmin), max(2, int(RATE / fmax))
    out = []
    for s in range(0, len(x) - frame - tmax, hop):
        w = x[s:s + frame + tmax]
        if np.sqrt(np.mean(w ** 2)) < 1e-5:
            continue
        # Difference function by FFT.
        # r(tau) = sum over the frame of x[j] * x[j + tau]
        n = 1 << int(math.ceil(math.log2(len(w) * 2)))
        acf = np.fft.irfft(np.fft.rfft(w, n) * np.conj(np.fft.rfft(w[:frame], n)), n)[:tmax + 1]
        energy = np.cumsum(np.concatenate(([0.0], w ** 2)))
        e0 = energy[frame] - energy[0]
        et = energy[np.arange(tmax + 1) + frame] - energy[np.arange(tmax + 1)]
        d = np.maximum(e0 + et - 2 * acf, 0.0)
        d[0] = 0
        cmnd = d[1:] * np.arange(1, tmax + 1) / np.maximum(np.cumsum(d[1:]), 1e-20)
        cmnd = np.concatenate(([1.0], cmnd))
        tau = None
        for t in range(tmin, tmax):
            if cmnd[t] < threshold:
                while t + 1 < tmax and cmnd[t + 1] < cmnd[t]:
                    t += 1
                tau = t
                break
        if tau is None:
            tau = int(np.argmin(cmnd[tmin:tmax]) + tmin)
        a, b, c = cmnd[tau - 1], cmnd[tau], cmnd[min(tau + 1, tmax)]
        shift = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
        out.append((RATE / (tau + shift), float(b)))
    return out


def refine(x, f0, partials):
    """f0 from the harmonics' peaks in a long, zero-padded spectrum."""
    n = 1 << 18
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)), n))
    binhz = RATE / n
    estimates, weights = [], []
    for k in range(1, partials + 1):
        target = f0 * k
        lo, hi = int(target * 0.97 / binhz), int(target * 1.03 / binhz) + 1
        if hi >= len(spec) - 1:
            break
        i = lo + int(np.argmax(spec[lo:hi]))
        a, b, c = np.log(spec[i - 1] + 1e-20), np.log(spec[i] + 1e-20), np.log(spec[i + 1] + 1e-20)
        shift = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
        estimates.append((i + shift) * binhz / k)
        weights.append(spec[i] / k)
    if not estimates:
        return f0
    return float(np.average(estimates, weights=weights))


def analysis_segment(x, onset, bowed):
    start = onset + int((0.35 if bowed else 0.07) * RATE)
    return x[start:start + int((1.2 if bowed else 0.6) * RATE)]


def pitch_of(x, onset, bowed, partials, near=None):
    """(MIDI pitch, aperiodicity). With `near` (a MIDI note) the search stays within 8
    semitones of it: a string ringing in sympathy (the guitar's open A, a violin's open
    strings) can be stronger than a high note's own fundamental."""
    seg = analysis_segment(x, onset, bowed)
    if len(seg) < 4096:
        return None, 1.0
    if near is None:
        frames = yin(seg)
    else:
        fmin = 440 * 2 ** ((near - 8 - 69) / 12)
        # Strings ringing far below the search range would still win YIN's dips.
        seg = signal.sosfiltfilt(signal.butter(4, 0.8 * fmin, "highpass", fs=RATE, output="sos"), seg)
        frames = yin(seg, fmin=fmin, fmax=440 * 2 ** ((near + 8 - 69) / 12))
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 3:
        return None, (min(ap for _, ap in frames) if frames else 1.0)
    f0 = float(np.median(good))
    f0 = refine(seg[:int(0.5 * RATE)], f0, partials)
    aper = float(np.median([ap for _, ap in frames]))
    return 69 + 12 * math.log2(f0 / 440.0), aper


def sung_notes(x, env, gap_db=30.0, min_seconds=0.25):
    """A singer's notes, by the gaps between them: every stretch within `gap_db` of the
    file's loudest moment (bridging dips under 200 ms), as candidates for `pick`. A voice
    swells and breathes inside a note, so onsets from the level's rise split notes."""
    sm = np.convolve(env, np.ones(8) / 8, mode="same")
    floor = float(np.percentile(sm, 5))
    on = sm > max(floor + 12, sm.max() - gap_db)
    out, i, n = [], 0, len(on)
    while i < n:
        if on[i]:
            j = i
            while j < n and (on[j] or (j + 40 < n and on[j:j + 40].any())):
                j += 1
            if (j - i) * ENV_HOP / RATE >= min_seconds:
                peak = int(i + np.argmax(env[i:j]))
                out.append({"start": i, "end": j, "peak": peak, "level": float(env[peak]), "midi": None, "aper": 0.0})
            i = j
        else:
            i += 1
    return out


def free_pitch(x, onset, bowed, partials, fmin=55.0, fmax=1100.0, seg=None):
    """The pitch the note really has, whatever its name says (the voice recordings are
    named an octave below what was sung): YIN over a wide range, then an octave check both
    ways on the spectrum: a real fundamental below the candidate (energy at half the
    candidate and at 1.5 times it) moves it down, a candidate with no odd harmonics moves
    it up. (MIDI pitch, aperiodicity) or (None, 1)."""
    if seg is None:
        seg = analysis_segment(x, onset, bowed)
    if len(seg) < 4096:
        return None, 1.0
    frames = yin(seg, fmin=fmin, fmax=fmax)
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 3:
        return None, (min(ap for _, ap in frames) if frames else 1.0)
    m = 69 + 12 * np.log2(np.array(good) / 440.0)
    med = float(np.median(m))
    m = m[np.abs(m - med) < 1.0]
    f0 = 440 * 2 ** ((float(np.median(m)) - 69) / 12) if len(m) else 440 * 2 ** ((med - 69) / 12)
    n = 1 << 18
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    binhz = RATE / n

    def level(hz):
        lo, hi = int(hz * 0.985 / binhz), int(hz * 1.015 / binhz) + 1
        return float(db(spec[lo:hi].max())) if hi < len(spec) and lo > 0 else -240.0
    for _ in range(2):
        # Both odd sub-harmonics clearly there (not one stray peak in the noise).
        if f0 / 2 >= fmin * 0.9 and min(level(f0 / 2), level(1.5 * f0)) > level(f0) - 25:
            f0 /= 2
        else:
            break
    h = [level(f0 * k) for k in (1, 2, 3, 4)]
    if max(h[0], h[2]) < max(h[1], h[3]) - 30:
        f0 *= 2
    f0 = refine(seg[:int(0.5 * RATE)], f0, partials)
    return 69 + 12 * math.log2(f0 / 440.0), float(np.median([ap for _, ap in frames]))


def harmonic_levels(seg, f0, count=4):
    n = 1 << 17
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    binhz = RATE / n
    out = []
    for k in range(1, count + 1):
        lo, hi = int(f0 * k * 0.98 / binhz), int(f0 * k * 1.02 / binhz) + 1
        out.append(float(db(spec[lo:hi].max())) if hi < len(spec) else -240.0)
    return out


def octave_check(x, onset, bowed, midi):
    """+12 when the note has no odd harmonics of `midi` (it sounds an octave higher)."""
    seg = analysis_segment(x, onset, bowed)
    h = harmonic_levels(seg, 440 * 2 ** ((midi - 69) / 12))
    odd, even = max(h[0], h[2]), max(h[1], h[3])
    return 12 if odd < even - 30 else 0


def sympathetic(y, f0):
    """Notches steady tones below the note's fundamental: other strings ringing in
    sympathy (they are not this note, and would drone in every key it is played in)."""
    removed = []
    if f0 > 250:
        # Above the guitar's G string, nothing of the note lies below half its pitch:
        # a gentle high-pass takes the ringing open strings down first.
        y = signal.sosfiltfilt(signal.butter(2, 0.5 * f0, "highpass", fs=RATE, output="sos"), y)
        removed.append((0.5 * f0, None))
    seg = y[int(0.3 * RATE):int(1.5 * RATE)]
    if len(seg) < RATE // 2 or f0 < 120:
        return y, removed
    n = 1 << 16
    spec = db(np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)))
    binhz = RATE / n
    top = spec[int(f0 * 0.97 / binhz):int(f0 * 1.03 / binhz)].max()
    lo, hi = int(60 / binhz), int(f0 * 0.85 / binhz)
    region = spec[lo:hi]
    if len(region) < 20:
        return y, []
    base = signal.medfilt(region, 41)
    peaks, _ = signal.find_peaks(region, prominence=12, distance=int(15 / binhz))
    for k in peaks:
        level = region[k]
        if level - base[k] < 15 or level < top - 30:
            continue
        hz = (lo + k) * binhz
        b, a = signal.iirnotch(hz, 12, RATE)
        y = signal.filtfilt(b, a, y)
        removed.append((hz, level - top))
    return y, removed


# ---------------------------------------------------------------------------
# 6. Loops and release tails

def sustain_region(x):
    env = envelope(x)
    if len(env) < 100:
        return None
    sm = np.convolve(env, np.ones(10) / 10, mode="same")
    level = np.percentile(sm, 80)
    above = np.nonzero(sm >= level - 4)[0]
    if len(above) < 40:
        return None
    attack_end = max(int(above[0]) + 20, 30)          # 100 ms past reaching the sustain
    release = int(above[-1])
    return attack_end * ENV_HOP, release * ENV_HOP


def find_loop(x, f0_hz, region):
    """Loop start and end (end exclusive) with the best-matching waveforms."""
    a, b = region
    if b - a < int(0.6 * RATE):
        return None
    period = RATE / f0_hz
    win = 512
    end_lo, end_hi = b - int(0.35 * RATE), b - int(0.05 * RATE)
    start_lo, start_hi = a, a + int(0.4 * (b - a))
    best = None
    # Ends on upward zero crossings near the end; starts anywhere in the window, scored
    # by the normalised difference of the waveforms around them.
    ends = [i for i in range(max(end_lo, win), min(end_hi, len(x) - win)) if x[i - 1] < 0 <= x[i]]
    step = max(1, len(ends) // 40)
    for e in ends[::step]:
        ref = x[e - win:e + win]
        region_s = x[start_lo - win:start_hi + win]
        if len(region_s) < 2 * win + 1:
            continue
        corr = signal.correlate(region_s, ref, mode="valid")
        energy = np.convolve(region_s ** 2, np.ones(2 * win), mode="valid")[:len(corr)]
        diff = energy - 2 * corr + np.sum(ref ** 2)
        norm = diff / np.maximum(energy + np.sum(ref ** 2), 1e-20)
        k = int(np.argmin(norm))
        s = start_lo + k
        length = e - s
        if length < int(0.5 * RATE):
            continue
        cost = float(norm[k])
        # Prefer longer loops a little.
        score = cost - 0.02 * length / RATE
        if best is None or score < best[0]:
            best = (score, s, e, cost)
    if best is None:
        return None
    _, s, e, cost = best
    return s, e, cost, period


def crossfade_loop(x, s, e, xf):
    """Blends the end of the loop into what precedes its start, so the jump is seamless.
    Two stretches of a bowed note are only partly alike, so a plain crossfade dips in
    level where they cancel: the blend is levelled (per 10 ms) to the level the two
    stretches have on their own."""
    xf = min(xf, s, (e - s) // 3)
    if xf <= 0:
        return x
    w = np.linspace(0.0, 1.0, xf)
    a, b = x[e - xf:e], x[s - xf:s]
    y = x.copy()
    blend = a * (1 - w) + b * w
    win = int(0.01 * RATE)
    rms = lambda v: np.sqrt(np.convolve(v ** 2, np.ones(win) / win, mode="same") + 1e-18)
    target = rms(a) * (1 - w) + rms(b) * w
    gain = np.clip(target / rms(blend), 0.5, 2.5)
    gain = np.convolve(gain, np.hanning(2 * win) / np.hanning(2 * win).sum(), mode="same")
    # The ends stay untouched (gain 1) so the blend joins the waveform on both sides.
    edge = np.minimum(1.0, np.minimum(np.arange(xf), np.arange(xf)[::-1]) / win)
    y[e - xf:e] = blend * (1 + (gain - 1) * edge)
    return y


# ---------------------------------------------------------------------------
# The sets

def evidence(x, onset, bowed, midis):
    """How clearly each expected pitch sounds in the note starting at `onset`: the
    prominence (dB, 0..30) of its fundamental or second harmonic over the spectrum
    around it."""
    seg = analysis_segment(x, onset, bowed)
    if len(seg) < 4096:
        return [0.0] * len(midis)
    n = 1 << 16
    spec = db(np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)))
    binhz = RATE / n
    out = []
    for m in midis:
        f0 = 440 * 2 ** ((m - 69) / 12)
        best = 0.0
        for k in (1, 2):
            lo, hi = int(f0 * k * 2 ** (-0.4 / 12) / binhz), int(f0 * k * 2 ** (0.4 / 12) / binhz) + 1
            wlo, whi = int(f0 * k * 2 ** (-3 / 12) / binhz), int(f0 * k * 2 ** (3 / 12) / binhz) + 1
            if whi >= len(spec):
                continue
            best = max(best, spec[lo:hi].max() - np.median(spec[wlo:whi]))
        out.append(float(np.clip(best, 0.0, 30.0)))
    return out


def pick(cands, x, bowed, partials, sequence):
    """Chooses one onset per expected note, in order (`sequence`: the expected MIDI notes,
    each repeated per take). Each onset is scored by how clearly the expected pitch
    sounds in it and by its level, and the best in-order assignment wins, so a handling
    noise or a thump never shifts the labels of the notes after it."""
    scored = []
    for start, peak, level in cands:
        onset = start * ENV_HOP
        midi, aper = pitch_of(x, onset, bowed, partials)
        scored.append({"start": start, "peak": peak, "level": level, "midi": midi, "aper": aper})
    if not sequence:
        # A varying number of takes: every tonal onset.
        pool = [c for c in scored if c["midi"] is not None and c["aper"] < 0.35]
        return pool, [c for c in scored if c not in pool]
    labels = sorted(set(sequence))
    top = max(c["level"] for c in scored) if scored else 0.0
    for c in scored:
        ev = dict(zip(labels, evidence(x, c["start"] * ENV_HOP, bowed, labels)))
        c["score"] = [ev[m] + 0.25 * max(0.0, 40.0 + c["level"] - top) for m in sequence]
    n, m = len(scored), len(sequence)
    if n < m:
        return scored, []
    # best[i][j]: the best total with the first j notes assigned among the first i onsets.
    neg = -1.0e18
    best = np.full((n + 1, m + 1), neg)
    best[:, 0] = 0.0
    take = np.zeros((n + 1, m + 1), dtype=bool)
    for i in range(1, n + 1):
        for j in range(1, min(i, m) + 1):
            skip = best[i - 1][j]
            use = best[i - 1][j - 1] + scored[i - 1]["score"][j - 1]
            if use >= skip:
                best[i][j], take[i][j] = use, True
            else:
                best[i][j] = skip
    chosen, i, j = [], n, m
    while j > 0:
        if take[i][j]:
            chosen.append(scored[i - 1])
            j -= 1
        i -= 1
    chosen.reverse()
    return chosen, [c for c in scored if c not in chosen]


def slice_note(x, onset_frame, next_frame, env, floor, max_seconds, bowed):
    start = max(0, onset_frame * ENV_HOP - int((0.03 if bowed else 0.006) * RATE))
    stop_limit = len(x) if next_frame is None else next_frame * ENV_HOP - int(0.02 * RATE)
    stop_limit = min(stop_limit, start + int(max_seconds * RATE))
    # The end: the smoothed level fallen to the floor (+5 dB) and staying there.
    sm = np.convolve(env, np.ones(10) / 10, mode="same")
    peak = onset_frame + int(np.argmax(sm[onset_frame:onset_frame + 200]))
    end = stop_limit
    for i in range(peak, min(len(sm), stop_limit // ENV_HOP)):
        if sm[i] < floor + 5:
            end = min(stop_limit, (i + 20) * ENV_HOP)
            break
    # Leading near-silence (a slow bow's onset walked back too far): start 10 ms before
    # the level first comes within 40 dB of the note's peak.
    env_note = envelope(x[start:end])
    if len(env_note):
        first = int(np.argmax(env_note >= env_note.max() - 40.0))
        start += max(0, first * ENV_HOP - int(0.01 * RATE))
    y = x[start:end].copy()
    # Fades: a short one in (the attack stays), a longer cosine one out.
    fin = min(int((0.012 if bowed else 0.002) * RATE), len(y) // 4)
    y[:fin] *= np.linspace(0, 1, fin)
    fout = min(int(np.clip(0.15 * len(y), 0.05 * RATE, 0.4 * RATE)), len(y) // 2)
    y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
    return y, start


def layer_names(spec):
    """The set's velocity layers, in order: layers that share a name are one layer (two
    files of the same dynamic: extra round-robins); a file can also feed several layers
    ("into": a falsetto take that plays at every velocity)."""
    names = []
    for layer in spec["layers"]:
        for n in layer.get("into", [layer["name"]]):
            if n not in names:
                names.append(n)
    return names


def layer_velocity(spec, name):
    for layer in spec["layers"]:
        if layer["name"] == name and "into" not in layer:
            return layer["velocity"]
    return [0.0, 1.0]


def texture_notes(x, env, floor, spec):
    """Unpitched sounds (breaths): every stretch that rises 12 dB over the floor and
    lasts long enough, as one take each."""
    sm = np.convolve(env, np.ones(8) / 8, mode="same")
    on = sm > max(floor + 12, sm.max() - spec.get("rangeDb", 30))
    out, i, n = [], 0, len(on)
    while i < n:
        if on[i]:
            j = i
            while j < n and (on[j] or (j + 40 < n and on[j:j + 40].any())):
                j += 1
            if (j - i) * ENV_HOP / RATE >= spec.get("minSeconds", 0.4):
                out.append((i, j))
            i = j
        else:
            i += 1
    return out


def process_set(spec, source, room, out_dir, report):
    """Finds, slices, tunes, loops and levels a set's notes (nothing written yet: a group
    of sets recorded at one gain gets one gain, `finish_set`)."""
    sid = spec["id"]
    kind = spec["kind"]
    bowed = kind in ("bowed", "voice")
    voice = kind == "voice"
    from_audio = spec.get("pitchFrom") == "audio"
    partials = spec.get("partials", 3)
    names = layer_names(spec)
    zones, floors = [], {}
    merged_rr = {}
    for layer in spec["layers"]:
        path = resolve(source, layer["file"])
        notes = layer.get("notes", spec.get("notes", []))
        expected = [midi_of(n) for n in notes]
        takes = layer.get("takes", spec.get("takes", 1))
        count = len(expected) * takes if takes > 0 else 0
        targets = [0] if spec.get("mergeLayers") else [names.index(n) for n in layer.get("into", [layer["name"]])]
        if path is None:
            report["problems"].append(f"{sid}: missing file '{layer['file']}'")
            continue
        raw, gap_note, corr, trailing = load(path)
        if spec.get("denoise", True):
            x, before, after, quiet = denoise(raw, room)
        else:
            x, before, after, quiet = raw, quiet_floor(raw), quiet_floor(raw), 0
        floors.setdefault(targets[0], []).append(before)
        x = signal.sosfiltfilt(signal.butter(2, 25, "highpass", fs=RATE, output="sos"), x)
        env = envelope(x)
        entry = {
            "file": used_name(source, path), "set": sid, "layer": layer["name"], "padding": gap_note,
            "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
            "trailingSilence": round(trailing / RATE, 2), "noiseFloorDb": round(before, 1),
            "noiseAfterDb": round(after, 1), "quietFrames": quiet, "rejected": [], "notes": [],
        }
        report["files"].append(entry)
        if kind == "texture":
            floor = float(np.percentile(env, 10))
            spans = texture_notes(x, env, floor, spec)
            entry.update({"expected": "varies", "found": len(spans), "candidates": len(spans)})
            for k, (i0, i1) in enumerate(spans):
                start = max(0, i0 * ENV_HOP - int(0.1 * RATE))
                y = x[start:min(len(x), i1 * ENV_HOP + int(0.3 * RATE))].copy()
                fade = int(0.05 * RATE)
                y[:fade] *= np.linspace(0, 1, fade)
                y[-fade:] *= np.linspace(1, 0, fade)
                for li in targets:
                    rr = merged_rr.get((li, 60), 0)
                    merged_rr[(li, 60)] = rr + 1
                    zones.append({"label": "C4", "root": 60.0, "layer": li, "velocity": [0.0, 1.0], "rr": rr, "audio": y,
                                  "source": entry["file"], "at": round(start / RATE, 3), "fixedRange": True})
                entry["notes"].append({"label": "-", "at": round(start / RATE, 2), "seconds": round(len(y) / RATE, 2), "detected": "unpitched"})
            continue
        cands, floor = onsets(env, bowed)
        sequence = [m for m in expected for _ in range(takes)] if takes > 0 else []
        tracked = spec.get("pitchFrom") == "track"
        if tracked:
            # A chromatic sweep (the piano: no notes.txt entry, every key from the lowest
            # up, a few played twice): every clear onset, each pitch looked for near the
            # key after the one before it, so a repeated or skipped key never shifts the
            # labels after it. Takes of one key become its round-robins.
            chosen = [c for c in cands if c[2] > max(c_[2] for c_ in cands) - spec.get("rangeDb", 45)]
            chosen = [{"start": a_, "peak": p_, "level": l_, "midi": None, "aper": 0.0} for a_, p_, l_ in chosen]
            rejected = []
            expect = midi_of(spec.get("lowest", "A0"))
            for c in chosen:
                m_, _ = pitch_of(x, c["start"] * ENV_HOP, False, partials, near=expect + 2)
                if m_ is not None and abs(m_ - (expect + 2)) <= 4.5:
                    c["track"] = m_
                    expect = int(round(m_)) + 1
                else:
                    c["track"] = None
            count = 0
        elif voice:
            sung = sung_notes(x, env)
            chosen, rejected = sung, []
            if count and len(chosen) > count:
                keep = sorted(sorted(chosen, key=lambda c: -(c["end"] - c["start"]))[:count], key=lambda c: c["start"])
                rejected = [c for c in chosen if c not in keep]
                chosen = keep
            cands = sung
        elif from_audio:
            # The names say where a note was meant; the audio says what was sung. Every
            # tonal onset, in order; the count is checked against the names.
            chosen, rejected = pick(cands, x, bowed, partials, [])
            if count and len(chosen) > count:
                # The weakest extras go (a breath or a false start).
                keep = sorted(sorted(chosen, key=lambda c: -c["level"])[:count], key=lambda c: c["start"])
                rejected += [c for c in chosen if c not in keep]
                chosen = keep
        else:
            chosen, rejected = pick(cands, x, bowed, partials, sequence)
        if "pick" in layer:
            lo, hi = layer["pick"]
            if len(chosen) == layer.get("fileTakes", len(chosen)):
                chosen = chosen[lo:hi]
                sequence = sequence[:hi - lo]
            else:
                report["problems"].append(f"{sid}/{layer['name']}: expected {layer.get('fileTakes')} notes in the file, found {len(chosen)}; "
                                          f"cannot tell the dynamics apart, layer skipped")
                chosen = []
            count = hi - lo
        entry.update({"expected": count if count else "varies", "found": len(chosen), "candidates": len(cands),
                      "rejected": [f"{c['start'] * ENV_HOP / RATE:.2f}s ({c['level']:.0f} dB)" for c in rejected]})
        if count and len(chosen) != count:
            report["problems"].append(f"{sid}/{layer['name']}: expected {count} notes, found {len(chosen)}")
        in_order = takes > 0 and len(chosen) == count and not from_audio
        rr_count = {}
        heard = {}
        if from_audio:
            for c in chosen:
                if "end" in c:
                    a_, b_ = c["start"] * ENV_HOP, c["end"] * ENV_HOP
                    mid = x[a_ + (b_ - a_) // 4:b_ - (b_ - a_) // 5][:int(3 * RATE)]
                    heard[c["start"]] = free_pitch(x, a_, bowed, partials, seg=mid)[0]
                else:
                    heard[c["start"]] = free_pitch(x, c["start"] * ENV_HOP, bowed, partials)[0]
            if len(expected) == 1 and takes > 1:
                # One pitch sung several times (a choir stack, a drone): one label for the
                # file, the median of its takes; a take the detector heard an octave off is
                # folded back, one more than a semitone from the others is not used.
                found = [m for m in heard.values() if m is not None]
                if found:
                    centre = float(np.median(found))
                    for k_, m in list(heard.items()):
                        if m is None:
                            continue
                        m -= 12 * round((m - centre) / 12)
                        heard[k_] = m if abs(m - centre) <= 1.0 else None
                        if heard[k_] is None:
                            report["warnings"].append(f"{sid}/{layer['name']} take at {k_ * ENV_HOP / RATE:.2f}s: {name_of(m)} {100 * (m - round(m)):+.0f} cents, "
                                                      f"more than a semitone from the other takes ({name_of(centre)}); not used")
                    heard["label"] = int(round(centre))
        for k, c in enumerate(chosen):
            nxt = chosen[k + 1]["start"] if k + 1 < len(chosen) else None
            onset = c["start"] * ENV_HOP
            named = expected[min(k // max(takes, 1), len(expected) - 1)] if expected and takes > 0 and len(chosen) == count else None
            if tracked:
                midi = c.get("track")
                if midi is None:
                    entry["rejected"].append(f"{onset / RATE:.2f}s (no pitch near the next key)")
                    continue
                label = int(round(midi))
            elif from_audio:
                midi = heard.get(c["start"])
                if midi is None:
                    entry["rejected"].append(f"{onset / RATE:.2f}s (no steady pitch, or off the others)")
                    continue
                label = heard.get("label", int(round(midi)))
            elif in_order:
                label = expected[k // takes]
            elif c["midi"] is not None:
                label = min(expected, key=lambda m: min(abs(m + o - c["midi"]) for o in (-12, 0, 12)))
                if min(abs(label + o - c["midi"]) for o in (-12, 0, 12)) > 0.5:
                    entry["rejected"].append(f"{c['start'] * ENV_HOP / RATE:.2f}s (sounds like {name_of(c['midi'])}, not a string)")
                    continue
            else:
                report["problems"].append(f"{sid}/{layer['name']}: note at {c['start'] * ENV_HOP / RATE:.2f}s has no pitch; skipped")
                continue
            y, start = slice_note(x, c["start"], nxt, env, floor, spec.get("maxSeconds", 6.0), bowed)
            if len(y) < spec.get("minNoteSeconds", 0.0) * RATE:
                entry["rejected"].append(f"{onset / RATE:.2f}s (only {len(y) / RATE:.2f} s: a false start)")
                continue
            if not from_audio and not tracked:
                midi, _ = pitch_of(x, onset, bowed, partials, near=label)
            note = {"label": name_of(label), "at": round(start / RATE, 2), "seconds": round(len(y) / RATE, 2)}
            where = f"{sid}/{layer['name']} {name_of(label)} at {start / RATE:.2f}s"
            if from_audio or tracked:
                cents = 100 * (midi - label)
                note["detected"] = f"{cents:+.0f} cents"
                if named is not None and named != label:
                    note["named"] = name_of(named)
                    report["renamed"].append(f"{where}: named {name_of(named)}, sounds {name_of(label)} {cents:+.0f} cents; mapped where it sounds")
                if abs(cents) > 25:
                    report["warnings"].append(f"{where}: {cents:+.0f} cents out (the sampler plays it in tune)")
            elif midi is None:
                found, _ = pitch_of(x, onset, bowed, partials)
                midi = float(label)
                note["detected"] = None
                report["problems"].append(f"{where}: no pitch within 8 semitones of the label"
                                          + (f" (sounds like {name_of(found)})" if found else "") + "; using the label")
            else:
                cents = 100 * (midi - label)
                if abs(cents) >= 50 and abs(abs(cents) - 1200) >= 50:
                    ev_label, ev_found = evidence(x, onset, bowed, [label, int(round(midi))])
                    if ev_label >= ev_found - 3 and ev_label >= 12:
                        report["warnings"].append(f"{where}: the pitch detector heard {name_of(midi)} (other strings ringing louder), "
                                                  f"but {name_of(label)}'s harmonics are as clear; kept the label")
                        midi = float(label)
                        cents = 0.0
                if octave_check(x, onset, bowed, midi):
                    midi += 12
                    report["problems"].append(f"{where}: no odd harmonics of {name_of(label)}; it sounds an octave higher ({name_of(midi)}), mapped there")
                    cents = 100 * (midi - label)
                note["detected"] = f"{name_of(midi)} {cents:+.0f} cents from the label" if abs(cents) >= 50 else f"{cents:+.0f} cents"
                if 50 <= abs(cents) < 1150:
                    report["problems"].append(f"{where}: played {name_of(midi)} ({cents:+.0f} cents from the label); mapped where it sounds")
                elif abs(cents) > 25 and abs(cents) < 50:
                    report["warnings"].append(f"{where}: {cents:+.0f} cents out (the sampler plays it in tune)")
            if not voice:
                y, removed = sympathetic(y, 440 * 2 ** ((midi - 69) / 12))
                if removed:
                    note["sympathetic"] = ", ".join(f"high-pass {hz:.0f} Hz" if lv is None else f"{hz:.0f} Hz ({name_of(69 + 12 * math.log2(hz / 440))}, {lv:+.0f} dB)" for hz, lv in removed)
            zone = {"label": name_of(label), "root": round(midi, 3), "velocity": layer.get("velocity", [0.0, 1.0]),
                    "audio": y, "source": entry["file"], "at": round(start / RATE, 3)}
            if bowed and spec.get("loop"):
                region = sustain_region(y)
                loop = find_loop(y, 440 * 2 ** ((midi - 69) / 12), region) if region else None
                if loop:
                    s, e, cost, _ = loop
                    frames = yin(y[s:e], fmin=440 * 2 ** ((label - 8 - 69) / 12), fmax=440 * 2 ** ((label + 8 - 69) / 12))
                    steady = [f for f, ap in frames if ap < 0.3]
                    if len(steady) >= 5:
                        spread = float(np.std([1200 * math.log2(f / np.median(steady)) for f in steady]))
                        zone["wander"] = spread
                        note["loop wander"] = f"{spread:.0f} cents"
                        held_midi = 69 + 12 * math.log2(float(np.median(steady)) / 440.0)
                        if abs(held_midi - midi) > 0.05:
                            note["detected"] += f", held {100 * (held_midi - label):+.0f} cents"
                        midi = held_midi
                        zone["root"] = round(midi, 3)
                    zone["audio"] = crossfade_loop(y, s, e, int(0.15 * RATE))
                    zone["loop"] = [s, e]
                    zone["release"] = int(region[1])
                    note["loop"] = f"{s / RATE:.2f}-{e / RATE:.2f}s (match {cost:.3f})"
                    if cost > 0.1:
                        report["warnings"].append(f"{where}: the best loop matches poorly ({cost:.3f}); crossfaded")
                else:
                    report["problems"].append(f"{where}: no sustain loop found (too short)")
            for li in targets:
                key = (li, label)
                rr = merged_rr.get(key, 0)
                merged_rr[key] = rr + 1
                z = dict(zone)
                z["layer"], z["rr"] = li, rr
                z["file_take"] = (entry["file"], len(expected) == 1 and takes > 1)
                if "into" in layer:
                    z["velocity"] = layer_velocity(spec, names[li])
                zones.append(z)
            rr_count[label] = rr_count.get(label, 0) + 1
            entry["notes"].append(note)
        entry["found"] = len(entry["notes"])
        if takes == 0 or from_audio or tracked:
            entry["takesPerNote"] = {name_of(k): v for k, v in sorted(rr_count.items())}
        if not from_audio and not tracked:
            for m in expected:
                if m not in rr_count:
                    report["problems"].append(f"{sid}/{layer['name']}: no note matched {name_of(m)}")
    if not zones:
        return None
    if from_audio:
        # Labels from what is held (the loop's pitch), not the onset's scoop: each note at
        # its nearest semitone, and every take of a one-pitch file (a stack, a drone) at
        # the nearest semitone of their median.
        for f in {z["file_take"] for z in zones}:
            mine = [z for z in zones if z["file_take"] == f]
            centre = int(round(float(np.median([z["root"] for z in mine])))) if f[1] else None
            for z in mine:
                new = name_of(centre if centre is not None else z["root"])
                if new != z["label"]:
                    report["renamed"].append(f"{sid}: {f[0]} take at {z['at']:.2f}s held at {name_of(z['root'])} {100 * (z['root'] - round(z['root'])):+.0f} cents; labelled {new}")
                z["label"] = new
    if spec.get("pitchFrom") == "track":
        # A second onset inside a ringing bass note (the hammer's rebound, a string's
        # beat) is not a new take: takes more than 10 dB under the key's strongest go.
        for key in {(z["layer"], z["label"]) for z in zones}:
            mine = [z for z in zones if (z["layer"], z["label"]) == key]
            top = max(float(np.abs(z["audio"]).max()) for z in mine)
            for z in mine:
                if float(np.abs(z["audio"]).max()) < top * 10 ** (-10 / 20) or (spec.get("oneTake") and float(np.abs(z["audio"]).max()) < top):
                    zones.remove(z)
                    report["warnings"].append(f"{sid} {z['label']} ({names[z['layer']]}) at {z['at']:.2f}s: a re-onset inside the note "
                                              f"(10 dB under the key's strongest); not a take")
    # A held take whose pitch wanders while held would wobble every time the loop turns:
    # dropped when the note has other takes (the voice sings with vibrato: its limit is wider).
    limit = spec.get("maxWander", 12)
    for z in [z for z in zones if z.get("wander", 0) > limit]:
        others = [o for o in zones if o is not z and o["label"] == z["label"] and o["layer"] == z["layer"] and o.get("wander", 0) <= limit]
        if others:
            zones.remove(z)
            report["warnings"].append(f"{sid} {z['label']} take {z['rr'] + 1}: dropped, its pitch wanders while held ({z['wander']:.0f} cents); "
                                      f"{len(others)} steady take{'s' if len(others) > 1 else ''} left")
    for key in {(z["layer"], z["label"]) for z in zones}:
        for i, z in enumerate(sorted([z for z in zones if (z["layer"], z["label"]) == key], key=lambda z: z["rr"])):
            z["rr"] = i

    # Layers recorded at different input levels (the soft takes turned up) are levelled by
    # their noise floors (the room is the same in all of them). Sets recorded at one gain
    # ("levelByFloor": false) keep their dynamics as they are.
    if spec.get("levelByFloor", True) and len(floors) > 1 and len(names) > 1:
        fl = {li: float(np.mean(v)) for li, v in floors.items()}
        mean = float(np.mean(list(fl.values())))
        trims = {li: mean - f for li, f in fl.items()}
        for z in zones:
            z["audio"] = z["audio"] * 10 ** (trims.get(z["layer"], 0.0) / 20)
        report["levelling"][sid] = {names[li]: {"floorDb": round(fl[li], 1), "trimDb": round(trims[li], 1)} for li in fl}
    steps = spec.get("layerStepsDb")
    if steps and len(names) == len(steps):
        changes = []
        for label in sorted({z["label"] for z in zones}, key=midi_of):
            mine = [z for z in zones if z["label"] == label]
            level = {li: float(np.mean([note_loudness(z["audio"]) for z in mine if z["layer"] == li]))
                     for li in {z["layer"] for z in mine}}
            centre = float(np.median([level[li] - steps[li] for li in level]))
            for z in mine:
                trim = float(np.clip(centre + steps[z["layer"]] - note_loudness(z["audio"]), -24.0, 18.0))
                z["audio"] = z["audio"] * 10 ** (trim / 20)
                changes.append(trim)
            for li in level:
                report["layerSteps"].setdefault(sid, {}).setdefault(label, {})[names[li]] = round(float(np.clip(
                    centre + steps[li] - level[li], -24.0, 18.0)), 1)
        report["warnings"].extend(f"{sid}: a layer step needed more than the limit ({c:+.1f} dB)" for c in changes if c in (-24.0, 18.0))
    return zones


def finish_set(spec, zones, gain, out_dir, report):
    """Applies the set's gain (its own loudest peak at -1 dBFS, or its group's), measures,
    maps and writes the notes."""
    sid = spec["id"]
    names = layer_names(spec)
    merge = spec.get("mergeLayers", False)
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    for z in zones:
        z["audio"] = z["audio"] * gain
        env_z = envelope(z["audio"][:int(1.5 * RATE)])
        top = int(np.argmax(np.convolve(env_z, np.ones(2) / 2, mode="same"))) * ENV_HOP
        head = z["audio"][top:top + int(0.3 * RATE)]
        z["rms"] = round(float(db(np.sqrt(np.mean(head ** 2)))), 2)
        z["peak"] = round(float(db(np.abs(z["audio"]).max())), 2)
        z["loudness"] = round(note_loudness(z["audio"]), 2)
    if len(names) > 1 and not merge:
        for label in sorted({z["label"] for z in zones}, key=midi_of):
            levels = [np.mean([z["loudness"] for z in zones if z["label"] == label and z["layer"] == li] or [np.nan]) for li in range(len(names))]
            if not any(np.isnan(levels)) and any(levels[i] >= levels[i + 1] for i in range(len(names) - 1)):
                report["warnings"].append(f"{sid} {label}: the velocity layers are not in loudness order "
                                          f"({', '.join(f'{v:.1f}' for v in levels)} dB); kept as labelled")
    # Key ranges: each recorded pitch covers up to halfway to its neighbours; the ends
    # stretch a fifth (the sampler folds anything further by octaves).
    stretch = spec.get("endStretch", 7)
    mapping_zones = []
    written = {}
    for z in zones:
        if z.get("fixedRange"):
            lo, hi = 0, 127
        else:
            roots = sorted({round(q["root"], 1) for q in zones if q["layer"] == z["layer"]})
            r = round(z["root"], 1)
            lower = [q for q in roots if q < r - 0.5]
            upper = [q for q in roots if q > r + 0.5]
            lo = math.floor((max(lower) + r) / 2) + 1 if lower else int(round(r)) - stretch
            hi = math.floor((min(upper) + r) / 2) if upper else int(round(r)) + stretch
        lname = names[z["layer"]] if not merge else spec["id"].split("-")[-1]
        name = f"{z['label'].replace('#', 's')}-{lname}-{z['rr'] + 1}.flac"
        key = id(z["audio"])
        sf.write(os.path.join(out_dir, sid, name), z["audio"].astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        written[key] = name
        mz = {"file": f"{sid}/{name}", "label": z["label"], "root": z["root"], "loKey": lo, "hiKey": hi,
              "loVel": z["velocity"][0], "hiVel": z["velocity"][1], "layer": 0 if merge else z["layer"], "rr": z["rr"],
              "length": len(z["audio"]), "rms": z["rms"], "loudness": z["loudness"], "peak": z["peak"], "source": z["source"], "at": z["at"]}
        if "loop" in z:
            mz["loopStart"], mz["loopEnd"] = z["loop"]
            mz["release"] = z["release"]
        mapping_zones.append(mz)
    mapping_zones.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
    mapping = {"id": sid, "name": spec["name"], "kind": spec["kind"], "rate": RATE, "channels": 1,
               "layers": names if not merge else [spec["id"].split("-")[-1]], "gainDb": round(20 * math.log10(gain), 2), "zones": mapping_zones}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    return mapping


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", required=True, help="folder with the recordings (the Samples repository)")
    ap.add_argument("--out", required=True, help="output folder (not in the repository)")
    ap.add_argument("--manifest", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "manifest.json"))
    ap.add_argument("--only", help="comma-separated set ids")
    args = ap.parse_args()

    with open(args.manifest) as f:
        manifest = json.load(f)
    os.makedirs(args.out, exist_ok=True)
    scan(args.source)
    room_path = resolve(args.source, manifest["roomTone"])
    room_raw, room_note, _, _ = load(room_path)
    room = room_profile(room_raw)
    report = {"roomTone": {"file": used_name(args.source, room_path), "seconds": round(len(room_raw) / RATE, 1), "padding": room_note,
                           "levelDb": round(float(db(np.sqrt(np.mean(room_raw ** 2)))), 1)},
              "files": [], "problems": [], "warnings": [], "renamed": [], "sets": {}, "levelling": {}, "layerSteps": {}}
    report["patches"] = {}
    known = {manifest["roomTone"]} | {u["file"] for u in manifest.get("unused", [])}
    only = set(args.only.split(",")) if args.only else None
    index = []
    synths = []
    analysed = []
    for spec in manifest["sets"]:
        if spec["kind"] == "synth":
            known.add(spec["file"])
            if not only or spec["id"] in only:
                synths.append(spec)
            continue
        known |= {l["file"] for l in spec["layers"]}
        if only and spec["id"] not in only:
            continue
        print(f"[{spec['id']}] ...", flush=True)
        zones = process_set(spec, args.source, room, args.out, report)
        if zones:
            analysed.append((spec, zones))
    # One gain per set (its loudest peak at -1 dBFS), or per group of sets recorded at
    # the same gain ("gainGroup": the voice's sweeps, stacks and drones keep their levels
    # relative to each other).
    peaks = {}
    for spec, zones in analysed:
        g = spec.get("gainGroup", spec["id"])
        peaks[g] = max(peaks.get(g, 0.0), max(float(np.abs(z["audio"]).max()) for z in zones))
    for spec, zones in analysed:
        gain = 10 ** (-1 / 20) / max(peaks[spec.get("gainGroup", spec["id"])], 1e-9)
        mapping = finish_set(spec, zones, gain, args.out, report)
        index.append(spec["id"])
        seconds = sum(z["length"] for z in mapping["zones"]) / RATE
        report["sets"][spec["id"]] = {"zones": len(mapping["zones"]), "seconds": round(seconds, 1),
                                      "loops": sum(1 for z in mapping["zones"] if "loopStart" in z)}
    # The synth sweeps (sweeps.py uses this module's helpers, so it is imported here).
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import sweeps
    for mapping in sweeps.run(synths, args.source, args.out, report):
        index.append(mapping["id"])
        seconds = sum(z["length"] for z in mapping["zones"]) / RATE
        report["sets"][mapping["id"]] = {"zones": len(mapping["zones"]), "seconds": round(seconds, 1),
                                         "loops": sum(1 for z in mapping["zones"] if "loopStart" in z)}
    known_base = {os.path.basename(k) for k in known}
    for rel in scan(args.source):
        if rel not in known and os.path.basename(rel) not in known_base:
            report["problems"].append(f"'{rel}' is not in the manifest (not used)")
    # A second index for the effect's bundle: only the sets the Atmosphere layer plays.
    fx_sets = [s["id"] for s, _ in analysed if s.get("fx")]
    if fx_sets:
        import shutil
        fx = os.path.join(args.out, "fx")
        shutil.rmtree(fx, ignore_errors=True)
        os.makedirs(fx)
        for sid in fx_sets:
            shutil.copytree(os.path.join(args.out, sid), os.path.join(fx, sid))
            shutil.copy(os.path.join(args.out, f"{sid}.json"), fx)
        with open(os.path.join(fx, "index.json"), "w") as f:
            json.dump({"version": 1, "instruments": fx_sets}, f, indent=1)
    # Sets no instrument plays ("ship": false): prepared and reported, then left out (not
    # loaded by the plug-in, not bundled).
    import shutil
    spare = {s["id"] for s in manifest["sets"] if s.get("ship", True) is False}
    for sid in [i for i in index if i in spare]:
        shutil.rmtree(os.path.join(args.out, sid), ignore_errors=True)
        os.remove(os.path.join(args.out, f"{sid}.json"))
        report["sets"][sid]["shipped"] = False
    index = [i for i in index if i not in spare]
    with open(os.path.join(args.out, "index.json"), "w") as f:
        json.dump({"version": 1, "instruments": index}, f, indent=1)
    total = sum(os.path.getsize(os.path.join(dp, fn)) for dp, _, fns in os.walk(args.out) for fn in fns
                if fn.endswith(".flac") and os.path.relpath(dp, args.out).split(os.sep)[0] != "fx")
    report["flacBytes"] = total
    with open(os.path.join(args.out, "report.json"), "w") as f:
        json.dump(report, f, indent=1)

    # The report, readable.
    print(f"\nRoom tone: {report['roomTone']['seconds']} s at {report['roomTone']['levelDb']} dBFS RMS ({room_note})")
    for e in report["files"]:
        if "padding" in e:
            print(f"\n{e['file']} -> {e['set']}/{e['layer']}: {e['found']} notes (expected {e['expected']}, {e['candidates']} candidates), "
                  f"{e['stereo']}, padding: {e['padding']}, noise floor {e['noiseFloorDb']} -> {e['noiseAfterDb']} dBFS")
        else:
            print(f"\n{e['file']} -> {e['set']}: {e['found']} notes (expected {e['expected']}, {e['candidates']} candidates, rise {e['riseNeededDb']:.0f} dB), "
                  f"{e['format']}, {e['stereo']}, {e.get('range', '?')}, floor {e['noiseFloorDb']} dBFS, sounding {e.get('sounding', '?')} s (median)")
        for n in e["notes"]:
            print(f"   {n['label']:4} at {n['at']:7.2f}s  {n['seconds']:5.2f}s  pitch {n['detected']}" + (f"  held {n['held']}s" if "held" in n else "")
                  + (f"  loop {n['loop']}" if "loop" in n else "")
                  + (f"  notched: {n['sympathetic']}" if "sympathetic" in n else ""))
        if e["rejected"]:
            print(f"   not notes: {', '.join(e['rejected'])}")
        if "takesPerNote" in e:
            print(f"   takes per note: {e['takesPerNote']}")
    for sid, s in report["sets"].items():
        print(f"\n{sid}: {s['zones']} notes, {s['seconds']} s of audio, {s['loops']} loops" + ("" if s.get("shipped", True) else " (not shipped: no instrument plays it)"))
    for pid, pt in report["patches"].items():
        print(f"{pid}: gain {pt['gainDb']:+.1f} dB (both articulations), sounding " + ", ".join(f"{k} {v} s" for k, v in pt["sounding"].items()))
    for sid, lv in report["levelling"].items():
        print(f"{sid}: layers levelled by their noise floors: " + ", ".join(f"{k} {v['floorDb']} dB -> {v['trimDb']:+.1f} dB" for k, v in lv.items()))
    for sid, notes in report["layerSteps"].items():
        print(f"{sid}: layer steps per note (dB): " + "; ".join(f"{label} " + " ".join(f"{k} {v:+.1f}" for k, v in t.items()) for label, t in notes.items()))
    if report["renamed"]:
        print(f"\nNamed differently from how they sound ({len(report['renamed'])}; mapped where they sound):")
        for r in report["renamed"]:
            print(f" - {r}")
    print(f"\nFLAC total: {total / 1e6:.2f} MB")
    print(f"\nCould not match ({len(report['problems'])}):" if report["problems"] else "\nEverything matched.")
    for p in report["problems"]:
        print(f" - {p}")
    if report["warnings"]:
        print(f"\nWarnings ({len(report['warnings'])}):")
        for w in report["warnings"]:
            print(f" - {w}")


if __name__ == "__main__":
    main()
