// levelIVO — INTERFAZ. 1200 x 820, escalable 75-200 %. La pantalla de forma de onda y la curva son el centro;
// alrededor, los bloques GLOBAL (cabecera y barra de acciones), NIVELACIÓN, LIMPIEZA y SALIDA.
#pragma once
#include "../PluginProcessor.h"
#include "WaveView.h"

namespace levelivo::ui
{
class LayerChip : public juce::Component
{
public:
    LayerChip (juce::String t, juce::Colour c, bool* flag, bool hatched = false) : text (std::move (t)), col (c), on (flag), hatch (hatched)
    { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (on) { *on = ! *on; repaint(); if (onChange) onChange(); } }
    int preferredWidth() const;
    std::function<void()> onChange;
private:
    juce::String text; juce::Colour col; bool* on; bool hatch;
};

class PresetPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetPill (LevelIvoProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void step (int d);
    LevelIvoProcessor& proc;
};

class MainView : public juce::Component, private juce::Timer
{
public:
    explicit MainView (LevelIvoProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    LookAndFeel lnf;
private:
    void timerCallback() override;
    void exportMenu();
    void abSwitch (int slot);

    LevelIvoProcessor& proc;
    ParamSeg modeSeg, engineSeg, applySeg, listenSeg;
    Segmented abSeg { { "A", "B" } };
    PresetPill presetPill;
    Button captureB { "CAPTURAR", Button::outline }, analyzeB { "ANALIZAR", Button::lime }, applyB { "APLICAR", Button::dark };
    WaveView wave;
    std::vector<std::unique_ptr<LayerChip>> chips;
    // nivelación
    HSlider phraseS, wordS, sylS, intensityS, sensS, smoothS, followS, speedS;
    ValueBox maxGainB, maxAttB, targetB;
    Switch peakSw, consSw, targetSw;
    // limpieza
    Switch noiseSw, bleedSw, otherSw, hitsSw, mouthSw, breathSw, clipSw;
    HSlider noiseS, bleedS, otherS, hitsS, mouthS, breathS;
    ParamSeg clipSeg;
    // salida
    ValueBox outputB;
    Meter inM { "IN" }, outM { "OUT" }, grM { "GAIN", true };
    Button exportB { "EXPORTAR WAV", Button::light }, rideB { u8 ("AUTOMATIZACI\xc3\x93N"), Button::light };
    Switch bypassSw, matchSw;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String exportMsg; juce::uint32 exportMsgTime = 0;
    std::unique_ptr<juce::XmlElement> abState[2];
    int abSlot = 0;
};
} // namespace levelivo::ui

class LevelIvoEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget
{
public:
    explicit LevelIvoEditor (levelivo::LevelIvoProcessor&);
    ~LevelIvoEditor() override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    static constexpr int kW = 1200, kH = 820;
private:
    levelivo::LevelIvoProcessor& proc;
    levelivo::ui::MainView view;
};
namespace levelivo { using ::LevelIvoEditor; }
