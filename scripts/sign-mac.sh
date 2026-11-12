#!/usr/bin/env bash
# Sign plug-in bundles with the Developer ID Application identity: hardened runtime, secure
# timestamp, inside out (every nested Mach-O file and bundle first, then the bundle itself;
# no --deep signing, which Apple advises against), then verify.
#
# Entitlements: none. A plug-in is a library loaded into its host (GarageBand, Logic, any
# DAW, or Apple's AUHostingService): the host's entitlements apply, a library's are ignored.
# VACANCY and VACANCY FX need no JIT, no unsigned memory, no library-validation exception,
# no microphone, network or file entitlement of their own (docs/RELEASING.md).
#
# Environment: VACANCY_APP_IDENTITY (SHA-1 or name), VACANCY_KEYCHAIN, APPLE_TEAM_ID
# Usage: scripts/sign-mac.sh <bundle>...
set -euo pipefail
identity="${VACANCY_APP_IDENTITY:?}"
keychain="${VACANCY_KEYCHAIN:?}"
team="${APPLE_TEAM_ID:?}"

is_macho() {   # Mach-O thin (32/64-bit, either byte order) or universal
    local magic
    magic=$(/usr/bin/head -c 4 "$1" 2>/dev/null | /usr/bin/od -An -tx1 | /usr/bin/tr -d ' \n')
    case "$magic" in
        feedface|feedfacf|cefaedfe|cffaedfe) return 0 ;;
        cafebabe) /usr/bin/lipo -info "$1" > /dev/null 2>&1 ;;   # not a Java class file
        *) return 1 ;;
    esac
}
sign() {
    /usr/bin/codesign --force --sign "$identity" --keychain "$keychain" --options runtime --timestamp "$@"
}

status=0
for bundle in "$@"; do
    [[ -d "$bundle" ]] || { echo "::error::missing bundle $bundle"; status=1; continue; }
    main="$bundle/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$bundle/Contents/Info.plist")"
    echo "== $bundle"
    # Leftover signatures and Finder metadata would break the seal.
    /usr/bin/find "$bundle" -name .DS_Store -delete
    /usr/bin/xattr -cr "$bundle"
    # 1. Loose Mach-O files (helpers, dylibs) other than the main executable.
    while IFS= read -r -d '' file; do
        [[ "$file" == "$main" ]] && continue
        if is_macho "$file"; then echo "  nested binary: ${file#"$bundle"/}"; sign "$file"; fi
    done < <(/usr/bin/find "$bundle/Contents" -type f -print0)
    # 2. Nested bundles and frameworks, deepest first.
    while IFS= read -r nested; do
        echo "  nested bundle: ${nested#"$bundle"/}"; sign "$nested"
    done < <(/usr/bin/find "$bundle/Contents" -mindepth 1 -type d \( -name '*.framework' -o -name '*.bundle' -o -name '*.app' \
                -o -name '*.appex' -o -name '*.xpc' -o -name '*.component' -o -name '*.vst3' \) \
             | /usr/bin/awk '{ print gsub("/", "/"), $0 }' | /usr/bin/sort -rn | /usr/bin/cut -d' ' -f2-)
    # 3. The bundle (signs its main executable and seals Contents/Resources).
    sign "$bundle"
    /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle"
    details=$(/usr/bin/codesign -dvvv "$bundle" 2>&1)
    echo "$details" | /usr/bin/grep -E "^(Identifier|Format|CodeDirectory|Authority|Timestamp|TeamIdentifier|Runtime Version)"
    echo "$details" | /usr/bin/grep -q "Authority=Developer ID Application" || { echo "::error::$bundle is not signed with Developer ID Application"; status=1; }
    echo "$details" | /usr/bin/grep -q "^TeamIdentifier=$team$" || { echo "::error::$bundle is not signed by team $team"; status=1; }
    echo "$details" | /usr/bin/grep -qE "^CodeDirectory .*flags=0x[0-9a-f]*\(.*runtime.*\)" || { echo "::error::$bundle has no hardened runtime"; status=1; }
    echo "$details" | /usr/bin/grep -q "^Timestamp=" || { echo "::error::$bundle has no secure timestamp"; status=1; }
    if /usr/bin/codesign -d --entitlements - --xml "$bundle" 2>/dev/null | /usr/bin/grep -q "<key>"; then
        echo "::error::$bundle carries entitlements; it should carry none"; status=1
    fi
done
exit $status
