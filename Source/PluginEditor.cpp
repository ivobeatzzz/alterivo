#include "PluginEditor.h"

namespace theme = alterivo::theme;

namespace
{
juce::Font font (float size, bool bold = false, float kerning = 0.0f)
{
    auto f = juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}

constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

constexpr int kW = 760, kH = 540;

// Posiciones del diseño
const juce::Rectangle<int> kPitchKnob   { 35, 108, 180, 180 };
const juce::Rectangle<int> kFormantKnob { 545, 108, 180, 180 };
const juce::Rectangle<int> kDisplay     { 238, 100, 284, 206 };
const juce::Rectangle<int> kLink        { 361, 317, 38, 38 };
const juce::Rectangle<int> kDriveKnob   { 60, 392, 90, 90 };
const juce::Rectangle<int> kMixKnob     { 190, 392, 90, 90 };
const juce::Rectangle<int> kInMeter     { 336, 404, 392, 22 };
const juce::Rectangle<int> kOutMeter    { 336, 440, 392, 22 };
const juce::Rectangle<int> kModes       { kW - 32 - 330, 26, 330, 34 };
const juce::Rectangle<int> kPresets     { 32, 501, 300, 30 };

// Marca IVO (la O es un mando): mismo SVG que el logotipo
const char* kLogoSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="-6 0 190 100">
<defs><linearGradient id="g" gradientUnits="userSpaceOnUse" x1="0" y1="10" x2="175" y2="90">
<stop offset="0" stop-color="#c8f56a"/><stop offset="1" stop-color="#7fbf3a"/></linearGradient></defs>
<line x1="14" y1="18" x2="14" y2="82" stroke="url(#g)" stroke-width="11" stroke-linecap="round"/>
<polyline points="38,18 63,82 88,18" fill="none" stroke="url(#g)" stroke-width="11" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M117.37,72.63 A32,32 0 1 1 162.63,72.63" fill="none" stroke="url(#g)" stroke-width="11" stroke-linecap="round"/>
<circle cx="149.64" cy="38.51" r="6.82" fill="url(#g)"/>
</svg>)SVG";

juce::Rectangle<int> centredRect (int cx, int y, int w, int h) { return { cx - w / 2, y, w, h }; }

// Trazo con halo luminoso
void strokeGlow (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c, float intensity = 1.0f)
{
    using PST = juce::PathStrokeType;
    const float widths[3] = { width * 5.0f, width * 3.0f, width * 1.8f };
    const float alphas[3] = { 0.05f, 0.10f, 0.22f };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (c.withAlpha (alphas[i] * intensity));
        g.strokePath (p, PST (widths[i], PST::curved, PST::rounded));
    }
    g.setColour (c);
    g.strokePath (p, PST (width, PST::curved, PST::rounded));
}

void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x10ffffff), 0.0f, r.getY(),
                                             juce::Colour (0x04ffffff), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x30ffffff), 0.0f, r.getY(),
                                             juce::Colour (0x0affffff), 0.0f, r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}
} // namespace

// ============================================================ LookAndFeel
AlterIvoLookAndFeel::AlterIvoLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0d120a));
    setColour (juce::PopupMenu::textColourId, theme::textMain);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::accentText);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
}

void AlterIvoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const bool big = size > 120.0f;
    const float ringR = size * 0.5f - (big ? 14.0f : 16.0f);
    const float ringW = big ? 4.0f : 3.0f;
    const float bodyR = ringR - (big ? 15.0f : 9.0f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    using PST = juce::PathStrokeType;

    // Pista
    juce::Path trackPath;
    trackPath.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::track);
    g.strokePath (trackPath, PST (ringW, PST::curved, PST::rounded));

    // Valor con halo (bipolar desde el centro si el rango cruza el 0)
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path valuePath;
        valuePath.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        strokeGlow (g, valuePath, ringW, theme::accent);
    }

    // Cuerpo del mando: metal oscuro con brillo superior
    juce::Path body;
    body.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), big ? 22 : 10, { 0, big ? 8 : 4 }).drawForPath (g, body);

    juce::ColourGradient bodyGrad (juce::Colour (0xff2a3324), c.x, c.y - bodyR * 0.9f,
                                   juce::Colour (0xff0b0f09), c.x, c.y + bodyR, false);
    g.setGradientFill (bodyGrad);
    g.fillPath (body);

    juce::ColourGradient rim (juce::Colour (0x40ffffff), c.x, c.y - bodyR, juce::Colour (0x00ffffff), c.x, c.y + bodyR * 0.2f, false);
    g.setGradientFill (rim);
    g.drawEllipse (c.x - bodyR + 0.5f, c.y - bodyR + 0.5f, bodyR * 2.0f - 1.0f, bodyR * 2.0f - 1.0f, 1.2f);

    // Anillo interior sutil
    const float innerR = bodyR * 0.72f;
    g.setColour (juce::Colour (0x0cffffff));
    g.drawEllipse (c.x - innerR, c.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    // Indicador
    juce::Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (bodyR * 0.45f, angle));
    pointer.lineTo (c.getPointOnCircumference (bodyR * 0.82f, angle));
    strokeGlow (g, pointer, big ? 3.0f : 2.2f, theme::textMain, 0.6f);

    // Punto luminoso en el anillo
    const auto dot = c.getPointOnCircumference (ringR, angle);
    juce::Path dotPath;
    const float dr = big ? 5.5f : 4.0f;
    dotPath.addEllipse (dot.x - dr, dot.y - dr, dr * 2.0f, dr * 2.0f);
    g.setColour (theme::accent.withAlpha (0.25f));
    g.fillEllipse (dot.x - dr * 2.2f, dot.y - dr * 2.2f, dr * 4.4f, dr * 4.4f);
    g.setColour (theme::accent);
    g.fillPath (dotPath);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (dot.x - dr * 0.4f, dot.y - dr * 0.4f, dr * 0.8f, dr * 0.8f);
}

// =========================================================== ModeSelector
ModeSelector::ModeSelector() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

void ModeSelector::setSelected (int index)
{
    if (index != selected) { selected = index; repaint(); }
}

void ModeSelector::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float radius = r.getHeight() * 0.5f;
    glassPanel (g, r, radius);

    static const char* names[3] = { "TRANSPOSE", "QUANTIZE", "ROBOT" };
    const float segW = r.getWidth() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight()).reduced (3.0f);
        if (i == selected)
        {
            g.setColour (theme::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (seg.expanded (2.0f), radius);
            g.setGradientFill (juce::ColourGradient (theme::accent, 0.0f, seg.getY(), theme::accentDeep, 0.0f, seg.getBottom(), false));
            g.fillRoundedRectangle (seg, radius - 3.0f);
        }
        g.setColour (i == selected ? theme::accentText : theme::textDim);
        g.setFont (font (11.0f, true, 0.14f));
        g.drawText (names[i], seg, juce::Justification::centred, false);
    }
}

void ModeSelector::mouseDown (const juce::MouseEvent& e)
{
    const int index = juce::jlimit (0, 2, (int) ((float) e.x / ((float) getWidth() / 3.0f)));
    if (onChange) onChange (index);
}

// ============================================================= LinkButton
LinkButton::LinkButton() : juce::Button ("Link")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Link: Pitch y Formant se mueven juntos");
}

void LinkButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = r.getCentre();
    const bool on = getToggleState();

    if (on)
    {
        g.setColour (theme::accent.withAlpha (0.22f));
        g.fillEllipse (r.expanded (2.0f));
        g.setGradientFill (juce::ColourGradient (theme::accent, 0.0f, r.getY(), theme::accentDeep, 0.0f, r.getBottom(), false));
        g.fillEllipse (r);
    }
    else
    {
        g.setColour (juce::Colour (0xff121810));
        g.fillEllipse (r);
        g.setColour (highlighted ? theme::textDim : theme::glassEdge);
        g.drawEllipse (r, 1.0f);
    }

    // Icono de cadena: dos eslabones a 45°
    const float s = r.getWidth() * 0.19f;
    juce::Path chain;
    chain.addRoundedRectangle (-s * 1.55f, -s * 0.55f, s * 1.9f, s * 1.1f, s * 0.55f);
    chain.addRoundedRectangle (-s * 0.35f, -s * 0.55f, s * 1.9f, s * 1.1f, s * 0.55f);
    chain.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::pi * 0.25f).translated (c.x, c.y));
    g.setColour (on ? theme::accentText : (highlighted ? theme::textMain : theme::textDim));
    g.strokePath (chain, juce::PathStrokeType (1.8f));
}

// =========================================================== PitchDisplay
static float hzToNote (float hz) { return hz > 0.0f ? 69.0f + 12.0f * std::log2 (hz / 440.0f) : 0.0f; }

