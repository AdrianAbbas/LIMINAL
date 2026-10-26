# Building and installing

Use CMake 3.22+, C++17, and JUCE 8.0.6 (commit `51a8a6d7aeae7326956d747737ccf1575e61e209`). The build downloads JUCE from GitHub; runtime DSP has no network dependency. Use an existing JUCE checkout with `-DJUCE_PATH=...` for offline builds.

## macOS: Intel and Apple Silicon

Install Xcode and its command line tools, accept Apple's license, and install CMake. Use a current SDK supporting both architectures. macOS 11 is the minimum deployment target; older Intel macOS releases are not supported by this configuration.

```sh
cmake -S . -B build -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
lipo -info build/LIMINAL_artefacts/Release/VST3/LIMINAL.vst3/Contents/MacOS/LIMINAL
lipo -info build/LIMINAL_artefacts/Release/AU/LIMINAL.component/Contents/MacOS/LIMINAL
```

For a development-only native build, explicitly pass `-DCMAKE_OSX_ARCHITECTURES=arm64` or `x86_64`. Do not label that artifact universal. Copy the entire `LIMINAL.vst3` bundle to `~/Library/Audio/Plug-Ins/VST3/`, or `LIMINAL.component` to `~/Library/Audio/Plug-Ins/Components/`. Restart/rescan the host. Run `auval -v aumu Lm01 Lmnl` after AU installation. On Apple Silicon, test a native DAW and an Intel DAW under Rosetta, and test on physical Intel hardware as well. Verify live automation, project reopen, MIDI, offline bounce, transport, GUI resize and preset file dialogs in each host.

Builds here are unsigned development artifacts. Public distribution needs signing with your Developer ID, hardened runtime where applicable, notarization and stapling. Do not disable Gatekeeper. Obtain the proper JUCE distribution license and preserve third-party notices.

## Windows 10/11 x64

Install Visual Studio 2022 with Desktop development with C++, Windows SDK, Git and CMake.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

Copy `build/LIMINAL_artefacts/Release/VST3/LIMINAL.vst3` into `C:\Program Files\Common Files\VST3\` (may require elevation). Run the standalone from its artefact directory or use a VST3 host. ASIO is not bundled; the standalone uses available JUCE system audio backends.

## Linux development

Install `g++ cmake pkg-config libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev`. Optional headless editor checks use Xvfb. Browser and curl support are disabled; WebKit is not required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 3
ctest --test-dir build --output-on-failure
build/host_smoke_artefacts/Release/host_smoke "$PWD/build/LIMINAL_artefacts/Release/VST3/LIMINAL.vst3"
mkdir -p build/renders
build/dsp_tests build/renders
```

Copy the VST3 bundle to `~/.vst3/` for a local host. Headless rendering does not require an audio device. The standalone requires a desktop/display and an audio device for interactive playback. With DISPLAY set, the smoke host opens/reopens the editor on a desktop peer. On bare Xvfb use `xvfb-run -a python3 tests/headless.py build/host_smoke_artefacts/Release/host_smoke "$PWD/build/LIMINAL_artefacts/Release/VST3/LIMINAL.vst3"`; the helper supplies the standard window protocol atoms normally provided by a window manager. DSP renders produce three 10-second stereo PCM16 WAVs for listening, not automatic perceptual certification.

CI is configured for macOS universal, Windows x64, and Linux. A checked-in workflow is not evidence that its remote jobs ran.
