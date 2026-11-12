#include "Widgets.h"
#include "BinaryData.h"
#include "../core/Params.h"
#include <unordered_map>
#include <map>

namespace levelivo::ui
{
using namespace levelivo::theme;

Fonts::Fonts()
{
    auto load = [] (const char* d, int s) { return juce::Typeface::createSystemTypefaceFor (d, (size_t) s); };
    soraLight = load (BinaryData::SoraLight_ttf, BinaryData::SoraLight_ttfSize);
    soraSemi  = load (BinaryData::SoraSemiBold_ttf, BinaryData::SoraSemiBold_ttfSize);
    soraBold  = load (BinaryData::SoraBold_ttf, BinaryData::SoraBold_ttfSize);
    manBold   = load (BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize);
    manX      = load (BinaryData::ManropeExtraBold_ttf, BinaryData::ManropeExtraBold_ttfSize);
}

juce::Font font (F face, float size, float tracking)
{
    static juce::SharedResourcePointer<Fonts> f;
    juce::Typeface::Ptr tf;
    switch (face)
    {
        case F::soraLight: tf = f->soraLight; break;
        case F::soraSemi:  tf = f->soraSemi; break;
        case F::soraBold:  tf = f->soraBold; break;
        case F::man:       tf = f->manBold; break;
        case F::manX:      tf = f->manX; break;
    }
    auto o = tf != nullptr ? juce::FontOptions (tf) : juce::FontOptions (size, juce::Font::bold);
    return juce::Font (o.withPointHeight (size).withKerningFactor (tracking));
}
float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

juce::String helpFor (const juce::String& p)
{
    static const std::map<juce::String, const char*> h = {
        { id::mode, "Natural toca poco, Balanced equilibra, Controlled iguala m\xc3\xa1s. Manual: t\xc3\xba decides cada control." },
        { id::engine, "OFFLINE: analiza la toma entera y decide como un ingeniero (recomendado). REALTIME: modo simple con 300 ms de latencia." },
        { id::autoApply, "AUTO APPLY aplica el resultado al terminar el an\xc3\xa1lisis. REVISAR ANTES te deja verlo y o\xc3\xadrlo antes de aplicar." },
        { id::listen, "Qu\xc3\xa9 suena: la toma original, s\xc3\xb3lo la nivelaci\xc3\xb3n, s\xc3\xb3lo la limpieza o el resultado final. Con el volumen igualado, ninguna gana por sonar m\xc3\xa1s fuerte." },
        { id::matchLoud, "Iguala el volumen de todas las escuchas al del original (comparaci\xc3\xb3n justa y el clip mantiene su volumen)." },
        { id::targetOn, "Lleva la voz a un nivel fijo (LUFS) adem\xc3\xa1s de nivelarla." },
        { id::target, "Nivel medio de la voz cuando el objetivo est\xc3\xa1 activo." },
        { id::output, "Volumen de salida." },
        { id::bypass, "Deja pasar la se\xc3\xb1" "al sin tocar." },
        { id::phrase, "Corrige la deriva lenta: frases que quedaron m\xc3\xa1s bajas o altas (distancia al micro, cansancio)." },
        { id::word, "Corrige palabras que se pierden o sobresalen dentro de su frase." },
        { id::syllable, "Recupera s\xc3\xadlabas que se caen (sobre todo finales). S\xc3\xb3lo sube, y como mucho 4 dB." },
        { id::intensity, "Cu\xc3\xa1nto de la correcci\xc3\xb3n decidida se aplica." },
        { id::sensitivity, "Baja: s\xc3\xb3lo errores claros (respeta la expresi\xc3\xb3n). Alta: corrige tambi\xc3\xa9n variaciones peque\xc3\xb1" "as." },
        { id::maxGain, "Lo m\xc3\xa1ximo que puede subir un tramo." },
        { id::maxAtt, "Lo m\xc3\xa1ximo que puede bajar un tramo." },
        { id::smoothing, "Rapidez de las transiciones entre correcciones." },
        { id::peakProtect, "La curva nunca hace pasar un pico de -1 dBFS." },
        { id::consonants, "Los cambios de ganancia terminan antes de cada ataque: consonantes y golpes de voz intactos." },
        { id::beatFollow, "Con el beat en el sidechain: la voz acompa\xc3\xb1" "a un poco a la energ\xc3\xad" "a de la base." },
        { id::rtSpeed, "Modo tiempo real: rapidez con la que sigue a la voz." },
        { id::noiseOn, "Ruido de fondo: aprende el ruido en los silencios y lo reduce, sobre todo entre frases." },
        { id::noise, "Reducci\xc3\xb3n m\xc3\xa1xima entre frases (durante la voz se reduce mucho menos)." },
        { id::bleedOn, "Sangrado de cascos. Con el beat en el sidechain lo resta de verdad; sin \xc3\xa9l, s\xc3\xb3lo lo baja donde no cantas." },
        { id::bleed, "Cantidad de sangrado que se quita." },
        { id::otherOn, "Otras voces de fondo (frases mucho m\xc3\xa1s flojas o con otro tono)." },
        { id::other, "Cu\xc3\xa1nto se bajan las otras voces." },
        { id::hitsOn, "Golpes al micro (graves sueltos): paso alto s\xc3\xb3lo en ese instante." },
        { id::hits, "Cantidad de correcci\xc3\xb3n de los golpes." },
        { id::mouthOn, "Ruidos de boca y chasquidos: se reconstruyen las muestras afectadas." },
        { id::mouth, "Sensibilidad: m\xc3\xa1s alta tambi\xc3\xa9n quita clics suaves." },
        { id::breathOn, "Respiraciones: se bajan, nunca se borran del todo." },
        { id::breath, "Cu\xc3\xa1nto se bajan las respiraciones." },
        { id::clipRepair, "DETECTAR s\xc3\xb3lo marca el clipping; REPARAR reconstruye los picos recortados cortos." },
    };
    auto it = h.find (p);
    return it != h.end() ? juce::String::fromUTF8 (it->second) : juce::String();
}

static juce::Path rr (juce::Rectangle<float> r, float rad) { juce::Path p; p.addRoundedRectangle (r, rad); return p; }

// ------------------------------------------------------------------ sombras con caché (la forma se difumina una vez)
namespace
{
struct ShadowCache : juce::DeletedAtShutdown
{
    struct Entry { juce::Image img; juce::uint32 lastUse = 0; };
    std::unordered_map<juce::uint64, Entry> map;
    juce::uint32 clock = 0; size_t pixels = 0;
};
ShadowCache& shadowCache() { static ShadowCache* c = new ShadowCache(); return *c; }
juce::uint64 shapeKey (const juce::Path& p, juce::Point<float> origin, int radius)
{
    juce::uint64 h = 1469598103934665603ull;
    auto mix = [&h] (juce::int64 v) { h ^= (juce::uint64) v; h *= 1099511628211ull; };
    mix (radius);
    for (juce::Path::Iterator it (p); it.next();)
    {
        mix ((juce::int64) it.elementType);
        for (float v : { it.x1 - origin.x, it.y1 - origin.y, it.x2 - origin.x, it.y2 - origin.y, it.x3 - origin.x, it.y3 - origin.y })
            mix ((juce::int64) std::lround (v * 8.0f));
    }
    return h;
}
}
void cachedShadow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, int radius, juce::Point<int> offset)
{
    if (radius <= 0 || path.isEmpty()) return;
    const auto area = (path.getBounds().getSmallestIntegerContainer() + offset).expanded (radius + 1);
    if (area.getWidth() <= 2 || area.getHeight() <= 2 || ! g.getClipBounds().intersects (area)) return;
    auto& c = shadowCache();
    const auto key = shapeKey (path, area.getPosition().toFloat() - offset.toFloat(), radius);
    auto it = c.map.find (key);
    if (it == c.map.end())
    {
        juce::Image img (juce::Image::SingleChannel, area.getWidth(), area.getHeight(), true, juce::SoftwareImageType());
        {
            juce::Graphics g2 (img);
            g2.setColour (juce::Colours::white);
            g2.fillPath (path, juce::AffineTransform::translation ((float) (offset.x - area.getX()), (float) (offset.y - area.getY())));
        }
        img.getPixelData()->applySingleChannelBoxBlurEffect (radius);
        c.pixels += (size_t) area.getWidth() * (size_t) area.getHeight();
        while (c.pixels > 6'000'000 && ! c.map.empty())
        {
            auto oldest = c.map.begin();
            for (auto e = c.map.begin(); e != c.map.end(); ++e) if (e->second.lastUse < oldest->second.lastUse) oldest = e;
            c.pixels -= (size_t) oldest->second.img.getWidth() * (size_t) oldest->second.img.getHeight();
            c.map.erase (oldest);
        }
        it = c.map.emplace (key, ShadowCache::Entry { img, 0 }).first;
    }
    it->second.lastUse = ++c.clock;
    g.setColour (colour);
    g.drawImageAt (it->second.img, area.getX(), area.getY(), true);
}

