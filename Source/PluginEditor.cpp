#include "PluginEditor.h"
#include "Theme.h"

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

// Posiciones (en píxeles) del diseño 520 x 470
const juce::Rectangle<int> kPitchKnob   { 75, 115, 150, 150 };
const juce::Rectangle<int> kFormantKnob { 295, 115, 150, 150 };
const juce::Rectangle<int> kDriveKnob   { 288, 350, 84, 84 };
const juce::Rectangle<int> kMixKnob     { 398, 350, 84, 84 };
const juce::Rectangle<int> kModeSwitch  { 20, 340, 164, 86 };
const juce::Rectangle<int> kLinkSwitch  { 200, 381, 44, 22 };
const juce::Rectangle<int> kDisplay     { 300, 32, 184, 46 };

juce::Rectangle<int> centredRect (int cx, int y, int w, int h) { return { cx - w / 2, y, w, h }; }
} // namespace

// ============================================================ LookAndFeel
AlterIvoLookAndFeel::AlterIvoLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, theme::window);
    setColour (juce::PopupMenu::textColourId, theme::textMain);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::cream);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::brand);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
}

void AlterIvoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const bool ticks = (bool) slider.getProperties().getWithDefault ("ticks", false);
    const float R = ticks ? size * 0.5f - 23.0f : size * 0.5f - 14.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Marcas de escala
    if (ticks)
    {
        g.setColour (theme::textDim);
        for (int k = 0; k <= 10; ++k)
        {
            const float a = rotaryStartAngle + (float) k / 10.0f * (rotaryEndAngle - rotaryStartAngle);
            const float r2 = R + (k % 5 == 0 ? 14.0f : 11.0f);
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (R + 8.0f, a), c.getPointOnCircumference (r2, a)), 1.5f);
        }
    }

    // Cuerpo con sombra
    juce::Path body;
    body.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 12, { 0, 4 }).drawForPath (g, body);

    juce::ColourGradient grad (theme::creamLight, c.x - R * 0.2f, c.y - R * 0.3f,
                               theme::creamDark, c.x + R * 0.55f, c.y + R * 0.85f, true);
    grad.addColour (0.7, theme::creamMid);
    g.setGradientFill (grad);
    g.fillPath (body);

    g.setColour (theme::creamRing);
    g.drawEllipse (c.x - R * 0.78f, c.y - R * 0.78f, R * 1.56f, R * 1.56f, 1.0f);

    // Indicador
    juce::Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (R * 0.38f, angle));
    pointer.lineTo (c.getPointOnCircumference (R * 0.82f, angle));
    g.setColour (theme::brand);
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (3.0f, R * 0.08f), juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

// ============================================================= ModeSwitch
ModeSwitch::ModeSwitch()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Point<float> ModeSwitch::centre() const { return { 80.0f, 52.0f }; }

void ModeSwitch::setSelected (int index)
{
    if (index != selected)
    {
        selected = index;
        repaint();
    }
}

void ModeSwitch::paint (juce::Graphics& g)
{
    const auto c = centre();
    const float R = 24.0f;
    const float angles[3] = { juce::degreesToRadians (-55.0f), 0.0f, juce::degreesToRadians (55.0f) };

    for (int i = 0; i < 3; ++i)
    {
        g.setColour (i == selected ? theme::cream : theme::textDim);
        g.fillEllipse (juce::Rectangle<float> (5.2f, 5.2f).withCentre (c.getPointOnCircumference (R + 8.0f, angles[i])));
    }

    auto label = [&] (int i, juce::Rectangle<float> r, juce::Justification j, const char* text)
    {
        g.setColour (i == selected ? theme::cream : theme::textDim);
        g.setFont (font (8.5f, i == selected, 0.12f));
        g.drawText (text, r, j, false);
    };
    label (0, { c.x - 82.0f, c.y - 24.0f, 44.0f, 14.0f }, juce::Justification::centredRight, "TRANS");
    label (1, { c.x - 40.0f, c.y - R - 26.0f, 80.0f, 14.0f }, juce::Justification::centred, "QUANT");
    label (2, { c.x + 38.0f, c.y - 24.0f, 44.0f, 14.0f }, juce::Justification::centredLeft, "ROBOT");

    juce::Path body;
    body.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 10, { 0, 3 }).drawForPath (g, body);
    g.setColour (theme::window);
    g.fillPath (body);
    g.setColour (theme::border);
    g.strokePath (body, juce::PathStrokeType (1.0f));

    juce::Path bar;
    bar.addRoundedRectangle (-4.0f, -R + 3.0f, 8.0f, R * 2.0f - 6.0f, 4.0f);
    bar.applyTransform (juce::AffineTransform::rotation (angles[selected]).translated (c.x, c.y));
    g.setColour (theme::cream);
    g.fillPath (bar);
}

