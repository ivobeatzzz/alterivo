cmake_minimum_required(VERSION 3.22)

# Mac: binario universal (Apple Silicon + Intel)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Versión mínima de macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas")

project(LevelIvo VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(LEVELIVO_FORMATS VST3 AU)
else()
    set(LEVELIVO_FORMATS VST3)
endif()

juce_add_plugin(LevelIvo
    COMPANY_NAME                "IVO Beatz"
    BUNDLE_ID                   com.ivobeatz.levelivo
    PLUGIN_MANUFACTURER_CODE    Ivob
    PLUGIN_CODE                 Livo
    FORMATS                     ${LEVELIVO_FORMATS}
    PRODUCT_NAME                "levelIVO"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx Dynamics Restoration
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(LevelIvo PRIVATE
    Source/PluginProcessor.cpp
    Source/core/Params.cpp
    Source/core/Engine.cpp
    Source/analysis/Analysis.cpp
    Source/cleanup/Cleaner.cpp
    Source/leveling/Leveler.cpp
    Source/presets/Presets.cpp
    Source/ui/Widgets.cpp
    Source/ui/WaveView.cpp
    Source/ui/PluginEditor.cpp)
target_include_directories(LevelIvo PRIVATE Source)

juce_add_binary_data(LevelIvoFonts SOURCES
    fonts/Sora-Light.ttf
    fonts/Sora-SemiBold.ttf
    fonts/Sora-Bold.ttf
    fonts/Manrope-Bold.ttf
    fonts/Manrope-ExtraBold.ttf)
set_target_properties(LevelIvoFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(MSVC)
    target_compile_options(LevelIvo PRIVATE /utf-8)
endif()

target_compile_definitions(LevelIvo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LevelIvo
    PRIVATE
        LevelIvoFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
