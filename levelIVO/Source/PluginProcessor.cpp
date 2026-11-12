#include "PluginProcessor.h"
#include "ui/PluginEditor.h"

namespace levelivo
{
// ============================================================================ CaptureFifo
bool CaptureFifo::push (juce::int64 hostPos, const float* l, const float* r, const float* ref, int n)
{
    if (fifo.getFreeSpace() < n || hfifo.getFreeSpace() < 1) { overflow = true; return false; }
    int s1, n1, s2, n2;
    fifo.prepareToWrite (n, s1, n1, s2, n2);
    const float* src[3] = { l, r != nullptr ? r : l, ref };
    for (int c = 0; c < 3; ++c)
    {
        if (src[c] == nullptr) { data.clear (c, s1, n1); if (n2) data.clear (c, s2, n2); continue; }
        data.copyFrom (c, s1, src[c], n1);
        if (n2) data.copyFrom (c, s2, src[c] + n1, n2);
    }
    fifo.finishedWrite (n1 + n2);
    int h1, hn1, h2, hn2;
    hfifo.prepareToWrite (1, h1, hn1, h2, hn2);
    headers[(size_t) (hn1 ? h1 : h2)] = { hostPos, n };
    hfifo.finishedWrite (1);
    return true;
}
bool CaptureFifo::pop (Header& h, juce::AudioBuffer<float>& out)
{
    if (hfifo.getNumReady() < 1) return false;
    int h1, hn1, h2, hn2;
    hfifo.prepareToRead (1, h1, hn1, h2, hn2);
    h = headers[(size_t) (hn1 ? h1 : h2)];
    hfifo.finishedRead (1);
    if (out.getNumSamples() < h.n) out.setSize (3, h.n, false, false, true);
    int s1, n1, s2, n2;
    fifo.prepareToRead (h.n, s1, n1, s2, n2);
    for (int c = 0; c < 3; ++c)
    {
        out.copyFrom (c, 0, data, c, s1, n1);
        if (n2) out.copyFrom (c, n1, data, c, s2, n2);
    }
    fifo.finishedRead (n1 + n2);
    return true;
}

// ============================================================================ Procesador
static juce::File captureFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("IVO Beatz").getChildFile ("levelIVO").getChildFile ("Capturas");
}

LevelIvoProcessor::LevelIvoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Beat (referencia)", juce::AudioChannelSet::stereo(), false)),
      juce::Thread ("levelIVO capture"),
      apvts (*this, &undo, "levelIVO", createLayout())
{
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) apvts.addParameterListener (r->getParameterID(), this);
    apvts.state.getOrCreateChildWithName ("EDITS", nullptr);
    engine.onResult = [this, a = alive] { if (a->load()) triggerAsyncUpdate(); };
    lastMode = (int) apvts.getRawParameterValue (id::mode)->load();
    startTimerHz (30);
    startThread (juce::Thread::Priority::normal);
}

LevelIvoProcessor::~LevelIvoProcessor()
{
    alive->store (false);
    stopTimer();
    cancelPendingUpdate();
    signalThreadShouldExit();
    stopThread (3000);
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) apvts.removeParameterListener (r->getParameterID(), this);
}

bool LevelIvoProcessor::isBusesLayoutSupported (const BusesLayout& l) const
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

void LevelIvoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = std::max (1, samplesPerBlock);
    fifo.prepare ((int) (sr * 20.0) + maxBlock);
    rt.prepare (sr, maxBlock, 2);
    engineMode = (int) apvts.getRawParameterValue (id::engine)->load();
    setLatencySamples (engineMode == 1 ? rt.latency() : 0);
    mismatchCount = matchCount = 0;
}

void LevelIvoProcessor::parameterChanged (const juce::String& p, float)
{
    if (isLevelParam (p)) levelDirty = true;
    if (isCleanParam (p)) { cleanDirty = true; cleanDirtySince = juce::Time::getMillisecondCounter(); }
    if (p == id::mode) modeDirty = true;
}

