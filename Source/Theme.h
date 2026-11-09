// alterIVO — colores de la interfaz clara (marca IVO Beatz).
// Formato 0xAARRGGBB: AA = ff, luego el hex.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace alterivo::theme
{
// Fondo y tinta
inline const juce::Colour bg         { 0xffeef1e9 };   // fondo verdoso muy claro
inline const juce::Colour surface    { 0xffffffff };   // tarjetas, mandos, píldoras
inline const juce::Colour surfaceLow { 0xfff1f4ec };   // borde inferior de los mandos
inline const juce::Colour ink        { 0xff15210f };   // texto principal, chip y modo activo
inline const juce::Colour green      { 0xff2f6b1f };   // verde de la marca (IVO del nombre, inicio de los arcos)

// Lima de acento
inline const juce::Colour lime       { 0xffb8ec5a };   // Link activado, halo
inline const juce::Colour limeBright { 0xff9be04f };   // final de los arcos
inline const juce::Colour limeDot    { 0xffa8e65a };   // centro del indicador de los mandos
inline const juce::Colour limeLine   { 0xff7cc23c };   // líneas del Link, punto de la firma
inline const juce::Colour limeDeep   { 0xff6fb534 };   // contorno de formantes
inline const juce::Colour meterGreen { 0xff5fae2f };
inline const juce::Colour chipText   { 0xffc9f27f };   // texto lima sobre tinta
inline const juce::Colour chipDim    { 0xff7f9a6a };

// Neutros
inline const juce::Colour textDim    { 0xff8a9580 };   // etiquetas
inline const juce::Colour textFaint  { 0xffa3ad99 };   // rótulos pequeños de la pantalla
inline const juce::Colour arrows     { 0xff9aa391 };
inline const juce::Colour track      { 0xffdde3d5 };   // pistas y segmentos apagados
inline const juce::Colour traceIn    { 0xffc3cbb9 };   // voz original en la pantalla
inline const juce::Colour shadow     { 0xff25450f };   // color de las sombras (con alfa)

inline const juce::Colour warn       { 0xfff2b93b };
inline const juce::Colour clip       { 0xffff6a4a };
} // namespace alterivo::theme
