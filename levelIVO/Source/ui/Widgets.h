// levelIVO — piezas de interfaz: tarjetas planas, conmutadores segmentados, deslizadores finos con valor,
// interruptores, casillas de valor arrastrables y botones. Todo vectorial (nítido a cualquier escala).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace levelivo::ui
{
struct Fonts
{
    Fonts();
    juce::Typeface::Ptr soraLight, soraSemi, soraBold, manBold, manX;
};
enum class F { soraLight, soraSemi, soraBold, man, manX };
juce::Font font (F face, float size, float tracking = 0.0f);
float textWidth (const juce::Font&, const juce::String&);
juce::String u8 (const char*);
juce::String helpFor (const juce::String& paramId);

void cachedShadow (juce::Graphics&, const juce::Path&, juce::Colour, int radius, juce::Point<int> offset);
void cardBg (juce::Graphics&, juce::Rectangle<float>, float radius = 16.0f);
void caps (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, juce::Colour = theme::dim, float size = 9.0f, float tracking = 0.22f);
void drawLogo (juce::Graphics&, juce::Rectangle<float>, juce::Colour inkC = theme::ink);
void greenDot (juce::Graphics&, juce::Point<float>, float r);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
};

// Conmutador segmentado: carril gris hundido, opción activa en blanco con el punto verde
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::StringArray names, float fontSize = 9.0f);
    void setSelected (int i) { if (i != selected) { selected = i; repaint(); } }
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::StringArray names;
    float preferredWidth() const;
private:
    juce::Rectangle<float> segmentBounds (int i) const;
    int selected = 0; float fs;
};

// Segmentado ligado a un parámetro de elección o bool (bool: 0 = segunda opción "off")
class ParamSeg : public Segmented, private juce::Timer
{
public:
    ParamSeg (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::StringArray names, juce::UndoManager*, bool invertBool = false);
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm; juce::UndoManager* um; bool isBool, invert;
};

// Interruptor pequeño (negro con bola lima = encendido)
class Switch : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Switch (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::String text = {});
    explicit Switch (juce::String text);   // sin parámetro (onChange)
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    std::function<void (bool)> onChange;
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm = nullptr;
    juce::String text; bool on = false;
};

// Deslizador horizontal fino con nombre y valor (barra verde, botón blanco con punto)
class HSlider : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    HSlider (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::String name, juce::UndoManager*);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    float nameWidth = 112.0f, valueWidth = 58.0f, detailWidth = 0.0f;
    juce::String detail;        // texto a la derecha (lo detectado)
    juce::Colour detailColour = theme::dim;
    bool enabledLook = true;
    juce::Rectangle<float> barArea() const;
private:
    void timerCallback() override;
    void setFromX (float x);
    juce::RangedAudioParameter* prm; juce::UndoManager* um;
    juce::String name; float shown = -1.0f;
};

// Casilla de valor (p. ej. "MÁX. SUBIDA +9,0 dB"): se arrastra arriba/abajo, doble clic = por defecto
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::String name, juce::UndoManager*, float stepPerPixel);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool soft = true;   // fondo gris claro
    bool big = false;
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm; juce::UndoManager* um;
    juce::String name; float step, startVal = 0, shown = -1e9f;
};

// Botón de texto: lima, oscuro, contorno (con punto rojo opcional) o gris claro
class Button : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { lime, dark, outline, light };
    Button (juce::String t, Style s) : text (std::move (t)), style (s) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (enabled && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    std::function<void()> onClick;
    juce::String text; Style style;
    bool redDot = false, blink = false, active = false, enabled = true;
    float fontSize = 9.5f;
private:
    bool hover = false;
};

// Medidor horizontal fino
class Meter : public juce::Component
{
public:
    explicit Meter (juce::String l, bool gr = false) : lbl (std::move (l)), isGr (gr) {}
    void setLevelDb (float db);
    void paint (juce::Graphics&) override;
private:
    juce::String lbl; bool isGr; float db = -100.0f, painted = 1000.0f;
};
} // namespace levelivo::ui
