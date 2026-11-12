#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace chainivo;

namespace
{
const juce::Identifier kMode ("mode"), kVoice ("voiceType"), kDetected ("detectedType"), kGenre ("genre"), kPreset ("preset"),
                       kOrder ("order"), kLocks ("locks"), kQuality ("quality"), kAutoDone ("autoApplied"), kAb ("abSlot"),
                       kScale ("uiScale"), kVersion ("version"), kBuilt ("enginesBuilt");
constexpr int kStateVersion = 1;
juce::String whyId (int i) { return "why" + juce::String (i); }
} // namespace

ChainIvoProcessor::ChainIvoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Voz principal", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "chainIVO", pid::createLayout())
{
    for (int i = 0; i < 6; ++i) order[(size_t) i] = i;
    auto& s = apvts.state;
    s.setProperty (kMode, 0, nullptr); s.setProperty (kVoice, -1, nullptr); s.setProperty (kDetected, -1, nullptr);
    s.setProperty (kGenre, -1, nullptr); s.setProperty (kPreset, juce::String::fromUTF8 ("Propuesta autom\xc3\xa1tica"), nullptr);
    s.setProperty (kOrder, "0,1,2,3,4,5", nullptr); s.setProperty (kLocks, "000000", nullptr);
    s.setProperty (kQuality, (int) Quality::normal, nullptr); s.setProperty (kAutoDone, false, nullptr);
    s.setProperty (kVersion, kStateVersion, nullptr);
    groupId = group->join();
    startTimerHz (3);
}

ChainIvoProcessor::~ChainIvoProcessor()
{
    stopTimer(); instCancel = true; group->leave (groupId);
    for (int i = 0; i < 200 && (instBusy.load() || exporting.load()); ++i) juce::Thread::sleep (20);
    analyzer.releaseResources();
}

bool ChainIvoProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void ChainIvoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    maxBlock = std::max (1, samplesPerBlock);
    const int nch = std::max (1, getMainBusNumInputChannels());
    const auto q = (Quality) quality.load();
    eq.prepare (sampleRate);
    comp.prepare (sampleRate);
    harm.prepare (sampleRate, maxBlock, nch, q);
    vocal.prepare (sampleRate);
    space.prepare (sampleRate, maxBlock, q);
    creative.prepare (sampleRate);
    analyzer.prepare (sampleRate);
    sideMono.assign ((size_t) maxBlock, 0.0f);
    dryBuf.setSize (2, maxBlock);
    const int lat = harm.getLatency();
    for (auto& d : dryDelay) d.prepare (lat + 8);
    for (auto* s : { &inGainS, &outGainS, &mixS, &bypassS }) s->prepare (sampleRate, 20.0f);
    matchS.prepare (sampleRate, 400.0f);
    inGainS.reset (dbToGain (p (pid::input))); outGainS.reset (dbToGain (p (pid::output)));
    mixS.reset (p (pid::mix) * 0.01f); bypassS.reset (p (pid::bypass) > 0.5f ? 1.0f : 0.0f); matchS.reset (1.0f);
    // Ponderación K aproximada (ITU-R BS.1770): paso alto 60 Hz + estante +4 dB en agudos
    for (auto* b : { &kwDryHp, &kwWetHp }) { b->setup (sampleRate, EqBand::highPass, 60.0f, 0.5f, 0.0f); b->reset(); }
    for (auto* b : { &kwDryHs, &kwWetHs }) { b->setup (sampleRate, EqBand::highShelf, 1500.0f, 0.7071f, 4.0f); b->reset(); }
    dryPow = wetPow = 0.0; matchTarget = 0.0f;
    setLatencySamples (lat);
}

void ChainIvoProcessor::releaseResources() {}

void ChainIvoProcessor::rebuildForQuality()
{
    // Cambiar la calidad cambia el sobremuestreo (y por tanto la latencia): se prepara de nuevo fuera del audio
    suspendProcessing (true);
    prepareToPlay (currentSr, maxBlock);
    suspendProcessing (false);
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withLatencyChanged (true));
}

void ChainIvoProcessor::setQuality (int q)
{
    q = juce::jlimit (0, 3, q);
    if (q == quality.load()) return;
    quality = q;
    apvts.state.setProperty (kQuality, q, nullptr);
    rebuildForQuality();
}

