#include "PluginEditor.h"

namespace levelivo::ui
{
using namespace levelivo::theme;

// ============================================================================ LayerChip
int LayerChip::preferredWidth() const { return (int) textWidth (font (F::man, 10.5f), text) + 31; }
void LayerChip::paint (juce::Graphics& g)
{
    const bool lit = on == nullptr || *on;
    auto r = getLocalBounds().toFloat();
    const auto sq = juce::Rectangle<float> (2.0f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
    if (hatch)
    {
        g.saveState(); g.reduceClipRegion (sq.toNearestInt());
        g.setColour (col.withAlpha (lit ? 1.0f : 0.3f));
        for (float x = sq.getX() - 9; x < sq.getRight(); x += 3.0f) g.drawLine (x, sq.getBottom(), x + 9, sq.getY(), 1.0f);
        g.restoreState();
    }
    else
    {
        g.setColour (col.withAlpha (lit ? 1.0f : 0.25f)); g.fillRoundedRectangle (sq, 2.0f);
        if (col == juce::Colours::white) { g.setColour (juce::Colour (0xff9aa58f)); g.drawRoundedRectangle (sq, 2.0f, 1.0f); }
    }
    g.setColour (lit ? juce::Colour (0xff5b6852) : faint); g.setFont (font (F::man, 10.5f));
    g.drawText (text, r.withTrimmedLeft (17.0f), juce::Justification::centredLeft, false);
}

// ============================================================================ PresetPill
void PresetPill::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    cardBg (g, r, r.getHeight() * 0.5f);
    greenDot (g, { r.getX() + 18.0f, r.getCentreY() }, 4.0f);
    g.setColour (ink); g.setFont (font (F::man, 12.5f));
    g.drawText (proc.presetName, r.withTrimmedLeft (32.0f).withTrimmedRight (40.0f), juce::Justification::centredLeft, true);
    g.setColour (dim); g.setFont (font (F::man, 14.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x80\xb9  \xe2\x80\xba"), r.withTrimmedRight (14.0f), juce::Justification::centredRight, false);
}
void PresetPill::step (int d)
{
    const auto& all = proc.presets.all();
    int cur = 0;
    for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].name == proc.presetName) cur = i;
    proc.loadPreset ((cur + d + (int) all.size()) % (int) all.size());
    repaint();
}
void PresetPill::mouseDown (const juce::MouseEvent& e)
{
    const float rx = (float) getWidth() - e.position.x;
    if (rx < 26.0f) { step (1); return; }
    if (rx < 46.0f) { step (-1); return; }
    juce::PopupMenu m;
    const auto& all = proc.presets.all();
    m.addSectionHeader (u8 ("F\xc3\xa1" "brica"));
    for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].factory) m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
    bool any = false;
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].factory) { if (! any) { m.addSectionHeader ("Usuario"); any = true; } m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName); }
    m.addSeparator();
    m.addItem (1000, u8 ("Guardar como\xe2\x80\xa6"));
    m.addItem (1001, u8 ("Abrir la carpeta de presets"));
    juce::Component::SafePointer<PresetPill> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        if (r == 1000)
        {
            auto* w = new juce::AlertWindow (u8 ("Guardar preset"), u8 ("Nombre del preset:"), juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("n", p.presetName);
            w->addButton ("Guardar", 1); w->addButton ("Cancelar", 0);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int res)
            {
                if (res == 1 && safe != nullptr) { auto name = w->getTextEditorContents ("n").trim(); if (name.isNotEmpty()) safe->proc.savePreset (name); safe->repaint(); }
            }), true);
            return;
        }
        if (r == 1001) { PresetBank::userFolder().createDirectory(); PresetBank::userFolder().startAsProcess(); return; }
        p.loadPreset (r - 1);
        safe->repaint();
    });
}

