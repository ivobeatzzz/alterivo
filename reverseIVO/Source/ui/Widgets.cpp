#include "Widgets.h"
#include "BinaryData.h"
#include "../core/Params.h"
#include <unordered_map>

namespace reverseivo::ui
{
using namespace reverseivo::theme;

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

static juce::Path rr (juce::Rectangle<float> r, float rad) { juce::Path p; p.addRoundedRectangle (r, rad); return p; }

// ------------------------------------------------------------------ sombras con caché (misma técnica que chainIVO)
namespace
{
struct ShadowCache : juce::DeletedAtShutdown
{
    struct Entry { juce::Image img; juce::uint32 lastUse = 0; };
    std::unordered_map<juce::uint64, Entry> map;
    juce::uint32 clock = 0;
    size_t pixels = 0;
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
} // namespace

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

static void innerShadow (juce::Graphics& g, juce::Rectangle<float> r, float rad, float alpha, int blur, int dy)
{
    juce::Path frame; frame.addRectangle (r.expanded (40.0f)); frame.addRoundedRectangle (r, rad); frame.setUsingNonZeroWinding (false);
    g.saveState(); g.reduceClipRegion (rr (r, rad));
    cachedShadow (g, frame, juce::Colours::black.withAlpha (alpha), blur, { 0, dy });
    g.restoreState();
}

void glass (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    const auto p = rr (r, rad);
    cachedShadow (g, p, juce::Colours::black.withAlpha (0.40f), 22, { 0, 18 });
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.085f), 0, r.getY(), juce::Colours::white.withAlpha (0.025f), 0, r.getBottom(), false));
    g.fillPath (p);
    innerShadow (g, r, rad, 0.25f, 20, -14);   // fondo del cristal más oscuro abajo (profundidad)
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    // brillo del borde superior
    juce::ColourGradient hl (juce::Colours::white.withAlpha (0.0f), r.getX() + rad, 0, juce::Colours::white.withAlpha (0.0f), r.getRight() - rad, 0, false);
    hl.addColour (0.5, juce::Colours::white.withAlpha (0.55f));
    g.setGradientFill (hl);
    g.fillRect (juce::Rectangle<float> (r.getX() + rad, r.getY(), r.getWidth() - rad * 2, 1.0f));
}

void raised (juce::Graphics& g, juce::Rectangle<float> r, float rad, bool shadow)
{
    const auto p = rr (r, rad);
    if (shadow)
    {
        cachedShadow (g, p, juce::Colours::black.withAlpha (0.50f), 10, { 0, 7 });
        cachedShadow (g, p, juce::Colours::black.withAlpha (0.40f), 3, { 0, 2 });
    }
    g.setGradientFill (juce::ColourGradient (raisedTop, 0, r.getY(), raisedBot, 0, r.getBottom(), false));
    g.fillPath (p);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (1.0f));
    // luz en el canto superior
    g.saveState(); g.reduceClipRegion (r.withHeight (std::max (2.0f, rad)).toNearestInt());
    g.setColour (juce::Colours::white.withAlpha (0.16f)); g.strokePath (rr (r.reduced (1.0f), rad - 1.0f), juce::PathStrokeType (1.0f));
    g.restoreState();
}

void well (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    g.setGradientFill (juce::ColourGradient (wellTop, 0, r.getY(), wellBot, 0, r.getBottom(), false));
    g.fillPath (rr (r, rad));
    innerShadow (g, r, rad, 0.75f, 5, 2);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (r.getX() + rad, r.getBottom() + 0.5f, r.getRight() - rad, r.getBottom() + 0.5f, 1.0f);
}