// ---------------------------------------------------------------------------- audio
void LevelIvoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    auto main = getBusBuffer (buffer, true, 0);
    const int nch = main.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (nch == 0 || n == 0) return;

    // referencia (beat) por el sidechain
    const float* ref = nullptr;
    std::array<float, 8192> refMono {};
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled() && n <= (int) refMono.size())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            float pk = 0;
            for (int i = 0; i < n; ++i)
            {
                float s = 0; for (int c = 0; c < sc.getNumChannels(); ++c) s += sc.getSample (c, i);
                refMono[(size_t) i] = s / (float) sc.getNumChannels(); pk = std::max (pk, std::abs (refMono[(size_t) i]));
            }
            if (pk > 1e-6f) { ref = refMono.data(); scActive = true; }
        }
    }

    // posición en el DAW
    juce::int64 hostPos = -1; bool isPlay = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            isPlay = pos->getIsPlaying();
            if (auto t = pos->getTimeInSamples()) hostPos = *t;
        }
    playing = isPlay;

    float inPk = 0; for (int c = 0; c < nch; ++c) inPk = std::max (inPk, main.getMagnitude (c, 0, n));
    inPeak = std::max (inPk, inPeak.load() * 0.9f);

    if (capturing.load() && isPlay && hostPos >= 0)
    {
        if (ref != nullptr) capturedRef = true;
        fifo.push (hostPos, main.getReadPointer (0), nch > 1 ? main.getReadPointer (1) : nullptr, ref, n);
    }

    const bool bypassed = apvts.getRawParameterValue (id::bypass)->load() > 0.5f;
    const int eng = (int) apvts.getRawParameterValue (id::engine)->load();
    const float outGain = dsp::dbToLin (apvts.getRawParameterValue (id::output)->load());

    if (eng == 1 && engineMode == 1)
    {
        RtLeveler::Settings s;
        s.intensity = bypassed ? 0.0f : apvts.getRawParameterValue (id::intensity)->load();
        s.sensitivity = apvts.getRawParameterValue (id::sensitivity)->load();
        s.maxGain = apvts.getRawParameterValue (id::maxGain)->load();
        s.maxAtt = apvts.getRawParameterValue (id::maxAtt)->load();
        s.speed = apvts.getRawParameterValue (id::rtSpeed)->load();
        s.peakProtect = apvts.getRawParameterValue (id::peakProtect)->load() > 0.5f;
        rt.process (main, n, s);
        if (! bypassed) main.applyGain (outGain);
        grNow = rt.currentGainDb();
        playPos = -1;
    }
    else if (! bypassed)
    {
        ResultPtr r;
        { juce::SpinLock::ScopedTryLockType l (resultLock); if (l.isLocked()) r = audioResult; }
        const auto listen = (Listen) (int) apvts.getRawParameterValue (id::listen)->load();
        const bool match = apvts.getRawParameterValue (id::matchLoud)->load() > 0.5f;
        const bool targetOn = apvts.getRawParameterValue (id::targetOn)->load() > 0.5f;
        const bool ride = rideMode.load();
        const float rideTarget = dsp::dbToLin (apvts.getRawParameterValue (id::rideGain)->load());

        if (r != nullptr && r->take != nullptr && std::abs (r->take->sampleRate - sr) < 1.0)
        {
            const auto& T = *r->take;
            const int tn = T.numSamples(), tch = T.numChannels();
            float comp = (match && ! (listen == Listen::final_ && targetOn)) ? r->comp[(int) listen] : 1.0f;
            if (ride) comp = 1.0f;
            juce::int64 shownPos = -1;
            if (previewing.load())
            {
                // escucha de un archivo arrastrado (sin línea de tiempo)
                int p = previewPos.load();
                for (int i = 0; i < n; ++i, ++p)
                {
                    if (p >= tn) { previewing = false; break; }
                    for (int c = 0; c < nch; ++c) main.setSample (c, i, r->sample (listen, std::min (c, tch - 1), p) * comp);
                }
                previewPos = p; shownPos = p;
            }
            else if (isPlay && hostPos >= 0 && listen != Listen::original && ! mismatch.load())
            {
                double eIn = 0, eTake = 0;
                const int fade = std::max (1, (int) (sr * 0.005));
                int i = 0;
                while (i < n)
                {
                    const juce::int64 hp = hostPos + i;
                    // región que contiene esta posición
                    const Region* reg = nullptr;
                    for (const auto& rg : T.regions) if (rg.hostStart >= 0 && hp >= rg.hostStart && hp < rg.hostStart + rg.length) { reg = &rg; break; }
                    if (reg == nullptr) { ++i; continue; }
                    const int run = (int) std::min<juce::int64> (n - i, reg->hostStart + reg->length - hp);
                    for (int k = 0; k < run; ++k)
                    {
                        const int off = (int) (hp + k - reg->hostStart);
                        const int idx = reg->takeStart + off;
                        if (idx < 0 || idx >= tn) continue;
                        const int edge = std::min (off, reg->length - 1 - off);
                        const float w = edge >= fade ? 1.0f : (float) edge / (float) fade;
                        if (ride) rideSmooth += 0.002f * (rideTarget - rideSmooth);
                        for (int c = 0; c < nch; ++c)
                        {
                            const int tc = std::min (c, tch - 1);
                            const float x = main.getSample (c, i + k);
                            const float o = T.audio.getSample (tc, idx);
                            eIn += (double) x * x; eTake += (double) o * o;
                            float y = ride ? r->sample (Listen::clean, tc, idx) * rideSmooth
                                           : r->sample (listen, tc, idx) * comp;
                            main.setSample (c, i + k, w * y + (1.0f - w) * x);
                        }
                        shownPos = idx;
                    }
                    i += std::max (1, run);
                }
                // ¿sigue sonando lo mismo que se capturó? (si moviste o cambiaste el clip, se deja pasar el audio)
                if (eTake > 1e-7 * n && eIn > 1e-7 * n)
                {
                    const double d = 10.0 * std::log10 (eIn / eTake);
                    if (std::abs (d) > 6.0) { if (++mismatchCount > 40) mismatch = true; }
                    else { mismatchCount = std::max (0, mismatchCount - 2); }
                }
            }
            else if (isPlay && hostPos >= 0)
            {
                const int idx = T.takeIndexFor (hostPos);
                shownPos = idx >= 0 ? idx : -1;
            }
            playPos = shownPos;
            if (shownPos >= 0 && r->level != nullptr && r->analysis != nullptr)
            {
                const int f = std::clamp ((int) (shownPos / r->analysis->hop), 0, (int) r->level->gainDb.size() - 1);
                grNow = ride ? dsp::linToDb (rideSmooth) : r->level->gainDb[(size_t) f];
            }
        }
        else playPos = -1;
        main.applyGain (outGain);
    }
    float outPk = 0; for (int c = 0; c < nch; ++c) outPk = std::max (outPk, main.getMagnitude (c, 0, n));
    outPeak = std::max (outPk, outPeak.load() * 0.9f);
}

