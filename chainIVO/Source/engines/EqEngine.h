// EQ ENGINE — filtro paso alto, estante grave, campana media, campana de presencia dinámica y estante de aire.
// Filtros SVF de Simper (TPT): sin zumbidos al automatizar y bien condicionados en float.
// La campana de presencia puede ser dinámica: actúa sólo cuando la voz principal (sidechain) ocupa esa zona,
// o, sin voz principal, sólo cuando la propia pista se pasa en esa zona.
#pragma once
#include "../dsp/DspCore.h"

namespace chainivo
{
struct EqSettings
{
    bool on = true;
    float hpf = 90.0f; bool hpf24 = true;
    float hpfAmount = 1.0f;   // 0..1: mezcla entre sin filtro y filtrado (INTENSITY 0 = sin filtro, nulo exacto)
    float lowFreq = 180.0f, lowGain = 0.0f;
    float midFreq = 800.0f, midGain = 0.0f, midQ = 1.0f;
    float presFreq = 3200.0f, presGain = 0.0f, presQ = 1.2f, presDyn = 0.0f;
    float airFreq = 10000.0f, airGain = 0.0f;
};

class EqEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto* s : { &sHpf, &sHpfAmt, &sLowF, &sLowG, &sMidF, &sMidG, &sMidQ, &sPresF, &sPresG, &sPresQ, &sAirF, &sAirG })
            s->prepare (sr / kSub, 40.0f);
        keyDet.prepare (sr, 4.0f, 160.0f);
        ownDet.prepare (sr, 4.0f, 160.0f);
        first = true;
        reset();
    }
    void reset()
    {
        for (auto* b : { &hp1, &hp2, &low, &mid, &pres, &air }) b->reset();
        keySvf.reset(); ownSvf.reset();
        keySmooth = 0.0f; leadPeak = -60.0f; ownPeak = -60.0f;
    }

    // side: voz principal en mono (puede ser nullptr)
    void process (float* const* ch, int nch, int n, const EqSettings& s, const float* side)
    {
        if (first) { snap (s); first = false; }
        sHpf.set (s.hpf); sHpfAmt.set (s.hpfAmount); sLowF.set (s.lowFreq); sLowG.set (s.lowGain); sMidF.set (s.midFreq); sMidG.set (s.midGain); sMidQ.set (s.midQ);
        sPresF.set (s.presFreq); sPresG.set (s.presGain); sPresQ.set (s.presQ); sAirF.set (s.airFreq); sAirG.set (s.airGain);
        hpf24 = s.hpf24;

        for (int start = 0; start < n; start += kSub)
        {
            const int len = std::min (kSub, n - start);
            updateCoefficients (s, ch, nch, start, len, side);
            if (! s.on) continue;
            for (int c = 0; c < nch; ++c)
            {
                float* x = ch[c] + start;
                for (int i = 0; i < len; ++i)
                {
                    float v = hp1.tick (c, x[i]);
                    if (hpf24) v = hp2.tick (c, v);
                    v = x[i] + hpAmt * (v - x[i]);
                    v = low.tick (c, v);
                    v = mid.tick (c, v);
                    v = pres.tick (c, v);
                    x[i] = air.tick (c, v);
                }
            }
        }
    }

    float dynamicKey() const { return keyShown.load(); }
    float currentPresGain() const { return presShown.load(); }

    // Curva real del EQ (para la pantalla): misma fórmula que los filtros
    static float responseDb (const EqSettings& s, double sr, float f, float presGainNow)
    {
        if (! s.on) return 0.0f;
        EqBand b; float db = 0.0f, hpDb = 0.0f;
        if (s.hpf24)
        {
            b.setup (sr, EqBand::highPass, s.hpf, 0.5412f, 0.0f); hpDb += b.magnitudeDb (sr, f);
            b.setup (sr, EqBand::highPass, s.hpf, 1.3066f, 0.0f); hpDb += b.magnitudeDb (sr, f);
        }
        else { b.setup (sr, EqBand::highPass, s.hpf, 0.7071f, 0.0f); hpDb += b.magnitudeDb (sr, f); }
        // aproximación en magnitud de la mezcla seca/filtrada
        db += gainToDb (1.0f - s.hpfAmount + s.hpfAmount * dbToGain (hpDb));
        b.setup (sr, EqBand::lowShelf, s.lowFreq, 0.7071f, s.lowGain); db += b.magnitudeDb (sr, f);
        b.setup (sr, EqBand::bell, s.midFreq, s.midQ, s.midGain); db += b.magnitudeDb (sr, f);
        b.setup (sr, EqBand::bell, s.presFreq, s.presQ, presGainNow); db += b.magnitudeDb (sr, f);
        b.setup (sr, EqBand::highShelf, s.airFreq, 0.7071f, s.airGain); db += b.magnitudeDb (sr, f);
        return db;
    }

