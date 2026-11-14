#include "PluginEditor.h"

namespace reverseivo
{
namespace ui
{
using namespace reverseivo::theme;

static const char* tabNames[3] = { "CREATE", "CUSTOMIZE", "FX" };

MainView::MainView (ReverseIvoProcessor& p)
    : proc (p), wave (p),
      sync ("SMART SYNC", p.apvts, id::smartSync, &p.undo),
      duration (p.apvts, id::duration, &p.undo, 11.0f),
      kIntensity ("INTENSITY", p.apvts, id::intensity, &p.undo, 116.0f),
      kPitch ("PITCH", p.apvts, id::pitch, &p.undo, 116.0f),
      kCharacter ("CHARACTER", p.apvts, id::character, &p.undo, 116.0f),
      curve (p),
      genMode (p.apvts, id::genMode, &p.undo, 9.5f)
{
    setLookAndFeel (&lnf);
    setSize (kW, kH);
    setWantsKeyboardFocus (true);

    // ruido muy suave encima del fondo: textura de material, no un degradado plano
    grain = juce::Image (juce::Image::ARGB, kW, kH, true);
    {
        juce::Random rnd (7);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                grain.setPixelAt (x, y, juce::Colours::white.withAlpha ((float) rnd.nextInt (10) / 255.0f));
    }

    // ---------------------------------------------------------------- CREATE
    for (auto* c : std::initializer_list<juce::Component*> { &wave, &capture, &import, &listen, &sync, &duration, &kIntensity, &kPitch, &kCharacter,
                                                            &preview, &reset, &generate, &hitPrev, &hitNext, &hitSet })
        addAndMakeVisible (c);
    wave.setBounds (heroX + 18, heroY + 62, heroW - 40, 140);
    capture.setBounds (heroX + 14, heroY + 12, 40, 40);
    import.setBounds (heroX + 54, heroY + 12, 40, 40);
    listen.setBounds (heroX + 94, heroY + 12, 40, 40);
    capture.setTooltip (u8 ("Capturar: pulsa y reproduce el proyecto. Al parar el transporte se genera solo."));
    import.setTooltip (u8 ("Importar un audio (WAV, AIFF, FLAC, MP3). Tambi\xc3\xa9n puedes arrastrarlo al plugin."));
    listen.setTooltip (u8 ("Escuchar la regi\xc3\xb3n original."));
    capture.onClick = [this] { proc.toggleCapture(); };
    import.onClick = [this] { openImport(); };
    listen.onClick = [this] { proc.setSourcePreview (! proc.isSourcePreviewing()); };
    sync.labelLeft = true;
    sync.setBounds (heroX + heroW - 20 - 160, heroY + 19, 160, 26);
    duration.plain = true;
    duration.setBounds (kW / 2 - 270, 350, 540, 34);
    kIntensity.setBounds (146, 404, 180, 176);
    kPitch.setBounds (360, 404, 180, 176);
    kCharacter.setBounds (574, 404, 180, 176);
    preview.setBounds (276, 587, 54, 54);
    reset.setBounds (570, 587, 54, 54);
    generate.setBounds (330, 576, 240, 76);
    preview.setTooltip (u8 ("Escuchar el reverse (barra espaciadora)."));
    reset.setTooltip (u8 ("Volver a los ajustes por defecto."));
    generate.setTooltip (u8 ("Generar el reverse. En AUTO se genera solo al tocar un control."));
    preview.onClick = [this] { proc.setPreview (! proc.isPreviewing()); };
    reset.onClick = [this] { proc.resetParameters(); };
    generate.onClick = [this] { proc.generate(); };
    const int cx = heroX + heroW / 2, rowY = heroY + heroH - 34;
    hitPrev.setBounds (cx - 92, rowY - 3, 30, 30);
    hitNext.setBounds (cx + 40, rowY - 3, 30, 30);
    hitSet.setBounds (cx + 70, rowY - 13, 76, 50);
    hitPrev.setTooltip (u8 ("Mover el golpe una negra antes."));
    hitNext.setTooltip (u8 ("Mover el golpe una negra despu\xc3\xa9s."));
    hitSet.setTooltip (u8 ("Poner el golpe donde est\xc3\xa1 el cursor del DAW."));
    hitPrev.onClick = [this] { proc.nudgeHit (-1.0); repaint(); };
    hitNext.onClick = [this] { proc.nudgeHit (1.0); repaint(); };
    hitSet.onClick = [this] { proc.setHitAtPlayhead(); repaint(); };
    durationRect = { heroX + heroW - 260, rowY, 240, 24 };

    // ---------------------------------------------------------------- CUSTOMIZE
    addChildComponent (lanes); addChildComponent (curve);
    lanes.setBounds (kW / 2 - 230, 94, 460, 36);
    curve.setBounds (44, 144, 812, 232);
    lanes.onChange = [this] (int l) { setLane (l); };
    auto row = [this] (int laneIdx, std::vector<juce::Component*> items)
    {
        const int colW = 128, total = colW * (int) items.size();
        int x = kW / 2 - total / 2;
        for (auto* c : items) { c->setBounds (x, 396, colW, 150); x += colW; laneItems[laneIdx].push_back (c); }
    };
    row (0, { knob ("START", id::volStart), knob ("END", id::volEnd), knob ("CURVE", id::volCurve), knob ("FADE IN", id::fadeIn), knob ("TRANSIENT", id::transient) });
    {
        auto e = std::make_unique<ParamChoice> (proc.apvts, id::ending, &proc.undo, 10.0f);
        ending = e.get();
        addChildComponent (*e);
        e->setBounds (kW / 2 - 160, 566, 320, 34);
        laneItems[0].push_back (e.get());
        owned.push_back (std::move (e));
    }
    row (1, { knob ("START", id::pitchStart), knob ("END", id::pitchEnd), knob ("CURVE", id::pitchCurve), knob ("MICRO", id::microPitch), knob ("PITCH", id::pitch) });
    row (2, { knob ("HP START", id::hpStart), knob ("HP END", id::hpEnd), knob ("LP START", id::lpStart), knob ("LP END", id::lpEnd), knob ("RESONANCE", id::resonance), knob ("CURVE", id::filterCurve) });
    laneCards[3].push_back ({ "MAIN LAYER", { 44, 396, 300, 200 }, toggle ("", id::mainOn), { knob ("LEVEL", id::mainLevel, 80.0f), knob ("WIDTH", id::mainWidth, 80.0f) } });
    laneCards[3].push_back ({ "TEXTURE LAYER", { 360, 396, 496, 200 }, toggle ("", id::texOn),
                              { knob ("LEVEL", id::texLevel, 80.0f), knob ("PITCH", id::texPitch, 80.0f), knob ("WIDTH", id::texWidth, 80.0f), knob ("SMEAR", id::texSmear, 80.0f) } });
    layoutCards (laneCards[3], 0, 0);
    for (auto& c : laneCards[3]) { laneItems[3].push_back (c.toggle); for (auto* i : c.items) laneItems[3].push_back (i); }

    // ---------------------------------------------------------------- FX
    addChildComponent (fxTabs);
    fxTabs.setBounds (kW / 2 - 320, 94, 640, 36);
    fxTabs.onChange = [this] (int c) { setFxPage (c); };
    const float D = 84.0f;
    fxCards[0] = { { "REVERSE WASH", { 44, 150, 190, 212 }, nullptr, { knob ("WASH", id::wash, D) } },
                   { "REVERB", { 250, 150, 606, 212 }, toggle ("", id::reverbOn),
                     { knob ("SIZE", id::revSize, D), knob ("DECAY", id::revDecay, D), knob ("PRE-DELAY", id::revPredelay, D), knob ("MIX", id::revMix, D), knob ("DAMPING", id::revDamp, D) } },
                   { "DELAY", { 44, 378, 812, 212 }, toggle ("", id::delayOn),
                     { knob ("TIME", id::delayTime, D), knob ("FEEDBACK", id::delayFb, D), knob ("MIX", id::delayMix, D), toggle ("PING-PONG", id::pingPong) } } };
    fxCards[1] = { { "TREMOLO", { 44, 196, 252, 330 }, toggle ("", id::tremOn), { knob ("RATE", id::tremRate, D), knob ("DEPTH", id::tremDepth, D) } },
                   { "PROGRESSIVE CUTS", { 310, 196, 290, 330 }, toggle ("", id::cutsOn),
                     { knob ("FROM", id::cutsFrom, D), knob ("TO", id::cutsTo, D), knob ("DEPTH", id::cutsDepth, D) } },
                   { "STUTTER", { 614, 196, 242, 330 }, toggle ("", id::stutterOn), { knob ("RATE", id::stutterRate, D), knob ("LENGTH", id::stutterLen, D) } } };
    fxCards[2] = { { "CHARACTER", { 44, 200, 812, 290 }, nullptr,
                     { knob ("DRIVE", id::drive, D), knob ("BITCRUSH", id::crush, D), knob ("NOISE", id::noise, D), knob ("HARMONICS", id::harmonics, D),
                       knob ("SUB BOOST", id::sub, D), knob ("AIR", id::air, D) } } };
    fxCards[3] = { { "STEREO", { 144, 200, 612, 290 }, nullptr, { knob ("WIDTH", id::width, D), knob ("AUTO-PAN", id::autoPan, D), knob ("PAN RATE", id::panRate, D) } } };
    fxCards[4] = { { "FX MOVEMENT", { 44, 200, 812, 290 }, nullptr,
                     { knob ("REVERB", id::moveReverb, D), knob ("DELAY", id::moveDelay, D), knob ("DRIVE", id::moveDrive, D), knob ("PAN", id::movePan, D),
                       knob ("FILTER", id::moveFilter, D), knob ("WIDTH", id::moveWidth, D), knob ("TREMOLO", id::moveTrem, D) } } };
    fxCards[5] = { { "ENDING & TAIL", { 44, 150, 812, 212 }, nullptr,
                     { toggle ("SMART ENDING", id::smartEnding), knob ("PRE-IMPACT", id::preImpact, D), knob ("TAIL", id::tail, D), knob ("OUT FADE", id::outFade, D), toggle ("CLIP SAFE", id::clipSafe) } },
                   { "PLAYBACK", { 44, 378, 812, 212 }, nullptr,
                     { toggle ("THROUGH", id::through), knob ("OUTPUT", id::output, D), knob ("DUCKING", id::ducking, D), knob ("RELEASE", id::duckRelease, D) } } };
    for (auto& cards : fxCards) layoutCards (cards, 0, 0);

    // ---------------------------------------------------------------- pie
    for (auto* c : std::initializer_list<juce::Component*> { &dragOut, &exportBtn, &genMode }) addAndMakeVisible (c);
    dragOut.hasIcon = false;
    dragOut.setBounds (32, 657, 178, 56);
    dragOut.setTooltip (u8 ("Arrastra el reverse a una pista del DAW. Su inicio cae en la rejilla: d\xc3\xa9jalo al principio del comp\xc3\xa1s y el golpe queda en su sitio."));
    dragOut.onDragOut = [this]
    {
        const auto f = proc.exportForDrag();
        if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    };
    dragOut.onClick = [this] { openExport(); };
    exportBtn.setBounds (212, 665, 40, 40);
    exportBtn.setTooltip ("Exportar WAV (24 bits, con marca en el golpe).");
    exportBtn.onClick = [this] { openExport(); };
    genMode.setBounds (kW / 2 - 80, 669, 160, 30);

    setPage (0);
    startTimerHz (30);
}

MainView::~MainView() { setLookAndFeel (nullptr); }

Knob* MainView::knob (const char* name, const char* pid, float d, bool compact)
{
    auto k = std::make_unique<Knob> (name, proc.apvts, pid, &proc.undo, d);
    k->compact = compact;
    auto* raw = k.get();
    addChildComponent (*k);
    owned.push_back (std::move (k));
    return raw;
}
ParamToggle* MainView::toggle (const char* name, const char* pid)
{
    auto t = std::make_unique<ParamToggle> (name, proc.apvts, pid, &proc.undo);
    auto* raw = t.get();
    addChildComponent (*t);
    owned.push_back (std::move (t));
    return raw;
}

void MainView::layoutCards (std::vector<Card>& cards, int, int)
{
    for (auto& c : cards)
    {
        const auto b = c.bounds;
        if (c.toggle != nullptr) c.toggle->setBounds (b.getRight() - 62, b.getY() + 14, 44, 26);
        const int n = (int) c.items.size();
        if (n == 0) continue;
        const int inner = b.getWidth() - 32;
        const int colW = std::min (130, inner / n);
        int x = b.getCentreX() - colW * n / 2;
        const int ky = b.getY() + std::max (52, (b.getHeight() - 150) / 2 + 18);   // la fila queda centrada en la tarjeta
        for (auto* it : c.items)
        {
            if (dynamic_cast<Toggle*> (it) != nullptr) it->setBounds (x + colW / 2 - 86, ky + 26, 172, 30);
            else it->setBounds (x, ky, colW, 150);
            x += colW;
        }
    }
}

void MainView::setPage (int p)
{
    page = p;
    const bool c = p == 0, cu = p == 1, fx = p == 2;
    for (auto* x : std::initializer_list<juce::Component*> { &wave, &capture, &import, &listen, &sync, &duration, &kIntensity, &kPitch, &kCharacter,
                                                            &preview, &reset, &generate, &hitPrev, &hitNext, &hitSet })
        x->setVisible (c);
    lanes.setVisible (cu); curve.setVisible (cu);
    for (int l = 0; l < 4; ++l) for (auto* x : laneItems[l]) x->setVisible (cu && l == lane);
    fxTabs.setVisible (fx);
    for (int k = 0; k < 6; ++k)
        for (auto& card : fxCards[k])
        {
            if (card.toggle) card.toggle->setVisible (fx && k == fxPage);
            for (auto* x : card.items) x->setVisible (fx && k == fxPage);
        }
    repaint();
}
void MainView::setLane (int l) { lane = l; lanes.setSelected (l); curve.setLane (l); setPage (page); }
void MainView::setFxPage (int c) { fxPage = c; fxTabs.setSelected (c); setPage (page); }

// ---------------------------------------------------------------------------- dibujo
void MainView::paint (juce::Graphics& g)
{
    // fondo: verdes de la marca IVO muy profundos, con textura
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBot, 0, (float) kH, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (glowA, 90, 30, glowA.withAlpha (0.0f), 90 + 520, 30, true)); g.fillAll();
    g.setGradientFill (juce::ColourGradient (glowB, 860, 640, glowB.withAlpha (0.0f), 860 + 560, 640, true)); g.fillAll();
    g.drawImageAt (grain, 0, 0);

