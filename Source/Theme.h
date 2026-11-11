// alterIVO — colores de la interfaz (marca IVO Beatz), con dos skins: LIGHT y DARK.
// Las skins sólo cambian la presentación: nunca el sonido ni los parámetros.
// Formato 0xAARRGGBB.
#pragma once

#include <juce_graphics/juce_graphics.h>
#include <atomic>

namespace alterivo::theme
{
// Fondo y tinta
inline juce::Colour bg         { 0xffeef1e9 };   // fondo de la ventana
inline juce::Colour surface    { 0xffffffff };   // tarjetas, mandos, píldoras
inline juce::Colour surfaceLow { 0xfff1f4ec };   // borde inferior de los mandos
inline juce::Colour cardBottom { 0xfffbfdf8 };   // degradado de la tarjeta central
inline juce::Colour ink        { 0xff15210f };   // texto principal y modo activo
inline juce::Colour green      { 0xff2f6b1f };   // IVO del nombre y nota grande
inline juce::Colour arcStart   { 0xff2f6b1f };   // inicio de arcos y trazos

// Lima de acento
inline juce::Colour lime       { 0xffb8ec5a };   // Link activado, halo
inline juce::Colour limeBright { 0xff9be04f };   // final de los arcos
inline juce::Colour limeDot    { 0xffa8e65a };   // centro del indicador de los mandos
inline juce::Colour limeLine   { 0xff7cc23c };   // líneas del Link, punto de la firma
inline juce::Colour limeDeep   { 0xff6fb534 };   // contorno de formantes
inline juce::Colour meterGreen { 0xff5fae2f };
inline juce::Colour presetDot  { 0xff8fd14a };
inline juce::Colour chipBg     { 0xff15210f };   // chip de la nota
inline juce::Colour chipText   { 0xffc9f27f };
inline juce::Colour chipDim    { 0xff7f9a6a };
inline juce::Colour onLime     { 0xff15210f };   // iconos sobre lima

// Neutros
inline juce::Colour textDim    { 0xff8a9580 };
inline juce::Colour textFaint  { 0xffa3ad99 };
inline juce::Colour arrows     { 0xff9aa391 };
inline juce::Colour track      { 0xffdde3d5 };
inline juce::Colour traceIn    { 0xffc3cbb9 };
inline juce::Colour shadow     { 0xff25450f };
inline float shadowStrength = 1.0f;              // multiplica el alfa de las sombras
inline float glowStrength   = 0.55f;             // halo lima detrás de la pantalla

inline juce::Colour warn       { 0xfff2b93b };
inline juce::Colour clip       { 0xffff6a4a };

// Cambia cada vez que se aplica una skin: las ventanas abiertas se redibujan
inline std::atomic<int> skinVersion { 0 };
inline bool darkSkin = false;

inline void applySkin (bool dark)
{
    darkSkin = dark;
    if (dark)
    {
        bg = juce::Colour (0xff0d120b);   surface = juce::Colour (0xff1b2317);  surfaceLow = juce::Colour (0xff12180f);
        cardBottom = juce::Colour (0xff171f13);
        ink = juce::Colour (0xffeef3e6);  green = juce::Colour (0xff9be04f);    arcStart = juce::Colour (0xff4f9a2a);
        lime = juce::Colour (0xffb8ec5a); limeBright = juce::Colour (0xffa8e65a); limeDot = juce::Colour (0xffa8e65a);
        limeLine = juce::Colour (0xff7cc23c); limeDeep = juce::Colour (0xff8fd14a); meterGreen = juce::Colour (0xff6fbf3a);
        presetDot = juce::Colour (0xff9be04f);
        chipBg = juce::Colour (0xff080b06); chipText = juce::Colour (0xffc9f27f); chipDim = juce::Colour (0xff6e8a5c);
        onLime = juce::Colour (0xff15210f);
        textDim = juce::Colour (0xff8a9580); textFaint = juce::Colour (0xff5d6a54); arrows = juce::Colour (0xff7a8771);
        track = juce::Colour (0xff2b3524); traceIn = juce::Colour (0xff4d5a44); shadow = juce::Colour (0xff000000);
        shadowStrength = 2.2f; glowStrength = 0.16f;
    }
    else
    {
        bg = juce::Colour (0xffeef1e9);   surface = juce::Colour (0xffffffff);  surfaceLow = juce::Colour (0xfff1f4ec);
        cardBottom = juce::Colour (0xfffbfdf8);
        ink = juce::Colour (0xff15210f);  green = juce::Colour (0xff2f6b1f);    arcStart = juce::Colour (0xff2f6b1f);
        lime = juce::Colour (0xffb8ec5a); limeBright = juce::Colour (0xff9be04f); limeDot = juce::Colour (0xffa8e65a);
        limeLine = juce::Colour (0xff7cc23c); limeDeep = juce::Colour (0xff6fb534); meterGreen = juce::Colour (0xff5fae2f);
        presetDot = juce::Colour (0xff8fd14a);
        chipBg = juce::Colour (0xff15210f); chipText = juce::Colour (0xffc9f27f); chipDim = juce::Colour (0xff7f9a6a);
        onLime = juce::Colour (0xff15210f);
        textDim = juce::Colour (0xff8a9580); textFaint = juce::Colour (0xffa3ad99); arrows = juce::Colour (0xff9aa391);
        track = juce::Colour (0xffdde3d5); traceIn = juce::Colour (0xffc3cbb9); shadow = juce::Colour (0xff25450f);
        shadowStrength = 1.0f; glowStrength = 0.55f;
    }
    ++skinVersion;
}
} // namespace alterivo::theme
