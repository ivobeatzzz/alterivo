#include "PluginEditor.h"
#include "BinaryData.h"

namespace theme = alterivo::theme;

// ======================================================= Preferencias de usuario
namespace
{
juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "alterIVO";
    o.folderName = "IVO Beatz";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}
} // namespace

bool UserSettings::loadDarkSkin()
{
    juce::PropertiesFile f (settingsOptions());
    return f.getBoolValue ("darkSkin", false);
}

void UserSettings::saveDarkSkin (bool dark)
{
    juce::PropertiesFile f (settingsOptions());
    f.setValue ("darkSkin", dark);
    f.saveIfNeeded();
}

// ============================================================== Fuentes
BrandFonts::BrandFonts()
{
    auto load = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
    soraLight   = load (BinaryData::SoraLight_ttf, BinaryData::SoraLight_ttfSize);
    soraSemi    = load (BinaryData::SoraSemiBold_ttf, BinaryData::SoraSemiBold_ttfSize);
    soraBold    = load (BinaryData::SoraBold_ttf, BinaryData::SoraBold_ttfSize);
    manropeBold = load (BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize);
    manropeX    = load (BinaryData::ManropeExtraBold_ttf, BinaryData::ManropeExtraBold_ttfSize);
}

namespace
{
// Tamaño en píxeles de diseño (como el font-size de la maqueta) y espaciado en em
juce::Font font (const BrandFonts& f, Face face, float size, float tracking = 0.0f)
{
    juce::Typeface::Ptr tf;
    switch (face)
    {
        case Face::soraLight:   tf = f.soraLight; break;
        case Face::soraSemi:    tf = f.soraSemi; break;
        case Face::soraBold:    tf = f.soraBold; break;
        case Face::manropeBold: tf = f.manropeBold; break;
        case Face::manropeX:    tf = f.manropeX; break;
    }
    auto opts = tf != nullptr ? juce::FontOptions (tf) : juce::FontOptions (size, juce::Font::bold);
    return juce::Font (opts.withPointHeight (size).withKerningFactor (tracking));
}

float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }

constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

constexpr int kW = AlterIvoEditor::kBaseW, kH = AlterIvoEditor::kBaseH;

// Posiciones del diseño (760 x 540)
// Fila inferior: INTENSITY y DRIVE bajo Pitch, MIX y OUTPUT bajo Formant
const juce::Point<int> kPitchC   { 124, 236 }, kFormantC { 636, 236 };
const juce::Point<int> kIntensityC { 78, 426 }, kDriveC { 170, 426 }, kMixC { 590, 426 }, kOutputC { 682, 426 };
constexpr int kBigBox = 180, kSmallBox = 96;
constexpr float kBigR = 58.0f, kSmallR = 24.0f;

const juce::Rectangle<int> kModes    { 215, 30, 330, 40 };
const juce::Rectangle<int> kCard     { 236, 100, 288, 244 };
const juce::Rectangle<int> kLink     { 350, 360, 60, 28 };
const juce::Rectangle<int> kInMeter  { 236, 404, 288, 20 };
const juce::Rectangle<int> kOutMeter { 236, 430, 288, 20 };
const juce::Rectangle<int> kPresets  { 270, 482, 220, 36 };
const juce::Rectangle<int> kKey      { kW - 36 - 166, 32, 166, 32 };
const juce::Rectangle<int> kSource   { kW - 36 - 166, 70, 166, 18 };
const juce::Rectangle<int> kSettings { 30, 496, 170, 30 };
const juce::Rectangle<int> kSyncPill { 252, 112, 70, 20 };     // dentro de la pantalla, arriba
const juce::Rectangle<int> kAbPill   { 452, 112, 56, 20 };

juce::Rectangle<int> box (juce::Point<int> c, int size) { return { c.x - size / 2, c.y - size / 2, size, size }; }
juce::Rectangle<int> centredRect (int cx, int y, int w, int h) { return { cx - w / 2, y, w, h }; }

// Firma IVO (solo letras): tinta oscura y punto lima, mismo trazado que logo/ivo-logo-simbolo.svg
// La tinta (#15210f) se cambia por la de la skin al cargarlo
const char* kLogoSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><line x1="17.050" y1="37.2" x2="17.050" y2="63.2" stroke="#15210f" stroke-width="7.2" stroke-linecap="round"/><polyline points="30.050,37.2 37.050,63.2 44.050,37.2" fill="none" stroke="#15210f" stroke-width="7.2" stroke-linecap="round" stroke-linejoin="round"/><path d="M61.381,60.720 A13.35,13.35 0 1 1 77.819,60.720" fill="none" stroke="#15210f" stroke-width="7.2" stroke-linecap="round"/><circle cx="73.714" cy="45.297" r="2.75" fill="#7cc23c"/></svg>)SVG";

// Sombra suave (equivale a box-shadow 0 dy blur)
void softShadow (juce::Graphics& g, const juce::Path& p, float alpha, int blur, int dy)
{
    juce::DropShadow (theme::shadow.withAlpha (juce::jmin (0.9f, alpha * theme::shadowStrength)), blur, { 0, dy }).drawForPath (g, p);
}

juce::Path roundRect (juce::Rectangle<float> r, float radius)
{
    juce::Path p;
    p.addRoundedRectangle (r, radius);
    return p;
}

juce::Path circle (juce::Point<float> c, float r)
{
    juce::Path p;
    p.addEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    return p;
}

// Icono de cadena: dos eslabones a 45°
void drawChain (juce::Graphics& g, juce::Point<float> c, float s, juce::Colour colour)
{
    juce::Path chain;
    chain.addRoundedRectangle (-s * 1.55f, -s * 0.55f, s * 1.9f, s * 1.1f, s * 0.55f);
    chain.addRoundedRectangle (-s * 0.35f, -s * 0.55f, s * 1.9f, s * 1.1f, s * 0.55f);
    chain.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::pi * 0.25f).translated (c.x, c.y));
    g.setColour (colour);
    g.strokePath (chain, juce::PathStrokeType (1.9f));
}

juce::String noteName (int note) { return juce::MidiMessage::getMidiNoteName (note, true, true, 3); }
} // namespace

// ============================================================ LookAndFeel
AlterIvoLookAndFeel::AlterIvoLookAndFeel() { applySkinColours(); }

void AlterIvoLookAndFeel::applySkinColours()
{
    setColour (juce::PopupMenu::backgroundColourId, theme::surface);
    setColour (juce::PopupMenu::textColourId, theme::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::chipBg);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::chipText);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
}

juce::Font AlterIvoLookAndFeel::getPopupMenuFont() { return font (*fonts, Face::manropeBold, 13.0f); }

void AlterIvoLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    // Tres puntos discretos en la esquina
    g.setColour (theme::textDim.withAlpha (isMouseOver || isMouseDragging ? 0.8f : 0.35f));
    const float r = 1.3f;
    for (auto p : { juce::Point<float> ((float) w - 4.0f, (float) h - 4.0f),
                    juce::Point<float> ((float) w - 9.0f, (float) h - 4.0f),
                    juce::Point<float> ((float) w - 4.0f, (float) h - 9.0f) })
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
}

void AlterIvoLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (theme::surface);
    g.setColour (theme::track);
    g.drawRect (0, 0, width, height, 1);
}

void AlterIvoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto c = area.getCentre();
    const bool big = width > 120;
    const float R = big ? kBigR : kSmallR;
    const float ringR = R + (big ? 16.0f : 11.0f);
    const float ringW = big ? 6.0f : 4.0f;
    const float tickR = ringR + (big ? 12.0f : 9.0f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    using PST = juce::PathStrokeType;

    // Marcas de escala: once puntos, el inicio, el centro y el final más marcados
    for (int k = 0; k <= 10; ++k)
    {
        const auto p = c.getPointOnCircumference (tickR, rotaryStartAngle + (float) k / 10.0f * (rotaryEndAngle - rotaryStartAngle));
        const bool major = k % 5 == 0;
        const float r = major ? (big ? 2.0f : 1.5f) : (big ? 1.2f : 1.0f);
        g.setColour (theme::ink.withAlpha (major ? 0.35f : 0.18f));
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
    }

    // Pista y arco de valor (bipolar desde el centro si el rango cruza el 0)
    juce::Path trackPath;
    trackPath.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::track);
    g.strokePath (trackPath, PST (ringW, PST::curved, PST::rounded));

    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path valuePath;
        valuePath.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setGradientFill (juce::ColourGradient (theme::arcStart, c.x - ringR, c.y - ringR, theme::limeBright, c.x + ringR, c.y + ringR, false));
        g.strokePath (valuePath, PST (ringW, PST::curved, PST::rounded));
    }

    // Cuerpo blanco (la sombra va en el fondo de la ventana)
    g.setColour (theme::surface);
    g.fillPath (circle (c, R));
    juce::ColourGradient body (theme::surface, c.x, c.y - R * 0.3f, theme::surfaceLow, c.x, c.y - R * 0.3f + R * 1.4f, true);
    g.setGradientFill (body);
    g.fillPath (circle (c, R - 1.0f));

    // Indicador: el punto del logo (círculo de tinta con centro lima)
    const auto p = c.getPointOnCircumference (R * 0.62f, angle);
    g.setColour (theme::ink);
    g.fillPath (circle (p, big ? 9.0f : 5.5f));
    g.setColour (theme::limeDot);
    g.fillPath (circle (p, big ? 4.5f : 2.8f));
}

// =========================================================== ModeSelector
ModeSelector::ModeSelector (juce::StringArray n, float size, bool dotBeside)
    : names (std::move (n)), fontSize (size), inlineDot (dotBeside)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModeSelector::setSelected (int index)
{
    if (index != selected) { selected = index; repaint(); }
}

// Cada palabra con su ancho real y el mismo espacio entre ellas
std::vector<juce::Rectangle<float>> ModeSelector::segments() const
{
    const auto f = font (*fonts, Face::manropeX, fontSize, 0.22f);
    const float pad = fontSize * 1.7f;
    std::vector<float> w;
    float total = 0.0f;
    for (auto& n : names) { w.push_back (inlineDot ? 14.0f + textWidth (f, n) + 16.0f : textWidth (f, n) + pad * 2.0f); total += w.back(); }
    if (inlineDot) total -= 16.0f;
    std::vector<juce::Rectangle<float>> r;
    float x = ((float) getWidth() - total) * 0.5f;
    for (auto wi : w) { r.push_back ({ x, 0.0f, wi, (float) getHeight() }); x += wi; }
    return r;
}

int ModeSelector::indexAt (float x) const
{
    const auto r = segments();
    for (int i = 0; i < (int) r.size(); ++i)
        if (x >= r[(size_t) i].getX() && x < r[(size_t) i].getRight()) return i;
    return -1;
}

void ModeSelector::paint (juce::Graphics& g)
{
    // La palabra activa en tinta con el punto lima del logo debajo
    const auto segs = segments();
    g.setFont (font (*fonts, Face::manropeX, fontSize, 0.22f));
    const float textH = fontSize * 1.6f, dotY = 6.0f + textH + 8.0f;
    if (inlineDot)
    {
        for (int i = 0; i < (int) segs.size(); ++i)
        {
            const auto seg = segs[(size_t) i];
            const float cy = seg.getCentreY();
            g.setColour (i == selected ? theme::ink : (i == hover ? theme::textDim : theme::arrows));
            g.drawText (names[i], seg.withTrimmedLeft (14.0f), juce::Justification::centredLeft, false);
            if (i == selected)
            {
                g.setColour (theme::limeBright.withAlpha (0.3f));
                g.fillEllipse (seg.getX() + 3.5f - 5.0f, cy - 5.0f, 10.0f, 10.0f);
                g.setColour (theme::limeLine);
                g.fillEllipse (seg.getX() + 3.5f - 2.5f, cy - 2.5f, 5.0f, 5.0f);
            }
            else
            {
                g.setColour (theme::track);
                g.fillEllipse (seg.getX() + 3.5f - 2.5f, cy - 2.5f, 5.0f, 5.0f);
            }
        }
        return;
    }
    for (int i = 0; i < (int) segs.size(); ++i)
    {
        const auto seg = segs[(size_t) i].withY (6.0f).withHeight (textH);
        g.setColour (i == selected ? theme::ink : (i == hover ? theme::textDim : theme::arrows));
        g.drawText (names[i], seg.translated (0.11f * fontSize, 0.0f), juce::Justification::centred, false);
        if (i == selected)
        {
            const float cx = seg.getCentreX();
            g.setColour (theme::limeBright.withAlpha (0.25f));
            g.fillEllipse (cx - 6.0f, dotY - 6.0f, 12.0f, 12.0f);
            g.setColour (theme::limeLine);
            g.fillEllipse (cx - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
        }
    }
}

void ModeSelector::mouseDown (const juce::MouseEvent& e)
{
    const int index = indexAt ((float) e.x);
    if (index >= 0 && onChange) onChange (index);
}

void ModeSelector::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt ((float) e.x);
    if (h != hover) { hover = h; repaint(); }
}

void ModeSelector::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

