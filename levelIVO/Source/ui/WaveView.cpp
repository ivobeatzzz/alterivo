#include "WaveView.h"

namespace levelivo::ui
{
using namespace levelivo::theme;

static constexpr float kLeft = 66.0f, kRightScale = 44.0f;

// ============================================================================ Inspector
Inspector::Inspector()
{
    for (auto* b : { &lockB, &exprB, &resetB }) { b->fontSize = 7.5f; addAndMakeVisible (b); }
    lockB.onClick = [this] { if (onLock) onLock(); };
    exprB.onClick = [this] { if (onExpression) onExpression(); };
    resetB.onClick = [this] { if (onReset) onReset(); };
    lockB.setTooltip (u8 ("Fija la ganancia actual de este tramo: los cambios de controles ya no le afectan."));
    exprB.setTooltip (u8 ("Es expresi\xc3\xb3n (un grito o susurro a prop\xc3\xb3sito): no se corrige frente a su frase."));
    resetB.setTooltip (u8 ("Quita tus ediciones en este tramo."));
}
void Inspector::set (juce::String t, juce::String k, std::vector<std::pair<juce::String, juce::String>> r)
{
    title = std::move (t); kind = std::move (k); rows = std::move (r);
    setSize (216, 78 + 17 * (int) rows.size());
    repaint();
}
void Inspector::resized()
{
    auto r = getLocalBounds().reduced (12, 10).removeFromBottom (24);
    const int w = (r.getWidth() - 12) / 3;
    lockB.setBounds (r.removeFromLeft (w)); r.removeFromLeft (6);
    exprB.setBounds (r.removeFromLeft (w)); r.removeFromLeft (6);
    resetB.setBounds (r);
}
void Inspector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::Path p; p.addRoundedRectangle (r.reduced (2.0f), 12.0f);
    cachedShadow (g, p, juce::Colours::black.withAlpha (0.45f), 14, { 0, 6 });
    g.setColour (card); g.fillPath (p);
    auto in = r.reduced (14.0f, 11.0f);
    caps (g, kind, in.removeFromTop (12.0f), juce::Justification::centredLeft, accent, 8.0f, 0.2f);
    g.setColour (ink); g.setFont (font (F::soraSemi, 15.0f));
    g.drawText (title, in.removeFromTop (24.0f), juce::Justification::centredLeft, true);
    for (auto& [k, v] : rows)
    {
        auto row = in.removeFromTop (17.0f);
        g.setColour (juce::Colour (0xff5b6852)); g.setFont (font (F::man, 10.5f));
        g.drawText (k, row, juce::Justification::centredLeft, false);
        g.setColour (v.startsWith ("+") ? deep : ink);
        g.drawText (v, row, juce::Justification::centredRight, false);
    }
}

// ============================================================================ WaveView
WaveView::WaveView (LevelIvoProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    addChildComponent (inspector);
    inspector.onLock = [this] { lockSelection(); };
    inspector.onExpression = [this] { expressionSelection(); };
    inspector.onReset = [this] { resetSelection(); };
    for (auto* b : { &undoB, &redoB, &fitB, &playB }) { b->fontSize = 8.0f; addAndMakeVisible (b); }
    undoB.fontSize = redoB.fontSize = 11.0f;
    undoB.setTooltip (u8 ("Deshacer (Cmd/Ctrl+Z)")); redoB.setTooltip (u8 ("Rehacer"));
    fitB.setTooltip (u8 ("Ver la toma entera (doble clic en un hueco hace lo mismo)"));
    playB.setTooltip (u8 ("Escuchar la toma desde el plugin (para archivos arrastrados o con el transporte parado). Barra espaciadora."));
    undoB.onClick = [this] { proc.undo.undo(); };
    redoB.onClick = [this] { proc.undo.redo(); };
    fitB.onClick = [this] { zoomToFit(); };
    playB.onClick = [this]
    {
        if (proc.isPreviewing()) proc.setPreview (false);
        else
        {
            int from = (int) viewStart;
            if (! sel.empty()) from = sel.front().a;
            proc.setPreview (true, from);
        }
    };
    startTimerHz (30);
}
WaveView::~WaveView() = default;

juce::Rectangle<float> WaveView::screenArea() const { return getLocalBounds().toFloat(); }
juce::Rectangle<float> WaveView::waveArea() const
{
    const auto r = screenArea();
    return { kLeft, 72.0f, r.getWidth() - kLeft - kRightScale, r.getHeight() - 72.0f - 36.0f };
}
juce::Rectangle<float> WaveView::laneArea (int lane) const
{
    const auto w = waveArea();
    const float ys[] = { 30.0f, 44.0f, 57.0f }, hs[] = { 9.0f, 8.0f, 6.0f };
    return { w.getX(), ys[lane], w.getWidth(), hs[lane] };
}
juce::Rectangle<float> WaveView::miniArea() const
{
    const auto r = screenArea(); const auto w = waveArea();
    return { w.getX(), r.getHeight() - 26.0f, w.getWidth() - 236.0f, 14.0f };
}
void WaveView::resized()
{
    const auto m = miniArea();
    int x = (int) m.getRight() + 12; const int y = (int) m.getY() - 5, h = 22;
    undoB.setBounds (x, y, 28, h); x += 32;
    redoB.setBounds (x, y, 28, h); x += 36;
    fitB.setBounds (x, y, 64, h); x += 68;
    playB.setBounds (x, y, 72, h);
}

