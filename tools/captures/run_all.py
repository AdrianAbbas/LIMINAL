"""Inventory + quality check + analysis of every effect capture under a folder.

    python3 -I tools/captures/run_all.py <samples>/effects <out-dir> [--kit kit.wav]

Classifies each WAV by name: `*capture-sequence*` = the kit sequence through a patch (a folder
or file name containing `static` = the minimum-modulation version of the same patch), anything
else = a reamped phrase (`effects/dry/` = the dry originals). Writes `<out>/<unit>/<patch>/`
(ir.wav, report.json), `<out>/inventory.md` and `<out>/summary.md` (glitches, re-record list,
measurements, reamp comparisons where a dry phrase of the same name exists).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze  # noqa: E402


def classify(root):
    takes, phrases, dry, other, kit_file = [], [], {}, [], []
    for d, _, files in os.walk(root):
        for f in sorted(files):
            p = os.path.join(d, f)
            rel = os.path.relpath(p, root)
            parts = rel.split(os.sep)
            if not f.lower().endswith(".wav"):
                other.append(rel); continue
            if parts[0].lower() == "dry":
                dry[os.path.splitext(f)[0].lower()] = p; continue
            if len(parts) == 1:   # flat layout (the developer's upload, 2026-11-01)
                low = f.lower()
                stem = os.path.splitext(f)[0]
                if low.startswith("01-pedal-capture-sequence"):
                    kit_file.append(p); continue
                if low == "notes.wav":
                    dry["notes"] = p; continue
                static = "0 rate" in low or "static" in low
                for marker, kind in ((" phrases", "phrase"), (" 48k", "sequence")):
                    if marker in low:
                        name = stem[: low.index(marker)].strip()
                        if kind == "sequence":
                            takes.append({"unit": name, "patch": "-", "static": static, "path": p, "rel": rel})
                        else:
                            phrases.append({"unit": name, "patch": "-", "path": p, "rel": rel, "static": static})
                        break
                else:
                    other.append(rel)
                continue
            unit = parts[0] if len(parts) > 1 else "?"
            patch = "/".join(parts[1:-1]) or os.path.splitext(f)[0]
            static = "static" in rel.lower()
            if "capture-sequence" in f.lower():
                takes.append({"unit": unit, "patch": patch.replace("-static", "").replace("static", "").strip("-_/ ") or patch,
                              "static": static, "path": p, "rel": rel})
            else:
                phrases.append({"unit": unit, "patch": patch, "path": p, "rel": rel})
    return takes, phrases, dry, other, kit_file


def main(root, out, kit_path=None):
    os.makedirs(out, exist_ok=True)
    takes, phrases, dry, other, kit_file = classify(root)
    kit_path = kit_path or (kit_file[0] if kit_file else None)
    inv = ["| unit | patch | kind | file |", "|---|---|---|---|"]
    for t in takes:
        inv.append(f"| {t['unit']} | {t['patch']} | sequence{' (static)' if t['static'] else ''} | `{t['rel']}` |")
    for p in phrases:
        inv.append(f"| {p['unit']} | {p['patch']} | reamped phrase | `{p['rel']}` |")
    for k, p in dry.items():
        inv.append(f"| dry | - | dry phrase / loopback | `{os.path.relpath(p, root)}` |")
    for o in other:
        inv.append(f"| ? | ? | not a WAV - not used | `{o}` |")
    open(os.path.join(out, "inventory.md"), "w").write("\n".join(inv) + "\n")

    summary = ["# Capture analysis summary", ""]
    rerecord = []
    for t in [t for t in takes if not t["static"]]:
        st = next((s for s in takes if s["static"] and s["unit"] == t["unit"] and s["patch"] == t["patch"]), None)
        od = os.path.join(out, t["unit"], t["patch"].replace("/", "_"))
        r = analyze.analyze(t["path"], od, kit_path, st["path"] if st else None)
        bad = [q["segment"] for q in r.get("quality", []) if not q["clean"]]
        if r.get("rerecord"):
            rerecord.append(t["rel"])
        m = r.get("measurements", {})
        summary += [f"## {t['unit']} / {t['patch']}", "",
                    f"- IR: {r.get('ir_from')} ({r.get('ir_source')}), {r.get('ir_length_s')} s; drift {r.get('clock_drift_ppm')} ppm; "
                    f"noise floor {(r.get('noise_floor') or {}).get('rms_dbfs')} dBFS; peak {r.get('peak_dbfs')} dBFS, clipped samples {r.get('clipped_samples')}",
                    f"- damaged segments: {', '.join(bad) or 'none'}",
                    f"- RT60: {m.get('rt60_s')}", f"- echo: {m.get('echo')}",
                    f"- modulation (tone): {json.dumps(m.get('tone_modulation'))}",
                    f"- modulation (wet tail): {json.dumps(m.get('tone_tail_modulation'))}",
                    f"- tape profile: {analyze.tape_profile(m) if m else None}",
                    f"- THD loud/quiet: {m.get('harmonics_loud', {}).get('thd_percent')} / {m.get('harmonics_quiet', {}).get('thd_percent')} %; "
                    f"level dependence: {m.get('level_dependence_db')}", ""]
        import numpy as np  # noqa: E402
        ir = analyze.load(os.path.join(od, "ir.wav"))
        for p in [p for p in phrases if p["unit"] == t["unit"] and p["patch"] == t["patch"]]:
            key = os.path.splitext(os.path.basename(p["path"]))[0].lower()
            key = key if key in dry else ("notes" if "notes" in dry else key)
            if key in dry:
                c = analyze.compare(analyze.load(dry[key]), analyze.load(p["path"]), ir)
                summary.append(f"- reamp `{os.path.basename(p['path'])}` vs IR: {c}")
            else:
                summary.append(f"- reamp `{os.path.basename(p['path'])}`: no dry original with the same name - not compared")
        summary.append("")
    summary += ["## Re-record", ""] + ([f"- `{r}`" for r in rerecord] or ["- none"])
    open(os.path.join(out, "summary.md"), "w").write("\n".join(summary) + "\n")
    print("\n".join(inv)); print("\n".join(summary))


if __name__ == "__main__":
    a = sys.argv[1:]
    k = None
    if "--kit" in a:
        i = a.index("--kit"); k = a[i + 1]; del a[i:i + 2]
    main(a[0], a[1], k)