// ============================================================================ MainView
MainView::MainView (LevelIvoProcessor& p)
    : proc (p),
      modeSeg (p.apvts, id::mode, { "NATURAL", "BALANCED", "CONTROLLED", "MANUAL" }, &p.undo),
      engineSeg (p.apvts, id::engine, { "OFFLINE", "REALTIME" }, &p.undo),
      applySeg (p.apvts, id::autoApply, { "AUTO APPLY", "REVISAR ANTES" }, &p.undo, true),
      listenSeg (p.apvts, id::listen, { "ORIGINAL", "NIVEL", "LIMPIEZA", "FINAL" }, nullptr),
      presetPill (p), wave (p),
      phraseS (p.apvts, id::phrase, "Frase", &p.undo), wordS (p.apvts, id::word, "Palabra", &p.undo),
      sylS (p.apvts, id::syllable, u8 ("S\xc3\xadlaba"), &p.undo), intensityS (p.apvts, id::intensity, "Intensidad", &p.undo),
      sensS (p.apvts, id::sensitivity, "Sensibilidad", &p.undo), smoothS (p.apvts, id::smoothing, "Suavizado", &p.undo),
      followS (p.apvts, id::beatFollow, "Seguir al beat", &p.undo), speedS (p.apvts, id::rtSpeed, "Velocidad (RT)", &p.undo),
      maxGainB (p.apvts, id::maxGain, u8 ("M\xc3\xa1x. subida"), &p.undo, 0.05f), maxAttB (p.apvts, id::maxAtt, u8 ("M\xc3\xa1x. bajada"), &p.undo, 0.05f),
      targetB (p.apvts, id::target, "Objetivo", &p.undo, 0.05f),
      peakSw (p.apvts, id::peakProtect, "Picos"), consSw (p.apvts, id::consonants, "Consonantes"), targetSw (p.apvts, id::targetOn),
      noiseSw (p.apvts, id::noiseOn), bleedSw (p.apvts, id::bleedOn), otherSw (p.apvts, id::otherOn), hitsSw (p.apvts, id::hitsOn),
      mouthSw (p.apvts, id::mouthOn), breathSw (p.apvts, id::breathOn), clipSw (p.apvts, id::clipRepair),
      noiseS (p.apvts, id::noise, "Ruido de fondo", &p.undo), bleedS (p.apvts, id::bleed, "Sangrado", &p.undo),
      otherS (p.apvts, id::other, "Otras voces", &p.undo), hitsS (p.apvts, id::hits, "Golpes al micro", &p.undo),
      mouthS (p.apvts, id::mouth, "Ruidos de boca", &p.undo), breathS (p.apvts, id::breath, "Respiraciones", &p.undo),
      clipSeg (p.apvts, id::clipRepair, { "DETECTAR", "REPARAR" }, &p.undo),
      outputB (p.apvts, id::output, "Output", &p.undo, 0.05f),
      bypassSw (p.apvts, id::bypass, "Bypass"), matchSw (p.apvts, id::matchLoud, "Volumen igualado")
{
    setLookAndFeel (&lnf);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &modeSeg, &engineSeg, &applySeg, &listenSeg, &abSeg, &presetPill, &captureB, &analyzeB, &applyB, &wave,
                                    &phraseS, &wordS, &sylS, &intensityS, &sensS, &smoothS, &followS, &speedS, &maxGainB, &maxAttB, &targetB, &peakSw, &consSw, &targetSw,
                                    &noiseSw, &bleedSw, &otherSw, &hitsSw, &mouthSw, &breathSw, &clipSw, &noiseS, &bleedS, &otherS, &hitsS, &mouthS, &breathS, &clipSeg,
                                    &outputB, &inM, &outM, &grM, &exportB, &rideB, &bypassSw, &matchSw })
        addAndMakeVisible (c);
    clipSw.setVisible (false);   // el clipping se detecta siempre; DETECTAR / REPARAR es su interruptor
    presetPill.setTooltip (u8 ("Presets: pulsa el nombre para elegir o guardar; \xe2\x80\xb9 \xe2\x80\xba pasan al anterior / siguiente."));
    abSeg.setTooltip (u8 ("Dos juegos de ajustes para comparar: A y B."));
    targetB.soft = false;
    outputB.soft = false; outputB.big = true;
    maxGainB.big = maxAttB.big = false;
    for (auto* s : { &noiseS, &bleedS, &otherS, &hitsS, &mouthS, &breathS }) { s->nameWidth = 104.0f; s->valueWidth = 50.0f; s->detailWidth = 66.0f; }

    captureB.redDot = true;
    captureB.setTooltip (u8 ("Graba lo que suena por esta pista mientras le das al play. Al parar el transporte se analiza solo."));
    analyzeB.setTooltip (u8 ("Para la captura y analiza (o vuelve a analizar la toma)."));
    applyB.setTooltip (u8 ("Aplica el resultado revisado (pasa a escucharse FINAL)."));
    exportB.setTooltip (u8 ("Guarda la voz procesada como WAV para arrastrarla al proyecto en la misma posici\xc3\xb3n."));
    rideB.setTooltip (u8 ("Escribe la curva en el par\xc3\xa1metro Ride Gain mientras suena: arma la automatizaci\xc3\xb3n de levelIVO en tu DAW, dale al play y luego retoca all\xc3\xad la curva."));
    captureB.onClick = [this] { if (proc.isCapturing()) proc.stopCapture (true); else proc.startCapture(); };
    analyzeB.onClick = [this] { proc.analyzeAgain(); };
    applyB.onClick = [this] { proc.apply(); };
    exportB.onClick = [this] { exportMenu(); };
    rideB.onClick = [this] { proc.setRideMode (! proc.isRideMode()); };
    abSeg.onChange = [this] (int s) { abSwitch (s); };

    struct C { const char* t; juce::Colour c; bool* f; bool h; };
    const C cs[] = { { "Curva", lime, &wave.layers.curve, false }, { "Frases", phraseC, &wave.layers.phrases, false },
                     { "Palabras", juce::Colour (0xff7fb85a), &wave.layers.words, false }, { "S\xc3\xadlabas", juce::Colour (0xffc3cfb6), &wave.layers.syllables, false },
                     { "Respiraciones", teal, &wave.layers.breaths, false }, { "Consonantes", juce::Colours::white, &wave.layers.consonants, false },
                     { "Ruido y golpes", amber, &wave.layers.noise, false }, { "Clipping", red, &wave.layers.clipping, false },
                     { "Baja confianza", juce::Colour (0xff9aa58f), &wave.layers.lowConf, true } };
    for (auto& c : cs)
    {
        chips.push_back (std::make_unique<LayerChip> (u8 (c.t), c.c, c.f, c.h));
        chips.back()->onChange = [this] { wave.repaint(); };
        addAndMakeVisible (*chips.back());
    }
    startTimerHz (30);
}
MainView::~MainView() { setLookAndFeel (nullptr); }