    // marca IVO: nombre y línea (igual que en todos los plugins IVO)
    drawWordmark (g, { 44.0f, 18.0f }, "reverse", u8 ("REVERSE \xc2\xb7 RISE \xc2\xb7 TRANSITION"));

    // pestañas
    {
        const auto f = font (F::manX, 11.0f, 0.14f);
        float total = 0; for (auto* t : tabNames) total += textWidth (f, t);
        total += 2 * 30.0f;
        float x = kW * 0.5f - total * 0.5f;
        for (int i = 0; i < 3; ++i)
        {
            const float w = textWidth (f, tabNames[i]);
            tabRects[i] = juce::Rectangle<float> (x - 8, 28, w + 16, 30).toNearestInt();
            g.setColour (i == page ? text : dim); g.setFont (f);
            g.drawText (tabNames[i], juce::Rectangle<float> (x, 28, w + 4, 30), juce::Justification::centredLeft, false);
            x += w + 30.0f;
        }
    }
    // preset
    {
        presetRect = { kW - 44 - 236, 24, 236, 38 };
        const auto r = presetRect.toFloat();
        raised (g, r, r.getHeight() * 0.5f);
        drawIcon (g, Icon::left, { r.getX() + 10, r.getCentreY() - 7, 14, 14 }, soft);
        drawIcon (g, Icon::right, { r.getRight() - 24, r.getCentreY() - 7, 14, 14 }, soft);
        g.setColour (text); g.setFont (font (F::soraBold, 13.0f));
        g.drawText (proc.presetName, r.reduced (34, 0), juce::Justification::centred, true);
    }