void PitchDisplay::push (float inHz, float outHz)
{
    head = (head + 1) % kLen;
    inNotes[(size_t) head] = hzToNote (inHz);
    outNotes[(size_t) head] = hzToNote (outHz);

    const float target = outNotes[(size_t) head] > 0.0f ? outNotes[(size_t) head] : inNotes[(size_t) head];
    if (target > 0.0f)
        centre += 0.08f * (target - centre);
    repaint();
}

void PitchDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xd0050705));
    g.fillRoundedRectangle (r, 14.0f);
    glassPanel (g, r, 14.0f);

    const auto plot = r.reduced (14.0f, 0.0f).withTrimmedTop (44.0f).withTrimmedBottom (12.0f);
    const float range = 9.0f;
    auto yFor = [&] (float note) { return plot.getCentreY() - (note - centre) / range * plot.getHeight() * 0.5f; };

    // Rejilla de semitonos y nombres de nota
    const int lo = (int) std::floor (centre - range), hi = (int) std::ceil (centre + range);
    for (int n = lo; n <= hi; ++n)
    {
        const float y = yFor ((float) n);
        if (y < plot.getY() || y > plot.getBottom()) continue;
        const int pc = ((n % 12) + 12) % 12;
        const bool natural = pc == 0 || pc == 2 || pc == 4 || pc == 7 || pc == 9;   // C D E G A (sin etiquetas pegadas)
        g.setColour (juce::Colour (natural ? 0x14ffffff : 0x08ffffff));
        g.drawHorizontalLine ((int) y, plot.getX() + 24.0f, plot.getRight());
        if (natural)
        {
            g.setColour (theme::textFaint);
            g.setFont (font (9.0f, true));
            g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), (int) plot.getX(), (int) y - 6, 24, 12,
                        juce::Justification::centredLeft, false);
        }
    }

    // Trazos (de antiguo a reciente)
    auto buildPath = [&] (const std::array<float, kLen>& notes)
    {
        juce::Path p;
        bool drawing = false;
        const float x0 = plot.getX() + 26.0f, w = plot.getRight() - x0;
        float prev = 0.0f;
        for (int i = 0; i < kLen; ++i)
        {
            const float v = notes[(size_t) ((head + 1 + i) % kLen)];
            const float x = x0 + w * (float) i / (float) (kLen - 1);
            const float y = yFor (v);
            // Sin voz, fuera de la vista o salto brusco: se corta el trazo
            if (v <= 0.0f || y < plot.getY() || y > plot.getBottom() || (drawing && std::abs (v - prev) > 2.5f))
            {
                drawing = false;
                if (v <= 0.0f || y < plot.getY() || y > plot.getBottom()) continue;
            }
            if (! drawing) { p.startNewSubPath (x, y); drawing = true; }
            else p.lineTo (x, y);
            prev = v;
        }
        return p;
    };

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    g.setColour (theme::textDim.withAlpha (0.55f));
    g.strokePath (buildPath (inNotes), juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    strokeGlow (g, buildPath (outNotes), 2.2f, theme::accent);
    g.restoreState();

    // Lectura de notas
    auto noteText = [] (float note)
    {
        if (note <= 0.0f) return juce::String ("--");
        const int n = (int) std::lround (note);
        const int cents = (int) std::lround ((note - (float) n) * 100.0f);
        return juce::MidiMessage::getMidiNoteName (n, true, true, 3) + juce::String (cents >= 0 ? "  +" : "  ") + juce::String (cents);
    };
    const float inNow = inNotes[(size_t) head], outNow = outNotes[(size_t) head];

    g.setFont (font (9.0f, true, 0.2f));
    g.setColour (theme::textDim);
    g.drawText ("INPUT", 16, 10, 100, 12, juce::Justification::centredLeft, false);
    g.drawText (robotNote >= 0 ? "ROBOT" : "OUTPUT", getWidth() - 116, 10, 100, 12, juce::Justification::centredRight, false);

    g.setFont (font (15.0f, true));
    g.setColour (theme::textMain);
    g.drawText (noteText (inNow), 16, 22, 120, 18, juce::Justification::centredLeft, false);
    g.setColour (theme::accent);
    const juce::String outStr = robotNote >= 0 ? juce::MidiMessage::getMidiNoteName (robotNote, true, true, 3) : noteText (outNow);
    g.drawText (outStr, getWidth() - 136, 22, 120, 18, juce::Justification::centredRight, false);
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
    g.setFont (font (9.5f, true, 0.2f));
    g.drawText (label, r.withWidth (34.0f), juce::Justification::centredLeft, false);

    const auto bar = r.withTrimmedLeft (36.0f).withTrimmedRight (56.0f).withSizeKeepingCentre (r.getWidth() - 92.0f, 8.0f);
    g.setColour (theme::track);
    g.fillRoundedRectangle (bar, 4.0f);

    auto xFor = [&] (float db) { return bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f); };

    // Marcas de escala
    g.setColour (juce::Colour (0x18ffffff));
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f })
        g.drawVerticalLine ((int) xFor (db), bar.getY() - 3.0f, bar.getY() - 1.0f);

    const float x = xFor (levelDb);
    if (x > bar.getX() + 1.0f)
    {
        juce::ColourGradient grad (theme::accentDeep, bar.getX(), 0.0f, theme::clip, bar.getRight(), 0.0f, false);
        grad.addColour ((xFor (-12.0f) - bar.getX()) / bar.getWidth(), theme::accent);
        grad.addColour ((xFor (-3.0f) - bar.getX()) / bar.getWidth(), theme::warn);
        const auto fill = bar.withRight (x);
        g.setGradientFill (grad);
        g.setOpacity (0.25f);
        g.fillRoundedRectangle (fill.expanded (0.0f, 3.0f), 6.0f);
        g.setOpacity (1.0f);
        g.fillRoundedRectangle (fill, 4.0f);
    }

    if (holdDb > -60.0f)
    {
        g.setColour (holdDb > -0.1f ? theme::clip : theme::textMain);
        g.fillRect (juce::Rectangle<float> (xFor (holdDb) - 1.0f, bar.getY() - 1.0f, 2.0f, bar.getHeight() + 2.0f));
    }

    g.setColour (holdDb > -0.1f ? theme::clip : theme::textDim);
    g.setFont (font (10.0f, true));
    const juce::String txt = holdDb > -60.0f ? juce::String (holdDb, 1) + " dB" : juce::String ("-inf");
    g.drawText (txt, r.withTrimmedLeft (r.getWidth() - 54.0f), juce::Justification::centredRight, false);
}

