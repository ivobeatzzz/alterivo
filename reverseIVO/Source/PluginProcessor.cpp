#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "analysis/Onset.h"
#include "dsp/Dsp.h"

namespace reverseivo
{
// ============================================================================ CaptureFifo
void CaptureFifo::prepare (int capacitySamples)
{
    data.setSize (2, capacitySamples); data.clear();
    fifo.setTotalSize (capacitySamples); fifo.reset();
    hfifo.setTotalSize (4096); hfifo.reset(); headers.resize (4096);
}
bool CaptureFifo::push (double ppq, bool playing, const float* l, const float* r, int n)
{
    if (fifo.getFreeSpace() < n || hfifo.getFreeSpace() < 1) return false;
    int s1, n1, s2, n2;
    fifo.prepareToWrite (n, s1, n1, s2, n2);
    const float* src[2] = { l, r != nullptr ? r : l };
    for (int c = 0; c < 2; ++c)
    {
        data.copyFrom (c, s1, src[c], n1);
        if (n2 > 0) data.copyFrom (c, s2, src[c] + n1, n2);
    }
    fifo.finishedWrite (n1 + n2);
    int h1, hn1, h2, hn2;
    hfifo.prepareToWrite (1, h1, hn1, h2, hn2);
    headers[(size_t) (hn1 > 0 ? h1 : h2)] = { ppq, playing, n };
    hfifo.finishedWrite (1);
    return true;
}
bool CaptureFifo::pop (Header& h, juce::AudioBuffer<float>& out)
{
    if (hfifo.getNumReady() < 1) return false;
    int h1, hn1, h2, hn2;
    hfifo.prepareToRead (1, h1, hn1, h2, hn2);
    h = headers[(size_t) (hn1 > 0 ? h1 : h2)];
    hfifo.finishedRead (1);
    if (out.getNumSamples() < h.n) out.setSize (2, h.n, false, false, true);
    int s1, n1, s2, n2;
    fifo.prepareToRead (h.n, s1, n1, s2, n2);
    for (int c = 0; c < 2; ++c)
    {
        out.copyFrom (c, 0, data, c, s1, n1);
        if (n2 > 0) out.copyFrom (c, n1, data, c, s2, n2);
    }
    fifo.finishedRead (n1 + n2);
    return true;
}

// ============================================================================ utilidades
static juce::File ivoFolder (const char* sub)
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("IVO Beatz").getChildFile ("reverseIVO").getChildFile (sub);
}
static constexpr double maxCaptureSeconds = 30.0;

// Cambio de frecuencia de muestreo de la fuente con sinc antialias (fuera del hilo de audio)
static std::shared_ptr<Source> resampled (const Source& s, double newSr)
{
    auto out = std::make_shared<Source>();
    out->sampleRate = newSr;
    if (std::abs (s.sampleRate - newSr) < 0.5) { out->l = s.l; out->r = s.r; return out; }
    static const dsp::SincReader sinc;
    const double ratio = s.sampleRate / newSr, cut = std::min (1.0, 1.0 / ratio) * 0.97;
    const int n = (int) std::floor (s.size() / ratio);
    out->l.resize ((size_t) n); out->r.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        out->l[(size_t) i] = sinc.read (s.l.data(), s.size(), i * ratio, cut);
        out->r[(size_t) i] = sinc.read (s.r.data(), s.size(), i * ratio, cut);
    }
    return out;
}

// ============================================================================ Procesador
ReverseIvoProcessor::ReverseIvoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("reverseIVO capture"),
      apvts (*this, &undo, "reverseIVO", createLayout())
{
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) apvts.addParameterListener (r->getParameterID(), this);
    fifo.prepare ((int) (48000 * 2));
    startTimerHz (30);
    startThread (juce::Thread::Priority::low);
}

ReverseIvoProcessor::~ReverseIvoProcessor()
{
    *alive = false;
    stopTimer();
    signalThreadShouldExit(); notify(); stopThread (3000);
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) apvts.removeParameterListener (r->getParameterID(), this);
}

