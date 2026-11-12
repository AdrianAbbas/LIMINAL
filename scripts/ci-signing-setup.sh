#!/usr/bin/env bash
# Developer ID signing, step 1 of 2: a temporary keychain on the CI runner with the two
# Developer ID certificates (Application and Installer, team YYZP4W7G7D) and Apple's
# Developer ID G2 intermediate, plus the App Store Connect API key for notarytool.
# scripts/ci-signing-cleanup.sh deletes all of it at the end of the job (docs/RELEASING.md).
#
# Environment (GitHub secrets; never printed):
#   MACOS_APP_CERT_P12, MACOS_APP_CERT_PASSWORD              Developer ID Application, base64 .p12
#   MACOS_INSTALLER_CERT_P12, MACOS_INSTALLER_CERT_PASSWORD  Developer ID Installer, base64 .p12
#   ASC_KEY_P8                                               App Store Connect API key, base64 .p8
#   APPLE_TEAM_ID                                            YYZP4W7G7D (not a secret)
# Writes to $GITHUB_ENV (none of these is secret):
#   VACANCY_KEYCHAIN, VACANCY_APP_IDENTITY (SHA-1), VACANCY_APP_IDENTITY_NAME,
#   VACANCY_INSTALLER_IDENTITY (name), VACANCY_ASC_KEY_PATH
set -euo pipefail
umask 077
: "${RUNNER_TEMP:?}" "${GITHUB_ENV:?}" "${APPLE_TEAM_ID:?}"
for name in MACOS_APP_CERT_P12 MACOS_APP_CERT_PASSWORD MACOS_INSTALLER_CERT_P12 MACOS_INSTALLER_CERT_PASSWORD ASC_KEY_P8; do
    if [[ -z "${!name:-}" ]]; then echo "::error::$name is empty"; exit 1; fi
done
secrets_dir="$RUNNER_TEMP/vacancy-signing"
keychain="$RUNNER_TEMP/vacancy-signing.keychain-db"
rm -rf "$secrets_dir"
mkdir -p "$secrets_dir"
keychain_password="$(/usr/bin/uuidgen)"
echo "::add-mask::$keychain_password"

# The keychain: unlocked for 6 hours, never locked on sleep, searched before the login keychain.
/usr/bin/security delete-keychain "$keychain" 2>/dev/null || true
/usr/bin/security create-keychain -p "$keychain_password" "$keychain"
/usr/bin/security set-keychain-settings -lut 21600 "$keychain"
/usr/bin/security unlock-keychain -p "$keychain_password" "$keychain"
# Remember the search list so the clean-up step can restore it.
/usr/bin/security list-keychains -d user | /usr/bin/sed -e 's/^[[:space:]]*"//' -e 's/"$//' > "$secrets_dir/keychains-before.txt"
# shellcheck disable=SC2046
/usr/bin/security list-keychains -d user -s "$keychain" $(cat "$secrets_dir/keychains-before.txt")

# Apple's Developer ID G2 intermediate, checked against the Apple Root CA in the system roots.
g2="$secrets_dir/DeveloperIDG2CA.cer"
for attempt in 1 2 3 4; do
    /usr/bin/curl -fsSL --retry 3 -o "$g2" https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer && break
    echo "download of the G2 intermediate failed (attempt $attempt)"; sleep $((attempt * 5))
done
/usr/bin/security find-certificate -a -c "Apple Root CA" -p /System/Library/Keychains/SystemRootCertificates.keychain > "$secrets_dir/apple-roots.pem"
/usr/bin/openssl x509 -inform der -in "$g2" -out "$secrets_dir/DeveloperIDG2CA.pem"
subject=$(/usr/bin/openssl x509 -in "$secrets_dir/DeveloperIDG2CA.pem" -noout -subject)
echo "G2 intermediate: $subject"
if [[ "$subject" != *"Developer ID Certification Authority"* || "$subject" != *"G2"* ]]; then
    echo "::error::the downloaded intermediate is not Apple's Developer ID G2 CA"; exit 1
fi
if ! /usr/bin/openssl verify -CAfile "$secrets_dir/apple-roots.pem" "$secrets_dir/DeveloperIDG2CA.pem"; then
    echo "::error::the Developer ID G2 intermediate does not chain to Apple Root CA"; exit 1
fi
/usr/bin/security import "$g2" -k "$keychain" -t cert