void creamPill (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    const auto p = rr (r, rad);
    cachedShadow (g, p, juce::Colours::black.withAlpha (0.5f), 14, { 0, 12 });
    cachedShadow (g, p, juce::Colours::black.withAlpha (0.45f), 3, { 0, 3 });
    juce::ColourGradient cg (juce::Colour (0xfff7f9f2), 0, r.getY(), creamLo, 0, r.getBottom(), false);
    cg.addColour (0.5, cream);
    g.setGradientFill (cg); g.fillPath (p);
    innerShadow (g, r, rad, 0.20f, 3, -2);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (1.0f));
    // reflejo superior
    const auto sh = juce::Rectangle<float> (r.getX() + rad * 1.1f, r.getY() + 2.0f, r.getWidth() - rad * 2.2f, r.getHeight() * 0.28f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), 0, sh.getY(), juce::Colours::white.withAlpha (0.0f), 0, sh.getBottom(), false));
    g.fillRoundedRectangle (sh, sh.getHeight() * 0.5f);
}

void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c, float size, float tracking)
{
    g.setColour (c); g.setFont (font (F::manX, size, tracking));
    g.drawText (t.toUpperCase(), r, j, false);
}

void dot (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
{
    g.setColour (col); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Firma IVO: mismo trazado que logo/ivo-logo-simbolo.svg (viewBox 100x100, letras entre x 13,45..86,55 y y 33,6..66,8)
    const float s = r.getHeight() / 33.2f, ox = r.getX() - 13.45f * s, oy = r.getY() - 33.6f * s;
    auto P = [&] (float x, float y) { return juce::Point<float> (ox + x * s, oy + y * s); };
    juce::PathStrokeType st (7.2f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (cream);
    juce::Path p;
    p.startNewSubPath (P (17.05f, 37.2f)); p.lineTo (P (17.05f, 63.2f));
    p.startNewSubPath (P (30.05f, 37.2f)); p.lineTo (P (37.05f, 63.2f)); p.lineTo (P (44.05f, 37.2f));
    g.strokePath (p, st);
    // O abierta por abajo (recorrido de mando): arco de 13,35 de radio centrado en (69,6, 50,2)
    const auto c = P (69.6f, 50.2f);
    juce::Path o; o.addCentredArc (c.x, c.y, 13.35f * s, 13.35f * s, 0.0f, -2.478f, 2.478f, true);
    g.strokePath (o, st);
    const auto d = P (73.714f, 45.297f);
    dot (g, d, 2.75f * s, logoDot);
}

void drawWordmark (juce::Graphics& g, juce::Point<float> tl, const juce::String& word, const juce::String& tagline)
{
    // Igual en todos los plugins IVO: palabra en Sora Light + IVO en Sora Bold, y la línea en Manrope ExtraBold
    juce::AttributedString t;
    t.append (word, font (F::soraLight, 34.0f, -0.01f), text);
    t.append ("IVO", font (F::soraBold, 34.0f, -0.01f), accent);
    t.draw (g, { tl.x, tl.y, 420.0f, 42.0f });
    label (g, tagline, { tl.x + 2.0f, tl.y + 44.0f, 420.0f, 14.0f }, juce::Justification::centredLeft, dim, 9.5f, 0.26f);
}

void drawIcon (juce::Graphics& g, Icon i, juce::Rectangle<float> r, juce::Colour c)
{
    const auto ctr = r.getCentre();
    const float s = std::min (r.getWidth(), r.getHeight());
    juce::PathStrokeType st (std::max (1.5f, s * 0.12f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;
    g.setColour (c);
    switch (i)
    {
        case Icon::rec: dot (g, ctr, s * 0.3f, rec); break;
        case Icon::play:
            p.startNewSubPath (ctr.x - s * 0.22f, ctr.y - s * 0.3f); p.lineTo (ctr.x + s * 0.3f, ctr.y); p.lineTo (ctr.x - s * 0.22f, ctr.y + s * 0.3f); p.closeSubPath();
            g.fillPath (p); g.strokePath (p, juce::PathStrokeType (s * 0.06f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); break;
        case Icon::stop: g.fillRoundedRectangle (juce::Rectangle<float> (s * 0.46f, s * 0.46f).withCentre (ctr), s * 0.06f); break;
        case Icon::import:
        case Icon::download:
            p.startNewSubPath (ctr.x, ctr.y - s * 0.34f); p.lineTo (ctr.x, ctr.y + s * 0.12f);
            p.startNewSubPath (ctr.x - s * 0.2f, ctr.y - s * 0.07f); p.lineTo (ctr.x, ctr.y + s * 0.13f); p.lineTo (ctr.x + s * 0.2f, ctr.y - s * 0.07f);
            p.startNewSubPath (ctr.x - s * 0.32f, ctr.y + s * 0.34f); p.lineTo (ctr.x + s * 0.32f, ctr.y + s * 0.34f);
            g.strokePath (p, st); break;
        case Icon::reset:
        {
            // flecha circular: arco de 300° y punta en el extremo superior
            const float rad = s * 0.3f, a0 = -0.35f, a1 = a0 + 5.2f;
            p.addCentredArc (ctr.x, ctr.y, rad, rad, 0, a0, a1, true);
            g.strokePath (p, st);
            const auto e = ctr.getPointOnCircumference (rad, a0);
            const auto tang = juce::Point<float> (std::cos (a0), std::sin (a0));         // dirección hacia atrás del arco
            const auto nrm = juce::Point<float> (std::sin (a0), -std::cos (a0));         // hacia fuera
            juce::Path h;
            h.startNewSubPath (e + (tang * -0.0f) + nrm * (s * 0.16f) + tang * (s * 0.02f));
            h.lineTo (e);
            h.lineTo (e + tang * (s * 0.18f) - nrm * (s * 0.02f));
            g.strokePath (h, st);
            break;
        }
        case Icon::left:
        case Icon::right:
        {
            const float d = i == Icon::left ? -1.0f : 1.0f;
            p.startNewSubPath (ctr.x - d * s * 0.12f, ctr.y - s * 0.24f); p.lineTo (ctr.x + d * s * 0.12f, ctr.y); p.lineTo (ctr.x - d * s * 0.12f, ctr.y + s * 0.24f);
            g.strokePath (p, st); break;
        }
        case Icon::ear:   // escuchar la región original: altavoz con onda
            p.startNewSubPath (ctr.x - s * 0.32f, ctr.y - s * 0.1f); p.lineTo (ctr.x - s * 0.16f, ctr.y - s * 0.1f); p.lineTo (ctr.x, ctr.y - s * 0.28f);
            p.lineTo (ctr.x, ctr.y + s * 0.28f); p.lineTo (ctr.x - s * 0.16f, ctr.y + s * 0.1f); p.lineTo (ctr.x - s * 0.32f, ctr.y + s * 0.1f); p.closeSubPath();
            g.fillPath (p);
            { juce::Path a; a.addCentredArc (ctr.x + s * 0.02f, ctr.y, s * 0.22f, s * 0.22f, 0, 0.6f, 2.55f, true); g.strokePath (a, st); }
            break;
        case Icon::drag:
            for (int k = -1; k <= 1; ++k) g.fillRoundedRectangle (juce::Rectangle<float> (s * 0.56f, s * 0.08f).withCentre ({ ctr.x, ctr.y + k * s * 0.2f }), s * 0.04f);
            break;
    }
}

// ------------------------------------------------------------------ LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff151a13));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a3524));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1c2319));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff2e3a28));
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff151a13));
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, juce::Colour (0xff2e3a28));
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0e0a));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2e3a28));
    setColour (juce::TextEditor::focusedOutlineColourId, accentDeep);
    setColour (juce::TextButton::buttonColourId, raisedTop);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::CaretComponent::caretColourId, accent);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    using PST = juce::PathStrokeType;
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float D = (float) std::min (w, h);
    const float R = D * 0.5f - 3.0f;                 // radio del arco
    const float body = D * 0.69f * 0.5f;             // radio del cuerpo
    const float ang = a0 + pos * (a1 - a0);

    // marcas de escala: principio, centro y final
    for (float f : { 0.0f, 0.5f, 1.0f })
    {
        const float a = a0 + f * (a1 - a0);
        const auto p1 = c.getPointOnCircumference (R + 4.0f, a), p2 = c.getPointOnCircumference (R + 7.0f, a);
        g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawLine ({ p1, p2 }, 1.2f);
    }
    // carril del arco (hundido) y valor
    juce::Path trackP; trackP.addCentredArc (c.x, c.y, R, R, 0, a0, a1, true);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (trackP, PST (4.0f, PST::curved, PST::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.09f)); g.strokePath (trackP, PST (2.0f, PST::curved, PST::rounded));
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path v; v.addCentredArc (c.x, c.y, R, R, 0, std::min (from, ang), std::max (from, ang), true);
        g.setColour (accent.withAlpha (0.14f)); g.strokePath (v, PST (6.0f, PST::curved, PST::rounded));
        g.setGradientFill (juce::ColourGradient (accentDeep, c.x - R, c.y + R, accentHi, c.x + R, c.y - R, false));
        g.strokePath (v, PST (2.5f, PST::curved, PST::rounded));
    }
    // hueco donde se asienta la perilla
    const float wr = body + 6.0f;
    juce::Path wellP; wellP.addEllipse (c.x - wr, c.y - wr, wr * 2, wr * 2);
    g.setGradientFill (juce::ColourGradient (wellTop, c.x, c.y - wr, wellBot, c.x, c.y + wr, false)); g.fillPath (wellP);
    {
        juce::Path ring; ring.addEllipse (c.x - wr - 30, c.y - wr - 30, (wr + 30) * 2, (wr + 30) * 2); ring.addEllipse (c.x - wr, c.y - wr, wr * 2, wr * 2);
        ring.setUsingNonZeroWinding (false);
        g.saveState(); g.reduceClipRegion (wellP);
        cachedShadow (g, ring, juce::Colours::black.withAlpha (0.85f), 5, { 0, 3 });
        g.restoreState();
    }
    // sombra proyectada y cuerpo con volumen
    juce::Path bodyP; bodyP.addEllipse (c.x - body, c.y - body, body * 2, body * 2);
    cachedShadow (g, bodyP, juce::Colours::black.withAlpha (0.70f), (int) (body * 0.3f), { 1, (int) (body * 0.3f) });
    juce::ColourGradient bg (knobTop, c.x, c.y - body, knobBot, c.x, c.y + body, false);
    bg.addColour (0.45, knobMid);
    g.setGradientFill (bg); g.fillPath (bodyP);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (bodyP, PST (1.0f));
    {   // canto superior iluminado (luz cenital, sin brillo blanco en la cara)
        juce::Path hi; hi.addCentredArc (c.x, c.y, body - 1.0f, body - 1.0f, 0, -1.0f, 1.0f, true);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.strokePath (hi, PST (1.0f));
    }
    // cara superior
    const float fr = body - 7.0f;
    juce::Path face; face.addEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2);
    juce::ColourGradient fg (faceTop, c.x, c.y - fr, faceBot, c.x, c.y + fr, false);
    g.setGradientFill (fg); g.fillPath (face);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (face, PST (1.0f));
    // marca crema
    const auto p1 = c.getPointOnCircumference (fr - 2.5f, ang), p2 = c.getPointOnCircumference (fr - 2.5f - body * 0.38f, ang);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawLine ({ p1.translated (0, 1.0f), p2.translated (0, 1.0f) }, 3.0f);
    juce::Path mark; mark.startNewSubPath (p1); mark.lineTo (p2);
    g.setColour (cream); g.strokePath (mark, PST (3.0f, PST::curved, PST::rounded));
}

