#!/usr/bin/env bash
# Developer ID signing, step 2 of 2: delete the temporary keychain, the App Store Connect key
# and every other file scripts/ci-signing-setup.sh wrote, and restore the keychain search
# list. Runs at the end of every job (if: always()); harmless when nothing was set up.
set -uo pipefail
secrets_dir="${RUNNER_TEMP:?}/vacancy-signing"
keychain="$RUNNER_TEMP/vacancy-signing.keychain-db"
if [[ -f "$secrets_dir/keychains-before.txt" ]]; then
    # shellcheck disable=SC2046
    /usr/bin/security list-keychains -d user -s $(/usr/bin/grep -v "vacancy-signing" "$secrets_dir/keychains-before.txt")
fi
if [[ -f "$keychain" ]]; then
    /usr/bin/security delete-keychain "$keychain" && echo "deleted the signing keychain"
fi
rm -rf "$secrets_dir" "$keychain"
/usr/bin/security list-keychains -d user
exit 0
