// alterIVO — colores de la interfaz (marca IVO Beatz).
// Formato 0xAARRGGBB: AA = ff, luego el hex.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace alterivo::theme
{
// Fondo: casi negro con tinte verde, y el oliva de la marca (#2E3720) como halo
inline const juce::Colour bgDeep     { 0xff080b06 };
inline const juce::Colour bgTop      { 0xff10160c };
inline const juce::Colour brandOlive { 0xff2e3720 };

// Cristal (paneles y botones)
inline const juce::Colour glass      { 0x14ffffff };
inline const juce::Colour glassEdge  { 0x22ffffff };
inline const juce::Colour glassDark  { 0xcc06080a };

// Verde luminoso de acento (degradado)
inline const juce::Colour accent     { 0xffb6ec5a };
inline const juce::Colour accentDeep { 0xff6fb534 };
inline const juce::Colour accentText { 0xff0b1007 };

inline const juce::Colour track      { 0xff1f2919 };
inline const juce::Colour textMain   { 0xffeef3e6 };
inline const juce::Colour textDim    { 0xff7f8c74 };
inline const juce::Colour textFaint  { 0xff4a5642 };
inline const juce::Colour warn       { 0xffffc24a };
inline const juce::Colour clip       { 0xffff5a4a };
} // namespace alterivo::theme
