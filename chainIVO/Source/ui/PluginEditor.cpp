#include "PluginEditor.h"

using namespace chainivo;
using namespace chainivo::ui;
using namespace chainivo::theme;

namespace
{
const char* kEngineNames[6] = { "EQ", "COMP", "HARMONICS", "VOCAL", "SPACE", "CREATIVE" };
const char* kOnIds[6] = { pid::eqOn, pid::compOn, pid::harmOn, pid::vocOn, pid::spaceOn, pid::creaOn };
float pv (ChainIvoProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
juce::String choiceText (ChainIvoProcessor& p, const char* id)
{
    auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id));
    return c != nullptr ? c->getCurrentChoiceName() : juce::String();
}
juce::String hz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }
juce::String dbs (float v) { return (v > 0.05f ? "+" : (v < -0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String())) + juce::String (std::abs (v), 1) + " dB"; }
const juce::Rectangle<int> kTrackCard { 40, 112, 266, 398 }, kEngineCard { 326, 270, 674, 240 };
} // namespace

// =================================================================== Chip
void Chip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    if (selected) limeFill (g, r, r.getHeight() * 0.5f); else raised (g, r, r.getHeight() * 0.5f, 0.35f);
    g.setColour (selected ? onLime : body); g.setFont (font (F::man, 12.5f));
    g.drawText (text, r, juce::Justification::centred, false);
}

// =================================================================== ChainTile
ChainTile::ChainTile (ChainIvoProcessor& p, int e) : proc (p), engine (e)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (u8 ("Pulsa la ficha para verla; el punto la enciende o la apaga"));
}
bool ChainTile::engineOn() const { return pv (proc, kOnIds[engine]) > 0.5f || (engine == 5 && pv (proc, pid::wordOn) > 0.5f); }

void ChainTile::refresh()
{
    on = engineOn();
    juce::String h, a, b;
    switch (engine)
    {
        case 0:
        {
            const float pg = pv (proc, pid::presGain), lg = pv (proc, pid::lowGain), mg = pv (proc, pid::midGain), ag = pv (proc, pid::airGain);
            float best = pg; juce::String where = hz (pv (proc, pid::presFreq));
            if (std::abs (lg) > std::abs (best)) { best = lg; where = hz (pv (proc, pid::lowFreq)); }
            if (std::abs (mg) > std::abs (best)) { best = mg; where = hz (pv (proc, pid::midFreq)); }
            if (std::abs (ag) > std::abs (best)) { best = ag; where = "aire"; }
            h = dbs (best); a = where + (pv (proc, pid::presDyn) > 1.0f && where == hz (pv (proc, pid::presFreq)) ? u8 (" \xc2\xb7 din.") : juce::String());
            b = "HPF " + hz (pv (proc, pid::hpf)); break;
        }
        case 1:
            h = choiceText (proc, pid::compType);
            a = dbs (-proc.compGr()) + " GR";
            b = juce::String (pv (proc, pid::compRatio), 1) + u8 (":1 \xc2\xb7 ") + (pv (proc, pid::compAttack) < 3 ? u8 ("r\xc3\xa1pido") : (pv (proc, pid::compAttack) < 15 ? "medio" : "lento")); break;
        case 2:
        {
            static const char* osTxt[] = { "2x", "4x", "4x+", "8x" };
            h = choiceText (proc, pid::harmType); a = juce::String (juce::roundToInt (pv (proc, pid::harmDrive))) + " %"; b = juce::String ("sobremuestreo ") + osTxt[proc.getQuality()]; break;
        }
        case 3:
            h = "De-ess"; a = dbs (-std::max (proc.deessGr(), proc.harshGr())); b = hz (pv (proc, pid::deessFreq)); break;
        case 4:
            h = choiceText (proc, pid::revType);
            a = (int) pv (proc, pid::revType) == 5 ? (proc.hasIR() ? proc.getIRName() : u8 ("sin IR: plate")) : juce::String (pv (proc, pid::revDecay), 1) + " s";
            b = pv (proc, pid::dlyMix) > 0.5f ? "delay " + choiceText (proc, pid::dlyNote) : "sin delay"; break;
        case 5:
            h = on ? choiceText (proc, pid::creaType) : "Off"; a = on ? choiceText (proc, pid::creaNote) : "pulsa el punto"; b = on ? juce::String (juce::roundToInt (pv (proc, pid::creaAmount))) + " %" : "para activar";
            if (pv (proc, pid::wordOn) > 0.5f) { if (! on) h = choiceText (proc, pid::wordFx); a = u8 ("palabras: ") + choiceText (proc, pid::wordTarget); b = choiceText (proc, pid::wordFx) + " " + juce::String (juce::roundToInt (pv (proc, pid::wordAmount))) + " %"; }
            break;
        default: break;
    }
    if (! on && engine != 5) { h = "Off"; a = "pulsa el punto"; b = "para activar"; }
    if (h != headline || a != line1 || b != line2) { headline = h; line1 = a; line2 = b; }
    repaint();
}

void ChainTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (on)
    {
        raised (g, r, 18.0f, selected ? 1.1f : 0.8f);
        if (selected) { g.setColour (juce::Colour (0xffa9e04b)); g.drawRoundedRectangle (r.reduced (1.25f), 17.0f, 2.5f); }
    }
    else
    {
        well (g, r, 18.0f);
        if (selected) { g.setColour (juce::Colour (0xffa9e04b)); g.drawRoundedRectangle (r.reduced (1.25f), 17.0f, 2.0f); }
    }
    const auto txt = on ? ink : faint;
    const float cx = r.getCentreX();
    logoDot (g, { cx, 17.0f }, 3.5f, on);
    g.setColour (on ? dim2 : faint); g.setFont (font (F::manX, 9.0f, 0.13f));
    g.drawText (kEngineNames[engine], juce::Rectangle<float> (4, 28, r.getWidth() - 4, 13), juce::Justification::centred, false);
    g.setColour (txt); g.setFont (font (F::soraBold, 18.0f, -0.01f));
    g.drawFittedText (headline, juce::Rectangle<int> (8, 45, (int) r.getWidth() - 12, 26), juce::Justification::centred, 1, 0.75f);
    g.setColour (on ? dim2 : faint); g.setFont (font (F::man, 11.0f));
    g.drawFittedText (line1, juce::Rectangle<int> (6, 74, (int) r.getWidth() - 8, 15), juce::Justification::centred, 1, 0.8f);
    g.drawFittedText (line2, juce::Rectangle<int> (6, 90, (int) r.getWidth() - 8, 15), juce::Justification::centred, 1, 0.8f);
    if (pro && hovering)
    {
        g.setColour (ink.withAlpha (0.6f)); g.setFont (font (F::manX, 14.0f));
        if (position > 0) g.drawText (u8 ("\xe2\x80\xb9"), juce::Rectangle<float> (4, r.getBottom() - 22, 20, 18), juce::Justification::centred, false);
        if (position < 5) g.drawText (u8 ("\xe2\x80\xba"), juce::Rectangle<float> (r.getRight() - 22, r.getBottom() - 22, 20, 18), juce::Justification::centred, false);
    }
    if (proc.isLocked (engine)) { g.setColour (deep); g.setFont (font (F::manX, 7.5f, 0.2f)); g.drawText ("LOCK", juce::Rectangle<float> (r.getRight() - 40, 11, 32, 12), juce::Justification::centredRight, false); }
}

void ChainTile::mouseDown (const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (e.position.getDistanceFrom ({ r.getCentreX(), 17.0f }) < 13.0f)
    {
        auto* prm = proc.apvts.getParameter (kOnIds[engine]);
        proc.undo.beginNewTransaction();
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture();
        refresh();
        if (onSelect) onSelect();
        return;
    }
    if (pro && e.position.y > r.getBottom() - 26)
    {
        if (e.position.x < 26 && position > 0 && onMove) { onMove (-1); return; }
        if (e.position.x > r.getRight() - 26 && position < 5 && onMove) { onMove (1); return; }
    }
    if (onSelect) onSelect();
}

