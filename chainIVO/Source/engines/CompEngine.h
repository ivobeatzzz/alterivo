// COMPRESSOR ENGINE — cinco topologías con comportamiento propio (no sólo otro nombre):
//  FET     : alimentación directa, detector de pico, ataque muy rápido, relevo que depende del programa,
//            ligera distorsión impar sólo mientras comprime.
//  OPTO    : realimentación, detector RMS, ataque fijo ~10 ms, relevo en dos etapas con memoria
//            (cuanto más tiempo comprime, más lento suelta), rodilla muy blanda.
//  VCA     : alimentación directa, detector RMS corto, rodilla dura, limpio y con pegada.
//  VARI-MU : realimentación, el ratio crece con el nivel (rodilla de 15 dB), lento y redondo,
//            armónicos pares suaves.
//  CLEAN   : digital transparente, mezcla pico/RMS, rodilla de 6 dB, sin coloración.
// Estéreo enlazado: la misma reducción en L y R (la imagen no se mueve).
#pragma once
#include "../dsp/DspCore.h"

namespace chainivo
{
enum class CompType { fet = 0, opto, vca, varimu, clean };

struct CompSettings
{
    bool on = true;
    CompType type = CompType::vca;
    float threshold = -18.0f, ratio = 4.0f, attackMs = 5.0f, releaseMs = 120.0f, makeup = 0.0f, mix = 1.0f;
};

class CompEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        makeupS.prepare (sr, 30.0f); mixS.prepare (sr, 30.0f);
        dc.prepare (sr);
        reset();
    }
    void reset()
    {
        gr = 0.0f; rms = 0.0f; memory = 0.0f; fbLevel = 0.0f; first = true; dc.reset();
    }

    void process (float* const* ch, int nch, int n, const CompSettings& s)
    {
        if (first) { makeupS.reset (s.makeup); mixS.reset (s.mix); first = false; }
        makeupS.set (s.makeup); mixS.set (s.on ? s.mix : 0.0f);
        if (! s.on && mixS.settled()) { grShown.store (0.0f); gr = 0.0f; return; }

        const auto t = s.type;
        const bool feedback = t == CompType::opto || t == CompType::varimu;
        float knee = 6.0f, attMs = s.attackMs, relMs = s.releaseMs, rmsMs = 5.0f;
        switch (t)
        {
            case CompType::fet:    knee = 2.0f;  attMs = std::max (0.02f, s.attackMs * 0.25f); break;
            case CompType::opto:   knee = 12.0f; attMs = 10.0f; rmsMs = 10.0f; break;
            case CompType::vca:    knee = 1.0f;  rmsMs = 5.0f; break;
            case CompType::varimu: knee = 15.0f; attMs = std::max (8.0f, s.attackMs); relMs = s.releaseMs * 1.5f; rmsMs = 20.0f; break;
            case CompType::clean:  knee = 6.0f;  rmsMs = 3.0f; break;
        }
        const float aC = msToCoeff (attMs, sr);
        const float rmsC = msToCoeff (rmsMs, sr);
        const float memC = msToCoeff (1500.0f, sr);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // Detector enlazado
            float pk = 0.0f, sq = 0.0f;
            for (int c = 0; c < nch; ++c) { const float v = ch[c][i]; pk = std::max (pk, std::abs (v)); sq = std::max (sq, v * v); }
            if (feedback) { pk = fbLevel; sq = fbLevel * fbLevel; }
            rms = sq + rmsC * (rms - sq);
            float levelDb;
            switch (t)
            {
                case CompType::fet:   levelDb = gainToDb (pk); break;
                case CompType::clean: levelDb = 0.5f * gainToDb (pk) + 0.5f * powToDb (rms); break;
                default:              levelDb = powToDb (rms); break;
            }

            // Calculador de ganancia con rodilla blanda
            float ratio = s.ratio;
            const float over = levelDb - s.threshold;
            if (t == CompType::varimu) ratio = 1.0f + (s.ratio - 1.0f) * juce::jlimit (0.0f, 1.0f, (over + knee * 0.5f) / 15.0f);
            float target;
            if (2.0f * over < -knee) target = 0.0f;
            else if (2.0f * over > knee) target = over * (1.0f - 1.0f / ratio);
            else { const float o = over + knee * 0.5f; target = (1.0f - 1.0f / ratio) * o * o / (2.0f * knee); }

            // Balística en dB
            float rC;
            if (t == CompType::opto)
            {
                memory = target > 0.5f ? 1.0f + memC * (memory - 1.0f) : memC * memory;
                rC = msToCoeff (60.0f + memory * s.releaseMs * 3.0f, sr);
            }
            else if (t == CompType::fet)
                rC = msToCoeff (relMs * (1.0f - 0.5f * juce::jlimit (0.0f, 1.0f, gr / 12.0f)), sr);
            else
                rC = msToCoeff (relMs, sr);
            gr = target > gr ? target + aC * (gr - target) : target + rC * (gr - target);
            maxGr = std::max (maxGr, gr);

            const float g = dbToGain (-gr + makeupS.next());
            const float mx = mixS.next();
            const float colour = juce::jlimit (0.0f, 1.0f, gr / 10.0f);
            float outPk = 0.0f;
            for (int c = 0; c < nch; ++c)
            {
                const float x = ch[c][i];
                float y = x * g;
                if (t == CompType::fet)    y = y - 0.035f * colour * y * y * y;
                if (t == CompType::varimu) y = dc.tick (c, y + 0.04f * colour * y * std::abs (y));
                outPk = std::max (outPk, std::abs (y));
                ch[c][i] = x + mx * (y - x);
            }
            fbLevel = outPk;
        }
        grShown.store (maxGr);
    }

    float gainReduction() const { return grShown.load(); }

private:
    double sr = 48000.0;
    float gr = 0.0f, rms = 0.0f, memory = 0.0f, fbLevel = 0.0f;
    bool first = true;
    Smooth makeupS, mixS;
    DcBlock dc;
    std::atomic<float> grShown { 0.0f };
};
} // namespace chainivo