float WaveView::xOf (double s) const { const auto w = waveArea(); return w.getX() + (float) ((s - viewStart) / viewLen) * w.getWidth(); }
double WaveView::sampleAt (float x) const { const auto w = waveArea(); return viewStart + (x - w.getX()) / w.getWidth() * viewLen; }
float WaveView::yOfDb (float db) const
{
    const auto w = waveArea();
    return w.getCentreY() - db * (w.getHeight() * 0.5f - 10.0f) / 15.0f;
}
float WaveView::curveDbAt (int f) const
{
    if (res == nullptr || res->level == nullptr || res->level->gainDb.empty()) return 0.0f;
    return res->level->gainDb[(size_t) std::clamp (f, 0, (int) res->level->gainDb.size() - 1)];
}

void WaveView::zoomToFit()
{
    if (res == nullptr || res->take == nullptr) return;
    viewStart = 0; viewLen = std::max (1000, res->take->numSamples());
    repaint();
}
void WaveView::zoomBy (float factor, float atX)
{
    if (res == nullptr || res->take == nullptr) return;
    const double total = res->take->numSamples();
    const double at = sampleAt (atX);
    const double minLen = res->take->sampleRate * 0.25;
    viewLen = juce::jlimit (minLen, std::max (minLen, total), viewLen * factor);
    viewStart = juce::jlimit (0.0, std::max (0.0, total - viewLen), at - (atX - waveArea().getX()) / waveArea().getWidth() * viewLen);
    repaint();
}

void WaveView::rebuild()
{
    const bool newTake = res == nullptr || res->take == nullptr;
    auto prevTake = res != nullptr ? res->take : nullptr;
    res = proc.getResult();
    resVersion = res != nullptr ? res->version : -1;
    pkOrigMax.clear(); pkOrigMin.clear(); pkOutMax.clear(); pkOutMin.clear(); miniPk.clear();
    if (res == nullptr || res->take == nullptr) { inspector.setVisible (false); sel.clear(); repaint(); return; }
    const auto& T = *res->take;
    const int n = T.numSamples(), nb = n / kBlock + 1, nc = T.numChannels();
    pkOrigMax.assign ((size_t) nb, 0.0f); pkOrigMin.assign ((size_t) nb, 0.0f); pkOutMax.assign ((size_t) nb, 0.0f); pkOutMin.assign ((size_t) nb, 0.0f);
    for (int b = 0; b < nb; ++b)
    {
        float omx = 0, omn = 0, fmx = 0, fmn = 0;
        const int a = b * kBlock, e = std::min (n, a + kBlock);
        for (int i = a; i < e; ++i)
            for (int c = 0; c < nc; ++c)
            {
                const float o = T.audio.getSample (c, i), f = res->sample (Listen::final_, c, i);
                omx = std::max (omx, o); omn = std::min (omn, o); fmx = std::max (fmx, f); fmn = std::min (fmn, f);
            }
        pkOrigMax[(size_t) b] = omx; pkOrigMin[(size_t) b] = omn; pkOutMax[(size_t) b] = fmx; pkOutMin[(size_t) b] = fmn;
    }
    const int mb = 600;
    miniPk.assign ((size_t) mb, 0.0f);
    for (int b = 0; b < nb; ++b) { const int k = (int) ((juce::int64) b * mb / nb); miniPk[(size_t) k] = std::max (miniPk[(size_t) k], std::max (pkOrigMax[(size_t) b], -pkOrigMin[(size_t) b])); }
    if (newTake || prevTake != res->take) { sel.clear(); zoomToFit(); }
    updateInspector();
    repaint();
}

void WaveView::timerCallback()
{
    auto r = proc.getResult();
    if (r != res || (r != nullptr && r->version != resVersion)) rebuild();
    const auto pp = proc.playTakePos();
    if (pp != lastPlay)
    {
        lastPlay = pp;
        // seguir la reproducción: si sale de la vista, se pasa página
        if (pp >= 0 && res != nullptr && ! draggingCurve && ! draggingMini && (pp < viewStart || pp > viewStart + viewLen))
            viewStart = juce::jlimit (0.0, std::max (0.0, (double) res->take->numSamples() - viewLen), (double) pp - viewLen * 0.1);
        repaint();
    }
    if (viewStart != shownStart || viewLen != shownLen) { shownStart = viewStart; shownLen = viewLen; updateInspector(); }
    playB.text = proc.isPreviewing() ? "PARAR" : "ESCUCHAR";
    playB.active = proc.isPreviewing();
    playB.repaint();
    undoB.enabled = proc.undo.canUndo(); redoB.enabled = proc.undo.canRedo();
    undoB.repaint(); redoB.repaint();
    if (res == nullptr || proc.isCapturing()) repaint();
}

// ---------------------------------------------------------------------------- dibujo
void WaveView::paint (juce::Graphics& g)
{
    const auto r = screenArea();
    g.setColour (scr); g.fillRoundedRectangle (r, 14.0f);
    g.setColour (scrEdge); g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    if (res == nullptr || res->take == nullptr || res->analysis == nullptr) { paintEmpty (g, r); return; }
    paintWave (g);
    paintMini (g);
}

