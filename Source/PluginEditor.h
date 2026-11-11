#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Theme.h"

#include <array>

// Fuentes de la marca incrustadas en el plugin (iguales en Mac y Windows)
struct BrandFonts
{
    BrandFonts();
    juce::Typeface::Ptr soraLight, soraSemi, soraBold, manropeBold, manropeX;
};

enum class Face { soraLight, soraSemi, soraBold, manropeBold, manropeX };

// Mandos blancos con el punto del logo como indicador
class AlterIvoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AlterIvoLookAndFeel();
    void applySkinColours();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

private:
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Selector de palabras con el punto lima bajo la activa (modo, fuente)
class ModeSelector : public juce::Component
{
public:
    // inlineDot: punto a la izquierda de la palabra y todo alineado a la derecha
    explicit ModeSelector (juce::StringArray names = { "TRANSPOSE", "QUANTIZE", "ROBOT" },
                           float fontSize = 10.0f, bool inlineDot = false);
    void setSelected (int index);
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    std::vector<juce::Rectangle<float>> segments() const;
    int indexAt (float x) const;
    juce::StringArray names;
    float fontSize = 10.0f;
    bool inlineDot = false;
    int selected = 0, hover = -1;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Botón de enlace (cadena) entre Pitch y Formant
class LinkButton : public juce::Button
{
public:
    LinkButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Pantalla central: nota, tono (arriba) y formantes (abajo)
class VoiceDisplay : public juce::Component
{
public:
    VoiceDisplay();
    void push (float inHz, float outHz, const float* envDb, float envLevel, float formantSemis);
    void setRobotNote (int note) { robotNote = note; }
    void paint (juce::Graphics&) override;

private:
    static constexpr int kLen = 200;            // ~6.5 s a 30 Hz
    static constexpr int kEnv = AlterIvoProcessor::kFormantPoints;
    std::array<float, kLen> inNotes {}, outNotes {};
    std::array<float, kEnv> env {};
    int head = 0, robotNote = -1;
    float centre = 57.0f, envShow = 0.0f, formantShift = 0.0f;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Medidor de segmentos con retención de pico
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String labelText) : label (std::move (labelText)) {}
    void setPeak (float linear);
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    float levelDb = -100.0f, holdDb = -100.0f;
    int holdFrames = 0;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Tonalidad para Quantize: píldora que abre la lista (con detección automática)
class KeyBox : public juce::Component
{
public:
    KeyBox();
    void setText (const juce::String& t, bool active);
    void setProgress (float p);   // 0..1 mientras analiza; < 0 oculta la barra
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String text;
    bool active = false;
    float progress = -1.0f;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Píldora pequeña dentro de la pantalla (SYNC y A/B)
class CardPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    CardPill();
    // text: lo que se lee; active: relleno oscuro; segments: "A|B" dibuja dos letras con la elegida marcada
    void setState (const juce::String& text, bool active, int selectedSegment = -1);
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::String text;
    bool active = false;
    int segment = -1;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Preferencias guardadas en el ordenador (no en el proyecto)
struct UserSettings
{
    static bool loadDarkSkin();
    static void saveDarkSkin (bool dark);
};

// Ajustes: skin y perfil de latencia (abajo a la izquierda)
class SettingsButton : public juce::Component
{
public:
    SettingsButton();
    void setText (const juce::String& skin, const juce::String& latency);
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::String skinText, latencyText;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Preset: nombre (abre la lista) y flechas anterior/siguiente
class PresetBox : public juce::Component
{
public:
    PresetBox();
    void setText (const juce::String& t);
    std::function<void()> onClick, onPrev, onNext;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String text;
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Contenido de la ventana, dibujado siempre a 760 x 540 y escalado por el editor
class AlterIvoView : public juce::Component, private juce::Timer
{
public:
    explicit AlterIvoView (AlterIvoProcessor&);
    ~AlterIvoView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int index);
    void setupKnob (juce::Slider&, bool big);
    void loadPreset (int index);
    void showPresetMenu();
    void showKeyMenu();
    void showSettingsMenu();
    void showPatternMenu();
    void loadLogo();
    void setChoice (const char* id, int index);
    void updateKeyText();
    void paintBackground (juce::Graphics&);

    AlterIvoProcessor& proc;
    AlterIvoLookAndFeel lnf;
    juce::SharedResourcePointer<BrandFonts> fonts;

    juce::Slider pitch, formant, drive, mix, intensity, output;
    ModeSelector modeSelector;
    ModeSelector sourceSelector { { "VOICE", "MIX" }, 9.0f, true };
    KeyBox keyBox;
    SettingsButton settingsButton;
    CardPill syncPill, abPill;
    juce::TooltipWindow tooltips { this, 700 };
    LinkButton linkButton;
    VoiceDisplay display;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    PresetBox presetBox;
    std::unique_ptr<juce::Drawable> logoMark;
    juce::Image background;
    float backgroundScale = 0.0f;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> pitchAtt, formantAtt, driveAtt, mixAtt, intensityAtt, outputAtt;
    std::unique_ptr<ButtonAttachment> linkAtt;

    int currentMode = -1, shownPreset = -1, shownSource = -1, shownKey = -1;
    int keyMessageFrames = 0, seenSkin = -1;
    bool wasAnalyzing = false;
    std::array<float, AlterIvoProcessor::kFormantPoints> envBuf {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlterIvoView)
};

class AlterIvoEditor : public juce::AudioProcessorEditor
{
public:
    explicit AlterIvoEditor (AlterIvoProcessor&);
    ~AlterIvoEditor() override;
    void resized() override;

    static constexpr int kBaseW = 760, kBaseH = 540;

private:
    AlterIvoProcessor& proc;
    AlterIvoView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlterIvoEditor)
};
