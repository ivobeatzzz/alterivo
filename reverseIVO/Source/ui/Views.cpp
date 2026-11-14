#include "Views.h"
#include "../dsp/Dsp.h"

namespace reverseivo::ui
{
using namespace reverseivo::theme;

// Posición en pantalla de una muestra del reverse: la subida ocupa el 86 % del ancho y la cola el resto,
// para que el reverse (lo importante) se vea grande aunque la cola sea larga.
static float xForResult (const Result& r, juce::Rectangle<float> a, juce::int64 idx)
{
    const float split = a.getX() + a.getWidth() * 0.86f;
    if (idx <= r.hitIndex) return a.getX() + (float) idx / (float) std::max (1, r.hitIndex) * (split - a.getX());
    const int tail = std::max (1, r.size() - r.hitIndex);
    return split + (float) (idx - r.hitIndex) / (float) tail * (a.getRight() - split);
}

// ============================================================================ WaveView
float WaveView::xForSource (int sample) const
{
    auto s = proc.getSource();
    const auto a = stripArea();
    if (s == nullptr || s->size() == 0) return a.getX();
    return a.getX() + (float) sample / (float) s->size() * a.getWidth();
}
int WaveView::sampleForX (float x) const
{
    auto s = proc.getSource();
    const auto a = stripArea();
    if (s == nullptr) return 0;
    return juce::jlimit (0, s->size(), (int) ((x - a.getX()) / a.getWidth() * (float) s->size()));
}

void WaveView::paint (juce::Graphics& g)
{
    const auto a = waveArea();
    auto res = proc.getResult();
    auto src = proc.getSource();
    if (src == nullptr)
    {
        label (g, "CAPTURE OR IMPORT A VOCAL", a.withTrimmedBottom (a.getHeight() * 0.5f - 4.0f).withTrimmedTop (a.getHeight() * 0.5f - 22.0f),
               juce::Justification::centred, theme::text, 12.0f, 0.24f);
        g.setColour (soft); g.setFont (font (F::man, 13.0f));
        g.drawText (u8 ("Pulsa \xe2\x97\x8f y reproduce el proyecto, o arrastra un audio aqu\xc3\xad."), a.withTrimmedTop (a.getHeight() * 0.5f + 4.0f).withHeight (20.0f),
                    juce::Justification::centred, false);
    }
    // línea central
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (juce::Rectangle<float> (a.getX(), a.getCentreY(), a.getWidth(), 1.0f));

    if (res != nullptr && res->size() > 0)
    {
        const auto& R = *res;
        const int bins = (int) R.peaks.size();
        const float cy = a.getCentreY(), half = a.getHeight() * 0.5f - 3.0f;
        float pk = 1e-6f; for (float v : R.peaks) pk = std::max (pk, v);
        juce::Path pre, tail;
        std::vector<juce::Point<float>> topPre, topTail;
        for (int k = 0; k < bins; ++k)
        {
            const float v = (R.peaks[(size_t) std::max (0, k - 1)] + R.peaks[(size_t) k] * 2.0f + R.peaks[(size_t) std::min (bins - 1, k + 1)]) * 0.25f;
            const juce::int64 idx = (juce::int64) k * R.size() / bins;
            const float x = xForResult (R, a, idx);
            const float y = cy - std::max (0.6f, v / pk * half);
            (idx <= R.hitIndex ? topPre : topTail).push_back ({ x, y });
        }
        auto build = [cy] (juce::Path& p, const std::vector<juce::Point<float>>& top)
        {
            if (top.size() < 2) return;
            p.startNewSubPath (top.front().x, cy);
            for (auto& q : top) p.lineTo (q);
            p.lineTo (top.back().x, cy);
            for (auto it = top.rbegin(); it != top.rend(); ++it) p.lineTo (it->x, 2.0f * cy - it->y);
            p.closeSubPath();
        };
        build (pre, topPre); build (tail, topTail);
        const float hx = xForResult (R, a, R.hitIndex);
        cachedShadow (g, pre, accent.withAlpha (0.20f), 12, { 0, 0 });
        juce::ColourGradient wg (waveDeep.withAlpha (0.15f), a.getX(), 0, cream, hx, 0, false);
        wg.addColour (0.65, waveMid.withAlpha (0.6f));
        wg.addColour (0.95, waveHi.withAlpha (0.95f));
        g.setGradientFill (wg); g.fillPath (pre);
        g.setColour (waveMid.withAlpha (0.28f)); g.fillPath (tail);
        // pulsos (negras) cuando hay tempo
        const double bpm = R.bpm;
        if (bpm > 0.0)
        {
            const double beatSamples = 60.0 / bpm * R.sampleRate;
            for (double b = (double) R.hitIndex; b >= 0.0; b -= beatSamples)
                dot (g, { xForResult (R, a, (juce::int64) b), a.getBottom() - 1.0f }, 1.6f, juce::Colours::white.withAlpha (0.22f));
        }
        // golpe
        g.setColour (cream);
        g.fillRoundedRectangle (juce::Rectangle<float> (hx - 0.75f, a.getY() + 2.0f, 1.5f, a.getHeight() - 4.0f), 0.75f);
        dot (g, { hx, a.getY() + 2.0f }, 2.5f, cream);
        // cabezal
        const int pi = proc.playIndex();
        if (pi >= 0 && pi < R.size())
        {
            const float px = xForResult (R, a, pi);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (px - 0.5f, a.getY(), 1.0f, a.getHeight()));
        }
    }
    else if (src != nullptr)
    {
        g.setColour (soft); g.setFont (font (F::man, 13.0f));
        g.drawText (proc.isGenerating() ? u8 ("Generando\xe2\x80\xa6") : u8 ("Pulsa GENERATE"), a, juce::Justification::centred, false);
    }