// ------------------------------------------------------------------ Parámetros -> motores
void ChainIvoProcessor::readSettings (EqSettings& e, CompSettings& c, HarmSettings& h, VocalSettings& v,
                                      SpaceSettings& s, CreativeSettings& cr, float I) const
{
    // INTENSITY lleva cada motor de neutro (0 %) a lo ajustado (100 %), dentro del propio motor
    e.on = p (pid::eqOn) > 0.5f;
    e.hpf = p (pid::hpf); e.hpfAmount = I; e.hpf24 = p (pid::hpfSlope) > 0.5f;
    e.lowFreq = p (pid::lowFreq); e.lowGain = p (pid::lowGain) * I;
    e.midFreq = p (pid::midFreq); e.midGain = p (pid::midGain) * I; e.midQ = p (pid::midQ);
    e.presFreq = p (pid::presFreq); e.presGain = p (pid::presGain) * I; e.presQ = p (pid::presQ); e.presDyn = p (pid::presDyn) * 0.01f;
    e.airFreq = p (pid::airFreq); e.airGain = p (pid::airGain) * I;

    c.on = p (pid::compOn) > 0.5f; c.type = (CompType) (int) p (pid::compType);
    c.threshold = p (pid::compThresh); c.ratio = 1.0f + (p (pid::compRatio) - 1.0f) * I;
    c.attackMs = p (pid::compAttack); c.releaseMs = p (pid::compRelease); c.makeup = p (pid::compMakeup) * I; c.mix = p (pid::compMix) * 0.01f;

    h.on = p (pid::harmOn) > 0.5f; h.type = (HarmType) (int) p (pid::harmType);
    h.drive = p (pid::harmDrive) * 0.01f; h.tone = p (pid::harmTone) * 0.01f; h.mix = p (pid::harmMix) * 0.01f * I;

    v.on = p (pid::vocOn) > 0.5f; v.deess = p (pid::deess) * 0.01f * I; v.deessFreq = p (pid::deessFreq);
    v.harsh = p (pid::harsh) * 0.01f * I; v.breath = p (pid::breath) * 0.01f * I;

    s.on = p (pid::spaceOn) > 0.5f; s.revType = (RevType) (int) p (pid::revType);
    s.revMix = p (pid::revMix) * 0.01f * I; s.decay = p (pid::revDecay); s.predelay = p (pid::revPredelay); s.tone = p (pid::revTone) * 0.01f;
    s.dlyType = (DlyType) (int) p (pid::dlyType); s.dlyNote = (int) p (pid::dlyNote); s.dlyFeedback = p (pid::dlyFeedback) * 0.01f;
    s.dlyMix = p (pid::dlyMix) * 0.01f * I; s.duck = p (pid::duck) * 0.01f;
    s.width = 1.0f + (p (pid::width) * 0.01f - 1.0f) * I; s.doubler = p (pid::doubler) * 0.01f * I;
    s.throwAmt = p (pid::dlyThrow) * 0.01f * I; s.swell = p (pid::revSwell) * 0.01f * I;

    cr.on = p (pid::creaOn) > 0.5f; cr.type = (CreaType) (int) p (pid::creaType); cr.note = (int) p (pid::creaNote);
    cr.amount = p (pid::creaAmount) * 0.01f * I; cr.pitch = p (pid::creaPitch);
}

EqSettings ChainIvoProcessor::eqSettingsNow() const
{
    EqSettings e; CompSettings c; HarmSettings h; VocalSettings v; SpaceSettings s; CreativeSettings cr;
    readSettings (e, c, h, v, s, cr, p (pid::intensity) * 0.01f);
    return e;
}