void WaveView::paintEmpty (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto c = r.reduced (40.0f);
    if (proc.isCapturing())
    {
        const double s = proc.capturedSeconds();
        g.setColour (red); g.fillEllipse (c.getCentreX() - 6.0f, c.getCentreY() - 52.0f, 12.0f, 12.0f);
        g.setColour (juce::Colour (0xffecf4e2)); g.setFont (font (F::soraSemi, 22.0f));
        g.drawText (juce::String::formatted ("%d:%02d", (int) s / 60, (int) s % 60), c.withHeight (30.0f).withCentre ({ c.getCentreX(), c.getCentreY() - 12.0f }), juce::Justification::centred);
        caps (g, u8 ("Capturando \xc2\xb7 cuando pares el transporte se analiza solo"), c.withHeight (16.0f).withCentre ({ c.getCentreX(), c.getCentreY() + 18.0f }), juce::Justification::centred, scrText, 9.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (std::max (proc.inLevel(), 1e-5f)) + 60.0f) / 60.0f);
        auto bar = juce::Rectangle<float> (220.0f, 4.0f).withCentre ({ c.getCentreX(), c.getCentreY() + 44.0f });
        g.setColour (scrLane); g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (lime); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lv), 2.0f);
        return;
    }
    const auto st = proc.engine.stage();
    if (st != Engine::Stage::idle)
    {
        g.setColour (juce::Colour (0xffecf4e2)); g.setFont (font (F::soraSemi, 18.0f));
        g.drawText (proc.statusText(), c.withHeight (30.0f).withCentre (c.getCentre()), juce::Justification::centred);
        auto bar = juce::Rectangle<float> (260.0f, 4.0f).withCentre ({ c.getCentreX(), c.getCentreY() + 28.0f });
        g.setColour (scrLane); g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (lime); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.engine.progress()), 2.0f);
        return;
    }
    g.setColour (juce::Colour (0xffecf4e2)); g.setFont (font (F::soraSemi, 19.0f));
    g.drawText (u8 ("Prepara la toma"), c.withHeight (28.0f).withCentre ({ c.getCentreX(), c.getCentreY() - 46.0f }), juce::Justification::centred);
    const char* steps[] = { "1 \xc2\xb7 Pulsa CAPTURAR", "2 \xc2\xb7 Dale al play en la parte que quieras (o en toda la canci\xc3\xb3n)",
                            "3 \xc2\xb7 Para el transporte: levelIVO analiza, limpia y nivela",
                            "Tambi\xc3\xa9n puedes arrastrar aqu\xc3\xad un WAV. Para el sangrado de cascos, manda el beat al sidechain." };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (i < 3 ? juce::Colour (0xffc8d3bd) : scrText); g.setFont (font (F::man, i < 3 ? 13.0f : 11.0f));
        g.drawText (u8 (steps[i]), c.withHeight (20.0f).withCentre ({ c.getCentreX(), c.getCentreY() - 10.0f + 24.0f * i + (i == 3 ? 10.0f : 0.0f) }), juce::Justification::centred);
    }
}