    // fuente con la región elegida
    const auto st = stripArea();
    well (g, st, 6.0f);
    if (src != nullptr)
    {
        const auto peaks = proc.sourcePeaks();
        const int bins = (int) peaks.size();
        float pk = 1e-6f; for (float v : peaks) pk = std::max (pk, v);
        const float cy = st.getCentreY(), h = st.getHeight() * 0.5f - 3.0f;
        const float x0 = xForSource (proc.regionStart()), x1 = xForSource (proc.regionEnd());
        for (int k = 0; k < bins; k += 2)
        {
            const float x = st.getX() + (float) k / (float) bins * st.getWidth();
            const float v = std::max (0.5f, peaks[(size_t) k] / pk * h);
            const bool in = x >= x0 && x <= x1;
            g.setColour (in ? accentHi.withAlpha (0.85f) : juce::Colours::white.withAlpha (0.18f));
            g.fillRect (juce::Rectangle<float> (x, cy - v, 1.0f, v * 2.0f));
        }
        g.setColour (accent.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (x0, st.getY() + 1.0f, x1 - x0, st.getHeight() - 2.0f));
        for (float x : { x0, x1 })
        {
            g.setColour (accent); g.fillRect (juce::Rectangle<float> (x - 1.0f, st.getY(), 2.0f, st.getHeight()));
            g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.5f, st.getY() - 3.0f, 7.0f, 5.0f), 1.5f);
        }
        const int sp = proc.sourcePlayIndex();
        if (sp >= 0) { g.setColour (juce::Colours::white.withAlpha (0.8f)); g.fillRect (juce::Rectangle<float> (xForSource (sp) - 0.5f, st.getY(), 1.0f, st.getHeight())); }
        label (g, "SRC", st.withX (0.0f).withWidth (28.0f), juce::Justification::centredLeft, dim, 8.5f, 0.2f);
    }
}

