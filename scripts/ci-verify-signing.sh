#!/usr/bin/env bash
# Gate for a Developer ID release: every bundle passes codesign's strict deep check and Gatekeeper
# accepts it as notarised; the installer package is signed with Developer ID Installer, carries a
# stapled ticket, and Gatekeeper accepts it ("accepted", "source=Notarized Developer ID").
# Usage: scripts/ci-verify-signing.sh <VACANCY.pkg> <bundle>...
set -euo pipefail
pkg="$1"; shift
team="${APPLE_TEAM_ID:?}"
status=0
assess() {   # assess <label> <spctl arguments...>
    local label="$1" out; shift
    out=$(/usr/sbin/spctl --assess -vvv "$@" 2>&1 || true)
    echo "$out" | /usr/bin/sed 's/^/    /'
    if echo "$out" | /usr/bin/grep -q ": accepted" && echo "$out" | /usr/bin/grep -q "^source=Notarized Developer ID"; then
        echo "  OK   $label: accepted, source=Notarized Developer ID"
    else
        echo "::error::Gatekeeper does not accept $label as notarised Developer ID"; status=1
    fi
}
for bundle in "$@"; do
    echo "== $bundle"
    if /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle" 2>&1 | /usr/bin/sed 's/^/    /'; then
        echo "  OK   codesign --verify --deep --strict"
    else
        echo "::error::codesign --verify --deep --strict failed for $bundle"; status=1
    fi
    /usr/bin/codesign -dvvv "$bundle" 2>&1 | /usr/bin/grep -q "^TeamIdentifier=$team$" || { echo "::error::$bundle is not signed by team $team"; status=1; }
    # A plug-in is not an app: Gatekeeper judges it as a document opened by its host.
    assess "$(basename "$bundle")" --type open --context context:primary-signature "$bundle"
    if /usr/bin/xcrun stapler validate "$bundle" > /dev/null 2>&1; then echo "  OK   ticket stapled"; else echo "  NOTE no stapled ticket (Gatekeeper checks online)"; fi
done
echo "== $pkg"
/usr/sbin/pkgutil --check-signature "$pkg" | /usr/bin/sed 's/^/    /'
if /usr/sbin/pkgutil --check-signature "$pkg" | /usr/bin/grep -qE "Developer ID Installer: .*\($team\)"; then
    echo "  OK   signed with Developer ID Installer ($team)"
else
    echo "::error::$pkg is not signed with the Developer ID Installer identity of team $team"; status=1
fi
if /usr/bin/xcrun stapler validate "$pkg"; then echo "  OK   ticket stapled"; else echo "::error::no notarisation ticket stapled to $pkg"; status=1; fi
assess "$(basename "$pkg")" --type install "$pkg"
exit $status