void cardBg (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    const auto p = rr (r, rad);
    cachedShadow (g, p, shadow.withAlpha (0.07f), 24, { 0, 8 });
    g.setColour (card); g.fillPath (p);
    g.setColour (deep.withAlpha (0.06f)); g.strokePath (p, juce::PathStrokeType (1.0f));
}
void caps (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c, float size, float tracking)
{
    // mayúsculas también para las vocales con tilde y la ñ (toUpperCase depende del sistema)
    auto up = t.toUpperCase();
    const char* pairs[][2] = { { "\xc3\xa1", "\xc3\x81" }, { "\xc3\xa9", "\xc3\x89" }, { "\xc3\xad", "\xc3\x8d" }, { "\xc3\xb3", "\xc3\x93" }, { "\xc3\xba", "\xc3\x9a" }, { "\xc3\xb1", "\xc3\x91" } };
    for (auto& pr : pairs) up = up.replace (juce::String::fromUTF8 (pr[0]), juce::String::fromUTF8 (pr[1]));
    g.setColour (c); g.setFont (font (F::manX, size, tracking));
    g.drawText (up, r, j, false);
}
void greenDot (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setColour (dot); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
}
void drawLogo (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour inkC)
{
    // Firma IVO: I, V y O abierta con el punto (mismo trazado que logo/ivo-logo-simbolo.svg)
    const float s = r.getHeight() / 34.0f, x = r.getX(), y = r.getY(), w = 7.2f * s;
    juce::PathStrokeType st (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (inkC);
    juce::Path p;
    p.startNewSubPath (x + 3.6f * s, y + 3.6f * s); p.lineTo (x + 3.6f * s, y + 30.4f * s);
    p.startNewSubPath (x + 16.6f * s, y + 3.6f * s); p.lineTo (x + 23.6f * s, y + 30.4f * s); p.lineTo (x + 30.6f * s, y + 3.6f * s);
    g.strokePath (p, st);
    juce::Path o; o.addCentredArc (x + 55.6f * s, y + 17.0f * s, 13.35f * s, 13.35f * s, 0.0f, -2.478f, 2.478f, true);
    g.strokePath (o, st);
    const float a = 0.698f; const float cx = x + (55.6f + 6.4f * std::sin (a)) * s, cy = y + (17.0f - 6.4f * std::cos (a)) * s;
    g.setColour (juce::Colour (0xff8dcb34)); g.fillEllipse (cx - 2.75f * s, cy - 2.75f * s, 5.5f * s, 5.5f * s);
}

// ------------------------------------------------------------------ LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, card);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lime);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::TooltipWindow::backgroundColourId, ink);
    setColour (juce::TooltipWindow::textColourId, limeText);
    setColour (juce::TooltipWindow::outlineColourId, ink);
    setColour (juce::AlertWindow::backgroundColourId, card);
    setColour (juce::AlertWindow::textColourId, ink);
    setColour (juce::TextEditor::backgroundColourId, cardSoft);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::outlineColourId, track);
    setColour (juce::TextEditor::focusedOutlineColourId, dot);
    setColour (juce::TextButton::buttonColourId, cardSoft);
    setColour (juce::TextButton::textColourOffId, ink);
}
juce::Font LookAndFeel::getPopupMenuFont() { return font (F::man, 14.0f); }
void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (card);
    g.setColour (shadow.withAlpha (0.12f)); g.drawRect (0, 0, w, h, 1);
}

