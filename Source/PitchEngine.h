// alterIVO — motor de pitch y formantes (C++ puro, sin dependencias de JUCE)
//
//  · Detección de tono: YIN sobre la señal filtrada y diezmada (~24 kHz).
//  · Cambio de tono: TD-PSOLA. Se marcan los pulsos glotales de la voz
//    (marcas de análisis) y se recolocan granos de 2 periodos a la nueva
//    distancia (marcas de síntesis). Así el tono cambia sin efecto "ardilla".
//  · Formantes: cada grano se re-muestrea en el tiempo; comprimirlo sube la
//    envolvente espectral (voz más pequeña) y estirarlo la baja.
//  · Modos: Transpose (intervalo), Quantize (nota cromática más cercana,
//    tipo hard-tune) y Robot (todo a una sola nota).
//  · Las partes sin tono (consonantes, ruido) pasan sin cambio de tono,
//    pero sí reciben el cambio de formante.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace alterivo
{

enum class Mode { Transpose = 0, Quantize = 1, Robot = 2 };

class PitchEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        maxPeriod      = (int) std::ceil (sr / kMinHz);
        minPeriod      = std::max (2, (int) std::floor (sr / kMaxHz));
        unvoicedPeriod = (int) std::round (sr / 200.0);

        D1 = 2 * maxPeriod + 8;   // anticipación para marcas y granos
        D2 = maxPeriod + 8;       // espacio para el solapamiento de granos
        latency = D1 + D2;

        ringSize = nextPow2 (8 * maxPeriod + 2048);
        ringMask = ringSize - 1;
        inBuf.assign ((size_t) ringSize, 0.0f);
        lpBuf.assign ((size_t) ringSize, 0.0f);
        accBuf.assign ((size_t) ringSize, 0.0f);

        // --- YIN sobre señal diezmada
        decim   = std::max (1, (int) std::round (sr / 24000.0));
        dsr     = sr / decim;
        tauMaxD = (int) std::ceil (dsr / kMinHz) + 2;
        tauMinD = std::max (2, (int) std::floor (dsr / kMaxHz));
        yinW    = std::max (tauMaxD, 256);
        decSize = nextPow2 (yinW + tauMaxD + 64);
        decMask = decSize - 1;
        decBuf.assign ((size_t) decSize, 0.0f);
        yinX.assign ((size_t) (yinW + tauMaxD + 2), 0.0f);
        yinD.assign ((size_t) (tauMaxD + 2), 0.0f);
        hopFull = decim * 128;

        levelCoef = (float) (1.0 - std::exp (-1.0 / (0.3 * sr)));   // medida de nivel: 300 ms
        gainCoef  = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));  // ajuste suave: 80 ms

        // Filtro paso bajo (RBJ, 1 kHz, Q 0.707) para análisis
        const double w0 = 2.0 * kPi * 1000.0 / sr;
        const double alpha = std::sin (w0) / (2.0 * 0.7071);
        const double cw = std::cos (w0);
        const double a0 = 1.0 + alpha;
        lb0 = (float) (((1.0 - cw) * 0.5) / a0);
        lb1 = (float) ((1.0 - cw) / a0);
        lb2 = lb0;
        la1 = (float) ((-2.0 * cw) / a0);
        la2 = (float) ((1.0 - alpha) / a0);

        reset();
    }

    void reset()
    {
        std::fill (inBuf.begin(), inBuf.end(), 0.0f);
        std::fill (lpBuf.begin(), lpBuf.end(), 0.0f);
        std::fill (accBuf.begin(), accBuf.end(), 0.0f);
        std::fill (decBuf.begin(), decBuf.end(), 0.0f);
        z1 = z2 = 0.0f;
        n = 0;
        decAccum = 0.0f; decCount = 0; decWritten = 0;
        hopCounter = 0;

        voiced = false;
        unvoicedRun = 0;
        curPeriod = (float) unvoicedPeriod;
        hist[0] = hist[1] = hist[2] = 0.0f;
        histCount = 0;

        markHead = markCount = 0;
        lastMarkPos = 0;
        searchIdx = 0;
        nextSynth = 0.0;

        inPow = outPow = 0.0f;
        levelGain = levelTarget = 1.0f;

        detectedHz.store (0.0f);
        outputHz.store (0.0f);
    }

    int getLatencySamples() const noexcept { return latency; }

    // ---- Parámetros (se pueden cambiar en cada bloque)
    void setPitchSemitones (float s) noexcept   { pitchSemis = s; }
    void setFormantSemitones (float s) noexcept { formantSemis = s; }
    void setLink (bool b) noexcept              { link = b; }
    void setMode (Mode m) noexcept              { mode = m; }
    void setMidiNote (int note) noexcept        { midiNote = note; }   // -1 = ninguna

    // Para la interfaz
    float getDetectedHz() const noexcept { return detectedHz.load (std::memory_order_relaxed); }
    float getOutputHz() const noexcept   { return outputHz.load (std::memory_order_relaxed); }

    // ---- Procesado muestra a muestra (mono)
    float process (float x) noexcept
    {
        // 1) Guardar entrada y versión filtrada
        const int64_t w = n;
        inBuf[(size_t) (w & ringMask)] = x;

        const float lp = lb0 * x + z1;
        z1 = lb1 * x - la1 * lp + z2;
        z2 = lb2 * x - la2 * lp;
        lpBuf[(size_t) (w & ringMask)] = lp;

        // 2) Diezmado para YIN
        decAccum += lp;
        if (++decCount >= decim)
        {
            decBuf[(size_t) (decWritten & decMask)] = decAccum / (float) decim;
            ++decWritten;
            decAccum = 0.0f;
            decCount = 0;
        }

        if (++hopCounter >= hopFull)
        {
            hopCounter = 0;
            runPitchDetection();
        }

        ++n;

        // 3) Marcas de análisis
        generateMarks();

        // 4) Marcas de síntesis y suma de granos
        const int64_t p = n - 1 - D1;
        if (p >= 0)
        {
            while (nextSynth <= (double) p)
                placeGrain ((int64_t) std::llround (nextSynth));
        }

        // 5) Salida
        const int64_t q = n - 1 - D1 - D2;
        if (q < 0)
            return 0.0f;

        float& slot = accBuf[(size_t) (q & ringMask)];
        const float y = slot;
        slot = 0.0f;

        // Compensación automática de nivel: la salida mantiene la sonoridad
        // de la entrada (alineada) aunque cambien el tono o los formantes.
        const float xin = inBuf[(size_t) (q & ringMask)];
        inPow  += levelCoef * (xin * xin - inPow);
        outPow += levelCoef * (y * y - outPow);
        if (inPow > 1.0e-8f && outPow > 1.0e-10f)
            levelTarget = std::clamp (std::sqrt (inPow / outPow), 0.5f, 2.0f);
        levelGain += gainCoef * (levelTarget - levelGain);
        return y * levelGain;
    }