// ------------------------------------------------------------------ Audio
void ChainIvoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    const int nch = std::min (2, getMainBusNumInputChannels());
    if (n == 0 || nch == 0) return;
    if (n > maxBlock)   // el host no debería pasar de lo anunciado; si lo hace, se procesa por trozos
    {
        for (int start = 0; start < n; start += maxBlock)
        {
            const int len = std::min (maxBlock, n - start);
            juce::AudioBuffer<float> part (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
            juce::MidiBuffer m;
            processBlock (part, m);
        }
        return;
    }
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    // Voz principal (sidechain) en mono
    const float* side = nullptr;
    auto scBus = getBusBuffer (buffer, true, 1);
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled() && scBus.getNumChannels() > 0)
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < scBus.getNumChannels(); ++c) v += scBus.getSample (c, i);
            v /= (float) scBus.getNumChannels();
            if (! std::isfinite (v)) v = 0.0f;
            sideMono[(size_t) i] = v; pk = std::max (pk, std::abs (v));
        }
        side = sideMono.data();
        if (pk > 1.0e-5f) leadConnected = true;
    }

    auto main = getBusBuffer (buffer, true, 0);
    float* ch[2] = { main.getWritePointer (0), main.getWritePointer (nch > 1 ? 1 : 0) };
    // Entrada saneada: un valor no válido o desmesurado que mande el host no entra en la cadena
    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (ch[c][i])) ch[c][i] = 0.0f;
            else ch[c][i] = juce::jlimit (-32.0f, 32.0f, ch[c][i]);
    inMeter.feed (ch, nch, n);

    // Entrada
    inGainS.set (dbToGain (p (pid::input)));
    for (int i = 0; i < n; ++i) { const float g = inGainS.next(); for (int c = 0; c < nch; ++c) ch[c][i] *= g; }

    // Seca alineada con la latencia de la cadena (para Dry/Wet, Gain Match y Bypass)
    for (int c = 0; c < nch; ++c)
    {
        float* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i) { dryDelay[(size_t) c].push (ch[c][i]); d[i] = dryDelay[(size_t) c].tap (harm.getLatency()); }
    }
    analyzer.push (ch[0], ch[nch > 1 ? 1 : 0], side, n);

    // Tempo
    TempoInfo tempo;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) tempo.bpm = *b;
            if (auto q = pos->getPpqPosition()) tempo.ppq = *q;
            tempo.playing = pos->getIsPlaying();
        }
    lastBpm.store (tempo.bpm);

    // Cadena en el orden elegido
    const float I = p (pid::intensity) * 0.01f;
    EqSettings es; CompSettings cs; HarmSettings hs; VocalSettings vs; SpaceSettings ss; CreativeSettings crs;
    readSettings (es, cs, hs, vs, ss, crs, I);
    // Escucha por módulo: los demás motores se saltan (HARMONICS pasa en seco para no cambiar la latencia)
    const int solo = soloEngine.load();
    if (solo >= 0)
    {
        if (solo != 0) es.on = false;
        if (solo != 1) cs.on = false;
        if (solo != 2) hs.on = false;
        if (solo != 3) vs.on = false;
        if (solo != 4) ss.on = false;
        if (solo != 5) crs.on = false;
    }
    for (int slot = 0; slot < 6; ++slot)
    {
        switch (order[(size_t) slot].load())
        {
            case 0: eq.process (ch, nch, n, es, side); break;
            case 1: comp.process (ch, nch, n, cs); break;
            case 2: harm.process (ch, n, hs); break;
            case 3: vocal.process (ch, nch, n, vs); break;
            case 4: space.process (ch, nch, n, ss, tempo); break;
            case 5: creative.process (ch, nch, n, crs, tempo); break;
            default: break;
        }
    }

    // Protección: si algún motor diera un valor no válido, se reinician los motores y el bloque sale seco
    {
        bool bad = false;
        for (int c = 0; c < nch && ! bad; ++c) for (int i = 0; i < n; ++i) if (! std::isfinite (ch[c][i])) { bad = true; break; }
        if (bad)
        {
            eq.reset(); comp.reset(); harm.reset(); vocal.reset(); space.reset(); creative.reset();
            for (int c = 0; c < nch; ++c) juce::FloatVectorOperations::copy (ch[c], dryBuf.getReadPointer (c), n);
            dspErrors.store (dspErrors.load() + 1);
        }
    }

    // Dry/Wet, Gain Match, Bypass, salida y protección
    mixS.set (p (pid::mix) * 0.01f);
    outGainS.set (dbToGain (p (pid::output)));
    bypassS.set (p (pid::bypass) > 0.5f ? 1.0f : 0.0f);
    const bool match = p (pid::gainMatch) > 0.5f;
    const bool clipSafe = p (pid::clipSafe) > 0.5f;
    const float tau = (float) std::exp (-1.0 / (1.5 * currentSr));
    int clips = 0;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.next();
        float dm = 0.0f, wm = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            const float d = dryBuf.getSample (c, i);
            ch[c][i] = d + m * (ch[c][i] - d);
            dm += d; wm += ch[c][i];
        }
        // Sonoridad (ponderación K aproximada) de la seca y de lo procesado
        const float kd = kwDryHs.tick (0, kwDryHp.tick (0, dm / (float) nch));
        const float kw = kwWetHs.tick (0, kwWetHp.tick (0, wm / (float) nch));
        dryPow = kd * kd + tau * (dryPow - kd * kd);
        wetPow = kw * kw + tau * (wetPow - kw * kw);
        if ((i & 63) == 0)
        {
            if (match && dryPow > 1.0e-6)   // sólo con señal (por encima de -60 dB): en silencio se congela
                matchTarget = juce::jlimit (-12.0f, 12.0f, powToDb ((float) dryPow) - powToDb ((float) wetPow));
            else if (! match) matchTarget = 0.0f;
            matchS.set (dbToGain (matchTarget));
        }
        const float g = matchS.next() * outGainS.next();
        const float bp = bypassS.next();
        for (int c = 0; c < nch; ++c)
        {
            float y = ch[c][i] * g;
            y = y + bp * (dryBuf.getSample (c, i) - y);
            if (clipSafe && std::abs (y) > 0.891f)   // -1 dBFS: rodilla suave hasta 0 dBFS (sólo protección)
            {
                const float s = y > 0 ? 1.0f : -1.0f, a = std::abs (y);
                y = s * (0.891f + 0.109f * std::tanh ((a - 0.891f) / 0.109f));
                ++clips;
            }
            ch[c][i] = y;
        }
    }
    gainMatchDb.store (matchTarget);
    if (clips > 0) clipCount += clips;
    if (nch == 1 && main.getNumChannels() > 1) main.copyFrom (1, 0, main, 0, 0, n);
    outMeter.feed (ch, nch, n);
    if (nch > 1)
    {
        double lr = 0, ll = 0, rr = 0;
        for (int i = 0; i < n; ++i) { lr += (double) ch[0][i] * ch[1][i]; ll += (double) ch[0][i] * ch[0][i]; rr += (double) ch[1][i] * ch[1][i]; }
        if (ll > 1.0e-7 && rr > 1.0e-7) correlation.store (0.95f * correlation.load() + 0.05f * (float) (lr / std::sqrt (ll * rr)));
    }

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (secs / ((double) n / currentSr));
    cpuLoad.store (0.9f * cpuLoad.load() + 0.1f * load);
}