juce::Font LookAndFeel::getPopupMenuFont() { return font (F::man, 14.0f); }
void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xff151a13));
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRect (0, 0, w, h, 1);
}
void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                     bool isTicked, bool, const juce::String& t, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator) { g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY())); return; }
    auto r = area.toFloat().reduced (4.0f, 1.0f);
    if (isHighlighted && isActive) { g.setColour (juce::Colour (0xff2a3524)); g.fillRoundedRectangle (r, 6.0f); }
    if (isTicked) dot (g, { r.getX() + 10.0f, r.getCentreY() }, 3.0f, accent);
    g.setColour (isActive ? text : dim); g.setFont (font (F::man, 14.0f));
    g.drawText (t, r.withTrimmedLeft (22.0f), juce::Justification::centredLeft, true);
}
void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& t)
{
    label (g, t, area.toFloat().withTrimmedLeft (12.0f), juce::Justification::centredLeft, dim, 9.5f, 0.24f);
}
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int) { return font (F::manX, 12.0f, 0.12f); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return font (F::man, 14.0f); }
juce::Font LookAndFeel::getAlertWindowTitleFont() { return font (F::soraBold, 18.0f); }

// ------------------------------------------------------------------ Segmented
Segmented::Segmented (juce::StringArray n, float f) : names (std::move (n)), fs (f) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

