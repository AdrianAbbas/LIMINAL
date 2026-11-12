#!/usr/bin/env bash
# Notarise a .pkg, .zip or .dmg with notarytool and the App Store Connect API key, wait for
# the verdict, and save Apple's notarisation log. On anything but "Accepted" the log is
# printed and the script fails. With --staple, the ticket is stapled to the file (and
# validated) afterwards.
#
# Environment: VACANCY_ASC_KEY_PATH (the .p8 file), ASC_KEY_ID, ASC_ISSUER_ID (secrets)
# Usage: scripts/notarize-mac.sh [--staple] <file> <log dir>
set -euo pipefail
staple=0
if [[ "${1:-}" == --staple ]]; then staple=1; shift; fi
file="$1"
logs="$2"
mkdir -p "$logs"
: "${VACANCY_ASC_KEY_PATH:?}" "${ASC_KEY_ID:?}" "${ASC_ISSUER_ID:?}"
auth=(--key "$VACANCY_ASC_KEY_PATH" --key-id "$ASC_KEY_ID" --issuer "$ASC_ISSUER_ID")
name="$(basename "$file")"
stem="$logs/${name// /_}"

echo "Submitting $name ($(/usr/bin/du -h "$file" | /usr/bin/cut -f1)) for notarisation"
result=""
for attempt in 1 2 3; do
    set +e
    result=$(/usr/bin/xcrun notarytool submit "$file" "${auth[@]}" --wait --timeout 45m --output-format json 2> "$stem.submit-stderr.txt")
    code=$?
    set -e
    echo "$result" > "$stem.submit.json"
    # Retry only when nothing was submitted (a network error before the upload finished).
    if [[ $code == 0 ]] || /usr/bin/grep -q '"id"' <<< "$result"; then break; fi
    echo "notarytool submit failed (exit $code, attempt $attempt):"; cat "$stem.submit-stderr.txt"
    sleep $((attempt * 20))
done
id=$(/usr/bin/python3 -c 'import json,sys; print(json.loads(sys.stdin.read() or "{}").get("id",""))' <<< "$result" 2>/dev/null || true)
verdict=$(/usr/bin/python3 -c 'import json,sys; print(json.loads(sys.stdin.read() or "{}").get("status",""))' <<< "$result" 2>/dev/null || true)
echo "Submission ${id:-(none)}: ${verdict:-(no status)}"
if [[ -n "$id" ]]; then
    /usr/bin/xcrun notarytool log "$id" "${auth[@]}" "$stem.notary-log.json" > /dev/null 2>&1 || echo "(the notarisation log could not be fetched)"
fi
if [[ "$verdict" != "Accepted" ]]; then
    echo "::error::notarisation of $name: ${verdict:-submission failed}"
    cat "$stem.submit-stderr.txt" 2>/dev/null || true
    if [[ -f "$stem.notary-log.json" ]]; then
        echo "---- notarisation log ----"; cat "$stem.notary-log.json"; echo
    fi
    exit 1
fi
# Accepted can still come with warnings; show them.
if [[ -f "$stem.notary-log.json" ]]; then
    /usr/bin/python3 - "$stem.notary-log.json" <<'PY'
import json, sys
log = json.load(open(sys.argv[1]))
issues = log.get("issues") or []
print("notarisation log: status %s, %d issue(s)" % (log.get("status"), len(issues)))
for i in issues:
    print("::warning::notarisation: %s %s (%s)" % (i.get("severity"), i.get("message"), i.get("path")))
PY
fi
if [[ $staple == 1 ]]; then
    for attempt in 1 2 3 4 5; do
        if /usr/bin/xcrun stapler staple "$file"; then break; fi
        # The ticket can take a moment to reach Apple's CDN.
        if [[ $attempt == 5 ]]; then echo "::error::could not staple the ticket to $name"; exit 1; fi
        sleep $((attempt * 15))
    done
    /usr/bin/xcrun stapler validate "$file"
fi
echo "$name notarised."
