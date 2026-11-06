#!/usr/bin/env bash
# Install TAPEWORN (the instrument) and TAPEWORN FX (the effect) for the current user, side by
# side, and make sure macOS will load them.
#
# Run it from the unzipped TAPEWORN-macOS-Universal folder:
#     bash install-mac.sh
# (Tip: type "bash " in Terminal, then drag install-mac.sh into the window.)
#
# What it does:
#   0. removes LIMINAL (TAPEWORN's old name): the same plug-in, so hosts would list two copies
#   1. copies TAPEWORN.component and TAPEWORN FX.component to ~/Library/Audio/Plug-Ins/Components
#      and TAPEWORN.vst3 and TAPEWORN FX.vst3 to ~/Library/Audio/Plug-Ins/VST3
#   2. removes the com.apple.quarantine flag your browser added. Without this,
#      macOS refuses to load the plug-in and GarageBand shows a silent slot
#   3. re-signs every bundle (ad hoc) and verifies the signatures
#   4. restarts the Audio Unit registrar so hosts see the new copies
#   5. runs Apple's Audio Unit validator (auval) on both and prints PASS or FAIL
#   6. prints the next steps for GarageBand and Logic
#
# Options:
#   --ci              non-interactive: no prompts, no check for running hosts
#   --out-of-process  remove the sandboxSafe flag from the installed copies, so
#                     GarageBand hosts them in a separate process (fallback only)
# (No `set -u`: macOS bash 3.2 treats an empty "$@" as unbound.)
set -eo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci=0
out_of_process=0
for arg in "$@"; do
    case "$arg" in
        --ci) ci=1 ;;
        --out-of-process) out_of_process=1 ;;
        -h|--help) /usr/bin/sed -n '2,24p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 64 ;;
    esac
done