void MainView::abSwitch (int slot)
{
    if (slot == abSlot) return;
    auto snap = [this]
    {
        auto x = std::make_unique<juce::XmlElement> ("AB");
        for (auto* prm : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (r->getParameterID() != id::bypass && r->getParameterID() != id::rideGain && r->getParameterID() != id::listen)
                    x->setAttribute (r->getParameterID(), r->getValue());
        return x;
    };
    abState[abSlot] = snap();
    abSlot = slot;
    if (abState[slot] != nullptr)
        for (auto* prm : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (abState[slot]->hasAttribute (r->getParameterID()))
                    r->setValueNotifyingHost ((float) abState[slot]->getDoubleAttribute (r->getParameterID()));
}

void MainView::exportMenu()
{
    if (proc.getResult() == nullptr) { exportMsg = u8 ("Primero captura o arrastra una toma."); exportMsgTime = juce::Time::getMillisecondCounter(); return; }
    juce::PopupMenu m;
    m.addItem (1, "WAV 24 bits");
    m.addItem (2, "WAV 32 bits flotante");
    juce::Component::SafePointer<MainView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportB), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto res = safe->proc.getResult();
        const auto name = juce::File::createLegalFileName ((res != nullptr ? res->take->name : juce::String ("voz")) + " levelIVO.wav");
        safe->chooser = std::make_unique<juce::FileChooser> ("Exportar WAV", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (name), "*.wav");
        const bool f32 = r == 2;
        safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe, f32] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            auto f = fc.getResult().withFileExtension ("wav");
            safe->exportMsg = u8 ("Exportando\xe2\x80\xa6"); safe->exportMsgTime = juce::Time::getMillisecondCounter();
            safe->proc.exportFile (f, f32, [safe] (juce::String msg) { if (safe != nullptr) { safe->exportMsg = msg; safe->exportMsgTime = juce::Time::getMillisecondCounter(); safe->repaint(); } });
        });
    });
}