// ---------------------------------------------------------------------------- captura
void LevelIvoProcessor::startCapture()
{
    {
        std::lock_guard<std::mutex> l (capMutex);
        capRegions.clear(); capSeconds = 0.0;
        capChannels = std::clamp (getMainBusNumInputChannels(), 1, 2);
    }
    fifo.reset(); fifo.overflow = false; capturedRef = false; mismatch = false; mismatchCount = 0;
    stopSeenMs = 0;
    capturing = true;
}

double LevelIvoProcessor::capturedSeconds() const { return capSeconds.load(); }

void LevelIvoProcessor::run()
{
    while (! threadShouldExit())
    {
        drainCapture();
        wait (20);
    }
}

void LevelIvoProcessor::drainCapture()
{
    static thread_local juce::AudioBuffer<float> tmp (3, 8192);
    CaptureFifo::Header h;
    std::lock_guard<std::mutex> l (capMutex);
    while (fifo.pop (h, tmp))
    {
        juce::int64 pos = h.hostPos; int off = 0, len = h.n;
        // ¿ya capturado? se salta lo que se solapa
        for (auto& r : capRegions)
            if (pos >= r.hostStart && pos < r.hostEnd)
            {
                const int skip = (int) std::min<juce::int64> (len, r.hostEnd - pos);
                pos += skip; off += skip; len -= skip;
            }
        if (len <= 0) continue;
        CapRegion* target = nullptr;
        for (auto& r : capRegions) if (r.hostEnd == pos) { target = &r; break; }
        if (target == nullptr) { capRegions.push_back ({}); target = &capRegions.back(); target->hostStart = target->hostEnd = pos; }
        // no invadir una región posterior
        for (auto& r : capRegions) if (&r != target && r.hostStart > pos && r.hostStart < pos + len) len = (int) (r.hostStart - pos);
        for (int c = 0; c < 2; ++c) target->ch[c].insert (target->ch[c].end(), tmp.getReadPointer (c) + off, tmp.getReadPointer (c) + off + len);
        target->ref.insert (target->ref.end(), tmp.getReadPointer (2) + off, tmp.getReadPointer (2) + off + len);
        target->hostEnd += len;
    }
    double tot = 0; for (auto& r : capRegions) tot += (double) (r.hostEnd - r.hostStart);
    capSeconds = tot / sr;
    if (capSeconds.load() > 20.0 * 60.0) capturing = false;   // límite de seguridad: 20 minutos
}

