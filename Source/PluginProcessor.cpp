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

// Patrones sincronizados al tempo: saltos de tono (semitonos) que cambian en cada paso
struct TempoPattern { double stepBeats; int length; int semis[4]; };
const TempoPattern kPatterns[] = {
    { 1.0,  1, { 0 } },                    // Off
    { 1.0,  2, { 0, 12 } },                // Octava arriba cada negra
    { 0.5,  4, { 0, 12, 0, -12 } },        // Octavas arriba/abajo en corcheas
    { 0.5,  2, { 0, 7 } },                 // Quinta en corcheas
    { 1.0,  2, { 0, -12 } },               // Octava abajo cada negra
    { 0.25, 4, { 0, 3, 7, 12 } },          // Escalera en semicorcheas
};
constexpr int kNumPatterns = (int) (sizeof (kPatterns) / sizeof (kPatterns[0]));
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

    // Tonalidad para Quantize y fuente (voz o mezcla)
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "key", 2 }, "Key", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "scale", 2 }, "Scale", StringArray { "Chromatic", "Major", "Minor" }, 0));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "source", 2 }, "Source", StringArray { "Voice", "Mix" }, 0));

    // Intensidad: cuánto del efecto (tono y formantes) se aplica, sin mezclar la señal seca
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "intensity", 3 }, "Intensity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float v, int) { return String (v, 0) + "%"; })));

    // Salida: ajuste final de nivel (el nivel igualado ya mantiene el volumen con 0 dB)
    // Patrón de tono sincronizado al tempo del DAW (sólo suena con el DAW en reproducción)
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "pattern", 4 }, "Tempo Pattern",
        StringArray { "Off", "Octave 1/4", "Octaves 1/8", "Fifth 1/8", "Sub octave 1/4", "Stairs 1/16" }, 0));

    // Comparación A/B a igual volumen: A = voz original (alineada), B = procesada
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "compare", 4 }, "A/B Original", false));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 3 }, "Output", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return signedValue (v, 1, " dB"); })));

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
    pKey     = apvts.getRawParameterValue ("key");
    pScale   = apvts.getRawParameterValue ("scale");
    pSource  = apvts.getRawParameterValue ("source");
    pIntensity = apvts.getRawParameterValue ("intensity");
    pOutput    = apvts.getRawParameterValue ("output");
    pPattern   = apvts.getRawParameterValue ("pattern");
    pCompare   = apvts.getRawParameterValue ("compare");
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

    const int numIn = juce::jlimit (1, 2, getTotalNumInputChannels());
    engine.prepare (sampleRate, numIn, static_cast<alterivo::Profile> (juce::jlimit (0, 1, latencyProfile.load())));
    const int engineLatency = engine.getLatencySamples();
    preparedProfile = latencyProfile.load();

    // Nivel igualado: la salida procesada suena tan fuerte como la entrada
    levelCoef = (float) (1.0 - std::exp (-1.0 / (0.4 * sampleRate)));
    levelIn = levelOut = 0.0f;
    levelGain.reset (sampleRate, 0.05);
    levelGain.setCurrentAndTargetValue (1.0f);
    sr = sampleRate;

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
    driveCurrent = pDrive->load() / 100.0f;
    abGain.reset (sampleRate, 0.03);
    abGain.setCurrentAndTargetValue (pCompare->load() > 0.5f ? 0.0f : 1.0f);
    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    driveStepPerSample = (float) (1.0 / (0.05 * sampleRate * 4.0));   // recorrido completo en 50 ms
    keyResetRequest.store (true);

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

    const auto engineMode = static_cast<alterivo::Mode> (juce::jlimit (0, 2, (int) std::lround (pMode->load())));
    const int keyRoot = juce::jlimit (0, 11, (int) std::lround (pKey->load()));
    const auto scale = static_cast<alterivo::Scale> (juce::jlimit (0, 2, (int) std::lround (pScale->load())));
    const bool voiceSource = pSource->load() < 0.5f;
    // Link: los formantes siguen al tono (en el motor, así vale para ratón, automatización,
    // MIDI y presets). Formant queda como ajuste adicional. Intensity escala todo el efecto.
    const float intensity = pIntensity->load() / 100.0f;
    const bool linked = pLink->load() > 0.5f;

    // Tempo del DAW para el patrón (posición en negras al principio del bloque)
    const int pattern = juce::jlimit (0, kNumPatterns - 1, (int) std::lround (pPattern->load()));
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    if (pattern > 0)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                playing = pos->getIsPlaying();
                if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
                if (auto b = pos->getBpm()) bpm = juce::jmax (1.0, *b);
            }
    const bool patternOn = pattern > 0 && playing;
    patternActive.store (patternOn);

    auto patternSemis = [&] (double atPpq)
    {
        if (! patternOn) return 0;
        const auto& pt = kPatterns[pattern];
        const auto step = (long long) std::floor (atPpq / pt.stepBeats + 1.0e-9);
        return pt.semis[(size_t) (((step % pt.length) + pt.length) % pt.length)];
    };

    // Ajusta el tono (con el paso del patrón), Link y la intensidad en el motor
    auto applyPitch = [&] (int patternOffset)
    {
        const float pitchParam = pPitch->load() + (float) patternOffset;
        const float pitchSemis = heldNote.load() >= 0 ? (float) (heldNote.load() - 60 + patternOffset) : pitchParam;
        const float formantSemis = juce::jlimit (-12.0f, 12.0f, pFormant->load() + (linked ? pitchSemis : 0.0f));
        engine.setPitchSemitones (pitchParam);
        engine.setFormantSemitones (formantSemis * intensity);
        return formantSemis;
    };
    const float formantSemis = applyPitch (patternSemis (ppq));
    engine.setIntensity (intensity);
    engine.setMode (engineMode);
    engine.setMidiNote (heldNote.load());
    engine.setScale (keyRoot, scale);
    engine.setVoiceSource (voiceSource);
    engine.setHighQuality (isNonRealtime());      // renderizado offline: máxima calidad, misma latencia

    const float driveAmount = pDrive->load() / 100.0f;
    driveTarget = driveAmount;
    driveGain.setTargetValue (driveAmount > 0.0005f || driveCurrent > 0.0005f ? 1.0f : 0.0f);
    mixSmoothed.setTargetValue (pMix->load() / 100.0f);

    // Con Pitch y Formant a 0 (Transpose, sin MIDI) la voz pasa intacta y en estéreo
    const bool engineNeeded = intensity > 0.0005f
                              && (engineMode != alterivo::Mode::Transpose
                                  || std::abs (pPitch->load()) > 0.005f
                                  || std::abs (formantSemis) > 0.005f
                                  || heldNote.load() >= 0
                                  || patternOn);
    abGain.setTargetValue (pCompare->load() > 0.5f ? 0.0f : 1.0f);
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    engineGain.setTargetValue (engineNeeded ? 1.0f : 0.0f);
    // Sin efecto audible el motor sólo analiza (pantalla y tonalidad): ahorra CPU
    engine.setActive (engineNeeded || engineGain.isSmoothing() || engineGain.getCurrentValue() > 0.0f);

    const int total = buffer.getNumSamples();
    const int numMeter = juce::jmin (numIn, buffer.getNumChannels());

    float pin = 0.0f;
    for (int ch = 0; ch < numMeter; ++ch)
        pin = juce::jmax (pin, buffer.getMagnitude (ch, 0, total));

    // Se procesa por tramos: cada cambio de paso del patrón cae en su muestra exacta
    const auto t0 = juce::Time::getHighResolutionTicks();
    const double samplesPerBeat = sr * 60.0 / bpm;
    for (int start = 0; start < total;)
    {
        int len = juce::jmin (maxBlock, total - start);
        if (patternOn)
        {
            const double here = ppq + start / samplesPerBeat;
            const double stepBeats = kPatterns[pattern].stepBeats;
            const double next = (std::floor (here / stepBeats + 1.0e-9) + 1.0) * stepBeats;
            const int toNext = juce::jmax (1, (int) std::ceil ((next - here) * samplesPerBeat - 1.0e-6));
            len = juce::jmin (len, toNext);
            applyPitch (patternSemis (here));
        }
        processChunk (buffer, start, len);
        start += len;
    }

    // Consumo de CPU por módulo (fracción del tiempo real del bloque, suavizado)
    {
        const double blockSecs = total / sr;
        const double secsTotal = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double eng = juce::Time::highResolutionTicksToSeconds (engineTicks), drv = juce::Time::highResolutionTicksToSeconds (driveTicks);
        engineTicks = driveTicks = 0;
        auto smooth = [] (std::atomic<float>& a, double v) { a.store (a.load() + 0.05f * ((float) v - a.load())); };
        if (blockSecs > 0.0)
        {
            smooth (cpuEngine, eng / blockSecs);
            smooth (cpuDrive, drv / blockSecs);
            smooth (cpuTotal, secsTotal / blockSecs);
        }
    }

    float pout = 0.0f;
    for (int ch = 0; ch < juce::jmin (numOut, buffer.getNumChannels()); ++ch)
        pout = juce::jmax (pout, buffer.getMagnitude (ch, 0, total));

    // Notas para el detector de tonalidad (sin bloqueos: valores atómicos, un solo escritor)
    {
        if (keyResetRequest.exchange (false))
        {
            for (auto& v : keyHist) v.store (0.0f, std::memory_order_relaxed);
            keySeconds.store (0.0f, std::memory_order_relaxed);
        }
        const float hz = engine.getDetectedHz();
        if (hz > 50.0f && pin > 0.003f)
        {
            const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
            const int n = (int) std::lround (note);
            if (std::abs (note - (float) n) < 0.35f)   // notas estables, no los glissandos ni el silencio
            {
                const float secs = (float) total / (float) sr;
                auto& slot = keyHist[(size_t) (((n % 12) + 12) % 12)];
                slot.store (slot.load (std::memory_order_relaxed) + secs, std::memory_order_relaxed);
                keySeconds.store (keySeconds.load (std::memory_order_relaxed) + secs, std::memory_order_relaxed);
            }
        }
    }

    if (pin > inPeak.load (std::memory_order_relaxed))   inPeak.store (pin, std::memory_order_relaxed);
    if (pout > outPeak.load (std::memory_order_relaxed)) outPeak.store (pout, std::memory_order_relaxed);
}

void AlterIvoProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len)
{
    const int numIn = juce::jmax (1, juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels()));
    const int numCh = juce::jmin (buffer.getNumChannels(), wetBuffer.getNumChannels());
    if (numCh <= 0)
        return;

    // 1) Pitch / formantes: un motor estéreo enlazado (mismo análisis para L y R),
    //    con el mismo nivel que la entrada, mezclado con la entrada intacta
    const int engineCh = juce::jmin (numIn, alterivo::kMaxChannels);
    const auto tEngine = juce::Time::getHighResolutionTicks();
    for (int i = 0; i < len; ++i)
    {
        float x[2] {}, processed[2] {}, intact[2] {};
        float eIn = 0.0f, eOut = 0.0f;
        for (int ch = 0; ch < engineCh; ++ch)
            x[ch] = buffer.getSample (ch, start + i);
        engine.process (x, processed);
        for (int ch = 0; ch < engineCh; ++ch)
        {
            intact[ch] = dryToEngine[(size_t) ch].process (x[ch]);
            eIn += intact[ch] * intact[ch];
            eOut += processed[ch] * processed[ch];
        }

        // Nivel igualado (lento, sólo cuando hay señal)
        levelIn  += levelCoef * (eIn - levelIn);
        levelOut += levelCoef * (eOut - levelOut);
        if ((i & 63) == 0)
        {
            if (levelIn > 1.0e-7f && levelOut > 1.0e-9f)
                levelGain.setTargetValue (juce::jlimit (0.5f, 2.0f, std::sqrt (levelIn / levelOut)));
        }
        const float lg = levelGain.getNextValue();
        const float g = engineGain.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            const int src = juce::jmin (ch, engineCh - 1);
            wetBuffer.setSample (ch, i, intact[src] + g * (processed[src] * lg - intact[src]));
        }
    }

    engineTicks += juce::Time::getHighResolutionTicks() - tEngine;

    // 2) Drive a 4x. Con Drive a 0 la señal no pasa por el sobremuestreo (solo se retrasa igual)
    const auto tDrive = juce::Time::getHighResolutionTicks();
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
        // La cantidad de Drive se desliza hacia su valor (sin escalones al automatizar)
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            if ((i & 31) == 0)
            {
                if (driveCurrent < driveTarget) driveCurrent = juce::jmin (driveTarget, driveCurrent + driveStepPerSample * 32.0f);
                else if (driveCurrent > driveTarget) driveCurrent = juce::jmax (driveTarget, driveCurrent - driveStepPerSample * 32.0f);
                drive.setAmount (driveCurrent);
            }
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                d[i] = drive.process (d[i]);
            }
        }
        oversampler->processSamplesDown (block);
    }
    else
    {
        oversamplerActive = false;
        driveCurrent = driveTarget;
    }

    driveTicks += juce::Time::getHighResolutionTicks() - tDrive;

    // 3) Mezcla seco / procesado (y A/B: A = original alineado, al mismo nivel de salida)
    for (int i = 0; i < len; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float og = outputGain.getNextValue();
        const float ab = abGain.getNextValue();
        const float dg = driveOn ? driveGain.getNextValue() : 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = dryTotal[(size_t) ch].process (buffer.getSample (juce::jmin (ch, numIn - 1), start + i));
            const float clean = cleanBuffer.getSample (ch, i);
            float wet = clean;
            if (driveOn)
                wet = clean + dg * (dcBlockers[(size_t) ch].process (wetBuffer.getSample (ch, i)) - clean);

            const float processed = dry + mix * (wet - dry);
            buffer.setSample (ch, start + i, (dry + ab * (processed - dry)) * og);
        }
    }
}