// ------------------------------------------------------------------ Segmented
Segmented::Segmented (juce::StringArray n, float f) : names (std::move (n)), fs (f) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
float Segmented::preferredWidth() const
{
    float w = 8.0f; const auto f = font (F::manX, fs, 0.17f);
    for (auto& s : names) w += textWidth (f, s) + 26.0f + 12.0f;
    return w;
}
void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (segTrack); g.fillRoundedRectangle (r, rad);
    {   // carril hundido: sombra interior suave
        juce::Path frame; frame.addRectangle (r.expanded (30.0f)); frame.addRoundedRectangle (r, rad); frame.setUsingNonZeroWinding (false);
        g.saveState(); g.reduceClipRegion (rr (r, rad));
        cachedShadow (g, frame, shadow.withAlpha (0.14f), 4, { 0, 1 });
        g.restoreState();
    }
    for (int i = 0; i < names.size(); ++i)
    {
        auto seg = segmentBounds (i);
        const bool on = i == selected;
        auto f = font (F::manX, fs, 0.17f);
        float tw = textWidth (f, names[i]);
        float size = fs;
        while (tw > seg.getWidth() - 10.0f && size > 6.5f) { size -= 0.5f; f = font (F::manX, size, 0.1f); tw = textWidth (f, names[i]); }
        if (on)
        {
            const auto pr = seg;
            juce::Path pp; pp.addRoundedRectangle (pr, pr.getHeight() * 0.5f);
            cachedShadow (g, pp, shadow.withAlpha (0.18f), 4, { 0, 2 });
            g.setColour (card); g.fillPath (pp);
        }
        const bool withDot = on && tw + 22.0f < seg.getWidth();
        const float start = seg.getCentreX() - (tw + (withDot ? 12.0f : 0.0f)) * 0.5f;
        if (withDot) greenDot (g, { start + 2.5f, seg.getCentreY() }, 2.6f);
        g.setColour (on ? ink : juce::Colour (0xff7f8a76)); g.setFont (f);
        g.drawText (names[i], juce::Rectangle<float> (start + (withDot ? 12.0f : 0.0f), seg.getY(), tw + 8.0f, seg.getHeight()), juce::Justification::centredLeft, false);
    }
}
juce::Rectangle<float> Segmented::segmentBounds (int idx) const
{
    const auto inner = getLocalBounds().toFloat().reduced (3.0f);
    const int n = std::max (1, names.size());
    const auto f = font (F::manX, fs, 0.17f);
    std::vector<float> nat ((size_t) n); float total = 0.0f;
    for (int i = 0; i < n; ++i) { nat[(size_t) i] = textWidth (f, names[i]) + 26.0f; total += nat[(size_t) i]; }
    float x = inner.getX();
    for (int i = 0; i < n; ++i)
    {
        const float w = nat[(size_t) i] * inner.getWidth() / total;
        if (i == idx) return { x, inner.getY(), w, inner.getHeight() };
        x += w;
    }
    return {};
}
void Segmented::mouseDown (const juce::MouseEvent& e)
{
    int i = selected;
    for (int k = 0; k < names.size(); ++k) if (segmentBounds (k).getHorizontalRange().contains (e.position.x)) i = k;
    if (i != selected) { selected = i; repaint(); if (onChange) onChange (i); }
}

