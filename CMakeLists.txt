cmake_minimum_required(VERSION 3.22)

# Mac: binario universal (Apple Silicon + Intel). En Windows se ignora.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Versión mínima de macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas")

project(AlterIvo VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE se descarga automáticamente la primera vez
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# La instalación la hacen los instaladores (packaging/), no la compilación
set(ALTERIVO_COPY FALSE)
if(APPLE)
    set(ALTERIVO_FORMATS VST3 AU)
else()
    set(ALTERIVO_FORMATS VST3)
endif()

juce_add_plugin(AlterIvo
    COMPANY_NAME                "IVO Beatz"
    BUNDLE_ID                   com.ivobeatz.alterivo
    PLUGIN_MANUFACTURER_CODE    Ivob
    PLUGIN_CODE                 Aivo
    FORMATS                     ${ALTERIVO_FORMATS}
    PRODUCT_NAME                "alterIVO"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx "Pitch Shift"
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD     ${ALTERIVO_COPY})

target_sources(AlterIvo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(AlterIvo PRIVATE Source)

# Fuentes de la marca (Sora y Manrope, licencia OFL) incrustadas en el plugin
juce_add_binary_data(AlterIvoFonts SOURCES
    fonts/Sora-Light.ttf
    fonts/Sora-SemiBold.ttf
    fonts/Sora-Bold.ttf
    fonts/Manrope-Bold.ttf
    fonts/Manrope-ExtraBold.ttf)
set_target_properties(AlterIvoFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Los nombres de presets llevan tildes: compilar el código como UTF-8 en Windows
if(MSVC)
    target_compile_options(AlterIvo PRIVATE /utf-8)
endif()

target_compile_definitions(AlterIvo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AlterIvo
    PRIVATE
        AlterIvoFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
