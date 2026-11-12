// levelIVO — prueba del plugin completo como lo usaría un DAW: captura con posición, análisis automático al parar,
// reproducción sincronizada, nulo, estado guardado/recargado, modo tiempo real y captura de la interfaz.
#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include <cstdio>

using namespace levelivo;

struct PH : juce::AudioPlayHead
{
    juce::int64 pos = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setTimeInSamples (pos); p.setBpm (124.0); return p; }
};
static std::vector<float> loadRaw (const char* path)
{
    std::vector<float> v; FILE* f = std::fopen (path, "rb"); if (! f) return v; float x;
    while (std::fread (&x, 4, 1, f) == 1) v.push_back (x);
    std::fclose (f); return v;
}
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
static void setP (LevelIvoProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }

// toca el clip [0, n) desde la posición 'start' del DAW
static juce::AudioBuffer<float> play (LevelIvoProcessor& p, PH& ph, const std::vector<float>& x, juce::int64 start, const std::vector<float>* beat = nullptr, int bs = 512)
{
    const int n = (int) x.size();
    juce::AudioBuffer<float> out (2, n);
    juce::MidiBuffer midi;
    const int nIn = p.getTotalNumInputChannels();
    for (int s = 0; s < n; s += bs)
    {
        const int m = std::min (bs, n - s);
        juce::AudioBuffer<float> blk (std::max (2, nIn), m); blk.clear();
        for (int i = 0; i < m; ++i)
        {
            blk.setSample (0, i, x[(size_t) (s + i)]); blk.setSample (1, i, x[(size_t) (s + i)]);
            if (beat && nIn >= 4) { blk.setSample (2, i, (*beat)[(size_t) (s + i)]); blk.setSample (3, i, (*beat)[(size_t) (s + i)]); }
        }
        ph.pos = start + s;
        p.processBlock (blk, midi);
        if ((s / bs) % 16 == 0) juce::Thread::sleep (1);   // como en un DAW: el hilo de guardado tiene tiempo de vaciar la cola
        for (int c = 0; c < 2; ++c) out.copyFrom (c, s, blk, c, 0, m);
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) return 1;
    auto x = loadRaw (argv[1]);
    float pk = 0; for (float v : x) pk = std::max (pk, std::abs (v));
    for (auto& v : x) v *= 0.5f / pk;
    const int n = (int) x.size();
    const juce::int64 start = 48000 * 30;   // el clip empieza en el segundo 30 del proyecto

    auto p = std::make_unique<LevelIvoProcessor>();
    PH ph; p->setPlayHead (&ph);
    p->setPlayConfigDetails (2, 2, 48000, 512);
    p->prepareToPlay (48000, 512);
    std::printf ("Latencia offline: %d muestras\n", p->getLatencySamples());

    // 1) Captura y análisis automático al parar
    p->startCapture();
    play (*p, ph, x, start);
    ph.playing = false;
    { juce::AudioBuffer<float> b (2, 512); b.clear(); juce::MidiBuffer m; p->processBlock (b, m); }
    for (int i = 0; i < 200 && (p->isCapturing() || p->engine.busy() || p->getResult() == nullptr); ++i) pump (50);
    auto r = p->getResult();
    if (r == nullptr) { std::printf ("ERROR: sin resultado\n"); return 1; }
    std::printf ("Captura: %.1f s en %zu región(es), inicio en el DAW %lld · estado: %s\n", r->take->numSamples() / 48000.0, r->take->regions.size(),
                 (long long) r->take->regions[0].hostStart, p->statusText().toRawUTF8());
    std::printf ("Resultado: %zu palabras, corrección media %.2f dB, dispersión %.2f -> %.2f dB\n", r->analysis->words.size(), r->level->meanAbsCorr, r->level->spreadBefore, r->level->spreadAfter);

    // 2) Revisar antes: suena el original hasta APLICAR
    ph.playing = true;
    auto o1 = play (*p, ph, x, start);
    double d = 0; for (int i = 0; i < n; ++i) d = std::max (d, (double) std::abs (o1.getSample (0, i) - x[(size_t) i]));
    std::printf ("Antes de aplicar (escucha ORIGINAL): diferencia máx. %.2e\n", d);
    p->apply();
    pump (100);
    auto o2 = play (*p, ph, x, start);
    double e = 0, s2 = 0;
    const float comp = r->comp[3];
    for (int i = 0; i < std::min (n, r->take->numSamples()); ++i) { const double ref = r->sample (Listen::final_, 0, i) * comp; e += std::pow (o2.getSample (0, i) - ref, 2); s2 += ref * ref; }
    std::printf ("Aplicado: salida frente al resultado calculado %.1f dB (debe ser muy bajo, salvo fundidos de 5 ms en los bordes)\n", 10 * std::log10 (e / s2 + 1e-30));

    // 3) Si el clip cambia (otro audio en esa posición), se deja pasar sin procesar
    {
        std::vector<float> other (x.size()); for (int i = 0; i < n; ++i) other[(size_t) i] = x[(size_t) ((i + 48000 * 5) % n)] * 0.3f;
        play (*p, ph, other, start);
        std::printf ("Clip cambiado detectado: %s\n", p->trackChanged() ? "sí" : "no");
    }

    // 4) Estado: guardar y recargar en otra instancia
    {
        p->addEdit (Edit::offset, 48000, 96000, 3.0f);
        pump (300);
        juce::MemoryBlock mb; p->getStateInformation (mb);
        auto q = std::make_unique<LevelIvoProcessor>();
        q->setPlayConfigDetails (2, 2, 48000, 512); q->prepareToPlay (48000, 512);
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        for (int i = 0; i < 200 && (q->getResult() == nullptr || q->engine.busy()); ++i) pump (50);
        auto r2 = q->getResult();
        std::printf ("Recarga del proyecto: %s · ediciones %zu · aplicado %s\n", r2 ? "toma recuperada" : "SIN TOMA", q->getEdits().size(), q->isApplied() ? "sí" : "no");
    }

    // 5) Nulo: todo apagado
    {
        auto q = std::make_unique<LevelIvoProcessor>();
        PH ph2; q->setPlayHead (&ph2);
        q->setPlayConfigDetails (2, 2, 48000, 512); q->prepareToPlay (48000, 512);
        for (auto* id : { id::noiseOn, id::bleedOn, id::otherOn, id::hitsOn, id::mouthOn, id::breathOn, id::clipRepair, id::peakProtect, id::matchLoud }) setP (*q, id, 0.0f);
        setP (*q, id::intensity, 0.0f); setP (*q, id::autoApply, 1.0f);
        q->startCapture(); play (*q, ph2, x, 0); ph2.playing = false;
        { juce::AudioBuffer<float> b (2, 512); b.clear(); juce::MidiBuffer m; q->processBlock (b, m); }
        for (int i = 0; i < 200 && (q->isCapturing() || q->engine.busy() || q->getResult() == nullptr); ++i) pump (50);
        pump (500);
        ph2.playing = true;
        auto o = play (*q, ph2, x, 0);
        double en = 0, sn = 0; for (int i = 0; i < n; ++i) { en += std::pow (o.getSample (0, i) - x[(size_t) i], 2); sn += (double) x[(size_t) i] * x[(size_t) i]; }
        std::printf ("Nulo del plugin (todo apagado, aplicado): %.1f dB\n", 10 * std::log10 (en / sn + 1e-30));
    }

    // 6) Tiempo real: latencia y CPU
    {
        setP (*p, id::engine, 1.0f); pump (100);
        p->prepareToPlay (48000, 512);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        play (*p, ph, x, start);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        std::printf ("Tiempo real: latencia %d muestras (%.0f ms) · %.1f %% de un núcleo\n", p->getLatencySamples(), p->getLatencySamples() / 48.0, 100.0 * (t1 - t0) / (n / 48.0));
        setP (*p, id::engine, 0.0f); pump (100); p->prepareToPlay (48000, 512);
    }

    // 7) CPU de la reproducción offline
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        play (*p, ph, x, start);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        std::printf ("Reproducción offline: %.2f %% de un núcleo\n", 100.0 * (t1 - t0) / (n / 48.0));
    }

    // 8) Interfaz
    if (argc > 2)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
        ed->setSize (1200, 820);
        pump (400);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f (argv[2]); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
        std::printf ("Interfaz guardada en %s\n", argv[2]);
        // con una palabra seleccionada
        if (argc > 3)
        {
            ed->setSize (1200, 820);
            auto* view = ed->getChildComponent (0);
            juce::Component* wave = nullptr;
            for (auto* c : view->getChildren()) if (dynamic_cast<levelivo::ui::WaveView*> (c)) wave = c;
            if (wave)
            {
                auto pos = juce::Point<float> (wave->getWidth() * 0.45f, wave->getHeight() * 0.5f);
                juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(), pos, {}, 0.0f, 0, 0, 0, 0, wave, wave, juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, false);
                wave->mouseDown (me); wave->mouseUp (me);
                // zoom a una zona
                juce::MouseWheelDetails w {}; w.deltaY = 1.0f;
                for (int k = 0; k < 3; ++k) wave->mouseWheelMove (me, w);
            }
            pump (300);
            auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            juce::File f2 (argv[3]); f2.deleteFile();
            juce::FileOutputStream os2 (f2); png.writeImageToStream (img2, os2);
        }
    }
    p->setPlayHead (nullptr);
    return 0;
}