ParamSeg::ParamSeg (juce::AudioProcessorValueTreeState& s, const juce::String& pid, juce::StringArray n, juce::UndoManager* u, bool inv)
    : Segmented (std::move (n)), prm (s.getParameter (pid)), um (u), isBool (dynamic_cast<juce::AudioParameterBool*> (s.getParameter (pid)) != nullptr), invert (inv)
{
    onChange = [this] (int i)
    {
        if (um) um->beginNewTransaction();
        float v = (float) i;
        if (isBool) v = invert ? (i == 0 ? 1.0f : 0.0f) : (float) i;
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture();
    };
    setTooltip (helpFor (pid));
    timerCallback();
    startTimerHz (12);
}
void ParamSeg::timerCallback()
{
    const float v = prm->convertFrom0to1 (prm->getValue());
    if (isBool) setSelected (invert ? (v > 0.5f ? 0 : 1) : (v > 0.5f ? 1 : 0));
    else setSelected ((int) std::lround (v));
}

// ------------------------------------------------------------------ Switch
Switch::Switch (juce::AudioProcessorValueTreeState& s, const juce::String& pid, juce::String t) : prm (s.getParameter (pid)), text (std::move (t))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (helpFor (pid));
    on = prm->getValue() > 0.5f;
    startTimerHz (12);
}
Switch::Switch (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
void Switch::timerCallback() { if (prm) setOn (prm->getValue() > 0.5f); }
void Switch::mouseDown (const juce::MouseEvent&)
{
    on = ! on; repaint();
    if (prm) { prm->beginChangeGesture(); prm->setValueNotifyingHost (on ? 1.0f : 0.0f); prm->endChangeGesture(); }
    if (onChange) onChange (on);
}
void Switch::paint (juce::Graphics& g)
{
    const auto t = juce::Rectangle<float> (0, (float) getHeight() * 0.5f - 7.0f, 25.0f, 14.0f);
    g.setColour (on ? ink : juce::Colour (0xffd8ded0)); g.fillRoundedRectangle (t, 7.0f);
    const auto k = juce::Rectangle<float> (on ? t.getRight() - 12.0f : t.getX() + 2.0f, t.getY() + 2.0f, 10.0f, 10.0f);
    g.setColour (on ? lime : card); g.fillEllipse (k);
    if (text.isNotEmpty())
    {
        g.setColour (on ? ink : dim); g.setFont (font (F::man, 11.0f));
        g.drawText (text, getLocalBounds().toFloat().withTrimmedLeft (33.0f), juce::Justification::centredLeft, false);
    }
}

// ------------------------------------------------------------------ HSlider
HSlider::HSlider (juce::AudioProcessorValueTreeState& s, const juce::String& pid, juce::String n, juce::UndoManager* u)
    : prm (s.getParameter (pid)), um (u), name (std::move (n))
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setTooltip (helpFor (pid));
    startTimerHz (20);
}
juce::Rectangle<float> HSlider::barArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (nameWidth); r.removeFromRight (valueWidth + detailWidth + 12.0f);
    return r.withSizeKeepingCentre (r.getWidth(), 14.0f);
}
void HSlider::timerCallback() { const float v = prm->getValue(); if (std::abs (v - shown) > 1e-4f) { shown = v; repaint(); } }
void HSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (enabledLook ? body : dim); g.setFont (font (F::man, 11.0f));
    g.drawText (name, r.withWidth (nameWidth), juce::Justification::centredLeft, true);
    const auto bar = barArea();
    const auto line = bar.withSizeKeepingCentre (bar.getWidth(), 4.0f);
    g.setColour (track); g.fillRoundedRectangle (line, 2.0f);
    const float v = prm->getValue();
    g.setColour (enabledLook ? deep : juce::Colour (0xffc3cbb9));
    g.fillRoundedRectangle (line.withWidth (line.getWidth() * v), 2.0f);
    const float x = line.getX() + line.getWidth() * v;
    juce::Path k; k.addEllipse (x - 7.0f, line.getCentreY() - 7.0f, 14.0f, 14.0f);
    cachedShadow (g, k, shadow.withAlpha (0.35f), 3, { 0, 1 });
    g.setColour (card); g.fillPath (k);
    if (enabledLook) greenDot (g, { x, line.getCentreY() }, 3.0f);
    auto right = r.removeFromRight (valueWidth + detailWidth);
    g.setColour (enabledLook ? ink : dim); g.setFont (font (F::soraSemi, 11.5f));
    g.drawText (prm->getCurrentValueAsText(), right.removeFromLeft (valueWidth), juce::Justification::centredRight, false);
    if (detailWidth > 0)
    {
        g.setColour (detailColour); g.setFont (font (F::man, 9.5f));
        g.drawText (detail, right, juce::Justification::centredRight, false);
    }
}
void HSlider::setFromX (float x)
{
    const auto b = barArea();
    const float v = juce::jlimit (0.0f, 1.0f, (x - b.getX()) / std::max (1.0f, b.getWidth()));
    prm->setValueNotifyingHost (v);
}
void HSlider::mouseDown (const juce::MouseEvent& e)
{
    if (um) um->beginNewTransaction();
    prm->beginChangeGesture();
    if (barArea().expanded (8.0f, 6.0f).contains (e.position)) setFromX (e.position.x);
}
void HSlider::mouseDrag (const juce::MouseEvent& e) { setFromX (e.position.x); }
void HSlider::mouseUp (const juce::MouseEvent&) { prm->endChangeGesture(); }
void HSlider::mouseDoubleClick (const juce::MouseEvent&)
{
    prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture();
}
void HSlider::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    prm->beginChangeGesture(); prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.05f)); prm->endChangeGesture();
}