// ============================================================== PresetBox
PresetBox::PresetBox() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

void PresetBox::setText (const juce::String& t)
{
    if (t != text) { text = t; repaint(); }
}

void PresetBox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    glassPanel (g, r, r.getHeight() * 0.5f);

    auto arrow = [&] (float cx, bool left)
    {
        juce::Path p;
        const float cy = r.getCentreY();
        if (left) p.addTriangle (cx + 3.0f, cy - 5.0f, cx + 3.0f, cy + 5.0f, cx - 4.0f, cy);
        else      p.addTriangle (cx - 3.0f, cy - 5.0f, cx - 3.0f, cy + 5.0f, cx + 4.0f, cy);
        g.setColour (theme::textDim);
        g.fillPath (p);
    };
    arrow (r.getX() + 18.0f, true);
    arrow (r.getRight() - 18.0f, false);

    g.setColour (juce::Colour (0x18ffffff));
    g.drawVerticalLine (36, r.getY() + 7.0f, r.getBottom() - 7.0f);
    g.drawVerticalLine (getWidth() - 36, r.getY() + 7.0f, r.getBottom() - 7.0f);

    g.setColour (theme::textMain);
    g.setFont (font (12.5f, true));
    g.drawText (text, r.reduced (44.0f, 0.0f), juce::Justification::centred, true);
}

void PresetBox::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < 36) { if (onPrev) onPrev(); }
    else if (e.x > getWidth() - 36) { if (onNext) onNext(); }
    else if (onClick) onClick();
}