bool ReverseIvoProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ReverseIvoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool srChanged = std::abs (sampleRate - sr) > 0.5;
    sr = sampleRate;
    fifo.prepare ((int) (sr * 2.0) + samplesPerBlock * 4);
    lastIdx = -1; ramp = 0.0f; duckEnv = 0.0f;
    // la fuente se adapta a la nueva frecuencia y el reverse se vuelve a generar
    if (srChanged)
    {
        SourcePtr s; { const juce::SpinLock::ScopedLockType l (resultLock); s = source; }
        if (s != nullptr && std::abs (s->sampleRate - sr) > 0.5)
        {
            const double k = sr / s->sampleRate;
            auto rs = resampled (*s, sr);
            const int a = (int) (regA.load() * k), b = (int) (regB.load() * k);
            { const juce::SpinLock::ScopedLockType l (resultLock); retired.push_back ({ juce::Time::getMillisecondCounter(), source }); source = rs; srcPeaks = computePeaks (*rs, 1200); }
            regA = a; regB = b;
            juce::MessageManager::callAsync ([this, w = std::weak_ptr<std::atomic<bool>> (alive)] { if (auto a2 = w.lock(); a2 && *a2) requestRender(); });
        }
    }
}

double ReverseIvoProcessor::getTailLengthSeconds() const
{
    return apvts.getRawParameterValue (id::tail)->load() + 0.1;
}

void ReverseIvoProcessor::parameterChanged (const juce::String& p, float)
{
    if (isPlaybackParam (p)) return;
    dirty = true;
    dirtySince = juce::Time::getMillisecondCounter();
}