    if (page == 0) paintCreate (g);
    else if (page == 1)
    {
        glass (g, curve.getBounds().toFloat(), 28.0f);
        static const char* hints[4] = { "VOLUME ENVELOPE", "PITCH EVOLUTION", "FILTER EVOLUTION", "LAYER ENGINE" };
        label (g, hints[lane], { 64.0f, 154.0f, 300.0f, 14.0f }, juce::Justification::centredLeft, dim, 9.5f, 0.24f);
        if (lane < 3) label (g, "DRAG THE POINTS", { 480.0f, 154.0f, 300.0f, 14.0f }, juce::Justification::centredRight, faint, 9.0f, 0.2f);
        if (lane == 3) paintCards (g, laneCards[3]);
        if (lane == 0) label (g, "ENDING", { kW / 2.0f - 300.0f, 566.0f, 130.0f, 34.0f }, juce::Justification::centredRight, dim, 9.5f, 0.24f);
    }
    else paintCards (g, fxCards[fxPage]);

    // pie: firma IVO abajo a la derecha (como en toda la familia)
    drawLogo (g, { kW - 44.0f - 48.5f, 674.0f, 48.5f, 22.0f });
}

void MainView::paintCards (juce::Graphics& g, const std::vector<Card>& cards)
{
    for (auto& c : cards)
    {
        const auto b = c.bounds.toFloat();
        glass (g, b, 24.0f);
        label (g, c.title, { b.getX() + 22.0f, b.getY() + 18.0f, b.getWidth() - (c.toggle != nullptr ? 90.0f : 40.0f), 20.0f }, juce::Justification::centredLeft, theme::text, 10.0f, 0.24f);
    }
}

void MainView::paintCreate (juce::Graphics& g)
{
    const auto hero = juce::Rectangle<float> ((float) heroX, (float) heroY, (float) heroW, (float) heroH);
    glass (g, hero, 30.0f);
    // estado y nombre de la fuente
    g.setColour (soft); g.setFont (font (F::man, 12.5f));
    if (proc.sourceName.isNotEmpty())
        g.drawText (proc.sourceName, juce::Rectangle<float> (hero.getX() + 146, hero.getY() + 22, 200, 24), juce::Justification::centredLeft, true);
    label (g, proc.statusText(), { hero.getCentreX() - 140, hero.getY() + 22, 280, 24 }, juce::Justification::centred,
           proc.isCapturing() ? rec : (proc.hasPendingChanges() ? accent : dim), 9.5f, 0.22f);
    // fila inferior: tempo, golpe y duración
    const float rowY = hero.getBottom() - 34;
    label (g, proc.tempoText(), { hero.getX() + 22, rowY, 220, 24 }, juce::Justification::centredLeft, dim, 9.5f, 0.2f);
    g.setColour (proc.hasHit() ? text : dim); g.setFont (font (F::soraBold, 13.0f));
    g.drawText (proc.hitText(), juce::Rectangle<float> (hero.getCentreX() - 60, rowY, 100, 24), juce::Justification::centred, false);
    const auto st = readSettings (proc.apvts);
    const bool free = st.durationIndex == 5 || ! st.smartSync || proc.hostBpm() <= 0.0;
    g.setColour (text); g.setFont (font (F::soraBold, 13.0f));
    g.drawText (proc.durationText(), durationRect.toFloat(), juce::Justification::centredRight, false);
    if (free) label (g, "DRAG", durationRect.toFloat().translated (-150, 0).withWidth (60), juce::Justification::centredRight, faint, 8.5f, 0.2f);
}

// ---------------------------------------------------------------------------- ratón y teclado
void MainView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.getPosition();
    for (int i = 0; i < 3; ++i) if (tabRects[i].contains (p)) { setPage (i); return; }
    if (presetRect.contains (p))
    {
        const int idx = proc.presets.indexOf (proc.presetName), n = (int) proc.presets.all().size();
        if (p.x < presetRect.getX() + 34) { proc.loadPreset ((idx - 1 + n) % n); repaint(); }
        else if (p.x > presetRect.getRight() - 34) { proc.loadPreset ((idx + 1) % n); repaint(); }
        else showPresetMenu();
        return;
    }
    if (page == 0 && durationRect.expanded (60, 4).contains (p))
    {
        draggingFree = true; freeDragY = p.y;
        freeDragStart = proc.apvts.getRawParameterValue (id::freeTime)->load();
        proc.undo.beginNewTransaction();
        proc.apvts.getParameter (id::freeTime)->beginChangeGesture();
    }
}
void MainView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingFree) return;
    auto* prm = proc.apvts.getParameter (id::freeTime);
    const float v = freeDragStart * std::pow (2.0f, (float) (freeDragY - e.getPosition().y) / 120.0f);
    prm->setValueNotifyingHost (prm->convertTo0to1 (juce::jlimit (0.1f, 12.0f, v)));
    // al arrastrar la duración, pasa a FREE (duración libre)
    auto* d = proc.apvts.getParameter (id::duration);
    if ((int) std::lround (d->convertFrom0to1 (d->getValue())) != 5 && std::abs (freeDragY - e.getPosition().y) > 3)
        d->setValueNotifyingHost (d->convertTo0to1 (5.0f));
    repaint();
}
void MainView::mouseUp (const juce::MouseEvent&)
{
    if (draggingFree) proc.apvts.getParameter (id::freeTime)->endChangeGesture();
    draggingFree = false;
}

