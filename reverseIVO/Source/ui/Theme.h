// reverseIVO — colores. Familia IVO con personalidad propia: oscuro y digital, verdes suaves de la marca
// (nada de lima neón), crema IVO para lo principal y relieve en cada pieza.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace reverseivo::theme
{
inline const juce::Colour bgTop      { 0xff10140e };
inline const juce::Colour bgBot      { 0xff080a07 };
inline const juce::Colour glowA      { 0x8c30561f };   // insignia IVO (arriba a la izquierda)
inline const juce::Colour glowB      { 0xbf1b3412 };   // verde IVO (abajo a la derecha)
inline const juce::Colour text       { 0xffeef3e6 };
inline const juce::Colour soft       { 0x9effffff };   // texto secundario sobre oscuro
inline const juce::Colour dim        { 0xff7f8c74 };   // etiquetas (5,6:1 sobre el fondo)
inline const juce::Colour faint      { 0xff4a5642 };
inline const juce::Colour accent     { 0xffa9cf86 };   // verde salvia (IVO del nombre, arcos, activos)
inline const juce::Colour accentDeep { 0xff4f7f37 };
inline const juce::Colour accentHi   { 0xffc4dfa9 };
inline const juce::Colour cream      { 0xffeef3e2 };
inline const juce::Colour creamLo    { 0xffd9e1cc };
inline const juce::Colour ink        { 0xff0d120a };   // texto sobre crema
inline const juce::Colour logoDot    { 0xffa8dc58 };   // punto lima de la firma IVO
inline const juce::Colour rec        { 0xffff5b4d };
inline const juce::Colour raisedTop  { 0xff2e332b };
inline const juce::Colour raisedBot  { 0xff171a15 };
inline const juce::Colour wellTop    { 0xff0a0c09 };
inline const juce::Colour wellBot    { 0xff0e110d };
inline const juce::Colour knobTop    { 0xff454b40 };
inline const juce::Colour knobMid    { 0xff262a23 };
inline const juce::Colour knobBot    { 0xff161915 };
inline const juce::Colour faceTop    { 0xff343a31 };
inline const juce::Colour faceBot    { 0xff191c17 };
inline const juce::Colour waveDeep   { 0xff30561f };
inline const juce::Colour waveMid    { 0xff5d8c44 };
inline const juce::Colour waveHi     { 0xffc9e0b2 };
} // namespace reverseivo::theme
