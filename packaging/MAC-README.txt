LIMINAL 0.1 — macOS universal preview

Compatible build target: Intel and Apple Silicon, macOS 11 or later.
No Xcode, compiler, CMake, or JUCE installation is needed to use this download.

INSTALL
Double-click LIMINAL-macOS-universal.pkg and follow the installer.
It installs:
  /Applications/LIMINAL.app
  /Library/Audio/Plug-Ins/Components/LIMINAL.component
  /Library/Audio/Plug-Ins/VST3/LIMINAL.vst3

Restart Logic, GarageBand, or your other DAW and rescan its plugins.
Use the Audio Unit in Logic/GarageBand, or VST3 in a compatible host.
LIMINAL is an instrument, so insert it on an instrument track.
Environment mode makes sound without MIDI when ACTIVE is enabled.

This is an unsigned, unnotarized development preview, not an Apple-approved
commercial installer. macOS may block it pending your explicit approval in
System Settings > Privacy & Security. Do not disable Gatekeeper.

The release also contains LIMINAL-macOS-universal.zip with the individual
plugin/app bundles, if you prefer manual installation.

LICENSING AND SOURCE
The combined JUCE-based binaries are distributed under GNU AGPL version 3.
See AGPL-3.0.txt. Original LIMINAL source also remains available under MIT.
The release's LIMINAL-corresponding-source.zip includes the exact LIMINAL
source, pinned JUCE source (including its dependency notices), and build files.
Source and releases: https://github.com/AdrianAbbas/LIMINAL

This is an early preview. Read docs/VALIDATION.md in the corresponding source
for remaining limitations. Automated build/tests do not certify artistic
sound quality or compatibility with every DAW.