// ============================================================= LinkButton
LinkButton::LinkButton() : juce::Button ("Link")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Link: Pitch y Formant se mueven juntos");
}

void LinkButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    auto fill = on ? theme::lime : theme::surface;
    if (down) fill = fill.darker (0.06f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    drawChain (g, r.getCentre(), 6.0f, on ? theme::onLime : (highlighted ? theme::ink : theme::textDim));
}

// =========================================================== VoiceDisplay
static float hzToNote (float hz) { return hz > 0.0f ? 69.0f + 12.0f * std::log2 (hz / 440.0f) : 0.0f; }

VoiceDisplay::VoiceDisplay()
{
    setInterceptsMouseClicks (false, false);
    env.fill (0.0f);
}

void VoiceDisplay::push (float inHz, float outHz, const float* envDb, float envLevel, float formantSemis)
{
    head = (head + 1) % kLen;
    inNotes[(size_t) head] = hzToNote (inHz);
    outNotes[(size_t) head] = hzToNote (outHz);

    const float target = outNotes[(size_t) head] > 0.0f ? outNotes[(size_t) head] : inNotes[(size_t) head];
    if (target > 0.0f)
        centre += 0.08f * (target - centre);

    // Envolvente: se suaviza y se apaga sola cuando no hay voz
    const bool voiced = envLevel > -55.0f;
    if (voiced)
    {
        float top = -200.0f;
        for (int i = 0; i < kEnv; ++i) top = juce::jmax (top, envDb[i]);
        for (int i = 0; i < kEnv; ++i)
        {
            const float v = juce::jlimit (0.0f, 1.0f, (envDb[i] - (top - 32.0f)) / 32.0f);
            env[(size_t) i] += 0.3f * (v - env[(size_t) i]);
        }
    }
    envShow += ((voiced ? 1.0f : 0.0f) - envShow) * (voiced ? 0.25f : 0.08f);
    formantShift = formantSemis;
    repaint();
}

void VoiceDisplay::paint (juce::Graphics& g)
{
    // La tarjeta blanca y su sombra están en el fondo de la ventana
    const float W = (float) getWidth();
    const float x0 = 20.0f, x1 = W - 20.0f;
    using PST = juce::PathStrokeType;

    // ---- Nota actual y cambio aplicado
    const float inNow = inNotes[(size_t) head], outNow = outNotes[(size_t) head];
    const bool voice = inNow > 0.0f;
    const int outNote = robotNote >= 0 ? robotNote : (outNow > 0.0f ? (int) std::lround (outNow) : -1);

    const auto noteArea = juce::Rectangle<float> (0.0f, 14.0f, W, 56.0f);
    g.setFont (font (*fonts, Face::soraBold, 44.0f, -0.03f));
    if (voice && outNote >= 0)
    {
        g.setGradientFill (juce::ColourGradient (theme::ink, 0.0f, noteArea.getY() + 18.0f, theme::green, 0.0f, noteArea.getBottom(), false));
        g.drawText (noteName (outNote), noteArea, juce::Justification::centred, false);

        const int inNote = (int) std::lround (inNow);
        const int diff = outNote - inNote;
        const auto f = font (*fonts, Face::manropeX, 9.5f, 0.12f);
        const juce::String a = noteName (inNote), b = noteName (outNote);
        const juce::String d = (diff > 0 ? "+" : "") + juce::String (diff) + " ST";
        const float gap = 6.0f, arrowW = 9.0f, dotW = 3.0f;
        const float wa = textWidth (f, a), wb = textWidth (f, b), wd = textWidth (f, d);
        const float chipW = 18.0f + wa + gap + arrowW + gap + wb + gap + dotW + gap + wd;
        const auto chip = juce::Rectangle<float> ((W - chipW) * 0.5f, 74.0f, chipW, 20.0f);
        g.setColour (theme::chipBg);
        g.fillRoundedRectangle (chip, 10.0f);

        float x = chip.getX() + 9.0f;
        const float cy = chip.getCentreY();
        g.setFont (f);
        auto put = [&] (const juce::String& s, float w)
        {
            g.setColour (theme::chipText);
            g.drawText (s, juce::Rectangle<float> (x, chip.getY(), w + 4.0f, chip.getHeight()), juce::Justification::centredLeft, false);
            x += w + gap;
        };
        put (a, wa);
        juce::Path arrow;
        arrow.startNewSubPath (x, cy); arrow.lineTo (x + arrowW, cy);
        arrow.startNewSubPath (x + arrowW - 3.0f, cy - 3.0f); arrow.lineTo (x + arrowW, cy); arrow.lineTo (x + arrowW - 3.0f, cy + 3.0f);
        g.setColour (theme::chipDim);
        g.strokePath (arrow, PST (1.3f, PST::curved, PST::rounded));
        x += arrowW + gap;
        put (b, wb);
        g.setColour (theme::chipDim);
        g.fillEllipse (x, cy - 1.5f, dotW, dotW);
        x += dotW + gap;
        put (d, wd);
    }
    else
    {
        g.setColour (theme::track);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")), noteArea, juce::Justification::centred, false);
    }

    // ---- Rótulos
    g.setFont (font (*fonts, Face::manropeX, 8.5f, 0.2f));
    g.setColour (theme::textFaint);
    g.drawText ("PITCH", juce::Rectangle<float> (x0, 104.0f, 100.0f, 12.0f), juce::Justification::centredLeft, false);
    g.drawText ("FORMANT", juce::Rectangle<float> (x0, 188.0f, 100.0f, 12.0f), juce::Justification::centredLeft, false);

    // ---- Tono: voz original (gris) y salida (verde)
    const auto plot = juce::Rectangle<float> (x0, 118.0f, x1 - x0, 62.0f);
    const float pxPerSemi = 4.2f;
    auto yFor = [&] (float note) { return plot.getCentreY() - (note - centre) * pxPerSemi; };
    auto buildPath = [&] (const std::array<float, kLen>& notes)
    {
        juce::Path p;
        bool drawing = false;
        float prev = 0.0f;
        for (int i = 0; i < kLen; ++i)
        {
            const float v = notes[(size_t) ((head + 1 + i) % kLen)];
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (kLen - 1);
            const float y = yFor (v);
            // Sin voz, fuera de la vista o salto brusco: se corta el trazo
            const bool out = v <= 0.0f || y < plot.getY() || y > plot.getBottom();
            if (out || (drawing && std::abs (v - prev) > 2.5f))
            {
                drawing = false;
                if (out) continue;
            }
            if (! drawing) { p.startNewSubPath (x, y); drawing = true; }
            else p.lineTo (x, y);
            prev = v;
        }
        return p;
    };

    g.saveState();
    g.reduceClipRegion (plot.expanded (2.0f).toNearestInt());
    g.setColour (theme::traceIn);
    g.strokePath (buildPath (inNotes), PST (1.2f, PST::curved, PST::rounded));
    g.setGradientFill (juce::ColourGradient (theme::arcStart, plot.getX(), 0.0f, theme::limeBright, plot.getRight(), 0.0f, false));
    g.strokePath (buildPath (outNotes), PST (2.0f, PST::curved, PST::rounded));
    g.restoreState();

    g.setColour (theme::ink.withAlpha (0.07f));
    g.drawHorizontalLine (183, x0, x1);

    // ---- Formantes: original (gris) y desplazada por Formant (verde)
    const float base = 226.0f, amp = 30.0f * envShow;
    const float pointsPerOctave = (float) (kEnv - 1) / std::log2 (alterivo::PitchEngine::kEnvHiHz / alterivo::PitchEngine::kEnvLoHz);
    const float shiftPts = formantShift / 12.0f * pointsPerOctave;
    auto sample = [&] (float idx)
    {
        if (idx <= 0.0f) return env[0];
        if (idx >= (float) (kEnv - 1)) return env[(size_t) kEnv - 1];
        const int i = (int) idx;
        const float t = idx - (float) i;
        return env[(size_t) i] + t * (env[(size_t) i + 1] - env[(size_t) i]);
    };
    auto curve = [&] (float shift, bool closed)
    {
        juce::Path p;
        constexpr int steps = 96;
        for (int s = 0; s <= steps; ++s)
        {
            const float t = (float) s / (float) steps;
            const float x = x0 + (x1 - x0) * t;
            const float y = base - amp * (0.08f + 0.92f * sample (t * (float) (kEnv - 1) - shift));
            if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (x1, base); p.lineTo (x0, base); p.closeSubPath(); }
        return p;
    };

    if (envShow > 0.01f)
    {
        g.setColour (theme::ink.withAlpha (0.04f));
        g.fillPath (curve (0.0f, true));
        g.setColour (theme::traceIn);
        g.strokePath (curve (0.0f, false), PST (1.2f, PST::curved, PST::rounded));

        g.setGradientFill (juce::ColourGradient (theme::limeBright.withAlpha (0.55f), 0.0f, base - 32.0f,
                                                 theme::limeBright.withAlpha (0.0f), 0.0f, base, false));
        g.fillPath (curve (shiftPts, true));
        g.setColour (theme::limeDeep);
        g.strokePath (curve (shiftPts, false), PST (1.6f, PST::curved, PST::rounded));
    }
    g.setColour (theme::track);
    g.drawHorizontalLine ((int) base, x0, x1);
}

