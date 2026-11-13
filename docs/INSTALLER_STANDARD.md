# Windward Audio installer standard

One installer for every Windward Audio product. VACANCY is the reference implementation; every other
product copies `installer/` unchanged and edits only `installer/product.json`.

## Files

| What | Name |
| --- | --- |
| Customer download | `<PRODUCT>-<version>-macOS.zip`, containing only the two files below |
| Installer | `<PRODUCT> Installer.pkg` (productbuild, Developer ID Installer, notarised, ticket stapled) |
| Read Me | `<PRODUCT> - Read Me.pdf` |
| Package identifiers | product `com.windwardaudio.pkg.<product>`; components `com.windwardaudio.pkg.<product>.au`, `.vst3` and `.prepare` (hidden, no payload) |
| Developer artifact | separate (`<PRODUCT>-dev-macOS`: bundles and install scripts, for testing only); never part of the customer download |

`<PRODUCT>` is the product name in capitals as shown to customers (`VACANCY`), `<product>` its lower-case
id (`vacancy`), `<version>` the marketing version (`1.0`). All three come from `installer/product.json`.

## Installer window

- Title: **`<PRODUCT> by Windward Audio`**.
- Pages, in this order:
  1. **Welcome** - "Welcome to `<PRODUCT>` by Windward Audio.", one line describing the product,
     "Please quit GarageBand, Logic Pro and any other music apps before continuing."
  2. **Read Me** - what gets installed (each plug-in, AU + VST3, with their sounds and presets, disk
     space), system requirements (macOS version, Apple Silicon and Intel), where to find it in hosts
     (under Windward Audio), support@windwardaudio.com.
  3. **License** - `installer/EULA.rtf`; the customer must click Agree. Until the real agreement is
     ready this is a placeholder clearly marked as such; replacing the file updates every product.
  4. **Destination Select** and **Installation Type** - standard. Installs to the system volume for
     all users; **Customize** lists "Audio Units" and "VST3", either can be unticked.
  5. **Summary** (conclusion) - "`<PRODUCT>` is installed. Open your music app and find it under
     Windward Audio." and support@windwardaudio.com.
- Visual style, identical on every product: `installer/background.png` (light mode) and
  `installer/background-dark.png` (dark mode, `background-darkAqua`), transparent except for the
  Windward Audio wordmark, bottom-left, readable in both modes. The product's accent colour appears
  only as the short rule above the wordmark (and the same rule under the Read Me's title). Pages use
  the system font, transparent backgrounds and follow light / dark mode (`installer/templates/style.css`).

## Read Me PDF

Same template for every product (`installer/templates/readme-pdf.txt.in`), A4, sections in this order:
what it is, system requirements, install, where to find it (GarageBand, Logic Pro, other music apps),
presets (location of the customer's own presets), updates, uninstall (exact paths of every installed
bundle, then the presets folders), support.

## Behaviour

- Before installing, a hidden preinstall step silently removes bundles from the product's earlier
  names (by name and by old bundle ID, system-wide and in every user's `~/Library`) and competing
  per-user copies of the product. Customer-facing text never mentions earlier names or scripts.
- The bundles are non-relocatable; the Audio Unit postinstall restarts `AudioComponentRegistrar`.
- Users' own presets are never touched.

## What is in `installer/`

| File | Purpose | Per product? |
| --- | --- | --- |
| `product.json` | name, id, marketing version, one-line description, accent colour, minimum macOS, plug-ins (`name`, `kind`: `instrument` or `effect`) | **yes - the only per-product file** |
| `templates/welcome.html.in`, `readme.html.in`, `conclusion.html.in`, `style.css` | installer pages | no |
| `templates/distribution.xml.in` | productbuild distribution: title, pages, backgrounds, licence, choices, identifiers | no |
| `templates/readme-pdf.txt.in` | Read Me PDF text | no |
| `templates/kinds.json` | per-kind wording (where an instrument / an effect is found in GarageBand and Logic Pro) | no |
| `EULA.rtf` | licence agreement (placeholder for now) | no |
| `background.png`, `background-dark.png` | backgrounds (wordmark; accent rule from `product.json`) | regenerated with `make-backgrounds.py` |
| `render.py` | renders the pages, the distribution and the Read Me PDF (Python 3, standard library only) | no |
| `make-backgrounds.py` | draws the two backgrounds (Pillow, Inter font); run once per product, commit the PNGs | no |

Templates use `@PRODUCT@`, `@ID@`, `@VERSION@`, `@PKG_VERSION@`, `@DESCRIPTION@`, `@MIN_MACOS@`,
`@MIN_MACOS_NAME@`, `@DISK_SPACE@`, `@PLUGIN_NAMES@`; `@EACH_PLUGIN@ ... @END@` repeats once per plug-in
with `@NAME@`, `@KIND@`, `@KIND_ARTICLE@`, `@GARAGEBAND_PATH@`, `@LOGIC_PATH@`, `@OTHER_PATH@`.

## Applying it to a product

1. Copy `installer/` and this document unchanged; fill in `installer/product.json`; run
   `python3 installer/make-backgrounds.py` and commit the two PNGs.
2. In the package build script (VACANCY: `scripts/build-pkg.sh`): build the component packages
   `au.pkg`, `vst3.pkg` and `prepare.pkg` with the identifiers above, then

   ```bash
   python3 installer/render.py --pkg-version "$version" --disk-space "$disk" \
       --resources "$work/resources" --distribution "$work/distribution.xml" \
       --readme-pdf "$(dirname "$out")/<PRODUCT> - Read Me.pdf"
   ```

   and `productbuild --distribution "$work/distribution.xml" --resources "$work/resources"
   --identifier com.windwardaudio.pkg.<product> ...`, signed with Developer ID Installer.
3. Notarise and staple the pkg; stage the customer download as `<PRODUCT>-<version>-macOS/` with
   `<PRODUCT> Installer.pkg` and `<PRODUCT> - Read Me.pdf` (VACANCY: `scripts/package-mac.sh --customer`).
4. CI checks the expanded package: the title, that the distribution uses every page, the licence and
   both backgrounds, and that no page mentions an earlier name or a script
   (VACANCY: `scripts/ci-pkg-install-test.sh`).
