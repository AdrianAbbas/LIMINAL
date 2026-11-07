#!/usr/bin/env bash
# Remove VACANCY and VACANCY FX for the current user.
#   bash uninstall-mac.sh           remove the plug-ins, keep your saved presets
#   bash uninstall-mac.sh --purge   also remove your presets and cached scenes
#                                   (~/Library/Windward Audio/VACANCY and ~/Library/Windward Audio/VACANCY FX,
#                                   and the folders the old names used)
# Copies under the old names (LIMINAL, TAPEWORN, TAPEWORN FX) are removed too.
set -euo pipefail
purge=0
[[ "${1:-}" == --purge ]] && purge=1

names=("VACANCY" "VACANCY FX" "LIMINAL" "TAPEWORN" "TAPEWORN FX")
removed=0
for name in "${names[@]}"; do
    for bundle in "$HOME/Library/Audio/Plug-Ins/Components/$name.component" "$HOME/Library/Audio/Plug-Ins/VST3/$name.vst3"; do
        if [[ -e "$bundle" ]]; then
            rm -rf "$bundle"
            echo "Removed $bundle"
            removed=1
        fi
    done
done
[[ $removed == 1 ]] || echo "No VACANCY / VACANCY FX plug-ins found in ~/Library/Audio/Plug-Ins."

# (JUCE's user application data folder on macOS is ~/Library.)
for presets in "$HOME/Library/Windward Audio/VACANCY" "$HOME/Library/Windward Audio/VACANCY FX" \
               "$HOME/Library/Elsewhere Audio/TAPEWORN" "$HOME/Library/Windward Audio/TAPEWORN FX" "$HOME/Library/Veil Audio/LIMINAL"; do
    if [[ $purge == 1 && -d "$presets" ]]; then
        rm -rf "$presets"
        echo "Removed $presets"
        rmdir "$(dirname "$presets")" 2>/dev/null || true
    elif [[ -d "$presets" ]]; then
        echo "Kept your presets in $presets (use --purge to remove them)."
    fi
done

for name in "${names[@]}"; do
    for system_copy in "/Library/Audio/Plug-Ins/Components/$name.component" "/Library/Audio/Plug-Ins/VST3/$name.vst3"; do
        if [[ -e "$system_copy" ]]; then echo "A system-wide copy remains: sudo rm -rf \"$system_copy\""; fi
    done
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Audio Unit registry reset. Restart GarageBand or Logic."
