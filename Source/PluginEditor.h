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
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

private:
    juce::SharedResourcePointer<BrandFonts> fonts;
};

// Selector de modo segmentado (Transpose / Quantize / Robot)
class ModeSelector : public juce::Component
{
public:
    ModeSelector();
    void setSelected (int index);
    std::function<void (int)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
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
    void linkedMove (juce::Slider& moved, juce::Slider& other, double& lastMoved, double& lastOther);
    void loadPreset (int index);
    void showPresetMenu();
    void paintBackground (juce::Graphics&);

    AlterIvoProcessor& proc;
    AlterIvoLookAndFeel lnf;
    juce::SharedResourcePointer<BrandFonts> fonts;

    juce::Slider pitch, formant, drive, mix;
    ModeSelector modeSelector;
    LinkButton linkButton;
    VoiceDisplay display;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    PresetBox presetBox;
    std::unique_ptr<juce::Drawable> logoMark;
    juce::Image background;
    float backgroundScale = 0.0f;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> pitchAtt, formantAtt, driveAtt, mixAtt;
    std::unique_ptr<ButtonAttachment> linkAtt;

    double lastPitch = 0.0, lastFormant = 0.0;
    bool linking = false;
    int currentMode = -1, shownPreset = -1;
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