// ------------------------------------------------------------------ ValueBox
ValueBox::ValueBox (juce::AudioProcessorValueTreeState& s, const juce::String& pid, juce::String n, juce::UndoManager* u, float st)
    : prm (s.getParameter (pid)), um (u), name (std::move (n)), step (st)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip (helpFor (pid));
    startTimerHz (20);
}
void ValueBox::timerCallback() { const float v = prm->getValue(); if (std::abs (v - shown) > 1e-5f) { shown = v; repaint(); } }
void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (soft) { g.setColour (cardSoft); g.fillRoundedRectangle (r, 10.0f); r = r.reduced (10.0f, 7.0f); }
    caps (g, name, r.removeFromTop (11.0f), juce::Justification::centredLeft, dim, 7.5f, 0.2f);
    auto txt = prm->getCurrentValueAsText();
    juce::String unit;
    if (txt.endsWith (" dB")) { unit = "dB"; txt = txt.dropLastCharacters (3); }
    else if (txt.endsWith (" LUFS")) { unit = "LUFS"; txt = txt.dropLastCharacters (5); }
    txt = txt.replace (".", ",").replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92"));
    const auto f = font (F::soraSemi, big ? 20.0f : 16.0f);
    g.setColour (ink); g.setFont (f);
    g.drawText (txt, r, juce::Justification::bottomLeft, false);
    if (unit.isNotEmpty())
    {
        g.setColour (dim); g.setFont (font (F::man, 11.0f));
        g.drawText (unit, r.withTrimmedLeft (textWidth (f, txt) + 3.0f), juce::Justification::bottomLeft, false);
    }
}
void ValueBox::mouseDown (const juce::MouseEvent&) { if (um) um->beginNewTransaction(); prm->beginChangeGesture(); startVal = prm->convertFrom0to1 (prm->getValue()); }
void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    const float v = startVal - (float) e.getDistanceFromDragStartY() * step * (e.mods.isShiftDown() ? 0.2f : 1.0f);
    prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
}
void ValueBox::mouseUp (const juce::MouseEvent&) { prm->endChangeGesture(); }
void ValueBox::mouseDoubleClick (const juce::MouseEvent&) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
void ValueBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float v = prm->convertFrom0to1 (prm->getValue()) + (w.deltaY > 0 ? 1.0f : -1.0f) * step * 10.0f;
    prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v))); prm->endChangeGesture();
}