// ============================================================= LevelMeter
void LevelMeter::setPeak (float linear)
{
    const float db = linear > 1.0e-6f ? 20.0f * std::log10 (linear) : -100.0f;
    levelDb = db > levelDb ? db : juce::jmax (db, levelDb - 1.2f);   // caída suave
    if (db >= holdDb) { holdDb = db; holdFrames = 45; }
    else if (--holdFrames <= 0) holdDb = juce::jmax (levelDb, holdDb - 0.8f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (theme::textDim);
    g.setFont (font (*fonts, Face::manropeX, 9.5f, 0.24f));
    g.drawText (label, r.withWidth (44.0f), juce::Justification::centredLeft, false);

    constexpr int N = 28;
    constexpr float lo = -48.0f;
    const float x0 = 38.0f, w = (r.getWidth() - x0 + 2.5f) / (float) N;
    const float cy = r.getCentreY();

    int lit = 0;
    for (int i = 0; i < N; ++i)
        if (levelDb >= lo - lo * (float) i / (float) N) lit = i + 1;
    const int holdSeg = holdDb > lo ? juce::jlimit (0, N - 1, (int) ((holdDb - lo) / -lo * (float) N)) : -1;

    for (int i = 0; i < N; ++i)
    {
        const float thr = lo - lo * (float) i / (float) N;
        const auto seg = juce::Rectangle<float> (x0 + (float) i * w, cy - 4.0f, w - 2.5f, 8.0f);
        const auto hot = thr > -3.0f ? theme::clip : thr > -9.0f ? theme::warn : theme::meterGreen;
        if (i < lit)
            g.setColour (hot == theme::meterGreen ? hot.withAlpha (0.55f + 0.45f * (float) (i + 1) / (float) lit) : hot);
        else if (i == holdSeg)
            g.setColour (hot);
        else
            g.setColour (theme::track);
        g.fillRoundedRectangle (seg, 2.0f);
    }
}

// ================================================================= KeyBox
KeyBox::KeyBox()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void KeyBox::setText (const juce::String& t, bool isActive)
{
    if (t != text || isActive != active) { text = t; active = isActive; repaint(); }
}

void KeyBox::setProgress (float p)
{
    if (std::abs (p - progress) > 0.001f) { progress = p; repaint(); }
}

void KeyBox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (theme::surface);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (progress >= 0.0f)
    {
        // Progreso del análisis: línea lima en la base de la píldora
        const auto bar = r.reduced (16.0f, 0.0f).withTop (r.getBottom() - 5.0f).withHeight (2.0f);
        g.setColour (theme::track);
        g.fillRoundedRectangle (bar, 1.0f);
        g.setColour (theme::limeLine);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 1.0f);
    }

    // Mientras analiza, el texto usa todo el ancho (sin el rótulo KEY)
    const float textX = progress >= 0.0f ? 16.0f : 46.0f;
    if (progress < 0.0f)
    {
        g.setFont (font (*fonts, Face::manropeX, 8.5f, 0.2f));
        g.setColour (theme::textFaint);
        g.drawText ("KEY", juce::Rectangle<float> (16.0f, 0.0f, 40.0f, r.getHeight()), juce::Justification::centredLeft, false);
    }

    g.setFont (font (*fonts, Face::manropeX, 10.0f, 0.12f));
    g.setColour (active ? theme::ink : theme::textDim);
    g.drawText (text, juce::Rectangle<float> (textX, 0.0f, r.getWidth() - textX - 26.0f, r.getHeight()), juce::Justification::centredLeft, false);

    // Flecha del menú
    juce::Path p;
    const float cx = r.getRight() - 17.0f, cy = r.getCentreY();
    p.startNewSubPath (cx - 3.5f, cy - 1.8f);
    p.lineTo (cx, cy + 1.8f);
    p.lineTo (cx + 3.5f, cy - 1.8f);
    g.setColour (theme::arrows);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ================================================================ CardPill
CardPill::CardPill() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