// =================================================================== EngineView
EngineView::EngineView (ChainIvoProcessor& p) : proc (p)
{
    histComp.assign (180, 0.0f); histDeess.assign (180, 0.0f); histHarsh.assign (180, 0.0f); histBreath.assign (180, 0.0f); histWord.assign (180, 0.0f);
    addChildComponent (lockToggle); addChildComponent (autoButton); addChildComponent (soloToggle);
    soloToggle.setTooltip (u8 ("Escucha por m\xc3\xb3" "dulo: s\xc3\xb3lo suena este motor (los dem\xc3\xa1s se saltan)"));
    soloToggle.onChange = [this] (bool v) { proc.soloEngine = v ? engine : -1; };
    lockToggle.setTooltip (u8 ("Bloquear: AUTO no cambiar\xc3\xa1 este motor"));
    autoButton.setTooltip (u8 ("Propuesta AUTO para lo que no est\xc3\xa9 bloqueado"));
    lockToggle.onChange = [this] (bool v) { proc.setLocked (engine, v); };
    autoButton.onClick = [this] { proc.autoPropose(); };
}

void EngineView::setEngine (int e, bool isPro)
{
    if (e == engine && isPro == pro && (! pro || ! controls.empty())) return;
    engine = e; pro = isPro;
    buildControls();
    resized(); repaint();
}

void EngineView::buildControls()
{
    controls.clear(); layout.clear();
    lockToggle.setVisible (pro); autoButton.setVisible (pro); soloToggle.setVisible (pro);
    lockToggle.setOn (proc.isLocked (engine));
    if (proc.soloEngine.load() >= 0) proc.soloEngine = pro ? engine : -1;   // el solo sigue al motor elegido; fuera de PRO se apaga
    soloToggle.setOn (proc.soloEngine.load() >= 0);
    if (! pro) return;
    auto* um = &proc.undo;
    auto knob = [&] (const char* name, const char* id)
    {
        auto k = std::make_unique<Knob> (u8 (name), proc.apvts, id, um);
        k->setRadius (15.0f);
        addAndMakeVisible (*k); controls.push_back (std::move (k));
    };
    auto choice = [&] (const char* id)
    {
        auto c = std::make_unique<ParamChoice> (proc.apvts, id, um);
        addAndMakeVisible (*c); controls.push_back (std::move (c));
    };
    // Disposición: x, y, w, h dentro de la tarjeta
    const int top = 50, kw = 63, kh = 90;
    auto row = [&] (int y, int count, int startX = 18) { for (int i = 0; i < count; ++i) layout.push_back ({ startX + i * kw, y, kw, kh }); };
    switch (engine)
    {
        case 0:
            choice (pid::hpfSlope); layout.push_back ({ 430, 54, 226, 30 });
            knob ("HPF", pid::hpf); knob ("LOW", pid::lowGain); knob ("LOW F", pid::lowFreq); knob ("MID", pid::midGain); knob ("MID F", pid::midFreq); knob ("MID Q", pid::midQ);
            knob ("PRES", pid::presGain); knob ("PRES F", pid::presFreq); knob ("PRES Q", pid::presQ); knob ("DYN", pid::presDyn);
            knob ("AIR", pid::airGain); knob ("AIR F", pid::airFreq);
            row (top - 6, 6); row (top + 84, 6); break;
        case 1:
            choice (pid::compType); layout.push_back ({ 18, 50, 400, 32 });
            knob ("THRESH", pid::compThresh); knob ("RATIO", pid::compRatio); knob ("ATTACK", pid::compAttack); knob ("RELEASE", pid::compRelease);
            knob ("MAKEUP", pid::compMakeup); knob ("MIX", pid::compMix);
            row (top + 40, 6, 18); break;
        case 2:
            choice (pid::harmType); layout.push_back ({ 18, 50, 360, 32 });
            knob ("DRIVE", pid::harmDrive); knob ("TONE", pid::harmTone); knob ("MIX", pid::harmMix);
            row (top + 40, 3, 18); break;
        case 3:
            knob ("DE-ESS", pid::deess); knob ("FREQ", pid::deessFreq); knob ("HARSH", pid::harsh); knob ("BREATH", pid::breath);
            row (top + 20, 4, 18); break;
        case 4:
            choice (pid::revType); layout.push_back ({ 430, 50, 226, 30 });
            choice (pid::dlyType); layout.push_back ({ 430, 86, 226, 30 });
            knob ("REV MIX", pid::revMix); knob ("DECAY", pid::revDecay); knob ("PRE", pid::revPredelay); knob ("TONE", pid::revTone);
            knob ("DUCK", pid::duck); knob ("SWELL", pid::revSwell);
            knob ("TIME", pid::dlyNote); knob ("FEEDBK", pid::dlyFeedback); knob ("DLY MIX", pid::dlyMix); knob ("THROW", pid::dlyThrow);
            knob ("WIDTH", pid::width); knob ("DOUBLER", pid::doubler);
            row (top - 6, 6); row (top + 84, 6); break;
        case 5:
        {
            choice (pid::creaType); layout.push_back ({ 18, 44, 400, 28 });
            knob ("RATE", pid::creaNote); knob ("AMOUNT", pid::creaAmount); knob ("PITCH", pid::creaPitch); knob ("PALABRAS", pid::wordAmount);
            row (top + 26, 4, 18);
            auto t = std::make_unique<ParamToggle> (u8 ("POR PALABRA"), proc.apvts, pid::wordOn);
            addAndMakeVisible (*t); controls.push_back (std::move (t)); layout.push_back ({ 280, 100, 140, 30 });
            choice (pid::wordTarget); layout.push_back ({ 18, 170, 400, 26 });
            choice (pid::wordFx); layout.push_back ({ 18, 202, 400, 26 });
            break;
        }
        default: break;
    }
}

void EngineView::resized()
{
    for (size_t i = 0; i < controls.size() && i < layout.size(); ++i) controls[i]->setBounds (layout[i]);
    lockToggle.setBounds (getWidth() - 236, 12, 100, 30);
    soloToggle.setBounds (getWidth() - 340, 12, 100, 30);
    autoButton.setBounds (getWidth() - 112, 10, 94, 34);
}

void EngineView::tick()
{
    push (histWord, proc.wordTargeted() ? 2.0f : (proc.wordActive() ? 1.0f : 0.0f));
    push (histComp, proc.compGr()); push (histDeess, proc.deessGr()); push (histHarsh, proc.harshGr()); push (histBreath, proc.breathGr());
    repaint (pro ? juce::Rectangle<int> (280, 44, 390, 190) : juce::Rectangle<int> (18, 44, 638, 150));
}

void EngineView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    raised (g, b, 22.0f, 1.0f);
    static const char* titles[6] = { "EQ", "COMP \xc2\xb7 reducci\xc3\xb3n de ganancia", "HARMONICS \xc2\xb7 curva de saturaci\xc3\xb3n",
                                     "VOCAL \xc2\xb7 de-ess, dureza y respiraciones", "SPACE \xc2\xb7 cola y ecos", "CREATIVE \xc2\xb7 patr\xc3\xb3n r\xc3\xadtmico" };
    const auto an = proc.getAnalysis();
    juce::String title = u8 (titles[engine]);
    if (engine == 0) title = an.hasLead ? u8 ("EQ \xc2\xb7 frente a la voz principal") : u8 ("EQ \xc2\xb7 espectro y curva");
    if (pro) title = juce::String (kEngineNames[engine]) + u8 (" \xc2\xb7 ajustes");
    label (g, title, { 22, 14, 420, 18 }, juce::Justification::centredLeft, dim, 10.0f);

    if (pro)
    {
        juce::Rectangle<float> ga;
        switch (engine)
        {
            case 0: ga = { 430, 96, 226, 132 }; break;
            case 1: case 2: ga = { 430, 50, 226, 178 }; break;
            case 3: ga = { 290, 50, 366, 178 }; break;
            case 4: ga = { 430, 124, 226, 104 }; break;
            case 5: ga = { 430, 46, 226, 182 }; break;
            default: break;
        }
        if (! ga.isEmpty()) { screen (g, ga, 12.0f); paintGraph (g, ga.reduced (10.0f, 8.0f)); }
        return;
    }
    if (engine == 0)
    {
        // Leyenda
        auto x = b.getRight() - 22.0f;
        auto item = [&] (const juce::String& t, juce::Colour c, bool box, bool dotOn = false, bool withDot = false)
        {
            g.setFont (font (F::man, 12.0f));
            const float w = textWidth (g.getCurrentFont(), t);
            x -= w; g.setColour (dim2); g.drawText (t, juce::Rectangle<float> (x, 14, w + 2, 18), juce::Justification::centredLeft, false);
            if (withDot) { logoDot (g, { x + w + 9.0f, 23.0f }, 3.0f, dotOn); }
            x -= 18.0f;
            g.setColour (c);
            if (box) g.fillRoundedRectangle (x + 2, 17, 12, 12, 3); else g.fillRoundedRectangle (x, 21.5f, 14, 3, 1.5f);
            x -= 14.0f;
        };
        if (an.hasLead) item ("choque", conflict, true);
        if (an.hasInstrumental) item ("beat", juce::Colour (0xffc9a85a), false);
        item (proc.leadConnected.load() ? "voz principal" : "voz principal (sin conectar)", leadLine, false);
        item (voiceTypeName ((VoiceType) proc.getVoiceType()).toLowerCase(), green, false);
    }
    const auto area = juce::Rectangle<float> (18, 44, b.getWidth() - 36, 146);
    screen (g, area, 14.0f);
    paintGraph (g, area.reduced (12.0f, 8.0f));

    // POR QUÉ
    const auto why = proc.getExplanations()[(size_t) engine];
    const auto chipR = juce::Rectangle<float> (18, 200, 84, 24);
    limeFill (g, chipR, 12.0f);
    g.setColour (onLime); g.setFont (font (F::manX, 10.0f, 0.18f)); g.drawText (u8 ("POR QU\xc3\x89"), chipR, juce::Justification::centred, false);
    g.setColour (body); g.setFont (font (F::man, 13.0f));
    const auto text = why.isNotEmpty() ? why : u8 ("AUTO explicar\xc3\xa1 aqu\xc3\xad cada decisi\xc3\xb3n cuando haya analizado la pista (pon la pista a sonar).");
    g.drawFittedText (text, juce::Rectangle<int> (114, 196, (int) b.getWidth() - 132, 32), juce::Justification::centredLeft, 2, 0.85f);
}