void WaveView::paintWave (juce::Graphics& g)
{
    const auto& T = *res->take; const auto& A = *res->analysis;
    const auto* L = res->level.get(); const auto* C = res->clean.get();
    const auto w = waveArea();
    const int hop = A.hop, n = T.numSamples();
    const double sr = T.sampleRate;
    const double v0 = viewStart, v1 = viewStart + viewLen;
    auto segX = [&] (int fa, int fb) { return juce::Rectangle<float> (xOf ((double) fa * hop), 0, xOf ((double) fb * hop) - xOf ((double) fa * hop), 0); };

    // regla de tiempo
    {
        const double secs = viewLen / sr;
        const double steps[] = { 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120 };
        double st = 120; for (double s : steps) if (secs / s <= 11) { st = s; break; }
        g.setFont (font (F::man, 9.0f));
        for (double t = std::ceil (v0 / sr / st) * st; t * sr < v1; t += st)
        {
            const float x = xOf (t * sr);
            g.setColour (scrGrid); g.fillRect (x, 24.0f, 1.0f, w.getBottom() - 24.0f);
            // tiempo del proyecto si la toma viene de la línea de tiempo
            juce::int64 hp = T.hostPosFor ((int) (t * sr));
            const double shown = hp >= 0 ? (double) hp / sr : t;
            g.setColour (scrText);
            g.drawText (juce::String::formatted ("%d:%04.1f", (int) shown / 60, std::fmod (shown, 60.0)).replace (".", ","), juce::Rectangle<float> (x - 30, 8, 60, 12), juce::Justification::centred);
        }
    }
    // nombres de carriles y escala
    caps (g, "Frases", { 10, 28, 56, 12 }, juce::Justification::centredLeft, scrText, 7.5f, 0.18f);
    caps (g, "Palabras", { 10, 42, 56, 12 }, juce::Justification::centredLeft, scrText, 7.5f, 0.18f);
    caps (g, u8 ("S\xc3\xadlabas"), { 10, 55, 56, 12 }, juce::Justification::centredLeft, scrText, 7.5f, 0.18f);
    g.setFont (font (F::man, 9.0f));
    for (int db : { 12, 6, 0, -6, -12 })
    {
        const float y = yOfDb ((float) db);
        g.setColour (lime.withAlpha (db == 0 ? 0.16f : 0.07f)); g.fillRect (w.getX(), y, w.getWidth(), 1.0f);
        g.setColour (scrText);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (w.getRight() + 8, y - 6, 34, 12), juce::Justification::centredLeft);
    }
    caps (g, "dB", { w.getRight() + 8, w.getY() - 2, 30, 12 }, juce::Justification::centredLeft, scrText, 8.0f);

    g.saveState();
    g.reduceClipRegion (juce::Rectangle<float> (w.getX(), 24.0f, w.getWidth(), w.getBottom() - 24.0f).toNearestInt());
    const int fa = std::max (0, (int) (v0 / hop) - 1), fb = std::min (A.nFrames, (int) (v1 / hop) + 2);

    // zonas: respiraciones, otras voces, baja confianza, selección
    if (layers.breaths)
        for (const auto& b : A.breaths) if (b.b >= fa && b.a <= fb)
        {
            auto x = segX (b.a, b.b);
            g.setColour (teal.withAlpha (0.10f)); g.fillRect (x.getX(), w.getY(), x.getWidth(), w.getHeight());
        }
    for (const auto& o : A.others) if (o.b >= fa && o.a <= fb)
    {
        auto x = segX (o.a, o.b);
        g.setColour (amber.withAlpha (0.07f)); g.fillRect (x.getX(), w.getY(), x.getWidth(), w.getHeight());
    }
    if (layers.lowConf)
        for (const auto& wd : A.words) if (wd.conf < 0.5f && wd.b >= fa && wd.a <= fb)
        {
            auto x = segX (wd.a, wd.b);
            g.saveState(); g.reduceClipRegion (juce::Rectangle<float> (x.getX(), w.getY(), x.getWidth(), w.getHeight()).toNearestInt());
            g.setColour (juce::Colour (0x389aa58f));
            for (float xx = x.getX() - w.getHeight(); xx < x.getRight(); xx += 7.0f) g.drawLine (xx, w.getBottom(), xx + w.getHeight(), w.getY(), 1.0f);
            g.restoreState();
        }
    for (const auto& s : sel)
    {
        const float x0 = xOf (s.a), x1 = xOf (s.b);
        g.setColour (lime.withAlpha (0.10f)); g.fillRect (x0, w.getY(), x1 - x0, w.getHeight());
    }

    // forma de onda: original (gris) detrás, resultado (claro) delante
    {
        float pk = 0.05f; for (size_t b = 0; b < pkOrigMax.size(); ++b) pk = std::max (pk, std::max (pkOrigMax[b], -pkOrigMin[b]));
        const float amp = (w.getHeight() * 0.5f - 6.0f) / pk;
        const float cy = w.getCentreY();
        const int px0 = (int) w.getX(), px1 = (int) w.getRight();
        const double spp = viewLen / w.getWidth();
        juce::RectangleList<float> ro, rf;
        for (int x = px0; x < px1; ++x)
        {
            const double s0 = sampleAt ((float) x), s1 = sampleAt ((float) x + 1.0f);
            if (s1 <= 0 || s0 >= n) continue;
            float omx = 0, omn = 0, fmx = 0, fmn = 0;
            if (spp >= kBlock)
            {
                const int b0 = std::max (0, (int) (s0 / kBlock)), b1 = std::min ((int) pkOrigMax.size(), (int) (s1 / kBlock) + 1);
                for (int b = b0; b < b1; ++b) { omx = std::max (omx, pkOrigMax[(size_t) b]); omn = std::min (omn, pkOrigMin[(size_t) b]); fmx = std::max (fmx, pkOutMax[(size_t) b]); fmn = std::min (fmn, pkOutMin[(size_t) b]); }
            }
            else
            {
                const int a = std::max (0, (int) s0), e = std::min (n, std::max (a + 1, (int) s1));
                for (int i = a; i < e; ++i)
                    for (int c = 0; c < T.numChannels(); ++c)
                    {
                        const float o = T.audio.getSample (c, i), f = res->sample (Listen::final_, c, i);
                        omx = std::max (omx, o); omn = std::min (omn, o); fmx = std::max (fmx, f); fmn = std::min (fmn, f);
                    }
            }
            ro.addWithoutMerging ({ (float) x, cy - omx * amp, 1.0f, std::max (1.0f, (omx - omn) * amp) });
            rf.addWithoutMerging ({ (float) x + 0.15f, cy - fmx * amp * 0.92f, 0.7f, std::max (1.0f, (fmx - fmn) * amp * 0.92f) });
        }
        g.setColour (waveOrig); g.fillRectList (ro);
        g.setColour (waveOut); g.fillRectList (rf);
    }

    // carriles: frases, palabras, sílabas
    std::vector<int> selIdx[3];
    for (auto& s : sel) if (s.index >= 0) selIdx[s.kind].push_back (s.index);
    auto isSel = [&] (int k, int i) { return std::find (selIdx[k].begin(), selIdx[k].end(), i) != selIdx[k].end(); };
    if (layers.phrases)
        for (int i = 0; i < (int) A.phrases.size(); ++i)
        {
            const auto& p = A.phrases[(size_t) i]; if (p.b < fa || p.a > fb) continue;
            auto x = segX (p.a, p.b); const auto ln = laneArea (0);
            g.setColour (isSel (0, i) ? lime : phraseC); g.fillRoundedRectangle (x.getX(), ln.getY(), std::max (2.0f, x.getWidth()), ln.getHeight(), 4.0f);
        }
    if (layers.words)
        for (int i = 0; i < (int) A.words.size(); ++i)
        {
            const auto& wd = A.words[(size_t) i]; if (wd.b < fa || wd.a > fb) continue;
            auto x = segX (wd.a, wd.b); const auto ln = laneArea (1);
            juce::Colour c = isSel (1, i) ? lime : wordC.withAlpha (0.45f + 0.55f * wd.conf);
            if (L != nullptr && i < (int) L->words.size() && L->words[(size_t) i].locked) c = juce::Colour (0xff9fb7e0);
            if (L != nullptr && i < (int) L->words.size() && L->words[(size_t) i].expression) c = amber.withAlpha (0.85f);
            g.setColour (c); g.fillRoundedRectangle (x.getX(), ln.getY(), std::max (2.0f, x.getWidth() - 1.0f), ln.getHeight(), 3.0f);
        }
    if (layers.syllables)
        for (int i = 0; i < (int) A.syllables.size(); ++i)
        {
            const auto& s = A.syllables[(size_t) i]; if (s.b < fa || s.a > fb) continue;
            auto x = segX (s.a, s.b); const auto ln = laneArea (2);
            g.setColour (isSel (2, i) ? lime.withAlpha (0.8f) : scrLane); g.fillRoundedRectangle (x.getX() + 0.5f, ln.getY(), std::max (1.5f, x.getWidth() - 1.5f), ln.getHeight(), 2.0f);
        }

    // respiraciones (etiqueta), golpes, clics, clipping, consonantes protegidas, ruido
    g.setFont (font (F::manX, 7.5f, 0.12f));
    if (layers.breaths)
        for (const auto& b : A.breaths) if (b.b >= fa && b.a <= fb)
        {
            auto x = segX (b.a, b.b);
            const float red2 = C != nullptr && b.a < (int) C->gainDb.size() ? C->gainDb[(size_t) ((b.a + b.b) / 2)] : 0.0f;
            g.setColour (teal);
            g.drawText (red2 < -0.5f ? "RESP " + juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String (juce::roundToInt (-red2)) + " dB" : juce::String ("RESP"),
                        juce::Rectangle<float> (x.getCentreX() - 40, w.getCentreY() + 26, 80, 10), juce::Justification::centred);
        }
    if (layers.noise)
    {
        for (const auto& h : A.hits) if (h.b / hop >= fa && h.a / hop <= fb)
        {
            const float x = xOf (h.a);
            g.setColour (amber); g.fillEllipse (x - 3.5f, w.getBottom() - 30.0f, 7.0f, 7.0f);
            g.drawText ("GOLPE", juce::Rectangle<float> (x + 6, w.getBottom() - 32.0f, 50, 10), juce::Justification::centredLeft);
        }
        if (C != nullptr)
            for (const auto& c : C->fixedClicks) if (c.b / hop >= fa && c.a / hop <= fb)
            {
                const float x = xOf (c.a);
                g.setColour (amber.withAlpha (0.9f)); g.fillRect (x - 0.75f, w.getBottom() - 14.0f, 1.5f, 8.0f);
            }
        // reducción de ruido / sangrado: franja inferior
        if (C != nullptr && ! C->reductionDb.empty())
            for (int f = fa; f < fb && f < (int) C->reductionDb.size(); ++f)
            {
                const float rdb = -C->reductionDb[(size_t) f];
                if (rdb < 1.0f) continue;
                const float x0 = xOf ((double) f * hop), x1 = xOf ((double) (f + 1) * hop);
                g.setColour (amber.withAlpha (juce::jlimit (0.05f, 0.5f, rdb / 24.0f))); g.fillRect (x0, w.getBottom() - 4.0f, std::max (1.0f, x1 - x0), 4.0f);
            }
    }
    if (layers.clipping)
        for (const auto& c : A.clips) if (c.b / hop >= fa && c.a / hop <= fb)
        {
            const float x0 = xOf (c.a), x1 = std::max (x0 + 3.0f, xOf (c.b));
            g.setColour (red.withAlpha (c.repairable ? 0.9f : 1.0f));
            g.fillRect (x0, w.getY() + 2.0f, x1 - x0, 4.0f); g.fillRect (x0, w.getBottom() - 10.0f, x1 - x0, 4.0f);
            if (viewLen / sr < 20.0) g.drawText (c.repairable ? "CLIP" : u8 ("CLIP \xe2\x9c\x95"), juce::Rectangle<float> (x0 - 10, w.getY() + 8, 50, 10), juce::Justification::centredLeft);
        }
    if (layers.consonants)
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        for (int o : A.onsets) if (o >= fa && o <= fb) g.fillRect (xOf ((double) o * hop), w.getBottom() + 2.0f, 1.2f, 5.0f);
    }
    // cortes entre regiones capturadas por separado
    for (const auto& rg : T.regions)
        if (rg.takeStart > 0)
        {
            const float x = xOf (rg.takeStart);
            g.setColour (scrText.withAlpha (0.6f));
            for (float y = w.getY(); y < w.getBottom(); y += 6.0f) g.fillRect (x, y, 1.0f, 3.0f);
        }

    // curva de ganancia (la real) y vista previa al arrastrar
    if (layers.curve && L != nullptr)
    {
        juce::Path p; bool started = false;
        const int px0 = (int) w.getX(), px1 = (int) w.getRight();
        for (int x = px0; x <= px1; x += 1)
        {
            const double s = sampleAt ((float) x);
            if (s < 0 || s >= n) { started = false; continue; }
            float db = curveDbAt ((int) (s / hop));
            if (draggingCurve) for (auto& sl : sel) if (s >= sl.a && s < sl.b) db += dragDb;
            const float y = juce::jlimit (w.getY(), w.getBottom(), yOfDb (db));
            if (! started) { p.startNewSubPath ((float) x, y); started = true; } else p.lineTo ((float) x, y);
        }
        g.setColour (lime); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
        // puntos de edición en la selección
        for (auto& s : sel)
        {
            for (double t : { (double) s.a, 0.5 * (s.a + s.b), (double) s.b })
            {
                const float x = xOf (t);
                if (x < w.getX() || x > w.getRight()) continue;
                const float y = yOfDb (curveDbAt ((int) (t / hop)) + (draggingCurve ? dragDb : 0.0f));
                g.setColour (scr); g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
                g.setColour (lime); g.drawEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f, 2.0f);
            }
        }
        if (draggingCurve)
        {
            g.setColour (lime); g.setFont (font (F::soraSemi, 13.0f));
            const auto& s = sel.front();
            g.drawText ((dragDb >= 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (dragDb), 1).replace (".", ",") + " dB",
                        juce::Rectangle<float> (xOf (s.a), yOfDb (meanGainDb (s.a, s.b) + dragDb) - 26.0f, 80, 16), juce::Justification::centredLeft);
        }
    }
    // cabezal
    const auto pp = proc.playTakePos();
    if (pp >= 0 && pp >= v0 && pp <= v1)
    {
        const float x = xOf ((double) pp);
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.fillRect (x - 0.5f, 24.0f, 1.5f, w.getBottom() - 24.0f);
    }
    g.restoreState();

    if (proc.trackChanged())
    {
        auto b = juce::Rectangle<float> (w.getX() + 10, w.getY() + 6, 430, 22);
        g.setColour (amber.withAlpha (0.92f)); g.fillRoundedRectangle (b, 6.0f);
        g.setColour (ink); g.setFont (font (F::man, 11.0f));
        g.drawText (u8 ("La pista no coincide con la captura: suena sin procesar. Vuelve a capturar."), b.reduced (10, 0), juce::Justification::centredLeft);
    }
    if (hoverText.isNotEmpty())
    {
        g.setColour (scrText); g.setFont (font (F::man, 9.5f));
        g.drawText (hoverText, juce::Rectangle<float> (12.0f, w.getBottom() + 8.0f, 54.0f, 12.0f), juce::Justification::centredLeft);
    }
}

