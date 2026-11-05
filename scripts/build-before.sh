#!/usr/bin/env bash
# Builds the test binary of the code BEFORE a patch, with today's listening render and measuring probe
# compiled in (Tests/PieceRender.h, Tests/TreatmentProbe.h), so a before/after render and the "before"
# measurements come from the old code itself (2026-11-05 patch notes; nothing of the old build is kept
# in the repository: the recordings it renders are private).
# Usage: scripts/build-before.sh <commit> <source dir> <build dir> <JUCE dir> <samples dir> <captures dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
commit="${1:?commit}"
src="${2:?source directory}"
build="${3:?build directory}"
juce="${4:?JUCE directory}"
samples="${5:?samples directory}"
captures="${6:?captures directory}"
git -C "$here" cat-file -e "$commit^{commit}" 2>/dev/null || git -C "$here" fetch -q --depth 1 origin "$commit"
rm -rf "$src"
mkdir -p "$src"
git -C "$here" archive "$commit" | tar -x -C "$src"
cp "$here/Tests/PieceRender.h" "$here/Tests/TreatmentProbe.h" "$src/Tests/"
python3 - "$src/Tests/LiminalTests.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace('#include "LibraryRender.h"\n', '#include "LibraryRender.h"\n#include "PieceRender.h"\n#include "TreatmentProbe.h"\n', 1)
hook = '        else if (arg == "--render-library" && i + 1 < argc) return renderLibrary(argv[++i]);\n'
assert hook in s, "the old test runner's argument list has changed"
s = s.replace(hook, hook + '        else if (arg == "--render-piece" && i + 1 < argc) return renderPiece(argv[++i]);\n'
                          '        else if (arg == "--probe-treatment") return probeTreatment();\n', 1)
open(p, "w").write(s)
PY
launcher=()
if command -v ccache > /dev/null; then launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache); fi
# (without the recordings or captures - no SAMPLES_TOKEN - the old build plays its stand-ins, as today's does)
mkdir -p "$samples" "$captures"
cmake -S "$src" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="$juce" \
      -DLIMINAL_SAMPLES_DIR="$(cd "$samples" && pwd)" -DLIMINAL_CAPTURES_DIR="$(cd "$captures" && pwd)" "${launcher[@]}" > "$build.configure.log" 2>&1 \
    || { tail -30 "$build.configure.log"; exit 1; }
cmake --build "$build" --target LiminalTests --parallel 4 > "$build.build.log" 2>&1 || { grep -E "error" "$build.build.log" | head -20; exit 1; }
echo "before: $(git -C "$here" log -1 --format='%h %s' "$commit" 2>/dev/null || echo "$commit") -> $build/LiminalTests_artefacts/Release/LiminalTests"