// ------------------------------------------------------------------ AUTO / PRO
bool ChainIvoProcessor::isPro() const { return (int) apvts.state.getProperty (kMode, 0) == 1; }
void ChainIvoProcessor::setPro (bool pro) { apvts.state.setProperty (kMode, pro ? 1 : 0, nullptr); }
bool ChainIvoProcessor::isVoiceTypeAuto() const { return (int) apvts.state.getProperty (kVoice, -1) < 0; }
int ChainIvoProcessor::getVoiceType() const
{
    const int v = apvts.state.getProperty (kVoice, -1);
    if (v >= 0) return v;
    const int d = apvts.state.getProperty (kDetected, -1);
    return d >= 0 ? d : (int) lastClass.type;
}
void ChainIvoProcessor::setVoiceType (int t)
{
    apvts.state.setProperty (kVoice, t, nullptr);
    if (! isPro()) autoPropose();
}
int ChainIvoProcessor::getGenre() const { return apvts.state.getProperty (kGenre, -1); }
void ChainIvoProcessor::setGenre (int g)
{
    apvts.state.setProperty (kGenre, g, nullptr);
    if (! isPro()) autoPropose();
}
juce::String ChainIvoProcessor::getPresetName() const { return apvts.state.getProperty (kPreset).toString(); }

void ChainIvoProcessor::applyBuild (const BuildResult& b, const juce::String& presetName)
{
    undo.beginNewTransaction ("chainIVO AUTO");
    const int type = getVoiceType();
    const bool useProfile = profile.enabled();
    bool profiled = false;
    lastAuto.clear(); learnedThisBuild.clear(); stableTicks.clear(); lastSeenVal.clear();
    for (auto [id, value] : b.values)
    {
        if (isLocked (pid::engineOf (id))) continue;   // lo bloqueado por el usuario no se toca
        if (useProfile && UserProfile::learnable().contains (id))
        {
            const float off = profile.offset (type, id);
            if (std::abs (off) >= UserProfile::threshold (id) * 0.5f) { value += off; profiled = true; }
        }
        if (auto* prm = apvts.getParameter (id)) value = prm->convertFrom0to1 (prm->convertTo0to1 (value));
        lastAuto[id] = value;
        if (auto* prm = apvts.getParameter (id))
        {
            const float norm = prm->convertTo0to1 (value);
            prm->beginChangeGesture(); prm->setValueNotifyingHost (norm); prm->endChangeGesture();
        }
    }
    for (int i = 0; i < 6; ++i)
    {
        explanations[(size_t) i] = b.why[(size_t) i] + (profiled && i == 0 ? juce::String::fromUTF8 (" Ajustado con tu perfil personal.") : juce::String());
        apvts.state.setProperty (whyId (i), explanations[(size_t) i], nullptr);
    }
    enginesBuilt = b.enginesOn;
    apvts.state.setProperty (kBuilt, enginesBuilt, nullptr);
    apvts.state.setProperty (kPreset, presetName, nullptr);
    // Si aún no hay análisis suficiente, AUTO volverá a adaptar esta receta cuando lo haya
    apvts.state.setProperty (kAutoDone, lastAnalysis.ready, nullptr);
}

