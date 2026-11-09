#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String signedValue (float v, int decimals, const juce::String& suffix)
{
    const auto text = juce::String (std::abs (v), decimals);
    const auto sign = v > 0.0005f ? "+" : (v < -0.0005f ? "-" : "");
    return sign + text + suffix;
}
struct FactoryPreset
{
    const char* name;
    float pitch, formant, drive, mix;
    int mode;      // 0 Transpose, 1 Quantize, 2 Robot
    bool link;
};

const FactoryPreset kPresets[] = {
    { "Por defecto",         0.0f,  0.0f,  0.0f, 100.0f, 0, false },
    { "Doble natural",       0.1f, -0.5f,  8.0f,  40.0f, 0, false },
    { "Hard Tune",           0.0f,  0.0f, 10.0f, 100.0f, 1, false },
    { "Hard Tune saturado",  0.0f,  0.0f, 55.0f, 100.0f, 1, false },
    { "Armonía 5ª arriba",   7.0f,  1.0f,  0.0f,  45.0f, 0, false },
    { "Armonía 3ª arriba",   4.0f,  0.5f,  0.0f,  45.0f, 0, false },
    { "Octava abajo",      -12.0f, -2.0f, 12.0f,  50.0f, 0, false },
    { "Octava arriba",      12.0f,  2.0f,  0.0f,  40.0f, 0, false },
    { "Voz más grave",      -2.0f, -2.5f,  0.0f, 100.0f, 0, false },
    { "Voz más aguda",       2.0f,  2.0f,  0.0f, 100.0f, 0, false },
    { "Ardilla",             7.0f,  0.0f,  0.0f, 100.0f, 0, true  },
    { "Gigante",            -7.0f,  0.0f, 20.0f, 100.0f, 0, true  },
    { "Robot",               0.0f,  0.0f, 25.0f, 100.0f, 2, false },
    { "Robot grave",       -12.0f, -3.0f, 35.0f, 100.0f, 2, false },
    { "Radio rota",          0.0f,  4.0f, 85.0f, 100.0f, 1, false },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout AlterIvoProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "pitch", 1 }, "Pitch", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (
            [] (float v, int) { return signedValue (v, 1, " st"); })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "formant", 1 }, "Formant", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return signedValue (v, 1, ""); })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float v, int) { return String (v, 0) + "%"; })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float v, int) { return String (v, 0) + "%"; })));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode", StringArray { "Transpose", "Quantize", "Robot" }, 0));

    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "link", 1 }, "Link", false));

    return { params.begin(), params.end() };
}

AlterIvoProcessor::AlterIvoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ALTERIVO", createLayout())
{
    pPitch   = apvts.getRawParameterValue ("pitch");
    pFormant = apvts.getRawParameterValue ("formant");
    pDrive   = apvts.getRawParameterValue ("drive");
    pMix     = apvts.getRawParameterValue ("mix");
    pMode    = apvts.getRawParameterValue ("mode");
    pLink    = apvts.getRawParameterValue ("link");
}

bool AlterIvoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void AlterIvoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (32, samplesPerBlock);
    const int numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());

    engine.prepare (sampleRate);
    const int engineLatency = engine.getLatencySamples();

    // 4x de sobremuestreo para la saturación (latencia entera para alinear el resto)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    oversampler->reset();
    oversamplerActive = false;

    const int osLatency = (int) std::lround (oversampler->getLatencyInSamples());
    setLatencySamples (engineLatency + osLatency);

    wetBuffer.setSize (numCh, maxBlock, false, true, true);
    cleanBuffer.setSize (numCh, maxBlock, false, true, true);

    for (int ch = 0; ch < 2; ++ch)
    {
        dryToEngine[(size_t) ch].setSize (engineLatency);
        cleanToDrive[(size_t) ch].setSize (osLatency);
        dryTotal[(size_t) ch].setSize (engineLatency + osLatency);
    }

    for (auto& dc : dcBlockers)
        dc.prepare (sampleRate);

    mixSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (pMix->load() / 100.0f);
    engineGain.reset (sampleRate, 0.03);
    engineGain.setCurrentAndTargetValue (0.0f);
    driveGain.reset (sampleRate, 0.03);
    driveGain.setCurrentAndTargetValue (0.0f);

    noteCount = 0;
    heldNote.store (-1);
}

// ------------------------------------------------------------------ MIDI
void AlterIvoProcessor::noteOn (int note)
{
    noteOff (note);
    if (noteCount < (int) noteStack.size())
        noteStack[(size_t) noteCount++] = note;
}

void AlterIvoProcessor::noteOff (int note)
{
    for (int i = 0; i < noteCount; ++i)
    {
        if (noteStack[(size_t) i] == note)
        {
            for (int j = i; j < noteCount - 1; ++j)
                noteStack[(size_t) j] = noteStack[(size_t) (j + 1)];
            --noteCount;
            return;
        }
    }
}

void AlterIvoProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            noteOn (msg.getNoteNumber());
        else if (msg.isNoteOff())
            noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            noteCount = 0;
    }
    heldNote.store (noteCount > 0 ? noteStack[(size_t) (noteCount - 1)] : -1);
}

// ------------------------------------------------------------- procesado
void AlterIvoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    handleMidi (midi);

    engine.setPitchSemitones (pPitch->load());
    engine.setFormantSemitones (pFormant->load());
    engine.setLink (pLink->load() > 0.5f);
    const auto engineMode = static_cast<alterivo::Mode> (juce::jlimit (0, 2, (int) std::lround (pMode->load())));
    engine.setMode (engineMode);
    engine.setMidiNote (heldNote.load());

    const float driveAmount = pDrive->load() / 100.0f;
    drive.setAmount (driveAmount);
    driveGain.setTargetValue (driveAmount > 0.0005f ? 1.0f : 0.0f);
    mixSmoothed.setTargetValue (pMix->load() / 100.0f);

    // Con Pitch y Formant a 0 (Transpose, sin MIDI) la voz pasa intacta y en estéreo
    const bool engineNeeded = engineMode != alterivo::Mode::Transpose
                              || std::abs (pPitch->load()) > 0.005f
                              || std::abs (pFormant->load()) > 0.005f
                              || heldNote.load() >= 0;
    engineGain.setTargetValue (engineNeeded ? 1.0f : 0.0f);

    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, total - start));
}

void AlterIvoProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len)
{
    const int numIn = juce::jmax (1, juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels()));
    const int numCh = juce::jmin (buffer.getNumChannels(), wetBuffer.getNumChannels());
    if (numCh <= 0)
        return;

    // 1) Pitch / formantes (mono, como el original) mezclado con la entrada intacta
    const float inScale = 1.0f / (float) numIn;
    for (int i = 0; i < len; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numIn; ++ch)
            mono += buffer.getSample (ch, start + i);

        const float processed = engine.process (mono * inScale);
        const float g = engineGain.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float intact = dryToEngine[(size_t) ch].process (buffer.getSample (juce::jmin (ch, numIn - 1), start + i));
            wetBuffer.setSample (ch, i, intact + g * (processed - intact));
        }
    }

    // 2) Drive a 4x. Con Drive a 0 la señal no pasa por el sobremuestreo (solo se retrasa igual)
    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < len; ++i)
            cleanBuffer.setSample (ch, i, cleanToDrive[(size_t) ch].process (wetBuffer.getSample (ch, i)));

    const bool driveOn = driveGain.isSmoothing() || driveGain.getTargetValue() > 0.0f;
    if (driveOn)
    {
        if (! oversamplerActive)
        {
            oversampler->reset();
            for (auto& dc : dcBlockers) dc.reset();
            oversamplerActive = true;
        }

        juce::dsp::AudioBlock<float> block (wetBuffer.getArrayOfWritePointers(), (size_t) numCh, 0, (size_t) len);
        auto up = oversampler->processSamplesUp (block);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = drive.process (d[i]);
        }
        oversampler->processSamplesDown (block);
    }
    else
    {
        oversamplerActive = false;
    }

    // 3) Mezcla seco / procesado
    for (int i = 0; i < len; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float dg = driveOn ? driveGain.getNextValue() : 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = dryTotal[(size_t) ch].process (buffer.getSample (juce::jmin (ch, numIn - 1), start + i));
            const float clean = cleanBuffer.getSample (ch, i);
            float wet = clean;
            if (driveOn)
                wet = clean + dg * (dcBlockers[(size_t) ch].process (wetBuffer.getSample (ch, i)) - clean);

            buffer.setSample (ch, start + i, dry + mix * (wet - dry));
        }
    }
}

// --------------------------------------------------------------- presets
int AlterIvoProcessor::getNumPrograms() { return kNumPresets; }

const juce::String AlterIvoProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumPresets) ? juce::String (juce::CharPointer_UTF8 (kPresets[index].name))
                                                         : juce::String();
}

void AlterIvoProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumPresets))
        return;

    currentPreset = index;
    const auto& p = kPresets[index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    set ("pitch", p.pitch);
    set ("formant", p.formant);
    set ("drive", p.drive);
    set ("mix", p.mix);
    set ("mode", (float) p.mode);
    set ("link", p.link ? 1.0f : 0.0f);
    apvts.state.setProperty ("preset", index, nullptr);
}

// ---------------------------------------------------------------- estado
void AlterIvoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AlterIvoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, kNumPresets - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessorEditor* AlterIvoProcessor::createEditor()
{
    return new AlterIvoEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AlterIvoProcessor();
}
