#!/usr/bin/env bash
# Build the installer package VACANCY.pkg with productbuild, to the Windward Audio installer standard
# (docs/INSTALLER_STANDARD.md: pages, wording, background and Read Me from installer/, rendered by
# installer/render.py; identifiers com.windwardaudio.pkg.vacancy[.au|.vst3|.prepare]): VACANCY and VACANCY FX as Audio
# Units (/Library/Audio/Plug-Ins/Components) and VST3s (/Library/Audio/Plug-Ins/VST3), for all
# users. The recordings (Contents/Resources/Samples) and the factory presets (compiled into each
# binary) travel inside the bundles. Preinstall scripts silently remove bundles under earlier names
# and competing copies (packaging/pkg/remove-copies.sh); the AU postinstall resets the registry.
# The bundles must already be signed (scripts/sign-mac.sh, or ad hoc by scripts/package-mac.sh).
# With an installer identity the product is signed (Developer ID Installer, secure timestamp);
# without one it is left unsigned, for the ad-hoc CI build only.
#
# Environment (optional): VACANCY_INSTALLER_IDENTITY, VACANCY_KEYCHAIN
# The customer download renames it "VACANCY Installer.pkg" (scripts/package-mac.sh --customer).
# Usage: scripts/build-pkg.sh <instrument artefacts dir> <effect artefacts dir> <output .pkg>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
artefacts="$1"
fx="$2"
out="$3"
identity="${VACANCY_INSTALLER_IDENTITY:-}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$artefacts/AU/VACANCY.component/Contents/Info.plist")
echo "VACANCY.pkg version $version"

# Payload roots: copies of the signed bundles (ditto keeps the signatures and the stapled tickets).
mkdir -p "$work/root-au" "$work/root-vst3" "$work/packages" "$work/resources"
/usr/bin/ditto "$artefacts/AU/VACANCY.component" "$work/root-au/VACANCY.component"
/usr/bin/ditto "$fx/AU/VACANCY FX.component" "$work/root-au/VACANCY FX.component"
/usr/bin/ditto "$artefacts/VST3/VACANCY.vst3" "$work/root-vst3/VACANCY.vst3"
/usr/bin/ditto "$fx/VST3/VACANCY FX.vst3" "$work/root-vst3/VACANCY FX.vst3"
/usr/bin/find "$work/root-au" "$work/root-vst3" -name .DS_Store -delete
/bin/chmod -R u+rwX,go+rX,go-w "$work/root-au" "$work/root-vst3"
for bundle in "$work"/root-*/*; do
    /usr/bin/codesign --verify --strict "$bundle" || { echo "::error::$(basename "$bundle") is not validly signed"; exit 1; }
done

# Preinstall scripts: one mode line above the shared body.
for mode in prepare au vst3; do
    mkdir -p "$work/scripts-$mode"
    { echo '#!/bin/bash'; echo "mode=$mode"; cat "$here/packaging/pkg/remove-copies.sh"; } > "$work/scripts-$mode/preinstall"
    chmod 755 "$work/scripts-$mode/preinstall"
done
install -m 755 "$here/packaging/pkg/postinstall-au" "$work/scripts-au/postinstall"

# Component packages. The bundles are not relocatable: without this the installer would follow
# a bundle with the same ID elsewhere on the disk (e.g. a copy in ~/Library) and "upgrade" it there.
plist_set() {   # plist_set <plist> <key path> <type> <value>
    /usr/libexec/PlistBuddy -c "Set :$2 $4" "$1" 2> /dev/null || /usr/libexec/PlistBuddy -c "Add :$2 $3 $4" "$1"
}
component_pkg() {   # component_pkg <name> <install location>
    local name="$1" location="$2" plist="$work/$1-components.plist" i=0
    /usr/bin/pkgbuild --analyze --root "$work/root-$name" "$plist" > /dev/null
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" > /dev/null 2>&1; do
        plist_set "$plist" "$i:BundleIsRelocatable" bool false
        plist_set "$plist" "$i:BundleOverwriteAction" string upgrade
        i=$((i + 1))
    done
    /usr/bin/pkgbuild --root "$work/root-$name" --component-plist "$plist" --install-location "$location" \
        --scripts "$work/scripts-$name" --identifier "com.windwardaudio.pkg.vacancy.$name" --version "$version" \
        --ownership recommended "$work/packages/$name.pkg"
}
component_pkg au /Library/Audio/Plug-Ins/Components
component_pkg vst3 /Library/Audio/Plug-Ins/VST3
/usr/bin/pkgbuild --nopayload --scripts "$work/scripts-prepare" --identifier com.windwardaudio.pkg.vacancy.prepare \
    --version "$version" "$work/packages/prepare.pkg"

# Installer pages, background, licence and the distribution (installer/product.json + templates).
# Disk space: the payload, rounded up to 10 MB.
kb=$(/usr/bin/du -sk "$work/root-au" "$work/root-vst3" | /usr/bin/awk '{ s += $1 } END { print s }')
disk="$(( (kb + 10239) / 10240 * 10 )) MB"
/usr/bin/python3 "$here/installer/render.py" --pkg-version "$version" --disk-space "$disk" \
    --resources "$work/resources" --distribution "$work/distribution.xml" --readme-pdf "$(dirname "$out")/VACANCY - Read Me.pdf"
mkdir -p "$(dirname "$out")"
rm -f "$out"
sign_args=()
if [[ -n "$identity" ]]; then
    sign_args=(--sign "$identity" --timestamp)
    [[ -n "${VACANCY_KEYCHAIN:-}" ]] && sign_args+=(--keychain "$VACANCY_KEYCHAIN")
else
    echo "::notice::VACANCY.pkg is built UNSIGNED (no Developer ID Installer identity): for the CI install test only, not for distribution"
fi
/usr/bin/productbuild --distribution "$work/distribution.xml" --package-path "$work/packages" \
    --resources "$work/resources" --identifier com.windwardaudio.pkg.vacancy --version "$version" \
    ${sign_args[@]+"${sign_args[@]}"} "$out"
echo "Built $out ($(/usr/bin/du -h "$out" | /usr/bin/cut -f1))"
/usr/sbin/pkgutil --check-signature "$out" || [[ -z "$identity" ]]
