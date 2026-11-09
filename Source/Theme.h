// alterIVO — colores de la interfaz (marca IVO Beatz, verde oliva #2E3720 de ivobeatz.com).
// Formato 0xAARRGGBB: AA = ff, luego el hex.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace alterivo::theme
{
inline const juce::Colour frame      { 0xff161b0f };  // borde exterior
inline const juce::Colour faceTop    { 0xff36412a };  // panel (arriba)
inline const juce::Colour faceBottom { 0xff262e1b };  // panel (abajo)
inline const juce::Colour border     { 0xff56633f };  // líneas y contornos
inline const juce::Colour window     { 0xff141810 };  // pantalla y huecos
inline const juce::Colour brand      { 0xff2e3720 };  // verde IVO (indicador de los mandos)

inline const juce::Colour cream      { 0xffe9e2c8 };  // acento
inline const juce::Colour creamLight { 0xfff6f1df };  // brillo del mando
inline const juce::Colour creamMid   { 0xffd9d0b0 };
inline const juce::Colour creamDark  { 0xffb3aa88 };  // sombra del mando
inline const juce::Colour creamRing  { 0xffbfb594 };

inline const juce::Colour textMain   { 0xfff3efe2 };
inline const juce::Colour textDim    { 0xffa3aa8f };
} // namespace alterivo::theme