void ChainIvoProcessor::loadFactoryPreset (int i)
{
    fillGroup (lastAnalysis);
    lastAnalysis.hasInstrumental = instName.isNotEmpty(); lastAnalysis.instrumentalSpectrum = instSpectrum;
    const auto& list = presets.recipes();
    if (i < 0 || i >= (int) list.size()) return;
    auto r = list[(size_t) i];
    const int g = r.genre >= 0 ? r.genre : getGenre();
    if (r.genre >= 0) apvts.state.setProperty (kGenre, r.genre, nullptr);
    apvts.state.setProperty (kVoice, (int) r.type, nullptr);
    applyBuild (buildChain (r, lastAnalysis, g), r.name);
}

void ChainIvoProcessor::autoPropose()
{
    fillGroup (lastAnalysis);
    lastAnalysis.hasInstrumental = instName.isNotEmpty(); lastAnalysis.instrumentalSpectrum = instSpectrum;
    const auto r = defaultRecipe ((VoiceType) juce::jlimit (0, (int) VoiceType::count - 1, getVoiceType()));
    applyBuild (buildChain (r, lastAnalysis, getGenre()), juce::String::fromUTF8 ("Propuesta autom\xc3\xa1tica"));
}

void ChainIvoProcessor::loadUserPreset (const juce::File& f)
{
    auto tree = PresetLibrary::loadUser (f);
    if (! tree.isValid()) return;
    undo.beginNewTransaction ("chainIVO preset");
    restoreSnapshot (tree);
    apvts.state.setProperty (kPreset, f.getFileNameWithoutExtension(), nullptr);
}

bool ChainIvoProcessor::saveUserPreset (const juce::String& name)
{
    auto snap = engineSnapshot();
    snap.setProperty ("genre", getGenre(), nullptr);
    snap.setProperty ("voiceType", getVoiceType(), nullptr);
    const bool ok = presets.saveUser (name, snap, "Usuario");
    if (ok) apvts.state.setProperty (kPreset, name, nullptr);
    return ok;
}

void ChainIvoProcessor::reanalyse()
{
    analyzer.restart();
    lastAnalysis = {};
    apvts.state.setProperty (kAutoDone, false, nullptr);
    apvts.state.setProperty (kDetected, -1, nullptr);
}

void ChainIvoProcessor::loadInstrumental (const juce::File& f)
{
    if (instBusy.exchange (true)) return;
    instCancel = false;
    juce::Thread::launch ([this, f]
    {
        std::array<float, kBands> spec {};
        if (Analyzer::spectrumOfFile (f, spec, instCancel))
        {
            const juce::ScopedLock sl (instLock);
            instPending = spec; instPendingName = f.getFileNameWithoutExtension(); instReady = true;
        }
        instBusy = false;
    });
}

void ChainIvoProcessor::clearInstrumental()
{
    instName = {}; apvts.state.removeProperty ("instName", nullptr); apvts.state.removeProperty ("instSpec", nullptr);
    if (! isPro()) autoPropose();
}

juce::String ChainIvoProcessor::getInstrumentalName() const { return instName; }

void ChainIvoProcessor::loadIR (const juce::File& f)
{
    if (! f.existsAsFile()) return;
    irFile = f;
    space.loadIR (f);
    apvts.state.setProperty ("irPath", f.getFullPathName(), nullptr);
    if (auto* prm = apvts.getParameter (pid::revType)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (int) RevType::ir)); prm->endChangeGesture(); }
}

void ChainIvoProcessor::clearIR()
{
    irFile = juce::File(); space.clearIR(); apvts.state.removeProperty ("irPath", nullptr);
}