// ================================================================= Editor
AlterIvoEditor::AlterIvoEditor (AlterIvoProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (pitch, true);
    setupKnob (formant, true);
    setupKnob (drive, false);
    setupKnob (mix, false);

    pitchAtt   = std::make_unique<SliderAttachment> (proc.apvts, "pitch", pitch);
    formantAtt = std::make_unique<SliderAttachment> (proc.apvts, "formant", formant);
    driveAtt   = std::make_unique<SliderAttachment> (proc.apvts, "drive", drive);
    mixAtt     = std::make_unique<SliderAttachment> (proc.apvts, "mix", mix);

    pitch.setDoubleClickReturnValue (true, 0.0);
    formant.setDoubleClickReturnValue (true, 0.0);
    drive.setDoubleClickReturnValue (true, 0.0);
    mix.setDoubleClickReturnValue (true, 100.0);

    lastPitch = pitch.getValue();
    lastFormant = formant.getValue();
    pitch.onValueChange   = [this] { linkedMove (pitch, formant, lastPitch, lastFormant); repaint(); };
    formant.onValueChange = [this] { linkedMove (formant, pitch, lastFormant, lastPitch); repaint(); };
    drive.onValueChange   = [this] { repaint(); };
    mix.onValueChange     = [this] { repaint(); };

    modeSelector.onChange = [this] (int i) { setMode (i); };
    addAndMakeVisible (modeSelector);

    linkButton.onStateChange = [this] { repaint(); };
    addAndMakeVisible (linkButton);
    linkAtt = std::make_unique<ButtonAttachment> (proc.apvts, "link", linkButton);

    addAndMakeVisible (display);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    presetBox.onClick = [this] { showPresetMenu(); };
    presetBox.onPrev  = [this] { loadPreset ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); };
    presetBox.onNext  = [this] { loadPreset ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    addAndMakeVisible (presetBox);

    if (auto xml = juce::XmlDocument::parse (kLogoSvg))
        logoMark = juce::Drawable::createFromSVG (*xml);

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

AlterIvoEditor::~AlterIvoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AlterIvoEditor::setupKnob (juce::Slider& s, bool big)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (kStart, kEnd, true);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setMouseDragSensitivity (big ? 280 : 200);
    s.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (s);
}

// Link: al mover un mando con el ratón, el otro se mueve lo mismo
void AlterIvoEditor::linkedMove (juce::Slider& moved, juce::Slider& other, double& lastMoved, double& lastOther)
{
    const double now = moved.getValue();
    if (! linking && linkButton.getToggleState() && moved.isMouseButtonDown())
    {
        linking = true;
        other.setValue (juce::jlimit (other.getMinimum(), other.getMaximum(), other.getValue() + (now - lastMoved)),
                        juce::sendNotificationSync);
        linking = false;
    }
    lastMoved = now;
    lastOther = other.getValue();
}

void AlterIvoEditor::setMode (int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mode")))
    {
        param->beginChangeGesture();
        *param = index;
        param->endChangeGesture();
    }
    timerCallback();
}

void AlterIvoEditor::loadPreset (int index)
{
    proc.setCurrentProgram (index);
    lastPitch = pitch.getValue();
    lastFormant = formant.getValue();
    timerCallback();
}

void AlterIvoEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        menu.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox),
                        [safe = juce::Component::SafePointer<AlterIvoEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->loadPreset (result - 1);
                        });
}

juce::String AlterIvoEditor::noteName (float hz)
{
    if (hz <= 0.0f) return "--";
    return juce::MidiMessage::getMidiNoteName ((int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)), true, true, 3);
}

void AlterIvoEditor::timerCallback()
{
    const int mode = (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load());
    if (mode != currentMode)
    {
        currentMode = mode;
        modeSelector.setSelected (mode);
    }

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
    display.push (proc.getDetectedHz(), proc.getOutputHz());

    inMeter.setPeak (proc.takeInputPeak());
    outMeter.setPeak (proc.takeOutputPeak());
}

