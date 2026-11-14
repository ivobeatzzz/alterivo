// reverseIVO — piezas de la interfaz. Todas vectoriales (nítidas a cualquier tamaño), con relieve real:
// luz arriba, sombra abajo, huecos hundidos. Las sombras difuminadas se cachean (no se recalculan al repintar).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace reverseivo::ui
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

// Superficies
void cachedShadow (juce::Graphics&, const juce::Path&, juce::Colour, int radius, juce::Point<int> offset);
void glass (juce::Graphics&, juce::Rectangle<float>, float radius);             // panel de cristal
void raised (juce::Graphics&, juce::Rectangle<float>, float radius, bool shadow = true);   // pieza elevada oscura
void well (juce::Graphics&, juce::Rectangle<float>, float radius);              // hueco hundido
void creamPill (juce::Graphics&, juce::Rectangle<float>, float radius);         // botón principal crema
void label (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, juce::Colour = theme::dim, float size = 11.0f, float tracking = 0.26f);
void drawLogo (juce::Graphics&, juce::Rectangle<float>);                         // firma IVO (trazado de logo/ivo-logo-simbolo.svg)
void drawWordmark (juce::Graphics&, juce::Point<float> topLeft, const juce::String& word, const juce::String& tagline);
void dot (juce::Graphics&, juce::Point<float> c, float r, juce::Colour);

enum class Icon { rec, import, play, stop, reset, left, right, download, ear, drag };
void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool, bool, bool, bool, bool, const juce::String&,
                            const juce::String&, const juce::Drawable*, const juce::Colour*) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowTitleFont() override;
};

// Conmutador: carril hundido, pastilla elevada con el punto en la opción activa
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::StringArray names, float fontSize = 11.0f);
    void setSelected (int i) { if (i != selected) { selected = i; repaint(); } }
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::StringArray names;
    bool plain = false;     // sin carril (texto suelto, como la fila de duración)
private:
    juce::Rectangle<float> segmentBounds (int i) const;
    int selected = 0; float fs;
};

class ParamChoice : public Segmented, private juce::Timer
{
public:
    ParamChoice (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::UndoManager*, float fontSize = 11.0f);
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm; juce::UndoManager* um;
};

// Interruptor (carril hundido + bola elevada)
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Toggle (juce::String name = {}) : text (std::move (name)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    bool isOn() const { return on; }
    std::function<void (bool)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { on = ! on; repaint(); if (onChange) onChange (on); }
    bool labelLeft = false;
private:
    juce::String text; bool on = false;
};

class ParamToggle : public Toggle, private juce::Timer
{
public:
    ParamToggle (juce::String name, juce::AudioProcessorValueTreeState&, const juce::String& id, juce::UndoManager*);
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm;
};

// Perilla: hueco hundido con el arco verde, cuerpo con relieve, marca crema. Valor y nombre debajo.
class Knob : public juce::Component
{
public:
    Knob (juce::String name, juce::AudioProcessorValueTreeState&, const juce::String& id, juce::UndoManager*, float diameter = 116.0f);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    float diameter;
    bool compact = false;    // valor y nombre más pequeños
private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Botón redondo elevado con icono
class RoundButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RoundButton (Icon i) : icon (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void setIcon (Icon i) { icon = i; repaint(); }
    void setActive (bool a) { if (a != active) { active = a; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { pressed = false; repaint(); if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
private:
    Icon icon; bool active = false, pressed = false;
};

// Botón de texto: crema (principal) u oscuro elevado
class PillButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { cream, dark };
    PillButton (juce::String t, Style s) : text (std::move (t)), style (s) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void setText (const juce::String& t) { text = t; repaint(); }
    void setPending (bool p) { if (p != pending) { pending = p; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { pressed = false; repaint(); if (! dragFired && getLocalBounds().contains (e.getPosition()) && onClick) onClick(); dragFired = false; }
    void mouseDrag (const juce::MouseEvent& e) override { if (onDragOut && ! dragFired && e.getDistanceFromDragStart() > 5) { dragFired = true; pressed = false; repaint(); onDragOut(); } }
    std::function<void()> onDragOut;   // arrastrar fuera del plugin (al DAW)
    juce::String text;
    Icon leading = Icon::drag; bool hasIcon = false;
    static constexpr int pad = 12;   // margen para la sombra
private:
    Style style; bool pressed = false, pending = false, dragFired = false;
};
} // namespace reverseivo::ui