bool MainView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { proc.setPreview (! proc.isPreviewing()); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undo.redo(); return true; }
    return false;
}

bool MainView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}
void MainView::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { proc.importFile (juce::File (f)); setPage (0); return; }
}

// ---------------------------------------------------------------------------- menús y archivos
void MainView::showPresetMenu()
{
    proc.presets.refreshUser();
    juce::PopupMenu m;
    const auto& all = proc.presets.all();
    juce::String cat;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        if (all[(size_t) i].category != cat) { cat = all[(size_t) i].category; m.addSectionHeader (cat); }
        m.addItem (i + 1, all[(size_t) i].name, true, all[(size_t) i].name == proc.presetName);
    }
    m.addSeparator();
    m.addItem (10000, u8 ("Guardar preset\xe2\x80\xa6"));
    m.addItem (10001, "Init");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetRect)).withMinimumWidth (presetRect.getWidth()),
                     [this] (int r)
                     {
                         if (r <= 0) return;
                         if (r == 10001) { proc.resetParameters(); repaint(); return; }
                         if (r == 10000)
                         {
                             auto* w = new juce::AlertWindow ("Guardar preset", "Nombre del preset:", juce::MessageBoxIconType::NoIcon, this);
                             w->addTextEditor ("name", proc.presetName == "Init" ? juce::String() : proc.presetName);
                             w->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
                             w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                             w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int res)
                             {
                                 if (res == 1) proc.saveUserPreset (w->getTextEditorContents ("name"));
                                 repaint();
                             }), true);
                             return;
                         }
                         proc.loadPreset (r - 1);
                         repaint();
                     });
}

