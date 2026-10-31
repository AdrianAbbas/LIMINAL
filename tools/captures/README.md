# Effect-capture tools

Python 3 + numpy + scipy (`pip install numpy scipy`). Captures stay in the private Samples
repository; nothing derived from them is committed here until it is built into the plugin.

| Script | What it does |
|---|---|
| `kit.py` | the capture-kit sequence (order from the developer; gaps assumed - pass the real kit with `--kit`) |
| `analyze.py <take.wav> <out> [--kit kit.wav] [--static static.wav]` | one take: find sections, clock drift, glitch QC (raw + cross-predicted residual), noise floor, IR (clean sweep / average / repaired), measurements, `ir.wav` + `report.json` |
| `run_all.py <Samples>/effects <out> [--kit kit.wav]` | every take under a folder: inventory, analysis, reamp comparisons, `summary.md` with the re-record list |
| `selftest.py <scratch>` | synthetic units with known answers (echo with wow/flutter + darkening + saturation, two-band hall) and injected glitches; last result in `docs/captures-selftest.txt` |

Glitch rules: a sweep segment (sweep + its 20 s tail) with any click, dropped buffer or hiss
burst is not used. Both loud sweeps clean: averaged (+3 dB SNR). One clean: that one. Neither:
the less damaged, clicks <= 2 ms repaired by AR interpolation, and the take goes on the
re-record list. Clicks <= 1 ms in the tone are repaired (it is only used for wow/flutter).
Transients that belong to the kit (sync clicks, the impulse, burst edges) are outside the
checked segments, so they are never mistaken for glitches.

Known limits: a patch that is 100 % wet with no dry path aligns on its first arrival unless a
loopback take is supplied; the 3 s level tone can't resolve modulation slower than ~0.6 Hz and
mixes dry + wet (use a phrase's sustained notes or the static/modulated pair for slow LFOs).