void EngineView::paintGraph (juce::Graphics& g, juce::Rectangle<float> r)
{
    switch (engine)
    {
        case 0: paintEq (g, r); break;
        case 1: paintHistory (g, r, { { &histComp, green } }, u8 ("dB de reducci\xc3\xb3n")); break;
        case 2: paintHarm (g, r); break;
        case 3: paintHistory (g, r, { { &histDeess, green }, { &histHarsh, warn }, { &histBreath, juce::Colour (0xff5aa8d6) } }, "de-ess / dureza / respiraciones"); break;
        case 4: paintSpace (g, r); break;
        case 5: paintCreative (g, r); break;
        default: break;
    }
}

void EngineView::paintEq (juce::Graphics& g, juce::Rectangle<float> r)
{
    const double sr = proc.getSampleRateSafe();
    const float lo = std::log10 (30.0f), hi = std::log10 (18000.0f);
    auto fx = [&] (float f) { return r.getX() + r.getWidth() * (std::log10 (f) - lo) / (hi - lo); };
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        g.setColour (ink.withAlpha (0.06f)); g.drawVerticalLine ((int) fx (f), r.getY(), r.getBottom() - 12);
        g.setColour (faint); g.setFont (font (F::man, 9.5f));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (fx (f) + 3, r.getBottom() - 12, 40, 12), juce::Justification::centredLeft, false);
    }
    const auto an = proc.getAnalysis();
    const float specTop = r.getY() + 36.0f, specBot = r.getBottom() - 14.0f;
    auto sy = [&] (float d) { return juce::jlimit (specTop, specBot, specTop + (specBot - specTop) * (-d / 42.0f)); };
    if (an.hasLead && an.conflictBand >= 0 && an.conflictAmount > 0.15f)
    {
        const float c = Analyzer::bandCentre (an.conflictBand);
        const float x0 = fx (c / 1.3f), x1 = fx (c * 1.3f);
        g.setColour (conflict.withAlpha (0.45f)); g.fillRoundedRectangle (x0, r.getY(), x1 - x0, r.getHeight() - 12, 6.0f);
    }
    auto spectrumPath = [&] (const std::array<float, kBands>& s, bool fill, juce::Colour col)
    {
        float mx = -200; for (auto v : s) mx = std::max (mx, v);
        juce::Path pth; bool started = false;
        for (int b = 0; b < kBands; ++b)
        {
            const float x = fx (Analyzer::bandCentre (b)), y = sy (s[(size_t) b] - mx);
            if (! started) { pth.startNewSubPath (x, y); started = true; } else pth.lineTo (x, y);
        }
        if (fill)
        {
            juce::Path f (pth); f.lineTo (fx (Analyzer::bandCentre (kBands - 1)), specBot); f.lineTo (fx (Analyzer::bandCentre (0)), specBot); f.closeSubPath();
            g.setGradientFill (juce::ColourGradient (lime.withAlpha (0.5f), 0, specTop, lime.withAlpha (0.04f), 0, specBot, false)); g.fillPath (f);
        }
        g.setColour (col); g.strokePath (pth, juce::PathStrokeType (fill ? 2.4f : 2.0f, juce::PathStrokeType::curved));
    };
    if (an.activeSeconds >= 1.0f)
    {
        if (an.hasInstrumental) spectrumPath (an.instrumentalSpectrum, false, juce::Colour (0xffc9a85a).withAlpha (0.8f));
        if (an.hasLead) spectrumPath (an.leadSpectrum, false, leadLine);
        spectrumPath (an.spectrum, true, green);
    }
    else
    {
        g.setColour (dim); g.setFont (font (F::man, 12.0f));
        g.drawText (u8 ("Pon la pista a sonar: chainIVO la escucha y mide su espectro"), r.withTrimmedTop (40), juce::Justification::centred, false);
    }
    // Curva real del EQ (con la ganancia de la banda dinámica en este momento)
    const auto es = proc.eqSettingsNow();
    const float presNow = proc.eqPresNow();
    auto ey = [&] (float d) { return juce::jlimit (r.getY(), r.getBottom() - 12, r.getY() + 22.0f - d * 3.2f); };
    juce::Path curve;
    for (int i = 0; i <= 160; ++i)
    {
        const float f = std::pow (10.0f, lo + (hi - lo) * (float) i / 160.0f);
        const float d = EqEngine::responseDb (es, sr, f, es.presDyn > 0.001f ? presNow : es.presGain);
        if (i == 0) curve.startNewSubPath (fx (f), ey (d)); else curve.lineTo (fx (f), ey (d));
    }
    juce::Path dashed; const float dl[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.8f).createDashedStroke (dashed, curve, dl, 2);
    g.setColour (ink); g.fillPath (dashed);
    label (g, "EQ", { r.getX() + 2, r.getY(), 40, 12 }, juce::Justification::centredLeft, ink, 9.0f);
    if (std::abs (es.presGain) > 0.1f)
    {
        const float x = fx (es.presFreq), y = ey (EqEngine::responseDb (es, sr, es.presFreq, es.presDyn > 0.001f ? presNow : es.presGain));
        g.setColour (juce::Colours::white); g.fillEllipse (x - 6, y - 6, 12, 12);
        g.setColour (ink); g.drawEllipse (x - 6, y - 6, 12, 12, 2.0f);
        g.setColour (juce::Colour (0xff8dcb34)); g.fillEllipse (x - 2.5f, y - 2.5f, 5, 5);
        const auto pill = juce::Rectangle<float> (x + 10, y + 6, 62, 20);
        g.setColour (juce::Colour (0xff15240f)); g.fillRoundedRectangle (pill, 10.0f);
        g.setColour (juce::Colour (0xffd9f59a)); g.setFont (font (F::manX, 10.5f));
        g.drawText (dbs (es.presDyn > 0.001f ? presNow : es.presGain), pill, juce::Justification::centred, false);
    }
}

void EngineView::paintHistory (juce::Graphics& g, juce::Rectangle<float> r, std::initializer_list<std::pair<const std::vector<float>*, juce::Colour>> lines, const juce::String& unit)
{
    const float maxDb = 12.0f;
    for (float d : { 3.0f, 6.0f, 9.0f })
    {
        const float y = r.getY() + r.getHeight() * d / maxDb;
        g.setColour (ink.withAlpha (0.05f)); g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (faint); g.setFont (font (F::man, 9.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String ((int) d), juce::Rectangle<float> (r.getX(), y - 12, 30, 12), juce::Justification::centredLeft, false);
    }
    float now = 0.0f;
    for (auto& [v, col] : lines)
    {
        juce::Path p; const int n = (int) v->size();
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + 30 + (r.getWidth() - 140) * (float) i / (float) (n - 1);
            const float y = r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, (*v)[(size_t) i] / maxDb);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col); g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));
        now = std::max (now, v->back());
    }
    g.setColour (ink); g.setFont (font (F::soraBold, 26.0f));
    g.drawText (dbs (-now), juce::Rectangle<float> (r.getRight() - 104, r.getCentreY() - 22, 104, 30), juce::Justification::centredRight, false);
    g.setColour (dim); g.setFont (font (F::man, 11.0f));
    g.drawText (unit, juce::Rectangle<float> (r.getRight() - 160, r.getCentreY() + 10, 160, 16), juce::Justification::centredRight, false);
}