void WaveView::mouseMove (const juce::MouseEvent& e)
{
    if (! stripArea().expanded (0, 6).contains (e.position) || proc.getSource() == nullptr) { setMouseCursor (juce::MouseCursor::NormalCursor); return; }
    const bool nearEdge = std::abs (e.position.x - xForSource (proc.regionStart())) < 6.0f || std::abs (e.position.x - xForSource (proc.regionEnd())) < 6.0f;
    setMouseCursor (nearEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::IBeamCursor);
}
void WaveView::mouseDown (const juce::MouseEvent& e)
{
    drag = 0;
    if (! stripArea().expanded (0, 6).contains (e.position) || proc.getSource() == nullptr) return;
    if (std::abs (e.position.x - xForSource (proc.regionStart())) < 6.0f) drag = 1;
    else if (std::abs (e.position.x - xForSource (proc.regionEnd())) < 6.0f) drag = 2;
    else { drag = 3; dragAnchor = sampleForX (e.position.x); }
}
void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    const int s = sampleForX (e.position.x);
    if (drag == 1) proc.setRegion (std::min (s, proc.regionEnd() - 1), proc.regionEnd());
    else if (drag == 2) proc.setRegion (proc.regionStart(), std::max (s, proc.regionStart() + 1));
    else if (drag == 3 && std::abs (s - dragAnchor) > 2) proc.setRegion (std::min (s, dragAnchor), std::max (s, dragAnchor));
    repaint();
}
void WaveView::mouseUp (const juce::MouseEvent&) { drag = 0; }
void WaveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (stripArea().expanded (0, 6).contains (e.position)) { proc.autoRegion(); repaint(); }
}

void WaveView::tick()
{
    auto r = proc.getResult();
    auto s = proc.getSource();
    const int gen = r != nullptr ? r->generation : -1;
    const int pi = proc.playIndex(), sp = proc.sourcePlayIndex();
    if (pi != lastPlay || sp != lastSrcPlay || gen != lastGen || proc.regionStart() != lastA || proc.regionEnd() != lastB || s.get() != lastSource)
    {
        lastPlay = pi; lastSrcPlay = sp; lastGen = gen; lastA = proc.regionStart(); lastB = proc.regionEnd(); lastSource = s.get();
        repaint();
    }
}

// ============================================================================ CurveView
float CurveView::yFor (float norm) const
{
    const auto p = plot();
    return p.getBottom() - dsp::clamp01 (norm) * p.getHeight();
}

static float logNorm (float hz) { return dsp::clamp01 (std::log (std::max (20.0f, hz) / 20.0f) / std::log (1000.0f)); }

std::vector<CurveView::Handle> CurveView::handles() const
{
    const auto s = readSettings (proc.apvts);
    std::vector<float> vol, pit, fil;
    laneCurves (s, 161, vol, pit, fil);
    const auto p = plot();
    std::vector<Handle> h;
    auto X = [&] (float u) { return p.getX() + u * p.getWidth(); };
    if (lane == volume)
    {
        h.push_back ({ id::volStart, X (0.0f), yFor ((s.volStartDb + 60.0f) / 66.0f), false });
        h.push_back ({ id::volCurve, X (0.5f), yFor (vol[80]), true });
        h.push_back ({ id::volEnd, X (1.0f), yFor ((s.volEndDb + 60.0f) / 66.0f), false });
    }
    else if (lane == pitch)
    {
        h.push_back ({ id::pitchStart, X (0.0f), yFor ((s.pitch + s.pitchStart + 24.0f) / 48.0f), false });
        h.push_back ({ id::pitchCurve, X (0.5f), yFor (pit[80]), true });
        h.push_back ({ id::pitchEnd, X (1.0f), yFor ((s.pitch + s.pitchEnd + 24.0f) / 48.0f), false });
    }
    else if (lane == filter)
    {
        h.push_back ({ id::lpStart, X (0.0f), yFor (logNorm (s.lpStart)), false });
        h.push_back ({ id::filterCurve, X (0.5f), yFor (fil[80]), true });
        h.push_back ({ id::lpEnd, X (1.0f), yFor (logNorm (s.lpEnd)), false });
        h.push_back ({ id::hpStart, X (0.0f), yFor (logNorm (s.hpStart)), false });
        h.push_back ({ id::hpEnd, X (1.0f), yFor (logNorm (s.hpEnd)), false });
    }
    return h;
}