// ---------------------------------------------------------------------------- audio
void ReverseIvoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    for (int c = nIn; c < nOut; ++c) buffer.clear (c, 0, n);
    if (n == 0 || nOut == 0) { midi.clear(); return; }

    // transporte del DAW
    double ppq0 = std::numeric_limits<double>::quiet_NaN(), bpmNow = 0.0;
    bool play = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            play = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpmNow = *b;
            if (auto q = pos->getPpqPosition()) ppq0 = *q;
            if (auto ts = pos->getTimeSignature()) { sigNum = ts->numerator; sigDen = ts->denominator; }
        }
    isPlaying = play;
    if (bpmNow > 0.0) bpm = bpmNow;
    if (! std::isnan (ppq0)) lastPpq = ppq0;

    const float* inL = nIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* inR = nIn > 1 ? buffer.getReadPointer (1) : inL;
    if (capturing.load() && inL != nullptr)
        fifo.push (play ? ppq0 : std::numeric_limits<double>::quiet_NaN(), play, inL, inR, n);

    // disparo por MIDI: una nota empieza el reverse desde el principio (termina «duración» después)
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) { midiActive = true; midiPos = -m.samplePosition; midiGain = 0.35f + 0.65f * msg.getFloatVelocity(); }
    }
    midi.clear();

    ResultPtr r; SourcePtr s;
    {
        const juce::SpinLock::ScopedTryLockType l (resultLock);
        if (l.isLocked()) { r = current; s = source; }
    }
    const bool rOk = r != nullptr && std::abs (r->sampleRate - sr) < 0.5 && r->size() > 0;
    const float outTarget = dsp::dbToLin (apvts.getRawParameterValue (id::output)->load());
    const bool through = apvts.getRawParameterValue (id::through)->load() > 0.5f;
    const float duck = apvts.getRawParameterValue (id::ducking)->load();
    const float relCoef = 1.0f - std::exp (-1.0f / (0.001f * apvts.getRawParameterValue (id::duckRelease)->load() * (float) sr));
    const float atkCoef = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    const float rampStep = 1.0f / (0.003f * (float) sr);
    const double hit = hitPpq.load();
    juce::int64 syncBase = -1;
    bool sync = false;
    if (rOk && play && ! std::isnan (hit) && ! std::isnan (ppq0) && bpmNow > 0.0)
    {
        syncBase = (juce::int64) r->hitIndex + (juce::int64) std::llround ((ppq0 - hit) * 60.0 / bpmNow * sr);
        sync = true;
    }

    float* outL = buffer.getWritePointer (0);
    float* outR = nOut > 1 ? buffer.getWritePointer (1) : nullptr;
    float pk = 0.0f;
    const int len = rOk ? r->size() : 0;
    for (int i = 0; i < n; ++i)
    {
        // ¿qué muestra del reverse toca ahora?
        juce::int64 idx = -1;
        if (rOk)
        {
            if (previewing.load())
            {
                idx = previewPos++;
                if (previewPos >= len) { previewing = false; previewPos = 0; }
            }
            else if (midiActive)
            {
                if (midiPos >= 0) idx = midiPos;   // negativo: la nota llega más adelante en este bloque
                ++midiPos;
                if (midiPos >= len) midiActive = false;
            }
            else if (sync)
            {
                const juce::int64 k = syncBase + i;
                if (k >= 0 && k < len) idx = k;
            }
        }
        float vl = 0.0f, vr = 0.0f;
        float g = 1.0f;
        if (idx >= 0)
        {
            if (lastIdx < 0 || idx != lastIdx + 1) ramp = idx == 0 ? 1.0f : 0.0f;   // salto en la línea de tiempo: entrada suave
            ramp = std::min (1.0f, ramp + rampStep);
            g = ramp;
            lastIdx = idx;
        }
        else if (lastIdx >= 0 && lastIdx + 1 < len && ramp > 0.0f)
        {
            // se paró o se saltó a mitad de reverse: se apaga en 3 ms en vez de cortar
            idx = ++lastIdx;
            ramp = std::max (0.0f, ramp - rampStep);
            g = ramp;
            if (ramp <= 0.0f) lastIdx = -1;
        }
        else lastIdx = -1;

        if (idx >= 0 && idx < len)
        {
            vl = r->l[(size_t) idx]; vr = r->r[(size_t) idx];
            if (midiActive && ! previewing.load()) { vl *= midiGain; vr *= midiGain; }
        }
        // ducking: la cola se aparta cuando entra la voz después del golpe
        const float inAbs = inL != nullptr ? std::max (std::abs (inL[i]), std::abs (inR[i])) : 0.0f;
        duckEnv += (inAbs > duckEnv ? atkCoef : relCoef) * (inAbs - duckEnv);
        float duckGain = 1.0f;
        if (duck > 0.0f && rOk && idx >= r->hitIndex)
        {
            const float envDb = dsp::linToDb (duckEnv);
            duckGain = 1.0f - duck * dsp::clamp01 ((envDb + 42.0f) / 30.0f);
        }
        outGainSmooth += 0.002f * (outTarget * duckGain - outGainSmooth);
        const float k = g * outGainSmooth;
        const float dl = through && inL != nullptr ? inL[i] : 0.0f;
        const float dr = through && inR != nullptr ? inR[i] : 0.0f;
        if (outR != nullptr)
        {
            outL[i] = dl + vl * k;
            outR[i] = dr + vr * k;
            pk = std::max (pk, std::max (std::abs (outL[i]), std::abs (outR[i])));
        }
        else
        {
            outL[i] = dl + 0.5f * (vl + vr) * k;
            pk = std::max (pk, std::abs (outL[i]));
        }
    }
    playIdx = (int) lastIdx;

    // escucha de la región original
    if (srcPreviewing.load() && s != nullptr)
    {
        const int a = regA.load(), b = std::min (regB.load(), s->size());
        for (int i = 0; i < n; ++i)
        {
            const int p = a + srcPreviewPos;
            if (p >= b) { srcPreviewing = false; srcPreviewPos = 0; break; }
            outL[i] += s->l[(size_t) p];
            if (outR != nullptr) outR[i] += s->r[(size_t) p];
            ++srcPreviewPos;
        }
        srcPlayIdx = srcPreviewing.load() ? a + srcPreviewPos : -1;
    }
    else srcPlayIdx = -1;
    outPeak = std::max (pk, outPeak.load() * 0.85f);
}