void ChainIvoProcessor::exportFile (const juce::File& in, const juce::File& out, bool float32, std::function<void (juce::String)> done)
{
    if (exporting.exchange (true)) return;
    juce::MemoryBlock state; getStateInformation (state);
    juce::Thread::launch ([this, in, out, float32, done, state]
    {
        juce::String msg;
        auto finish = [&] { exporting = false; juce::MessageManager::callAsync ([done, msg] { if (done) done (msg); }); };
        if (in == out) { msg = "No se puede escribir encima del original."; finish(); return; }
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (in));
        if (reader == nullptr) { msg = "No se pudo leer el archivo."; finish(); return; }
        const int fileCh = (int) juce::jlimit (1u, 2u, reader->numChannels);
        const double fsr = reader->sampleRate;
        // Copia de la cadena con el mismo estado; AUTO congelado para que el sonido no cambie a mitad
        auto copy = std::make_unique<ChainIvoProcessor>();
        copy->freezeAuto();
        copy->setStateInformation (state.getData(), (int) state.getSize());
        copy->setNonRealtime (true);
        const int bs = 1024;
        copy->prepareToPlay (fsr, bs);
        const int lat = copy->getLatencySamples();
        const juce::int64 len = reader->lengthInSamples;
        const juce::int64 tail = (juce::int64) (fsr * (copy->apvts.getRawParameterValue (pid::revDecay)->load() + 2.0));   // la cola completa
        out.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os (out.createOutputStream());
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (os != nullptr ? wav.createWriterFor (os.get(), fsr, (unsigned) fileCh, float32 ? 32 : 24, {}, 0) : nullptr);
        if (writer == nullptr) { msg = "No se pudo crear el archivo de salida."; finish(); return; }
        os.release();
        juce::AudioBuffer<float> blk (copy->getTotalNumInputChannels(), bs), outBlk (fileCh, bs);
        juce::MidiBuffer midi;
        juce::int64 readPos = 0, written = 0; const juce::int64 target = len + tail;
        while (written < target && ! instCancel.load())
        {
            blk.clear();
            const int n = bs;
            juce::AudioBuffer<float> in2 (fileCh, n); in2.clear();
            if (readPos < len) reader->read (&in2, 0, (int) std::min<juce::int64> (n, len - readPos), readPos, true, fileCh > 1);
            for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, in2, std::min (c, fileCh - 1), 0, n);
            copy->processBlock (blk, midi);
            readPos += n;
            // quitar la latencia del principio: el archivo queda alineado con el original
            int start = 0;
            const juce::int64 produced = readPos - lat;
            if (produced <= 0) continue;
            if (produced < n) start = (int) (n - produced);
            const int count = (int) std::min<juce::int64> (n - start, target - written);
            for (int c = 0; c < fileCh; ++c)
            {
                if (fileCh == 1) { outBlk.copyFrom (0, 0, blk, 0, start, count); outBlk.addFrom (0, 0, blk, 1, start, count); outBlk.applyGain (0, 0, count, 0.5f); }
                else outBlk.copyFrom (c, 0, blk, c, start, count);
            }
            writer->writeFromAudioSampleBuffer (outBlk, 0, count);
            written += count;
        }
        writer.reset();
        msg = "Exportado: " + out.getFileName();
        finish();
    });
}

void ChainIvoProcessor::fillGroup (AnalysisResult& a) const
{
    int idx = 0;
    auto others = group->others (groupId, idx);
    a.groupCount = (int) others.size(); a.groupIndex = idx; a.groupHasSpectrum = false; a.groupNames.clear();
    std::array<double, kBands> sum {};
    for (auto& m : others)
    {
        a.groupNames.add (m.name.isNotEmpty() ? m.name : voiceTypeName ((VoiceType) juce::jlimit (0, (int) VoiceType::count - 1, std::max (0, m.type))));
        if (! m.hasSpectrum) continue;
        a.groupHasSpectrum = true;
        for (int b = 0; b < kBands; ++b) sum[(size_t) b] += std::pow (10.0, m.spectrum[(size_t) b] / 10.0);
    }
    for (int b = 0; b < kBands; ++b) a.groupSpectrum[(size_t) b] = powToDb ((float) sum[(size_t) b]);
}