void WaveView::paintMini (juce::Graphics& g)
{
    const auto m = miniArea();
    g.setColour (juce::Colour (0xff1a2614)); g.fillRoundedRectangle (m, 4.0f);
    if (! miniPk.empty())
    {
        float pk = 0.05f; for (float v : miniPk) pk = std::max (pk, v);
        g.setColour (juce::Colour (0xff4c6440));
        const float bw = m.getWidth() / (float) miniPk.size();
        for (size_t i = 0; i < miniPk.size(); ++i)
        {
            const float h = std::max (1.0f, miniPk[i] / pk * (m.getHeight() - 4.0f));
            g.fillRect (m.getX() + i * bw, m.getCentreY() - h * 0.5f, std::max (1.0f, bw - 0.3f), h);
        }
    }
    const double total = std::max (1, res->take->numSamples());
    const float x0 = m.getX() + (float) (viewStart / total) * m.getWidth(), x1 = m.getX() + (float) ((viewStart + viewLen) / total) * m.getWidth();
    g.setColour (lime); g.drawRoundedRectangle (juce::Rectangle<float> (x0, m.getY() - 1.0f, std::max (6.0f, x1 - x0), m.getHeight() + 2.0f), 4.0f, 1.2f);
    const double zoom = total / viewLen;
    caps (g, zoom > 1.05 ? juce::String ("Zoom ") + juce::String (zoom, zoom < 10 ? 1 : 0).replace (".", ",") + juce::String::fromUTF8 ("\xc3\x97") : juce::String ("Toda"),
          { 10.0f, m.getY(), 56.0f, m.getHeight() }, juce::Justification::centredLeft, scrText, 7.5f, 0.16f);
}

