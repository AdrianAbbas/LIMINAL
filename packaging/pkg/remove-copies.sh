# Shared body of VACANCY.pkg's preinstall scripts (scripts/build-pkg.sh puts a line
# `mode=prepare|au|vst3` above it). Runs as root, with macOS's bash 3.2. Everything it removes is
# logged to /var/log/install.log. Never removes presets or anything outside the plug-in folders.
#   mode=prepare  LIMINAL, TAPEWORN and TAPEWORN FX (VACANCY's and VACANCY FX's old names: the same
#              Audio Units and VST3s, so hosts would list two copies), by name and by old bundle
#              ID, in every format, system-wide and in every user's ~/Library
#   mode=au    other copies of VACANCY.component and VACANCY FX.component: per-user copies
#              (installed from the zip with install-mac.sh) and renamed copies, which would
#              compete with the system-wide copy this package installs
#   mode=vst3  the same for VACANCY.vst3 and VACANCY FX.vst3
# shellcheck disable=SC2154  # mode is set by the line build-pkg.sh puts above this body
set -u
log() { echo "VACANCY installer: $*"; }
case "$mode" in
    prepare) exts="component vst3"; names="LIMINAL|TAPEWORN|TAPEWORN FX"
          ids=" audio.veil.liminal com.elsewhereaudio.tapeworn com.windwardaudio.tapewornfx "
          # Receipts of the package's earlier identifiers (com.windwardaudio.vacancy.pkg.*).
          for receipt in au vst3 remove-old; do
              /usr/sbin/pkgutil --forget "com.windwardaudio.vacancy.pkg.$receipt" > /dev/null 2>&1 || true
          done ;;
    au)   exts="component"; names="VACANCY|VACANCY FX"; ids=" com.windwardaudio.vacancy com.windwardaudio.vacancyfx " ;;
    vst3) exts="vst3"; names="VACANCY|VACANCY FX"; ids=" com.windwardaudio.vacancy com.windwardaudio.vacancyfx " ;;
    *)    log "unknown mode $mode"; exit 0 ;;
esac
remove() {
    if rm -rf "$1"; then log "removed $1"; else log "could not remove $1"; fi
}
for home in /Library /Users/*/Library; do
    [ -d "$home/Audio/Plug-Ins" ] || continue
    for ext in $exts; do
        if [ "$ext" = component ]; then folder="$home/Audio/Plug-Ins/Components"; else folder="$home/Audio/Plug-Ins/VST3"; fi
        [ -d "$folder" ] || continue
        for bundle in "$folder"/*."$ext"; do
            [ -e "$bundle" ] || continue
            base="$(basename "$bundle" ".$ext")"
            id="$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)"
            by_name=0; by_id=0
            case "|$names|" in *"|$base|"*) by_name=1 ;; esac
            if [ -n "$id" ]; then case "$ids" in *" $id "*) by_id=1 ;; esac; fi
            [ $by_name = 1 ] || [ $by_id = 1 ] || continue
            # The copy this package installs (mode au/vst3, system-wide, its own name) is
            # replaced by the payload; leave it to the installer.
            if [ "$mode" != prepare ] && [ "$home" = /Library ] && [ $by_name = 1 ]; then continue; fi
            remove "$bundle"
        done
    done
done
exit 0
