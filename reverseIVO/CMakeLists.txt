cmake_minimum_required(VERSION 3.22)

# Mac: binario universal (Apple Silicon + Intel)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Versión mínima de macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas")

project(ReverseIvo VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: se descarga la primera vez. Con -DREVERSEIVO_JUCE_DIR=/ruta/JUCE se usa una copia local.
set(REVERSEIVO_JUCE_DIR "" CACHE PATH "Copia local de JUCE (opcional)")
if(REVERSEIVO_JUCE_DIR AND EXISTS "${REVERSEIVO_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${REVERSEIVO_JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.17-security
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(REVERSEIVO_FORMATS VST3 AU)
else()
    set(REVERSEIVO_FORMATS VST3)
endif()
# Linux (sólo para compilar y probar en servidores): Standalone ayuda a revisar la interfaz
if(UNIX AND NOT APPLE)
    list(APPEND REVERSEIVO_FORMATS Standalone)
endif()

juce_add_plugin(ReverseIvo
    COMPANY_NAME                "IVO Beatz"
    BUNDLE_ID                   com.ivobeatz.reverseivo
    PLUGIN_MANUFACTURER_CODE    Ivob
    PLUGIN_CODE                 Rivo
    FORMATS                     ${REVERSEIVO_FORMATS}
    PRODUCT_NAME                "reverseIVO"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx Reverb Generator
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(ReverseIvo PRIVATE
    Source/PluginProcessor.cpp
    Source/core/Params.cpp
    Source/core/Engine.cpp
    Source/render/Renderer.cpp
    Source/presets/Presets.cpp
    Source/ui/Widgets.cpp
    Source/ui/Views.cpp
    Source/ui/PluginEditor.cpp)
target_include_directories(ReverseIvo PRIVATE Source)

# Fuentes de la marca IVO (Sora y Manrope, licencia OFL) incrustadas en el plugin
juce_add_binary_data(ReverseIvoFonts SOURCES
    fonts/Sora-Light.ttf
    fonts/Sora-SemiBold.ttf
    fonts/Sora-Bold.ttf
    fonts/Manrope-Bold.ttf
    fonts/Manrope-ExtraBold.ttf)
set_target_properties(ReverseIvoFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(MSVC)
    target_compile_options(ReverseIvo PRIVATE /utf-8)
endif()

target_compile_definitions(ReverseIvo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Compilación de prueba en Linux sin cabeceras de sistema extra (ALSA/Xrandr…)
option(REVERSEIVO_MINIMAL_LINUX "Compilar en Linux sin dependencias opcionales" OFF)
if(REVERSEIVO_MINIMAL_LINUX)
    target_compile_definitions(ReverseIvo PUBLIC JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0
                               JUCE_USE_XSHM=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(ReverseIvo
    PRIVATE
        ReverseIvoFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Pruebas del plugin completo (opcional): cmake -DREVERSEIVO_TESTS=ON
option(REVERSEIVO_TESTS "Compilar la prueba del plugin completo" OFF)
if(REVERSEIVO_TESTS)
    juce_add_console_app(ReverseIvoPluginTest PRODUCT_NAME "ReverseIvoPluginTest")
    target_sources(ReverseIvoPluginTest PRIVATE
        tests/PluginTest.cpp
        Source/PluginProcessor.cpp
        Source/core/Params.cpp
        Source/core/Engine.cpp
        Source/render/Renderer.cpp
        Source/presets/Presets.cpp
        Source/ui/Widgets.cpp
        Source/ui/Views.cpp
        Source/ui/PluginEditor.cpp)
    target_include_directories(ReverseIvoPluginTest PRIVATE Source tests)
    target_compile_definitions(ReverseIvoPluginTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ReverseIvoPluginTest PRIVATE ReverseIvoFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
