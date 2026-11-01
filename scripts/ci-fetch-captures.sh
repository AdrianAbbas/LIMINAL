#!/usr/bin/env bash
# Fetches the developer's effect captures from the PRIVATE AdrianAbbas/Samples repository and
# prepares the plug-in's capture assets (tools/captures/prepare.py) into <out dir>, which CMake
# embeds (LIMINAL_CAPTURES_DIR, default build/captures). The token comes from the repository
# secret SAMPLES_TOKEN (read access to Samples), passed as an HTTP header, never in a URL or on
# disk. Without the secret the build goes on WITHOUT the captures (the rooms' previous sound) and
# says so as a warning annotation. Nothing fetched or prepared here is committed.
# Usage: scripts/ci-fetch-captures.sh <checkout dir> <out dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:?checkout directory}"
out="${2:?output directory}"
repo="${SAMPLES_REPOSITORY:-AdrianAbbas/Samples}"
if [[ -z "${SAMPLES_TOKEN:-}" ]]; then
    echo "::warning title=Effect captures not embedded::The repository secret SAMPLES_TOKEN is not set, so the effect captures in $repo cannot be fetched; this build keeps the rooms' previous sound. Add SAMPLES_TOKEN (read access to $repo) under Settings > Secrets and variables > Actions, then re-run."
    exit 0
fi
rm -rf "$checkout" "$out"
auth=$(printf 'x-access-token:%s' "$SAMPLES_TOKEN" | base64 | tr -d '\n')
echo "::add-mask::$auth"
if ! git -c http.extraheader="AUTHORIZATION: basic $auth" clone -q --depth 1 "https://github.com/$repo.git" "$checkout"; then
    echo "::warning title=Effect captures not embedded::Could not clone $repo with SAMPLES_TOKEN (expired, or no Contents read access); this build keeps the rooms' previous sound."
    exit 0
fi
echo "Samples: $(git -C "$checkout" log -1 --format='%h %s')"
python3 -m pip install --quiet numpy scipy
python3 -I "$here/tools/captures/prepare.py" "$checkout" "$out" > "$out.log" 2>&1 || { tail -40 "$out.log"; echo "::error title=Capture preparation failed::tools/captures/prepare.py failed (log above)"; exit 1; }
rm -rf "$checkout"
ls -la "$out"