# The two identities. A .p12 written by OpenSSL 3 (AES, PBKDF2) is not always readable by
# `security import`: then it is re-packed in memory in the legacy format and imported again.
import_p12() {   # import_p12 <base64 variable> <password variable> <label>
    local p12="$secrets_dir/$3.p12" repacked="$secrets_dir/$3-legacy.p12"
    printf '%s' "${!1}" | /usr/bin/base64 --decode > "$p12"
    if /usr/bin/security import "$p12" -k "$keychain" -f pkcs12 -P "${!2}" \
        -T /usr/bin/codesign -T /usr/bin/productbuild -T /usr/bin/productsign -T /usr/bin/pkgbuild -T /usr/bin/security > /dev/null; then
        echo "imported the $3 certificate"
    else
        local openssl3
        openssl3=$(command -v /opt/homebrew/opt/openssl@3/bin/openssl /usr/local/opt/openssl@3/bin/openssl 2>/dev/null | head -1 || true)
        if [[ -z "$openssl3" ]]; then echo "::error::could not import the $3 certificate (wrong password?)"; return 1; fi
        echo "re-packing the $3 certificate in the legacy PKCS#12 format"
        P12_PASS="${!2}" "$openssl3" pkcs12 -in "$p12" -passin env:P12_PASS -nodes \
            | P12_PASS="${!2}" "$openssl3" pkcs12 -export -legacy -passout env:P12_PASS -out "$repacked"
        /usr/bin/security import "$repacked" -k "$keychain" -f pkcs12 -P "${!2}" \
            -T /usr/bin/codesign -T /usr/bin/productbuild -T /usr/bin/productsign -T /usr/bin/pkgbuild -T /usr/bin/security > /dev/null \
            || { echo "::error::could not import the $3 certificate (wrong password?)"; return 1; }
        echo "imported the $3 certificate"
    fi
    rm -f "$p12" "$repacked"
}
import_p12 MACOS_APP_CERT_P12 MACOS_APP_CERT_PASSWORD application
import_p12 MACOS_INSTALLER_CERT_P12 MACOS_INSTALLER_CERT_PASSWORD installer
# Let codesign and productbuild use the keys without a prompt.
/usr/bin/security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain" > /dev/null

# Find the identities of this team (names and hashes are public, printed on every signature).
/usr/bin/security find-identity -v -p codesigning "$keychain" | tee "$secrets_dir/identities-codesigning.txt"
/usr/bin/security find-identity -v "$keychain" > "$secrets_dir/identities-all.txt"
app_line=$(/usr/bin/grep -E "\"Developer ID Application: .*\($APPLE_TEAM_ID\)\"" "$secrets_dir/identities-codesigning.txt" | head -1 || true)
installer_line=$(/usr/bin/grep -E "\"Developer ID Installer: .*\($APPLE_TEAM_ID\)\"" "$secrets_dir/identities-all.txt" | head -1 || true)
if [[ -z "$app_line" ]]; then echo "::error::no valid 'Developer ID Application' identity of team $APPLE_TEAM_ID in MACOS_APP_CERT_P12"; exit 1; fi
if [[ -z "$installer_line" ]]; then
    /usr/bin/grep -E "Developer ID Installer" "$secrets_dir/identities-all.txt" || true
    echo "::error::no valid 'Developer ID Installer' identity of team $APPLE_TEAM_ID in MACOS_INSTALLER_CERT_P12"; exit 1
fi
app_hash=$(echo "$app_line" | /usr/bin/awk '{print $2}')
app_name=$(echo "$app_line" | /usr/bin/sed -E 's/^[^"]*"(.*)"[^"]*$/\1/')
installer_name=$(echo "$installer_line" | /usr/bin/sed -E 's/^[^"]*"(.*)"[^"]*$/\1/')
echo "Application identity: $app_name ($app_hash)"
echo "Installer identity:   $installer_name"
# Certificate expiry, so a renewal is never a surprise (docs/RELEASING.md).
for cn in "$app_name" "$installer_name"; do
    end=$(/usr/bin/security find-certificate -c "$cn" -p "$keychain" | /usr/bin/openssl x509 -noout -enddate | /usr/bin/cut -d= -f2)
    end_epoch=$(/bin/date -j -f "%b %e %T %Y %Z" "$end" +%s 2>/dev/null || echo 0)
    days=$(( (end_epoch - $(/bin/date +%s)) / 86400 ))
    echo "$cn expires $end ($days days)"
    if (( end_epoch > 0 && days < 60 )); then echo "::warning::$cn expires in $days days: renew it (docs/RELEASING.md)"; fi
done

# The App Store Connect API key for notarytool.
key_path="$secrets_dir/AuthKey.p8"
printf '%s' "$ASC_KEY_P8" | /usr/bin/base64 --decode > "$key_path"
if ! /usr/bin/grep -q "BEGIN PRIVATE KEY" "$key_path"; then echo "::error::ASC_KEY_P8 is not a base64 .p8 private key"; exit 1; fi

{
    echo "VACANCY_KEYCHAIN=$keychain"
    echo "VACANCY_APP_IDENTITY=$app_hash"
    echo "VACANCY_APP_IDENTITY_NAME=$app_name"
    echo "VACANCY_INSTALLER_IDENTITY=$installer_name"
    echo "VACANCY_ASC_KEY_PATH=$key_path"
} >> "$GITHUB_ENV"
echo "Signing keychain ready."
