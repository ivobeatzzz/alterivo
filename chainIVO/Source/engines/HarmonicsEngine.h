// HARMONICS ENGINE — saturación con sobremuestreo (filtros FIR de media banda, fase lineal).
//  TUBE        : curva asimétrica (armónicos pares), pendiente 1 en señal pequeña.
//  TAPE        : pre-énfasis de agudos, saturación suave, de-énfasis (los agudos saturan antes, como en cinta).
//  TRANSFORMER : los graves saturan más que los medios (núcleo del transformador), armónicos impares.
//  EXCITER     : genera armónicos sólo de la banda alta y los suma a la señal (realce).
// Sobremuestreo según calidad: ECO 2x · NORMAL 4x · HIGH 4x (filtro máximo) · ULTRA 8x (filtro máximo).
// La latencia es fija para cada calidad y se mantiene aunque el motor esté apagado (el host no la ve cambiar).
#pragma once
#include "../dsp/DspCore.h"

namespace chainivo
{
enum class HarmType { tube = 0, tape, transformer, exciter };

struct HarmSettings
{
    bool on = false;
    HarmType type = HarmType::tube;
    float drive = 0.3f;   // 0..1
    float tone = 0.0f;    // -1..1
    float mix = 1.0f;     // 0..1
};

class HarmonicsEngine
{
public:
    void prepare (double sampleRate, int maxBlock, int numCh, Quality q)
    {
        sr = sampleRate; nch = std::min (numCh, kMaxCh);
        int factor = 1; bool maxQ = false;
        switch (q)
        {
            case Quality::eco:    factor = 1; break;
            case Quality::normal: factor = 2; break;
            case Quality::high:   factor = 2; maxQ = true; break;
            case Quality::ultra:  factor = 3; maxQ = true; break;
        }
        os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) nch, (size_t) factor,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, maxQ, true);
        os->initProcessing ((size_t) maxBlock);
        latency = (int) std::lround (os->getLatencyInSamples());
        osRate = sr * (double) (1 << factor);
        for (auto& d : dryDelay) d.prepare (latency + 8);
        driveS.prepare (sr, 40.0f); mixS.prepare (sr, 40.0f); toneS.prepare (sr, 60.0f);
        dryBuf.setSize (nch, maxBlock);
        mixRamp.assign ((size_t) maxBlock, 0.0f);
        dc.prepare (osRate);
        first = true;
        reset();
    }
    void reset()
    {
        if (os) os->reset();
        for (auto& d : dryDelay) d.reset();
        pre.reset(); post.reset(); split.reset(); exHp.reset(); exHp2.reset(); tiltLo.reset(); tiltHi.reset(); dc.reset();
    }
    int getLatency() const { return latency; }

    void process (float* const* ch, int n, const HarmSettings& s)
    {
        if (first) { driveS.reset (s.drive); mixS.reset (s.on ? s.mix : 0.0f); toneS.reset (s.tone); kPrev = dbToGain (s.drive * 24.0f); first = false; }
        driveS.set (s.drive); toneS.set (s.tone); mixS.set (s.on ? s.mix : 0.0f);

        // Copia seca retrasada exactamente lo mismo que el camino sobremuestreado (mezcla en fase)
        for (int c = 0; c < nch; ++c)
        {
            float* d = dryBuf.getWritePointer (c);
            for (int i = 0; i < n; ++i) { dryDelay[c].push (ch[c][i]); d[i] = dryDelay[c].tap (latency); }
        }
        const bool active = s.on || ! mixS.settled();

        // Rampa de mezcla por muestra (igual para los dos canales)
        for (int i = 0; i < n; ++i) mixRamp[(size_t) i] = mixS.next();
        float drive = s.drive;
        for (int i = 0; i < n; ++i) drive = driveS.next();
        const float tone = toneS.cur; for (int i = 0; i < n; ++i) toneS.next();
        const float kNow = dbToGain (drive * 24.0f);

        // El sobremuestreo funciona siempre (el estado de sus filtros nunca se rompe)
        juce::dsp::AudioBlock<float> block (ch, (size_t) nch, (size_t) n);
        auto up = os->processSamplesUp (block);
        const int m = (int) up.getNumSamples();
        if (active)
        {
            setupFilters (s.type, tone);
            for (int c = 0; c < nch; ++c)
            {
                float* x = up.getChannelPointer ((size_t) c);
                for (int i = 0; i < m; ++i)
                {
                    const float k = kPrev + (kNow - kPrev) * (float) i / (float) m;   // sin escalones al mover DRIVE
                    x[i] = shape (c, x[i], s.type, k);
                }
            }
        }
        os->processSamplesDown (block);
        kPrev = kNow;

        for (int c = 0; c < nch; ++c)
        {
            const float* d = dryBuf.getReadPointer (c);
            if (! active) { juce::FloatVectorOperations::copy (ch[c], d, n); continue; }
            for (int i = 0; i < n; ++i)
            {
                float y = ch[c][i];
                if (s.type != HarmType::exciter) y = tiltHi.tick (c, tiltLo.tick (c, y));
                ch[c][i] = d[i] + mixRamp[(size_t) i] * (y - d[i]);
            }
        }
    }

    // Curva de transferencia real (para la pantalla)
    static float transfer (HarmType t, float x, float drive)
    {
        const float k = dbToGain (drive * 24.0f);
        switch (t)
        {
            case HarmType::tube:        return tube (x, k);
            case HarmType::tape:        return std::tanh (k * x) / k;
            case HarmType::transformer: return softCubic (x * k) / k;
            case HarmType::exciter:     return x;
        }
        return x;
    }

