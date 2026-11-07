#!/usr/bin/env python3
"""VACANCY FX's level match table (2026-11-06).

Reads the output of `TapewornFXTests --calibrate-levels` (lines "place character offset_db": each
room's loudness against the input, the mean over the guitar, vocal and mix test sources, measured
with the table's trims removed) and writes Source/Fx/FxLevelMatchTable.h with trim = -offset.

Usage: TapewornFXTests --calibrate-levels > levels.txt
       python3 tools/levelmatch/fx_levelmatch.py Source/Fx/FxLevelMatchTable.h levels.txt
Re-run it whenever a room, a Character or the effect's defaults change their sound.
"""
import sys

def main(out_path, levels_path):
    table = [[0.0] * 3 for _ in range(10)]
    seen = 0
    for line in open(levels_path):
        parts = line.split()
        if len(parts) != 3 or line.startswith("#"):
            continue
        p, c, off = int(parts[0]), int(parts[1]), float(parts[2])
        table[p][c] = -off
        seen += 1
    if seen != 30:
        sys.exit(f"expected 30 lines (10 places x 3 Characters), got {seen}")
    rows = []
    for p in range(10):
        rows.append("    { " + ", ".join(f"{v:.2f}f" for v in table[p]) + " },")
    text = ("#pragma once\n"
            "// Written by tools/levelmatch/fx_levelmatch.py from `TapewornFXTests --calibrate-levels` (do not edit by hand).\n"
            "// dB per place (hospital .. impossible room) x Character, at the default settings.\n"
            "namespace liminal::fx::levelmatch {\n"
            "inline constexpr float table[10][3] {\n" + "\n".join(rows) + "\n};\n"
            "} // namespace liminal::fx::levelmatch\n")
    open(out_path, "w").write(text)
    print(f"wrote {out_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