// ------------------------------------------------------------- tonalidad
// Análisis progresivo: empieza al pedirlo, acumula notas estables de L+R, evalúa cada
// segundo y sólo acepta cuando hay audio suficiente, la ventaja sobre la segunda opción
// es clara y el resultado se repite. Una vez aceptado no cambia solo.
namespace
{
const float kMajorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
const float kMinorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

float keyCorrelation (const std::array<float, 12>& h, const float* prof, int r)
{
    float mx = 0, mp = 0;
    for (int i = 0; i < 12; ++i) { mx += h[(size_t) ((i + r) % 12)]; mp += prof[i]; }
    mx /= 12.0f; mp /= 12.0f;
    float num = 0, dx = 0, dp = 0;
    for (int i = 0; i < 12; ++i)
    {
        const float a = h[(size_t) ((i + r) % 12)] - mx, b = prof[i] - mp;
        num += a * b; dx += a * a; dp += b * b;
    }
    return dx > 0 && dp > 0 ? num / std::sqrt (dx * dp) : -1.0f;
}
} // namespace

void AlterIvoProcessor::startKeyAnalysis()
{
    keyResetRequest.store (true);
    keyStatus = {};
    keyStatus.state = KeyStatus::Analyzing;
    keyEvalCount = 0;
    keyLastSeconds = 0.0f;
    keyLastCandidate = -1;
}

