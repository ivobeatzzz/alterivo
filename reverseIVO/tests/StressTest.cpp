// reverseIVO — PRUEBA DE ESTABILIDAD: lo que hace un DAW al abrir y cerrar proyectos y plugins.
//  · Abrir el plugin, cerrarlo del todo (JUCE se apaga como cuando el DAW cierra la última instancia) y volver a abrirlo.
//  · Varias instancias a la vez, editores abiertos y cerrados muchas veces.
//  · Quitar el plugin mientras está generando (el hilo se tiene que parar en milisegundos).
//  · Cargar un proyecto antes de que el DAW diga la frecuencia de muestreo.
// Se compila con AddressSanitizer para que cualquier acceso a memoria liberada salte.
#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "Voice.h"
#include <cstdio>

using namespace reverseivo;

struct Check { int fails = 0; void expect (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "OK   " : "FALLA", what.toRawUTF8()); if (! ok) ++fails; } };

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static juce::MemoryBlock makeProject()
{
    ReverseIvoProcessor p;
    p.setRateAndBufferSizeDetails (48000.0, 512); p.prepareToPlay (48000.0, 512);
    juce::TemporaryFile tmp (".wav");
    {
        const auto v = makeVoice (48000.0);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (tmp.getFile().createOutputStream().release());
        if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (24)))
        { const float* ch[2] = { v.l.data(), v.r.data() }; w->writeFromFloatArrays (ch, 2, (int) v.l.size()); }
    }
    p.importFile (tmp.getFile());
    for (int i = 0; i < 300 && (p.getResult() == nullptr || p.isGenerating()); ++i) pump (20);
    juce::MemoryBlock mb; p.getStateInformation (mb);
    return mb;
}

static void openEditorAndPaint (ReverseIvoProcessor& p)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    for (int v = 0; v < 3; ++v)
    {
        dynamic_cast<ReverseIvoEditor*> (ed.get())->getView().showView (v, v);
        pump (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::ignoreUnused (img);
    }
}

int main()
{
    Check c;
    juce::MemoryBlock project;

    std::printf ("\n== Abrir, cerrar del todo y volver a abrir (como el DAW al quitar y poner el plugin)\n");
    for (int round = 0; round < 3; ++round)
    {
        juce::ScopedJuceInitialiser_GUI gui;   // al salir de aquí JUCE se apaga, igual que al cerrar la última instancia
        if (project.isEmpty()) project = makeProject();
        ReverseIvoProcessor p;
        p.setStateInformation (project.getData(), (int) project.getSize());
        p.setRateAndBufferSizeDetails (44100.0, 256); p.prepareToPlay (44100.0, 256);
        openEditorAndPaint (p);
        openEditorAndPaint (p);
        for (int i = 0; i < 300 && (p.getResult() == nullptr || p.isGenerating()); ++i) pump (20);
        c.expect (p.getResult() != nullptr, "ronda " + juce::String (round + 1) + ": abre el proyecto, pinta la interfaz y genera");
    }

    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("\n== Varias instancias y editores\n");
    {
        std::vector<std::unique_ptr<ReverseIvoProcessor>> procs;
        for (int i = 0; i < 6; ++i)
        {
            procs.push_back (std::make_unique<ReverseIvoProcessor>());
            procs.back()->setStateInformation (project.getData(), (int) project.getSize());
            procs.back()->setRateAndBufferSizeDetails (48000.0, 128); procs.back()->prepareToPlay (48000.0, 128);
        }
        for (int k = 0; k < 4; ++k) for (auto& p : procs) openEditorAndPaint (*p);
        procs.clear();
        c.expect (true, "6 instancias con editores abiertos y cerrados 4 veces cada una");
    }

    std::printf ("\n== Quitar el plugin mientras genera\n");
    {
        double worst = 0;
        for (int i = 0; i < 4; ++i)
        {
            auto p = std::make_unique<ReverseIvoProcessor>();
            p->setRateAndBufferSizeDetails (96000.0, 512); p->prepareToPlay (96000.0, 512);
            p->setStateInformation (project.getData(), (int) project.getSize());
            p->loadPreset (p->presets.indexOf ("Ambient Rise"));   // el más pesado
            pump (150 + i * 200);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            p.reset();
            worst = std::max (worst, juce::Time::getMillisecondCounterHiRes() - t0);
        }
        std::printf ("  el plugin se cierra en %.0f ms como mucho\n", worst);
        c.expect (worst < 1000.0, "se cierra al momento aunque esté generando (sin matar hilos)");
    }

    std::printf ("\n== Proyecto cargado antes de saber la frecuencia (orden de Ableton/Logic)\n");
    {
        ReverseIvoProcessor p;
        p.setStateInformation (project.getData(), (int) project.getSize());   // aún sin prepareToPlay
        p.setRateAndBufferSizeDetails (44100.0, 512); p.prepareToPlay (44100.0, 512);
        for (int i = 0; i < 400 && (p.getResult() == nullptr || p.isGenerating() || p.getResult()->sampleRate != 44100.0); ++i) pump (20);
        c.expect (p.getResult() != nullptr && p.getResult()->sampleRate == 44100.0, "se adapta a 44,1 kHz y genera");
    }

    std::printf ("\n%s (%d fallos)\n", c.fails == 0 ? "TODO OK" : "HAY FALLOS", c.fails);
    return c.fails == 0 ? 0 : 1;
}
