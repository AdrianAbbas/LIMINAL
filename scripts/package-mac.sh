#!/usr/bin/env bash
# Sign the built bundles ad hoc (whole bundle, --deep), verify them, and stage
# dist/TAPEWORN-macOS-Universal.zip with both plug-ins - TAPEWORN (the instrument) and
# TAPEWORN FX (the effect), each as AU and VST3 - and the install scripts.
# Usage: scripts/package-mac.sh <instrument artefacts dir> <dist dir> <effect artefacts dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
artefacts="$1"
dist="$2"
fx="${3:?the artefacts directory of the effect}"
stage="$dist/TAPEWORN-macOS-Universal"
rm -rf "$stage" "$dist/TAPEWORN-macOS-Universal.zip"
mkdir -p "$stage"

for bundle in "$artefacts/AU/TAPEWORN.component" "$artefacts/VST3/TAPEWORN.vst3" "$artefacts/Standalone/TAPEWORN.app" "$fx/AU/TAPEWORN FX.component" "$fx/VST3/TAPEWORN FX.vst3"; do
    # Developer ID signing (optional CI path) runs before this script; keep it.
    if ! /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application"; then
        /usr/bin/codesign --force --deep --sign - --timestamp=none "$bundle"
    fi
    /usr/bin/codesign --verify --strict --verbose=4 "$bundle"
    /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -E "^(Identifier|Format|Signature|Authority|Sealed Resources)"
done

/usr/bin/ditto "$artefacts/AU/TAPEWORN.component" "$stage/TAPEWORN.component"
/usr/bin/ditto "$artefacts/VST3/TAPEWORN.vst3" "$stage/TAPEWORN.vst3"
/usr/bin/ditto "$fx/AU/TAPEWORN FX.component" "$stage/TAPEWORN FX.component"
/usr/bin/ditto "$fx/VST3/TAPEWORN FX.vst3" "$stage/TAPEWORN FX.vst3"
install -m 755 "$here/scripts/install-mac.sh" "$here/scripts/uninstall-mac.sh" "$stage/"
install -m 644 "$here/packaging/INSTALL.txt" "$stage/"
# ditto keeps permissions and bundle metadata, unlike a plain zip of the artifact.
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$stage" "$dist/TAPEWORN-macOS-Universal.zip"
echo "Packaged $dist/TAPEWORN-macOS-Universal.zip"
/usr/bin/unzip -l "$dist/TAPEWORN-macOS-Universal.zip" | /usr/bin/grep -E "install|INSTALL|MacOS/TAPEWORN$|MacOS/TAPEWORN FX$"