void ChainIvoProcessor::timerCallback()
{
    if (instReady.exchange (false))
    {
        { const juce::ScopedLock sl (instLock); instSpectrum = instPending; instName = instPendingName; }
        juce::StringArray v; for (auto x : instSpectrum) v.add (juce::String (x, 2));
        apvts.state.setProperty ("instName", instName, nullptr);
        apvts.state.setProperty ("instSpec", v.joinIntoString (","), nullptr);
        if (! isPro()) autoPropose();
    }
    lastAnalysis = analyzer.getResult();
    lastAnalysis.hasInstrumental = instName.isNotEmpty();
    lastAnalysis.instrumentalSpectrum = instSpectrum;
    // Publicar esta capa para las demás instancias y leer las suyas
    {
        GroupMember me; me.name = trackName; me.type = getVoiceType(); me.hasSpectrum = lastAnalysis.ready; me.spectrum = lastAnalysis.spectrum;
        group->update (groupId, me);
        fillGroup (lastAnalysis);
    }
    if (lastAnalysis.activeSeconds >= 2.0f)
    {
        lastClass = classify (lastAnalysis);
        if (lastAnalysis.ready) apvts.state.setProperty (kDetected, (int) lastClass.type, nullptr);
    }
    // Perfil personal: lo que el usuario deja cambiado (estable 2 s) después de una propuesta de AUTO
    if (profile.enabled() && ! lastAuto.empty())
    {
        for (auto& [id, autoVal] : lastAuto)
        {
            if (learnedThisBuild.contains (id) || ! UserProfile::learnable().contains (id)) continue;
            const float cur = p (id.toRawUTF8());
            const float prev = lastSeenVal.count (id) ? lastSeenVal[id] : cur;
            lastSeenVal[id] = cur;
            if (std::abs (cur - prev) > 1.0e-4f) { stableTicks[id] = 0; continue; }
            if (std::abs (cur - autoVal) < UserProfile::threshold (id)) continue;
            if (++stableTicks[id] >= 6) { profile.learn (getVoiceType(), id, cur - autoVal); learnedThisBuild.add (id); }
        }
    }
    // Diagnóstico: registrar en un archivo si el audio tuvo que recuperarse
    if (dspErrors.load() > loggedErrors)
    {
        loggedErrors = dspErrors.load();
        auto log = PresetLibrary::userFolder().getParentDirectory().getChildFile ("diagnostico.txt");
        log.appendText (juce::Time::getCurrentTime().toString (true, true) + "  bloques recuperados: " + juce::String (loggedErrors)
                        + "  preset: " + getPresetName() + "  calidad: " + juce::String (getQuality()) + "\n");
    }
    // AUTO: la primera vez que hay análisis suficiente, se construye la cadena (una sola vez; nunca al abrir un proyecto ya hecho)
    if (! isPro() && lastAnalysis.ready && ! (bool) apvts.state.getProperty (kAutoDone, false))
    {
        const juce::String preset = getPresetName();
        int idx = -1;
        for (int i = 0; i < (int) presets.recipes().size(); ++i) if (presets.recipes()[(size_t) i].name == preset) idx = i;
        if (idx >= 0) loadFactoryPreset (idx); else autoPropose();
    }
}

// ------------------------------------------------------------------ Orden y bloqueos
std::array<int, 6> ChainIvoProcessor::getOrder() const
{
    std::array<int, 6> o {};
    for (int i = 0; i < 6; ++i) o[(size_t) i] = order[(size_t) i].load();
    return o;
}
void ChainIvoProcessor::moveEngine (int engine, int delta)
{
    auto o = getOrder();
    int pos = -1; for (int i = 0; i < 6; ++i) if (o[(size_t) i] == engine) pos = i;
    const int to = pos + delta;
    if (pos < 0 || to < 0 || to > 5) return;
    std::swap (o[(size_t) pos], o[(size_t) to]);
    juce::StringArray s;
    for (int i = 0; i < 6; ++i) { order[(size_t) i] = o[(size_t) i]; s.add (juce::String (o[(size_t) i])); }
    apvts.state.setProperty (kOrder, s.joinIntoString (","), nullptr);
}
bool ChainIvoProcessor::isLocked (int engine) const
{
    const auto s = apvts.state.getProperty (kLocks, "000000").toString();
    return engine >= 0 && engine < s.length() && s[engine] == '1';
}
void ChainIvoProcessor::setLocked (int engine, bool l)
{
    auto s = apvts.state.getProperty (kLocks, "000000").toString().paddedRight ('0', 6);
    s = s.substring (0, engine) + (l ? "1" : "0") + s.substring (engine + 1);
    apvts.state.setProperty (kLocks, s, nullptr);
}