TakePtr LevelIvoProcessor::buildTake()
{
    std::lock_guard<std::mutex> l (capMutex);
    std::sort (capRegions.begin(), capRegions.end(), [] (const CapRegion& a, const CapRegion& b) { return a.hostStart < b.hostStart; });
    // unir tramos contiguos
    std::vector<CapRegion> merged;
    for (auto& r : capRegions)
    {
        if (r.hostEnd <= r.hostStart) continue;
        if (! merged.empty() && merged.back().hostEnd == r.hostStart)
        {
            auto& m = merged.back();
            for (int c = 0; c < 2; ++c) m.ch[c].insert (m.ch[c].end(), r.ch[c].begin(), r.ch[c].end());
            m.ref.insert (m.ref.end(), r.ref.begin(), r.ref.end());
            m.hostEnd = r.hostEnd;
        }
        else merged.push_back (std::move (r));
    }
    capRegions.clear();
    int total = 0; for (auto& r : merged) total += (int) (r.hostEnd - r.hostStart);
    if (total < (int) (sr * 0.5)) return nullptr;
    auto t = std::make_shared<Take>();
    t->sampleRate = sr;
    t->audio.setSize (capChannels, total);
    const bool hasRef = capturedRef.load();
    if (hasRef) t->reference.resize ((size_t) total);
    int at = 0;
    for (auto& r : merged)
    {
        const int len = (int) (r.hostEnd - r.hostStart);
        for (int c = 0; c < capChannels; ++c) std::copy (r.ch[c].begin(), r.ch[c].begin() + len, t->audio.getWritePointer (c) + at);
        if (hasRef) std::copy (r.ref.begin(), r.ref.begin() + len, t->reference.begin() + at);
        t->regions.push_back ({ r.hostStart, at, len });
        at += len;
    }
    t->name = trackName.isNotEmpty() ? trackName : juce::String ("Voz");
    t->fileId = juce::Uuid().toString();
    return t;
}

void LevelIvoProcessor::stopCapture (bool analyze)
{
    if (! capturing.exchange (false)) return;
    drainCapture();
    auto t = buildTake();
    if (t == nullptr) { loadError = juce::String::fromUTF8 ("No se capturó suficiente audio: dale al play mientras CAPTURAR está activo."); return; }
    loadError.clear();
    if (fifo.overflow.load()) loadError = juce::String::fromUTF8 ("Aviso: el ordenador iba justo y se perdi\xc3\xb3 parte de la captura. Si notas huecos, vuelve a capturar.");
    takeFromFile = false;
    saveTakeAsync (t);
    applied = false;
    if (analyze) { mismatch = false; mismatchCount = 0; engine.setTake (t); levelDirty = cleanDirty = true; }
}

void LevelIvoProcessor::saveTakeAsync (TakePtr t)
{
    auto f = captureFolder().getChildFile (t->fileId + ".wav");
    takeFile = f; takeFileId = t->fileId;
    juce::Thread::launch ([t, f]
    {
        f.getParentDirectory().createDirectory();
        const int vc = t->numChannels(), tc = vc + (t->hasReference() ? 1 : 0);
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        if (os == nullptr) return;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), t->sampleRate, (unsigned) tc, 32, {}, 0));
        if (w == nullptr) return;
        os.release();
        std::vector<const float*> chans;
        for (int c = 0; c < vc; ++c) chans.push_back (t->audio.getReadPointer (c));
        if (t->hasReference()) chans.push_back (t->reference.data());
        w->writeFromFloatArrays (chans.data(), tc, t->numSamples());
    });
}