void EngineView::paintHarm (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto type = (HarmType) (int) pv (proc, pid::harmType);
    const float drive = pv (proc, pid::harmDrive) * 0.01f;
    auto sq = r.withWidth (r.getHeight()).reduced (4.0f);
    g.setColour (ink.withAlpha (0.06f)); g.drawLine (sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY()); g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom());
    g.setColour (leadLine); g.drawLine (sq.getX(), sq.getBottom(), sq.getRight(), sq.getY(), 1.2f);
    juce::Path p;
    for (int i = 0; i <= 120; ++i)
    {
        const float x = -1.5f + 3.0f * (float) i / 120.0f;
        const float y = juce::jlimit (-1.5f, 1.5f, HarmonicsEngine::transfer (type, x, drive));
        const float px = sq.getX() + (x + 1.5f) / 3.0f * sq.getWidth(), py = sq.getBottom() - (y + 1.5f) / 3.0f * sq.getHeight();
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (green); g.strokePath (p, juce::PathStrokeType (2.4f));
    static const char* osTxt[] = { "2x (ECO)", "4x (NORMAL)", "4x con filtro m\xc3\xa1ximo (HIGH)", "8x con filtro m\xc3\xa1ximo (ULTRA)" };
    auto t = r.withTrimmedLeft (sq.getWidth() + 30);
    g.setColour (ink); g.setFont (font (F::soraBold, 22.0f)); g.drawText (choiceText (proc, pid::harmType), t.removeFromTop (34), juce::Justification::centredLeft, false);
    g.setColour (body); g.setFont (font (F::man, 12.5f));
    g.drawText ("Drive " + juce::String (juce::roundToInt (drive * 100)) + u8 (" %  \xc2\xb7  mezcla ") + juce::String (juce::roundToInt (pv (proc, pid::harmMix))) + " %", t.removeFromTop (20), juce::Justification::centredLeft, false);
    g.drawText (u8 ("Sobremuestreo ") + u8 (osTxt[proc.getQuality()]), t.removeFromTop (20), juce::Justification::centredLeft, false);
    g.drawText (u8 (type == HarmType::exciter ? "Arm\xc3\xb3nicos s\xc3\xb3lo en la banda alta" : "La l\xc3\xadnea gris es sin saturar"), t.removeFromTop (20), juce::Justification::centredLeft, false);
    if (pv (proc, pid::harmOn) < 0.5f) { g.setColour (red); g.drawText (u8 ("Apagado (enci\xc3\xa9ndelo con el punto de la ficha)"), t.removeFromTop (20), juce::Justification::centredLeft, false); }
}

void EngineView::paintSpace (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Respuesta dibujada con los ajustes reales: pre-delay, caída de la reverb (T60) y ecos del delay con su realimentación
    const double bpm = proc.lastBpm.load();
    const float decay = pv (proc, pid::revDecay), pre = pv (proc, pid::revPredelay) * 0.001f;
    const float revMix = pv (proc, pid::revMix) * 0.01f, dlyMix = pv (proc, pid::dlyMix) * 0.01f, fb = pv (proc, pid::dlyFeedback) * 0.01f;
    const float dt = (float) (noteBeats ((int) pv (proc, pid::dlyNote)) * 60.0 / bpm);
    const float span = std::max (2.0f, std::min (8.0f, std::max (decay * 1.2f, dt * 6.0f)));
    auto tx = [&] (float t) { return r.getX() + r.getWidth() * t / span; };
    auto ay = [&] (float db) { return r.getBottom() - 14 - (r.getHeight() - 20) * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
    for (int s = 1; s < (int) span + 1; ++s)
    {
        g.setColour (ink.withAlpha (0.05f)); g.drawVerticalLine ((int) tx ((float) s), r.getY(), r.getBottom() - 12);
        g.setColour (faint); g.setFont (font (F::man, 9.0f)); g.drawText (juce::String (s) + " s", juce::Rectangle<float> (tx ((float) s) + 3, r.getBottom() - 12, 30, 12), juce::Justification::centredLeft, false);
    }
    // voz (seca)
    g.setColour (ink.withAlpha (0.8f)); g.fillRect (juce::Rectangle<float> (tx (0), ay (0), 3, r.getBottom() - 14 - ay (0)));
    if (revMix > 0.001f)
    {
        juce::Path p; const float startDb = gainToDb (revMix * 1.4f);
        p.startNewSubPath (tx (pre), r.getBottom() - 14);
        for (int i = 0; i <= 100; ++i) { const float t = pre + span * (float) i / 100.0f; p.lineTo (tx (t), ay (startDb - 60.0f * (t - pre) / std::max (0.1f, decay))); }
        p.lineTo (tx (span), r.getBottom() - 14); p.closeSubPath();
        g.setGradientFill (juce::ColourGradient (lime.withAlpha (0.6f), tx (0), 0, lime.withAlpha (0.08f), tx (span), 0, false)); g.fillPath (p);
    }
    if (dlyMix > 0.001f)
        for (int k = 1; k < 12; ++k)
        {
            const float t = dt * (float) k; if (t > span) break;
            const float db = gainToDb (dlyMix * std::pow (fb, (float) (k - 1)));
            g.setColour (green); g.fillRoundedRectangle (tx (t) - 2, ay (db), 4, r.getBottom() - 14 - ay (db), 2);
        }
    g.setColour (body); g.setFont (font (F::man, 12.0f));
    g.drawFittedText (choiceText (proc, pid::revType) + " " + juce::String (decay, 1) + u8 (" s \xc2\xb7 ") + (dlyMix > 0.001f ? "delay " + choiceText (proc, pid::dlyNote) + " " + choiceText (proc, pid::dlyType) : juce::String ("sin delay"))
                + u8 (" \xc2\xb7 ducking ") + juce::String (juce::roundToInt (pv (proc, pid::duck))) + " %", r.withHeight (16).toNearestInt(), juce::Justification::centredRight, 1, 0.6f);
    if (pv (proc, pid::dlyThrow) > 0.5f || pv (proc, pid::revSwell) > 0.5f)
        g.drawText (u8 ("Automatizaciones: ") + juce::String (proc.phraseEnds()) + u8 (" finales de frase detectados"), r.withTrimmedTop (18).withHeight (16), juce::Justification::centredRight, false);
}

void EngineView::paintCreative (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool on = pv (proc, pid::creaOn) > 0.5f;
    const int type = (int) pv (proc, pid::creaType);
    if (pv (proc, pid::wordOn) > 0.5f)
    {
        // Carril real: gris = palabra detectada, lima = palabra (o compás) con efecto
        auto lane = r.removeFromBottom (std::min (52.0f, r.getHeight() * 0.35f));
        label (g, u8 ("Palabras \xc2\xb7 ") + proc.apvts.getParameter (pid::wordFx)->getCurrentValueAsText() + u8 (" en ") + proc.apvts.getParameter (pid::wordTarget)->getCurrentValueAsText()
                  + u8 (" \xc2\xb7 ") + juce::String (proc.wordsDetected()), lane.removeFromTop (14), juce::Justification::centredLeft, dim, 8.5f);
        const float w = lane.getWidth() / (float) histWord.size();
        for (size_t k = 0; k < histWord.size(); ++k)
        {
            const float v = histWord[k];
            if (v < 0.5f) continue;
            g.setColour (v > 1.5f ? lime : track);
            g.fillRect (lane.getX() + w * (float) k, lane.getY() + 6, w + 0.5f, lane.getHeight() - 12);
        }
        if (! on) return;
        r.removeFromBottom (6);
    }
    if (! on)
    {
        g.setColour (dim); g.setFont (font (F::man, 13.0f));
        g.drawText (u8 ("Apagado. Nunca se activa solo: enci\xc3\xa9ndelo con el punto de la ficha CREATIVE"), r, juce::Justification::centred, false);
        return;
    }
    auto textArea = r.removeFromBottom (32.0f);
    const auto row = r.withSizeKeepingCentre (r.getWidth() - 20, std::min (56.0f, r.getHeight() - 8.0f));
    const float w = row.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto c = juce::Rectangle<float> (row.getX() + w * (float) i, row.getY(), w, row.getHeight()).reduced (6, 0);
        bool active = true; juce::String t;
        if (type == 1) { static const int pat[4] = { 1, 0, 1, 1 }; active = pat[i] == 1; t = active ? "abre" : "cierra"; }
        else if (type == 0) { t = i < 2 ? "voz" : "repite 2"; active = true; }
        else if (type == 3) { t = proc.apvts.getParameter (pid::creaPitch)->getCurrentValueAsText(); active = true; }
        else if (type == 4) { t = "granos"; active = (i % 2) == 0; }
        else { t = u8 ("al rev\xc3\xa9s"); }
        if (active) limeFill (g, c, 10.0f); else well (g, c, 10.0f);
        g.setColour (active ? onLime : ink); g.setFont (font (F::manX, 11.0f, 0.05f)); g.drawFittedText (t.toUpperCase(), c.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.6f);
    }
    g.setColour (body); g.setFont (font (F::man, 12.0f));
    g.drawFittedText (type == 3 ? u8 ("Capa afinada mezclada con la voz (la voz original no se toca)")
                : type == 4 ? u8 ("Nube de granos del \xc3\xbaltimo segundo \xc2\xb7 tama\xc3\xb1o seg\xc3\xban RATE (") + choiceText (proc, pid::creaNote) + ")"
                : u8 ("Grupo de 4 divisiones de ") + choiceText (proc, pid::creaNote) + u8 (" \xc2\xb7 sincronizado al tempo del proyecto"), textArea.toNearestInt(), juce::Justification::centred, 2, 0.8f);
}

