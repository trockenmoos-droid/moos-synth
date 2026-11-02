cmake_minimum_required(VERSION 3.22)

project(MOOS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: statische Runtime, damit kein Visual-C++-Redistributable nötig ist
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: entweder lokaler Pfad (-DJUCE_DIR=...) oder automatischer Download
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MOOS
    COMPANY_NAME "trockenmoos"
    COMPANY_EMAIL "chris@trockenmoos.com"
    PLUGIN_MANUFACTURER_CODE Trck
    PLUGIN_CODE Moos
    PRODUCT_NAME "MOOS"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone)

target_sources(MOOS PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(MOOS PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MOOS
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: Offline-Render-Test (schreibt eine WAV-Datei), nur für Entwicklung
option(MOOS_BUILD_RENDER_TEST "Build offline render test" OFF)
if(MOOS_BUILD_RENDER_TEST)
    juce_add_console_app(MoosRender PRODUCT_NAME "MoosRender")
    target_sources(MoosRender PRIVATE Tests/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(MoosRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="MOOS")
    target_link_libraries(MoosRender PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
                          PUBLIC juce::juce_recommended_config_flags)
endif()