void AlterIvoProcessor::cancelKeyAnalysis() { keyStatus.state = KeyStatus::Idle; }

const AlterIvoProcessor::KeyStatus& AlterIvoProcessor::updateKeyAnalysis()
{
    if (keyStatus.state != KeyStatus::Analyzing)
        return keyStatus;

    const float secs = keySeconds.load (std::memory_order_relaxed);
    keyStatus.progress = juce::jlimit (0.0f, 1.0f, secs / kKeyMinSeconds);
    if (secs - keyLastSeconds < 1.0f)       // una evaluación por cada segundo de notas nuevas
        return keyStatus;
    keyLastSeconds = secs;

    std::array<float, 12> h {};
    for (int i = 0; i < 12; ++i) h[(size_t) i] = keyHist[(size_t) i].load (std::memory_order_relaxed);

    float best = -2.0f, second = -2.0f;
    int bestKey = 0;                         // root + 12 * (escala - 1)
    for (int r = 0; r < 12; ++r)
        for (int sc = 1; sc <= 2; ++sc)
        {
            const float c = keyCorrelation (h, sc == 1 ? kMajorProfile : kMinorProfile, r);
            if (c > best) { second = best; best = c; bestKey = r + 12 * (sc - 1); }
            else if (c > second) second = c;
        }

    const float margin = best - second;
    keyStatus.confidence = juce::jlimit (0.0f, 1.0f, margin / 0.12f);
    keyStatus.root = bestKey % 12;
    keyStatus.scale = bestKey / 12 + 1;

    keyEvalCount = bestKey == keyLastCandidate ? keyEvalCount + 1 : 1;
    keyLastCandidate = bestKey;

    const bool enough = secs >= kKeyMinSeconds && keyEvalCount >= 3 && margin >= 0.03f;
    const bool timeout = secs >= kKeyMaxSeconds && keyEvalCount >= 2;
    if (enough || timeout)
    {
        keyStatus.state = KeyStatus::Done;
        keyStatus.progress = 1.0f;
        auto set = [this] (const char* id, int index)
        {
            if (auto* q = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            {
                q->beginChangeGesture();
                *q = index;
                q->endChangeGesture();
            }
        };
        set ("key", keyStatus.root);
        set ("scale", keyStatus.scale);
    }
    return keyStatus;
}

void AlterIvoProcessor::setLatencyProfile (int profile)
{
    // HIGH QUALITY se descartó: en las pruebas afinaba peor que NORMAL (80 % frente a 93 %)
    profile = juce::jlimit (0, 1, profile);
    if (profile == latencyProfile.load())
        return;
    latencyProfile.store (profile);
    // Se vuelve a preparar fuera del audio: el DAW lee la latencia nueva y la compensa
    if (getSampleRate() > 0.0)
    {
        suspendProcessing (true);
        prepareToPlay (getSampleRate(), juce::jmax (32, getBlockSize()));
        suspendProcessing (false);
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
    set ("intensity", 100.0f);
    set ("pattern", 0.0f);
    set ("output", 0.0f);
    apvts.state.setProperty ("preset", index, nullptr);
}

// ---------------------------------------------------------------- estado
void AlterIvoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("latencyProfile", latencyProfile.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AlterIvoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, kNumPresets - 1, (int) apvts.state.getProperty ("preset", 0));
            uiScale = juce::jlimit (0.75f, 2.0f, (float) apvts.state.getProperty ("uiScale", 1.0f));
            setLatencyProfile ((int) apvts.state.getProperty ("latencyProfile", 1));
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