// ------------------------------------------------------------------ A/B
juce::ValueTree ChainIvoProcessor::engineSnapshot() const
{
    juce::ValueTree t ("Snapshot");
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != pid::bypass)
                t.setProperty (r->getParameterID(), r->convertFrom0to1 (r->getValue()), nullptr);
    t.setProperty (kOrder, apvts.state.getProperty (kOrder), nullptr);
    return t;
}
void ChainIvoProcessor::restoreSnapshot (const juce::ValueTree& t)
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (t.hasProperty (r->getParameterID()) && r->getParameterID() != pid::bypass)
            {
                r->beginChangeGesture(); r->setValueNotifyingHost (r->convertTo0to1 ((float) t.getProperty (r->getParameterID()))); r->endChangeGesture();
            }
    if (t.hasProperty (kOrder))
    {
        auto parts = juce::StringArray::fromTokens (t.getProperty (kOrder).toString(), ",", "");
        if (parts.size() == 6) { for (int i = 0; i < 6; ++i) order[(size_t) i] = parts[i].getIntValue(); apvts.state.setProperty (kOrder, t.getProperty (kOrder), nullptr); }
    }
}
void ChainIvoProcessor::selectAb (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abSlots[abSlot] = engineSnapshot();
    if (! abSlots[slot].isValid()) abSlots[slot] = abSlots[abSlot].createCopy();
    restoreSnapshot (abSlots[slot]);
    abSlot = slot;
    apvts.state.setProperty (kAb, abSlot, nullptr);
}
void ChainIvoProcessor::copyAb()
{
    abSlots[1 - abSlot] = engineSnapshot();
}

// ------------------------------------------------------------------ Estado del proyecto
void ChainIvoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty (kScale, uiScale.load(), nullptr);
    for (int i = state.getNumChildren(); --i >= 0;) if (state.getChild (i).hasType ("Snapshot")) state.removeChild (i, nullptr);
    // A y B se guardan en orden (primero A, luego B) para que el proyecto los recupere
    for (int s = 0; s < 2; ++s)
        state.appendChild (abSlots[s].isValid() ? abSlots[s].createCopy() : juce::ValueTree ("Snapshot"), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ChainIvoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    // Proyectos de versiones futuras/anteriores: se leen los parámetros que existan; lo demás queda por defecto
    for (int s = 0; s < 2; ++s) abSlots[s] = {};
    int slot = 0;
    for (auto child : tree)
        if (child.hasType ("Snapshot") && slot < 2) { if (child.getNumProperties() > 0) abSlots[slot] = child.createCopy(); ++slot; }
    for (int i = tree.getNumChildren(); --i >= 0;) if (tree.getChild (i).hasType ("Snapshot")) tree.removeChild (i, nullptr);
    apvts.replaceState (tree);
    uiScale = (float) tree.getProperty (kScale, 1.0f);
    abSlot = tree.getProperty (kAb, 0);
    auto parts = juce::StringArray::fromTokens (tree.getProperty (kOrder, "0,1,2,3,4,5").toString(), ",", "");
    if (parts.size() == 6) for (int i = 0; i < 6; ++i) order[(size_t) i] = juce::jlimit (0, 5, parts[i].getIntValue());
    for (int i = 0; i < 6; ++i) explanations[(size_t) i] = tree.getProperty (whyId (i)).toString();
    enginesBuilt = tree.getProperty (kBuilt, 0);
    instName = tree.getProperty ("instName").toString();
    {
        const juce::File ir (tree.getProperty ("irPath").toString());
        if (tree.hasProperty ("irPath") && ir.existsAsFile()) { irFile = ir; space.loadIR (ir); } else { irFile = juce::File(); space.clearIR(); }
    }
    {
        auto v = juce::StringArray::fromTokens (tree.getProperty ("instSpec").toString(), ",", "");
        if (v.size() == kBands) for (int b = 0; b < kBands; ++b) instSpectrum[(size_t) b] = v[b].getFloatValue(); else instName = {};
    }
    const int q = juce::jlimit (0, 3, (int) tree.getProperty (kQuality, (int) Quality::normal));
    if (q != quality.load()) { quality = q; if (currentSr > 0 && maxBlock > 0) rebuildForQuality(); }
}

juce::AudioProcessorEditor* ChainIvoProcessor::createEditor() { return new ChainIvoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChainIvoProcessor(); }