// ---------------------------------------------------------------------------- interacción
int WaveView::hitSegment (int lane, double s, Sel& out) const
{
    if (res == nullptr || res->analysis == nullptr) return -1;
    const auto& A = *res->analysis;
    const auto& v = lane == 0 ? A.phrases : lane == 1 ? A.words : A.syllables;
    const int f = (int) (s / A.hop);
    for (int i = 0; i < (int) v.size(); ++i)
        if (f >= v[(size_t) i].a && f < v[(size_t) i].b) { out = { v[(size_t) i].a * A.hop, v[(size_t) i].b * A.hop, lane, i }; return i; }
    return -1;
}

float WaveView::meanGainDb (int a, int b) const
{
    if (res == nullptr || res->analysis == nullptr) return 0.0f;
    const int hop = res->analysis->hop;
    double s = 0; int c = 0;
    for (int f = a / hop; f < std::max (a / hop + 1, b / hop); ++f) { s += curveDbAt (f); ++c; }
    return c ? (float) (s / c) : 0.0f;
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (res == nullptr || res->analysis == nullptr) return;
    const auto p = e.position;
    if (miniArea().expanded (0, 4).contains (p))
    {
        draggingMini = true;
        const double total = res->take->numSamples();
        viewStart = juce::jlimit (0.0, std::max (0.0, total - viewLen), (p.x - miniArea().getX()) / miniArea().getWidth() * total - viewLen * 0.5);
        repaint(); return;
    }
    const double s = sampleAt (p.x);
    if (e.mods.isPopupMenu()) { if (sel.empty()) { Sel h; if (hitSegment (1, s, h) >= 0) sel = { h }; } showMenu (e); repaint(); return; }
    if (p.y < 24.0f)
    {
        if (proc.isPreviewing()) proc.setPreview (true, (int) std::max (0.0, s));
        return;
    }
    int lane = -1;
    for (int k = 0; k < 3; ++k) if (laneArea (k).expanded (0, 2).contains (p)) lane = k;
    if (lane < 0 && waveArea().contains (p))
    {
        // dentro de una selección: arrastrar la curva
        for (auto& sl : sel)
            if (s >= sl.a && s < sl.b) { draggingCurve = true; dragStartY = p.y; dragDb = 0; return; }
        lane = 1;
    }
    if (lane < 0) return;
    Sel h;
    if (hitSegment (lane, s, h) >= 0)
    {
        if (e.mods.isShiftDown() || e.mods.isCommandDown())
        {
            auto it = std::find_if (sel.begin(), sel.end(), [&] (const Sel& x) { return x.a == h.a && x.b == h.b; });
            if (it != sel.end()) sel.erase (it); else sel.push_back (h);
        }
        else sel = { h };
    }
    else if (! e.mods.isShiftDown()) sel.clear();
    updateInspector();
    repaint();
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingMini && res != nullptr)
    {
        const double total = res->take->numSamples();
        viewStart = juce::jlimit (0.0, std::max (0.0, total - viewLen), (e.position.x - miniArea().getX()) / miniArea().getWidth() * total - viewLen * 0.5);
        repaint(); return;
    }
    if (draggingCurve)
    {
        const auto w = waveArea();
        const float perDb = (w.getHeight() * 0.5f - 10.0f) / 15.0f;
        dragDb = std::round ((dragStartY - e.position.y) / perDb * (e.mods.isShiftDown() ? 0.25f : 1.0f) * 10.0f) / 10.0f;
        repaint();
    }
}

