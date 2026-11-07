#!/usr/bin/env bash
# Reproduce what a user does, then validate like a host.
#   1. unzip the release package and mark every file as a browser download
#      (com.apple.quarantine), which is what broke VEIL in GarageBand
#   2. record whether macOS on this runner refuses the quarantined copy
#   3. install with install-mac.sh --ci, over copies under the old names (LIMINAL, TAPEWORN,
#      TAPEWORN FX, and one renamed copy with an old bundle ID) it must remove
#   4. auval and the AU host harnesses (--strict), arm64 and x86_64 (Rosetta), for both plug-ins:
#      VACANCY (aumu Lmnl Veil) and VACANCY FX (aufx Tpfx Wnwd, its harness TapewornFXAUHarness
#      next to the instrument's), each in-process and out-of-process
# Usage: scripts/ci-user-install-test.sh <VACANCY-macOS-Universal.zip> <instrument harness binary> <log dir>
set -euo pipefail
package="$1"
harness="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
fx_harness="$(dirname "$harness")/TapewornFXAUHarness"
logs="$3"
mkdir -p "$logs"
work="$(mktemp -d)"
status=0

# Run a command with a time limit (a Gatekeeper prompt would otherwise block forever).
limit() { local seconds="$1"; shift; /usr/bin/perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!\n"' "$seconds" "$@"; }

/usr/bin/ditto -x -k "$package" "$work"
folder="$work/VACANCY-macOS-Universal"
flag="0081;$(printf '%x' "$(date +%s)");Chrome;$(/usr/bin/uuidgen)"
/usr/bin/find "$folder" -exec /usr/bin/xattr -w com.apple.quarantine "$flag" {} +
{
    echo "Gatekeeper: $(/usr/sbin/spctl --status 2>&1)"
    /usr/bin/xattr -l "$folder/VACANCY.component/Contents/MacOS/VACANCY"
} | tee "$logs/quarantine.txt"

# 2. Quarantined copy installed by hand, without the script (informational).
components="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$components"
rm -rf "$components/VACANCY.component"
/usr/bin/ditto "$folder/VACANCY.component" "$components/VACANCY.component"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
set +e
limit 120 /usr/bin/auval -v aumu Lmnl Veil > "$logs/auval-quarantined-without-script.txt" 2>&1
quarantined_exit=$?
set -e
echo "auval on the quarantined copy (no install script): exit $quarantined_exit" | tee -a "$logs/quarantine.txt"
rm -rf "$components/VACANCY.component"

# 3. The user's path: run the installer from the quarantined download. Copies under the old names
# (LIMINAL, TAPEWORN, TAPEWORN FX: the same Audio Units) are planted first, with their old bundle IDs,
# and one renamed by hand that only its old bundle ID gives away: the installer must remove them all.
vst3_dir="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$vst3_dir"
plant() {   # plant <source bundle> <destination> <old bundle ID>
    /usr/bin/ditto "$1" "$2"
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $3" "$2/Contents/Info.plist"
    echo "planted $2 ($3)"
}
plant "$folder/VACANCY.component" "$components/LIMINAL.component" audio.veil.liminal
plant "$folder/VACANCY.vst3" "$vst3_dir/LIMINAL.vst3" audio.veil.liminal
plant "$folder/VACANCY.component" "$components/TAPEWORN.component" com.elsewhereaudio.tapeworn
plant "$folder/VACANCY.vst3" "$vst3_dir/TAPEWORN.vst3" com.elsewhereaudio.tapeworn
plant "$folder/VACANCY FX.component" "$components/TAPEWORN FX.component" com.windwardaudio.tapewornfx
plant "$folder/VACANCY FX.vst3" "$vst3_dir/TAPEWORN FX.vst3" com.windwardaudio.tapewornfx
plant "$folder/VACANCY FX.component" "$components/Old Effect Copy.component" com.windwardaudio.tapewornfx
set +e
limit 300 /bin/bash "$folder/install-mac.sh" --ci 2>&1 | tee "$logs/install-mac.txt"
install_exit=${PIPESTATUS[0]}
set -e
echo "install-mac.sh exit $install_exit"
if [[ $install_exit != 0 ]]; then echo "::error::install-mac.sh failed (exit $install_exit)"; status=1; fi
for installed in "$components/VACANCY.component" "$components/VACANCY FX.component" "$vst3_dir/VACANCY.vst3" "$vst3_dir/VACANCY FX.vst3"; do
    if [[ ! -d "$installed" ]]; then echo "::error::install-mac.sh did not install $installed"; status=1; continue; fi
    if /usr/bin/xattr -lr "$installed" | /usr/bin/grep -q com.apple.quarantine; then
        echo "::error::installed $(basename "$installed") is still quarantined"; status=1
    fi
done
for old in "$components/LIMINAL.component" "$vst3_dir/LIMINAL.vst3" "$components/TAPEWORN.component" "$vst3_dir/TAPEWORN.vst3" \
           "$components/TAPEWORN FX.component" "$vst3_dir/TAPEWORN FX.vst3" "$components/Old Effect Copy.component"; do
    if [[ -e "$old" ]]; then echo "::error::install-mac.sh left the old $old in place"; status=1; else echo "old copy removed: $old"; fi
done
# The registry lists the instrument once, under its new name and company ...
/usr/bin/auval -a 2>&1 | /usr/bin/grep -E 'Lmnl' | tee "$logs/registry.txt"
entries=$(/usr/bin/grep -c -E '^[[:space:]]*aumu[[:space:]]+Lmnl[[:space:]]+Veil' "$logs/registry.txt" || true)
if [[ "$entries" != 1 ]]; then echo "::error::the AU registry lists aumu Lmnl Veil $entries times (expected once)"; status=1; fi
if ! /usr/bin/grep -q 'Windward Audio: VACANCY' "$logs/registry.txt"; then echo "::error::the AU registry does not name it 'Windward Audio: VACANCY'"; status=1; fi
# ... and the effect once, as itself, side by side, under the same company.
/usr/bin/auval -a 2>&1 | /usr/bin/grep -E 'Tpfx' | tee "$logs/registry-fx.txt"
fx_entries=$(/usr/bin/grep -c -E '^[[:space:]]*aufx[[:space:]]+Tpfx[[:space:]]+Wnwd' "$logs/registry-fx.txt" || true)
if [[ "$fx_entries" != 1 ]]; then echo "::error::the AU registry lists aufx Tpfx Wnwd $fx_entries times (expected once)"; status=1; fi
if ! /usr/bin/grep -q 'Windward Audio: VACANCY FX' "$logs/registry-fx.txt"; then echo "::error::the AU registry does not name it 'Windward Audio: VACANCY FX'"; status=1; fi
# Nowhere in the registry the old names or the old company.
/usr/bin/auval -a > "$logs/registry-all.txt" 2>&1 || true
if /usr/bin/grep -iE 'liminal|tapeworn|elsewhere' "$logs/registry-all.txt"; then echo "::error::the AU registry still shows an old name or the old company"; status=1; fi
windward=$(/usr/bin/grep -c 'Windward Audio:' "$logs/registry-all.txt" || true)
echo "Windward Audio entries in the registry: $windward"
if [[ "$windward" != 2 ]]; then echo "::error::the AU registry lists $windward Windward Audio plug-ins (expected 2: VACANCY and VACANCY FX)"; status=1; fi

# 4. Validators on the installed copy.
run_check() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name: exit $code"
    if [[ $code != 0 ]]; then echo "::error::$name failed (exit $code)"; status=1; fi
}
strict=""  # becomes -strict when this auval supports it
/usr/bin/auval -h 2>&1 | /usr/bin/grep -qE -- '(^|[[:space:]])-strict([[:space:]]|$)' && strict="-strict"
# shellcheck disable=SC2086  # $strict is empty or one flag
run_check auval-arm64 /usr/bin/arch -arm64 /usr/bin/auval $strict -v aumu Lmnl Veil
# Diagnostic run: reported in the logs, never fails the build.
run_diagnostic() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name (diagnostic, not a gate): exit $code"
}
# One process per hosting route, so a crash in one cannot hide the others. The
# brief's routes (AUv2 API out-of-process and in-process) are gates. The AUAudioUnit
# route is diagnostic only: as driven by the harness it gives silence/-10876 for
# Apple's own AULowpass too (see docs/DEVLOG.md), so it cannot judge VACANCY yet.
suites="v2-oop v2-inproc"
# Songs' states saved by LIMINAL and TAPEWORN (VACANCY's old names) and by TAPEWORN FX (VACANCY FX's):
# the harnesses restore them and read their settings back.
states_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/tools/au-harness"
old_states=(--liminal-state "$states_dir/liminal-session.state" --tapeworn-state "$states_dir/tapeworn-session.state")
fx_old_state=(--tapeworn-fx-state "$states_dir/tapeworn-fx-session.state")
for suite in $suites; do run_check "harness-arm64-$suite" /usr/bin/arch -arm64 "$harness" --strict --only "$suite" "${old_states[@]}"; done
run_diagnostic harness-arm64-auaudiounit /usr/bin/arch -arm64 "$harness" --only auaudiounit
# VACANCY FX: auval for an effect (aufx) and its harness, out-of-process and in-process (gates).
# shellcheck disable=SC2086
run_check auval-fx-arm64 /usr/bin/arch -arm64 /usr/bin/auval $strict -v aufx Tpfx Wnwd
for suite in $suites; do run_check "harness-fx-arm64-$suite" /usr/bin/arch -arm64 "$fx_harness" --strict --only "$suite" "${fx_old_state[@]}"; done
run_diagnostic harness-fx-arm64-auaudiounit /usr/bin/arch -arm64 "$fx_harness" --only auaudiounit
if /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
    # shellcheck disable=SC2086
    run_check auval-x86_64 /usr/bin/arch -x86_64 /usr/bin/auval $strict -v aumu Lmnl Veil
    for suite in $suites; do run_check "harness-x86_64-$suite" /usr/bin/arch -x86_64 "$harness" --strict --only "$suite" "${old_states[@]}"; done
    run_diagnostic harness-x86_64-auaudiounit /usr/bin/arch -x86_64 "$harness" --only auaudiounit
    # shellcheck disable=SC2086
    run_check auval-fx-x86_64 /usr/bin/arch -x86_64 /usr/bin/auval $strict -v aufx Tpfx Wnwd
    for suite in $suites; do run_check "harness-fx-x86_64-$suite" /usr/bin/arch -x86_64 "$fx_harness" --strict --only "$suite" "${fx_old_state[@]}"; done
    run_diagnostic harness-fx-x86_64-auaudiounit /usr/bin/arch -x86_64 "$fx_harness" --only auaudiounit
else
    echo "::error::Rosetta unavailable: the x86_64 slice was not validated"; status=1
fi
# Control (diagnostic, not a gate): Apple's own DLS synth through the AUAudioUnit route,
# to tell a harness problem on that route from a VACANCY problem.
set +e
limit 300 "$harness" --only auaudiounit --no-window --component "aumu:dls :appl" > "$logs/control-apple-dls-auaudiounit.txt" 2>&1
echo "control (Apple DLSMusicDevice, AUAudioUnit route): exit $?"
set -e
grep -h -E "^(FAIL|SUMMARY)" "$logs"/harness-*.txt || true
python3 "$(dirname "${BASH_SOURCE[0]}")/ci-crash-summary.py" 60 LiminalAUHarness TapewornFXAUHarness AUHostingService auval VACANCY > "$logs/crashes.txt" 2>&1 || true
cat "$logs/crashes.txt"
exit $status