void LevelIvoProcessor::loadTakeFile (const juce::File& f, std::vector<Region> regions, int voiceCh, bool hasRef, juce::String name)
{
    auto* self = this;
    auto a = alive;
    juce::Thread::launch ([self, a, f, regions, voiceCh, hasRef, name]
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        if (rd == nullptr) return;
        auto t = std::make_shared<Take>();
        t->sampleRate = rd->sampleRate;
        const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) std::numeric_limits<int>::max() / 4);
        const int fileCh = (int) rd->numChannels;
        juce::AudioBuffer<float> all (std::max (1, fileCh), len);
        rd->read (&all, 0, len, 0, true, true);
        const int vc = std::clamp (voiceCh > 0 ? voiceCh : std::min (2, fileCh), 1, std::max (1, std::min (2, fileCh)));
        t->audio.setSize (vc, len);
        for (int c = 0; c < vc; ++c) t->audio.copyFrom (c, 0, all, std::min (c, fileCh - 1), 0, len);
        if (hasRef && fileCh > vc) t->reference.assign (all.getReadPointer (vc), all.getReadPointer (vc) + len);
        t->regions = regions;
        if (t->regions.empty()) t->regions.push_back ({ -1, 0, len });
        t->name = name; t->fileId = f.getFileNameWithoutExtension(); t->fromFile = regions.empty() || regions[0].hostStart < 0;
        juce::MessageManager::callAsync ([self, a, t] { if (! a->load()) return; self->mismatch = false; self->engine.setTake (t); self->levelDirty = self->cleanDirty = true; });
    });
}

void LevelIvoProcessor::loadFile (const juce::File& f)
{
    if (! f.existsAsFile()) return;
    capturing = false;
    takeFromFile = true; takeFile = f; takeFileId = f.getFileNameWithoutExtension();
    loadError.clear(); applied = false;
    loadTakeFile (f, {}, 0, false, f.getFileNameWithoutExtension());
}

void LevelIvoProcessor::analyzeAgain()
{
    if (capturing.load()) { stopCapture (true); return; }
    if (auto r = current) { mismatch = false; mismatchCount = 0; engine.setTake (r->take); levelDirty = cleanDirty = true; }
}

void LevelIvoProcessor::apply()
{
    applied = true;
    if (auto* p = apvts.getParameter (id::listen)) p->setValueNotifyingHost (p->convertTo0to1 (3.0f));
}

void LevelIvoProcessor::setRideMode (bool on) { rideMode = on; rideSmooth = 1.0f; }

void LevelIvoProcessor::setPreview (bool play, int from)
{
    if (from >= 0) previewPos = from;
    previewing = play;
}

void LevelIvoProcessor::loadPreset (int index)
{
    const auto& all = presets.all();
    if (index < 0 || index >= (int) all.size()) return;
    applyingMode = true;
    presets.apply (all[(size_t) index], apvts);
    presetName = all[(size_t) index].name;
    lastMode = (int) apvts.getRawParameterValue (id::mode)->load();
    modeDirty = false;
    applyingMode = false;
}

void LevelIvoProcessor::savePreset (const juce::String& name)
{
    if (presets.saveUser (name, apvts)) presetName = name;
}

// ---------------------------------------------------------------------------- ediciones
std::vector<Edit> LevelIvoProcessor::getEdits() const
{
    std::vector<Edit> out;
    auto t = apvts.state.getChildWithName ("EDITS");
    for (auto c : t)
    {
        Edit e; e.kind = (int) c.getProperty ("k"); e.a = (int) c.getProperty ("a"); e.b = (int) c.getProperty ("b"); e.value = (float) (double) c.getProperty ("v");
        out.push_back (e);
    }
    return out;
}
void LevelIvoProcessor::addEdit (int kind, int a, int b, float value)
{
    auto t = apvts.state.getOrCreateChildWithName ("EDITS", nullptr);
    // un mismo tramo sólo guarda una edición de cada tipo
    for (int i = t.getNumChildren(); --i >= 0;)
    {
        auto c = t.getChild (i);
        if ((int) c.getProperty ("k") == kind && (int) c.getProperty ("a") == a && (int) c.getProperty ("b") == b) t.removeChild (i, &undo);
    }
    juce::ValueTree e ("E");
    e.setProperty ("k", kind, nullptr); e.setProperty ("a", a, nullptr); e.setProperty ("b", b, nullptr); e.setProperty ("v", value, nullptr);
    undo.beginNewTransaction();
    t.appendChild (e, &undo);
    levelDirty = true;
}
void LevelIvoProcessor::clearEdits (int a, int b)
{
    auto t = apvts.state.getOrCreateChildWithName ("EDITS", nullptr);
    undo.beginNewTransaction();
    for (int i = t.getNumChildren(); --i >= 0;)
    {
        auto c = t.getChild (i);
        if ((int) c.getProperty ("a") < b && (int) c.getProperty ("b") > a) t.removeChild (i, &undo);
    }
    levelDirty = true;
}

