#!/usr/bin/env bash
# Install a LIMINAL.component into ~/Library/Audio/Plug-Ins/Components, restart the
# AU registrar and wait until aumu/Lmnl/Veil is listed. Used by CI experiments.
# Usage: scripts/register-au.sh <path/to/LIMINAL.component>
set -euo pipefail
source_bundle="$1"
components="$HOME/Library/Audio/Plug-Ins/Components"
installed="$components/LIMINAL.component"
mkdir -p "$components"
rm -rf "$installed"
/usr/bin/ditto "$source_bundle" "$installed"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
for attempt in $(seq 1 30); do
    if /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -Eq '^[[:space:]]*aumu[[:space:]]+Lmnl[[:space:]]+Veil'; then
        echo "registered after $attempt attempt(s)"
        exit 0
    fi
    sleep 1
done
echo "aumu/Lmnl/Veil not registered after 30 s" >&2
exit 2
