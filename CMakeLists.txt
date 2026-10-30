cmake_minimum_required(VERSION 3.22)
# macOS defaults: one universal binary that loads on Apple Silicon and Intel Macs
# back to 10.15. These must be set before project(); override on the command line.
# (Build/CI pipeline adapted from AdrianAbbas/Worldbuilder, see docs/DEVLOG.md.)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()
project(LIMINAL VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(LIMINAL_BUILD_TESTS "Build the offline processor tests" ON)
set(JUCE_PATH "" CACHE PATH "Existing JUCE 8.0.16 source (otherwise fetched)")
if(JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ea77444750de45e036e0279bb0291144bf8e3688 # JUCE 8.0.16
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()
set(LIMINAL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LIMINAL_FORMATS AU)
endif()
juce_add_plugin(LIMINAL
    COMPANY_NAME "Veil Audio"
    BUNDLE_ID "audio.veil.liminal"
    PLUGIN_MANUFACTURER_CODE Veil
    PLUGIN_CODE Lmnl
    PRODUCT_NAME "LIMINAL"
    DESCRIPTION "Cinematic Environmental Instrument"
    FORMATS ${LIMINAL_FORMATS}
    # An instrument: GarageBand and Logic only reliably host generators this way.
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    # Lets sandboxed hosts (GarageBand) load it in-process. It must still work
    # out-of-process, which the AU host harness checks.
    AU_SANDBOX_SAFE TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(LIMINAL PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Parameters.cpp
    Source/DSP/ForgottenPiano.cpp Source/DSP/FadedSynth.cpp Source/DSP/GlassMemories.cpp Source/DSP/EmptyChoir.cpp Source/DSP/LostMusicBox.cpp Source/DSP/HollowDrone.cpp Source/DSP/Ambience.cpp Source/DSP/Environment.cpp Source/DSP/Effects.cpp Source/DSP/Space.cpp
    Source/UI/SceneArt.cpp)
target_include_directories(LIMINAL PUBLIC Source)
target_compile_definitions(LIMINAL PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(LIMINAL PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
if(LIMINAL_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(LiminalTests PRODUCT_NAME "LIMINAL Tests")
    # The linked plugin target exports the same setting using its format macro.
    get_target_property(lm_test_definitions LiminalTests COMPILE_DEFINITIONS)
    list(REMOVE_ITEM lm_test_definitions JUCE_STANDALONE_APPLICATION=1)
    set_target_properties(LiminalTests PROPERTIES COMPILE_DEFINITIONS "${lm_test_definitions}")
    target_sources(LiminalTests PRIVATE Tests/LiminalTests.cpp)
    target_link_libraries(LiminalTests PRIVATE LIMINAL juce::juce_audio_utils)
    target_compile_definitions(LiminalTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    add_test(NAME liminal_tests COMMAND LiminalTests)
endif()
