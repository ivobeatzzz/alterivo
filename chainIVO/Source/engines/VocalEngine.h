// VOCAL CONTROL ENGINE — de-esser, dureza y respiraciones. Todo dinámico y enlazado en estéreo.
//  DE-ESS  : banda de sibilancia (SVF, Q 1). Sólo baja esa banda, y sólo cuando su energía se acerca a la
//            de toda la voz (eso es una "s", no una vocal brillante). y = x - (1 - g) * banda: con g = 1 la
//            reconstrucción es exacta, así que cuando no actúa no cambia ni la fase ni el timbre.
//  HARSH   : lo mismo en 2.5-5 kHz con rodilla más suave y tiempos más lentos (dureza de las vocales).
//  BREATH  : baja las respiraciones: tramos flojos (14 dB o más por debajo de cómo canta), ruidosos
//            (más agudos que graves) y que duran más de 80 ms. Las consonantes cortas no cumplen la
//            duración, así que se respetan. Es una regla medible, no una "IA".
#pragma once
#include "../dsp/DspCore.h"

namespace chainivo
{
struct VocalSettings
{
    bool on = true;
    float deess = 0.0f, deessFreq = 6500.0f, harsh = 0.0f, breath = 0.0f;   // cantidades 0..1
};

class VocalEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        sibFull.prepare (sr, 0.5f, 60.0f); sibBand.prepare (sr, 0.5f, 60.0f);
        harFull.prepare (sr, 3.0f, 90.0f); harBand.prepare (sr, 3.0f, 90.0f);
        brFull.prepare (sr, 5.0f, 50.0f); brHigh.prepare (sr, 5.0f, 50.0f); brLow.prepare (sr, 5.0f, 50.0f);
        reset();
    }
    void reset()
    {
        for (auto* f : { &detS, &detH, &apS, &apH, &brHp, &brLp }) f->reset();
        grS = grH = grB = 0.0f; speechPeak = -40.0f; quietFor = 0;
    }

    void process (float* const* ch, int nch, int n, const VocalSettings& s)
    {
        if (! s.on && grS < 0.01f && grH < 0.01f && grB < 0.01f) { publish (0, 0, 0); return; }
        const float deess = s.on ? s.deess : 0.0f, harsh = s.on ? s.harsh : 0.0f, breath = s.on ? s.breath : 0.0f;

        detS.setup (sr, s.deessFreq, 1.0f); apS.setup (sr, s.deessFreq, 1.0f);
        detH.setup (sr, 3300.0f, 1.2f);     apH.setup (sr, 3300.0f, 1.2f);
        brHp.setup (sr, 2500.0f, 0.7071f);  brLp.setup (sr, 400.0f, 0.7071f);
        const float aS = msToCoeff (0.4f, sr), rS = msToCoeff (45.0f, sr);
        const float aH = msToCoeff (4.0f, sr), rH = msToCoeff (90.0f, sr);
        const float aB = msToCoeff (15.0f, sr), rB = msToCoeff (80.0f, sr);
        const float peakFall = 20.0f / (3.0f * (float) sr);
        const int holdNeeded = (int) (0.08 * sr);
        float mS = 0, mH = 0, mB = 0;

        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < nch; ++c) m += ch[c][i];
            m /= (float) nch;

            // Sibilancia
            const float sb = detS.tick (0, m).bp * detS.k;
            const float fS = sibFull.tick (powToDb (m * m)), bS = sibBand.tick (powToDb (sb * sb));
            float tS = 0.0f;
            if (fS > -55.0f) tS = juce::jlimit (0.0f, deess * 12.0f, (bS - fS + 8.0f) * 2.0f);
            grS = tS > grS ? tS + aS * (grS - tS) : tS + rS * (grS - tS);

            // Dureza
            const float hb = detH.tick (0, m).bp * detH.k;
            const float fH = harFull.tick (powToDb (m * m)), bH = harBand.tick (powToDb (hb * hb));
            float tH = 0.0f;
            if (fH > -55.0f) tH = juce::jlimit (0.0f, harsh * 6.0f, (bH - fH + 11.0f) * 0.8f);
            grH = tH > grH ? tH + aH * (grH - tH) : tH + rH * (grH - tH);

            // Respiraciones
            const auto bo = brHp.tick (0, m); const auto lo = brLp.tick (0, m);
            const float fB = brFull.tick (powToDb (m * m));
            const float hiB = brHigh.tick (powToDb (bo.hp * bo.hp)), loB = brLow.tick (powToDb (lo.lp * lo.lp));
            speechPeak = std::max (fB, speechPeak - peakFall);
            const bool quiet = fB < speechPeak - 14.0f && fB > speechPeak - 55.0f;
            quietFor = quiet ? quietFor + 1 : 0;
            const bool noisy = hiB > loB + 3.0f;
            const float tB = (quietFor > holdNeeded && noisy) ? breath * 12.0f : 0.0f;
            grB = tB > grB ? tB + aB * (grB - tB) : tB + rB * (grB - tB);

            const float gS = dbToGain (-grS), gH = dbToGain (-grH), gB = dbToGain (-grB);
            for (int c = 0; c < nch; ++c)
            {
                float x = ch[c][i];
                if (grS > 0.001f || deess > 0.0f) x -= (1.0f - gS) * apS.tick (c, x).bp * apS.k;
                if (grH > 0.001f || harsh > 0.0f) x -= (1.0f - gH) * apH.tick (c, x).bp * apH.k;
                ch[c][i] = x * gB;
            }
            mS = std::max (mS, grS); mH = std::max (mH, grH); mB = std::max (mB, grB);
        }
        publish (mS, mH, mB);
    }

    float deessReduction() const  { return shownS.load(); }
    float harshReduction() const  { return shownH.load(); }
    float breathReduction() const { return shownB.load(); }

private:
    void publish (float a, float b, float c) { shownS.store (a); shownH.store (b); shownB.store (c); }

    double sr = 48000.0;
    Svf detS, detH, apS, apH, brHp, brLp;
    EnvDb sibFull, sibBand, harFull, harBand, brFull, brHigh, brLow;
    float grS = 0, grH = 0, grB = 0, speechPeak = -40.0f;
    int quietFor = 0;
    std::atomic<float> shownS { 0.0f }, shownH { 0.0f }, shownB { 0.0f };
};
} // namespace chainivo
