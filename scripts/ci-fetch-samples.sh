#!/usr/bin/env bash
# Fetches the developer's PRIVATE AdrianAbbas/Samples repository once and prepares, from it,
#   1. the instruments' recordings (Milestone 8): tools/sample-prep/prepare.py (copied from
#      LIGHT SEEKER) then tools/sample-prep/liminal.py (the piano's handover, tail, stretch)
#      into <samples out> (CMake ships it in every bundle: LIMINAL_SAMPLES_DIR, default build/samples);
#   2. the effect captures (pedal-capture milestone): tools/captures/prepare.py into <captures out>
#      (CMake embeds it: LIMINAL_CAPTURES_DIR, default build/captures).
# The token comes from the repository secret SAMPLES_TOKEN (read access to Samples), passed as an
# HTTP header, never in a URL or on disk. Without the secret the build goes on WITHOUT them (the
# instruments' synthesised stand-ins, the rooms' previous sound) and says so as a warning
# annotation. Nothing fetched or prepared here is committed.
# Usage: scripts/ci-fetch-samples.sh <checkout dir> <samples out> <captures out>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:?checkout directory}"
out="${2:?samples output directory}"
captures="${3:?captures output directory}"
repo="${SAMPLES_REPOSITORY:-AdrianAbbas/Samples}"
if [[ -z "${SAMPLES_TOKEN:-}" ]]; then
    echo "::warning title=Recordings and captures not included::The repository secret SAMPLES_TOKEN is not set, so $repo cannot be fetched; this build's instruments play their synthesised stand-ins and the rooms keep their previous sound. Add SAMPLES_TOKEN (read access to $repo) under Settings > Secrets and variables > Actions, then re-run."
    exit 0
fi
rm -rf "$checkout" "$out" "$captures"
auth=$(printf 'x-access-token:%s' "$SAMPLES_TOKEN" | base64 | tr -d '\n')
echo "::add-mask::$auth"
if ! git -c http.extraheader="AUTHORIZATION: basic $auth" clone -q --depth 1 "https://github.com/$repo.git" "$checkout"; then
    echo "::warning title=Recordings and captures not included::Could not clone $repo with SAMPLES_TOKEN (expired, or no Contents read access); this build's instruments play their synthesised stand-ins and the rooms keep their previous sound."
    exit 0
fi
echo "Samples: $(git -C "$checkout" log -1 --format='%h %s') ($(find "$checkout" -path "$checkout/.git" -prune -o -iname '*.mp3' -print | wc -l | tr -d ' ') MP3s, $(find "$checkout" -path "$checkout/.git" -prune -o -iname '*.wav' -print | wc -l | tr -d ' ') WAVs)"
python3 -m pip install --quiet -r "$here/tools/sample-prep/requirements.txt"

# 1. The recordings.
mkdir -p "$out"
if python3 "$here/tools/sample-prep/prepare.py" --source "$checkout" --out "$out" > "$out.log" 2>&1 \
   && python3 "$here/tools/sample-prep/liminal.py" "$out" >> "$out.log" 2>&1; then
    grep -E '^(piano|  -|Problems|Warnings)' "$out.log" | head -40 || true
    # One line per sample set (and the total), for the CI log.
    python3 - "$out" <<'PY'
import json, os, sys
out = sys.argv[1]
total = 0
for sid in json.load(open(os.path.join(out, "index.json")))["instruments"]:
    zones = json.load(open(os.path.join(out, sid + ".json")))["zones"]
    total += len(zones)
    print(f"Prepared: {len(zones)} notes ({sid})")
print(f"Prepared: {total} notes in all")
PY
else
    tail -40 "$out.log"
    echo "::warning title=Recordings not included::tools/sample-prep failed (log above): this build's instruments play their synthesised stand-ins."
    rm -rf "$out"
fi

# 2. The effect captures (as scripts/ci-fetch-captures.sh did before).
python3 -I "$here/tools/captures/prepare.py" "$checkout" "$captures" > "$captures.log" 2>&1 || { tail -40 "$captures.log"; echo "::error title=Capture preparation failed::tools/captures/prepare.py failed (log above)"; exit 1; }
ls -la "$captures"
# The recordings themselves are not needed after preparation.
rm -rf "$checkout"
