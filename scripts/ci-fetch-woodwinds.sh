#!/usr/bin/env bash
# Fetches the woodwind samples of the free CC0 library "VSCO 2 Community Edition" (Versilian Studios)
# from its OFFICIAL source, github.com/sgossner/VSCO-2-CE at release tag 1.1.0 (docs/LICENSES/README.md),
# and prepares them into <samples out> with tools/sample-prep/woodwinds.py (sets ww-flute, ww-clarinet).
# Only what TAPEWORN uses is fetched (a sparse, blob-filtered checkout: the sustained flute and clarinet
# folders, LICENSE, Readme.txt); <cache dir> keeps it between runs (actions/cache). The licence is checked
# every time: the LICENSE file must be the CC0 1.0 text recorded in docs/LICENSES (md5 below), or nothing
# is prepared and the step fails. Nothing fetched or prepared here is committed. Without network access
# the build goes on WITHOUT the woodwinds (their synthesised stand-ins play) and says so as a warning.
# Run after scripts/ci-fetch-samples.sh (which empties <samples out> first).
# Usage: scripts/ci-fetch-woodwinds.sh <cache dir> <samples out>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${1:?cache directory}"
out="${2:?samples output directory}"
url="https://github.com/sgossner/VSCO-2-CE.git"
tag="1.1.0"
commit="28092772094b2d9f1148d84cea97f4545b8c687d"
licence_md5="7bae63a234e80ee7c6427dce9fdba6cc"
folders=("Woodwinds/Flute/susNV" "Woodwinds/Clarinet/susLong")

have() {
    [[ -f "$cache/LICENSE" ]] || return 1
    for f in "${folders[@]}"; do [[ -n "$(ls "$cache/$f"/*.wav 2>/dev/null)" ]] || return 1; done
}
if have; then
    echo "Woodwinds: from the cache ($(cat "$cache/.commit" 2>/dev/null || echo '?'))"
else
    rm -rf "$cache"
    fetched=0
    for attempt in 1 2 3; do
        if git clone -q --depth 1 --branch "$tag" --filter=blob:none --no-checkout "$url" "$cache" \
           && git -C "$cache" sparse-checkout set --no-cone /LICENSE /Readme.txt "${folders[@]/#//}" \
           && git -C "$cache" checkout -q "$tag"; then
            fetched=1
            break
        fi
        rm -rf "$cache"
        sleep $((attempt * 4))
    done
    if [[ $fetched -ne 1 ]]; then
        echo "::warning title=Woodwinds not included::Could not fetch $url ($tag): this build's woodwind layers play their synthesised stand-ins."
        exit 0
    fi
    got="$(git -C "$cache" rev-parse HEAD)"
    if [[ "$got" != "$commit" ]]; then
        echo "::error title=Woodwind source changed::$url tag $tag is $got, not the reviewed $commit (docs/LICENSES/README.md): check its licence again before using it."
        exit 1
    fi
    echo "$got" > "$cache/.commit"
    rm -rf "$cache/.git"
fi
sum="$(python3 -I -c 'import hashlib,sys; print(hashlib.md5(open(sys.argv[1],"rb").read()).hexdigest())' "$cache/LICENSE")"
if [[ "$sum" != "$licence_md5" ]] || ! head -1 "$cache/LICENSE" | grep -q '^CC0 1.0 Universal'; then
    echo "::error title=Woodwind licence changed::$cache/LICENSE is not the CC0 1.0 text recorded in docs/LICENSES (md5 $sum): not used."
    exit 1
fi
echo "Woodwinds: VSCO 2 CE $tag ($commit), LICENSE CC0 1.0 (md5 $sum), $(find "$cache/Woodwinds" -name '*.wav' | wc -l | tr -d ' ') WAVs"
python3 -m pip install --quiet -r "$here/tools/sample-prep/requirements.txt"
mkdir -p "$out"
if ! python3 -I "$here/tools/sample-prep/woodwinds.py" --vsco "$cache" --out "$out" > "$out.woodwinds.log" 2>&1; then
    tail -40 "$out.woodwinds.log"
    echo "::warning title=Woodwinds not included::tools/sample-prep/woodwinds.py failed (log above): this build's woodwind layers play their synthesised stand-ins."
    exit 0
fi
grep -E '^(Prepared|  problem| warning)' "$out.woodwinds.log" || true