// =================================================================== TrackCard
TrackCard::TrackCard (ChainIvoProcessor& p) : proc (p)
{
    static const int mainTypes[] = { 0, 1, 2, 4, 5, 7 };
    for (int t : mainTypes)
    {
        auto* c = chips.add (new Chip (voiceTypeName ((VoiceType) t)));
        c->getProperties().set ("type", t);
        c->onClick = [this, t] { proc.setVoiceType (t); refresh(); };
        addAndMakeVisible (c);
    }
    auto* more = chips.add (new Chip (u8 ("M\xc3\xa1s \xe2\x96\xbe")));
    more->getProperties().set ("type", -2);
    more->onClick = [this] { showMore(); };
    addAndMakeVisible (more);
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TrackCard::refresh()
{
    const int t = proc.getVoiceType();
    bool inMain = false;
    for (auto* c : chips)
    {
        const int ct = c->getProperties()["type"];
        c->selected = ct == t; inMain |= c->selected;
        c->repaint();
    }
    auto* more = chips.getLast();
    more->text = inMain ? u8 ("M\xc3\xa1s \xe2\x96\xbe") : voiceTypeName ((VoiceType) t) + u8 (" \xe2\x96\xbe");
    more->selected = ! inMain;
    resized(); repaint();
}

void TrackCard::resized()
{
    // Chips en filas centradas
    const float maxW = (float) getWidth() - 40.0f, gap = 6.0f;
    std::vector<std::vector<Chip*>> rows (1);
    float rowW = 0;
    for (auto* c : chips)
    {
        const float w = (float) c->preferredWidth();
        if (! rows.back().empty() && rowW + gap + w > maxW) { rows.emplace_back(); rowW = 0; }
        rowW += (rows.back().empty() ? 0 : gap) + w;
        rows.back().push_back (c);
    }
    float y = 164;
    for (auto& r : rows)
    {
        float total = 0; for (auto* c : r) total += (float) c->preferredWidth(); total += gap * (float) (r.size() - 1);
        float x = ((float) getWidth() - total) * 0.5f;
        for (auto* c : r) { c->setBounds ((int) x, (int) y, c->preferredWidth(), 32); x += (float) c->preferredWidth() + gap; }
        y += 34;
    }
    genreBox = { 24, 302, (float) getWidth() - 48, 40 };
    reBox = { 24, 360, 92, 24 };
    toolsBox = { 116, 360, (float) getWidth() - 140, 24 };
}

void TrackCard::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    raised (g, getLocalBounds().toFloat(), 22.0f, 1.0f);
    label (g, "Tu pista", { 0, 18, W, 14 }, juce::Justification::centred);
    const auto an = proc.getAnalysis();
    const auto cl = proc.getClassification();
    const bool manual = ! proc.isVoiceTypeAuto();
    const bool analysing = ! an.ready;

    // Tipo de voz: discreto y centrado
    const juce::String typeName = (an.activeSeconds < 2.0f && ! manual) ? u8 ("Escuchando\xe2\x80\xa6") : voiceTypeName ((VoiceType) proc.getVoiceType());
    g.setColour (ink); g.setFont (font (F::soraSemi, 20.0f, -0.01f));
    g.drawFittedText (typeName, juce::Rectangle<int> (20, 38, (int) W - 40, 26), juce::Justification::centred, 1, 0.8f);

    juce::String left, right; float frac;
    if (manual) { left = u8 ("Elegido por ti"); right = an.ready ? juce::String (juce::roundToInt (cl.confidence * 100)) + " %" : juce::String(); frac = 1.0f; }
    else if (analysing) { left = an.activeSeconds < 0.5f ? u8 ("Esperando audio") : u8 ("Analizando"); right = juce::String (juce::roundToInt (an.progress * 100)) + " %"; frac = an.progress; }
    else { left = "Detectado solo"; right = juce::String (juce::roundToInt (cl.confidence * 100)) + " %"; frac = cl.confidence; }
    {
        const auto fl = font (F::man, 12.0f), fr = font (F::manX, 12.0f);
        const juce::String sep = right.isNotEmpty() ? u8 ("  \xc2\xb7  ") : juce::String();
        const float wl = textWidth (fl, left + sep), wr = textWidth (fr, right);
        float x = (W - wl - wr) * 0.5f;
        g.setColour (dim2); g.setFont (fl); g.drawText (left + sep, juce::Rectangle<float> (x, 66, wl + 2, 16), juce::Justification::centredLeft, false);
        g.setColour (green); g.setFont (fr); g.drawText (right, juce::Rectangle<float> (x + wl, 66, wr + 4, 16), juce::Justification::centredLeft, false);
    }
    const auto bar = juce::Rectangle<float> (24, 88, W - 48, 8);
    well (g, bar, 4.0f);
    if (frac > 0.01f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4e9c2c), bar.getX(), 0, lime, bar.getRight(), 0, false));
        g.fillRoundedRectangle (bar.reduced (2.0f).withWidth ((bar.getWidth() - 4.0f) * frac), 2.0f);
    }
    g.setColour (dim2); g.setFont (font (F::man, 11.0f));
    juce::String reason = an.activeSeconds >= 2.0f ? cl.reason : u8 ("Pon la pista a sonar unos segundos");
    if (cl.needsLead && an.ready) reason += u8 (". Sin voz principal no distingo doble, armon\xc3\xad" "a o respuesta");
    if (an.groupCount > 0) reason = u8 ("Grupo: ") + juce::String (an.groupCount + 1) + u8 (" capas con chainIVO \xc2\xb7 ") + reason;
    g.drawFittedText (reason, juce::Rectangle<int> (20, 102, (int) W - 40, 30), juce::Justification::centredTop, 2, 0.9f);

    label (g, "Tipo de voz", { 0, 144, W, 14 }, juce::Justification::centred);
    label (g, "Estilo", { 0, 282, W, 14 }, juce::Justification::centred);
    raised (g, genreBox, 12.0f, 0.5f);
    const int gi = proc.getGenre();
    {
        const juce::String name = gi >= 0 ? u8 (genre (gi).name) : u8 ("Sin estilo");
        const juce::String fam = gi >= 0 ? u8 ("  \xc2\xb7  ") + u8 (genre (gi).family) : juce::String();
        const auto fn = font (F::man, 14.0f), ff = font (F::man, 12.0f);
        const float wn = textWidth (fn, name), wf = textWidth (ff, fam), wa = 16.0f;
        float x = genreBox.getCentreX() - (wn + wf + wa) * 0.5f;
        g.setColour (ink); g.setFont (fn); g.drawText (name, juce::Rectangle<float> (x, genreBox.getY(), wn + 2, genreBox.getHeight()), juce::Justification::centredLeft, false);
        g.setColour (dim2); g.setFont (ff); g.drawText (fam + u8 ("  \xe2\x96\xbe"), juce::Rectangle<float> (x + wn, genreBox.getY(), wf + wa + 10, genreBox.getHeight()), juce::Justification::centredLeft, false);
    }
    // Pie de la tarjeta: dos acciones centradas en su mitad, con separador
    g.setColour (shadow.withAlpha (0.08f)); g.fillRect (juce::Rectangle<float> (24, 352, W - 48, 1));
    g.fillRect (juce::Rectangle<float> (reBox.getRight(), 364, 1, 16));
    g.setColour (deep); g.setFont (font (F::manX, 9.5f, 0.1f));
    g.drawText (u8 ("\xe2\x86\xbb  ANALIZAR"), reBox, juce::Justification::centred, false);
    juce::String tools = proc.isExporting() ? u8 ("EXPORTANDO\xe2\x80\xa6") : (proc.isInstrumentalBusy() ? u8 ("LEYENDO BEAT\xe2\x80\xa6") : u8 ("BEAT \xc2\xb7 EXPORTAR  \xe2\x96\xbe"));
    g.drawText (tools, toolsBox, juce::Justification::centred, false);
}