// ---------------------------------------------------------------------------- captura
void ReverseIvoProcessor::toggleCapture()
{
    if (capturing.load()) { stopCapture(); return; }
    {
        std::lock_guard<std::mutex> g (capMutex);
        fifo.reset();
        capL.clear(); capR.clear();
        capPpq = std::numeric_limits<double>::quiet_NaN();
        capSawPlay = false;
    }
    capSeconds = 0.0;
    stopSeenMs = 0;
    capturing = true;
}

void ReverseIvoProcessor::run()
{
    juce::AudioBuffer<float> tmp (2, 8192);
    while (! threadShouldExit())
    {
        if (capturing.load())
        {
            std::lock_guard<std::mutex> g (capMutex);
            CaptureFifo::Header h;
            while (fifo.pop (h, tmp))
            {
                if ((double) capL.size() >= maxCaptureSeconds * sr) continue;
                if (h.playing && ! std::isnan (h.ppq) && std::isnan (capPpq))
                {
                    // posición musical de la primera muestra capturada
                    capPpq = h.ppq - (double) capL.size() / sr * (bpm.load() > 0 ? bpm.load() / 60.0 : 2.0);
                    capSawPlay = true;
                }
                capL.insert (capL.end(), tmp.getReadPointer (0), tmp.getReadPointer (0) + h.n);
                capR.insert (capR.end(), tmp.getReadPointer (1), tmp.getReadPointer (1) + h.n);
            }
            capSeconds = (double) capL.size() / sr;
        }
        wait (15);
    }
}

void ReverseIvoProcessor::stopCapture()
{
    if (! capturing.exchange (false)) return;
    auto src = std::make_shared<Source>();
    double ppqStart;
    {
        std::lock_guard<std::mutex> g (capMutex);
        CaptureFifo::Header h; juce::AudioBuffer<float> tmp (2, 8192);
        while (fifo.pop (h, tmp))
        {
            capL.insert (capL.end(), tmp.getReadPointer (0), tmp.getReadPointer (0) + h.n);
            capR.insert (capR.end(), tmp.getReadPointer (1), tmp.getReadPointer (1) + h.n);
        }
        src->l = std::move (capL); src->r = std::move (capR);
        capL.clear(); capR.clear();
        ppqStart = capSawPlay ? capPpq : std::numeric_limits<double>::quiet_NaN();
    }
    src->sampleRate = sr;
    if (src->size() < (int) (0.05 * sr)) return;   // nada útil
    setSource (src, ppqStart, "Capture " + juce::Time::getCurrentTime().formatted ("%H:%M:%S"));
}

void ReverseIvoProcessor::setSource (std::shared_ptr<Source> s, double ppqAtStart, const juce::String& name, bool keepRegion)
{
    auto peaks = computePeaks (*s, 1200);
    {
        const juce::SpinLock::ScopedLockType l (resultLock);
        if (source != nullptr) retired.push_back ({ juce::Time::getMillisecondCounter(), source });
        source = s;
        srcPeaks = std::move (peaks);
    }
    sourceName = name;
    sourcePpq = ppqAtStart;
    if (! keepRegion) autoRegion();
    if (! keepRegion && ! std::isnan (ppqAtStart) && bpm.load() > 0.0)
    {
        // el golpe va donde empieza la frase capturada: el reverse entra justo en ella
        double h = ppqAtStart + (double) regA.load() / sr * bpm.load() / 60.0;
        const double grid = 0.25, snapped = std::round (h / grid) * grid;
        if (std::abs (snapped - h) * 60.0 / bpm.load() < 0.045) h = snapped;   // a la semicorchea si está a < 45 ms
        hitPpq = h;
    }
    saveSourceAsync();
    requestRender();
}

