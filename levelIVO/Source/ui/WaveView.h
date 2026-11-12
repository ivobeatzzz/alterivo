// levelIVO — PANTALLA CENTRAL. Forma de onda original y resultado, curva de ganancia real (la que se aplica),
// frases / palabras / sílabas con su confianza, respiraciones, consonantes protegidas, ruido, golpes, clics,
// clipping y zonas de baja confianza. Selección, edición de la curva, zoom y desplazamiento.
#pragma once
#include "../PluginProcessor.h"
#include "Widgets.h"

namespace levelivo::ui
{
struct Layers
{
    bool curve = true, phrases = true, words = true, syllables = true, breaths = true, consonants = true,
         noise = true, clipping = true, lowConf = true;
};

class Inspector : public juce::Component
{
public:
    Inspector();
    void paint (juce::Graphics&) override;
    void resized() override;
    void set (juce::String title, juce::String kind, std::vector<std::pair<juce::String, juce::String>> rows);
    std::function<void()> onLock, onExpression, onReset;
    juce::Colour accent = theme::deep;
private:
    juce::String title, kind;
    std::vector<std::pair<juce::String, juce::String>> rows;
    Button lockB { "BLOQUEAR", Button::light }, exprB { u8 ("EXPRESI\xc3\x93N"), Button::light }, resetB { "RESET", Button::light };
};

class WaveView : public juce::Component, private juce::Timer
{
public:
    explicit WaveView (LevelIvoProcessor&);
    ~WaveView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float) override;
    bool keyPressed (const juce::KeyPress&) override;

    Layers layers;
    void zoomToFit();
    void zoomBy (float factor, float atX);

private:
    struct Sel { int a = 0, b = 0; int kind = 1; int index = -1; };   // muestras; kind 0 frase, 1 palabra, 2 sílaba
    void timerCallback() override;
    void rebuild();                       // al llegar un resultado nuevo
    juce::Rectangle<float> screenArea() const;
    juce::Rectangle<float> waveArea() const;
    juce::Rectangle<float> laneArea (int lane) const;   // 0 frases, 1 palabras, 2 sílabas
    juce::Rectangle<float> miniArea() const;
    float xOf (double sample) const;
    double sampleAt (float x) const;
    float yOfDb (float db) const;
    float curveDbAt (int frame) const;
    void paintEmpty (juce::Graphics&, juce::Rectangle<float>);
    void paintWave (juce::Graphics&);
    void paintMini (juce::Graphics&);
    int hitSegment (int lane, double sample, Sel& out) const;
    void updateInspector();
    void showMenu (const juce::MouseEvent&);
    void lockSelection();
    void expressionSelection();
    void resetSelection();
    float meanGainDb (int a, int b) const;

    LevelIvoProcessor& proc;
    ResultPtr res;
    int resVersion = -1;
    // picos por bloques de 256 muestras (original y resultado) para dibujar rápido con mucho zoom out
    std::vector<float> pkOrigMax, pkOrigMin, pkOutMax, pkOutMin, miniPk;
    static constexpr int kBlock = 256;
    double viewStart = 0, viewLen = 48000 * 10, shownStart = -1, shownLen = -1;
    std::vector<Sel> sel;
    bool draggingCurve = false, draggingMini = false;
    float dragStartY = 0, dragDb = 0;
    juce::int64 lastPlay = -1;
    Inspector inspector;
    Button undoB { u8 ("\xe2\x86\xb6"), Button::light }, redoB { u8 ("\xe2\x86\xb7"), Button::light }, fitB { "AJUSTAR", Button::light }, playB { "ESCUCHAR", Button::light };
    juce::String hoverText;
};
} // namespace levelivo::ui