void CardPill::setState (const juce::String& t, bool isActive, int selectedSegment)
{
    if (t != text || isActive != active || selectedSegment != segment)
    {
        text = t; active = isActive; segment = selectedSegment;
        repaint();
    }
}

void CardPill::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    if (active)
    {
        g.setColour (theme::chipBg);
        g.fillRoundedRectangle (r, rad);
    }
    else
    {
        g.setColour (isMouseOver() ? theme::textDim : theme::track);
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
    g.setFont (font (*fonts, Face::manropeX, 8.5f, 0.16f));

    if (segment >= 0)
    {
        // A | B: la letra elegida en un círculo
        const float w = r.getWidth() / 2.0f;
        for (int i = 0; i < 2; ++i)
        {
            const auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
            if (i == segment)
            {
                const float d = r.getHeight() - 6.0f;
                g.setColour (i == 0 ? theme::warn : theme::limeLine);
                g.fillEllipse (seg.withSizeKeepingCentre (d, d));
            }
            g.setColour (i == segment ? theme::onLime : theme::textDim);
            g.drawText (i == 0 ? "A" : "B", seg.translated (0.7f, 0.0f), juce::Justification::centred, false);
        }
        return;
    }
    g.setColour (active ? theme::chipText : (isMouseOver() ? theme::ink : theme::textDim));
    g.drawText (text, r.translated (0.7f, 0.0f), juce::Justification::centred, false);
}

// ========================================================== SettingsButton
SettingsButton::SettingsButton()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SettingsButton::setText (const juce::String& skin, const juce::String& latency)
{
    if (skin != skinText || latency != latencyText) { skinText = skin; latencyText = latency; repaint(); }
}

void SettingsButton::paint (juce::Graphics& g)
{
    // Engranaje + skin y latencia actuales
    const float cy = (float) getHeight() * 0.5f, cx = 13.0f;
    const auto col = isMouseOver() ? theme::ink : theme::textDim;
    juce::Path gear;
    for (int i = 0; i < 8; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
        gear.addRectangle (-1.4f, -7.0f, 2.8f, 3.0f);
        gear.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::twoPi / 8.0f));
        juce::ignoreUnused (a);
    }
    gear.addEllipse (-5.0f, -5.0f, 10.0f, 10.0f);
    gear.applyTransform (juce::AffineTransform::translation (cx, cy));
    g.setColour (col);
    g.fillPath (gear);
    g.setColour (theme::bg);
    g.fillEllipse (cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);

    g.setFont (font (*fonts, Face::manropeX, 9.0f, 0.22f));
    g.setColour (col);
    g.drawText (skinText + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + latencyText,
                juce::Rectangle<float> (28.0f, 0.0f, (float) getWidth() - 28.0f, (float) getHeight()), juce::Justification::centredLeft, false);
}

void SettingsButton::mouseDown (const juce::MouseEvent&) { if (onClick) onClick(); }

// ============================================================== PresetBox
PresetBox::PresetBox() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

void PresetBox::setText (const juce::String& t)
{
    if (t != text) { text = t; repaint(); }
}

void PresetBox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (theme::surface);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    const float cy = r.getCentreY();
    g.setColour (theme::limeBright.withAlpha (0.3f));
    g.fillEllipse (20.0f - 6.5f, cy - 6.5f, 13.0f, 13.0f);
    g.setColour (theme::presetDot);
    g.fillEllipse (20.0f - 3.5f, cy - 3.5f, 7.0f, 7.0f);

    g.setColour (theme::ink);
    g.setFont (font (*fonts, Face::manropeBold, 12.5f));
    g.drawText (text, juce::Rectangle<float> (34.0f, 0.0f, r.getWidth() - 34.0f - 50.0f, r.getHeight()),
                juce::Justification::centredLeft, true);

    // Flechas anterior / siguiente
    auto chevron = [&] (float cx, bool left)
    {
        juce::Path p;
        const float d = left ? 1.0f : -1.0f;
        p.startNewSubPath (cx + 1.8f * d, cy - 3.6f);
        p.lineTo (cx - 1.8f * d, cy);
        p.lineTo (cx + 1.8f * d, cy + 3.6f);
        g.setColour (theme::arrows);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    chevron (r.getWidth() - 38.0f, true);
    chevron (r.getWidth() - 22.0f, false);
}

void PresetBox::mouseDown (const juce::MouseEvent& e)
{
    const int w = getWidth();
    if (e.x >= w - 48 && e.x < w - 30) { if (onPrev) onPrev(); }
    else if (e.x >= w - 30)            { if (onNext) onNext(); }
    else if (onClick) onClick();
}

// =================================================================== View
AlterIvoView::AlterIvoView (AlterIvoProcessor& p) : proc (p)
{
    // Skin guardada como preferencia (la misma para todos los proyectos)
    static bool skinLoaded = false;
    if (! skinLoaded) { theme::applySkin (UserSettings::loadDarkSkin()); skinLoaded = true; }
    seenSkin = theme::skinVersion.load();
    lnf.applySkinColours();

    setLookAndFeel (&lnf);
    setOpaque (true);

    setupKnob (pitch, true);
    setupKnob (formant, true);
    setupKnob (intensity, false);
    setupKnob (drive, false);
    setupKnob (mix, false);
    setupKnob (output, false);

    pitchAtt   = std::make_unique<SliderAttachment> (proc.apvts, "pitch", pitch);
    formantAtt = std::make_unique<SliderAttachment> (proc.apvts, "formant", formant);
    driveAtt   = std::make_unique<SliderAttachment> (proc.apvts, "drive", drive);
    mixAtt     = std::make_unique<SliderAttachment> (proc.apvts, "mix", mix);
    intensityAtt = std::make_unique<SliderAttachment> (proc.apvts, "intensity", intensity);
    outputAtt    = std::make_unique<SliderAttachment> (proc.apvts, "output", output);
    intensity.setDoubleClickReturnValue (true, 100.0);
    output.setDoubleClickReturnValue (true, 0.0);

    pitch.setDoubleClickReturnValue (true, 0.0);
    formant.setDoubleClickReturnValue (true, 0.0);
    drive.setDoubleClickReturnValue (true, 0.0);
    mix.setDoubleClickReturnValue (true, 100.0);

    auto repaintValue = [this] (juce::Point<int> c, bool big)
    {
        repaint (big ? centredRect (c.x, c.y + 78, 180, 34) : centredRect (c.x, c.y + 44, 120, 24));
    };
    // Con Link, el valor de Formant muestra el total (su ajuste + lo que mueve Pitch)
    pitch.onValueChange     = [this, repaintValue] { repaintValue (kPitchC, true); repaintValue (kFormantC, true); };
    formant.onValueChange   = [repaintValue] { repaintValue (kFormantC, true); };
    drive.onValueChange     = [repaintValue] { repaintValue (kDriveC, false); };
    mix.onValueChange       = [repaintValue] { repaintValue (kMixC, false); };
    intensity.onValueChange = [repaintValue] { repaintValue (kIntensityC, false); };
    output.onValueChange    = [repaintValue] { repaintValue (kOutputC, false); };

    modeSelector.onChange = [this] (int i) { setMode (i); };
    addAndMakeVisible (modeSelector);

    sourceSelector.onChange = [this] (int i) { setChoice ("source", i); timerCallback(); };
    addAndMakeVisible (sourceSelector);

    keyBox.onClick = [this] { showKeyMenu(); };
    addAndMakeVisible (keyBox);

    linkButton.onStateChange = [this] { repaint (260, 364, 240, 20); repaint (centredRect (kFormantC.x, kFormantC.y + 78, 180, 34)); };
    addAndMakeVisible (linkButton);
    linkAtt = std::make_unique<ButtonAttachment> (proc.apvts, "link", linkButton);

    addAndMakeVisible (display);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    presetBox.onClick = [this] { showPresetMenu(); };
    presetBox.onPrev  = [this] { loadPreset ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); };
    presetBox.onNext  = [this] { loadPreset ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    addAndMakeVisible (presetBox);

    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    syncPill.onClick = [this] { showPatternMenu(); };
    syncPill.setTooltip ("Patrones de tono sincronizados al tempo del DAW");
    addAndMakeVisible (syncPill);

    abPill.onClick = [this]
    {
        if (auto* q = proc.apvts.getParameter ("compare"))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f);
            q->endChangeGesture();
        }
        timerCallback();
    };
    abPill.setTooltip ("A/B: A = voz original, B = alterIVO (al mismo volumen)");
    addAndMakeVisible (abPill);

    loadLogo();

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

