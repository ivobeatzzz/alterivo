cmake_minimum_required(VERSION 3.22)

# Mac: binario universal (Apple Silicon + Intel)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Versión mínima de macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas")

project(ChainIvo VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(CHAINIVO_FORMATS VST3 AU)
else()
    set(CHAINIVO_FORMATS VST3)
endif()

# Pro Tools (AAX): sólo si se indica el AAX SDK de Avid (no se usa en la distribución gratuita)
set(CHAINIVO_AAX_SDK "" CACHE PATH "Ruta al AAX SDK de Avid (opcional)")
if(CHAINIVO_AAX_SDK AND EXISTS "${CHAINIVO_AAX_SDK}")
    juce_set_aax_sdk_path("${CHAINIVO_AAX_SDK}")
    list(APPEND CHAINIVO_FORMATS AAX)
endif()

juce_add_plugin(ChainIvo
    COMPANY_NAME                "IVO Beatz"
    BUNDLE_ID                   com.ivobeatz.chainivo
    PLUGIN_MANUFACTURER_CODE    Ivob
    PLUGIN_CODE                 Civo
    FORMATS                     ${CHAINIVO_FORMATS}
    PRODUCT_NAME                "chainIVO"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx Dynamics EQ Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AAX_CATEGORY                AAX_ePlugInCategory_Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(ChainIvo PRIVATE
    Source/PluginProcessor.cpp
    Source/core/Params.cpp
    Source/analysis/Analyzer.cpp
    Source/intelligence/Musical.cpp
    Source/presets/PresetLibrary.cpp
    Source/ui/Widgets.cpp
    Source/ui/PluginEditor.cpp)
target_include_directories(ChainIvo PRIVATE Source)

juce_add_binary_data(ChainIvoFonts SOURCES
    fonts/Sora-Light.ttf
    fonts/Sora-SemiBold.ttf
    fonts/Sora-Bold.ttf
    fonts/Manrope-Bold.ttf
    fonts/Manrope-ExtraBold.ttf)
set_target_properties(ChainIvoFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(MSVC)
    target_compile_options(ChainIvo PRIVATE /utf-8)
endif()

target_compile_definitions(ChainIvo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChainIvo
    PRIVATE
        ChainIvoFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