void TrackCard::mouseDown (const juce::MouseEvent& e)
{
    if (genreBox.contains (e.position)) showGenres();
    else if (reBox.contains (e.position)) { proc.reanalyse(); repaint(); }
    else if (toolsBox.contains (e.position)) showTools();
}

void TrackCard::showTools()
{
    juce::PopupMenu m;
    const auto inst = proc.getInstrumentalName();
    m.addSectionHeader ("INSTRUMENTAL DE REFERENCIA");
    m.addItem (1, inst.isEmpty() ? u8 ("Cargar el beat (WAV, AIFF, FLAC, MP3)\xe2\x80\xa6") : u8 ("Cambiar beat (ahora: ") + inst + ")", ! proc.isInstrumentalBusy());
    m.addItem (2, "Quitar el beat", inst.isNotEmpty());
    m.addSectionHeader (u8 ("EXPORTAR CON ESTA CADENA (el original no se toca)"));
    m.addItem (3, u8 ("Procesar un archivo \xe2\x86\x92 WAV 24 bits\xe2\x80\xa6"), ! proc.isExporting());
    m.addItem (4, u8 ("Procesar un archivo \xe2\x86\x92 WAV 32 bits float\xe2\x80\xa6"), ! proc.isExporting());
    m.addSectionHeader ("REVERB IR (CONVOLUCI\xc3\x93N)");
    m.addItem (6, proc.hasIR() ? u8 ("Cambiar IR (ahora: ") + proc.getIRName() + ")" : u8 ("Cargar una respuesta de impulso (WAV)\xe2\x80\xa6"));
    m.addItem (7, "Quitar la IR", proc.hasIR());
    m.addSectionHeader ("PERFIL PERSONAL");
    m.addItem (8, u8 ("Aprender de lo que cambio despu\xc3\xa9s de AUTO"), true, proc.profile.enabled());
    m.addItem (9, u8 ("Ver lo aprendido"));
    m.addItem (10, u8 ("Reiniciar el perfil"));
    m.addSectionHeader ("VISTA");
    m.addItem (5, u8 ("Tema oscuro"), true, theme::dark);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toolsBox.toNearestInt())),
                     [safe = juce::Component::SafePointer<TrackCard> (this)] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        if (r == 2) { pr.clearInstrumental(); return; }
        if (r == 7) { pr.clearIR(); return; }
        if (r == 8) { pr.profile.setEnabled (! pr.profile.enabled()); return; }
        if (r == 9) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Perfil personal", pr.profile.describe (voiceTypeNames())); return; }
        if (r == 10) { pr.profile.reset(); return; }
        if (r == 6)
        {
            safe->chooser = std::make_unique<juce::FileChooser> ("Respuesta de impulso", juce::File(), "*.wav;*.aif;*.aiff");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult().existsAsFile()) safe->proc.loadIR (fc.getResult()); });
            return;
        }
        if (r == 5)
        {
            theme::applySkin (! theme::dark); saveDarkSkin (theme::dark);
            if (auto* top = safe->getTopLevelComponent()) top->repaint();
            return;
        }
        if (r == 1)
        {
            safe->chooser = std::make_unique<juce::FileChooser> ("Instrumental de referencia", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult().existsAsFile()) safe->proc.loadInstrumental (fc.getResult()); });
            return;
        }
        const bool f32 = r == 4;
        safe->chooser = std::make_unique<juce::FileChooser> ("Archivo para procesar", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, f32] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
            const auto in = fc.getResult();
            const auto out = in.getParentDirectory().getNonexistentChildFile (in.getFileNameWithoutExtension() + " (chainIVO)", ".wav", false);
            safe->proc.exportFile (in, out, f32, [] (juce::String msg)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "chainIVO", msg);
            });
        });
    });
}

void TrackCard::showMore()
{
    juce::PopupMenu m;
    m.addItem (1000, u8 ("Detectar autom\xc3\xa1ticamente"), true, proc.isVoiceTypeAuto());
    m.addSeparator();
    for (int t = 0; t < (int) VoiceType::count; ++t) m.addItem (t + 1, voiceTypeName ((VoiceType) t), true, ! proc.isVoiceTypeAuto() && proc.getVoiceType() == t);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (chips.getLast()),
                     [safe = juce::Component::SafePointer<TrackCard> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         safe->proc.setVoiceType (r == 1000 ? -1 : r - 1);
                         safe->refresh();
                     });
}

void TrackCard::showGenres()
{
    juce::PopupMenu m;
    m.addItem (1000, "Sin estilo", true, proc.getGenre() < 0);
    juce::String fam; juce::PopupMenu sub;
    auto flush = [&] { if (fam.isNotEmpty()) m.addSubMenu (fam, sub); sub = juce::PopupMenu(); };
    for (int i = 0; i < genreCount(); ++i)
    {
        const auto f = u8 (genre (i).family);
        if (f != fam) { flush(); fam = f; }
        sub.addItem (i + 1, u8 (genre (i).name), true, proc.getGenre() == i);
    }
    flush();
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (genreBox.toNearestInt())),
                     [safe = juce::Component::SafePointer<TrackCard> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         safe->proc.setGenre (r == 1000 ? -1 : r - 1);
                         safe->repaint();
                     });
}

// =================================================================== PresetPill
void PresetPill::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    raised (g, r, r.getHeight() * 0.5f, 0.7f);
    logoDot (g, { r.getX() + 22.0f, r.getCentreY() }, 4.0f, true);
    const auto name = proc.getPresetName();
    juce::String sub = u8 ("pulsa para buscar \xc2\xb7 ") + juce::String ((int) proc.presets.recipes().size()) + " IVO";
    for (int i = 0; i < (int) proc.presets.recipes().size(); ++i)
        if (proc.presets.recipes()[(size_t) i].name == name)
        {
            const auto& rc = proc.presets.recipes()[(size_t) i];
            sub = (rc.genre >= 0 ? u8 (genre (rc.genre).name) + u8 (" \xc2\xb7 ") : juce::String()) + juce::String (i + 1) + " de " + juce::String ((int) proc.presets.recipes().size());
        }
    g.setColour (ink); g.setFont (font (F::man, 14.5f));
    g.drawFittedText (name, juce::Rectangle<int> ((int) r.getX() + 36, (int) r.getY() + 6, (int) r.getWidth() - 96, 20), juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (dim2); g.setFont (font (F::man, 11.0f));
    g.drawText (sub, juce::Rectangle<float> (r.getX() + 36, r.getY() + 25, r.getWidth() - 96, 15), juce::Justification::centredLeft, true);
    g.setColour (dim); g.setFont (font (F::man, 18.0f));
    g.drawText (u8 ("\xe2\x80\xb9"), juce::Rectangle<float> (r.getRight() - 58, r.getY(), 20, r.getHeight()), juce::Justification::centred, false);
    g.drawText (u8 ("\xe2\x80\xba"), juce::Rectangle<float> (r.getRight() - 32, r.getY(), 20, r.getHeight()), juce::Justification::centred, false);
}

void PresetPill::step (int d)
{
    const int n = (int) proc.presets.recipes().size();
    int cur = -1;
    for (int i = 0; i < n; ++i) if (proc.presets.recipes()[(size_t) i].name == proc.getPresetName()) cur = i;
    cur = cur < 0 ? (d > 0 ? 0 : n - 1) : (cur + d + n) % n;
    proc.loadFactoryPreset (cur);
    repaint();
}

void PresetPill::mouseDown (const juce::MouseEvent& e)
{
    const float rx = (float) getWidth() - 2.0f;
    if (e.position.x > rx - 60 && e.position.x < rx - 36) step (-1);
    else if (e.position.x >= rx - 36) step (1);
    else if (onOpen) onOpen();
}

