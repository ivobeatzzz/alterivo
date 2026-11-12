#include "Widgets.h"
#include "BinaryData.h"
#include "../core/Params.h"

namespace chainivo::ui
{
using namespace chainivo::theme;

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
static juce::PropertiesFile::Options uiOptions()
{
    juce::PropertiesFile::Options o; o.applicationName = "chainIVO"; o.folderName = "IVO Beatz"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
    return o;
}
bool loadDarkSkin() { juce::PropertiesFile f (uiOptions()); return f.getBoolValue ("darkSkin", false); }
void saveDarkSkin (bool d) { juce::PropertiesFile f (uiOptions()); f.setValue ("darkSkin", d); f.saveIfNeeded(); }
float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

static juce::Path rr (juce::Rectangle<float> r, float rad) { juce::Path p; p.addRoundedRectangle (r, rad); return p; }

void shadowOnly (juce::Graphics& g, juce::Rectangle<float> r, float rad, float lift)
{
    const auto p = rr (r, rad);
    juce::DropShadow (shadow.withAlpha (0.20f * lift), (int) (26 * lift), { 0, (int) (12 * lift) }).drawForPath (g, p);
    juce::DropShadow (shadow.withAlpha (0.09f), 6, { 0, 2 }).drawForPath (g, p);
}

// lift > 0: dibuja también su sombra; lift = 0: sólo la superficie (la sombra la pinta el contenedor, sin recortes)
void raised (juce::Graphics& g, juce::Rectangle<float> r, float rad, float lift)
{
    const auto p = rr (r, rad);
    if (lift > 0.0f) shadowOnly (g, r, rad, lift);
    g.setGradientFill (juce::ColourGradient (surface, 0, r.getY(), surfaceLo, 0, r.getBottom(), false));
    g.fillPath (p);
    g.setColour (shadow.withAlpha (0.05f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    // brillo en el borde superior
    g.setColour (highlight);
    juce::Path top; top.addRoundedRectangle (r.reduced (1.0f).withHeight (rad * 2), rad - 1.0f);
    g.saveState(); g.reduceClipRegion (r.withHeight (rad).toNearestInt()); g.strokePath (top, juce::PathStrokeType (1.0f)); g.restoreState();
}

static void innerShadow (juce::Graphics& g, juce::Rectangle<float> r, float rad, float alpha, int blur, int dy)
{
    juce::Path frame; frame.addRectangle (r.expanded (40.0f)); frame.addRoundedRectangle (r, rad); frame.setUsingNonZeroWinding (false);
    g.saveState(); g.reduceClipRegion (rr (r, rad));
    juce::DropShadow (shadow.withAlpha (alpha), blur, { 0, dy }).drawForPath (g, frame);
    g.restoreState();
}

void well (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    g.setGradientFill (juce::ColourGradient (wellTop, 0, r.getY(), wellBot, 0, r.getBottom(), false));
    g.fillPath (rr (r, rad));
    innerShadow (g, r, rad, 0.22f, 7, 3);
    g.setColour (highlight);
    g.drawLine (r.getX() + rad, r.getBottom() + 0.5f, r.getRight() - rad, r.getBottom() + 0.5f, 1.0f);
}

void screen (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    g.setGradientFill (juce::ColourGradient (screenTop, 0, r.getY(), screenBot, 0, r.getBottom(), false));
    g.fillPath (rr (r, rad));
    innerShadow (g, r, rad, 0.16f, 6, 2);
    g.setColour (shadow.withAlpha (0.05f));
    g.strokePath (rr (r, rad), juce::PathStrokeType (1.0f));
}

void limeFill (juce::Graphics& g, juce::Rectangle<float> r, float rad)
{
    const auto p = rr (r, rad);
    juce::DropShadow (juce::Colour (0xff6eaa28).withAlpha (0.5f), 10, { 0, 4 }).drawForPath (g, p);
    g.setGradientFill (juce::ColourGradient (limeLight, 0, r.getY(), limeDark, 0, r.getBottom(), false));
    g.fillPath (p);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawLine (r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
}

void logoDot (juce::Graphics& g, juce::Point<float> c, float r, bool on)
{
    if (on)
    {
        g.setColour (limeDot.withAlpha (0.45f)); g.fillEllipse (c.x - r * 2, c.y - r * 2, r * 4, r * 4);
        g.setColour (onLime); g.fillEllipse (c.x - r - 2, c.y - r - 2, (r + 2) * 2, (r + 2) * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6f58c), c.x - r * 0.4f, c.y - r * 0.4f, juce::Colour (0xff8dcb34), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    }
    else
    {
        g.setColour (track); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (shadow.withAlpha (0.18f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    }
}

void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c, float size)
{
    g.setColour (c); g.setFont (font (F::manX, size, 0.26f));
    g.drawText (t.toUpperCase(), r, j, false);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Firma IVO: I, V y O abierta con el punto lima (mismo trazado que logo/ivo-logo-simbolo.svg)
    const float s = r.getHeight() / 34.0f, x = r.getX(), y = r.getY(), w = 7.2f * s;
    juce::PathStrokeType st (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (ink);
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
    setColour (juce::PopupMenu::backgroundColourId, surface);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, limeLight);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, deep);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, ink);
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffe8f7c8));
    setColour (juce::TooltipWindow::outlineColourId, ink);
    setColour (juce::AlertWindow::backgroundColourId, surface);
    setColour (juce::AlertWindow::textColourId, ink);
    setColour (juce::TextButton::buttonColourId, surface);
    setColour (juce::TextButton::textColourOffId, ink);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) s.getProperties().getWithDefault ("radius", 20.0f);
    const bool big = R >= 40.0f;
    const float ring = R + (big ? 17.0f : (R < 20 ? 8.0f : 12.0f)), rw = big ? 7.0f : (R < 20 ? 3.5f : 5.0f);
    const float tickR = ring + (big ? 11.0f : (R < 20 ? 6.0f : 9.0f));
    const float ang = a0 + pos * (a1 - a0);
    using PST = juce::PathStrokeType;