// ---------------------------------------------------------------------------- motor
void LevelIvoProcessor::pushLevelParams() { engine.setLevelParams (readLevelParams (apvts), getEdits()); }
void LevelIvoProcessor::pushCleanParams() { engine.setCleanParams (readCleanParams (apvts)); }

void LevelIvoProcessor::publish (ResultPtr r)
{
    ResultPtr old;
    { juce::SpinLock::ScopedLockType l (resultLock); old = audioResult; audioResult = r; }
    // lo anterior se libera más tarde, nunca en el hilo de audio
    if (old != nullptr) retired.push_back ({ juce::Time::getMillisecondCounter(), old });
    current = r;
}

void LevelIvoProcessor::handleAsyncUpdate()
{
    auto r = engine.getResult();
    publish (r);
    if (r != nullptr && r->analysis != lastAnalysis)
    {
        lastAnalysis = r->analysis;
        const bool autoA = apvts.getRawParameterValue (id::autoApply)->load() > 0.5f;
        if (autoA) apply();
        else if (! applied.load())
            if (auto* p = apvts.getParameter (id::listen)) p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
    }
}

void LevelIvoProcessor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    // liberar resultados antiguos
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (const auto& p) { return now - p.first > 3000; }), retired.end());

    // modos: al elegir Natural/Balanced/Controlled se ponen sus valores; si se toca un control, pasa a Manual
    const int mode = (int) apvts.getRawParameterValue (id::mode)->load();
    if (modeDirty.exchange (false) && mode != lastMode && ! applyingMode)
    {
        lastMode = mode;
        if (mode < 3)
        {
            applyingMode = true;
            const auto mv = modeValues (mode);
            const std::pair<const char*, float> vals[] = { { id::phrase, mv.phrase }, { id::word, mv.word }, { id::syllable, mv.syllable },
                                                          { id::sensitivity, mv.sensitivity }, { id::maxGain, mv.maxGain }, { id::maxAtt, mv.maxAtt } };
            for (auto& [pidv, val] : vals)
                if (auto* p = apvts.getParameter (pidv)) p->setValueNotifyingHost (p->convertTo0to1 (val));
            applyingMode = false;
            presetName = apvts.getParameter (id::mode)->getCurrentValueAsText();
        }
    }
    else if (mode < 3 && ! applyingMode)
    {
        const auto mv = modeValues (mode);
        auto off = [this] (const char* i, float v) { return std::abs (apvts.getRawParameterValue (i)->load() - v) > 0.006f; };
        if (off (id::phrase, mv.phrase) || off (id::word, mv.word) || off (id::syllable, mv.syllable)
            || off (id::sensitivity, mv.sensitivity) || off (id::maxGain, mv.maxGain) || off (id::maxAtt, mv.maxAtt))
        {
            lastMode = 3;
            if (auto* p = apvts.getParameter (id::mode)) p->setValueNotifyingHost (p->convertTo0to1 (3.0f));
            modeDirty = false;
        }
    }

    // ediciones (deshacer/rehacer incluido): huella barata del árbol
    {
        juce::int64 h = 17;
        for (auto c : apvts.state.getChildWithName ("EDITS"))
            h = h * 31 + (juce::int64) c.getProperty ("k") * 7 + (juce::int64) c.getProperty ("a") * 13 + (juce::int64) c.getProperty ("b") + (juce::int64) ((double) c.getProperty ("v") * 1000);
        if (h != editsHash) { editsHash = h; levelDirty = true; }
    }
    if (levelDirty.exchange (false)) pushLevelParams();
    if (cleanDirty.load() && now - cleanDirtySince > 250) { cleanDirty = false; pushCleanParams(); }

    // motor tiempo real / offline
    const int eng = (int) apvts.getRawParameterValue (id::engine)->load();
    if (eng != engineMode) { engineMode = eng; setLatencySamples (eng == 1 ? rt.latency() : 0); }

    // al parar el transporte tras capturar, se analiza solo
    if (capturing.load())
    {
        if (! playing.load() && capSeconds.load() > 0.5)
        {
            if (stopSeenMs == 0) stopSeenMs = now;
            else if (now - stopSeenMs > 400) stopCapture (true);
        }
        else stopSeenMs = 0;
    }

    // escribir la curva como automatización (rideGain) mientras suena
    if (rideMode.load() && applied.load() && current != nullptr && current->level != nullptr)
    {
        auto* p = apvts.getParameter (id::rideGain);
        const auto pos = playPos.load();
        if (playing.load() && pos >= 0)
        {
            if (! rideGesture) { p->beginChangeGesture(); rideGesture = true; }
            const int f = std::clamp ((int) (pos / current->analysis->hop), 0, (int) current->level->gainDb.size() - 1);
            p->setValueNotifyingHost (p->convertTo0to1 (current->level->gainDb[(size_t) f]));
        }
        else if (rideGesture) { p->endChangeGesture(); rideGesture = false; }
    }
    if (mismatch.load() && ! playing.load()) { /* se mantiene el aviso hasta volver a capturar */ }
}