AlterIvoView::~AlterIvoView()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AlterIvoView::setupKnob (juce::Slider& s, bool big)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (kStart, kEnd, true);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setMouseDragSensitivity (big ? 280 : 200);
    s.setVelocityBasedMode (false);
    s.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    s.setPaintingIsUnclipped (true);
    addAndMakeVisible (s);
}

void AlterIvoView::setMode (int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mode")))
    {
        param->beginChangeGesture();
        *param = index;
        param->endChangeGesture();
    }
    timerCallback();
}

void AlterIvoView::setChoice (const char* id, int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        param->beginChangeGesture();
        *param = index;
        param->endChangeGesture();
    }
}

static const char* kRoots[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

void AlterIvoView::updateKeyText()
{
    if (keyMessageFrames > 0)
        return;
    const int root = (int) std::lround (proc.apvts.getRawParameterValue ("key")->load());
    const int scale = (int) std::lround (proc.apvts.getRawParameterValue ("scale")->load());
    keyBox.setText (scale == 0 ? juce::String ("CHROMATIC") : juce::String (kRoots[root]) + (scale == 1 ? " MAJOR" : " MINOR"), scale != 0);
}

void AlterIvoView::showKeyMenu()
{
    const int root = (int) std::lround (proc.apvts.getRawParameterValue ("key")->load());
    const int scale = (int) std::lround (proc.apvts.getRawParameterValue ("scale")->load());

    juce::PopupMenu major, minor, menu;
    for (int r = 0; r < 12; ++r)
    {
        major.addItem (100 + r, juce::String (kRoots[r]) + " major", true, scale == 1 && root == r);
        minor.addItem (200 + r, juce::String (kRoots[r]) + " minor", true, scale == 2 && root == r);
    }
    menu.setLookAndFeel (&lnf);
    const bool analyzing = proc.updateKeyAnalysis().state == AlterIvoProcessor::KeyStatus::Analyzing;
    if (analyzing) menu.addItem (3, juce::String (juce::CharPointer_UTF8 ("Cancelar el análisis")));
    else menu.addItem (1, juce::String (juce::CharPointer_UTF8 ("Detectar tonalidad (reproduce el clip)")));
    menu.addSeparator();
    menu.addItem (2, "Chromatic (todas las notas)", true, scale == 0);
    menu.addSubMenu ("Major", major, true, nullptr, scale == 1);
    menu.addSubMenu ("Minor", minor, true, nullptr, scale == 2);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyBox),
                        [safe = juce::Component::SafePointer<AlterIvoView> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            auto& v = *safe;
                            if (result == 1) v.proc.startKeyAnalysis();
                            else if (result == 3) v.proc.cancelKeyAnalysis();
                            if (result == 2 || result >= 100) v.proc.cancelKeyAnalysis();
                            if (result == 2) v.setChoice ("scale", 0);
                            else if (result >= 200) { v.setChoice ("key", result - 200); v.setChoice ("scale", 2); }
                            else if (result >= 100) { v.setChoice ("key", result - 100); v.setChoice ("scale", 1); }
                            v.shownKey = -1;
                            v.timerCallback();
                        });
}

void AlterIvoView::loadLogo()
{
    juce::String svg (kLogoSvg);
    svg = svg.replace ("#15210f", "#" + theme::ink.toDisplayString (false).toLowerCase());
    if (auto xml = juce::XmlDocument::parse (svg))
        logoMark = juce::Drawable::createFromSVG (*xml);
}

void AlterIvoView::showPatternMenu()
{
    const int current = (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load());
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSectionHeader ("TEMPO SYNC");
    static const char* names[] = { "Off", "Octava arriba \xc2\xb7 1/4", "Octavas arriba y abajo \xc2\xb7 1/8",
                                   "Quinta \xc2\xb7 1/8", "Octava abajo \xc2\xb7 1/4", "Escalera menor \xc2\xb7 1/16" };
    for (int i = 0; i < 6; ++i)
        menu.addItem (i + 1, juce::String (juce::CharPointer_UTF8 (names[i])), true, i == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&syncPill),
                        [safe = juce::Component::SafePointer<AlterIvoView> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            safe->setChoice ("pattern", result - 1);
                            safe->timerCallback();
                        });
}