    for (int k = 0; k <= 10; ++k)
    {
        const auto p = c.getPointOnCircumference (tickR, a0 + (float) k / 10.0f * (a1 - a0));
        const float r = big ? 1.7f : (R < 20 ? 1.0f : 1.35f);
        g.setColour (tick); g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
    }
    juce::Path trackP; trackP.addCentredArc (c.x, c.y, ring, ring, 0, a0, a1, true);
    g.setColour (track); g.strokePath (trackP, PST (rw + 2.0f, PST::curved, PST::rounded));
    g.setColour (track.interpolatedWith (surface, 0.45f)); g.strokePath (trackP, PST (rw - 1.0f, PST::curved, PST::rounded));
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.02f)
    {
        juce::Path v; v.addCentredArc (c.x, c.y, ring, ring, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setGradientFill (juce::ColourGradient (green, c.x - ring, c.y + ring, juce::Colour (0xffa9e04b), c.x + ring, c.y - ring, false));
        g.strokePath (v, PST (rw, PST::curved, PST::rounded));
    }
    // cuerpo con volumen
    juce::Path body; body.addEllipse (c.x - R, c.y - R, R * 2, R * 2);
    juce::DropShadow (shadow.withAlpha (0.28f), (int) (R * 0.5f + 4), { 0, (int) (R * 0.16f + 1) }).drawForPath (g, body);
    g.setGradientFill (juce::ColourGradient (knobTop, c.x - R * 0.16f, c.y - R * 0.4f, knobBot, c.x + R * 0.5f, c.y + R, true));
    g.fillPath (body);
    g.setColour (shadow.withAlpha (0.07f)); g.strokePath (body, PST (1.0f));
    juce::Path hi; hi.addCentredArc (c.x, c.y, R - 1.5f, R - 1.5f, 0, -1.1f, 1.1f, true);
    g.setColour (highlight); g.strokePath (hi, PST (1.5f));
    // indicador: punto del logo
    const auto ip = c.getPointOnCircumference (R * 0.64f, ang);
    const float ir = std::max (3.0f, R * 0.15f);
    g.setColour (limeDot.withAlpha (0.5f)); g.fillEllipse (ip.x - ir - 3, ip.y - ir - 3, (ir + 3) * 2, (ir + 3) * 2);
    g.setColour (onLime); g.fillEllipse (ip.x - ir, ip.y - ir, ir * 2, ir * 2);
    g.setColour (limeDot); g.fillEllipse (ip.x - ir * 0.55f, ip.y - ir * 0.55f, ir * 1.1f, ir * 1.1f);
}

