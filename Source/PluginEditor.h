#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Theme.h"

// Mandos color crema con aspecto de hardware
class AlterIvoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AlterIvoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

// Selector giratorio de modo: TRANS / QUANT / ROBOT
class ModeSwitch : public juce::Component
{
public:
    ModeSwitch();
    void setSelected (int index);
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Point<float> centre() const;
    int selected = 0;
};

// Interruptor tipo palanca para Link
class PillSwitch : public juce::Button
{
public:
    PillSwitch();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Nombre del preset: al pulsar abre la lista
class PresetBox : public juce::Component
{
public:
    PresetBox();
    void setText (const juce::String& t);
    std::function<void()> onClick;

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
    static juce::String noteName (float hz);
    void loadPreset (int index);
    void showPresetMenu();

    AlterIvoProcessor& proc;
    AlterIvoLookAndFeel lnf;

    juce::Slider pitch, formant, drive, mix;
    ModeSwitch modeSwitch;
    PillSwitch linkSwitch;
    juce::ArrowButton prevPreset { "Preset anterior", 0.5f, alterivo::theme::textDim };
    juce::ArrowButton nextPreset { "Preset siguiente", 0.0f, alterivo::theme::textDim };
    PresetBox presetBox;
    int shownPreset = -1;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> pitchAtt, formantAtt, driveAtt, mixAtt;
    std::unique_ptr<ButtonAttachment> linkAtt;

    juce::String inText { "--" }, outText { "--" };
    bool robotMode = false;
    int currentMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlterIvoEditor)
};