void AlterIvoView::showSettingsMenu()
{
    const int profile = proc.getLatencyProfile();
    juce::PopupMenu skins, latency, menu;
    skins.addItem (1, "Light", true, ! theme::darkSkin);
    skins.addItem (2, "Dark", true, theme::darkSkin);
    latency.addItem (10, "Low latency (41 ms)", true, profile == 0);
    latency.addItem (11, "Normal (78 ms, mejor afinación)", true, profile == 1);
    menu.setLookAndFeel (&lnf);
    menu.addSubMenu ("Skin", skins);
    menu.addSubMenu ("Latencia", latency);
    menu.addSeparator();
    auto pct = [] (float v) { return juce::String (v * 100.0f, 1) + " %"; };
    menu.addSectionHeader ("CPU (un núcleo)");
    menu.addItem (90, "Motor de pitch: " + pct (proc.cpuEngine.load()), false);
    menu.addItem (91, "Drive: " + pct (proc.cpuDrive.load()), false);
    menu.addItem (92, "Total alterIVO: " + pct (proc.cpuTotal.load()), false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [safe = juce::Component::SafePointer<AlterIvoView> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            if (result == 1 || result == 2)
                            {
                                theme::applySkin (result == 2);
                                UserSettings::saveDarkSkin (result == 2);
                            }
                            else if (result >= 10) safe->proc.setLatencyProfile (result - 10);
                            safe->timerCallback();
                        });
}

void AlterIvoView::loadPreset (int index)
{
    proc.setCurrentProgram (index);
    timerCallback();
}

void AlterIvoView::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        menu.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox),
                        [safe = juce::Component::SafePointer<AlterIvoView> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->loadPreset (result - 1);
                        });
}

