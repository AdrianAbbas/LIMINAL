#!/usr/bin/env bash
# Install VACANCY.pkg on the runner the way a user does, then validate the installed copies.
#   1. with --notarized: mark the package as a browser download (com.apple.quarantine) and check
#      that Gatekeeper accepts it ("accepted", "source=Notarized Developer ID")
#   2. plant what the package must remove: copies under the old names (LIMINAL, TAPEWORN,
#      TAPEWORN FX; by name and by old bundle ID only), system-wide and in ~/Library. The per-user
#      copies install-mac.sh left there (scripts/ci-user-install-test.sh runs first) must go too.
#   3. sudo installer -pkg; check the receipts, the four installed bundles (root-owned, signed,
#      with their recordings), that the planted copies are gone, and the registry (each once)
#   4. auval and the AU host harnesses (--strict), arm64 and x86_64 (Rosetta), for both plug-ins,
#      in-process and out-of-process, on the installed copies
# Usage: scripts/ci-pkg-install-test.sh [--notarized] <VACANCY.pkg> <instrument harness binary> <log dir>
set -euo pipefail
notarized=0
if [[ "${1:-}" == --notarized ]]; then notarized=1; shift; fi
pkg="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
harness="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
fx_harness="$(dirname "$harness")/TapewornFXAUHarness"
logs="$3"
mkdir -p "$logs"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
status=0
limit() { local seconds="$1"; shift; /usr/bin/perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!\n"' "$seconds" "$@"; }
sys_components="/Library/Audio/Plug-Ins/Components"
sys_vst3="/Library/Audio/Plug-Ins/VST3"
user_components="$HOME/Library/Audio/Plug-Ins/Components"
user_vst3="$HOME/Library/Audio/Plug-Ins/VST3"

# 1. Gatekeeper on the downloaded package.
if [[ $notarized == 1 ]]; then
    /usr/bin/xattr -w com.apple.quarantine "0081;$(printf '%x' "$(date +%s)");Chrome;$(/usr/bin/uuidgen)" "$pkg"
    out=$(/usr/sbin/spctl --assess --type install -vvv "$pkg" 2>&1 || true)
    echo "$out" | tee "$logs/pkg-gatekeeper.txt"
    if ! echo "$out" | /usr/bin/grep -q ": accepted" || ! echo "$out" | /usr/bin/grep -q "^source=Notarized Developer ID"; then
        echo "::error::Gatekeeper does not accept the downloaded VACANCY.pkg as notarised Developer ID"; status=1
    fi
else
    echo "::notice::VACANCY.pkg is unsigned in this run (ad-hoc build): Gatekeeper checks skipped" | tee "$logs/pkg-gatekeeper.txt"
fi

# 2. Plant old copies. Their sources: the bundles inside the package itself.
expanded="$(mktemp -d)/expanded"
/usr/sbin/pkgutil --expand-full "$pkg" "$expanded"
src_au="$expanded/au.pkg/Payload"
src_vst3="$expanded/vst3.pkg/Payload"
ls "$src_au" "$src_vst3"
plant() {   # plant <source bundle> <destination> <bundle ID> [sudo]
    ${4:+/usr/bin/sudo} mkdir -p "$(dirname "$2")"
    ${4:+/usr/bin/sudo} /usr/bin/ditto "$1" "$2"
    ${4:+/usr/bin/sudo} /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $3" "$2/Contents/Info.plist"
    echo "planted $2 ($3)"
}
plant "$src_au/VACANCY.component" "$sys_components/LIMINAL.component" audio.veil.liminal sudo
plant "$src_vst3/VACANCY.vst3" "$sys_vst3/TAPEWORN.vst3" com.elsewhereaudio.tapeworn sudo
plant "$src_au/VACANCY FX.component" "$sys_components/Old Effect Copy.component" com.windwardaudio.tapewornfx sudo
plant "$src_au/VACANCY.component" "$user_components/TAPEWORN.component" com.elsewhereaudio.tapeworn
plant "$src_vst3/VACANCY FX.vst3" "$user_vst3/TAPEWORN FX.vst3" com.windwardaudio.tapewornfx
planted=("$sys_components/LIMINAL.component" "$sys_vst3/TAPEWORN.vst3" "$sys_components/Old Effect Copy.component"
         "$user_components/TAPEWORN.component" "$user_vst3/TAPEWORN FX.vst3")
# The zip install (install-mac.sh, run before this) left per-user copies of the same plug-ins.
for copy in "$user_components/VACANCY.component" "$user_components/VACANCY FX.component" "$user_vst3/VACANCY.vst3" "$user_vst3/VACANCY FX.vst3"; do
    if [[ -d "$copy" ]]; then planted+=("$copy"); echo "per-user copy from the zip install: $copy"; fi
done

# 3. Install.
set +e
limit 600 /usr/bin/sudo /usr/sbin/installer -pkg "$pkg" -target / -verboseR 2>&1 | tee "$logs/pkg-installer.txt"
install_exit=${PIPESTATUS[0]}
set -e
/usr/bin/grep "VACANCY installer:" /var/log/install.log | /usr/bin/tail -40 | tee "$logs/pkg-scripts.txt" || true
echo "installer exit $install_exit"
if [[ $install_exit != 0 ]]; then echo "::error::installer -pkg VACANCY.pkg failed (exit $install_exit)"; status=1; fi
/usr/sbin/pkgutil --pkgs | /usr/bin/grep com.windwardaudio | tee "$logs/pkg-receipts.txt" || true
for receipt in com.windwardaudio.vacancy.pkg.au com.windwardaudio.vacancy.pkg.vst3; do
    /usr/bin/grep -qx "$receipt" "$logs/pkg-receipts.txt" || { echo "::error::no installer receipt $receipt"; status=1; }
done
installed=("$sys_components/VACANCY.component" "$sys_components/VACANCY FX.component" "$sys_vst3/VACANCY.vst3" "$sys_vst3/VACANCY FX.vst3")
for bundle in "${installed[@]}"; do
    if [[ ! -d "$bundle" ]]; then echo "::error::VACANCY.pkg did not install $bundle"; status=1; continue; fi
    owner=$(/usr/bin/stat -f %Su:%Sg "$bundle")
    [[ "$owner" == root:wheel ]] || { echo "::error::$bundle is owned by $owner (expected root:wheel)"; status=1; }
    if /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle" >> "$logs/pkg-codesign.txt" 2>&1; then
        echo "installed and validly signed: $bundle"
    else
        echo "::error::the installed $bundle fails codesign --verify --deep --strict"; status=1
    fi
    if [[ $notarized == 1 ]]; then
        out=$(/usr/sbin/spctl --assess --type open --context context:primary-signature -vvv "$bundle" 2>&1 || true)
        echo "$out" >> "$logs/pkg-gatekeeper.txt"
        echo "$out" | /usr/bin/grep -q "^source=Notarized Developer ID" || { echo "::error::Gatekeeper does not accept the installed $bundle as notarised"; status=1; }
    fi
done
# The recordings travel inside the instrument's bundles when this build has them.
if [[ -d "$src_au/VACANCY.component/Contents/Resources/Samples" ]]; then
    for bundle in "$sys_components/VACANCY.component" "$sys_vst3/VACANCY.vst3"; do
        [[ -f "$bundle/Contents/Resources/Samples/index.json" ]] || { echo "::error::$bundle was installed without its recordings"; status=1; }
    done
fi
for old in "${planted[@]}"; do
    if [[ -e "$old" ]]; then echo "::error::VACANCY.pkg left $old in place"; status=1; else echo "removed by the package: $old"; fi
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
for _ in $(seq 1 30); do
    listing=$(/usr/bin/auval -a 2>/dev/null || true)
    if echo "$listing" | /usr/bin/grep -Eq '^[[:space:]]*aumu[[:space:]]+Lmnl[[:space:]]+Veil' \
        && echo "$listing" | /usr/bin/grep -Eq '^[[:space:]]*aufx[[:space:]]+Tpfx[[:space:]]+Wnwd'; then break; fi
    sleep 1
done
/usr/bin/auval -a > "$logs/pkg-registry-all.txt" 2>&1 || true
/usr/bin/grep -E 'Lmnl|Tpfx' "$logs/pkg-registry-all.txt" | tee "$logs/pkg-registry.txt" || true
for codes in "aumu Lmnl Veil" "aufx Tpfx Wnwd"; do
    pattern="^[[:space:]]*${codes// /[[:space:]]+}"
    n=$(/usr/bin/grep -c -E "$pattern" "$logs/pkg-registry.txt" || true)
    [[ "$n" == 1 ]] || { echo "::error::after the package install the AU registry lists $codes $n times (expected once)"; status=1; }
done
if /usr/bin/grep -iE 'liminal|tapeworn|elsewhere' "$logs/pkg-registry-all.txt"; then echo "::error::the AU registry still shows an old name"; status=1; fi
windward=$(/usr/bin/grep -c 'Windward Audio:' "$logs/pkg-registry-all.txt" || true)
[[ "$windward" == 2 ]] || { echo "::error::the AU registry lists $windward Windward Audio plug-ins (expected 2)"; status=1; }

# 4. Validators on the installed copies.
run_check() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name: exit $code"
    if [[ $code != 0 ]]; then echo "::error::$name failed (exit $code)"; status=1; fi
}
strict=""
/usr/bin/auval -h 2>&1 | /usr/bin/grep -qE -- '(^|[[:space:]])-strict([[:space:]]|$)' && strict="-strict"
states="$here/tools/au-harness"
old_states=(--liminal-state "$states/liminal-session.state" --tapeworn-state "$states/tapeworn-session.state")
fx_old_state=(--tapeworn-fx-state "$states/tapeworn-fx-session.state")
archs="arm64"
if /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then archs="arm64 x86_64"; else echo "::error::Rosetta unavailable: the x86_64 slice was not validated"; status=1; fi
for arch in $archs; do
    # shellcheck disable=SC2086
    run_check "pkg-auval-$arch" /usr/bin/arch -$arch /usr/bin/auval $strict -v aumu Lmnl Veil
    # shellcheck disable=SC2086
    run_check "pkg-auval-fx-$arch" /usr/bin/arch -$arch /usr/bin/auval $strict -v aufx Tpfx Wnwd
    for suite in v2-oop v2-inproc; do
        run_check "pkg-harness-$arch-$suite" /usr/bin/arch -$arch "$harness" --strict --only "$suite" "${old_states[@]}"
        run_check "pkg-harness-fx-$arch-$suite" /usr/bin/arch -$arch "$fx_harness" --strict --only "$suite" "${fx_old_state[@]}"
    done
done
grep -h -E "^(FAIL|SUMMARY)" "$logs"/pkg-harness-*.txt || true
for f in "$logs"/pkg-auval-*.txt; do echo "$(basename "$f"): $(/usr/bin/grep -E 'AU VALIDATION (SUCCEEDED|FAILED)' "$f" || echo 'no verdict')"; done
python3 "$here/scripts/ci-crash-summary.py" 60 LiminalAUHarness TapewornFXAUHarness AUHostingService auval VACANCY > "$logs/pkg-crashes.txt" 2>&1 || true
cat "$logs/pkg-crashes.txt"
exit $status