juce::Rectangle<float> Segmented::segmentBounds (int idx) const
{
    const auto inner = getLocalBounds().toFloat().reduced (plain ? 0.0f : 4.0f);
    const int n = std::max (1, names.size());
    const auto f = font (F::manX, fs, 0.14f);
    std::vector<float> nat ((size_t) n);
    float total = 0.0f;
    for (int i = 0; i < n; ++i) { nat[(size_t) i] = textWidth (f, names[i]) + 30.0f; total += nat[(size_t) i]; }
    float x = inner.getX();
    for (int i = 0; i < n; ++i)
    {
        const float w = nat[(size_t) i] + (inner.getWidth() - total) / (float) n;
        if (i == idx) return { x, inner.getY(), w, inner.getHeight() };
        x += w;
    }
    return {};
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (! plain) well (g, r, r.getHeight() * 0.5f);
    for (int i = 0; i < names.size(); ++i)
    {
        auto seg = segmentBounds (i);
        const bool on = i == selected;
        if (on)
        {
            const auto pr = seg.reduced (plain ? 2.0f : 1.0f, 0.0f);
            raised (g, pr, pr.getHeight() * 0.5f, true);
        }
        const auto f = font (F::manX, fs, 0.14f);
        const float tw = textWidth (f, names[i]);
        const float start = seg.getCentreX() - (tw + (on ? 13.0f : 0.0f)) * 0.5f;
        if (on) dot (g, { start + 2.5f, seg.getCentreY() }, 2.5f, accent);
        g.setColour (on ? text : dim); g.setFont (f);
        g.drawText (names[i], juce::Rectangle<float> (start + (on ? 13.0f : 0.0f), seg.getY(), tw + 12.0f, seg.getHeight()), juce::Justification::centredLeft, false);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    for (int k = 0; k < names.size(); ++k)
        if (segmentBounds (k).contains (e.position.x, (float) getHeight() * 0.5f))
        {
            if (k != selected) { selected = k; repaint(); if (onChange) onChange (k); }
            return;
        }
}

ParamChoice::ParamChoice (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::UndoManager* u, float fsz)
    : Segmented (dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))->choices, fsz), prm (s.getParameter (id)), um (u)
{
    onChange = [this] (int i)
    {
        if (um) um->beginNewTransaction();
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture();
    };
    setTooltip (helpFor (id));
    timerCallback();
    startTimerHz (12);
}
void ParamChoice::timerCallback() { setSelected ((int) std::lround (prm->convertFrom0to1 (prm->getValue()))); }

