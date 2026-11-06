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
# 2026-11-06: the product is TAPEWORN by Elsewhere Audio. The CMake target keeps its internal name
# (LIMINAL), and the AU's manufacturer / subtype codes (Veil / Lmnl) - which also make the VST3's
# class ID - are unchanged on purpose: songs made with LIMINAL find TAPEWORN and load unchanged.
juce_add_plugin(LIMINAL
    COMPANY_NAME "Elsewhere Audio"
    BUNDLE_ID "com.elsewhereaudio.tapeworn"
    PLUGIN_MANUFACTURER_CODE Veil
    PLUGIN_CODE Lmnl
    PRODUCT_NAME "TAPEWORN"
    PLUGIN_NAME "TAPEWORN"
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
    Source/DSP/ForgottenPiano.cpp Source/DSP/FadedSynth.cpp Source/DSP/GlassMemories.cpp Source/DSP/EmptyChoir.cpp Source/DSP/LostMusicBox.cpp Source/DSP/HollowDrone.cpp Source/DSP/Ambience.cpp Source/DSP/Environment.cpp Source/DSP/Signature.cpp Source/DSP/Effects.cpp Source/DSP/Characters.cpp Source/Presets.cpp Source/Export.cpp Source/DSP/Space.cpp Source/DSP/Convolver.cpp Source/DSP/Captures.cpp Source/DSP/Liminality.cpp Source/DSP/Library.cpp Source/Samples/SampleBank.cpp
    Source/UI/SceneArt.cpp Source/UI/PhotoScene.cpp Source/UI/Vhs.cpp Source/UI/InstrumentIcons.cpp)
target_include_directories(LIMINAL PUBLIC Source)
# Photographic scenes (Direction Update 2): light passes rendered offline by tools/scenes and
# committed in assets/scenes/<place>/, so building the plug-in never needs Blender.
file(GLOB LIMINAL_SCENE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes/*/*.jpg" "${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes/*/*.json")
if(LIMINAL_SCENE_FILES)
    juce_add_binary_data(LiminalScenes HEADER_NAME SceneData.h NAMESPACE SceneData SOURCES ${LIMINAL_SCENE_FILES})
    target_link_libraries(LIMINAL PRIVATE LiminalScenes)
    target_compile_definitions(LIMINAL PRIVATE LIMINAL_HAS_SCENES=1)
else()
    target_compile_definitions(LIMINAL PRIVATE LIMINAL_HAS_SCENES=0)
endif()
# The developer's effect captures (pedal-capture milestone): impulse responses and measured
# profiles made by tools/captures/prepare.py from the PRIVATE Samples repository (CI: SAMPLES_TOKEN).
# Never committed. Without them the rooms sound exactly as before.
set(LIMINAL_CAPTURES_DIR "${CMAKE_CURRENT_BINARY_DIR}/captures" CACHE PATH "Prepared effect captures (tools/captures/prepare.py)")
if(EXISTS "${LIMINAL_CAPTURES_DIR}/profiles.json")
    file(GLOB LIMINAL_CAPTURE_FILES "${LIMINAL_CAPTURES_DIR}/*.wav" "${LIMINAL_CAPTURES_DIR}/profiles.json")
    juce_add_binary_data(LiminalCaptures HEADER_NAME CaptureData.h NAMESPACE CaptureData SOURCES ${LIMINAL_CAPTURE_FILES})
    target_link_libraries(LIMINAL PRIVATE LiminalCaptures)
    target_compile_definitions(LIMINAL PUBLIC LIMINAL_HAS_CAPTURES=1)
    message(STATUS "TAPEWORN: effect captures embedded from ${LIMINAL_CAPTURES_DIR}")
else()
    target_compile_definitions(LIMINAL PUBLIC LIMINAL_HAS_CAPTURES=0)
    message(STATUS "TAPEWORN: no effect captures (${LIMINAL_CAPTURES_DIR}) - the rooms keep their previous sound")
endif()
# The developer's recordings (Milestone 8): notes prepared by tools/sample-prep (prepare.py +
# liminal.py) from the PRIVATE Samples repository (CI: SAMPLES_TOKEN, scripts/ci-fetch-samples.sh).
# Never committed. They ship inside every bundle (Contents/Resources/Samples) and load on a
# background thread; without them every instrument plays its synthesised stand-in.
set(LIMINAL_SAMPLES_DIR "${CMAKE_CURRENT_BINARY_DIR}/samples" CACHE PATH "Prepared recordings (tools/sample-prep)")
if(EXISTS "${LIMINAL_SAMPLES_DIR}/index.json")
    target_compile_definitions(LIMINAL PUBLIC LIMINAL_HAS_SAMPLES=1 LIMINAL_SAMPLES_BUILD_DIR="${LIMINAL_SAMPLES_DIR}")
    foreach(lm_format IN LISTS LIMINAL_FORMATS)
        set(lm_target LIMINAL_${lm_format})
        if(TARGET ${lm_target})
            if(lm_format STREQUAL "Standalone" AND NOT APPLE)
                set(lm_res "$<TARGET_FILE_DIR:${lm_target}>")
            else()
                set(lm_res "$<TARGET_FILE_DIR:${lm_target}>/../Resources")
            endif()
            add_custom_command(TARGET ${lm_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rm -rf "${lm_res}/Samples"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${LIMINAL_SAMPLES_DIR}" "${lm_res}/Samples"
                COMMAND ${CMAKE_COMMAND} -E rm -f "${lm_res}/Samples/report.json"
                VERBATIM)
        endif()
    endforeach()
    message(STATUS "TAPEWORN: recordings shipped from ${LIMINAL_SAMPLES_DIR}")
else()
    target_compile_definitions(LIMINAL PUBLIC LIMINAL_HAS_SAMPLES=0)
    message(STATUS "TAPEWORN: no recordings (${LIMINAL_SAMPLES_DIR}) - the instruments play their synthesised stand-ins")
endif()
target_compile_definitions(LIMINAL PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(LIMINAL PRIVATE juce::juce_audio_utils juce::juce_dsp
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
    target_compile_definitions(LiminalTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        LIMINAL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")   # (fixtures: __FILE__ is relative under ccache's base dir)
    add_test(NAME liminal_tests COMMAND LiminalTests)
endif()