juce::Font LookAndFeel::getPopupMenuFont() { return font (F::man, 14.0f); }
void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (surface);
    g.setColour (shadow.withAlpha (0.12f)); g.drawRect (0, 0, w, h, 1);
}
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int) { return font (F::manX, 12.0f, 0.15f); }
void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool, bool)
{
    raised (g, b.getLocalBounds().toFloat().reduced (2.0f), (float) b.getHeight() * 0.4f, 0.6f);
}

// ------------------------------------------------------------------ Segmented
Segmented::Segmented (juce::StringArray n, bool d, float f) : names (std::move (n)), dot (d), fs (f)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}
void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rad = r.getHeight() * 0.5f;
    well (g, r, rad);
    const auto inner = r.reduced (4.0f);
    const float w = inner.getWidth() / (float) std::max (1, names.size());
    for (int i = 0; i < names.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (inner.getX() + w * (float) i, inner.getY(), w, inner.getHeight());
        const bool on = i == selected;
        if (on)
        {
            // pastilla elevada dentro del carril: sombra corta para que no toque el borde
            const auto pr = seg.reduced (1.0f, 0.0f);
            juce::Path pp; pp.addRoundedRectangle (pr, pr.getHeight() * 0.5f);
            juce::DropShadow (shadow.withAlpha (0.22f), 5, { 0, 2 }).drawForPath (g, pp);
            raised (g, pr, pr.getHeight() * 0.5f, 0.0f);
        }
        // si el texto no cabe en su hueco, se aprieta el espaciado y luego el tamaño (nunca se corta)
        float size = fs, track = 0.2f;
        auto f = font (F::manX, size, track);
        while (textWidth (f, names[i]) > seg.getWidth() - 8.0f && (track > 0.02f || size > 7.0f))
        {
            if (track > 0.02f) track = std::max (0.0f, track - 0.06f); else size -= 0.5f;
            f = font (F::manX, size, track);
        }
        const float tw = textWidth (f, names[i]) - size * track;
        const bool withDot = on && dot && tw + 22.0f < seg.getWidth();
        const float start = seg.getCentreX() - (tw + (withDot ? 15.0f : 0.0f)) * 0.5f;
        if (withDot) logoDot (g, { start + 4.0f, seg.getCentreY() }, 3.0f, true);
        g.setColour (on ? ink : juce::Colour (0xff8f998a)); g.setFont (f);
        g.drawText (names[i], juce::Rectangle<float> (start + (withDot ? 15.0f : 0.0f), seg.getY(), tw + 10.0f, seg.getHeight()), juce::Justification::centredLeft, false);
    }
}
void Segmented::mouseDown (const juce::MouseEvent& e)
{
    const auto inner = getLocalBounds().toFloat().reduced (4.0f);
    const int i = juce::jlimit (0, names.size() - 1, (int) ((e.position.x - inner.getX()) / (inner.getWidth() / (float) names.size())));
    if (i != selected) { selected = i; repaint(); if (onChange) onChange (i); }
}