// ------------------------------------------------------------------ Toggle
void Toggle::paint (juce::Graphics& g)
{
    const float tx = labelLeft ? (float) getWidth() - 38.0f : 0.0f;
    const auto t = juce::Rectangle<float> (tx, (float) getHeight() * 0.5f - 11.0f, 38.0f, 22.0f);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (accentDeep, 0, t.getY(), juce::Colour (0xff6f9c52), 0, t.getBottom(), false));
        g.fillRoundedRectangle (t, 11.0f);
        innerShadow (g, t, 11.0f, 0.55f, 3, 1);
    }
    else well (g, t, 11.0f);
    const auto k = juce::Rectangle<float> (on ? t.getRight() - 20.0f : t.getX() + 2.0f, t.getY() + 2.0f, 18.0f, 18.0f);
    juce::Path kp; kp.addEllipse (k);
    cachedShadow (g, kp, juce::Colours::black.withAlpha (0.5f), 3, { 0, 2 });
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0, k.getY(), juce::Colour (0xffe3e8db), 0, k.getBottom(), false));
    g.fillPath (kp);
    if (text.isNotEmpty())
    {
        const auto area = labelLeft ? getLocalBounds().toFloat().withTrimmedRight (48.0f) : getLocalBounds().toFloat().withTrimmedLeft (48.0f);
        label (g, text, area, labelLeft ? juce::Justification::centredRight : juce::Justification::centredLeft, on ? theme::text : dim, 9.5f, 0.2f);
    }
}

