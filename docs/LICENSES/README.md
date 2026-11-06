# Third-party material used by TAPEWORN builds

TAPEWORN's own code is MIT (`LICENSE` at the top of the repository); `LICENSES/AGPL-3.0.txt` is
JUCE's. This folder records the licences of sound material that builds fetch from outside this
repository. None of that material is committed here: CI fetches it from its official source.

## VSCO 2 Community Edition (Versilian Studios): woodwind samples

| | |
|---|---|
| What | "VS Chamber Orchestra: Community Edition" (VSCO 2: CE), an open-source orchestral sample library |
| By | Versilian Studios LLC / Sam Gossner, and Simon Dalzell (Ivy Audio); sample cutting by Elan Hickler (Soundemote) |
| Official source | https://github.com/sgossner/VSCO-2-CE (homepage: http://vis.versilstudios.net/vsco-community.html) |
| Version used | release tag **1.1.0**, commit `28092772094b2d9f1148d84cea97f4545b8c687d` (the `master` branch, `440300901dfe9275fd84e0b7763af1f8443ae62e` of 2020-08-04, has the identical `Woodwinds/` tree `c63d11e0` and the identical `LICENSE` blob `670154e3`) |
| Licence | **CC0 1.0 Universal** (public domain dedication): the full text, exactly as shipped in the library's `LICENSE` file, is `VSCO-2-CE_CC0-1.0.txt` in this folder (md5 `7bae63a234e80ee7c6427dce9fdba6cc`) |
| Checked | 2026-11-06, by reading the `LICENSE` file of the tag above (it is CC0 1.0, so the work went ahead) |
| What TAPEWORN uses | the soft (`_v1_`) takes of `Woodwinds/Flute/susNV` (sustained flute, no vibrato: 10 notes, C4-C7) and `Woodwinds/Clarinet/susLong` (sustained clarinet: 11 notes, D3-F6), fetched by CI (`scripts/ci-fetch-woodwinds.sh`: a sparse checkout of the tag above, cached; the LICENSE's md5 checked on every run), prepared by `tools/sample-prep/woodwinds.py` into the build folder, shipped inside the plug-in bundle only |

The library's `Readme.txt` adds two requests that are not licence conditions (CC0 waives all
rights): *"We ask that you do not sell the samples directly, and encourage you to keep any work
done on improving the sample set open and public"* and *"Please provide credit to Versilian
Studios/Sam Gossner, and/or Ivy Audio/Simon Dalzell where applicable, and link to the VSCO: CE
homepage."* TAPEWORN honours both: the samples are not sold on their own (they are heard only as a
treated layer inside the instrument), the preparation script is public in this repository, and
the credit is given here, in the README and in the DEVLOG. The product itself (its interface,
parameter names and preset names) carries no brand names; the prepared sets are called
`ww-flute` and `ww-clarinet`.