// =================================================================== PresetBrowser
PresetBrowser::PresetBrowser (ChainIvoProcessor& p)
    : proc (p), cats ({ "PARA TI", "TODOS", "AD-LIBS", "COROS", "DOBLES", u8 ("ARMON\xc3\x8d" "AS"), "EFECTOS", "USUARIO" }, false, 9.5f)
{
    search.setFont (font (F::man, 15.0f));
    search.setTextToShowWhenEmpty (u8 ("Buscar: nombre, estilo, car\xc3\xa1" "cter (ej. oscuro, rage, delay)\xe2\x80\xa6"), dim);
    search.onTextChange = [this] { update(); };
    search.setJustification (juce::Justification::centredLeft);
    addAndMakeVisible (search);
    cats.onChange = [this] (int) { update(); };
    addAndMakeVisible (cats);
    favs.onChange = [this] (bool) { update(); };
    addAndMakeVisible (favs);
    list.setRowHeight (52);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);
    for (auto* b : { &saveB, &importB, &exportB, &closeB }) addAndMakeVisible (b);
    closeB.onClick = [this] { setVisible (false); if (onClose) onClose(); };
    saveB.onClick = [this] { saveAs(); };
    importB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Importar preset", juce::File(), "*.chainivo");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) { proc.presets.importFile (fc.getResult()); update(); } });
    };
    exportB.onClick = [this]
    {
        const int row = list.getSelectedRow();
        if (row < 0 || row >= (int) items.size() || items[(size_t) row].factory)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Exportar", u8 ("Elige un preset de USUARIO en la lista (los presets IVO vienen con el plugin)."));
            return;
        }
        const auto name = items[(size_t) row].name;
        chooser = std::make_unique<juce::FileChooser> ("Exportar preset", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (name + ".chainivo"), "*.chainivo");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, name] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc.presets.exportFile (name, fc.getResult()); });
    };
}

void PresetBrowser::open()
{
    // Si chainIVO ya sabe qué es la pista, se abre en sus sugerencias; el usuario puede elegir cualquier otro
    cats.setSelected (proc.getAnalysis().ready || ! proc.isVoiceTypeAuto() ? 0 : 1);
    update(); setVisible (true); toFront (true); search.grabKeyboardFocus();
}

void PresetBrowser::update()
{
    static const char* map[] = { "", "", "Ad-libs", "Coros", "Dobles y segundas", "Armon\xc3\xad" "as", "Efectos", "Usuario" };
    if (cats.getSelected() == 0)
    {
        // Sugeridos: los de la categoría de su tipo de voz, con su estilo primero; después, los efectos
        const auto type = (VoiceType) juce::jlimit (0, (int) VoiceType::count - 1, proc.getVoiceType());
        const int g = proc.getGenre();
        auto mine = proc.presets.search (search.getText(), PresetLibrary::categoryFor (type), favs.isOn());
        std::stable_sort (mine.begin(), mine.end(), [&] (const PresetInfo& a, const PresetInfo& b)
        {
            auto score = [&] (const PresetInfo& p)
            {
                if (p.factory && p.recipeIndex >= 0)
                {
                    const auto& r = proc.presets.recipes()[(size_t) p.recipeIndex];
                    return (r.type == type ? 2 : 0) + (g >= 0 && r.genre == g ? 1 : 0);
                }
                return 0;
            };
            return score (a) > score (b);
        });
        items = mine;
        if (PresetLibrary::categoryFor (type) != "Efectos")
            for (auto& p : proc.presets.search (search.getText(), "Efectos", favs.isOn())) items.push_back (p);
    }
    else items = proc.presets.search (search.getText(), u8 (map[cats.getSelected()]), favs.isOn());
    list.updateContent(); list.repaint();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (dark ? 0.5f : 0.22f));
    raised (g, card.toFloat(), 24.0f, 1.4f);
    g.setColour (ink); g.setFont (font (F::soraBold, 22.0f));
    g.drawText ("Presets", card.getX() + 28, card.getY() + 20, 200, 30, juce::Justification::centredLeft);
    g.setColour (dim2); g.setFont (font (F::man, 12.0f));
    {
        const auto an = proc.getAnalysis();
        const juce::String det = an.ready ? u8 ("Tu pista suena a ") + voiceTypeName ((VoiceType) proc.getVoiceType()).toLowerCase() + u8 (" \xc2\xb7 ")
                                          : juce::String();
        g.drawText (det + u8 ("Elige el que quieras: el preset manda y se adapta a tu grabaci\xc3\xb3n"), card.getX() + 130, card.getY() + 24, card.getWidth() - 158, 24, juce::Justification::centredLeft);
    }
    well (g, search.getBounds().toFloat().expanded (6, 0), 20.0f);
    screen (g, list.getBounds().toFloat().expanded (6, 6), 14.0f);
}

void PresetBrowser::resized()
{
    card = getLocalBounds().withSizeKeepingCentre (700, 560);
    auto r = card.reduced (28);
    r.removeFromTop (44);
    search.setBounds (r.removeFromTop (40).reduced (14, 0));
    r.removeFromTop (12);
    auto rowR = r.removeFromTop (38);
    favs.setBounds (rowR.removeFromRight (160).reduced (0, 6));
    cats.setBounds (rowR.reduced (0, 0).withTrimmedRight (10));
    r.removeFromTop (14);
    auto bottom = r.removeFromBottom (44);
    r.removeFromBottom (14);
    list.setBounds (r.reduced (6));
    closeB.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (8);
    saveB.setBounds (bottom.removeFromLeft (170)); bottom.removeFromLeft (8);
    importB.setBounds (bottom.removeFromLeft (120)); bottom.removeFromLeft (8);
    exportB.setBounds (bottom.removeFromLeft (120));
}

void PresetBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition())) { setVisible (false); if (onClose) onClose(); }
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel)
{
    if (row < 0 || row >= (int) items.size()) return;
    const auto& it = items[(size_t) row];
    auto r = juce::Rectangle<float> (4, 3, (float) w - 8, (float) h - 6);
    if (sel || it.name == proc.getPresetName()) raised (g, r, 12.0f, 0.35f);
    const bool fav = proc.presets.isFavourite (it.name);
    juce::Path s; s.addStar ({ r.getX() + 20, r.getCentreY() }, 5, 4.5f, 9.5f, 0.0f);
    if (fav) { g.setColour (juce::Colour (0xff8dcb34)); g.fillPath (s); g.setColour (ink); g.strokePath (s, juce::PathStrokeType (1.1f)); }
    else { g.setColour (faint); g.strokePath (s, juce::PathStrokeType (1.3f)); }
    g.setColour (ink); g.setFont (font (F::man, 14.5f));
    g.drawText (it.name, juce::Rectangle<float> (r.getX() + 42, r.getY() + 5, 240, 20), juce::Justification::centredLeft, true);
    g.setColour (dim2); g.setFont (font (F::man, 11.5f));
    g.drawText (it.category + (it.genreName.isNotEmpty() ? u8 (" \xc2\xb7 ") + it.genreName : juce::String()), juce::Rectangle<float> (r.getX() + 42, r.getY() + 25, 240, 16), juce::Justification::centredLeft, true);
    g.setColour (body); g.setFont (font (F::man, 12.0f));
    g.drawFittedText (it.intention, juce::Rectangle<int> ((int) r.getX() + 290, (int) r.getY() + 4, (int) r.getWidth() - 300, (int) r.getHeight() - 8), juce::Justification::centredLeft, 2, 0.9f);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) items.size()) return;
    const auto it = items[(size_t) row];
    if (e.x < 40) { proc.presets.toggleFavourite (it.name); update(); return; }
    if (it.factory) proc.loadFactoryPreset (it.recipeIndex); else proc.loadUserPreset (it.file);
    setVisible (false);
    if (onClose) onClose();
}

void PresetBrowser::saveAs()
{
    auto* w = new juce::AlertWindow (u8 ("Guardar preset"), u8 ("Se guarda una copia exacta de la cadena actual (los presets IVO no se modifican)."), juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getPresetName() + " (mi variante)", "Nombre:");
    w->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        if (r == 1) { proc.saveUserPreset (w->getTextEditorContents ("name")); update(); }
    }), true);
}

