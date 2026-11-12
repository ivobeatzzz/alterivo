// chainIVO — colores de la familia IVO (mismos tonos que alterIVO) y utilidades de dibujo con relieve.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace chainivo::theme
{
inline juce::Colour bg        { 0xffe9ede4 };
inline juce::Colour bgGlow    { 0xfff4f7ef };
inline juce::Colour surface   { 0xffffffff };
inline juce::Colour surfaceLo { 0xfff6f8f3 };
inline juce::Colour wellTop   { 0xffe3e8dd };
inline juce::Colour wellBot   { 0xffebefe6 };
inline juce::Colour screenTop { 0xfffbfcf9 };
inline juce::Colour screenBot { 0xfff3f6ef };
inline juce::Colour ink       { 0xff15240f };
inline juce::Colour deep      { 0xff2c5e1c };
inline juce::Colour green     { 0xff3c8a22 };
inline juce::Colour lime      { 0xffb4e657 };
inline juce::Colour limeLight { 0xffd2f48a };
inline juce::Colour limeDark  { 0xffabdd4c };
inline juce::Colour limeDot   { 0xffa6e04a };
inline juce::Colour dim       { 0xff8d9787 };
inline juce::Colour dim2      { 0xff7f8a79 };
inline juce::Colour faint     { 0xffa9b2a3 };
inline juce::Colour body      { 0xff3e4a38 };
inline juce::Colour track     { 0xffd6dccf };
inline juce::Colour tick      { 0xffc2cabb };
inline juce::Colour leadLine  { 0xffaeb6a8 };
inline juce::Colour conflict  { 0xfff7d98a };
inline juce::Colour warn      { 0xfff2b93b };
inline juce::Colour red       { 0xffd9583b };
inline juce::Colour shadow    { 0xff284619 };
inline juce::Colour highlight { 0xe6ffffff };   // brillo del borde superior de lo elevado
inline juce::Colour knobTop   { 0xffffffff };
inline juce::Colour knobBot   { 0xffe1e7da };
inline juce::Colour onLime    { 0xff15240f };   // texto e indicadores sobre lima (siempre oscuro)
inline bool dark = false;

// Skins: sólo cambian la presentación, nunca el sonido
inline void applySkin (bool d)
{
    dark = d;
    auto c = [] (juce::uint32 v) { return juce::Colour (v); };
    if (d)
    {
        bg = c (0xff0d120b); bgGlow = c (0xff151d12); surface = c (0xff1c2518); surfaceLo = c (0xff161e13);
        wellTop = c (0xff0a0e08); wellBot = c (0xff10150d); screenTop = c (0xff141b11); screenBot = c (0xff10160d);
        ink = c (0xffeef3e6); deep = c (0xff9be04f); green = c (0xff6fbf3a); dim = c (0xff8a9580); dim2 = c (0xff9aa391);
        faint = c (0xff5d6a54); body = c (0xffc8d0c0); track = c (0xff2b3524); tick = c (0xff3d4836); leadLine = c (0xff6d7866);
        shadow = c (0xff000000); highlight = c (0x14ffffff); knobTop = c (0xff2c3826); knobBot = c (0xff141b11);
    }
    else
    {
        bg = c (0xffe9ede4); bgGlow = c (0xfff4f7ef); surface = c (0xffffffff); surfaceLo = c (0xfff6f8f3);
        wellTop = c (0xffe3e8dd); wellBot = c (0xffebefe6); screenTop = c (0xfffbfcf9); screenBot = c (0xfff3f6ef);
        ink = c (0xff15240f); deep = c (0xff2c5e1c); green = c (0xff3c8a22); dim = c (0xff8d9787); dim2 = c (0xff7f8a79);
        faint = c (0xffa9b2a3); body = c (0xff3e4a38); track = c (0xffd6dccf); tick = c (0xffc2cabb); leadLine = c (0xffaeb6a8);
        shadow = c (0xff284619); highlight = c (0xe6ffffff); knobTop = c (0xffffffff); knobBot = c (0xffe1e7da);
    }
}
} // namespace chainivo::theme
