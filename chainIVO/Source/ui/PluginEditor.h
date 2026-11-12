// chainIVO — USER INTERFACE. Diseño aprobado (claro, con relieve, familia IVO), 1040 x 740 escalable 75-200 %.
// La interfaz nunca bloquea el audio: sólo lee atómicos y el resultado del análisis, y escribe parámetros.
#pragma once
#include "../PluginProcessor.h"
#include "Widgets.h"

namespace chainivo::ui
{
class Chip : public juce::Component
{
public:
    explicit Chip (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    static constexpr int pad = 9;    // margen para la sombra redonda
    bool hitTest (int x, int y) override { return getLocalBounds().reduced (pad).contains (x, y); }
    bool selected = false;
    std::function<void()> onClick;
    juce::String text;
    int preferredWidth() const { return (int) textWidth (font (F::man, 12.5f), text) + 26; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

// Ficha de un motor en la cadena
class ChainTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChainTile (ChainIvoProcessor&, int engine);
    bool selected = false, pro = false, hovering = false;
    int position = 0;
    std::function<void()> onSelect;
    std::function<void (int)> onMove;
    void refresh();
    bool lit() const { return on; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hovering = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hovering = false; repaint(); }
private:
    bool engineOn() const;
    ChainIvoProcessor& proc;
    int engine;
    juce::String headline, line1, line2;
    bool on = true;
};

// Tarjeta central: gráfico + explicación (AUTO) o ajustes del motor (PRO)
class EngineView : public juce::Component
{
public:
    explicit EngineView (ChainIvoProcessor&);
    void setEngine (int e, bool pro);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void buildControls();
    void paintGraph (juce::Graphics&, juce::Rectangle<float>);
    void paintEq (juce::Graphics&, juce::Rectangle<float>);
    void paintHistory (juce::Graphics&, juce::Rectangle<float>, std::initializer_list<std::pair<const std::vector<float>*, juce::Colour>>, const juce::String& unit);
    void paintHarm (juce::Graphics&, juce::Rectangle<float>);
    void paintSpace (juce::Graphics&, juce::Rectangle<float>);
    void paintCreative (juce::Graphics&, juce::Rectangle<float>);
    void push (std::vector<float>& v, float x) { v.erase (v.begin()); v.push_back (x); }

    ChainIvoProcessor& proc;
    int engine = 0; bool pro = false;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<juce::Rectangle<int>> layout;
    Toggle lockToggle { "LOCK" }, soloToggle { "SOLO" };
    PillButton autoButton { "AUTO", PillButton::lime };
    std::vector<float> histComp, histDeess, histHarsh, histBreath, histWord, histWet;
};

class TrackCard : public juce::Component
{
public:
    explicit TrackCard (ChainIvoProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void showMore();
    void showGenres();
    void showTools();
    ChainIvoProcessor& proc;
    juce::OwnedArray<Chip> chips;
    juce::Rectangle<float> genreBox, reBox, toolsBox;
    std::unique_ptr<juce::FileChooser> chooser;
};

class PresetPill : public juce::Component
{
public:
    explicit PresetPill (ChainIvoProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    static constexpr int pad = 14;   // margen para la sombra redonda
    bool hitTest (int x, int y) override { return getLocalBounds().reduced (pad).contains (x, y); }
    std::function<void()> onOpen;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void step (int d);
    ChainIvoProcessor& proc;
};

class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (ChainIvoProcessor&);
    std::function<void()> onClose;
    void open();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void update();
    void saveAs();
    ChainIvoProcessor& proc;
    juce::TextEditor search;
    Segmented cats;
    Toggle favs { u8 ("S\xc3\x93LO FAVORITOS") };
    juce::ListBox list { "presets", this };
    PillButton saveB { u8 ("GUARDAR COMO\xe2\x80\xa6") }, importB { "IMPORTAR" }, exportB { "EXPORTAR" }, closeB { "CERRAR", PillButton::dark };
    std::vector<PresetInfo> items;
    juce::Rectangle<int> card;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Deshacer / rehacer: una sola pastilla con dos flechas claras (se apagan cuando no hay nada que deshacer)
class UndoRedo : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit UndoRedo (juce::UndoManager& u) : um (u) { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip ("Deshacer / rehacer"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { if (e.position.x < getWidth() * 0.5f) um.undo(); else um.redo(); repaint(); }
    void mouseMove (const juce::MouseEvent& e) override { const int h = e.position.x < getWidth() * 0.5f ? 0 : 1; if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    juce::UndoManager& um;
    int hover = -1;
};

class MainView : public juce::Component, private juce::Timer
{
public:
    explicit MainView (ChainIvoProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    LookAndFeel lnf;
private:
    void timerCallback() override;
    void selectEngine (int e);
    void layoutTiles();
    ChainIvoProcessor& proc;
    Segmented modeSwitch { { "AUTO", "PRO" } };
    PresetPill presetPill;
    UndoRedo undoRedo;
    Segmented abSwitch { { "A", "B" }, false, 12.0f };
    TrackCard trackCard;
    juce::OwnedArray<ChainTile> tiles;
    EngineView engineView;
    Knob inputK, intensityK, mixK, outputK;
    Segmented quality { { "ECO", "NORMAL", "HIGH", "ULTRA" } };
    ParamToggle gainMatch, clipSafe, bypass;
    Meter inMeter { "IN" }, outMeter { "OUT" };
    PresetBrowser browser;
    juce::TooltipWindow tooltips { this, 600 };
    int selected = 0;
    bool lastPro = false;
};
} // namespace chainivo::ui

class ChainIvoEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChainIvoEditor (ChainIvoProcessor&);
    ~ChainIvoEditor() override;
    void resized() override;
    static constexpr int kW = 1040, kH = 740;
private:
    ChainIvoProcessor& proc;
    chainivo::ui::MainView view;
};