bold() { printf '\033[1m%s\033[0m\n' "$*"; }
ok()   { printf '  \033[32mOK\033[0m   %s\n' "$*"; }
warn() { printf '  \033[33mNOTE\033[0m %s\n' "$*"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$*"; }

if [[ "$(uname -s)" != Darwin ]]; then
    echo "install-mac.sh runs on macOS only." >&2
    exit 1
fi

component_src="$here/TAPEWORN.component"
vst3_src="$here/TAPEWORN.vst3"
fx_component_src="$here/TAPEWORN FX.component"
fx_vst3_src="$here/TAPEWORN FX.vst3"
if [[ ! -d "$component_src" ]]; then
    echo "TAPEWORN.component was not found next to this script ($here)." >&2
    echo "Unzip TAPEWORN-macOS-Universal.zip first and run the script from that folder." >&2
    exit 1
fi

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
logs="$HOME/Library/Logs/TAPEWORN"
mkdir -p "$components" "$vst3" "$logs"

bold "TAPEWORN + TAPEWORN FX installer"

# Hosts keep their own view of plug-ins while running.
if [[ $ci == 0 ]]; then
    running=$(/usr/bin/pgrep -l -x GarageBand 2>/dev/null; /usr/bin/pgrep -l -f 'Logic Pro' 2>/dev/null || true)
    if [[ -n "$running" ]]; then
        warn "GarageBand or Logic is running. Quit it (Cmd-Q), then press Return to continue."
        read -r _
    fi
fi

# LIMINAL is TAPEWORN's old name: the same Audio Unit (aumu Lmnl Veil) and VST3, so leaving it would
# show two copies in hosts (and the registry would pick one of them). Your songs and presets made
# with LIMINAL open in TAPEWORN; its presets are carried over the first time TAPEWORN lists them.
bold "0. Removing LIMINAL (TAPEWORN's old name)"
old_found=0
for old in "$components/LIMINAL.component" "$vst3/LIMINAL.vst3"; do
    if [[ -e "$old" ]]; then
        rm -rf "$old"
        ok "removed $old"
        old_found=1
    fi
done
for old in "/Library/Audio/Plug-Ins/Components/LIMINAL.component" "/Library/Audio/Plug-Ins/VST3/LIMINAL.vst3"; do
    if [[ -e "$old" ]]; then
        if rm -rf "$old" 2>/dev/null && [[ ! -e "$old" ]]; then
            ok "removed $old"
        elif [[ $ci == 0 ]] && /usr/bin/sudo -n true 2>/dev/null && /usr/bin/sudo rm -rf "$old"; then
            ok "removed $old"
        else
            warn "A system-wide LIMINAL copy remains: $old"
            warn "Remove it so hosts do not show two copies:  sudo rm -rf \"$old\""
        fi
        old_found=1
    fi
done
[[ $old_found == 1 ]] || ok "no LIMINAL copy found"

# A second, system-wide copy would compete with this one.
for system_copy in "/Library/Audio/Plug-Ins/Components/TAPEWORN.component" "/Library/Audio/Plug-Ins/VST3/TAPEWORN.vst3" \
                   "/Library/Audio/Plug-Ins/Components/TAPEWORN FX.component" "/Library/Audio/Plug-Ins/VST3/TAPEWORN FX.vst3"; do
    if [[ -e "$system_copy" ]]; then
        warn "An older system-wide copy exists: $system_copy"
        warn "Remove it so hosts load the new one:  sudo rm -rf \"$system_copy\""
    fi
done

install_bundle() {
    local src="$1" dest="$2" binary name
    name="$(basename "$dest")"
    name="${name%.*}"   # TAPEWORN or TAPEWORN FX
    rm -rf "$dest"
    /usr/bin/ditto "$src" "$dest"
    binary="$dest/Contents/MacOS/$name"
    chmod +x "$binary"
    /usr/bin/xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    if /usr/bin/xattr -lr "$dest" 2>/dev/null | /usr/bin/grep -q com.apple.quarantine; then
        fail "could not remove the quarantine flag from $dest"
        return 1
    fi
    if [[ "$dest" == *.component && $out_of_process == 1 ]]; then
        /usr/libexec/PlistBuddy -c "Delete :AudioComponents:0:sandboxSafe" "$dest/Contents/Info.plist" 2>/dev/null || true
        warn "sandboxSafe removed: GarageBand will host $name out-of-process"
    fi
    # A Developer ID signed and notarised release keeps its signature; anything else
    # is re-signed ad hoc for this Mac.
    if /usr/bin/codesign -dv --verbose=2 "$dest" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application" \
        && [[ $out_of_process == 0 ]]; then
        :
    elif ! /usr/bin/codesign --force --deep --sign - "$dest" > "$logs/codesign-sign-$name.txt" 2>&1; then
        fail "signing failed for $dest (see $logs/codesign-sign-$name.txt)"
        return 1
    fi
    if ! /usr/bin/codesign --verify --strict --verbose=2 "$dest" > "$logs/codesign-$name.txt" 2>&1; then
        fail "signature check failed for $dest (see $logs/codesign-$name.txt)"
        return 1
    fi
    ok "$(basename "$dest") installed, quarantine removed, signature valid ($(/usr/bin/file -b "$binary" | /usr/bin/sed 's/Mach-O universal binary with //'))"
}

bold "1-3. Copying, removing quarantine, signing"
install_bundle "$component_src" "$components/TAPEWORN.component"
if [[ -d "$vst3_src" ]]; then
    install_bundle "$vst3_src" "$vst3/TAPEWORN.vst3"
else
    warn "TAPEWORN.vst3 not in this folder; skipped"
fi
# TAPEWORN FX, the effect: its own Audio Unit (aufx Tpfx Wnwd) next to the instrument.
have_fx=0
if [[ -d "$fx_component_src" ]]; then
    install_bundle "$fx_component_src" "$components/TAPEWORN FX.component"
    have_fx=1
else
    warn "TAPEWORN FX.component not in this folder; the effect was skipped"
fi
if [[ -d "$fx_vst3_src" ]]; then
    install_bundle "$fx_vst3_src" "$vst3/TAPEWORN FX.vst3"
else
    warn "TAPEWORN FX.vst3 not in this folder; skipped"
fi

bold "4. Resetting the Audio Unit registry"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
registered=0
fx_registered=0
for _ in $(seq 1 30); do
    listing=$(/usr/bin/auval -a 2>/dev/null || true)
    echo "$listing" | /usr/bin/grep -Eq '^[[:space:]]*aumu[[:space:]]+Lmnl[[:space:]]+Veil' && registered=1
    echo "$listing" | /usr/bin/grep -Eq '^[[:space:]]*aufx[[:space:]]+Tpfx[[:space:]]+Wnwd' && fx_registered=1
    if [[ $registered == 1 && ( $have_fx == 0 || $fx_registered == 1 ) ]]; then break; fi
    sleep 1
done
if [[ $registered == 1 ]]; then ok "macOS lists aumu Lmnl Veil (Elsewhere Audio: TAPEWORN)"; else fail "TAPEWORN is not listed by the Audio Unit registry after 30 s"; fi
if [[ $have_fx == 1 ]]; then
    if [[ $fx_registered == 1 ]]; then ok "macOS lists aufx Tpfx Wnwd (Windward Audio: TAPEWORN FX)"; else fail "TAPEWORN FX is not listed by the Audio Unit registry after 30 s"; fi
fi

bold "5. Validating with Apple's auval"
auval_log="$logs/install-auval.txt"
set +e
/usr/bin/auval -v aumu Lmnl Veil > "$auval_log" 2>&1
auval_exit=$?
set -e
if [[ $auval_exit == 0 ]] && /usr/bin/grep -q "AU VALIDATION SUCCEEDED" "$auval_log"; then
    ok "auval: PASS ($(uname -m))"
    result=0
else
    fail "auval: FAIL ($(uname -m), exit $auval_exit). Full log: $auval_log"
    /usr/bin/grep -E "FATAL|ERROR|Cannot open" "$auval_log" | /usr/bin/head -5 | /usr/bin/sed 's/^/       /'
    result=1
fi
if [[ "$(uname -m)" == arm64 ]] && /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
    set +e
    /usr/bin/arch -x86_64 /usr/bin/auval -v aumu Lmnl Veil > "$logs/install-auval-x86_64.txt" 2>&1
    x86_exit=$?
    set -e
    if [[ $x86_exit == 0 ]]; then ok "auval: PASS (x86_64 under Rosetta)"; else warn "auval under Rosetta exited $x86_exit (only matters for Intel-mode hosts)"; fi
fi
if [[ $have_fx == 1 ]]; then
    fx_log="$logs/install-auval-fx.txt"
    set +e
    /usr/bin/auval -v aufx Tpfx Wnwd > "$fx_log" 2>&1
    fx_exit=$?
    set -e
    if [[ $fx_exit == 0 ]] && /usr/bin/grep -q "AU VALIDATION SUCCEEDED" "$fx_log"; then
        ok "auval TAPEWORN FX: PASS ($(uname -m))"
    else
        fail "auval TAPEWORN FX: FAIL ($(uname -m), exit $fx_exit). Full log: $fx_log"
        /usr/bin/grep -E "FATAL|ERROR|Cannot open" "$fx_log" | /usr/bin/head -5 | /usr/bin/sed 's/^/       /'
        result=1
    fi
fi

bold "6. Next steps"
cat <<'NEXT'
  GarageBand
    1. Open GarageBand > Settings > Audio/MIDI and make sure "Audio Units" is ticked.
    2. Quit and reopen GarageBand.
    3. Create a Software Instrument track (Track > New Track > Software Instrument).
       Open Smart Controls (B), click Plug-ins, click the Instrument slot (the top
       slot): AU Instruments > Elsewhere Audio > TAPEWORN.
    4. The room ambience plays straight away while the track is selected. Play notes
       (a MIDI keyboard, or Window > Show Musical Typing) to hear the Forgotten Piano.
    5. If the window shows sliders instead of the TAPEWORN interface, use the view menu
       at the top of the plug-in window to switch from "Controls" to "Editor".
  TAPEWORN FX (the effect) in GarageBand
    1. Select any audio track (a guitar, a voice, a loop) or a software instrument track.
    2. Open Smart Controls (B), click Plug-ins, click an empty slot:
       Audio Units > Windward Audio > TAPEWORN FX.
    3. Play the track: it is heard in the place on screen. Choose another place in the
       film strip; turn LIMINALITY up for worn, unstable tape.
  Logic Pro
    1. Logic Pro > Settings > Plug-in Manager, find Elsewhere Audio > TAPEWORN and
       Windward Audio > TAPEWORN FX. If one shows as failed, select it and click
       "Reset & Rescan Selection".
    2. On a Software Instrument track, click the Instrument slot:
       AU Instruments > Elsewhere Audio > TAPEWORN.
    3. On any track, click an Audio FX slot: Audio Units > Windward Audio > TAPEWORN FX.
NEXT
if [[ $result == 0 ]]; then
    bold "TAPEWORN and TAPEWORN FX are installed and validated."
else
    bold "Installed, but validation failed. Send the logs in $logs when reporting this."
fi
exit $result