void AlterIvoEditor::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();

    // Fondo: negro verdoso con halos del verde de la marca
    g.fillAll (theme::bgDeep);
    g.setGradientFill (juce::ColourGradient (theme::brandOlive.withAlpha (0.75f), W * 0.22f, -60.0f,
                                             theme::bgDeep.withAlpha (0.0f), W * 0.22f, H * 0.85f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (theme::accent.withAlpha (0.06f), W * 0.5f, H * 0.42f,
                                             theme::bgDeep.withAlpha (0.0f), W * 0.5f + 330.0f, H * 0.42f, true));
    g.fillAll();

    // Cabecera
    juce::AttributedString title;
    title.append ("alter", font (32.0f), theme::textMain);
    title.append ("IVO", font (32.0f, true), theme::accent);
    title.setJustification (juce::Justification::centredLeft);
    title.draw (g, juce::Rectangle<float> (32.0f, 20.0f, 300.0f, 40.0f));

    g.setColour (theme::textDim);
    g.setFont (font (9.5f, true, 0.32f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("FORMANT \xc2\xb7 PITCH \xc2\xb7 VOICE")), 34, 60, 300, 14,
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0x12ffffff));
    g.drawHorizontalLine (84, 32.0f, W - 32.0f);

    // Nombres y valores de los mandos grandes
    g.setFont (font (11.0f, true, 0.3f));
    g.setColour (theme::textDim);
    g.drawText ("PITCH",   centredRect (kPitchKnob.getCentreX(), 96, 180, 14), juce::Justification::centred, false);
    g.drawText ("FORMANT", centredRect (kFormantKnob.getCentreX(), 96, 180, 14), juce::Justification::centred, false);

    g.setFont (font (20.0f, true));
    g.setColour (theme::textMain);
    g.drawText (pitch.getTextFromValue (pitch.getValue()),     centredRect (kPitchKnob.getCentreX(), 286, 180, 24), juce::Justification::centred, false);
    g.drawText (formant.getTextFromValue (formant.getValue()), centredRect (kFormantKnob.getCentreX(), 286, 180, 24), juce::Justification::centred, false);

    // Puente de Link entre Pitch y Formant
    {
        const bool on = linkButton.getToggleState();
        const float y = (float) kLink.getCentreY();
        juce::Path bridge;
        bridge.startNewSubPath ((float) kPitchKnob.getCentreX(), 314.0f);
        bridge.quadraticTo ((float) kPitchKnob.getCentreX(), y, (float) kPitchKnob.getCentreX() + 30.0f, y);
        bridge.lineTo ((float) kLink.getX() - 4.0f, y);
        bridge.startNewSubPath ((float) kLink.getRight() + 4.0f, y);
        bridge.lineTo ((float) kFormantKnob.getCentreX() - 30.0f, y);
        bridge.quadraticTo ((float) kFormantKnob.getCentreX(), y, (float) kFormantKnob.getCentreX(), 314.0f);
        if (on) strokeGlow (g, bridge, 1.6f, theme::accent, 0.8f);
        else
        {
            g.setColour (theme::track.brighter (0.2f));
            g.strokePath (bridge, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (on ? theme::accent : theme::textFaint);
        g.setFont (font (9.0f, true, 0.3f));
        g.drawText ("LINK", centredRect (kLink.getCentreX(), kLink.getBottom() + 4, 80, 12), juce::Justification::centred, false);
    }

    // Fila inferior
    g.setColour (juce::Colour (0x12ffffff));
    g.drawHorizontalLine (378, 32.0f, W - 32.0f);

    g.setFont (font (10.0f, true, 0.3f));
    g.setColour (theme::textDim);
    g.drawText ("DRIVE", centredRect (kDriveKnob.getCentreX(), 382, 120, 12), juce::Justification::centred, false);
    g.drawText ("MIX",   centredRect (kMixKnob.getCentreX(), 382, 120, 12), juce::Justification::centred, false);
    g.setFont (font (13.0f, true));
    g.setColour (theme::textMain);
    g.drawText (drive.getTextFromValue (drive.getValue()), centredRect (kDriveKnob.getCentreX(), 470, 120, 16), juce::Justification::centred, false);
    g.drawText (mix.getTextFromValue (mix.getValue()),     centredRect (kMixKnob.getCentreX(), 470, 120, 16), juce::Justification::centred, false);

    g.setFont (font (10.0f, true, 0.3f));
    g.setColour (theme::textDim);
    g.drawText ("LEVEL", kInMeter.getX(), 382, 120, 12, juce::Justification::centredLeft, false);

    // Pie: marca IVO
    g.setColour (juce::Colour (0x0cffffff));
    g.fillRect (juce::Rectangle<float> (0.0f, 494.0f, W, H - 494.0f));
    g.setColour (juce::Colour (0x12ffffff));
    g.drawHorizontalLine (494, 0.0f, W);

    if (logoMark != nullptr)
        logoMark->drawWithin (g, juce::Rectangle<float> (W - 32.0f - 168.0f, 503.0f, 50.0f, 26.0f),
                              juce::RectanglePlacement::centred, 1.0f);
    g.setColour (theme::textMain);
    g.setFont (font (12.0f, true, 0.32f));
    g.drawText ("IVO BEATZ", (int) W - 32 - 110, 503, 110, 26, juce::Justification::centredRight, false);
}

void AlterIvoEditor::resized()
{
    pitch.setBounds (kPitchKnob);
    formant.setBounds (kFormantKnob);
    drive.setBounds (kDriveKnob);
    mix.setBounds (kMixKnob);
    modeSelector.setBounds (kModes);
    linkButton.setBounds (kLink);
    display.setBounds (kDisplay);
    inMeter.setBounds (kInMeter);
    outMeter.setBounds (kOutMeter);
    presetBox.setBounds (kPresets);
}
