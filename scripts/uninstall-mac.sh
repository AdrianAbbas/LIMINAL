#!/usr/bin/env bash
# Remove TAPEWORN and TAPEWORN FX for the current user.
#   bash uninstall-mac.sh           remove the plug-ins, keep your saved presets
#   bash uninstall-mac.sh --purge   also remove your presets and cached scenes
#                                   (~/Library/Elsewhere Audio/TAPEWORN and ~/Library/Windward Audio/TAPEWORN FX)
# Copies under the old name (LIMINAL) are removed too.
set -euo pipefail
purge=0
[[ "${1:-}" == --purge ]] && purge=1

removed=0
for bundle in "$HOME/Library/Audio/Plug-Ins/Components/TAPEWORN.component" "$HOME/Library/Audio/Plug-Ins/VST3/TAPEWORN.vst3" \
              "$HOME/Library/Audio/Plug-Ins/Components/TAPEWORN FX.component" "$HOME/Library/Audio/Plug-Ins/VST3/TAPEWORN FX.vst3" \
              "$HOME/Library/Audio/Plug-Ins/Components/LIMINAL.component" "$HOME/Library/Audio/Plug-Ins/VST3/LIMINAL.vst3"; do
    if [[ -e "$bundle" ]]; then
        rm -rf "$bundle"
        echo "Removed $bundle"
        removed=1
    fi
done
[[ $removed == 1 ]] || echo "No TAPEWORN / TAPEWORN FX plug-ins found in ~/Library/Audio/Plug-Ins."

# (JUCE's user application data folder on macOS is ~/Library.)
for presets in "$HOME/Library/Elsewhere Audio/TAPEWORN" "$HOME/Library/Windward Audio/TAPEWORN FX"; do
    if [[ $purge == 1 && -d "$presets" ]]; then
        rm -rf "$presets"
        echo "Removed $presets"
    elif [[ -d "$presets" ]]; then
        echo "Kept your presets in $presets (use --purge to remove them)."
    fi
done

for system_copy in "/Library/Audio/Plug-Ins/Components/TAPEWORN.component" "/Library/Audio/Plug-Ins/VST3/TAPEWORN.vst3" \
                   "/Library/Audio/Plug-Ins/Components/TAPEWORN FX.component" "/Library/Audio/Plug-Ins/VST3/TAPEWORN FX.vst3" \
                   "/Library/Audio/Plug-Ins/Components/LIMINAL.component" "/Library/Audio/Plug-Ins/VST3/LIMINAL.vst3"; do
    [[ -e "$system_copy" ]] && echo "A system-wide copy remains: sudo rm -rf \"$system_copy\""
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Audio Unit registry reset. Restart GarageBand or Logic."