void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (draggingCurve && std::abs (dragDb) >= 0.1f)
    {
        const auto edits = proc.getEdits();
        proc.undo.beginNewTransaction();
        for (auto& s : sel)
        {
            float prev = 0;
            for (auto& ed : edits) if (ed.kind == Edit::offset && ed.a == s.a && ed.b == s.b) prev = ed.value;
            proc.addEdit (Edit::offset, s.a, s.b, prev + dragDb);
        }
    }
    draggingCurve = draggingMini = false; dragDb = 0;
    repaint();
}

void WaveView::mouseMove (const juce::MouseEvent& e)
{
    bool overSel = false;
    const double s = sampleAt (e.position.x);
    if (waveArea().contains (e.position)) for (auto& sl : sel) if (s >= sl.a && s < sl.b) overSel = true;
    setMouseCursor (overSel ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const double s = sampleAt (e.position.x);
    for (auto& sl : sel) if (s >= sl.a && s < sl.b && waveArea().contains (e.position)) { resetSelection(); return; }
    zoomToFit();
}

void WaveView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (res == nullptr) return;
    if (std::abs (w.deltaX) > std::abs (w.deltaY) || e.mods.isShiftDown())
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        viewStart = juce::jlimit (0.0, std::max (0.0, (double) res->take->numSamples() - viewLen), viewStart - d * viewLen * 0.5);
        repaint();
    }
    else zoomBy (w.deltaY > 0 ? 0.8f : 1.25f, e.position.x);
}
void WaveView::mouseMagnify (const juce::MouseEvent& e, float s) { zoomBy (1.0f / s, e.position.x); }

bool WaveView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { playB.onClick(); return true; }
    if (k == juce::KeyPress::escapeKey) { sel.clear(); updateInspector(); repaint(); return true; }
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { resetSelection(); return true; }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'Z')
    {
        if (k.getModifiers().isShiftDown()) proc.undo.redo(); else proc.undo.undo();
        return true;
    }
    if (k.getTextCharacter() == '+' || k.getTextCharacter() == '=') { zoomBy (0.7f, waveArea().getCentreX()); return true; }
    if (k.getTextCharacter() == '-') { zoomBy (1.4f, waveArea().getCentreX()); return true; }
    return false;
}