void MainView::timerCallback()
{
    inM.setLevelDb (20.0f * std::log10 (std::max (proc.inLevel(), 1e-6f)));
    outM.setLevelDb (20.0f * std::log10 (std::max (proc.outLevel(), 1e-6f)));
    grM.setLevelDb (proc.gainNow());

    const bool cap = proc.isCapturing();
    captureB.active = cap; captureB.blink = cap; captureB.text = cap ? "CAPTURANDO" : "CAPTURAR";
    captureB.repaint();
    auto r = proc.getResult();
    applyB.enabled = r != nullptr && ! proc.isApplied();
    applyB.text = proc.isApplied() && r != nullptr ? "APLICADO" : "APLICAR";
    applyB.repaint();
    rideB.active = proc.isRideMode(); rideB.repaint();
    const bool rtMode = (int) proc.apvts.getRawParameterValue (id::engine)->load() == 1;
    followS.setVisible (! rtMode); speedS.setVisible (rtMode);

    // filas de limpieza: aspecto apagado y lo detectado
    struct Row { Switch* sw; HSlider* s; };
    for (auto rr : { Row { &noiseSw, &noiseS }, Row { &bleedSw, &bleedS }, Row { &otherSw, &otherS }, Row { &hitsSw, &hitsS }, Row { &mouthSw, &mouthS }, Row { &breathSw, &breathS } })
        if (rr.s->enabledLook != rr.sw->isOn()) { rr.s->enabledLook = rr.sw->isOn(); rr.s->repaint(); }
    auto setDetail = [] (HSlider& s, juce::String t, juce::Colour c = theme::dim) { if (s.detail != t || s.detailColour != c) { s.detail = t; s.detailColour = c; s.repaint(); } };
    if (r != nullptr && r->clean != nullptr && r->analysis != nullptr)
    {
        const auto& C = *r->clean; const auto& A = *r->analysis;
        setDetail (noiseS, juce::String (juce::roundToInt (C.noiseFloorDb)).replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92")) + " dBFS");
        setDetail (bleedS, r->take->hasReference() ? "coh. " + juce::String (C.coherence, 2).replace (".", ",") : juce::String ("sin ref."), r->take->hasReference() && C.coherence < 0.2f ? amber : dim);
        setDetail (otherS, juce::String ((int) A.others.size()));
        setDetail (hitsS, juce::String ((int) A.hits.size()));
        setDetail (mouthS, juce::String (C.nClicks));
        setDetail (breathS, juce::String ((int) A.breaths.size()));
    }
    else for (auto* s : { &noiseS, &bleedS, &otherS, &hitsS, &mouthS, &breathS }) setDetail (*s, juce::String::fromUTF8 ("\xe2\x80\x94"));
    repaint (getLocalBounds().withHeight (136));               // estado y cabecera
    repaint (juce::Rectangle<int> (440, 542, 476, 40));        // referencia
    repaint (juce::Rectangle<int> (440, 736, 476, 30));        // clipping
    repaint (juce::Rectangle<int> (928, 542, 248, 80));        // corrección media
    repaint (juce::Rectangle<int> (928, 760, 248, 40));
}

void MainView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (bgGlow, b.getCentreX(), 0, bg, b.getCentreX(), b.getHeight() * 0.7f, true));
    g.fillAll();
    // cabecera
    g.setColour (ink); g.setFont (font (F::soraLight, 27.0f, -0.01f));
    g.drawText ("level", 28, 18, 80, 36, juce::Justification::centredLeft);
    const float lw = textWidth (font (F::soraLight, 27.0f, -0.01f), "level");
    g.setColour (deep); g.setFont (font (F::soraBold, 27.0f, -0.01f));
    g.drawText ("IVO", juce::Rectangle<float> (28 + lw, 18, 80, 36), juce::Justification::centredLeft);
    caps (g, u8 ("Analiza \xc2\xb7 Nivela \xc2\xb7 Limpia"), { 29, 54, 200, 12 }, juce::Justification::centredLeft, dim, 8.5f);
    drawLogo (g, { 1100.0f, 23.0f, 70.0f, 30.0f });

    // barra de acciones
    cardBg (g, { 24, 76, 1152, 50 });
    auto r = proc.getResult();
    g.setColour (ink); g.setFont (font (F::man, 12.5f));
    g.drawText (proc.statusText(), juce::Rectangle<float> (296, 82, 520, 18), juce::Justification::centredLeft, true);
    juce::String sub;
    if (r != nullptr && r->analysis != nullptr && r->level != nullptr)
    {
        const auto& A = *r->analysis;
        double c = 0; for (auto& w : A.words) c += w.conf;
        int low = 0; for (auto& w : A.words) low += w.conf < 0.5f;
        sub << (int) A.phrases.size() << " frases " << juce::String::fromUTF8 ("\xc2\xb7 ") << (int) A.words.size() << " palabras " << juce::String::fromUTF8 ("\xc2\xb7 ")
            << (int) A.syllables.size() << juce::String::fromUTF8 (" s\xc3\xadlabas \xc2\xb7 confianza media ") << juce::roundToInt (100.0 * c / std::max<size_t> (1, A.words.size()))
            << " % " << juce::String::fromUTF8 ("\xc2\xb7 ") << low << " a revisar " << juce::String::fromUTF8 ("\xc2\xb7 dispersi\xc3\xb3n ")
            << juce::String (r->level->spreadBefore, 1).replace (".", ",") << juce::String::fromUTF8 (" \xe2\x86\x92 ") << juce::String (r->level->spreadAfter, 1).replace (".", ",") << " dB";
        if (r->take->hasReference()) sub << juce::String::fromUTF8 (" \xc2\xb7 con beat de referencia");
    }
    else if (! proc.isCapturing()) sub = juce::String::fromUTF8 ("Para el sangrado de cascos, env\xc3\xad" "a el beat al sidechain de levelIVO antes de capturar.");
    g.setColour (dim); g.setFont (font (F::man, 9.5f));
    g.drawText (sub, juce::Rectangle<float> (296, 101, 520, 14), juce::Justification::centredLeft, true);

    // fila de capas y escucha

    // tarjetas
    cardBg (g, { 24, 542, 404, 258 });
    cardBg (g, { 440, 542, 476, 258 });
    cardBg (g, { 928, 542, 248, 258 });
    caps (g, u8 ("Nivelaci\xc3\xb3n"), { 42, 554, 200, 16 }, juce::Justification::centredLeft, deep, 9.0f);
    caps (g, "Limpieza", { 458, 554, 200, 16 }, juce::Justification::centredLeft, deep, 9.0f);
    caps (g, "Salida", { 946, 554, 200, 16 }, juce::Justification::centredLeft, deep, 9.0f);
    {
        const bool ref = r != nullptr ? r->take->hasReference() : proc.sidechainActive();
        g.setFont (font (F::man, 10.0f));
        g.setColour (ref ? deep : dim);
        g.drawText (ref ? juce::String::fromUTF8 ("\xe2\x97\x8f Referencia: beat por sidechain") : juce::String::fromUTF8 ("Sin referencia (beat al sidechain = mejor sangrado)"),
                    juce::Rectangle<float> (600, 554, 298, 16), juce::Justification::centredRight, false);
    }
    // clipping (fila propia)
    {
        g.setColour (body); g.setFont (font (F::man, 11.0f));
        g.drawText ("Clipping", juce::Rectangle<float> (458, 738, 110, 24), juce::Justification::centredLeft);
        juce::String t = juce::String::fromUTF8 ("\xe2\x80\x94"); juce::Colour c = dim;
        if (r != nullptr && r->clean != nullptr)
        {
            const int tot = (int) r->analysis->clips.size();
            if (tot == 0) t = "ninguno";
            else { t = juce::String (r->clean->nClipsFixed) + " / " + juce::String (tot) + " reparados"; c = red; }
        }
        g.setColour (c); g.setFont (font (F::man, 9.5f));
        g.drawText (t, juce::Rectangle<float> (790, 738, 108, 24), juce::Justification::centredRight);
        g.setColour (dim); g.setFont (font (F::man, 9.0f));
        g.drawText (u8 ("Las cifras de la derecha son lo detectado en esta toma. La limpieza va antes de la nivelaci\xc3\xb3n."),
                    juce::Rectangle<float> (458, 772, 440, 14), juce::Justification::centredLeft, true);
    }
    // salida: corrección media
    {
        caps (g, u8 ("Correcci\xc3\xb3n media"), { 946, 578, 120, 11 }, juce::Justification::centredLeft, dim, 7.5f, 0.2f);
        juce::String v = juce::String::fromUTF8 ("\xe2\x80\x94");
        if (r != nullptr && r->level != nullptr) v = juce::String (r->level->meanAbsCorr, 1).replace (".", ",");
        const auto f = font (F::soraSemi, 20.0f);
        g.setColour (ink); g.setFont (f);
        g.drawText (v, juce::Rectangle<float> (946, 590, 100, 26), juce::Justification::bottomLeft);
        g.setColour (dim); g.setFont (font (F::man, 11.0f));
        g.drawText ("dB", juce::Rectangle<float> (946 + textWidth (f, v) + 3, 590, 30, 26), juce::Justification::bottomLeft);
    }
    if (exportMsg.isNotEmpty() && juce::Time::getMillisecondCounter() - exportMsgTime < 9000)
    {
        g.setColour (deep); g.setFont (font (F::man, 9.5f));
        g.drawText (exportMsg, juce::Rectangle<float> (946, 774, 222, 14), juce::Justification::centredLeft, true);
    }
}

