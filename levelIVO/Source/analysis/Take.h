// levelIVO — la toma: audio capturado (o arrastrado), su referencia opcional (el beat del sidechain)
// y la tabla que une cada muestra de la toma con su posición en la línea de tiempo del DAW.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>

namespace levelivo
{
// Tramo continuo de la toma. hostStart = posición en el DAW (muestras); -1 si viene de un archivo arrastrado.
struct Region
{
    juce::int64 hostStart = -1;
    int takeStart = 0;
    int length = 0;
};

struct Take
{
    double sampleRate = 48000.0;
    juce::AudioBuffer<float> audio;      // canales de la voz (1 o 2)
    std::vector<float> reference;        // beat de referencia en mono (vacío si no hay)
    std::vector<Region> regions;         // ordenadas por posición en la toma
    juce::String name;                   // nombre de pista o archivo
    juce::String fileId;                 // archivo guardado en disco (para recargar el proyecto)
    bool fromFile = false;

    int numSamples() const { return audio.getNumSamples(); }
    int numChannels() const { return audio.getNumChannels(); }
    bool hasReference() const { return ! reference.empty(); }

    // Muestra de la toma que corresponde a una posición del DAW (-1 si no está capturada)
    int takeIndexFor (juce::int64 hostPos) const
    {
        // búsqueda binaria por hostStart (las regiones capturadas están ordenadas también por posición del DAW)
        int lo = 0, hi = (int) regions.size() - 1;
        while (lo <= hi)
        {
            const int mid = (lo + hi) / 2;
            const auto& r = regions[(size_t) mid];
            if (hostPos < r.hostStart) hi = mid - 1;
            else if (hostPos >= r.hostStart + r.length) lo = mid + 1;
            else return r.takeStart + (int) (hostPos - r.hostStart);
        }
        return -1;
    }
    juce::int64 hostPosFor (int takeIndex) const
    {
        for (const auto& r : regions)
            if (takeIndex >= r.takeStart && takeIndex < r.takeStart + r.length)
                return r.hostStart < 0 ? -1 : r.hostStart + (takeIndex - r.takeStart);
        return -1;
    }
    // Mezcla mono para el análisis
    std::vector<float> mono() const
    {
        std::vector<float> m ((size_t) numSamples(), 0.0f);
        const int nc = numChannels();
        for (int c = 0; c < nc; ++c)
        {
            const float* p = audio.getReadPointer (c);
            for (int i = 0; i < numSamples(); ++i) m[(size_t) i] += p[i] / (float) nc;
        }
        return m;
    }
};
using TakePtr = std::shared_ptr<const Take>;
} // namespace levelivo