// ------------------------------------------------------------------ Button
void Button::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const float a = enabled ? 1.0f : 0.45f;
    juce::Colour fg = ink;
    switch (style)
    {
        case lime:  g.setColour (theme::lime.withAlpha (a).brighter (hover ? 0.06f : 0.0f)); g.fillRoundedRectangle (r, rad); break;
        case dark:  g.setColour (ink.withAlpha (a)); g.fillRoundedRectangle (r, rad); fg = limeText; break;
        case light: g.setColour ((active ? theme::lime : juce::Colour (0xffeef2e8)).withAlpha (a)); g.fillRoundedRectangle (r, rad); break;
        case outline:
            g.setColour (active ? ink : card); g.fillRoundedRectangle (r, rad);
            g.setColour (ink.withAlpha (a)); g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.5f);
            if (active) fg = limeText;
            break;
    }
    if (hover && style != lime) { g.setColour (juce::Colours::white.withAlpha (style == dark || active ? 0.06f : 0.25f)); g.fillRoundedRectangle (r, rad); }
    const auto f = font (F::manX, fontSize, 0.17f);
    const float tw = textWidth (f, text);
    float x = r.getCentreX() - tw * 0.5f;
    if (redDot)
    {
        const bool lit = ! blink || (juce::Time::getMillisecondCounter() / 450) % 2 == 0;
        x += 9.0f;
        g.setColour (red.withAlpha (lit ? 1.0f : 0.35f));
        g.fillEllipse (x - 17.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    }
    g.setColour (fg.withAlpha (a)); g.setFont (f);
    g.drawText (text, juce::Rectangle<float> (x, r.getY(), tw + 6.0f, r.getHeight()), juce::Justification::centredLeft, false);
}

// ------------------------------------------------------------------ Meter
void Meter::setLevelDb (float d)
{
    db = isGr ? d : (d > db ? d : std::max (d, db - 1.2f));
    if (std::abs (db - painted) < 0.25f) return;
    painted = db; repaint();
}
void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (dim); g.setFont (font (F::man, 9.5f));
    g.drawText (lbl, r.removeFromLeft (30.0f), juce::Justification::centredLeft, false);
    const auto bar = r.withSizeKeepingCentre (r.getWidth(), 6.0f);
    g.setColour (track); g.fillRoundedRectangle (bar, 3.0f);
    if (isGr)
    {
        // ganancia aplicada: desde el centro (0 dB) hacia la izquierda (baja) o la derecha (sube), ±12 dB
        const float c = bar.getCentreX(), w = bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, db / 12.0f);
        auto fill = w >= 0 ? juce::Rectangle<float> (c, bar.getY(), w, bar.getHeight()) : juce::Rectangle<float> (c + w, bar.getY(), -w, bar.getHeight());
        g.setColour (lime); g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (dim.withAlpha (0.6f)); g.fillRect (c - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
        return;
    }
    const float f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -1.0f ? red : (lbl == "IN" ? juce::Colour (0xff7fb85a) : deep));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * f), 3.0f);
}
} // namespace levelivo::ui