// ------------------------------------------------------------------ Toggle
Toggle::Toggle (juce::String n) : text (std::move (n)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
void Toggle::paint (juce::Graphics& g)
{
    const auto t = juce::Rectangle<float> (0, (float) getHeight() * 0.5f - 11.0f, 38.0f, 22.0f);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9bd443), 0, t.getY(), juce::Colour (0xffbdeb66), 0, t.getBottom(), false));
        g.fillRoundedRectangle (t, 11.0f);
    }
    else well (g, t, 11.0f);
    const auto k = juce::Rectangle<float> (on ? t.getRight() - 20.0f : t.getX() + 2.0f, t.getY() + 2.0f, 18.0f, 18.0f);
    juce::Path kp; kp.addEllipse (k);
    juce::DropShadow (shadow.withAlpha (0.35f), 4, { 0, 2 }).drawForPath (g, kp);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0, k.getY(), juce::Colour (0xffeef2ea), 0, k.getBottom(), false));
    g.fillPath (kp);
    if (text.isNotEmpty())
    {
        g.setColour (ink); g.setFont (font (F::manX, 9.5f, 0.1f));
        g.drawText (text.toUpperCase(), getLocalBounds().toFloat().withTrimmedLeft (46.0f), juce::Justification::centredLeft, false);
    }
}

ParamToggle::ParamToggle (juce::String n, juce::AudioProcessorValueTreeState& s, const juce::String& id)
    : Toggle (std::move (n)), prm (s.getParameter (id))
{
    setOn (prm->getValue() > 0.5f);
    setTooltip (chainivo::pid::helpFor (id));
    onChange = [this] (bool v) { prm->beginChangeGesture(); prm->setValueNotifyingHost (v ? 1.0f : 0.0f); prm->endChangeGesture(); };
    startTimerHz (10);
}
void ParamToggle::timerCallback() { setOn (prm->getValue() > 0.5f); }

// ------------------------------------------------------------------ Knob
Knob::Knob (juce::String n, juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::UndoManager* um) : name (std::move (n))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.onDragStart = [um] { if (um) um->beginNewTransaction(); };
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setTooltip (chainivo::pid::helpFor (id));
    slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
}
void Knob::resized()
{
    slider.getProperties().set ("radius", radius);
    const float ring = radius + (radius >= 40 ? 30.0f : (radius < 20 ? 15.0f : 22.0f));
    const auto c = getLocalBounds().toFloat().getCentre();
    slider.setBounds (juce::Rectangle<float> (ring * 2, ring * 2).withCentre (c).toNearestInt());
}
void Knob::paint (juce::Graphics& g)
{
    const bool big = radius >= 40.0f, small = radius < 20.0f;
    const float ls = small ? 8.5f : 10.0f;
    label (g, name, getLocalBounds().toFloat().withHeight (14.0f), juce::Justification::centredTop, dim, ls);
    auto valueText = slider.getTextFromValue (slider.getValue());
    g.setColour (ink);
    g.setFont (font (F::soraBold, big ? 20.0f : (small ? 12.0f : 16.0f), -0.01f));
    g.drawText (valueText, getLocalBounds().toFloat().removeFromBottom (big ? 26.0f : (small ? 16.0f : 22.0f)), juce::Justification::centredBottom, false);
}

// ------------------------------------------------------------------ ParamChoice
ParamChoice::ParamChoice (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::UndoManager* u)
    : Segmented (dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))->choices, false, 9.5f), prm (s.getParameter (id)), um (u)
{
    onChange = [this] (int i)
    {
        if (um) um->beginNewTransaction();
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture();
    };
    timerCallback();
    setTooltip (chainivo::pid::helpFor (id));
    startTimerHz (10);
}
void ParamChoice::timerCallback() { setSelected ((int) std::lround (prm->convertFrom0to1 (prm->getValue()))); }

