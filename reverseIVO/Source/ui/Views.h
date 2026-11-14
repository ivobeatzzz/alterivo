// reverseIVO — pantallas: la forma de onda del reverse (CREATE) y el editor de curvas (CUSTOMIZE).
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace reverseivo::ui
{
// Forma de onda del reverse hasta el golpe, cola después, cabezal de reproducción y, debajo,
// la fuente con la región elegida (sus bordes se arrastran; doble clic = selección automática).
class WaveView : public juce::Component
{
public:
    explicit WaveView (ReverseIvoProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick();   // 30 Hz: sólo repinta si cambió algo visible
    static constexpr float stripH = 22.0f;
private:
    juce::Rectangle<float> waveArea() const { return getLocalBounds().toFloat().withTrimmedBottom (stripH + 8.0f); }
    juce::Rectangle<float> stripArea() const { return getLocalBounds().toFloat().removeFromBottom (stripH).withTrimmedLeft (34.0f); }
    float xForSource (int sample) const;
    int sampleForX (float x) const;
    ReverseIvoProcessor& proc;
    int drag = 0;   // 1 inicio, 2 fin, 3 nueva región
    int dragAnchor = 0;
    int lastPlay = -2, lastSrcPlay = -2, lastGen = -1, lastA = -1, lastB = -1;
    const void* lastSource = nullptr;
};

// Editor de curvas: la curva del carril elegido sobre la forma de onda. Los puntos se arrastran:
// principio y final cambian el nivel/tono/frecuencia de cada extremo; el central, la forma de la curva.
class CurveView : public juce::Component
{
public:
    enum Lane { volume = 0, pitch, filter, layers };
    CurveView (ReverseIvoProcessor& p) : proc (p) {}
    void setLane (int l) { lane = l; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
private:
    struct Handle { juce::String pid; float x, y; bool curve; };
    std::vector<Handle> handles() const;
    float yFor (float norm) const;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedTop (46.0f).withTrimmedBottom (24.0f).reduced (34.0f, 0.0f); }
    ReverseIvoProcessor& proc;
    int lane = volume, active = -1, hover = -1;
    float dragStartY = 0, dragStartVal = 0;
    juce::int64 lastHash = 0;
    int lastGen = -1;
};
} // namespace reverseivo::ui