private:
    static float tube (float x, float k)
    {
        constexpr float b = 0.22f;
        const float tb = std::tanh (k * b);
        const float slope = k * (1.0f - tb * tb);
        return (std::tanh (k * (x + b)) - tb) / slope;
    }
    static float softCubic (float x)
    {
        if (x > 1.0f) return 2.0f / 3.0f;
        if (x < -1.0f) return -2.0f / 3.0f;
        return x - x * x * x / 3.0f;
    }

    void setupFilters (HarmType t, float tone)
    {
        if (t == HarmType::tape) { pre.setup (osRate, EqBand::highShelf, 3000.0f, 0.7071f, 5.0f); post.setup (osRate, EqBand::highShelf, 3000.0f, 0.7071f, -5.0f); }
        if (t == HarmType::transformer) split.setup (osRate, 160.0f, 0.7071f);
        if (t == HarmType::exciter)
        {
            const float f = 2500.0f * std::pow (2.0f, tone * 1.3f);   // tono = dónde empieza el realce
            exHp.setup (osRate, f, 0.7071f); exHp2.setup (osRate, f, 0.7071f);
        }
        tiltLo.setup (sr, EqBand::lowShelf, 800.0f, 0.5f, -tone * 3.0f);
        tiltHi.setup (sr, EqBand::highShelf, 1600.0f, 0.5f, tone * 3.0f);
    }

    float shape (int c, float x, HarmType t, float k)
    {
        switch (t)
        {
            case HarmType::tube: return dc.tick (c, tube (x, k));
            case HarmType::tape:
            {
                const float v = pre.tick (c, x);
                return post.tick (c, std::tanh (k * v) / k);
            }
            case HarmType::transformer:
            {
                const auto o = split.tick (c, x);
                const float lowPart = o.lp, rest = x - o.lp;
                const float kl = k * 1.8f, kr = k * 0.6f;
                return softCubic (lowPart * kl) / kl + softCubic (rest * kr) / kr;
            }
            case HarmType::exciter:
            {
                // Armónicos nuevos de la banda alta: lo que la saturación añade a esa banda, filtrado otra vez
                const float h = exHp.tick (c, x).hp;
                const float g = 2.0f * k;
                const float gen = std::tanh (g * h) / g - h;
                return x - 2.5f * exHp2.tick (c, gen).hp;
            }
        }
        return x;
    }

    double sr = 48000.0, osRate = 96000.0;
    int nch = 2, latency = 0;
    bool first = true;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    std::array<DelayLine, kMaxCh> dryDelay;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float> mixRamp;
    float kPrev = 1.0f;
    Smooth driveS, mixS, toneS;
    EqBand pre, post, tiltLo, tiltHi;
    Svf split, exHp, exHp2;
    DcBlock dc;
};
} // namespace chainivo
