#!/usr/bin/env bash
# CI gate for the built macOS bundles: both architecture slices present, AU identity and sandboxSafe in Info.plist, minimum macOS version
# reported.
# Both plug-ins (2026-11-06): TAPEWORN (the instrument) and TAPEWORN FX (the effect).
# Usage: scripts/ci-check-bundles.sh <instrument artefacts, e.g. build/LIMINAL_artefacts/Release> <effect artefacts, e.g. build/TapewornFX_artefacts/Release>
set -euo pipefail
artefacts="$1"
fx="${2:?the effect's artefacts directory}"
status=0
for bundle in "$artefacts/AU/TAPEWORN.component" "$artefacts/VST3/TAPEWORN.vst3" "$artefacts/Standalone/TAPEWORN.app"; do
    binary="$bundle/Contents/MacOS/TAPEWORN"
    if [[ ! -f "$binary" ]]; then
        echo "::error::missing $binary"; status=1; continue
    fi
    archs=$(/usr/bin/lipo -archs "$binary")
    printf '%-60s %s\n' "$binary" "$archs"
    if [[ " $archs " != *" arm64 "* || " $archs " != *" x86_64 "* ]]; then
        echo "::error::$binary must contain arm64 and x86_64 slices, has: $archs"; status=1
    fi
    for arch in x86_64 arm64; do
        minos=$(/usr/bin/otool -arch "$arch" -l "$binary" | /usr/bin/awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')
        printf '    %-7s minimum macOS %s\n' "$arch" "${minos:-unknown}"
    done
done

plist="$artefacts/AU/TAPEWORN.component/Contents/Info.plist"
check() {
    local key="$1" expected="$2" actual
    actual=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$key" "$plist" 2>/dev/null || echo "(absent)")
    if [[ "$actual" == "$expected" ]]; then
        printf 'Info.plist %-16s %s\n' "$key" "$actual"
    else
        echo "::error::Info.plist $key expected '$expected', got '$actual'"; status=1
    fi
}
check type aumu
check subtype Lmnl
check manufacturer Veil
check factoryFunction TAPEWORNAUFactory
check sandboxSafe true
check name "Elsewhere Audio: TAPEWORN"
# The bundle identifier (2026-11-06: renamed; the AU codes above are unchanged, so songs still find it).
for bundle in "$artefacts/AU/TAPEWORN.component" "$artefacts/VST3/TAPEWORN.vst3" "$artefacts/Standalone/TAPEWORN.app"; do
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || echo "(absent)")
    printf 'CFBundleIdentifier %-40s %s\n' "$(basename "$bundle")" "$id"
    if [[ "$id" != "com.elsewhereaudio.tapeworn" ]]; then echo "::error::$(basename "$bundle") CFBundleIdentifier expected com.elsewhereaudio.tapeworn, got $id"; status=1; fi
    if /usr/bin/grep -aq 'LIMINAL' "$bundle/Contents/Info.plist"; then echo "::error::$(basename "$bundle") Info.plist still names LIMINAL"; status=1; fi
done

# TAPEWORN FX: an effect (aufx) with its own codes and identity, side by side with the instrument.
for bundle in "$fx/AU/TAPEWORN FX.component" "$fx/VST3/TAPEWORN FX.vst3"; do
    binary="$bundle/Contents/MacOS/TAPEWORN FX"
    if [[ ! -f "$binary" ]]; then
        echo "::error::missing $binary"; status=1; continue
    fi
    archs=$(/usr/bin/lipo -archs "$binary")
    printf '%-60s %s\n' "$binary" "$archs"
    if [[ " $archs " != *" arm64 "* || " $archs " != *" x86_64 "* ]]; then
        echo "::error::$binary must contain arm64 and x86_64 slices, has: $archs"; status=1
    fi
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || echo "(absent)")
    printf 'CFBundleIdentifier %-40s %s\n' "$(basename "$bundle")" "$id"
    if [[ "$id" != "com.windwardaudio.tapewornfx" ]]; then echo "::error::$(basename "$bundle") CFBundleIdentifier expected com.windwardaudio.tapewornfx, got $id"; status=1; fi
done
fxplist="$fx/AU/TAPEWORN FX.component/Contents/Info.plist"
fxcheck() {
    local key="$1" expected="$2" actual
    actual=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$key" "$fxplist" 2>/dev/null || echo "(absent)")
    if [[ "$actual" == "$expected" ]]; then
        printf 'FX Info.plist %-16s %s\n' "$key" "$actual"
    else
        echo "::error::FX Info.plist $key expected '$expected', got '$actual'"; status=1
    fi
}
fxcheck type aufx
fxcheck subtype Tpfx
fxcheck manufacturer Wnwd
fxcheck factoryFunction TapewornFXAUFactory
fxcheck sandboxSafe true
fxcheck name "Windward Audio: TAPEWORN FX"
# The instrument's codes stay its own.
if [[ "$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:subtype" "$plist" 2>/dev/null)" == "Tpfx" ]]; then echo "::error::the instrument took the effect's subtype"; status=1; fi
exit $status
