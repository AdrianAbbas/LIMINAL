# Releasing VACANCY and VACANCY FX (macOS signing and notarisation)

VACANCY (the instrument) and VACANCY FX (the effect) are signed with the Developer ID of
**LJP Records Inc.** (team ID **YYZP4W7G7D**, brand Windward Audio), notarised by Apple and shipped
as a signed installer package, **VACANCY.pkg**. The zip with `install-mac.sh` stays available as a
secondary download. All of it happens in CI (`.github/workflows/build.yml`, job `macos`, on `macos-15`).

## What CI does

| Step | Script | When |
| --- | --- | --- |
| Decide whether the run signs (event, fork, all seven secrets present) | inline in the workflow | always |
| Temporary keychain with both certificates and Apple's Developer ID G2 intermediate; API key file | `scripts/ci-signing-setup.sh` | signing runs |
| Sign `VACANCY.component`, `VACANCY.vst3`, `VACANCY FX.component`, `VACANCY FX.vst3` inside out (nested binaries first, then the bundle): Developer ID Application, hardened runtime, secure timestamp, no entitlements | `scripts/sign-mac.sh` | signing runs |
| Ad-hoc signing of whatever is not Developer ID signed, zip `VACANCY-macOS-Universal.zip` | `scripts/package-mac.sh` | always |
| Notarise the zip; staple the tickets to the four bundles; re-package the zip with the stapled bundles | `scripts/notarize-mac.sh` | signing runs |
| Build `VACANCY.pkg` with `pkgbuild` + `productbuild`, signed with Developer ID Installer | `scripts/build-pkg.sh` | always (unsigned without signing) |
| Notarise `VACANCY.pkg`, staple and validate the ticket | `scripts/notarize-mac.sh --staple` | signing runs |
| `codesign --verify --deep --strict`, `spctl --assess` for the bundles (`--type open`) and the installer (`--type install`): each must say `accepted` and `source=Notarized Developer ID` | `scripts/ci-verify-signing.sh` | signing runs |
| Zip path: quarantined download, `install-mac.sh --ci`, auval and AU harnesses (unchanged) | `scripts/ci-user-install-test.sh` | always |
| Package path: Gatekeeper on the quarantined `.pkg`, `sudo installer -pkg`, receipts, old copies removed, registry, codesign / Gatekeeper on the installed copies, auval and AU harnesses arm64 + x86_64 on the installed copies | `scripts/ci-pkg-install-test.sh` | always (Gatekeeper parts only when notarised) |
| Delete the keychain and the key file, restore the keychain search list | `scripts/ci-signing-cleanup.sh` | always, last step |

Artifacts: `VACANCY-dev-macOS` (the developer zip with the bundles and install scripts, every run,
for testing only) and `VACANCY-1.0-macOS` (the customer download, to the
[installer standard](INSTALLER_STANDARD.md): only `VACANCY Installer.pkg` and `VACANCY - Read Me.pdf`,
only signed and notarised runs; tagged releases publish it as `VACANCY-1.0-macOS.zip`). Notarisation logs are in the
`VACANCY-macOS-diagnostics` artifact under `notary/`; on a rejection the log is also printed in the step.

### When it signs

Only **pushes to `main`** and **manual runs** (Actions > Native build > Run workflow, which only people
with write access can start) of this repository, never pull requests and never forks. Every other run,
and any run where one of the secrets is missing, is the ad-hoc build: a notice titled "Ad-hoc build"
says why, the bundles are signed ad hoc exactly as before, `VACANCY.pkg` is built unsigned so the
installer is still tested, and it is not uploaded. CI never fails because the secrets are absent.

### Releases

Tag pushes (`v*`) are not signing runs. To publish a signed release, push the tag, then start
**Run workflow** on that tag: the run signs and notarises, and the `release` job (which runs for
`refs/tags/v*`) attaches `VACANCY-1.0-macOS.zip` (`VACANCY Installer.pkg` and the Read Me) to the GitHub Release.

## Secrets (Settings > Secrets and variables > Actions)

| Secret | Contents |
| --- | --- |
| `MACOS_APP_CERT_P12` | base64 of the **Developer ID Application** certificate and its private key (.p12) |
| `MACOS_APP_CERT_PASSWORD` | password of that .p12 |
| `MACOS_INSTALLER_CERT_P12` | base64 of the **Developer ID Installer** certificate and its private key (.p12) |
| `MACOS_INSTALLER_CERT_PASSWORD` | password of that .p12 |
| `ASC_KEY_ID` | App Store Connect API key ID (10 characters) |
| `ASC_ISSUER_ID` | App Store Connect issuer ID (a UUID) |
| `ASC_KEY_P8` | base64 of the API key file `AuthKey_<KEYID>.p8` |

Secrets are passed only to the steps that use them, as environment variables, and never echoed; the
.p12 files are deleted right after import, the keychain password is random and masked. The team ID is
not a secret and is set in the workflow (`APPLE_TEAM_ID`). The setup script finds the identities by
team ID and fails clearly if a .p12 holds a different team's certificate.

Encode files for a secret without line breaks:

```bash
base64 -i DeveloperIDApplication.p12 | pbcopy
```

## Entitlements

None. A plug-in is a library loaded into its host's process (GarageBand, Logic Pro, a DAW, or Apple's
AUHostingService for out-of-process Audio Units): the host's entitlements apply and a library's own
are ignored. VACANCY and VACANCY FX need no JIT, no unsigned executable memory, no library-validation
exception and no microphone, network or file access of their own. `scripts/sign-mac.sh` fails if a
bundle ends up carrying entitlements. (The standalone `VACANCY.app`, built for development and not
shipped, stays ad-hoc signed.)

