cmake_minimum_required(VERSION 3.22)

project(TROCKENMOOS_PLUGINS VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: statische Runtime, damit kein Visual-C++-Redistributable nötig ist
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: lokaler Pfad (-DJUCE_DIR=...) oder automatischer Download
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

# Gemeinsame Bausteine aller trockenmoos-Plugins
add_library(trockenmoos_common INTERFACE)
target_include_directories(trockenmoos_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common)

# tm_add_plugin(<Target> <Produktname> <4-Zeichen-Code> <IS_SYNTH TRUE/FALSE> <Version> <Quellen...>)
function(tm_add_plugin target product code is_synth version)
    if(is_synth)
        set(cats Instrument Synth)
    else()
        set(cats Fx Reverb Delay)
    endif()

    juce_add_plugin(${target}
        VERSION ${version}
        COMPANY_NAME "trockenmoos"
        COMPANY_WEBSITE "https://trockenmoos.com"
        COMPANY_EMAIL "chris@trockenmoos.com"
        PLUGIN_MANUFACTURER_CODE Trck
        PLUGIN_CODE ${code}
        PRODUCT_NAME "${product}"
        IS_SYNTH ${is_synth}
        NEEDS_MIDI_INPUT ${is_synth}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES ${cats}
        FORMATS VST3 Standalone)

    target_sources(${target} PRIVATE ${ARGN})

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target}
        PRIVATE
            trockenmoos_common
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

set(TM_PLUGINS moos hallstatt soleleitung dachstein)
foreach(p ${TM_PLUGINS})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/${p}/Source/PluginProcessor.cpp)
        add_subdirectory(plugins/${p})
    endif()
endforeach()

# Optional: Offline-Tests (rendern Audio, prüfen Stabilität, speichern Screenshots)
option(TM_BUILD_TESTS "Build offline render tests" OFF)
if(TM_BUILD_TESTS)
    function(tm_add_test target)
        juce_add_console_app(${target} PRODUCT_NAME "${target}")
        target_sources(${target} PRIVATE ${ARGN})
        target_compile_definitions(${target} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Test")
        target_link_libraries(${target} PRIVATE trockenmoos_common juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
                              PUBLIC juce::juce_recommended_config_flags)
    endfunction()

    foreach(p ${TM_PLUGINS})
        string(SUBSTRING ${p} 0 1 first)
        string(TOUPPER ${first} first)
        string(SUBSTRING ${p} 1 -1 rest)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/${first}${rest}Test.cpp)
            tm_add_test(${first}${rest}Test tests/${first}${rest}Test.cpp
                        plugins/${p}/Source/PluginProcessor.cpp plugins/${p}/Source/PluginEditor.cpp)
        endif()
    endforeach()
endif()