void MainView::resized()
{
    modeSeg.setBounds (232, 24, 352, 30);
    engineSeg.setBounds (596, 24, 172, 30);
    presetPill.setBounds (778, 18, 206, 42);
    abSeg.setBounds (996, 24, 76, 30);

    captureB.setBounds (40, 85, 122, 32);
    analyzeB.setBounds (170, 85, 110, 32);
    applyB.setBounds (1062, 85, 100, 32);
    applySeg.setBounds (1062 - 10 - 230, 86, 230, 30);

    wave.setBounds (24, 136, 1152, 360);

    int x = 30;
    for (auto& c : chips) { const int w = c->preferredWidth(); c->setBounds (x, 504, w, 26); x += w + 6; }
    listenSeg.setBounds (884, 502, 292, 30);
    listenSeg.setTooltip (helpFor (id::listen));

    // nivelación
    targetSw.setBounds (276, 552, 30, 20);
    targetSw.setTooltip (helpFor (id::targetOn));
    targetB.setBounds (310, 546, 104, 32);
    int y = 578;
    for (auto* s : { &phraseS, &wordS, &sylS, &intensityS, &sensS, &smoothS }) { s->setBounds (42, y, 368, 24); y += 24; }
    followS.setBounds (42, y, 368, 24); speedS.setBounds (42, y, 368, 24);
    maxGainB.setBounds (42, 750, 108, 42);
    maxAttB.setBounds (158, 750, 108, 42);
    peakSw.setBounds (282, 752, 128, 18);
    consSw.setBounds (282, 774, 128, 18);

    // limpieza
    y = 580;
    for (auto rr : { std::make_pair (&noiseSw, &noiseS), std::make_pair (&bleedSw, &bleedS), std::make_pair (&otherSw, &otherS),
                     std::make_pair (&hitsSw, &hitsS), std::make_pair (&mouthSw, &mouthS), std::make_pair (&breathSw, &breathS) })
    {
        rr.first->setBounds (458, y + 4, 28, 18);
        rr.second->setBounds (492, y, 406, 26);
        y += 26;
    }
    clipSeg.setBounds (586, 739, 200, 24);

    // salida
    outputB.setBounds (1074, 574, 96, 44);
    inM.setBounds (946, 636, 212, 14);
    outM.setBounds (946, 656, 212, 14);
    grM.setBounds (946, 676, 212, 14);
    exportB.setBounds (946, 704, 100, 28);
    rideB.setBounds (1052, 704, 118, 28);
    exportB.fontSize = rideB.fontSize = 8.0f;
    bypassSw.setBounds (946, 744, 90, 20);
    matchSw.setBounds (1040, 744, 130, 20);
}
} // namespace levelivo::ui

// ============================================================================ Editor
LevelIvoEditor::LevelIvoEditor (levelivo::LevelIvoProcessor& p) : AudioProcessorEditor (&p), proc (p), view (p)
{
    const float s = juce::jlimit (0.75f, 2.0f, proc.uiScale.load());
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (kW * 3 / 4, kH * 3 / 4, kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
}
LevelIvoEditor::~LevelIvoEditor() = default;
void LevelIvoEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, kW, kH);
    proc.uiScale = s;
}
bool LevelIvoEditor::isInterestedInFileDrag (const juce::StringArray& f)
{
    for (auto& s : f) if (s.endsWithIgnoreCase (".wav") || s.endsWithIgnoreCase (".aif") || s.endsWithIgnoreCase (".aiff") || s.endsWithIgnoreCase (".flac")) return true;
    return false;
}
void LevelIvoEditor::filesDropped (const juce::StringArray& f, int, int)
{
    for (auto& s : f) if (isInterestedInFileDrag ({ s })) { proc.loadFile (juce::File (s)); break; }
}
