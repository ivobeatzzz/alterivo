// levelIVO — RESULTADO publicado para el hilo de audio y la interfaz (inmutable una vez creado).
// El hilo de audio sólo lee: original, limpio y las dos curvas de ganancia muestra a muestra.
#pragma once
#include "../analysis/Analysis.h"
#include "../cleanup/Cleaner.h"
#include "../leveling/Leveler.h"
#include "../dsp/Dsp.h"

namespace levelivo
{
enum class Listen { original = 0, level = 1, clean = 2, final_ = 3 };

struct Result
{
    TakePtr take;
    AnalysisPtr analysis;
    std::shared_ptr<const CleanResult> clean;
    std::shared_ptr<const LevelResult> level;
    std::vector<float> levelLin, cleanLin;     // por muestra
    float comp[4] = { 1, 1, 1, 1 };            // compensación de volumen por modo de escucha (igualado al original)
    int version = 0;

    // Una muestra de salida para la posición i de la toma y el canal c
    inline float sample (Listen l, int c, int i) const
    {
        switch (l)
        {
            case Listen::original: return take->audio.getSample (c, i);
            case Listen::level:    return take->audio.getSample (c, i) * levelLin[(size_t) i];
            case Listen::clean:    return clean->cleaned.getSample (c, i) * cleanLin[(size_t) i];
            case Listen::final_:   default: return clean->cleaned.getSample (c, i) * cleanLin[(size_t) i] * levelLin[(size_t) i];
        }
    }
};
using ResultPtr = std::shared_ptr<const Result>;

namespace render
{
// Curva por trama (dB) a ganancia lineal por muestra (interpolación entre centros de trama)
inline std::vector<float> framesToLinear (const std::vector<float>& db, int hop, int n)
{
    std::vector<float> out ((size_t) n, 1.0f);
    const int F = (int) db.size();
    if (F == 0) return out;
    for (int i = 0; i < n; ++i)
    {
        const float pos = (i - hop * 0.5f) / (float) hop;
        int f0 = (int) std::floor (pos);
        const float t = pos - f0;
        f0 = std::clamp (f0, 0, F - 1);
        const int f1 = std::min (F - 1, f0 + 1);
        const float v = pos < 0 ? db[0] : db[(size_t) f0] + t * (db[(size_t) f1] - db[(size_t) f0]);
        out[(size_t) i] = dsp::dbToLin (v);
    }
    return out;
}
// Pico por trama (dB) de audio * ganancia
inline std::vector<float> framePeaks (const juce::AudioBuffer<float>& a, int hop, int F)
{
    std::vector<float> pk ((size_t) F, -120.0f);
    for (int f = 0; f < F; ++f)
    {
        float m = 0;
        for (int c = 0; c < a.getNumChannels(); ++c)
        {
            const float* p = a.getReadPointer (c) + f * hop;
            const int len = std::min (hop, a.getNumSamples() - f * hop);
            for (int i = 0; i < len; ++i) m = std::max (m, std::abs (p[i]));
        }
        pk[(size_t) f] = dsp::linToDb (m);
    }
    return pk;
}
// Sonoridad integrada (K) de las tramas con voz de una señal mono
inline float activeLoudness (const std::vector<float>& mono, double sr, const Analysis& A, const std::vector<float>* gainDb = nullptr)
{
    std::vector<float> k = mono;
    dsp::runChain ({ dsp::Biquad::kShelf (sr), dsp::Biquad::kHighpass (sr) }, k);
    const auto L = dsp::framePowerDb (k, A.hop, 2 * A.hop, A.nFrames);
    double s = 0; int c = 0;
    for (int f = 0; f < A.nFrames; ++f)
        if (A.active[(size_t) f]) { s += std::pow (10.0, (L[(size_t) f] + (gainDb ? (*gainDb)[(size_t) f] : 0.0f)) / 10.0); ++c; }
    return c ? dsp::powToDb (s / c) : -120.0f;
}
} // namespace render
} // namespace levelivo
