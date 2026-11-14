// reverseIVO — INTERFAZ. Tres pantallas: CREATE (lo de cada día), CUSTOMIZE (diseño del sonido) y FX.
// Se dibuja a 900 x 712 y se escala entera (mantiene la proporción) para que todo quede nítido a cualquier tamaño.
#pragma once
#include "Views.h"

namespace reverseivo
{
namespace ui
{
// Una «tarjeta» de FX/CUSTOMIZE: título, interruptor opcional y una fila de perillas
struct Card
{
    juce::String title;
    juce::Rectangle<int> bounds;
    ParamToggle* toggle = nullptr;
    std::vector<juce::Component*> items;
};

class MainView : public juce::Component, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit MainView (ReverseIvoProcessor&);
    ~MainView() override;
    static constexpr int kW = 900, kH = 712;
    void paint (juce::Graphics&) override;
    void resized() override {}
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    // muestra una pantalla (0 CREATE, 1 CUSTOMIZE, 2 FX) y su sub-pestaña: lo usan las pruebas
    void showView (int p, int sub) { if (p == 1) setLane (sub); if (p == 2) setFxPage (sub); setPage (p); }
private:
    void timerCallback() override;
    void setPage (int p);
    void setLane (int l);
    void setFxPage (int c);
    void layoutCards (std::vector<Card>& cards, int y, int h);
    Knob* knob (const char* name, const char* id, float d = 88.0f, bool compact = true);
    ParamToggle* toggle (const char* name, const char* id);
    void showPresetMenu();
    void openImport();
    void openExport();
    void paintCreate (juce::Graphics&);
    void paintCards (juce::Graphics&, const std::vector<Card>&);

    ReverseIvoProcessor& proc;
    LookAndFeel lnf;
    juce::TooltipWindow tips { this, 600 };
    std::vector<std::unique_ptr<juce::Component>> owned;
    int page = 0, lane = 0, fxPage = 0;

    // cabecera
    juce::Rectangle<int> tabRects[3];
    juce::Rectangle<int> presetRect;
    RoundButton presetPrev { Icon::left }, presetNext { Icon::right };

    // CREATE
    static constexpr int heroX = 44, heroY = 90, heroW = 812, heroH = 240;
    WaveView wave;
    RoundButton capture { Icon::rec }, import { Icon::import }, listen { Icon::ear };
    ParamToggle sync;
    ParamChoice duration;
    Knob kIntensity, kPitch, kCharacter;
    RoundButton preview { Icon::play }, reset { Icon::reset };
    PillButton generate { "GENERATE", PillButton::cream };
    RoundButton hitPrev { Icon::left }, hitNext { Icon::right };
    PillButton hitSet { "SET", PillButton::dark };
    juce::Rectangle<int> durationRect;
    float freeDragStart = 0.0f; int freeDragY = 0; bool draggingFree = false;

    // CUSTOMIZE
    Segmented lanes { { "VOLUME", "PITCH", "FILTER", "LAYERS" } };
    CurveView curve;
    std::vector<juce::Component*> laneItems[4];
    std::vector<Card> laneCards[4];
    ParamChoice* ending = nullptr;

    // FX
    Segmented fxTabs { { "SPACE", "RHYTHM", "CHARACTER", "STEREO", "MOVEMENT", "SMART" } };
    std::vector<Card> fxCards[6];

    // pie
    PillButton dragOut { "DRAG TO DAW", PillButton::dark };
    RoundButton exportBtn { Icon::download };
    ParamChoice genMode;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Image grain;
    juce::String lastStatus, lastPreset;
};
} // namespace ui

class ReverseIvoEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverseIvoEditor (ReverseIvoProcessor&);
    ~ReverseIvoEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff080a07)); }
    ui::MainView& getView() { return view; }
private:
    ReverseIvoProcessor& proc;
    ui::MainView view;
};
} // namespace reverseivo
