#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Theme.h"

#include <array>

// Mandos oscuros con anillo luminoso
class AlterIvoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AlterIvoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
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

private:
    int selected = 0;
};

// Botón de enlace (cadena) entre Pitch y Formant
class LinkButton : public juce::Button
{
public:
    LinkButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Pantalla en tiempo real: tono de entrada y de salida
class PitchDisplay : public juce::Component
{
public:
    void push (float inHz, float outHz);
    void setRobotNote (int note) { robotNote = note; }
    void paint (juce::Graphics&) override;

private:
    static constexpr int kLen = 240;          // ~8 s a 30 Hz
    std::array<float, kLen> inNotes {}, outNotes {};
    int head = 0;
    float centre = 57.0f;                     // nota central de la vista (se desplaza sola)
    int robotNote = -1;
};

// Medidor horizontal con retención de pico
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
};

// Nombre del preset: al pulsar abre la lista
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
};

class AlterIvoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AlterIvoEditor (AlterIvoProcessor&);
    ~AlterIvoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int index);
    void setupKnob (juce::Slider&, bool big);
    void linkedMove (juce::Slider& moved, juce::Slider& other, double& lastMoved, double& lastOther);
    void loadPreset (int index);
    void showPresetMenu();
    static juce::String noteName (float hz);

    AlterIvoProcessor& proc;
    AlterIvoLookAndFeel lnf;

    juce::Slider pitch, formant, drive, mix;
    ModeSelector modeSelector;
    LinkButton linkButton;
    PitchDisplay display;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    PresetBox presetBox;
    std::unique_ptr<juce::Drawable> logoMark;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> pitchAtt, formantAtt, driveAtt, mixAtt;
    std::unique_ptr<ButtonAttachment> linkAtt;

    double lastPitch = 0.0, lastFormant = 0.0;
    bool linking = false;
    int currentMode = -1, shownPreset = -1;
    juce::String inText { "--" }, outText { "--" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlterIvoEditor)
};