private:
    static constexpr int kSub = 32;   // los coeficientes se recalculan cada 32 muestras

    void snap (const EqSettings& s)
    {
        sHpf.reset (s.hpf); sHpfAmt.reset (s.hpfAmount); sLowF.reset (s.lowFreq); sLowG.reset (s.lowGain); sMidF.reset (s.midFreq); sMidG.reset (s.midGain);
        sMidQ.reset (s.midQ); sPresF.reset (s.presFreq); sPresG.reset (s.presGain); sPresQ.reset (s.presQ);
        sAirF.reset (s.airFreq); sAirG.reset (s.airGain);
    }

    void updateCoefficients (const EqSettings& s, float* const* ch, int nch, int start, int len, const float* side)
    {
        const float hpf = sHpf.next(); hpAmt = sHpfAmt.next(); const float lf = sLowF.next(), lg = sLowG.next(), mf = sMidF.next(), mg = sMidG.next(), mq = sMidQ.next();
        const float pf = sPresF.next(), pg = sPresG.next(), pq = sPresQ.next(), af = sAirF.next(), ag = sAirG.next();

        // Llave de la banda dinámica
        float key = 1.0f;
        if (s.presDyn > 0.001f)
        {
            keySvf.setup (sr, pf, 1.4f); ownSvf.setup (sr, pf, 1.4f);
            float kdb = -120.0f, odb = -120.0f;
            for (int i = 0; i < len; ++i)
            {
                float m = 0.0f;
                for (int c = 0; c < nch; ++c) m += ch[c][start + i];
                m /= (float) nch;
                const float ob = ownSvf.tick (0, m).bp * ownSvf.k;
                odb = ownDet.tick (powToDb (ob * ob));
                if (side != nullptr)
                {
                    const float kb = keySvf.tick (0, side[start + i]).bp * keySvf.k;
                    kdb = keyDet.tick (powToDb (kb * kb));
                }
            }
            const float blockSec = (float) len / (float) sr;
            if (side != nullptr)
            {
                // Pico de la voz principal en esa zona, con caída lenta (3 s): la llave es relativa a cómo canta
                leadPeak = std::max (kdb, leadPeak - 20.0f * blockSec / 3.0f);
                key = juce::jlimit (0.0f, 1.0f, (kdb - (leadPeak - 24.0f)) / 14.0f);
            }
            else
            {
                ownPeak = std::max (odb, ownPeak - 20.0f * blockSec / 3.0f);
                key = juce::jlimit (0.0f, 1.0f, (odb - (ownPeak - 10.0f)) / 8.0f);
            }
            const float c = key > keySmooth ? 0.5f : 0.15f;
            keySmooth += c * (key - keySmooth);
            key = keySmooth;
        }
        const float presNow = pg * ((1.0f - s.presDyn) + s.presDyn * key);
        keyShown.store (key); presShown.store (presNow);

        if (hpf24) { hp1.setup (sr, EqBand::highPass, hpf, 0.5412f, 0.0f); hp2.setup (sr, EqBand::highPass, hpf, 1.3066f, 0.0f); }
        else hp1.setup (sr, EqBand::highPass, hpf, 0.7071f, 0.0f);
        low.setup (sr, EqBand::lowShelf, lf, 0.7071f, lg);
        mid.setup (sr, EqBand::bell, mf, mq, mg);
        pres.setup (sr, EqBand::bell, pf, pq, presNow);
        air.setup (sr, EqBand::highShelf, af, 0.7071f, ag);
    }

    double sr = 48000.0;
    bool first = true, hpf24 = true;
    EqBand hp1, hp2, low, mid, pres, air;
    float hpAmt = 1.0f;
    Smooth sHpf, sHpfAmt, sLowF, sLowG, sMidF, sMidG, sMidQ, sPresF, sPresG, sPresQ, sAirF, sAirG;
    Svf keySvf, ownSvf;
    EnvDb keyDet, ownDet;
    float keySmooth = 0.0f, leadPeak = -60.0f, ownPeak = -60.0f;
    std::atomic<float> keyShown { 0.0f }, presShown { 0.0f };
};
} // namespace chainivo