void ReverseIvoProcessor::autoRegion()
{
    auto s = getSource();
    if (s == nullptr) return;
    const auto reg = analysis::detectFirstWord (s->l, s->r, s->sampleRate);
    regA = reg.start; regB = std::max (reg.start + 1, reg.end);
    dirty = true; dirtySince = juce::Time::getMillisecondCounter();
}

void ReverseIvoProcessor::setRegion (int a, int b)
{
    auto s = getSource();
    if (s == nullptr) return;
    a = juce::jlimit (0, s->size() - 1, a);
    b = juce::jlimit (a + (int) (0.01 * s->sampleRate), s->size(), b);
    if (a == regA.load() && b == regB.load()) return;
    regA = a; regB = b;
    dirty = true; dirtySince = juce::Time::getMillisecondCounter();
}

std::vector<float> ReverseIvoProcessor::computePeaks (const Source& s, int bins)
{
    std::vector<float> p ((size_t) bins, 0.0f);
    const int n = s.size();
    if (n == 0) return p;
    for (int i = 0; i < n; ++i)
    {
        const int k = (int) ((juce::int64) i * bins / n);
        p[(size_t) k] = std::max (p[(size_t) k], 0.5f * (std::abs (s.l[(size_t) i]) + std::abs (s.r[(size_t) i])));
    }
    return p;
}

// ---------------------------------------------------------------------------- generación
Tempo ReverseIvoProcessor::currentTempo() const
{
    Tempo t;
    t.bpm = bpm.load();
    t.beatsPerBar = (double) std::max (1, sigNum.load()) * 4.0 / (double) std::max (1, sigDen.load());
    return t;
}

void ReverseIvoProcessor::requestRender()
{
    auto s = getSource();
    if (s == nullptr) return;
    Engine::Job j;
    j.source = s; j.regionStart = regA.load(); j.regionEnd = regB.load();
    j.settings = readSettings (apvts);
    j.tempo = currentTempo();
    j.generation = ++generation;
    renderedBpm = j.tempo.bpm; renderedBeats = j.tempo.beatsPerBar;
    dirty = false;
    engine.request (std::move (j));
}

void ReverseIvoProcessor::generate() { requestRender(); }

void ReverseIvoProcessor::publish (ResultPtr r)
{
    const juce::SpinLock::ScopedLockType l (resultLock);
    if (current != nullptr) retired.push_back ({ juce::Time::getMillisecondCounter(), current });
    current = std::move (r);
}

void ReverseIvoProcessor::timerCallback()
{
    if (auto r = engine.take())
        if (r->generation >= publishedGeneration) { publishedGeneration = r->generation; publish (r); }

    const bool autoMode = apvts.getRawParameterValue (id::genMode)->load() < 0.5f;
    if (dirty.load() && autoMode && juce::Time::getMillisecondCounter() - dirtySince > 140) requestRender();

    // Smart Sync: si cambia el tempo o el compás del proyecto, el reverse se adapta solo
    const auto st = readSettings (apvts);
    if (st.smartSync && st.durationIndex < 5 && getSource() != nullptr && ! engine.busy())
    {
        const auto t = currentTempo();
        if (t.bpm > 0.0 && (std::abs (t.bpm - renderedBpm) > 0.01 || std::abs (t.beatsPerBar - renderedBeats) > 1e-6)) requestRender();
    }

    // la captura se cierra sola al parar el transporte (o al llegar al máximo)
    if (capturing.load())
    {
        bool sawPlay; { std::lock_guard<std::mutex> g (capMutex); sawPlay = capSawPlay; }
        if (sawPlay && ! isPlaying.load())
        {
            if (stopSeenMs == 0) stopSeenMs = juce::Time::getMillisecondCounter();
            else if (juce::Time::getMillisecondCounter() - stopSeenMs > 350) stopCapture();
        }
        else stopSeenMs = 0;
        if (capSeconds.load() >= maxCaptureSeconds) stopCapture();
    }

    // lo retirado se libera aquí (nunca en el hilo de audio)
    const auto now = juce::Time::getMillisecondCounter();
    std::vector<std::shared_ptr<const void>> toFree;
    {
        const juce::SpinLock::ScopedLockType l (resultLock);
        for (auto it = retired.begin(); it != retired.end();)
            if (now - it->first > 3000) { toFree.push_back (std::move (it->second)); it = retired.erase (it); }
            else ++it;
    }
    toFree.clear();
}