## The installer package

Pages, wording, background, licence and Read Me follow the Windward Audio
[installer standard](INSTALLER_STANDARD.md). `VACANCY.pkg` (shipped as `VACANCY Installer.pkg`) installs for all users:

- `/Library/Audio/Plug-Ins/Components/VACANCY.component` and `VACANCY FX.component` (choice "Audio Units")
- `/Library/Audio/Plug-Ins/VST3/VACANCY.vst3` and `VACANCY FX.vst3` (choice "VST3")

The recordings ship inside the instrument's bundles (`Contents/Resources/Samples`); the factory
presets of both plug-ins are compiled into their binaries, so they install with the bundles. Users'
own presets (`~/Library/Windward Audio/...`) are never touched.

Before installing, the preinstall scripts (`packaging/pkg/remove-copies.sh`) remove, system-wide and
in every user's `~/Library`:

- LIMINAL, TAPEWORN and TAPEWORN FX `.component` / `.vst3`, by name and by their old bundle IDs
  (`audio.veil.liminal`, `com.elsewhereaudio.tapeworn`, `com.windwardaudio.tapewornfx`)
- per-user copies of VACANCY / VACANCY FX (e.g. from the zip and `install-mac.sh`) and renamed copies
  with their bundle IDs, which would otherwise be listed twice by hosts

The bundles are marked non-relocatable, so the installer always writes to `/Library`. The AU
postinstall restarts `AudioComponentRegistrar`. Everything the scripts do is logged to
`/var/log/install.log` (lines starting with `VACANCY installer:`).

To uninstall a package installation:

```bash
sudo rm -rf "/Library/Audio/Plug-Ins/Components/VACANCY.component" "/Library/Audio/Plug-Ins/Components/VACANCY FX.component" "/Library/Audio/Plug-Ins/VST3/VACANCY.vst3" "/Library/Audio/Plug-Ins/VST3/VACANCY FX.vst3"
```

```bash
sudo pkgutil --forget com.windwardaudio.pkg.vacancy.au; sudo pkgutil --forget com.windwardaudio.pkg.vacancy.vst3
```

## Renewing the certificates

The current certificates expire on **12 December 2027** (both). The setup step prints each expiry
date on every signing run and warns when fewer than 60 days remain. Code signed and notarised before expiry keeps working: the
secure timestamp proves when it was signed. Only new builds need the new certificate.

1. On a Mac, in **Keychain Access > Certificate Assistant > Request a Certificate From a Certificate
   Authority**, create a certificate signing request (saved to disk). Do this twice if you renew both.
2. As the Account Holder of LJP Records Inc., go to
   [developer.apple.com/account/resources/certificates](https://developer.apple.com/account/resources/certificates),
   click **+**, choose **Developer ID Application** (G2 Sub-CA), upload the request and download the
   certificate; repeat for **Developer ID Installer**. (Apple limits how many Developer ID
   certificates a team can have; revoke a certificate only if its key was compromised: revoking
   invalidates what it signed.)
3. Double-click each downloaded `.cer` so it joins its private key in the login keychain. In
   **Keychain Access > My Certificates**, select "Developer ID Application: LJP Records Inc.
   (YYZP4W7G7D)" (with its key), **File > Export Items**, format .p12, set a strong password. Same for
   "Developer ID Installer: LJP Records Inc. (YYZP4W7G7D)".
4. Replace the secrets `MACOS_APP_CERT_P12` / `MACOS_APP_CERT_PASSWORD` and
   `MACOS_INSTALLER_CERT_P12` / `MACOS_INSTALLER_CERT_PASSWORD` (base64 as above).
5. Start **Run workflow** on `main` and check that the run is green and the "Import the Developer ID
   certificates" step prints the new expiry date.

### Renewing or rotating the App Store Connect API key

API keys do not expire, but rotate one if it may have leaked or its owner leaves:

1. [appstoreconnect.apple.com/access/integrations/api](https://appstoreconnect.apple.com/access/integrations/api)
   > Team Keys > **+**, access **Developer** (enough for notarisation). Download `AuthKey_<KEYID>.p8`
   (only possible once).
2. Replace `ASC_KEY_ID`, `ASC_ISSUER_ID` (shown above the key list) and `ASC_KEY_P8`
   (`base64 -i AuthKey_<KEYID>.p8`).
3. Run the workflow, then revoke the old key in App Store Connect.

## Troubleshooting

- **"could not import the ... certificate (wrong password?)"**: the password secret does not match the
  .p12, or the secret is not base64 of the .p12. (A .p12 exported by OpenSSL 3 is re-packed
  automatically.)
- **"no valid 'Developer ID Application' identity of team YYZP4W7G7D"**: the .p12 holds another
  certificate, lacks its private key, or the certificate expired or was revoked.
- **Notarisation "Invalid"**: the step prints Apple's log; each issue names the file and the reason
  (typically a binary without hardened runtime or timestamp). The log is also in the diagnostics
  artifact (`notary/*.notary-log.json`).
- **Notarisation "unauthorized"**: `ASC_KEY_ID`, `ASC_ISSUER_ID` and `ASC_KEY_P8` do not belong
  together, or the key was revoked.
- Check a downloaded package on any Mac:

  ```bash
  spctl --assess --type install -vvv VACANCY.pkg
  ```

  ```bash
  pkgutil --check-signature VACANCY.pkg
  ```
