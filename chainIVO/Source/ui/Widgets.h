// chainIVO — piezas de la interfaz con el lenguaje visual IVO: superficies elevadas, carriles hundidos,
// mandos blancos con el punto del logo. Todas dibujadas en vectorial (se ven nítidas a cualquier tamaño).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace chainivo::ui
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
void raised (juce::Graphics&, juce::Rectangle<float>, float radius, float lift = 1.0f);
void well (juce::Graphics&, juce::Rectangle<float>, float radius);
void screen (juce::Graphics&, juce::Rectangle<float>, float radius);
void limeFill (juce::Graphics&, juce::Rectangle<float>, float radius);
void logoDot (juce::Graphics&, juce::Point<float> c, float r, bool on);
void label (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, juce::Colour = theme::dim, float size = 10.0f);
void drawLogo (juce::Graphics&, juce::Rectangle<float>);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

// Conmutador segmentado: carril hundido y pastilla elevada en la opción activa
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::StringArray names, bool dotOnActive = true, float fontSize = 11.0f);
    void setSelected (int i) { if (i != selected) { selected = i; repaint(); } }
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::StringArray names;
private:
    int selected = 0; bool dot; float fs;
};

// Interruptor con nombre
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Toggle (juce::String name);
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    bool isOn() const { return on; }
    std::function<void (bool)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { on = ! on; repaint(); if (onChange) onChange (on); }
private:
    juce::String text; bool on = false;
};

// Interruptor ligado a un parámetro bool
class ParamToggle : public Toggle, private juce::Timer
{
public:
    ParamToggle (juce::String name, juce::AudioProcessorValueTreeState&, const juce::String& id);
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm;
};

// Mando con nombre arriba y valor abajo, ligado a un parámetro
class Knob : public juce::Component
{
public:
    Knob (juce::String name, juce::AudioProcessorValueTreeState&, const juce::String& id, juce::UndoManager*);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    void setRadius (float r) { radius = r; resized(); }
    float radius = 28.0f;
private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Selector de opciones ligado a un parámetro de elección (segmentado pequeño)
class ParamChoice : public Segmented, private juce::Timer
{
public:
    ParamChoice (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::UndoManager*);
private:
    void timerCallback() override;
    juce::RangedAudioParameter* prm; juce::UndoManager* um;
};

// Botón pequeño redondo con un símbolo dibujado (deshacer, rehacer)
class IconButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Icon { undo, redo, star, starOn, refresh };
    IconButton (Icon i) : icon (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void setIcon (Icon i) { icon = i; repaint(); }
private:
    Icon icon;
};

// Botón de texto con relieve (blanco, lima u oscuro)
class PillButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { white, lime, dark };
    PillButton (juce::String t, Style s = white) : text (std::move (t)), style (s) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void setText (const juce::String& t) { text = t; repaint(); }
    void setStyle (Style s) { style = s; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
private:
    juce::String text; Style style;
};

// Medidor horizontal fino
class Meter : public juce::Component
{
public:
    explicit Meter (juce::String l) : lbl (std::move (l)) {}
    void setLevel (float linear);
    void paint (juce::Graphics&) override;
private:
    juce::String lbl; float db = -100.0f, hold = -100.0f; int holdFrames = 0;
};
} // namespace chainivo::ui