void ModeSwitch::mouseDown (const juce::MouseEvent& e)
{
    const auto c = centre();
    int index;
    if (e.position.getDistanceFrom (c) <= 28.0f)
        index = (selected + 1) % 3;                       // clic en el selector: siguiente modo
    else if (e.position.x < c.x - 15.0f)
        index = 0;
    else if (e.position.x > c.x + 15.0f)
        index = 2;
    else
        index = 1;

    if (onChange)
        onChange (index);
}

// ============================================================= PillSwitch
PillSwitch::PillSwitch() : juce::Button ("Link")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PillSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (theme::window);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (highlighted ? theme::border.brighter (0.2f) : theme::border);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    const float d = r.getHeight() - 7.0f;
    const float cx = on ? r.getRight() - 3.5f - d * 0.5f : r.getX() + 3.5f + d * 0.5f;
    g.setColour (on ? theme::cream : theme::textDim);
    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ cx, r.getCentreY() }));
}

// ============================================================== PresetBox
PresetBox::PresetBox()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PresetBox::setText (const juce::String& t)
{
    if (t != text)
    {
        text = t;
        repaint();
    }
}

void PresetBox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::faceBottom);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    g.setColour (theme::textMain);
    g.setFont (font (12.0f, true));
    g.drawText (text, getLocalBounds().reduced (10, 0).withTrimmedRight (14), juce::Justification::centredLeft, true);

    juce::Path tri;
    const float tx = r.getRight() - 14.0f, ty = r.getCentreY();
    tri.addTriangle (tx - 4.0f, ty - 2.0f, tx + 4.0f, ty - 2.0f, tx, ty + 3.0f);
    g.setColour (theme::textDim);
    g.fillPath (tri);
}

void PresetBox::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
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

    // Doble clic = valor por defecto
    pitch.setDoubleClickReturnValue (true, 0.0);
    formant.setDoubleClickReturnValue (true, 0.0);
    drive.setDoubleClickReturnValue (true, 0.0);
    mix.setDoubleClickReturnValue (true, 100.0);

    modeSwitch.onChange = [this] (int i) { setMode (i); };
    addAndMakeVisible (modeSwitch);

    linkSwitch.onStateChange = [this] { repaint(); };
    addAndMakeVisible (linkSwitch);
    linkAtt = std::make_unique<ButtonAttachment> (proc.apvts, "link", linkSwitch);

    prevPreset.onClick = [this] { loadPreset ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); };
    nextPreset.onClick = [this] { loadPreset ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    presetBox.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);
    addAndMakeVisible (presetBox);

    setSize (520, 510);
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
    s.setMouseDragSensitivity (big ? 260 : 200);
    s.getProperties().set ("ticks", big);
    s.onValueChange = [this] { repaint(); };
    addAndMakeVisible (s);
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

juce::String AlterIvoEditor::noteName (float hz)
{
    if (hz <= 0.0f)
        return "--";
    const int note = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);   // nomenclatura de Ableton (60 = C3)
}

void AlterIvoEditor::loadPreset (int index)
{
    proc.setCurrentProgram (index);
    timerCallback();
}

void AlterIvoEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSectionHeader ("PRESETS ALTERIVO");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        menu.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox),
                        [safe = juce::Component::SafePointer<AlterIvoEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->loadPreset (result - 1);
                        });
}

void AlterIvoEditor::timerCallback()
{
    if (proc.getCurrentProgram() != shownPreset)
    {
        shownPreset = proc.getCurrentProgram();
        presetBox.setText (proc.getProgramName (shownPreset));
    }

    const int mode = (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load());
    if (mode != currentMode)
    {
        currentMode = mode;
        modeSwitch.setSelected (mode);
    }

    const bool robot = mode == 2;
    juce::String newIn = noteName (proc.getDetectedHz());
    juce::String newOut;
    if (robot)
    {
        const int midi = proc.getHeldMidiNote();
        const int note = midi >= 0 ? midi : 72 + (int) std::lround (proc.apvts.getRawParameterValue ("pitch")->load());
        newOut = juce::MidiMessage::getMidiNoteName (note, true, true, 3);
    }
    else
    {
        newOut = noteName (proc.getOutputHz());
    }

    if (newIn != inText || newOut != outText || robot != robotMode)
    {
        inText = newIn;
        outText = newOut;
        robotMode = robot;
        repaint (kDisplay);
    }
}

void AlterIvoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::frame);

    // Panel
    const juce::Rectangle<float> face (10.0f, 10.0f, (float) getWidth() - 20.0f, (float) getHeight() - 20.0f);
    g.setGradientFill (juce::ColourGradient (theme::faceTop, 0.0f, face.getY(), theme::faceBottom, 0.0f, face.getBottom(), false));
    g.fillRoundedRectangle (face, 18.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (face.reduced (0.5f), 18.0f, 1.0f);

    // Logotipo
    juce::AttributedString logo;
    logo.append ("alter", font (30.0f), theme::textMain);
    logo.append ("IVO", font (30.0f, true), theme::cream);
    logo.setJustification (juce::Justification::centredLeft);
    logo.draw (g, juce::Rectangle<float> (36.0f, 28.0f, 260.0f, 36.0f));

    g.setColour (theme::textDim);
    g.setFont (font (9.5f, false, 0.25f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("VOICE MANIPULATOR \xc2\xb7 IVO BEATZ")),
                38, 64, 300, 14, juce::Justification::centredLeft, false);

    // Pantalla de notas
    g.setColour (theme::window);
    g.fillRoundedRectangle (kDisplay.toFloat(), 8.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (kDisplay.toFloat().reduced (0.5f), 8.0f, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (font (9.0f, false, 0.18f));
    g.drawText ("IN", 314, 38, 60, 12, juce::Justification::centredLeft, false);
    g.drawText (robotMode ? "NOTE" : "OUT", 392, 38, 80, 12, juce::Justification::centredLeft, false);
    g.setFont (font (16.0f, true));
    g.setColour (theme::textMain);
    g.drawText (inText, 314, 52, 76, 20, juce::Justification::centredLeft, false);
    g.setColour (theme::cream);
    g.drawText (outText, 392, 52, 84, 20, juce::Justification::centredLeft, false);

    // Separadores
    g.setColour (theme::border);
    g.drawHorizontalLine (96, 36.0f, (float) getWidth() - 36.0f);
    g.drawHorizontalLine (300, 36.0f, (float) getWidth() - 36.0f);

    // Nombres
    g.setColour (theme::textMain);
    g.setFont (font (11.0f, true, 0.2f));
    g.drawText ("PITCH",   centredRect (150, 102, 150, 16), juce::Justification::centred, false);
    g.drawText ("FORMANT", centredRect (370, 102, 150, 16), juce::Justification::centred, false);
    g.drawText ("MODE",    centredRect (100, 319, 100, 16), juce::Justification::centred, false);
    g.drawText ("LINK",    centredRect (222, 319, 100, 16), juce::Justification::centred, false);
    g.drawText ("DRIVE",   centredRect (330, 319, 100, 16), juce::Justification::centred, false);
    g.drawText ("MIX",     centredRect (440, 319, 100, 16), juce::Justification::centred, false);

    // Valores
    g.setFont (font (13.0f, true));
    g.setColour (theme::cream);
    g.drawText (pitch.getTextFromValue (pitch.getValue()),     centredRect (150, 256, 150, 22), juce::Justification::centred, false);
    g.drawText (formant.getTextFromValue (formant.getValue()), centredRect (370, 256, 150, 22), juce::Justification::centred, false);
    g.drawText (drive.getTextFromValue (drive.getValue()),     centredRect (330, 434, 100, 18), juce::Justification::centred, false);
    g.drawText (mix.getTextFromValue (mix.getValue()),         centredRect (440, 434, 100, 18), juce::Justification::centred, false);
    g.setColour (linkSwitch.getToggleState() ? theme::cream : theme::textDim);
    g.drawText (linkSwitch.getToggleState() ? "ON" : "OFF",   centredRect (222, 434, 100, 18), juce::Justification::centred, false);

    // Barra de presets (parte baja del panel)
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path facePath;
        facePath.addRoundedRectangle (face, 18.0f);
        g.reduceClipRegion (facePath);
        g.setColour (theme::window.withAlpha (0.75f));
        g.fillRect (juce::Rectangle<float> (face.getX(), 462.0f, face.getWidth(), face.getBottom() - 462.0f));
    }
    g.setColour (theme::border);
    g.drawHorizontalLine (462, face.getX() + 1.0f, face.getRight() - 1.0f);
    g.drawRoundedRectangle (face.reduced (0.5f), 18.0f, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (font (9.5f, true, 0.2f));
    g.drawText ("PRESET", 36, 474, 60, 14, juce::Justification::centredLeft, false);
    g.drawText ("IVO BEATZ", 384, 474, 100, 14, juce::Justification::centredRight, false);
}

void AlterIvoEditor::resized()
{
    pitch.setBounds (kPitchKnob);
    formant.setBounds (kFormantKnob);
    drive.setBounds (kDriveKnob);
    mix.setBounds (kMixKnob);
    modeSwitch.setBounds (kModeSwitch);
    linkSwitch.setBounds (kLinkSwitch);
    prevPreset.setBounds (94, 473, 16, 16);
    nextPreset.setBounds (118, 473, 16, 16);
    presetBox.setBounds (144, 468, 220, 26);
}
