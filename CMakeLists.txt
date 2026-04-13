cmake_minimum_required(VERSION 3.25)
project(Tanghim VERSION 0.9.0)

# ── Version-consistency guard ─────────────────────────────────────────────────
# When the workflow builds a tagged release it passes -DTANGHIM_EXPECTED_VERSION=<version>
# (the git tag with the leading 'v' stripped). If the value doesn't match PROJECT_VERSION,
# the build fails early — preventing releases where the installer filename and the plugin's
# internal version disagree. Local builds that don't set this option skip the check.
if(DEFINED TANGHIM_EXPECTED_VERSION AND NOT TANGHIM_EXPECTED_VERSION STREQUAL "")
    if(NOT PROJECT_VERSION VERSION_EQUAL TANGHIM_EXPECTED_VERSION)
        message(FATAL_ERROR
            "Version mismatch: git tag says ${TANGHIM_EXPECTED_VERSION}, "
            "CMakeLists.txt says ${PROJECT_VERSION}. "
            "Bump the project() version or fix the tag.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── macOS deployment target ──────────────────────────────────────────────────
# Without this, CMake links against whatever SDK version the build machine
# happens to have (macOS-latest CI runners use the newest SDK). Symbols
# introduced in newer SDKs (e.g. MIDI 2.0 UMP in 15.0) then cause dlopen to
# fail with "symbol not found" on older macOS. 11.0 matches JUCE 8's minimum.
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# ── Build timestamp (regenerated on every build) ─────────────────────────────
set(TIMESTAMP_HEADER "${CMAKE_BINARY_DIR}/generated/BuildTimestamp.h")
add_custom_target(GenerateBuildTimestamp ALL
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE=${TIMESTAMP_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateTimestamp.cmake
    COMMENT "Generating build timestamp"
)

# ── Dependencies ──────────────────────────────────────────────────────────────
add_subdirectory(libs/JUCE)
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(Tanghim
    COMPANY_NAME            "KhyamAllami"
    BUNDLE_ID               "com.khyamallami.tanghim"
    PLUGIN_MANUFACTURER_CODE Khya
    PLUGIN_CODE             AmqT
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Tanghim"
    IS_SYNTH                TRUE     # Instrument category — allows placement on MIDI tracks without a synth before it
    IS_MIDI_EFFECT          FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEBVIEW2          FALSE
    VST3_CATEGORIES         "Instrument" "Tools"
    # No custom plist needed (WebView removed)
)

# CLAP format via clap-juce-extensions
clap_juce_extensions_plugin(
    TARGET      Tanghim
    CLAP_ID     "com.khyamallami.tanghim"
    CLAP_FEATURES "note-effect" "utility"
)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(Tanghim
    PRIVATE
        # MTS-ESP transmitter (included directly — no separate cmake target)
        libs/MTS-ESP/Master/libMTSMaster.cpp

        # Plugin entry points
        source/PluginProcessor.cpp

        # Native editor components
        source/editor/TanghimNativeEditor.cpp
        source/editor/NoteSliderComponent.cpp
        source/editor/NoteSliderBankComponent.cpp
        source/editor/ReferenceFreqControl.cpp
        source/editor/SearchablePopup.cpp
        source/editor/OutputBadges.cpp
        source/editor/RangeScrollerComponent.cpp
        source/editor/MaqamPresetBar.cpp
        source/editor/MaqamSelectorComponent.cpp

        # API layer
        source/api/DiArMaqArClient.cpp
        source/api/ApiDataCache.cpp
        source/api/ApiResponseParser.cpp
        source/api/DataUpdateChecker.cpp

        # Tuning engine
        source/engine/TuningEngine.cpp
        source/engine/MtsEspTransmitter.cpp
        source/engine/MpePitchBendProcessor.cpp
        source/engine/MonoPitchBendProcessor.cpp
        source/engine/MidiFileGenerator.cpp
)

target_include_directories(Tanghim
    PRIVATE
        source
        libs/MTS-ESP/Master
        ${CMAKE_BINARY_DIR}/generated
)

# Ensure timestamp is generated before compiling
add_dependencies(Tanghim GenerateBuildTimestamp)
set_source_files_properties(source/editor/TanghimNativeEditor.cpp PROPERTIES OBJECT_DEPENDS ${TIMESTAMP_HEADER})

# ── Linux: libcurl for HTTP client ────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBCURL REQUIRED libcurl)
    target_link_libraries(Tanghim PRIVATE ${LIBCURL_LIBRARIES})
endif()

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(Tanghim
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        PLUGIN_VERSION="${PROJECT_VERSION}"
)

# ── Linked libraries ──────────────────────────────────────────────────────────
target_link_libraries(Tanghim
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices    # MidiInput for direct MIDI preset triggering
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ── Receiver plugin target ────────────────────────────────────────────────────
# Lightweight MIDI effect that reads MTS-ESP tuning and applies pitch bend/MPE
juce_add_plugin(TanghimReceiver
    COMPANY_NAME            "KhyamAllami"
    BUNDLE_ID               "com.khyamallami.tanghimreceiver"
    PLUGIN_MANUFACTURER_CODE Khya
    PLUGIN_CODE             AmqR
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Tanghim Receiver"
    IS_SYNTH                FALSE
    IS_MIDI_EFFECT          TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Tools"
)

clap_juce_extensions_plugin(
    TARGET      TanghimReceiver
    CLAP_ID     "com.khyamallami.tanghimreceiver"
    CLAP_FEATURES "note-effect" "utility"
)

target_sources(TanghimReceiver
    PRIVATE
        libs/MTS-ESP/Client/libMTSClient.cpp
        source/receiver/ReceiverProcessor.cpp
        source/receiver/ReceiverEditor.cpp
        source/engine/MpePitchBendProcessor.cpp
        source/engine/MonoPitchBendProcessor.cpp
)

target_include_directories(TanghimReceiver
    PRIVATE
        source
        libs/MTS-ESP/Client
)

target_compile_definitions(TanghimReceiver
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        PLUGIN_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(TanghimReceiver
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ── Linux: link libcurl for TanghimReceiver (juce_core needs it) ─────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(TanghimReceiver PRIVATE ${LIBCURL_LIBRARIES})
endif()

# (WebView UI bundle removed — now using native JUCE editor)

# ── Tests ─────────────────────────────────────────────────────────────────────
# Each test file has its own main(), so each gets its own executable.
include(CTest)

# Shared sources and settings for all test targets
set(TEST_COMMON_SOURCES
    source/api/ApiResponseParser.cpp
    source/engine/TuningEngine.cpp
    source/engine/MtsEspTransmitter.cpp
    source/engine/MpePitchBendProcessor.cpp
    source/engine/MonoPitchBendProcessor.cpp
    libs/MTS-ESP/Master/libMTSMaster.cpp
)
set(TEST_COMMON_INCLUDES source libs/MTS-ESP/Master)
set(TEST_COMMON_LIBS juce::juce_core juce::juce_audio_basics)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND TEST_COMMON_LIBS ${LIBCURL_LIBRARIES})
endif()
set(TEST_COMMON_DEFS JUCE_STANDALONE_APPLICATION=0 JUCE_WEB_BROWSER=0)

foreach(TEST_NAME ApiResponseParserTest TuningEngineTest MidiProcessingTest)
    add_executable(${TEST_NAME}
        tests/${TEST_NAME}.cpp
        ${TEST_COMMON_SOURCES}
    )
    target_include_directories(${TEST_NAME} PRIVATE ${TEST_COMMON_INCLUDES})
    target_link_libraries(${TEST_NAME} PRIVATE ${TEST_COMMON_LIBS})
    target_compile_definitions(${TEST_NAME} PRIVATE ${TEST_COMMON_DEFS})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