ParamToggle::ParamToggle (juce::String n, juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::UndoManager* um)
    : Toggle (std::move (n)), prm (s.getParameter (id))
{
    setOn (prm->getValue() > 0.5f);
    setTooltip (helpFor (id));
    onChange = [this, um] (bool v) { if (um) um->beginNewTransaction(); prm->beginChangeGesture(); prm->setValueNotifyingHost (v ? 1.0f : 0.0f); prm->endChangeGesture(); };
    startTimerHz (12);
}
void ParamToggle::timerCallback() { setOn (prm->getValue() > 0.5f); }

// ------------------------------------------------------------------ Knob
Knob::Knob (juce::String n, juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::UndoManager* um, float d) : diameter (d), name (std::move (n))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    slider.onDragStart = [um] { if (um) um->beginNewTransaction(); };
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.setTooltip (helpFor (id));
}
void Knob::resized()
{
    slider.setBounds (juce::Rectangle<int> ((int) diameter, (int) diameter).withCentre ({ getWidth() / 2, (int) (diameter * 0.5f) + 2 }));
}
void Knob::paint (juce::Graphics& g)
{
    const float top = diameter + 12.0f;
    g.setColour (text);
    g.setFont (font (F::soraBold, compact ? 15.0f : 19.0f, -0.01f));
    const auto valueText = slider.getTextFromValue (slider.getValue());
    g.drawText (valueText, juce::Rectangle<float> (0, top, (float) getWidth(), compact ? 20.0f : 24.0f), juce::Justification::centred, false);
    label (g, name, { 0, top + (compact ? 22.0f : 28.0f), (float) getWidth(), 14.0f }, juce::Justification::centred, dim, compact ? 9.5f : 10.5f, 0.26f);
}

// ------------------------------------------------------------------ RoundButton
void RoundButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    r = juce::Rectangle<float> (std::min (r.getWidth(), r.getHeight()), std::min (r.getWidth(), r.getHeight())).withCentre (r.getCentre());
    if (pressed) r = r.translated (0, 1.0f);
    raised (g, r, r.getHeight() * 0.5f, ! pressed);
    if (active && icon == Icon::rec)
    {   // grabando: la luz roja ilumina el botón
        g.setColour (rec.withAlpha (0.22f)); g.fillEllipse (r.reduced (2.0f));
    }
    drawIcon (g, icon, r.reduced (r.getWidth() * 0.27f), active && icon != Icon::rec ? accent : text);
}

// ------------------------------------------------------------------ PillButton
void PillButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced ((float) pad);
    if (pressed) r = r.translated (0, 1.0f);
    const float rad = r.getHeight() * 0.5f;
    if (style == cream) creamPill (g, r, rad);
    else raised (g, r, rad, ! pressed);
    const auto f = style == cream ? font (F::manX, 13.0f, 0.24f) : font (F::manX, 10.0f, 0.16f);
    const float tw = textWidth (f, text);
    float x = r.getCentreX() - tw * 0.5f;
    if (hasIcon) { x += 10.0f; drawIcon (g, leading, { x - 22.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f }, style == cream ? ink : theme::soft); }
    if (pending) dot (g, { x - 12.0f, r.getCentreY() }, 3.5f, accentDeep);
    g.setColour (style == cream ? ink : theme::text); g.setFont (f);
    g.drawText (text, juce::Rectangle<float> (x, r.getY(), tw + 12.0f, r.getHeight()), juce::Justification::centredLeft, false);
}
} // namespace reverseivo::ui