// ------------------------------------------------------------------ IconButton
void IconButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (icon == undo || icon == redo || icon == refresh) raised (g, r, r.getHeight() * 0.5f, 0.4f);
    const auto c = r.getCentre();
    g.setColour (ink);
    juce::PathStrokeType st (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    if (icon == undo || icon == redo || icon == refresh)
    {
        const float s = r.getHeight() * 0.22f, dir = icon == redo ? 1.0f : -1.0f;
        juce::Path a; a.addCentredArc (c.x, c.y + s * 0.2f, s, s, 0, icon == refresh ? 0.6f : -dir * 1.7f, icon == refresh ? 5.6f : dir * 1.2f, true);
        g.strokePath (a, st);
        const auto e = a.getCurrentPosition();
        juce::Path h; h.startNewSubPath (e.x - dir * 3.5f, e.y - 2.0f); h.lineTo (e.x, e.y); h.lineTo (e.x - dir * 3.5f, e.y + 2.5f);
        if (icon != refresh) g.strokePath (h, st);
    }
    else
    {
        juce::Path s; s.addStar (c, 5, r.getHeight() * 0.2f, r.getHeight() * 0.42f, 0.0f);
        if (icon == starOn) { g.setColour (juce::Colour (0xff8dcb34)); g.fillPath (s); g.setColour (ink); g.strokePath (s, juce::PathStrokeType (1.2f)); }
        else { g.setColour (faint); g.strokePath (s, juce::PathStrokeType (1.4f)); }
    }
}

// ------------------------------------------------------------------ PillButton
void PillButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced ((float) pad);
    const float rad = r.getHeight() * 0.5f;
    if (style == lime) limeFill (g, r, rad);
    else if (style == dark)
    {
        juce::Path p; p.addRoundedRectangle (r, rad);
        juce::DropShadow (juce::Colour (0xff142810).withAlpha (0.5f), 10, { 0, 4 }).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a3b22), 0, r.getY(), juce::Colour (0xff15240f), 0, r.getBottom(), false));
        g.fillPath (p);
    }
    else raised (g, r, rad, 0.4f);
    g.setColour (style == dark ? juce::Colour (0xffe8f7c8) : (style == lime ? onLime : ink));
    g.setFont (font (F::manX, 10.5f, 0.2f));
    g.drawText (text, r, juce::Justification::centred, false);
}

// ------------------------------------------------------------------ Meter
void Meter::setLevel (float lin)
{
    const float d = 20.0f * std::log10 (std::max (lin, 1.0e-6f));
    db = d > db ? d : std::max (d, db - 1.5f);
    if (d >= hold) { hold = d; holdFrames = 40; } else if (--holdFrames < 0) hold = std::max (-100.0f, hold - 1.0f);
    repaint();
}
void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    label (g, lbl, r.removeFromLeft (34.0f), juce::Justification::centredLeft, dim, 9.0f);
    const auto bar = r.withSizeKeepingCentre (r.getWidth(), 8.0f);
    well (g, bar, 4.0f);
    auto frac = [] (float d) { return juce::jlimit (0.0f, 1.0f, (d + 60.0f) / 60.0f); };
    const auto fill = bar.reduced (1.5f).withWidth ((bar.getWidth() - 3.0f) * frac (db));
    if (fill.getWidth() > 1.0f)
    {
        g.setGradientFill (juce::ColourGradient (green, fill.getX(), 0, db > -1.0f ? warn : lime, fill.getRight(), 0, false));
        g.fillRoundedRectangle (fill, 3.0f);
    }
    const float hx = bar.getX() + 1.5f + (bar.getWidth() - 3.0f) * frac (hold);
    g.setColour (hold > -1.0f ? red : ink.withAlpha (0.5f));
    g.fillRect (juce::Rectangle<float> (hx - 1.0f, bar.getY() + 1.0f, 2.0f, bar.getHeight() - 2.0f));
}
} // namespace chainivo::ui