void CurveView::paint (juce::Graphics& g)
{
    const auto p = plot();
    const auto s = readSettings (proc.apvts);
    // fondo: el reverse actual muy suave
    if (auto res = proc.getResult())
    {
        const int bins = (int) res->peaks.size();
        float pk = 1e-6f; for (float v : res->peaks) pk = std::max (pk, v);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        const int last = (int) ((juce::int64) res->hitIndex * bins / std::max (1, res->size()));
        for (int k = 0; k < last; k += 2)
        {
            const float x = p.getX() + (float) k / (float) std::max (1, last) * p.getWidth();
            const float v = res->peaks[(size_t) k] / pk * p.getHeight() * 0.45f;
            g.fillRect (juce::Rectangle<float> (x, p.getCentreY() - v, 1.2f, v * 2.0f));
        }
    }
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float f : { 0.25f, 0.5f, 0.75f })
    {
        const float y = p.getY() + p.getHeight() * f;
        for (float x = p.getX(); x < p.getRight(); x += 8.0f) g.fillRect (juce::Rectangle<float> (x, y, 2.0f, 1.0f));
    }
    g.setColour (cream.withAlpha (0.8f));
    g.fillRect (juce::Rectangle<float> (p.getRight() + 6.0f, p.getY() - 6.0f, 1.5f, p.getHeight() + 12.0f));
    label (g, "HIT", { p.getRight() - 30.0f, p.getY() - 20.0f, 40.0f, 12.0f }, juce::Justification::centredRight, cream, 8.5f, 0.2f);

    if (lane == layers)
    {
        g.setColour (soft); g.setFont (font (F::man, 13.0f));
        g.drawText (u8 ("Capa principal y capa de textura: cada una con su volumen, tono y anchura."), p, juce::Justification::centred, false);
        return;
    }
    std::vector<float> vol, pit, fil;
    laneCurves (s, 161, vol, pit, fil);
    auto curvePath = [&] (const std::vector<float>& c)
    {
        juce::Path path;
        for (size_t i = 0; i < c.size(); ++i)
        {
            const float x = p.getX() + (float) i / (float) (c.size() - 1) * p.getWidth(), y = yFor (c[i]);
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        return path;
    };
    const auto& main = lane == volume ? vol : (lane == pitch ? pit : fil);
    auto path = curvePath (main);
    if (lane == volume)
    {
        juce::Path area (path); area.lineTo (p.getRight(), p.getBottom()); area.lineTo (p.getX(), p.getBottom()); area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.20f), 0, p.getY(), accent.withAlpha (0.0f), 0, p.getBottom(), false));
        g.fillPath (area);
    }
    if (lane == filter)
    {
        std::vector<float> hp (161);
        for (int i = 0; i <= 160; ++i)
            hp[(size_t) i] = logNorm (dsp::freqLerp (std::max (20.0f, s.hpStart), std::max (20.0f, s.hpEnd), dsp::shapeCurve (i / 160.0f, s.filterCurve)));
        g.setColour (cream.withAlpha (0.45f));
        juce::Path hpPath = curvePath (hp);
        juce::Path dashed; const float dl[] = { 5.0f, 5.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, hpPath, dl, 2);
        g.fillPath (dashed);
        label (g, "LP", { p.getX() - 24.0f, yFor (main.front()) - 7.0f, 20.0f, 14.0f }, juce::Justification::centredRight, accent, 8.5f, 0.1f);
        label (g, "HP", { p.getX() - 24.0f, yFor (hp.front()) - 7.0f, 20.0f, 14.0f }, juce::Justification::centredRight, cream.withAlpha (0.6f), 8.5f, 0.1f);
    }
    g.setColour (accent.withAlpha (0.18f)); g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (accentDeep, p.getX(), 0, accentHi, p.getRight(), 0, false));
    g.strokePath (path, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto hs = handles();
    for (int i = 0; i < (int) hs.size(); ++i)
    {
        const auto& h = hs[(size_t) i];
        const bool hot = i == active || i == hover;
        const bool hp = h.pid == id::hpStart || h.pid == id::hpEnd;
        const juce::Colour c = hp ? cream.withAlpha (0.8f) : accent;
        if (hot) dot (g, { h.x, h.y }, 11.0f, c.withAlpha (0.18f));
        if (h.curve) { dot (g, { h.x, h.y }, 4.5f, juce::Colour (0xff0b0e0a)); g.setColour (c); g.drawEllipse (h.x - 4.5f, h.y - 4.5f, 9.0f, 9.0f, 2.0f); }
        else { dot (g, { h.x, h.y }, hot ? 6.0f : 5.0f, c); dot (g, { h.x, h.y }, 2.0f, juce::Colours::white); }
        if (hot)
        {
            auto* prm = proc.apvts.getParameter (h.pid);
            const auto txt = prm->getCurrentValueAsText();
            const auto f = font (F::soraBold, 11.0f);
            const float w = textWidth (f, txt) + 18.0f;
            auto box = juce::Rectangle<float> (w, 20.0f).withCentre ({ juce::jlimit (p.getX() + w * 0.5f, p.getRight() - w * 0.5f, h.x), h.y - 22.0f });
            raised (g, box, 10.0f, false);
            g.setColour (theme::text); g.setFont (f); g.drawText (txt, box, juce::Justification::centred, false);
        }
    }
}