// ---------------------------------------------------------------------------- escucha
void ReverseIvoProcessor::setPreview (bool p)
{
    previewPos = 0;
    if (p) srcPreviewing = false;
    previewing = p && getResult() != nullptr;
}
void ReverseIvoProcessor::setSourcePreview (bool p)
{
    srcPreviewPos = 0;
    if (p) previewing = false;
    srcPreviewing = p && getSource() != nullptr;
}

// ---------------------------------------------------------------------------- golpe
void ReverseIvoProcessor::nudgeHit (double q)
{
    double h = hitPpq.load();
    if (std::isnan (h)) h = std::isnan (lastPpq.load()) ? 4.0 : std::round (lastPpq.load());
    hitPpq = std::max (0.0, h + q);
}
void ReverseIvoProcessor::setHitAtPlayhead()
{
    const double p = lastPpq.load();
    if (std::isnan (p)) return;
    hitPpq = std::max (0.0, std::round (p / 0.25) * 0.25);
}

juce::String ReverseIvoProcessor::hitText() const
{
    const double h = hitPpq.load();
    if (std::isnan (h)) return "HIT  -";
    const double bpb = currentTempo().beatsPerBar;
    const int bar = (int) std::floor (h / bpb) + 1;
    const double inBar = h - (bar - 1) * bpb;
    const double beatLen = 4.0 / std::max (1, sigDen.load());
    const int beat = (int) std::floor (inBar / beatLen + 1e-6) + 1;
    const double sixteenth = std::fmod (inBar, beatLen) / 0.25;
    juce::String s = "HIT  " + juce::String (bar) + "." + juce::String (beat);
    if (sixteenth > 0.01) s << "." << juce::String ((int) std::round (sixteenth) + 1);
    return s;
}

juce::String ReverseIvoProcessor::durationText() const
{
    const auto st = readSettings (apvts);
    const auto t = currentTempo();
    static const char* names[] = { "1/4 BAR", "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "FREE" };
    const double secs = st.durationSeconds (t.bpm, t.beatsPerBar);
    juce::String name = (st.durationIndex < 5 && (! st.smartSync || t.bpm <= 0)) ? juce::String ("FREE") : juce::String (names[juce::jlimit (0, 5, st.durationIndex)]);
    return name + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (secs, 3) + " s";
}

juce::String ReverseIvoProcessor::tempoText() const
{
    if (bpm.load() <= 0.0) return "NO TEMPO";
    return juce::String (bpm.load(), std::abs (bpm.load() - std::round (bpm.load())) < 0.005 ? 0 : 2) + juce::String::fromUTF8 (" BPM \xc2\xb7 ")
           + juce::String (sigNum.load()) + "/" + juce::String (sigDen.load());
}

juce::String ReverseIvoProcessor::statusText() const
{
    if (capturing.load()) return "REC  " + juce::String (capSeconds.load(), 1) + " s";
    if (getSource() == nullptr) return "CAPTURE OR IMPORT A VOCAL";
    if (engine.busy()) return juce::String::fromUTF8 ("GENERATING\xe2\x80\xa6");
    if (dirty.load()) return "CHANGES PENDING";
    if (getResult() != nullptr) return "READY  " + juce::String (engine.lastMs() / 1000.0, 2) + " s";
    return {};
}

// ---------------------------------------------------------------------------- archivos
void ReverseIvoProcessor::importFile (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr) return;
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 60.0));
    if (n <= 0) return;
    juce::AudioBuffer<float> b ((int) std::max (1u, rd->numChannels), n);
    rd->read (&b, 0, n, 0, true, true);
    auto s = std::make_shared<Source>();
    s->sampleRate = rd->sampleRate;
    s->l.assign (b.getReadPointer (0), b.getReadPointer (0) + n);
    const int rc = b.getNumChannels() > 1 ? 1 : 0;
    s->r.assign (b.getReadPointer (rc), b.getReadPointer (rc) + n);
    std::shared_ptr<Source> use = std::abs (s->sampleRate - sr) > 0.5 ? resampled (*s, sr) : s;
    setSource (use, std::numeric_limits<double>::quiet_NaN(), f.getFileNameWithoutExtension());
}