void WaveView::showMenu (const juce::MouseEvent&)
{
    juce::PopupMenu m;
    const bool has = ! sel.empty();
    m.addItem (1, u8 ("Bloquear la ganancia de la selecci\xc3\xb3n"), has);
    m.addItem (2, u8 ("Marcar como expresi\xc3\xb3n (no corregir)"), has);
    m.addItem (3, u8 ("Restablecer la selecci\xc3\xb3n"), has);
    m.addSeparator();
    m.addItem (4, u8 ("Zoom a la selecci\xc3\xb3n"), has);
    m.addItem (5, u8 ("Ver la toma entera"));
    m.addSeparator();
    m.addItem (6, u8 ("Quitar todas las ediciones"), ! proc.getEdits().empty());
    juce::Component::SafePointer<WaveView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& v = *safe;
        if (r == 1) v.lockSelection();
        if (r == 2) v.expressionSelection();
        if (r == 3) v.resetSelection();
        if (r == 4 && ! v.sel.empty() && v.res != nullptr)
        {
            int a = v.sel.front().a, b = v.sel.front().b;
            for (auto& s : v.sel) { a = std::min (a, s.a); b = std::max (b, s.b); }
            const double pad = (b - a) * 0.3 + v.res->take->sampleRate * 0.1;
            v.viewStart = std::max (0.0, a - pad); v.viewLen = (b - a) + 2 * pad; v.repaint();
        }
        if (r == 5) v.zoomToFit();
        if (r == 6) v.proc.clearEdits (0, std::numeric_limits<int>::max());
    });
}

void WaveView::lockSelection()
{
    proc.undo.beginNewTransaction();
    for (auto& s : sel) proc.addEdit (Edit::lock, s.a, s.b, meanGainDb (s.a, s.b));
}
void WaveView::expressionSelection()
{
    proc.undo.beginNewTransaction();
    for (auto& s : sel) proc.addEdit (Edit::expression, s.a, s.b, 0.0f);
}
void WaveView::resetSelection()
{
    for (auto& s : sel) proc.clearEdits (s.a, s.b);
}

void WaveView::updateInspector()
{
    if (sel.size() != 1 || res == nullptr || res->analysis == nullptr || res->level == nullptr) { inspector.setVisible (false); return; }
    const auto& s = sel.front();
    const auto& A = *res->analysis; const auto& L = *res->level;
    const double sr = res->take->sampleRate;
    auto fmtDb = [] (float v) { return (v >= 0.05f ? "+" : (v <= -0.05f ? juce::String::fromUTF8 ("\xe2\x88\x92") : juce::String())) + juce::String (std::abs (v), 1).replace (".", ",") + " dB"; };
    auto tstr = [&] (int smp) { const juce::int64 hp = res->take->hostPosFor (smp); const double t = (hp >= 0 ? (double) hp : (double) smp) / sr; return juce::String::formatted ("%d:%05.2f", (int) t / 60, std::fmod (t, 60.0)).replace (".", ","); };
    const char* reasons[] = { "dentro de lo natural", "deriva de la frase", "se pierde en su frase", "sobresale en su frase", "s\xc3\xadlaba que se cae", "ajuste manual", "bloqueada" };
    std::vector<std::pair<juce::String, juce::String>> rows;
    juce::String kind, title;
    if (s.kind == 1 && s.index >= 0 && s.index < (int) L.words.size())
    {
        const auto& wi = L.words[(size_t) s.index];
        kind = "Palabra seleccionada"; title = juce::String::fromUTF8 ("Palabra ") + juce::String (s.index + 1) + juce::String::fromUTF8 (" \xc2\xb7 ") + tstr (s.a);
        rows.push_back ({ "Medida", juce::String (wi.levelDb, 1).replace (".", ",").replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92")) + " LUFS" });
        rows.push_back ({ juce::String::fromUTF8 ("Correcci\xc3\xb3n"), fmtDb (meanGainDb (s.a, s.b)) });
        rows.push_back ({ "Motivo", u8 (reasons[std::clamp (wi.reason, 0, 6)]) + (wi.expression ? juce::String::fromUTF8 (" \xc2\xb7 expresi\xc3\xb3n") : juce::String()) });
        rows.push_back ({ "Confianza", juce::String (wi.conf, 2).replace (".", ",") });
    }
    else
    {
        const auto& v = s.kind == 0 ? A.phrases : A.syllables;
        if (s.index < 0 || s.index >= (int) v.size()) { inspector.setVisible (false); return; }
        kind = s.kind == 0 ? "Frase seleccionada" : u8 ("S\xc3\xadlaba seleccionada");
        title = (s.kind == 0 ? juce::String ("Frase ") : juce::String::fromUTF8 ("S\xc3\xadlaba ")) + juce::String (s.index + 1) + juce::String::fromUTF8 (" \xc2\xb7 ") + tstr (s.a);
        rows.push_back ({ "Medida", juce::String (A.segLevel (v[(size_t) s.index]), 1).replace (".", ",").replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92")) + " LUFS" });
        rows.push_back ({ juce::String::fromUTF8 ("Correcci\xc3\xb3n media"), fmtDb (meanGainDb (s.a, s.b)) });
        rows.push_back ({ juce::String::fromUTF8 ("Duraci\xc3\xb3n"), juce::String ((s.b - s.a) / sr, 2).replace (".", ",") + " s" });
        rows.push_back ({ "Confianza", juce::String (v[(size_t) s.index].conf, 2).replace (".", ",") });
    }
    inspector.set (title, kind, rows);
    const auto w = waveArea();
    float x = xOf (s.b) + 14.0f;
    if (x + inspector.getWidth() > w.getRight()) x = xOf (s.a) - inspector.getWidth() - 14.0f;
    x = juce::jlimit (w.getX(), w.getRight() - (float) inspector.getWidth(), x);
    inspector.setTopLeftPosition ((int) x, (int) w.getY() + 8);
    inspector.setVisible (true);
}
} // namespace levelivo::ui
