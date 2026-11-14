// reverseIVO — PRUEBA DEL PLUGIN COMPLETO como lo usa un DAW: captura con posición musical, generación
// automática al parar, golpe exacto en la línea de tiempo, MIDI, escucha previa, estado guardado y recargado,
// cambio de tempo, exportación WAV, todos los presets y capturas de la interfaz.
#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "Voice.h"
#include <cstdio>

using namespace reverseivo;

struct PH : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool playing = true; int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm);
        juce::AudioPlayHead::TimeSignature ts; ts.numerator = num; ts.denominator = den; p.setTimeSignature (ts);
        return p;
    }
};
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
static void setP (ReverseIvoProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static ResultPtr waitResult (ReverseIvoProcessor& p, int prevGen, int ms = 15000)
{
    for (int t = 0; t < ms; t += 20)
    {
        pump (20);
        auto r = p.getResult();
        if (r != nullptr && r->generation > prevGen && ! p.isGenerating() && ! p.hasPendingChanges()) return r;
    }
    return p.getResult();
}

// toca 'in' desde la posición ppq0; devuelve la salida
static juce::AudioBuffer<float> play (ReverseIvoProcessor& p, PH& ph, const std::vector<float>& inL, const std::vector<float>& inR, double ppq0,
                                       juce::MidiBuffer* midiFirst = nullptr, int bs = 480)
{
    const int n = (int) inL.size();
    juce::AudioBuffer<float> out (2, n);
    const double sr = p.getSampleRate();
    for (int s = 0; s < n; s += bs)
    {
        const int m = std::min (bs, n - s);
        juce::AudioBuffer<float> blk (2, m);
        for (int i = 0; i < m; ++i) { blk.setSample (0, i, inL[(size_t) (s + i)]); blk.setSample (1, i, inR[(size_t) (s + i)]); }
        ph.ppq = ppq0 + (double) s / sr * ph.bpm / 60.0;
        juce::MidiBuffer midi;
        if (s == 0 && midiFirst != nullptr) midi = *midiFirst;
        p.processBlock (blk, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, s, blk, c, 0, m);
    }
    return out;
}

struct Check { int fails = 0; void expect (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "OK  " : "FALLA", what.toRawUTF8()); if (! ok) ++fails; } };

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : "/tmp";
    Check c;
    const double sr = 48000.0;
    const Source voice = makeVoice (sr);

    ReverseIvoProcessor p;
    PH ph;
    p.setPlayHead (&ph);
    p.setRateAndBufferSizeDetails (sr, 480);
    p.prepareToPlay (sr, 480);
    setP (p, id::through, 0.0f);   // sólo el reverse en la salida (para medirlo)

    // ---------------------------------------------------------------- 1. captura con transporte
    std::printf ("\n== Captura\n");
    p.toggleCapture();
    pump (50);
    const double capPpq = 16.0;   // compás 5
    play (p, ph, voice.l, voice.r, capPpq);
    ph.playing = false;
    { juce::AudioBuffer<float> b (2, 480); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }
    auto r = waitResult (p, 0);
    c.expect (! p.isCapturing(), "la captura se cierra sola al parar el transporte");
    c.expect (r != nullptr, "genera el reverse automáticamente después de capturar");
    if (r == nullptr) { std::printf ("sin resultado\n"); return 1; }
    const double regionPpq = capPpq + (double) p.regionStart() / sr * 2.0;
    std::printf ("  región %.3f–%.3f s · %s · %s\n", p.regionStart() / sr, p.regionEnd() / sr, p.hitText().toRawUTF8(), p.durationText().toRawUTF8());
    c.expect (p.hasHit(), "coloca el golpe en la línea de tiempo");
    c.expect (r->hitIndex == 96000, "1 compás a 120 BPM = 96000 muestras exactas");

    // ---------------------------------------------------------------- 2. sincronía con la línea de tiempo
    std::printf ("\n== Sincronía\n");
    {
        // golpe: se lee lo que guardó el procesador
        juce::MemoryBlock mb; p.getStateInformation (mb);
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        const double hit = xml->getDoubleAttribute ("hitPpq");
        std::printf ("  golpe en ppq %.4f (inicio de la frase %.4f)\n", hit, regionPpq);
        c.expect (std::abs (hit - regionPpq) < 0.11, "el golpe cae donde empieza la frase capturada");
        ph.playing = true;
        const double from = hit - 6.0;   // empezar 6 negras antes del golpe
        const int n = (int) (sr * 6.0);
        std::vector<float> z ((size_t) n, 0.0f);
        auto out = play (p, ph, z, z, from);
        const int hitSample = (int) std::llround ((hit - from) * 0.5 * sr);
        const int N = r->hitIndex;
        double maxDiff = 0;
        for (int i = 0; i < N; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (out.getSample (0, hitSample - N + i) - r->l[(size_t) i]));
        std::printf ("  diferencia máxima con el reverse generado: %.2e\n", maxDiff);
        c.expect (maxDiff < 2e-3, "la salida es exactamente el reverse, terminando en la muestra del golpe");
        double before = 0; for (int i = hitSample - N - 4800; i < hitSample - N; ++i) before = std::max (before, (double) std::abs (out.getSample (0, i)));
        c.expect (before < 1e-6, "silencio antes de que empiece el reverse");
    }

    // ---------------------------------------------------------------- 3. disparo por MIDI y escucha previa
    std::printf ("\n== MIDI y escucha\n");
    {
        ph.playing = false;
        p.setPreview (false);
        juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 100);
        std::vector<float> z ((size_t) (sr * 0.5), 0.0f);
        auto out = play (p, ph, z, z, 0.0, &midi);
        double d = 0; for (int i = 0; i < 20000; ++i) d = std::max (d, (double) std::abs (out.getSample (0, 100 + i) - r->l[(size_t) i]));
        c.expect (d < 2e-3 && std::abs (out.getSample (0, 99)) < 1e-9, "una nota MIDI dispara el reverse en la muestra exacta");
        for (int k = 0; k < 400; ++k) { juce::AudioBuffer<float> b (2, 480); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }   // termina
        p.setPreview (true);
        auto pv = play (p, ph, z, z, 0.0);
        double e = 0; for (int i = 0; i < pv.getNumSamples(); ++i) e = std::max (e, (double) std::abs (pv.getSample (0, i) - r->l[(size_t) i]));
        c.expect (e < 2e-3, "PREVIEW reproduce el reverse desde el principio");
        p.setPreview (false);
    }

    // ---------------------------------------------------------------- 4. estado guardado y recargado
    std::printf ("\n== Estado\n");
    {
        setP (p, id::intensity, 0.9f); setP (p, id::pitch, -3.0f);
        auto r2 = waitResult (p, r->generation);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        pump (300);   // la fuente se guarda en segundo plano
        ReverseIvoProcessor q; PH ph2; q.setPlayHead (&ph2); q.setRateAndBufferSizeDetails (sr, 480); q.prepareToPlay (sr, 480);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        { juce::AudioBuffer<float> b (2, 480); b.clear(); juce::MidiBuffer m; q.processBlock (b, m); }
        auto r3 = waitResult (q, 0);
        c.expect (r3 != nullptr && r2 != nullptr && r3->l == r2->l, "al reabrir el proyecto se recupera la fuente y el mismo reverse");
        c.expect (q.hitText() == p.hitText(), "y el golpe sigue en el mismo sitio");
        r = r2;
    }

    // ---------------------------------------------------------------- 5. cambio de tempo (Smart Sync)
    std::printf ("\n== Smart Sync\n");
    {
        ph.bpm = 100.0;
        { juce::AudioBuffer<float> b (2, 480); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }
        auto r4 = waitResult (p, r->generation);
        c.expect (r4 != nullptr && r4->hitIndex == (int) std::llround (4 * 0.6 * sr), "al cambiar el tempo del proyecto el reverse se adapta solo (1 compás a 100 BPM)");
        ph.num = 3; ph.den = 4;
        { juce::AudioBuffer<float> b (2, 480); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }
        auto r5 = waitResult (p, r4->generation);
        c.expect (r5 != nullptr && r5->hitIndex == (int) std::llround (3 * 0.6 * sr), "y al compás (3/4)");
        ph.num = 4; ph.bpm = 120.0;
        { juce::AudioBuffer<float> b (2, 480); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }
        r = waitResult (p, r5->generation);
    }

    // ---------------------------------------------------------------- 6. MANUAL conserva el resultado hasta GENERATE
    std::printf ("\n== AUTO / MANUAL\n");
    {
        setP (p, id::genMode, 1.0f);
        const int g0 = r->generation;
        setP (p, id::character, 0.9f);
        pump (600);
        c.expect (p.getResult()->generation == g0 && p.hasPendingChanges(), "en MANUAL el audio generado se conserva al tocar controles");
        p.generate();
        auto r6 = waitResult (p, g0);
        c.expect (r6->generation > g0, "GENERATE lo regenera");
        setP (p, id::genMode, 0.0f);
        r = r6;
    }

    // ---------------------------------------------------------------- 7. exportación
    std::printf ("\n== Exportar\n");
    {
        const juce::File f (outDir + "/reverseIVO-export.wav");
        juce::String err;
        c.expect (p.exportWav (f, err), "exporta WAV " + err);
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        c.expect (rd != nullptr && rd->lengthInSamples == r->size() && rd->bitsPerSample == 24, "el WAV tiene la longitud exacta, 24 bits");
    }

    // ---------------------------------------------------------------- 8. presets
    std::printf ("\n== Presets\n");
    {
        int gen = r->generation;
        for (int i = 0; i < (int) p.presets.all().size(); ++i)
        {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            p.loadPreset (i);
            auto rp = waitResult (p, gen);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            bool ok = rp != nullptr && rp->generation > gen;
            float pk = 0; bool fin = true;
            if (ok) for (int k = 0; k < rp->size(); ++k) { fin &= std::isfinite (rp->l[(size_t) k]); pk = std::max (pk, std::abs (rp->l[(size_t) k])); }
            if (ok) gen = rp->generation;
            std::printf ("  %-22s %-12s %6.0f ms  pico %.2f dBFS\n", p.presets.all()[(size_t) i].name.toRawUTF8(), p.presets.all()[(size_t) i].category.toRawUTF8(), ms, dsp::linToDb (pk));
            c.expect (ok && fin && pk <= dsp::dbToLin (-0.25f), "  genera bien");
            if (ok)
            {
                std::vector<float> l (rp->l), rr (rp->r);
                juce::WavAudioFormat wav;
                auto f = juce::File (outDir + "/preset-" + juce::String (i + 1).paddedLeft ('0', 2) + "-" + juce::File::createLegalFileName (p.presets.all()[(size_t) i].name) + ".wav");
                f.deleteFile();
                std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
                if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
                { const float* ch[2] = { l.data(), rr.data() }; w->writeFromFloatArrays (ch, 2, (int) l.size()); }
            }
        }
        p.loadPreset (p.presets.indexOf ("Crank Riser"));
        waitResult (p, gen);
    }

    // ---------------------------------------------------------------- 9. interfaz
    std::printf ("\n== Interfaz\n");
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* e = dynamic_cast<ReverseIvoEditor*> (ed.get());
        ed->setSize (1350, 1068);   // 1,5x
        struct V { int page, sub; const char* name; } views[] = { { 0, 0, "create" }, { 1, 0, "customize-volume" }, { 1, 1, "customize-pitch" },
            { 1, 2, "customize-filter" }, { 1, 3, "customize-layers" }, { 2, 0, "fx-space" }, { 2, 1, "fx-rhythm" }, { 2, 2, "fx-character" },
            { 2, 3, "fx-stereo" }, { 2, 4, "fx-movement" }, { 2, 5, "fx-smart" } };
        for (auto& v : views)
        {
            e->getView().showView (v.page, v.sub);
            pump (120);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (outDir + "/ui-" + v.name + ".png"); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        }
        c.expect (true, "capturas de la interfaz en " + outDir);
    }

    std::printf ("\n%s (%d fallos)\n", c.fails == 0 ? "TODO OK" : "HAY FALLOS", c.fails);
    p.setPlayHead (nullptr);
    return c.fails == 0 ? 0 : 1;
}
