cmake_minimum_required(VERSION 3.22)
project(VegaChords VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VEGA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VEGA_FORMATS AU)
endif()

juce_add_plugin(VegaChords
    COMPANY_NAME "Vega Productions"
    BUNDLE_ID com.vegaproductions.chords
    PLUGIN_MANUFACTURER_CODE Vgap
    PLUGIN_CODE Vch9
    FORMATS ${VEGA_FORMATS}
    PRODUCT_NAME "Vega Productions Chords"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VegaChords PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(VegaChords PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VegaChords
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(VEGA_TESTS "Build offline test" OFF)
if(VEGA_TESTS)
    juce_add_console_app(VegaChordsTest PRODUCT_NAME "VegaChordsTest")
    target_sources(VegaChordsTest PRIVATE tests/ChordsTest.cpp)
    target_include_directories(VegaChordsTest PRIVATE $<TARGET_PROPERTY:VegaChords,INCLUDE_DIRECTORIES>)
    target_compile_definitions(VegaChordsTest PRIVATE $<TARGET_PROPERTY:VegaChords,COMPILE_DEFINITIONS>)
    target_link_libraries(VegaChordsTest PRIVATE VegaChords)
endif()