juce::String LevelIvoProcessor::statusText() const
{
    auto mmss = [] (double s) { const int t = (int) s; return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2); };
    if (capturing.load())
        return capSeconds.load() < 0.05 ? juce::String::fromUTF8 ("Capturando: dale al play en la parte que quieras")
                                        : juce::String::fromUTF8 ("Capturando\xe2\x80\xa6 ") + mmss (capSeconds.load()) + juce::String::fromUTF8 (" \xc2\xb7 para el transporte para analizar");
    switch (engine.stage())
    {
        case Engine::Stage::analyzing: return juce::String::fromUTF8 ("Analizando ") + juce::String (juce::roundToInt (engine.progress() * 100)) + " %";
        case Engine::Stage::cleaning:  return juce::String::fromUTF8 ("Limpiando ") + juce::String (juce::roundToInt (engine.progress() * 100)) + " %";
        case Engine::Stage::leveling:  return juce::String::fromUTF8 ("Calculando la curva\xe2\x80\xa6");
        case Engine::Stage::idle: break;
    }
    if (loadError.isNotEmpty()) return loadError;
    auto r = current;
    if (r == nullptr || r->take == nullptr) return juce::String::fromUTF8 ("Sin toma: pulsa CAPTURAR y dale al play, o arrastra un WAV aqu\xc3\xad");
    juce::String s = r->take->name.toUpperCase() + juce::String::fromUTF8 (" \xc2\xb7 ") + mmss (r->take->numSamples() / r->take->sampleRate);
    s += applied.load() ? juce::String::fromUTF8 (" \xc2\xb7 aplicado") : juce::String::fromUTF8 (" \xc2\xb7 an\xc3\xa1lisis listo, rev\xc3\xadsalo y pulsa APLICAR");
    return s;
}

// ---------------------------------------------------------------------------- exportar
void LevelIvoProcessor::exportFile (const juce::File& out, bool float32, std::function<void (juce::String)> done)
{
    auto r = current;
    if (r == nullptr || exporting.exchange (true)) { if (done) done (juce::String::fromUTF8 ("No hay nada que exportar.")); exporting = false; return; }
    const bool match = apvts.getRawParameterValue (id::matchLoud)->load() > 0.5f && apvts.getRawParameterValue (id::targetOn)->load() < 0.5f;
    const float gain = dsp::dbToLin (apvts.getRawParameterValue (id::output)->load()) * (match ? r->comp[3] : 1.0f);
    auto* self = this;
    auto a = alive;
    juce::Thread::launch ([self, a, r, out, float32, gain, done]
    {
        juce::String msg;
        const auto& T = *r->take;
        // con línea de tiempo: el archivo empieza donde empieza la primera región capturada (huecos en silencio)
        juce::int64 h0 = -1, h1 = -1;
        for (auto& g : T.regions) if (g.hostStart >= 0) { h0 = h0 < 0 ? g.hostStart : std::min (h0, g.hostStart); h1 = std::max (h1, g.hostStart + g.length); }
        const bool timeline = h0 >= 0;
        const juce::int64 len = timeline ? h1 - h0 : T.numSamples();
        const int nc = T.numChannels();
        out.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os (out.createOutputStream());
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (os != nullptr ? wav.createWriterFor (os.get(), T.sampleRate, (unsigned) nc, float32 ? 32 : 24, {}, 0) : nullptr);
        if (w == nullptr) msg = juce::String::fromUTF8 ("No se pudo crear el archivo.");
        else
        {
            os.release();
            const int bs = 8192;
            juce::AudioBuffer<float> blk (nc, bs);
            for (juce::int64 s = 0; s < len; s += bs)
            {
                const int m = (int) std::min<juce::int64> (bs, len - s);
                blk.clear();
                for (int i = 0; i < m; ++i)
                {
                    const int idx = timeline ? T.takeIndexFor (h0 + s + i) : (int) (s + i);
                    if (idx < 0) continue;
                    for (int c = 0; c < nc; ++c) blk.setSample (c, i, r->sample (Listen::final_, c, idx) * gain);
                }
                w->writeFromAudioSampleBuffer (blk, 0, m);
            }
            w.reset();
            auto mmss = [] (double sec) { return juce::String ((int) sec / 60) + ":" + juce::String (sec - 60 * ((int) sec / 60), 3).paddedLeft ('0', 6); };
            msg = juce::String::fromUTF8 ("Exportado: ") + out.getFileName();
            if (timeline) msg += juce::String::fromUTF8 (" \xc2\xb7 col\xc3\xb3" "calo en ") + mmss ((double) h0 / T.sampleRate) + juce::String::fromUTF8 (" del proyecto");
        }
        juce::MessageManager::callAsync ([self, a, done, msg] { if (! a->load()) return; self->exporting = false; if (done) done (msg); });
    });
}