void CurveView::mouseMove (const juce::MouseEvent& e)
{
    const auto hs = handles();
    int h = -1;
    for (int i = 0; i < (int) hs.size(); ++i) if (e.position.getDistanceFrom ({ hs[(size_t) i].x, hs[(size_t) i].y }) < 11.0f) h = i;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}
void CurveView::mouseDown (const juce::MouseEvent& e)
{
    mouseMove (e);
    active = hover;
    if (active < 0) return;
    const auto h = handles()[(size_t) active];
    auto* prm = proc.apvts.getParameter (h.pid);
    proc.undo.beginNewTransaction();
    prm->beginChangeGesture();
    dragStartY = e.position.y;
    dragStartVal = prm->convertFrom0to1 (prm->getValue());
}
void CurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (active < 0) return;
    const auto hs = handles();
    if (active >= (int) hs.size()) return;
    const auto& h = hs[(size_t) active];
    auto* prm = proc.apvts.getParameter (h.pid);
    const auto p = plot();
    float v;
    if (h.curve) v = dragStartVal - (dragStartY - e.position.y) / p.getHeight() * 1.2f;   // arriba: llega antes (curva más suave)
    else
    {
        const float norm = dsp::clamp01 ((p.getBottom() - e.position.y) / p.getHeight());
        if (lane == volume) v = norm * 66.0f - 60.0f;
        else if (lane == pitch) v = norm * 48.0f - 24.0f - proc.apvts.getRawParameterValue (id::pitch)->load();
        else v = 20.0f * std::pow (1000.0f, norm);
    }
    prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
    repaint();
}
void CurveView::mouseUp (const juce::MouseEvent&)
{
    if (active >= 0)
        if (auto* prm = proc.apvts.getParameter (handles()[(size_t) active].pid)) prm->endChangeGesture();
    active = -1;
    repaint();
}

void CurveView::tick()
{
    // repinta si cambió algún parámetro de las curvas o llegó un reverse nuevo
    const auto s = readSettings (proc.apvts);
    juce::int64 h = 0;
    for (float v : { s.volStartDb, s.volEndDb, s.volCurve, s.fadeIn, s.pitch, s.pitchStart, s.pitchEnd, s.pitchCurve, s.hpStart, s.hpEnd, s.lpStart, s.lpEnd,
                     s.filterCurve, s.moveFilter, s.intensity })
        h = h * 1000003 + (juce::int64) std::lround (v * 1000.0f);
    auto r = proc.getResult();
    const int gen = r != nullptr ? r->generation : -1;
    if (h != lastHash || gen != lastGen) { lastHash = h; lastGen = gen; repaint(); }
}
} // namespace reverseivo::ui
