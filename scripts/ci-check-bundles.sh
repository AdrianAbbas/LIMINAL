#!/usr/bin/env bash
# CI gate for the built macOS bundles: both architecture slices present, AU identity and sandboxSafe in Info.plist, minimum macOS version
# reported.
# Both plug-ins (2026-11-06; renamed 2026-11-07): VACANCY (the instrument) and VACANCY FX (the effect), by Windward Audio.
# Usage: scripts/ci-check-bundles.sh <instrument artefacts, e.g. build/LIMINAL_artefacts/Release> <effect artefacts, e.g. build/TapewornFX_artefacts/Release>
set -euo pipefail
artefacts="$1"
fx="${2:?the artefacts directory of the effect}"
status=0
for bundle in "$artefacts/AU/VACANCY.component" "$artefacts/VST3/VACANCY.vst3" "$artefacts/Standalone/VACANCY.app"; do
    binary="$bundle/Contents/MacOS/VACANCY"
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

plist="$artefacts/AU/VACANCY.component/Contents/Info.plist"
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
check factoryFunction VACANCYAUFactory
check sandboxSafe true
check name "Windward Audio: VACANCY"
# The bundle identifier (2026-11-06 and 2026-11-07: renamed; the AU codes above are unchanged, so songs still find it).
for bundle in "$artefacts/AU/VACANCY.component" "$artefacts/VST3/VACANCY.vst3" "$artefacts/Standalone/VACANCY.app"; do
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || echo "(absent)")
    printf 'CFBundleIdentifier %-40s %s\n' "$(basename "$bundle")" "$id"
    if [[ "$id" != "com.windwardaudio.vacancy" ]]; then echo "::error::$(basename "$bundle") CFBundleIdentifier expected com.windwardaudio.vacancy, got $id"; status=1; fi
    if /usr/bin/grep -aqE 'LIMINAL|TAPEWORN|[Tt]apeworn|[Ee]lsewhere' "$bundle/Contents/Info.plist"; then echo "::error::$(basename "$bundle") Info.plist still names LIMINAL, TAPEWORN or Elsewhere Audio"; status=1; fi
done

# VACANCY FX: an effect (aufx) with its own codes and identity, side by side with the instrument.
for bundle in "$fx/AU/VACANCY FX.component" "$fx/VST3/VACANCY FX.vst3"; do
    binary="$bundle/Contents/MacOS/VACANCY FX"
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
    if [[ "$id" != "com.windwardaudio.vacancyfx" ]]; then echo "::error::$(basename "$bundle") CFBundleIdentifier expected com.windwardaudio.vacancyfx, got $id"; status=1; fi
    if /usr/bin/grep -aqE 'LIMINAL|TAPEWORN|[Tt]apeworn|[Ee]lsewhere' "$bundle/Contents/Info.plist"; then echo "::error::$(basename "$bundle") Info.plist still names LIMINAL, TAPEWORN or Elsewhere Audio"; status=1; fi
done
fxplist="$fx/AU/VACANCY FX.component/Contents/Info.plist"
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
fxcheck factoryFunction VacancyFXAUFactory
fxcheck sandboxSafe true
fxcheck name "Windward Audio: VACANCY FX"
# The instrument keeps its own codes.
instrument_subtype=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:subtype" "$plist" 2>/dev/null || echo "(absent)")
if [[ "$instrument_subtype" == "Tpfx" ]]; then
    echo "::error::the instrument took the subtype of the effect"; status=1
fi
exit $status
