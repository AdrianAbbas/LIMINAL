#!/usr/bin/env bash
# LIMINAL Mac installer. Original script: MIT; distributed plugin: AGPLv3.
set -euo pipefail
if [ "$(uname -s)" != Darwin ]; then
  printf 'This installer is for macOS.\n' >&2
  exit 1
fi
major=$(/usr/bin/sw_vers -productVersion | /usr/bin/cut -d . -f 1)
if [ "$major" -lt 11 ]; then
  printf 'LIMINAL needs macOS 11 or newer.\n' >&2
  exit 1
fi
version=v0.2.0-milestone1.2
base="https://github.com/AdrianAbbas/LIMINAL/releases/download/$version"
archive=LIMINAL-macOS-universal.zip
work=$(mktemp -d "${TMPDIR:-/tmp}/liminal-install.XXXXXX")
trap 'rm -rf "$work"' EXIT
printf '\nDownloading LIMINAL for Intel and Apple Silicon…\n'
fetch() {
  /usr/bin/curl --fail --location --silent --show-error --retry 2 --connect-timeout 15 --max-time 180 "$base/$1" --output "$work/$1"
}
if ! fetch SHA256SUMS.txt; then
  printf '\nThe Mac download is not available yet. Check https://github.com/AdrianAbbas/LIMINAL/releases\n' >&2
  exit 1
fi
fetch "$archive"
expected=$(/usr/bin/awk -v name="$archive" '$2 == name {print $1}' "$work/SHA256SUMS.txt")
if [ "${#expected}" -ne 64 ]; then
  printf 'The release checksum is missing. Installation stopped.\n' >&2
  exit 1
fi
printf '%s  %s\n' "$expected" "$archive" > "$work/check.txt"
(cd "$work" && /usr/bin/shasum -a 256 -c check.txt)
/usr/bin/ditto -x -k "$work/$archive" "$work/unpacked"
bundles="$work/unpacked/bundles"
for name in LIMINAL.app LIMINAL.component LIMINAL.vst3; do
  test -d "$bundles/$name"
  /usr/bin/codesign --verify --deep --strict "$bundles/$name"
done
stamp=$(/bin/date +%Y%m%d-%H%M%S)
install_bundle() {
  source_bundle="$bundles/$1"
  destination="$2/$1"
  /bin/mkdir -p "$2"
  if [ -e "$destination" ]; then
    /bin/mv "$destination" "$destination.backup-$stamp"
    printf 'Previous version preserved: %s\n' "$destination.backup-$stamp"
  fi
  /usr/bin/ditto "$source_bundle" "$destination"
  printf 'Installed: %s\n' "$destination"
}
install_bundle LIMINAL.component "$HOME/Library/Audio/Plug-Ins/Components"
install_bundle LIMINAL.vst3 "$HOME/Library/Audio/Plug-Ins/VST3"
install_bundle LIMINAL.app "$HOME/Applications"
/bin/mkdir -p "$HOME/Library/Application Support/LIMINAL"
/usr/bin/ditto "$bundles/READ-ME.txt" "$HOME/Library/Application Support/LIMINAL/READ-ME.txt"
/usr/bin/ditto "$bundles/AGPL-3.0.txt" "$HOME/Library/Application Support/LIMINAL/AGPL-3.0.txt"
printf '\nDone. Restart your DAW and add LIMINAL as an instrument.\n'
printf 'Logic / GarageBand: Audio Units. Other compatible hosts: VST3.\n'
printf 'Standalone app: %s/Applications/LIMINAL.app\n' "$HOME"
printf 'Unsigned preview: macOS may ask for approval in Privacy & Security.\n'