// ---------------------------------------------------------------------------- estado
void LevelIvoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    auto t = st.getOrCreateChildWithName ("TAKE", nullptr);
    t.removeAllProperties (nullptr);
    if (auto r = current; r != nullptr && r->take != nullptr)
    {
        juce::String regs;
        for (auto& g : r->take->regions) regs << juce::String (g.hostStart) << "," << g.takeStart << "," << g.length << ";";
        t.setProperty ("file", takeFile.getFullPathName(), nullptr);
        t.setProperty ("regions", regs, nullptr);
        t.setProperty ("voiceCh", r->take->numChannels(), nullptr);
        t.setProperty ("ref", r->take->hasReference(), nullptr);
        t.setProperty ("name", r->take->name, nullptr);
        t.setProperty ("fromFile", takeFromFile, nullptr);
    }
    t.setProperty ("applied", applied.load(), nullptr);
    t.setProperty ("ride", rideMode.load(), nullptr);
    t.setProperty ("preset", presetName, nullptr);
    t.setProperty ("uiScale", uiScale.load(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void LevelIvoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.isValid()) return;
    auto t = st.getChildWithName ("TAKE");
    st.removeChild (t, nullptr);
    if (! st.getChildWithName ("EDITS").isValid()) st.appendChild (juce::ValueTree ("EDITS"), nullptr);
    apvts.replaceState (st);
    lastMode = (int) apvts.getRawParameterValue (id::mode)->load();
    modeDirty = false;
    if (t.isValid())
    {
        applied = (bool) t.getProperty ("applied", false);
        rideMode = (bool) t.getProperty ("ride", false);
        presetName = t.getProperty ("preset", "Balanced").toString();
        uiScale = (float) (double) t.getProperty ("uiScale", 1.0);
        const juce::File f (t.getProperty ("file").toString());
        if (f.existsAsFile())
        {
            std::vector<Region> regs;
            for (auto& s : juce::StringArray::fromTokens (t.getProperty ("regions").toString(), ";", ""))
            {
                auto p = juce::StringArray::fromTokens (s, ",", "");
                if (p.size() == 3) regs.push_back ({ p[0].getLargeIntValue(), p[1].getIntValue(), p[2].getIntValue() });
            }
            takeFile = f; takeFromFile = (bool) t.getProperty ("fromFile", false);
            if (takeFromFile) regs.clear();
            loadTakeFile (f, regs, (int) t.getProperty ("voiceCh", 2), (bool) t.getProperty ("ref", false), t.getProperty ("name").toString());
        }
        else if (t.getProperty ("file").toString().isNotEmpty())
            loadError = juce::String::fromUTF8 ("No se encuentra la captura guardada: vuelve a capturar la pista.");
    }
    levelDirty = cleanDirty = true;
}

juce::AudioProcessorEditor* LevelIvoProcessor::createEditor() { return new LevelIvoEditor (*this); }
} // namespace levelivo

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new levelivo::LevelIvoProcessor(); }