void AlterIvoView::timerCallback()
{
    const int mode = (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load());
    if (mode != currentMode)
    {
        currentMode = mode;
        modeSelector.setSelected (mode);
    }

    const int src = (int) std::lround (proc.apvts.getRawParameterValue ("source")->load());
    if (src != shownSource) { shownSource = src; sourceSelector.setSelected (src); }

    const int keyState = (int) std::lround (proc.apvts.getRawParameterValue ("key")->load()) * 4
                       + (int) std::lround (proc.apvts.getRawParameterValue ("scale")->load());
    // Detector de tonalidad: progreso mientras analiza y resultado con su seguridad
    const auto& ks = proc.updateKeyAnalysis();
    if (ks.state == AlterIvoProcessor::KeyStatus::Analyzing)
    {
        keyBox.setText ("ANALYZING " + juce::String (juce::roundToInt (ks.progress * 100.0f)) + "%", true);
        keyBox.setProgress (ks.progress);
        wasAnalyzing = true;
        shownKey = -1;
    }
    else
    {
        if (wasAnalyzing)
        {
            wasAnalyzing = false;
            keyBox.setProgress (-1.0f);
            if (ks.state == AlterIvoProcessor::KeyStatus::Done)
            {
                keyBox.setText (juce::String (kRoots[ks.root]) + (ks.scale == 1 ? " MAJ " : " MIN ")
                                + juce::String (juce::roundToInt (ks.confidence * 100.0f)) + "%", true);
                keyMessageFrames = 90;
            }
            else shownKey = -1;
        }
        if (keyMessageFrames > 0 && --keyMessageFrames == 0) shownKey = -1;
        if (keyState != shownKey && keyMessageFrames == 0) { shownKey = keyState; updateKeyText(); }
    }

    // SYNC (patrón de tempo) y A/B
    {
        static const char* shortNames[] = { "SYNC", "OCT 1/4", "OCT 1/8", "5TH 1/8", "SUB 1/4", "STAIRS" };
        const int pat = juce::jlimit (0, 5, (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
        syncPill.setState (shortNames[pat], pat > 0);
        abPill.setState ("A|B", false, proc.apvts.getRawParameterValue ("compare")->load() > 0.5f ? 0 : 1);
    }

    // Skin cambiada (en esta ventana o en otra instancia)
    if (seenSkin != theme::skinVersion.load())
    {
        seenSkin = theme::skinVersion.load();
        lnf.applySkinColours();
        loadLogo();
        background = {};
        repaint();
    }
    settingsButton.setText (theme::darkSkin ? "DARK" : "LIGHT",
                            juce::String (juce::roundToInt (1000.0 * proc.getLatencySamples() / juce::jmax (1.0, proc.getSampleRate()))) + " MS");

    if (proc.getCurrentProgram() != shownPreset)
    {
        shownPreset = proc.getCurrentProgram();
        presetBox.setText (proc.getProgramName (shownPreset));
    }

    int robot = -1;
    if (mode == 2)
    {
        const int midi = proc.getHeldMidiNote();
        robot = midi >= 0 ? midi : 72 + (int) std::lround (proc.apvts.getRawParameterValue ("pitch")->load());
    }
    display.setRobotNote (robot);
    const float level = proc.getFormantEnvelope (envBuf.data());
    display.push (proc.getDetectedHz(), proc.getOutputHz(), envBuf.data(), level,
                  proc.apvts.getRawParameterValue ("formant")->load());

    inMeter.setPeak (proc.takeInputPeak());
    outMeter.setPeak (proc.takeOutputPeak());
}

void AlterIvoView::paint (juce::Graphics& g)
{
    // ---- Fondo fijo (se dibuja una vez y se guarda en imagen a la resolución de la pantalla)
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! background.isValid() || std::abs (backgroundScale - scale) > 0.01f)
    {
        backgroundScale = scale;
        background = juce::Image (juce::Image::ARGB, juce::roundToInt (kW * scale), juce::roundToInt (kH * scale), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (scale));
        paintBackground (bg);
    }
    g.drawImageTransformed (background, juce::AffineTransform::scale (1.0f / backgroundScale));

    // ---- Valores de los mandos
    auto value = [&] (juce::Slider& s, juce::Point<int> c, bool big)
    {
        g.setColour (theme::ink);
        g.setFont (font (*fonts, Face::soraSemi, big ? 22.0f : 14.0f, -0.01f));
        const auto r = big ? centredRect (c.x, c.y + 80, 180, 30) : centredRect (c.x, c.y + 45, 120, 20);
        g.drawText (s.getTextFromValue (s.getValue()), r, juce::Justification::centred, false);
    };
    value (pitch, kPitchC, true);
    value (drive, kDriveC, false);
    value (mix, kMixC, false);
    value (intensity, kIntensityC, false);
    value (output, kOutputC, false);
    {
        // Formant: con Link se muestra el total en verde
        const bool linked = linkButton.getToggleState();
        const double total = juce::jlimit (-12.0, 12.0, formant.getValue() + (linked ? pitch.getValue() : 0.0));
        g.setColour (linked ? theme::limeDeep : theme::ink);
        g.setFont (font (*fonts, Face::soraSemi, 22.0f, -0.01f));
        g.drawText (formant.getTextFromValue (total), centredRect (kFormantC.x, kFormantC.y + 80, 180, 30), juce::Justification::centred, false);
    }

    // ---- Líneas del Link
    const bool on = linkButton.getToggleState();
    const float y = (float) kLink.getCentreY();
    g.setColour (on ? theme::limeLine : theme::track);
    g.drawLine (270.0f, y, (float) kLink.getX() - 4.0f, y, 1.6f);
    g.drawLine ((float) kLink.getRight() + 4.0f, y, 490.0f, y, 1.6f);
    g.fillEllipse (267.0f, y - 3.0f, 6.0f, 6.0f);
    g.fillEllipse (487.0f, y - 3.0f, 6.0f, 6.0f);
}

void AlterIvoView::paintBackground (juce::Graphics& g)
{
    const float W = (float) kW;
    g.fillAll (theme::bg);

    // Halo lima detrás de la pantalla
    {
        juce::ColourGradient glow (theme::lime.withAlpha (theme::glowStrength), 380.0f, 262.0f, theme::bg.withAlpha (0.0f), 380.0f + 312.0f, 262.0f, true);
        glow.addColour (0.35, theme::lime.withAlpha (theme::glowStrength * 0.33f));
        glow.addColour (0.68, theme::lime.withAlpha (0.0f));
        g.setGradientFill (glow);
        g.fillRect (getLocalBounds());
    }

    // Nombre y descripción
    juce::AttributedString title;
    title.append ("alter", font (*fonts, Face::soraLight, 26.0f, -0.01f), theme::ink);
    title.append ("IVO", font (*fonts, Face::soraBold, 26.0f, -0.01f), theme::green);
    // Frase descriptiva centrada bajo el nombre (los dos sobre el mismo eje)
    const auto tagFont = font (*fonts, Face::manropeX, 9.5f, 0.24f);
    const juce::String tag (juce::CharPointer_UTF8 ("FORMANT \xc2\xb7 PITCH \xc2\xb7 VOICE"));
    const float tagW = textWidth (tagFont, tag) - 9.5f * 0.24f;   // sin el espaciado tras la última letra
    const float titleW = textWidth (font (*fonts, Face::soraLight, 26.0f, -0.01f), "alter")
                       + textWidth (font (*fonts, Face::soraBold, 26.0f, -0.01f), "IVO");
    const float axis = 36.0f + juce::jmax (tagW, titleW) * 0.5f;
    title.setJustification (juce::Justification::centred);
    title.draw (g, juce::Rectangle<float> (axis - 150.0f, 28.0f, 300.0f, 36.0f));

    g.setColour (theme::textDim);
    g.setFont (tagFont);
    g.drawText (tag, juce::Rectangle<float> (axis - tagW * 0.5f, 64.0f, tagW + 20.0f, 14.0f), juce::Justification::centredLeft, false);

    // Sombras de las piezas blancas
    softShadow (g, roundRect (kPresets.toFloat(), 18.0f), 0.10f, 18, 6);
    softShadow (g, roundRect (kKey.toFloat(), 16.0f), 0.10f, 16, 5);
    softShadow (g, roundRect (kLink.toFloat(), 14.0f), 0.14f, 12, 5);
    for (auto c : { kPitchC, kFormantC })
        softShadow (g, circle (c.toFloat(), kBigR), 0.20f, 30, 16);
    for (auto c : { kDriveC, kMixC, kIntensityC, kOutputC })
        softShadow (g, circle (c.toFloat(), kSmallR), 0.16f, 12, 6);

    // Tarjeta de la pantalla
    const auto card = kCard.toFloat();
    softShadow (g, roundRect (card, 26.0f), 0.17f, 50, 26);
    g.setGradientFill (juce::ColourGradient (theme::surface, 0.0f, card.getY(), theme::cardBottom, 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle (card, 26.0f);
    g.setColour (theme::green.withAlpha (0.06f));
    g.drawRoundedRectangle (card.reduced (0.5f), 26.0f, 1.0f);

    // Nombres de los mandos
    g.setColour (theme::textDim);
    g.setFont (font (*fonts, Face::manropeX, 9.5f, 0.24f));
    g.drawText ("PITCH",   centredRect (kPitchC.x, 116, 160, 14), juce::Justification::centred, false);
    g.drawText ("FORMANT", centredRect (kFormantC.x, 116, 160, 14), juce::Justification::centred, false);
    g.drawText ("DRIVE",   centredRect (kDriveC.x, kDriveC.y - 72, 120, 14), juce::Justification::centred, false);
    g.drawText ("MIX",     centredRect (kMixC.x, kMixC.y - 72, 120, 14), juce::Justification::centred, false);
    g.drawText ("INTENSITY", centredRect (kIntensityC.x, kIntensityC.y - 72, 120, 14), juce::Justification::centred, false);
    g.drawText ("OUTPUT",  centredRect (kOutputC.x, kOutputC.y - 72, 120, 14), juce::Justification::centred, false);

    // Firma IVO abajo a la derecha (las letras ocupan del 13.45 % al 86.55 % del ancho del SVG)
    if (logoMark != nullptr)
    {
        const float size = 46.0f;
        const float x = W - 36.0f - 0.8655f * size;
        logoMark->drawWithin (g, juce::Rectangle<float> (x, 515.0f - size * 0.5f, size, size), juce::RectanglePlacement::centred, 1.0f);
    }
}

void AlterIvoView::resized()
{
    pitch.setBounds (box (kPitchC, kBigBox));
    formant.setBounds (box (kFormantC, kBigBox));
    drive.setBounds (box (kDriveC, kSmallBox));
    mix.setBounds (box (kMixC, kSmallBox));
    intensity.setBounds (box (kIntensityC, kSmallBox));
    output.setBounds (box (kOutputC, kSmallBox));
    modeSelector.setBounds (kModes);
    sourceSelector.setBounds (kSource);
    syncPill.setBounds (kSyncPill);
    abPill.setBounds (kAbPill);
    settingsButton.setBounds (kSettings);
    keyBox.setBounds (kKey);
    linkButton.setBounds (kLink);
    display.setBounds (kCard);
    inMeter.setBounds (kInMeter);
    outMeter.setBounds (kOutMeter);
    presetBox.setBounds (kPresets);
}

// ================================================================= Editor
AlterIvoEditor::AlterIvoEditor (AlterIvoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    const float s = juce::jlimit (0.75f, 2.0f, proc.uiScale.load());   // antes de que el redimensionador la cambie
    setLookAndFeel (&view.getLookAndFeel());
    addAndMakeVisible (view);

    // Ventana escalable de 75 % a 200 % manteniendo la proporción
    setResizable (true, true);
    setResizeLimits (kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 2, kBaseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    setSize (juce::roundToInt (kBaseW * s), juce::roundToInt (kBaseH * s));
}

AlterIvoEditor::~AlterIvoEditor() { setLookAndFeel (nullptr); }

void AlterIvoEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, kBaseW, kBaseH);
    proc.uiScale = s;
}