// =================================================================== MainView
MainView::MainView (ChainIvoProcessor& p)
    : proc (p), presetPill (p), trackCard (p), engineView (p),
      inputK ("INPUT", p.apvts, pid::input, &p.undo), intensityK ("INTENSITY", p.apvts, pid::intensity, &p.undo),
      mixK ("DRY / WET", p.apvts, pid::mix, &p.undo), outputK ("OUTPUT", p.apvts, pid::output, &p.undo),
      gainMatch ("GAIN MATCH", p.apvts, pid::gainMatch), clipSafe ("CLIP SAFE", p.apvts, pid::clipSafe), bypass ("BYPASS", p.apvts, pid::bypass),
      browser (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (modeSwitch);
    modeSwitch.onChange = [this] (int i) { proc.setPro (i == 1); selectEngine (selected); layoutTiles(); };
    addAndMakeVisible (presetPill);
    presetPill.onOpen = [this] { browser.open(); };
    addAndMakeVisible (undoB); addAndMakeVisible (redoB);
    undoB.setTooltip ("Deshacer"); redoB.setTooltip ("Rehacer");
    undoB.onClick = [this] { proc.undo.undo(); };
    redoB.onClick = [this] { proc.undo.redo(); };
    addAndMakeVisible (abSwitch);
    abSwitch.onChange = [this] (int i) { proc.selectAb (i); };
    addAndMakeVisible (trackCard);
    for (int e = 0; e < 6; ++e)
    {
        auto* t = tiles.add (new ChainTile (proc, e));
        t->setComponentID ("tile" + juce::String (e));
        t->onSelect = [this, e] { selectEngine (e); };
        t->onMove = [this, e] (int d) { proc.moveEngine (e, d); layoutTiles(); };
        addAndMakeVisible (t);
    }
    addAndMakeVisible (engineView);
    for (auto* k : { &inputK, &intensityK, &mixK, &outputK }) addAndMakeVisible (k);
    intensityK.setRadius (44.0f);
    addAndMakeVisible (quality);
    quality.onChange = [this] (int q) { proc.setQuality (q); };
    quality.setTooltip (u8 ("ECO: 2x, reverb de 4 l\xc3\xadneas \xc2\xb7 NORMAL: 4x, 8 l\xc3\xadneas \xc2\xb7 HIGH: 4x filtro m\xc3\xa1ximo, interpolaci\xc3\xb3n fina \xc2\xb7 ULTRA: 8x, 16 l\xc3\xadneas"));
    for (auto* t : { &gainMatch, &clipSafe, &bypass }) addAndMakeVisible (t);
    gainMatch.setTooltip (u8 ("Iguala el volumen procesado al original (comparaci\xc3\xb3n justa)"));
    clipSafe.setTooltip (u8 ("Protecci\xc3\xb3n suave por encima de -1 dBFS (no es un limitador para tapar errores)"));
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
    addChildComponent (browser);
    browser.onClose = [this] { presetPill.repaint(); trackCard.refresh(); };
    selectEngine (0);
    startTimerHz (30);
}

MainView::~MainView() { setLookAndFeel (nullptr); }

void MainView::selectEngine (int e)
{
    selected = e;
    for (auto* t : tiles) { t->selected = t == tiles[e]; t->repaint(); }
    engineView.setEngine (e, proc.isPro());
}

void MainView::layoutTiles()
{
    const bool pro = proc.isPro();
    const auto order = proc.getOrder();
    for (int pos = 0; pos < 6; ++pos)
    {
        auto* t = tiles[order[(size_t) pos]];
        t->pro = pro; t->position = pos;
        t->setBounds (326 + pos * 114 - 2, 134, 108, 116);
    }
}

void MainView::resized()
{
    // Cabecera: todo centrado en y = 56 y con 16 px entre piezas, alineado al margen derecho (1000)
    abSwitch.setBounds (904, 34, 96, 44);
    redoB.setBounds (856, 40, 32, 32); undoB.setBounds (820, 40, 32, 32);
    presetPill.setBounds (560, 30, 244, 52);
    modeSwitch.setBounds (354, 34, 190, 44);
    trackCard.setBounds (kTrackCard);
    layoutTiles();
    engineView.setBounds (kEngineCard);
    auto knobBox = [] (int cx, int cy, int w, int h) { return juce::Rectangle<int> (cx - w / 2, cy - h / 2, w, h); };
    // Mandos globales: misma separación (144 px) y nombres a la misma altura que QUALITY
    inputK.setBounds (knobBox (104, 634, 110, 196));
    intensityK.setBounds (knobBox (248, 634, 170, 196));
    mixK.setBounds (knobBox (392, 634, 110, 196));
    outputK.setBounds (knobBox (536, 634, 110, 196));
    quality.setBounds (640, 558, 360, 44);
    gainMatch.setBounds (640, 614, 132, 30); clipSafe.setBounds (778, 614, 116, 30); bypass.setBounds (900, 614, 100, 30);
    inMeter.setBounds (640, 656, 172, 14); outMeter.setBounds (828, 656, 172, 14);
    browser.setBounds (getLocalBounds());
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    {
        juce::ColourGradient glow (bgGlow, 520, 380, bg.withAlpha (0.0f), 520 + 520, 380, true);
        g.setGradientFill (glow); g.fillRect (getLocalBounds());
        juce::ColourGradient lg (lime.withAlpha (0.10f), 600, 390, lime.withAlpha (0.0f), 600 + 360, 390, true);
        g.setGradientFill (lg); g.fillRect (getLocalBounds());
    }
    juce::AttributedString t;
    t.append ("chain", font (F::soraLight, 34.0f, -0.01f), ink);
    t.append ("IVO", font (F::soraBold, 34.0f, -0.01f), deep);
    t.draw (g, { 40, 26, 300, 42 });
    label (g, u8 ("Secondary \xc2\xb7 Vocal \xc2\xb7 Chain"), { 42, 70, 300, 14 }, juce::Justification::centredLeft);
    label (g, "La cadena", { 328, 110, 200, 14 }, juce::Justification::centredLeft);
    g.setColour (shadow.withAlpha (0.07f)); g.fillRect (juce::Rectangle<float> (40, 526, 960, 1));   // separa la zona de trabajo de los globales
    g.setColour (dim2); g.setFont (font (F::man, 12.0f));
    const juce::String hint = proc.isPro() ? u8 ("PRO: orden real de la cadena \xc2\xb7 \xe2\x80\xb9 \xe2\x80\xba para moverlas \xc2\xb7 el punto enciende o apaga")
                                           : u8 ("AUTO ha montado ") + juce::String (std::max (proc.getEnginesBuilt(), 0)) + u8 (" de 6 m\xc3\xb3" "dulos \xc2\xb7 pulsa uno para verlo");
    g.drawText (hint, juce::Rectangle<float> (500, 106, 500, 18), juce::Justification::centredRight, false);
    label (g, "Quality", { 640, 536, 360, 14 }, juce::Justification::centred);
    g.setColour (dim2); g.setFont (font (F::man, 12.0f));
    const float latMs = (float) proc.getLatencySamples() * 1000.0f / (float) proc.getSampleRateSafe();
    g.drawText ("CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + u8 (" % \xc2\xb7 ") + juce::String (latMs, 1) + u8 (" ms \xc2\xb7 GM ") + dbs (proc.gainMatchDb.load()) + u8 (" \xc2\xb7 corr. ") + juce::String (proc.correlation.load(), 2) + (proc.dspErrors.load() > 0 ? u8 (" \xc2\xb7 recuperado ") + juce::String (proc.dspErrors.load()) : juce::String()),
                juce::Rectangle<float> (640, 688, 270, 18), juce::Justification::centredLeft, false);
    drawLogo (g, { 922, 684, 78, 26 });
}

void MainView::timerCallback()
{
    inMeter.setLevel (proc.inMeter.take()); outMeter.setLevel (proc.outMeter.take());
    engineView.tick();
    static int frame = 0;
    if (++frame % 6 == 0)
    {
        for (auto* t : tiles) t->refresh();
        trackCard.refresh();
        presetPill.repaint();
        modeSwitch.setSelected (proc.isPro() ? 1 : 0);
        abSwitch.setSelected (proc.getAbSlot());
        quality.setSelected (proc.getQuality());
        if (proc.isPro() != lastPro) { lastPro = proc.isPro(); selectEngine (selected); layoutTiles(); }
        repaint (juce::Rectangle<int> (500, 100, 520, 30));
        repaint (juce::Rectangle<int> (640, 686, 300, 26));
        if (! proc.isPro()) engineView.repaint();
    }
}

// =================================================================== Editor
ChainIvoEditor::ChainIvoEditor (ChainIvoProcessor& p) : AudioProcessorEditor (&p), proc (p), view ((theme::applySkin (loadDarkSkin()), p))
{
    const float s = juce::jlimit (0.75f, 2.0f, proc.uiScale.load());
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (kW * 3 / 4, kH * 3 / 4, kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
}
ChainIvoEditor::~ChainIvoEditor() = default;
void ChainIvoEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, kW, kH);
    proc.uiScale = s;
}