private:
    static constexpr double kPi    = 3.14159265358979323846;
    static constexpr double kMinHz = 70.0;
    static constexpr double kMaxHz = 1000.0;
    static constexpr int    kMaxMarks = 1024;

    struct Mark { int64_t pos; float period; bool voiced; };

    static int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

    // ------------------------------------------------------------------ YIN
    void runPitchDetection() noexcept
    {
        const int len = yinW + tauMaxD;
        if (decWritten < (int64_t) len)
            return;

        const int64_t start = decWritten - len;
        double energy = 0.0;
        for (int i = 0; i < len; ++i)
        {
            const float v = decBuf[(size_t) ((start + i) & decMask)];
            yinX[(size_t) i] = v;
            if (i < yinW) energy += (double) v * v;
        }

        const double rms = std::sqrt (energy / yinW);
        float periodD = -1.0f;

        if (rms > 2.0e-3)  // ~ -54 dBFS
        {
            // Diferencia
            yinD[0] = 1.0f;
            double running = 0.0;
            for (int tau = 1; tau <= tauMaxD; ++tau)
            {
                double d = 0.0;
                const float* a = yinX.data();
                const float* b = yinX.data() + tau;
                for (int j = 0; j < yinW; ++j)
                {
                    const float diff = a[j] - b[j];
                    d += (double) diff * diff;
                }
                running += d;
                yinD[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
            }

            // Primer mínimo por debajo del umbral
            int best = -1;
            for (int tau = tauMinD; tau < tauMaxD; ++tau)
            {
                if (yinD[(size_t) tau] < kYinThreshold)
                {
                    while (tau + 1 < tauMaxD && yinD[(size_t) (tau + 1)] < yinD[(size_t) tau])
                        ++tau;
                    best = tau;
                    break;
                }
            }

            if (best > 0)
            {
                // Interpolación parabólica
                const float s0 = yinD[(size_t) (best - 1)];
                const float s1 = yinD[(size_t) best];
                const float s2 = yinD[(size_t) std::min (best + 1, tauMaxD)];
                const float den = s0 - 2.0f * s1 + s2;
                float shift = std::abs (den) > 1.0e-9f ? 0.5f * (s0 - s2) / den : 0.0f;
                shift = std::clamp (shift, -0.5f, 0.5f);
                periodD = (float) best + shift;
            }
        }

        if (periodD > 0.0f)
        {
            const float periodFull = periodD * (float) decim;
            pushHistory (periodFull);
            curPeriod = std::clamp (medianHistory(), (float) minPeriod, (float) maxPeriod);
            voiced = true;
            unvoicedRun = 0;
            detectedHz.store ((float) (sr / curPeriod), std::memory_order_relaxed);
        }
        else
        {
            if (++unvoicedRun >= 2)
            {
                voiced = false;
                histCount = 0;
                detectedHz.store (0.0f, std::memory_order_relaxed);
            }
        }
    }

    void pushHistory (float v) noexcept
    {
        hist[2] = hist[1];
        hist[1] = hist[0];
        hist[0] = v;
        histCount = std::min (histCount + 1, 3);
    }

    float medianHistory() const noexcept
    {
        if (histCount < 3)
            return hist[0];
        const float a = hist[0], b = hist[1], c = hist[2];
        return std::max (std::min (a, b), std::min (std::max (a, b), c));
    }

    // --------------------------------------------------- marcas de análisis
    void pushMark (const Mark& m) noexcept
    {
        if (markCount == kMaxMarks)            // descarta la más antigua
        {
            markHead = (markHead + 1) % kMaxMarks;
            --markCount;
            searchIdx = std::max (0, searchIdx - 1);
        }
        marks[(markHead + markCount) % kMaxMarks] = m;
        ++markCount;
    }

    const Mark& markAt (int i) const noexcept { return marks[(markHead + i) % kMaxMarks]; }

    void pruneMarks (int64_t before) noexcept
    {
        while (markCount > 2 && markAt (1).pos < before)
        {
            markHead = (markHead + 1) % kMaxMarks;
            --markCount;
            searchIdx = std::max (0, searchIdx - 1);
        }
    }

    void generateMarks() noexcept
    {
        for (;;)
        {
            const bool v = voiced;
            const float T = v ? curPeriod : (float) unvoicedPeriod;
            const int Ti = std::max (2, (int) std::lround (T));
            const int64_t cand = lastMarkPos + Ti;
            const int radius = v ? Ti / 3 : 0;

            if (cand + radius >= n - 1)
                break;

            int64_t pos = cand;
            if (v)
            {
                // Alinear con el pico de la onda (pulso glotal)
                float best = -1.0e30f;
                const int64_t lo = std::max (lastMarkPos + Ti / 2, cand - radius);
                for (int64_t k = lo; k <= cand + radius; ++k)
                {
                    const float s = lpBuf[(size_t) (k & ringMask)];
                    if (s > best) { best = s; pos = k; }
                }
            }

            pushMark ({ pos, (float) Ti, v });
            lastMarkPos = pos;
        }
    }

    int nearestMark (int64_t s) noexcept
    {
        if (markCount == 0)
            return -1;
        searchIdx = std::clamp (searchIdx, 0, markCount - 1);
        while (searchIdx + 1 < markCount
               && std::llabs (markAt (searchIdx + 1).pos - s) <= std::llabs (markAt (searchIdx).pos - s))
            ++searchIdx;
        return searchIdx;
    }

    // ------------------------------------------------------- síntesis
    static double midiToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    double synthesisPeriod (const Mark& m) const noexcept
    {
        if (! m.voiced)
            return m.period;

        const double Ta = m.period;
        const double semis = midiNote >= 0 ? (double) (midiNote - 60) : (double) pitchSemis;

        switch (mode)
        {
            case Mode::Transpose:
                return Ta / std::pow (2.0, semis / 12.0);

            case Mode::Quantize:
            {
                const double f = (sr / Ta) * std::pow (2.0, semis / 12.0);
                const double note = std::round (69.0 + 12.0 * std::log2 (f / 440.0));
                return sr / midiToHz (note);
            }

            case Mode::Robot:
            default:
            {
                const double note = midiNote >= 0 ? (double) midiNote : 72.0 + std::round ((double) pitchSemis);  // 0 = C una octava sobre el Do central
                return sr / midiToHz (note);
            }
        }
    }

    void placeGrain (int64_t s) noexcept
    {
        pruneMarks (s - 4 * (int64_t) maxPeriod);
        const int idx = nearestMark (s);

        if (idx < 0)
        {
            nextSynth += unvoicedPeriod;
            return;
        }

        const Mark m = markAt (idx);
        double Ts = std::clamp (synthesisPeriod (m), (double) minPeriod * 0.5, (double) maxPeriod * 2.0);

        const int h = std::clamp ((int) std::lround (m.period), 8, maxPeriod);

        double F = std::pow (2.0, (double) formantSemis / 12.0);
        if (link && m.voiced)
            F *= m.period / Ts;
        F = std::clamp (F, 0.25, 4.0);

        const int hs = std::clamp ((int) std::lround (h / F), 8, maxPeriod);
        const float g = (float) std::clamp (std::sqrt (Ts / (double) hs), 0.4, 2.0);

        const double invH = 1.0 / hs;
        for (int k = -hs; k <= hs; ++k)
        {
            const float w = 0.5f * (1.0f + (float) std::cos (kPi * k * invH));
            const double src = (double) m.pos + k * F;
            accBuf[(size_t) ((s + k) & ringMask)] += g * w * readCubic (src);
        }

        if (m.voiced)
            outputHz.store ((float) (sr / Ts), std::memory_order_relaxed);
        else
            outputHz.store (0.0f, std::memory_order_relaxed);

        // Sin cambio de tono (partes sin voz, o intervalo 0): las marcas de
        // síntesis se enganchan a las de análisis, así la salida queda
        // alineada con la entrada (importante para el Mix seco/procesado).
        const bool noPitchChange = ! m.voiced || std::abs (Ts - (double) m.period) < 0.5;
        if (noPitchChange && idx + 1 < markCount && markAt (idx + 1).pos > s)
        {
            const int64_t next = markAt (idx + 1).pos;
            nextSynth = (double) (next - s) <= 2.0 * Ts ? (double) next : nextSynth + Ts;
        }
        else
        {
            nextSynth += Ts;
        }
    }

    float readCubic (double pos) const noexcept
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float t = (float) (pos - (double) i);
        const float y0 = inBuf[(size_t) ((i - 1) & ringMask)];
        const float y1 = inBuf[(size_t) (i & ringMask)];
        const float y2 = inBuf[(size_t) ((i + 1) & ringMask)];
        const float y3 = inBuf[(size_t) ((i + 2) & ringMask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    // ------------------------------------------------------------ estado
    static constexpr float kYinThreshold = 0.2f;

    double sr = 48000.0;
    int maxPeriod = 0, minPeriod = 0, unvoicedPeriod = 0;
    int D1 = 0, D2 = 0, latency = 0;

    int ringSize = 0;
    int64_t ringMask = 0;
    std::vector<float> inBuf, lpBuf, accBuf;
    int64_t n = 0;

    float lb0 = 0, lb1 = 0, lb2 = 0, la1 = 0, la2 = 0, z1 = 0, z2 = 0;

    int decim = 2, tauMaxD = 0, tauMinD = 0, yinW = 0, hopFull = 256;
    double dsr = 24000.0;
    int decSize = 0;
    int64_t decMask = 0;
    std::vector<float> decBuf, yinX, yinD;
    float decAccum = 0.0f;
    int decCount = 0;
    int64_t decWritten = 0;
    int hopCounter = 0;

    bool voiced = false;
    int unvoicedRun = 0;
    float curPeriod = 240.0f;
    float hist[3] {};
    int histCount = 0;

    Mark marks[kMaxMarks] {};
    int markHead = 0, markCount = 0, searchIdx = 0;
    int64_t lastMarkPos = 0;
    double nextSynth = 0.0;

    float pitchSemis = 0.0f, formantSemis = 0.0f;
    bool link = false;
    Mode mode = Mode::Transpose;
    int midiNote = -1;

    float levelCoef = 0.0f, gainCoef = 0.0f;
    float inPow = 0.0f, outPow = 0.0f, levelGain = 1.0f, levelTarget = 1.0f;

    std::atomic<float> detectedHz { 0.0f }, outputHz { 0.0f };
};

} // namespace alterivo