void MainView::openImport()
{
    chooser = std::make_unique<juce::FileChooser> ("Importar voz", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile()) { proc.importFile (fc.getResult()); repaint(); }
    });
}

void MainView::openExport()
{
    if (proc.getResult() == nullptr) return;
    chooser = std::make_unique<juce::FileChooser> ("Exportar WAV", proc.defaultExportFile(), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        juce::String err;
        if (! proc.exportWav (f.withFileExtension ("wav"), err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "reverseIVO", err);
    });
}

// ---------------------------------------------------------------------------- refresco
void MainView::timerCallback()
{
    capture.setActive (proc.isCapturing());
    preview.setIcon (proc.isPreviewing() ? Icon::stop : Icon::play);
    listen.setActive (proc.isSourcePreviewing());
    const bool manual = proc.apvts.getRawParameterValue (id::genMode)->load() > 0.5f;
    generate.setPending (manual && proc.hasPendingChanges());
    if (page == 0) wave.tick();
    if (page == 1) curve.tick();
    const auto status = proc.statusText() + "|" + proc.hitText() + "|" + proc.durationText() + "|" + proc.tempoText() + "|" + proc.sourceName;
    if (status != lastStatus) { lastStatus = status; if (page == 0) repaint (juce::Rectangle<int> (heroX, heroY, heroW, heroH)); }
    if (proc.presetName != lastPreset) { lastPreset = proc.presetName; repaint (presetRect); }
}
} // namespace ui

// =================================================================== Editor
ReverseIvoEditor::ReverseIvoEditor (ReverseIvoProcessor& p) : AudioProcessorEditor (&p), proc (p), view (p)
{
    using V = ui::MainView;
    const float s = juce::jlimit (0.7f, 2.0f, proc.uiScale.load());
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (V::kW * 7 / 10, V::kH * 7 / 10, V::kW * 2, V::kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) V::kW / (double) V::kH);
    setSize (juce::roundToInt (V::kW * s), juce::roundToInt (V::kH * s));
}
ReverseIvoEditor::~ReverseIvoEditor() = default;
void ReverseIvoEditor::resized()
{
    const float s = (float) getWidth() / (float) ui::MainView::kW;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, ui::MainView::kW, ui::MainView::kH);
    proc.uiScale = s;
}
} // namespace reverseivo