void ReverseIvoProcessor::saveSourceAsync()
{
    auto s = getSource();
    if (s == nullptr) return;
    auto dir = ivoFolder ("Capturas");
    dir.createDirectory();
    sourceFile = dir.getChildFile ("source-" + juce::Uuid().toString().substring (0, 12) + ".wav");
    const auto file = sourceFile;
    juce::Thread::launch ([s, file]
    {
        juce::WavAudioFormat wav;
        if (auto os = file.createOutputStream())
        {
            std::unique_ptr<juce::OutputStream> stream (os.release());
            if (auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (s->sampleRate).withNumChannels (2).withBitsPerSample (32)
                                                          .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)))
            {
                const float* ch[2] = { s->l.data(), s->r.data() };
                w->writeFromFloatArrays (ch, 2, s->size());
            }
        }
    });
}

void ReverseIvoProcessor::loadSourceFile (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr) return;
    const int n = (int) rd->lengthInSamples;
    juce::AudioBuffer<float> b (2, n);
    rd->read (&b, 0, n, 0, true, true);
    auto s = std::make_shared<Source>();
    s->sampleRate = rd->sampleRate;
    s->l.assign (b.getReadPointer (0), b.getReadPointer (0) + n);
    s->r.assign (b.getReadPointer (1), b.getReadPointer (1) + n);
    auto peaks = computePeaks (*s, 1200);
    const juce::SpinLock::ScopedLockType l (resultLock);
    source = s; srcPeaks = std::move (peaks);
}

juce::File ReverseIvoProcessor::defaultExportFile() const
{
    auto dir = ivoFolder ("Exports");
    dir.createDirectory();
    const auto st = readSettings (apvts);
    static const char* names[] = { "quarter-bar", "half-bar", "1bar", "2bars", "4bars", "free" };
    juce::String name = "reverseIVO " + juce::File::createLegalFileName (presetName) + " "
                        + (bpm.load() > 0 ? juce::String (juce::roundToInt (bpm.load())) + "bpm " : juce::String())
                        + names[juce::jlimit (0, 5, st.durationIndex)];
    return dir.getNonexistentChildFile (name, ".wav", false);
}

bool ReverseIvoProcessor::exportWav (const juce::File& file, juce::String& error)
{
    auto r = getResult();
    if (r == nullptr) { error = "Genera un reverse primero."; return false; }
    file.deleteFile();
    auto os = file.createOutputStream();
    if (os == nullptr) { error = "No se puede escribir en " + file.getFullPathName(); return false; }
    // marca (cue) en el golpe: algunos DAW la muestran al importar
    const auto opts = juce::AudioFormatWriterOptions().withSampleRate (r->sampleRate).withNumChannels (2).withBitsPerSample (24)
                          .withMetadata ("NumCuePoints", "1").withMetadata ("Cue0Identifier", "1")
                          .withMetadata ("Cue0Offset", juce::String (r->hitIndex))
                          .withMetadata ("NumCueLabels", "1").withMetadata ("CueLabel0Identifier", "1").withMetadata ("CueLabel0Text", "HIT");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (os.release());
    auto w = wav.createWriterFor (stream, opts);
    if (w == nullptr) { error = "No se pudo crear el WAV."; return false; }
    const float* ch[2] = { r->l.data(), r->r.data() };
    // el WAV lleva el reverse con el nivel de OUTPUT aplicado (lo que suena es lo que se exporta)
    const float g = dsp::dbToLin (apvts.getRawParameterValue (id::output)->load());
    if (std::abs (g - 1.0f) < 1e-4f) return w->writeFromFloatArrays (ch, 2, r->size());
    std::vector<float> l (r->l), rr (r->r);
    for (auto& v : l) v = juce::jlimit (-1.0f, 1.0f, v * g);
    for (auto& v : rr) v = juce::jlimit (-1.0f, 1.0f, v * g);
    const float* ch2[2] = { l.data(), rr.data() };
    return w->writeFromFloatArrays (ch2, 2, r->size());
}

