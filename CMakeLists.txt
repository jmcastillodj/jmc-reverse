cmake_minimum_required(VERSION 3.22)

# Windows: link the C/C++ runtime statically so friends do not need the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "")
    project(JMCReverse VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(JMCReverse VERSION 1.0.0 LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JMCReverse
    PRODUCT_NAME "JMC REVERSE"
    COMPANY_NAME "JMC"
    PLUGIN_MANUFACTURER_CODE JmcA
    PLUGIN_CODE Jrev
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb)

target_sources(JMCReverse PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/audio/AudioFileLoader.cpp
    Source/audio/WavExport.cpp
    Source/Presets.cpp
    Source/ui/WaveformView.cpp
    Source/ui/LookAndFeel.cpp
    Source/dsp/FdnReverb.cpp
    Source/dsp/ReverseRenderer.cpp
    Source/dsp/Sibilance.cpp)

# Hide every symbol except the three VST3 entry points. Without this the bundle exports ~8000 JUCE/FLAC
# symbols, which collide with other JUCE plugins (JMC VERB, JMC AirLimit...) loaded into the same DAW
# process and can corrupt shared statics (the crash found by tests/HostCheck.cpp).
if(APPLE)
    foreach(tgt JMCReverse JMCReverse_VST3)
        if(TARGET ${tgt})
            set_target_properties(${tgt} PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden
                OBJC_VISIBILITY_PRESET hidden OBJCXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
        endif()
    endforeach()
    if(TARGET JMCReverse_VST3)
        target_link_options(JMCReverse_VST3 PRIVATE "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/Source/vst3.exports")
    endif()
endif()

target_compile_definitions(JMCReverse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(JMCReverse PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---- Automated tests (console executable, no plugin host needed) ----
add_executable(JMCTests Tests/main.cpp Tests/EngineTests.cpp Tests/SibilanceTests.cpp Tests/EffectsTests.cpp Tests/CharacterTests.cpp Tests/ExportTests.cpp Source/audio/AudioFileLoader.cpp Source/audio/WavExport.cpp
    Source/dsp/FdnReverb.cpp Source/dsp/ReverseRenderer.cpp Source/dsp/Sibilance.cpp)
target_include_directories(JMCTests PRIVATE Source)
target_compile_definitions(JMCTests PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(JMCTests PRIVATE juce::juce_audio_formats juce::juce_recommended_config_flags
                                      juce::juce_recommended_warning_flags)
enable_testing()
add_test(NAME dsp_and_io COMMAND JMCTests)

# Command-line renderer for auditioning the engine without a host
add_executable(jmc_render Tests/RenderCli.cpp Source/audio/AudioFileLoader.cpp
    Source/dsp/FdnReverb.cpp Source/dsp/ReverseRenderer.cpp Source/dsp/Sibilance.cpp)
target_include_directories(jmc_render PRIVATE Source)
target_compile_definitions(jmc_render PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(jmc_render PRIVATE juce::juce_audio_formats juce::juce_recommended_config_flags)

# Plugin-level tests: instantiate the real processor (state, undo, presets, parameter IDs).
add_executable(JMCPluginTests Tests/PluginTests.cpp)
target_include_directories(JMCPluginTests PRIVATE Source)
target_link_libraries(JMCPluginTests PRIVATE JMCReverse juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
add_test(NAME plugin_state COMMAND JMCPluginTests)

# Off-screen UI snapshot (for visual review without a host)
add_executable(jmc_snapshot Tests/UiSnapshot.cpp)
target_include_directories(jmc_snapshot PRIVATE Source)
target_link_libraries(jmc_snapshot PRIVATE JMCReverse juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(jmc_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

# Performance measurements
add_executable(jmc_bench Tests/Bench.cpp Source/dsp/FdnReverb.cpp Source/dsp/ReverseRenderer.cpp Source/dsp/Sibilance.cpp)
target_include_directories(jmc_bench PRIVATE Source)
target_compile_features(jmc_bench PRIVATE cxx_std_20)

# Loads the finished .vst3 bundle through JUCE's VST3 host (what a DAW does)
add_executable(jmc_hostcheck Tests/HostCheck.cpp)
target_compile_definitions(jmc_hostcheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(jmc_hostcheck PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats juce::juce_recommended_config_flags)
add_dependencies(jmc_hostcheck JMCReverse_VST3)

# Parameter table generator for docs/PARAMETERS.md
add_executable(jmc_params Tests/ParamDoc.cpp)
target_include_directories(jmc_params PRIVATE Source)
target_link_libraries(jmc_params PRIVATE JMCReverse juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
