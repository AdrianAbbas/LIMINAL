#!/usr/bin/env bash
# Sign the built bundles ad hoc (whole bundle, --deep), verify them, and stage
# dist/VACANCY-macOS-Universal.zip with both plug-ins - VACANCY (the instrument) and
# VACANCY FX (the effect), each as AU and VST3 - and the install scripts.
# That zip is the developer package (CI artifact VACANCY-dev-macOS), for testing only.
# Usage: scripts/package-mac.sh <instrument artefacts dir> <dist dir> <effect artefacts dir>
#
# Customer package (docs/INSTALLER_STANDARD.md): scripts/package-mac.sh --customer <signed,
# notarised VACANCY.pkg> <dist dir> stages <dist dir>/VACANCY-<version>-macOS with only
# "VACANCY Installer.pkg" and "VACANCY - Read Me.pdf" (written by scripts/build-pkg.sh next to the
# pkg), and zips it as <dist dir>/VACANCY-<version>-macOS.zip (the release asset). <version> is
# installer/product.json's. It refuses an unsigned pkg.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == --customer ]]; then
    pkg="${2:?the signed VACANCY.pkg}"
    dist="${3:?the dist directory}"
    if ! /usr/sbin/pkgutil --check-signature "$pkg" | /usr/bin/grep -q "Developer ID Installer"; then
        echo "error: $pkg is not signed with Developer ID Installer; no customer package" >&2
        exit 1
    fi
    version=$(/usr/bin/python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["version"])' "$here/installer/product.json")
    customer="$dist/VACANCY-$version-macOS"
    rm -rf "$customer" "$customer.zip"
    mkdir -p "$customer"
    /usr/bin/ditto "$pkg" "$customer/VACANCY Installer.pkg"
    install -m 644 "$(dirname "$pkg")/VACANCY - Read Me.pdf" "$customer/"
    /usr/bin/ditto -c -k --sequesterRsrc --keepParent "$customer" "$customer.zip"
    echo "Packaged $customer.zip"
    /usr/bin/unzip -l "$customer.zip"
    exit 0
fi
artefacts="$1"
dist="$2"
fx="${3:?the artefacts directory of the effect}"
stage="$dist/VACANCY-macOS-Universal"
rm -rf "$stage" "$dist/VACANCY-macOS-Universal.zip"
mkdir -p "$stage"

for bundle in "$artefacts/AU/VACANCY.component" "$artefacts/VST3/VACANCY.vst3" "$artefacts/Standalone/VACANCY.app" "$fx/AU/VACANCY FX.component" "$fx/VST3/VACANCY FX.vst3"; do
    # Developer ID signing (optional CI path) runs before this script; keep it.
    if ! /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application"; then
        /usr/bin/codesign --force --deep --sign - --timestamp=none "$bundle"
    fi
    /usr/bin/codesign --verify --strict --verbose=4 "$bundle"
    /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -E "^(Identifier|Format|Signature|Authority|Sealed Resources)"
done

/usr/bin/ditto "$artefacts/AU/VACANCY.component" "$stage/VACANCY.component"
/usr/bin/ditto "$artefacts/VST3/VACANCY.vst3" "$stage/VACANCY.vst3"
/usr/bin/ditto "$fx/AU/VACANCY FX.component" "$stage/VACANCY FX.component"
/usr/bin/ditto "$fx/VST3/VACANCY FX.vst3" "$stage/VACANCY FX.vst3"
install -m 755 "$here/scripts/install-mac.sh" "$here/scripts/uninstall-mac.sh" "$stage/"
install -m 644 "$here/packaging/INSTALL.txt" "$stage/"
# ditto keeps permissions and bundle metadata, unlike a plain zip of the artifact.
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$stage" "$dist/VACANCY-macOS-Universal.zip"
echo "Packaged $dist/VACANCY-macOS-Universal.zip"
/usr/bin/unzip -l "$dist/VACANCY-macOS-Universal.zip" | /usr/bin/grep -E "install|INSTALL|MacOS/VACANCY$|MacOS/VACANCY FX$"