juce::File ReverseIvoProcessor::exportForDrag()
{
    auto f = defaultExportFile();
    juce::String err;
    return exportWav (f, err) ? f : juce::File();
}

// ---------------------------------------------------------------------------- presets
void ReverseIvoProcessor::loadPreset (int index)
{
    const auto& all = presets.all();
    if (index < 0 || index >= (int) all.size()) return;
    undo.beginNewTransaction();
    presets.apply (apvts, index);
    presetName = all[(size_t) index].name; presetCategory = all[(size_t) index].category;
    requestRender();   // un preset siempre se escucha al momento (también en MANUAL)
}
bool ReverseIvoProcessor::saveUserPreset (const juce::String& name)
{
    if (! presets.saveUser (apvts, name)) return false;
    presetName = name.trim(); presetCategory = "USER";
    return true;
}
void ReverseIvoProcessor::resetParameters()
{
    undo.beginNewTransaction();
    PresetBank::resetToDefaults (apvts);
    presetName = "Init"; presetCategory = {};
    requestRender();
}

// ---------------------------------------------------------------------------- estado
void ReverseIvoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    xml->setAttribute ("sourceFile", sourceFile.getFullPathName());
    xml->setAttribute ("sourceName", sourceName);
    xml->setAttribute ("regA", regA.load());
    xml->setAttribute ("regB", regB.load());
    if (! std::isnan (hitPpq.load())) xml->setAttribute ("hitPpq", hitPpq.load());
    if (! std::isnan (sourcePpq)) xml->setAttribute ("sourcePpq", sourcePpq);
    xml->setAttribute ("preset", presetName);
    xml->setAttribute ("presetCategory", presetCategory);
    xml->setAttribute ("uiScale", (double) uiScale.load());
    copyXmlToBinary (*xml, dest);
}

void ReverseIvoProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    presetName = xml->getStringAttribute ("preset", "Init");
    presetCategory = xml->getStringAttribute ("presetCategory");
    sourceName = xml->getStringAttribute ("sourceName");
    uiScale = (float) xml->getDoubleAttribute ("uiScale", 1.0);
    hitPpq = xml->hasAttribute ("hitPpq") ? xml->getDoubleAttribute ("hitPpq") : std::numeric_limits<double>::quiet_NaN();
    sourcePpq = xml->hasAttribute ("sourcePpq") ? xml->getDoubleAttribute ("sourcePpq") : std::numeric_limits<double>::quiet_NaN();
    const juce::File f (xml->getStringAttribute ("sourceFile"));
    if (f.existsAsFile())
    {
        loadSourceFile (f);
        sourceFile = f;
        regA = xml->getIntAttribute ("regA"); regB = xml->getIntAttribute ("regB");
        auto s = getSource();
        if (s != nullptr && std::abs (s->sampleRate - sr) > 0.5)
        {
            const double k = sr / s->sampleRate;
            auto rs = resampled (*s, sr);
            regA = (int) (regA.load() * k); regB = (int) (regB.load() * k);
            const juce::SpinLock::ScopedLockType l (resultLock);
            source = rs; srcPeaks = computePeaks (*rs, 1200);
        }
        requestRender();
    }
    dirty = false;
}

juce::AudioProcessorEditor* ReverseIvoProcessor::createEditor() { return new ReverseIvoEditor (*this); }
} // namespace reverseivo

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new reverseivo::ReverseIvoProcessor(); }
