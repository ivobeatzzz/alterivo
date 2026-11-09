// alterIVO — motor de pitch y formantes (C++ puro, sin dependencias de JUCE)
//
//  · Cambio de tono por PSOLA (pulso a pulso), como los procesadores de voz de
//    estudio: se marca cada pulso de la voz, se recorta con una ventana de dos
//    periodos y se vuelve a colocar con la nueva separación. La voz conserva el
//    cuerpo, los ataques y los formantes; no hay "lata" ni fase emborronada.
//  · Formant: cada grano se estira o se encoge (remuestreo), lo que mueve los
//    formantes sin cambiar la nota.
//  · Partes sin tono (consonantes, aire): se dejan pasar tal cual; con Formant
//    se desplaza su color igual que la voz.
//  · Con Pitch = 0 y Formant = 0 la reconstrucción es exacta.
//  · Detección de tono YIN para colocar los pulsos, Quantize (con escala), Robot,
//    la pantalla y la detección de tonalidad.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace alterivo
{

enum class Mode { Transpose = 0, Quantize = 1, Robot = 2 };

// Escalas: máscara de 12 bits (bit 0 = tónica)
enum class Scale { Chromatic = 0, Major = 1, Minor = 2 };

inline uint16_t scaleMask (int root, Scale s) noexcept
{
    uint16_t base = 0xFFF;
    if (s == Scale::Major) base = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
    if (s == Scale::Minor) base = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);
    root = ((root % 12) + 12) % 12;
    return (uint16_t) (((base << root) | (base >> (12 - root))) & 0xFFF);
}

// FFT compleja radix-2 (iterativa), sólo para la pantalla de formantes
class SimpleFFT
{
public:
    void prepare (int size)
    {
        n = size;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) (n / 2));
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * 3.14159265358979323846 * i / n));
    }

    void perform (std::complex<float>* d, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (d[i], d[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const auto u = d[i + j];
                    const auto v = d[i + j + half] * w;
                    d[i + j] = u + v;
                    d[i + j + half] = u - v;
                }
        }

        if (inverse)
        {
            const float s = 1.0f / (float) n;
            for (int i = 0; i < n; ++i) d[i] *= s;
        }
    }

private:
    int n = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

// PSOLA (voz: un solo tono)
class PsolaEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        N = 1;
        while (N < (int) std::lround (sr * 0.04)) N <<= 1;     // 2048 a 44.1/48 kHz (ventana de análisis)
        pitchHop = N / 4;
        maxPeriod = (int) std::ceil (sr / kMinHz);
        // Retardo: el detector va N/2 por detrás y un grano de voz grave con Formant -12
        // ocupa hasta 4 periodos; el DAW compensa este retardo automáticamente
        latency = N / 2 + 4 * maxPeriod + 128;
        unvoicedPeriod = std::max (32, (int) std::lround (sr * 0.004));

        int size = 1;
        while (size < latency * 4 + N * 2) size <<= 1;
        mask = size - 1;
        in.assign ((size_t) size, 0.0f);
        lp.assign ((size_t) size, 0.0f);
        acc.assign ((size_t) size, 0.0f);

        // Detección de tono (YIN sobre la ventana diezmada)
        decim   = std::max (1, (int) std::round (sr / 24000.0));
        dsr     = sr / decim;
        tauMaxD = (int) std::ceil (dsr / kMinHz) + 2;
        tauMinD = std::max (2, (int) std::floor (dsr / kMaxHz));
        yinLen  = N / decim;
        yinW    = std::max (16, yinLen - tauMaxD - 1);
        yinX.assign ((size_t) yinLen, 0.0f);
        yinD.assign ((size_t) tauMaxD + 2, 0.0f);

        // Paso bajo 1 kHz para el detector y las marcas de pulso
        const double w0 = 2.0 * kPi * 1000.0 / sr;
        const double alpha = std::sin (w0) / (2.0 * 0.7071);
        const double cw = std::cos (w0), a0 = 1.0 + alpha;
        lb0 = (float) (((1.0 - cw) * 0.5) / a0);
        lb1 = (float) ((1.0 - cw) / a0);
        lb2 = lb0;
        la1 = (float) ((-2.0 * cw) / a0);
        la2 = (float) ((1.0 - alpha) / a0);

        // Pantalla de formantes
        fft.prepare (N);
        half = N / 2;
        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));
        spec.assign ((size_t) N, {});
        cep.assign ((size_t) N, {});
        logEnv.assign ((size_t) half + 1, 0.0f);
        lifter = std::max (8, (int) std::lround (sr / 1000.0));
        envEvery = std::max (pitchHop, (int) std::lround (sr / 30.0));
        scoreCoef = (float) std::min (1.0, pitchHop / (1.5 * sr));

        reset();
    }

    void reset()
    {
        std::fill (in.begin(), in.end(), 0.0f);
        std::fill (lp.begin(), lp.end(), 0.0f);
        std::fill (acc.begin(), acc.end(), 0.0f);
        z1 = z2 = 0.0f;
        t = 0;
        marks.clear();
        marksStart = 0;
        history.clear();
        historyStart = 0;
        synth = -1.0;
        lastSynthIndex = 0;
        envCount = 0;
        unvoicedRun = 0;
        glitches = 0;
        curF0 = 0.0f;
        quantNote = -1;
        voiceScore = 1.0f;
        detectedHz.store (0.0f);
        outputHz.store (0.0f);
        for (auto& v : dispEnv) v.store (-200.0f, std::memory_order_relaxed);
        dispLevel.store (-200.0f, std::memory_order_relaxed);
    }

    int getLatencySamples() const noexcept { return latency; }

    // ---- Parámetros
    void setPitchSemitones (float s) noexcept   { pitchSemis = s; }
    void setFormantSemitones (float s) noexcept { formantSemis = s; }
    void setMode (Mode m) noexcept              { mode = m; }
    void setMidiNote (int note) noexcept        { midiNote = note; }   // -1 = ninguna
    void setScale (uint16_t mask) noexcept      { allowed = mask; }

    // 0..1: cuánto parece una sola voz limpia (para elegir PSOLA o vocoder)
    float getVoiceScore() const noexcept { return voiceScore; }
    float getLastAperiodicity() const noexcept { return lastAperiodicity; }
    float getLastFlatness() const noexcept { return lastFlatness; }
    bool isVoicedNow() const noexcept { return curF0 > 0.0f; }
    static constexpr float kVoiceAperiodicity = 0.15f, kTonalFlatness = 0.12f;

    float getDetectedHz() const noexcept { return detectedHz.load (std::memory_order_relaxed); }
    float getOutputHz() const noexcept   { return outputHz.load (std::memory_order_relaxed); }

    // Envolvente de formantes de la entrada para la pantalla: kEnvPoints valores en dB,
    // frecuencias en escala logarítmica de kEnvLoHz a kEnvHiHz. Devuelve el nivel aproximado (dBFS).
    static constexpr int kEnvPoints = 64;
    static constexpr float kEnvLoHz = 150.0f, kEnvHiHz = 6000.0f;
    float getDisplayEnvelope (float* dest) const noexcept
    {
        for (int i = 0; i < kEnvPoints; ++i) dest[i] = dispEnv[(size_t) i].load (std::memory_order_relaxed);
        return dispLevel.load (std::memory_order_relaxed);
    }

    // ---- Procesado muestra a muestra (mono)
    float process (float x) noexcept
    {
        const float l = lb0 * x + z1;
        z1 = lb1 * x - la1 * l + z2;
        z2 = lb2 * x - la2 * l;

        in[(size_t) (t & mask)] = x;
        lp[(size_t) (t & mask)] = l;
        ++t;

        if (t % pitchHop == 0) detectPitch();
        if (++envCount >= envEvery) { envCount = 0; publishEnvelope(); }

        placeMarks();
        synthesize();

        // Salida con el retardo fijo
        const int64_t u = t - latency;
        if (u < 0) return 0.0f;
        float& slot = acc[(size_t) (u & mask)];
        const float y = slot;
        slot = 0.0f;
        return y;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kMinHz = 75.0, kMaxHz = 1000.0;

    struct Mark { int64_t pos; bool voiced; };
    struct PitchPoint { int64_t centre; float hz; };

    static double midiToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    float sampleAt (double pos) const noexcept   // interpolación cúbica (Hermite)
    {
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float f = (float) (pos - fl);
        const float xm1 = in[(size_t) ((i - 1) & mask)], x0 = in[(size_t) (i & mask)];
        const float x1 = in[(size_t) ((i + 1) & mask)], x2 = in[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    // Tono en un instante, interpolado del historial de detecciones
    float f0At (int64_t pos) const noexcept
    {
        if (history.size() <= historyStart) return 0.0f;
        size_t lo = historyStart;
        if (pos <= history[lo].centre) return history[lo].hz;
        for (size_t i = lo + 1; i < history.size(); ++i)
        {
            const auto& b = history[i];
            if (pos <= b.centre)
            {
                const auto& a = history[i - 1];
                if (a.hz <= 0.0f || b.hz <= 0.0f)   // límite sonoro/sordo: el más cercano
                    return (pos - a.centre < b.centre - pos) ? a.hz : b.hz;
                const float k = (float) (pos - a.centre) / (float) (b.centre - a.centre);
                return a.hz * std::pow (b.hz / a.hz, k);
            }
        }
        return history.back().hz;
    }

    // ------------------------------------------------------- marcas de pulso
    void placeMarks() noexcept
    {
        // Los datos válidos llegan hasta t; el detector va N/2 por detrás
        const int64_t knownUntil = t - N / 2;

        while (true)
        {
            int64_t next;
            bool voiced;
            if (marks.size() <= marksStart)
            {
                if (knownUntil < (int64_t) maxPeriod * 2) return;
                next = std::max<int64_t> (0, knownUntil - maxPeriod * 2);
                marks.push_back ({ next, false });
                continue;
            }

            const Mark last = marks.back();
            const float hz = f0At (last.pos + maxPeriod / 2);
            if (hz > 0.0f)
            {
                const double T = sr / hz;
                const int64_t predicted = last.pos + (int64_t) std::lround (T);
                const int radius = std::max (2, (int) (T * 0.2));
                if (predicted + radius + (int64_t) std::ceil (T) > knownUntil) return;

                // El pulso: máximo de la señal filtrada cerca de donde se espera
                int64_t best = predicted;
                float bestV = -1.0e9f;
                for (int64_t p = predicted - radius; p <= predicted + radius; ++p)
                {
                    const float v = lp[(size_t) (p & mask)];
                    if (v > bestV) { bestV = v; best = p; }
                }
                next = best;
                voiced = true;
            }
            else
            {
                next = last.pos + unvoicedPeriod;
                if (next + unvoicedPeriod > knownUntil) return;
                voiced = false;
            }
            marks.push_back ({ next, voiced });
        }
    }

    // ------------------------------------------------------------ síntesis
    void synthesize() noexcept
    {
        const int64_t u = t - latency;           // próxima muestra que sale
        const double f = std::clamp (std::pow (2.0, (double) formantSemis / 12.0), 0.5, 2.0);

        while (true)
        {
            const size_t count = marks.size() - marksStart;
            if (count < 3) return;

            if (synth < 0.0)
            {
                synth = (double) marks[marksStart + 1].pos;
                lastSynthIndex = marksStart + 1;
            }

            // Marca de análisis más cercana a la de síntesis (sólo hacia delante)
            size_t i = std::max (lastSynthIndex, marksStart + 1);
            while (i + 1 < marks.size() && std::abs ((double) marks[i + 1].pos - synth) <= std::abs ((double) marks[i].pos - synth))
                ++i;
            if (i + 1 >= marks.size() || (double) marks[i + 1].pos < synth) return;   // falta la marca siguiente

            const Mark& m = marks[i];
            const int Pl = (int) std::max<int64_t> (1, m.pos - marks[i - 1].pos);
            const int Pr = (int) std::max<int64_t> (1, marks[i + 1].pos - m.pos);

            // Separación de salida según el modo
            double advance = Pr;
            double ratio = 1.0;
            const float hz = m.voiced ? f0At (m.pos) : 0.0f;
            if (m.voiced && hz > 0.0f)
            {
                ratio = std::clamp (targetRatio (hz), 0.25, 4.0);
                advance = Pr / ratio;
            }
            outputHz.store (hz > 0.0f ? (float) (hz * ratio) : 0.0f, std::memory_order_relaxed);

            // Grano: ventana Hann asimétrica (un periodo a cada lado), estirada por Formant
            const int64_t centre = (int64_t) std::llround (synth);
            const float gain = (float) std::sqrt (f / ratio);
            const int left = (int) std::ceil (Pl / f), right = (int) std::ceil (Pr / f);
            for (int j = -left; j <= right; ++j)
            {
                const int64_t o = centre + j;
                if (o < u) continue;                            // ya salió (no debería pasar)
                const double src = (double) j * f;
                const double x = src < 0.0 ? src / Pl : src / Pr;
                if (x <= -1.0 || x >= 1.0) continue;
                const float w = (float) (0.5 + 0.5 * std::cos (kPi * x));
                acc[(size_t) (o & mask)] += gain * w * sampleAt ((double) m.pos + src);
            }

            synth += std::max (1.0, advance);
            lastSynthIndex = i;

            // Se olvidan las marcas que ya no se usarán
            while (marksStart + 2 < lastSynthIndex) ++marksStart;
            if (marksStart > 256) { marks.erase (marks.begin(), marks.begin() + (long) marksStart); lastSynthIndex -= marksStart; marksStart = 0; }
        }
    }

    // Relación de tono para un pulso con frecuencia hz
    double targetRatio (float hz) noexcept
    {
        const double semis = midiNote >= 0 ? (double) (midiNote - 60) : (double) pitchSemis;
        const double transpose = std::pow (2.0, semis / 12.0);

        switch (mode)
        {
            case Mode::Transpose:
                return transpose;

            case Mode::Quantize:
            {
                const double note = 69.0 + 12.0 * std::log2 (hz * transpose / 440.0);
                // Se mantiene la nota actual hasta que la voz se aleja claramente (sin "temblores")
                if (quantNote < 0 || std::abs (note - (double) quantNote) > 0.65 || ! isAllowed (quantNote))
                    quantNote = nearestAllowed (note);
                return midiToHz ((double) quantNote) / hz;
            }

            case Mode::Robot:
            default:
            {
                const double note = midiNote >= 0 ? (double) midiNote : 72.0 + std::round ((double) pitchSemis);
                return midiToHz (note) / hz;
            }
        }
    }

    bool isAllowed (int note) const noexcept { return (allowed >> (((note % 12) + 12) % 12)) & 1; }

    int nearestAllowed (double note) const noexcept
    {
        const int base = (int) std::lround (note);
        for (int d = 0; d <= 6; ++d)
        {
            // Primero el más cercano de los dos lados
            const int a = base + d, b = base - d;
            const bool okA = isAllowed (a), okB = isAllowed (b);
            if (okA && okB) return std::abs (a - note) <= std::abs (b - note) ? a : b;
            if (okA) return a;
            if (okB) return b;
        }
        return base;
    }

    // --------------------------------------------------------------- YIN
    void detectPitch() noexcept
    {
        double energy = 0.0;
        const int64_t start = t - N;
        for (int i = 0; i < yinLen; ++i)
        {
            float s = 0.0f;
            for (int d = 0; d < decim; ++d) s += lp[(size_t) ((start + i * decim + d) & mask)];
            yinX[(size_t) i] = s / (float) decim;
            if (i < yinW) energy += (double) yinX[(size_t) i] * yinX[(size_t) i];
        }

        float periodD = -1.0f, dBest = 1.0f;
        const bool energetic = start >= 0 && std::sqrt (energy / yinW) > 1.0e-3;
        if (energetic)
        {
            double running = 0.0;
            yinD[0] = 1.0f;
            for (int tau = 1; tau <= tauMaxD; ++tau)
            {
                double dsum = 0.0;
                const float* a = yinX.data();
                const float* b = yinX.data() + tau;
                for (int j = 0; j < yinW; ++j)
                {
                    const float diff = a[j] - b[j];
                    dsum += (double) diff * diff;
                }
                running += dsum;
                yinD[(size_t) tau] = running > 0.0 ? (float) (dsum * tau / running) : 1.0f;
            }

            int best = -1;
            for (int tau = tauMinD; tau < tauMaxD; ++tau)
            {
                if (yinD[(size_t) tau] < 0.2f)
                {
                    while (tau + 1 < tauMaxD && yinD[(size_t) (tau + 1)] < yinD[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }

            if (best > 0)
            {
                const float s0 = yinD[(size_t) (best - 1)], s1 = yinD[(size_t) best], s2 = yinD[(size_t) (best + 1)];
                const float den = s0 - 2.0f * s1 + s2;
                const float sh = std::abs (den) > 1.0e-9f ? std::clamp (0.5f * (s0 - s2) / den, -0.5f, 0.5f) : 0.0f;
                periodD = (float) best + sh;
                dBest = s1;
            }
        }

        if (periodD > 0.0f)
        {
            const float hz = (float) (dsr / periodD);
            // Saltos bruscos (errores de octava) se ignoran durante un par de detecciones
            if (curF0 > 0.0f && std::abs (12.0f * std::log2 (hz / curF0)) > 9.0f && glitches < 2)
                ++glitches;
            else
            {
                curF0 = hz;
                glitches = 0;
            }
            unvoicedRun = 0;
        }
        else if (++unvoicedRun >= 2)
        {
            curF0 = 0.0f;
            glitches = 0;
            quantNote = -1;
        }
        detectedHz.store (curF0, std::memory_order_relaxed);

        // ¿Una sola voz limpia? (periodicidad clara de forma sostenida)
        // Pulsos claros cuentan como voz; tonos sin periodo único (acordes, mezcla) como
        // material polifónico; el ruido (consonantes) y el silencio no cuentan
        if (energetic)
        {
            if (periodD > 0.0f && dBest < kVoiceAperiodicity)
                voiceScore += (1.0f - voiceScore) * scoreCoef;
            else if ((periodD <= 0.0f || dBest > 0.2f) && lastFlatness < kTonalFlatness)
                voiceScore += (0.0f - voiceScore) * scoreCoef;
        }
        lastAperiodicity = dBest;

        history.push_back ({ t - N / 2, curF0 });
        // Sólo hace falta el historial que cubre las marcas pendientes
        const int64_t keepFrom = (marks.size() > marksStart ? marks[marksStart].pos : t) - N;
        while (historyStart + 2 < history.size() && history[historyStart + 1].centre < keepFrom) ++historyStart;
        if (historyStart > 256) { history.erase (history.begin(), history.begin() + (long) historyStart); historyStart = 0; }
    }

    // ------------------------------------------------- pantalla de formantes
    void publishEnvelope() noexcept
    {
        const int64_t start = t - N;
        if (start < 0) return;
        float total = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            spec[(size_t) i] = { in[(size_t) ((start + i) & mask)] * window[(size_t) i], 0.0f };
        }
        fft.perform (spec.data(), false);
        double logSum = 0.0, linSum = 0.0;
        int flatCount = 0;
        const int kLo = (int) (300.0 * N / sr), kHi = std::min (half, (int) (8000.0 * N / sr));
        for (int k = 0; k <= half; ++k)
        {
            const float m = std::abs (spec[(size_t) k]);
            total += m;
            cep[(size_t) k] = { std::log (m + 1.0e-9f), 0.0f };
            if (k >= kLo && k <= kHi)
            {
                const double pw = (double) m * m + 1.0e-12;
                logSum += std::log (pw); linSum += pw; ++flatCount;
            }
        }
        // Planitud espectral: alta = ruido (consonantes), baja = tonos (notas, acordes)
        lastFlatness = flatCount > 0 ? (float) (std::exp (logSum / flatCount) / (linSum / flatCount)) : 1.0f;
        for (int k = 1; k < half; ++k)
            cep[(size_t) (N - k)] = cep[(size_t) k];
        fft.perform (cep.data(), true);
        for (int i = lifter; i <= N - lifter; ++i)
            cep[(size_t) i] = {};
        fft.perform (cep.data(), false);
        for (int k = 0; k <= half; ++k)
            logEnv[(size_t) k] = cep[(size_t) k].real();

        constexpr float toDb = 8.685889638f;
        const float ref = 20.0f * std::log10 ((float) N * 0.25f);
        const float binPerHz = (float) N / (float) sr;
        float peak = -200.0f;
        for (int i = 0; i < kEnvPoints; ++i)
        {
            // Media en ±1/12 de octava (quita el rizado) y +4.5 dB/oct de inclinación
            const float hz = kEnvLoHz * std::pow (kEnvHiHz / kEnvLoHz, (float) i / (float) (kEnvPoints - 1));
            float sum = 0.0f;
            for (int k = -2; k <= 2; ++k)
                sum += envAt (hz * std::pow (2.0f, (float) k / 24.0f) * binPerHz);
            const float db = toDb * sum / 5.0f - ref;
            dispEnv[(size_t) i].store (db + 4.5f * std::log2 (hz / 1000.0f), std::memory_order_relaxed);
            peak = std::max (peak, db);
        }
        dispLevel.store (total > 0.0f ? peak : -200.0f, std::memory_order_relaxed);
    }

    float envAt (float bin) const noexcept
    {
        if (bin <= 0.0f) return logEnv[0];
        if (bin >= (float) half) return logEnv[(size_t) half];
        const int i = (int) bin;
        const float k = bin - (float) i;
        return logEnv[(size_t) i] + k * (logEnv[(size_t) (i + 1)] - logEnv[(size_t) i]);
    }

    // ---------------------------------------------------------- estado
    double sr = 48000.0;
    int N = 2048, half = 1024, latency = 3072, pitchHop = 512, maxPeriod = 686, unvoicedPeriod = 192;
    int64_t mask = 0, t = 0;
    std::vector<float> in, lp, acc;

    std::vector<Mark> marks;
    size_t marksStart = 0, lastSynthIndex = 0;
    std::vector<PitchPoint> history;
    size_t historyStart = 0;
    double synth = -1.0;

    float lb0 = 0, lb1 = 0, lb2 = 0, la1 = 0, la2 = 0, z1 = 0, z2 = 0;

    int decim = 2, tauMaxD = 0, tauMinD = 0, yinLen = 0, yinW = 0;
    double dsr = 24000.0;
    std::vector<float> yinX, yinD;
    int unvoicedRun = 0, glitches = 0;
    float curF0 = 0.0f;
    int quantNote = -1;
    float voiceScore = 1.0f, scoreCoef = 0.005f, lastAperiodicity = 1.0f, lastFlatness = 1.0f;

    float pitchSemis = 0.0f, formantSemis = 0.0f;
    Mode mode = Mode::Transpose;
    int midiNote = -1;
    uint16_t allowed = 0xFFF;

    SimpleFFT fft;
    int lifter = 48, envEvery = 1600, envCount = 0;
    std::vector<float> window, logEnv;
    std::vector<std::complex<float>> spec, cep;

    std::atomic<float> detectedHz { 0.0f }, outputHz { 0.0f };
    std::array<std::atomic<float>, kEnvPoints> dispEnv {};
    std::atomic<float> dispLevel { -200.0f };
};

// Phase vocoder (mezclas y material polifónico)
class VocoderEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        N = 1;
        while (N < (int) std::lround (sr * 0.04)) N <<= 1;     // 2048 a 44.1/48 kHz
        H = N / 8;
        half = N / 2;
        fft.prepare (N);

        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));

        double wsum = 0.0;                       // normalización del solapamiento
        for (int m = 0; m < N / H; ++m)
            wsum += (double) window[(size_t) (m * H)] * window[(size_t) (m * H)];
        olaScale = (float) (1.0 / wsum);

        inFifo.assign ((size_t) N, 0.0f);
        lpFifo.assign ((size_t) N, 0.0f);
        acc.assign ((size_t) N, 0.0f);
        outQueue.assign ((size_t) H, 0.0f);

        spec.assign ((size_t) N, {});
        outSpec.assign ((size_t) N, {});
        cep.assign ((size_t) N, {});
        mag.assign ((size_t) half + 1, 0.0f);
        prevMag.assign ((size_t) half + 1, 0.0f);
        phase.assign ((size_t) half + 1, 0.0f);
        prevPhase.assign ((size_t) half + 1, 0.0f);
        omega.assign ((size_t) half + 1, 0.0f);
        logEnv.assign ((size_t) half + 1, 0.0f);
        peaks.reserve ((size_t) half);
        prevPeaks.reserve ((size_t) half);
        curPeaks.reserve ((size_t) half);

        lifter = std::max (8, (int) std::lround (sr / 1000.0));
        envEvery = std::max (1, (int) std::lround (sr / H / 30.0));   // pantalla: ~30 envolventes por segundo
        envCount = 0;
        for (auto& v : dispEnv) v.store (-200.0f, std::memory_order_relaxed);
        dispLevel.store (-200.0f, std::memory_order_relaxed);

        // Detección de tono (YIN sobre la trama diezmada)
        decim   = std::max (1, (int) std::round (sr / 24000.0));
        dsr     = sr / decim;
        tauMaxD = (int) std::ceil (dsr / kMinHz) + 2;
        tauMinD = std::max (2, (int) std::floor (dsr / kMaxHz));
        yinLen  = N / decim;
        yinW    = std::max (16, yinLen - tauMaxD - 1);
        yinX.assign ((size_t) yinLen, 0.0f);
        yinD.assign ((size_t) tauMaxD + 2, 0.0f);

        // Paso bajo 1 kHz para el detector
        const double w0 = 2.0 * kPi * 1000.0 / sr;
        const double alpha = std::sin (w0) / (2.0 * 0.7071);
        const double cw = std::cos (w0), a0 = 1.0 + alpha;
        lb0 = (float) (((1.0 - cw) * 0.5) / a0);
        lb1 = (float) ((1.0 - cw) / a0);
        lb2 = lb0;
        la1 = (float) ((-2.0 * cw) / a0);
        la2 = (float) ((1.0 - alpha) / a0);

        reset();
    }

    void reset()
    {
        std::fill (inFifo.begin(), inFifo.end(), 0.0f);
        std::fill (lpFifo.begin(), lpFifo.end(), 0.0f);
        std::fill (acc.begin(), acc.end(), 0.0f);
        std::fill (outQueue.begin(), outQueue.end(), 0.0f);
        std::fill (prevPhase.begin(), prevPhase.end(), 0.0f);
        std::fill (prevMag.begin(), prevMag.end(), 0.0f);
        prevPeaks.clear();
        hopCount = 0;
        glitches = 0;
        pos = 0;
        z1 = z2 = 0.0f;
        unvoicedRun = 0;
        curF0 = 0.0f;
        detectedHz.store (0.0f);
        outputHz.store (0.0f);
    }

    int getLatencySamples() const noexcept { return N; }

    // ---- Parámetros
    void setPitchSemitones (float s) noexcept   { pitchSemis = s; }
    void setFormantSemitones (float s) noexcept { formantSemis = s; }
    void setMode (Mode m) noexcept              { mode = m; }
    void setMidiNote (int note) noexcept        { midiNote = note; }   // -1 = ninguna
    void setScale (uint16_t mask) noexcept      { allowed = mask; }

    float getDetectedHz() const noexcept { return detectedHz.load (std::memory_order_relaxed); }
    float getOutputHz() const noexcept   { return outputHz.load (std::memory_order_relaxed); }

    // Envolvente de formantes de la entrada para la pantalla: kEnvPoints valores en dB,
    // frecuencias en escala logarítmica de kEnvLoHz a kEnvHiHz. Devuelve el nivel aproximado (dBFS).
    static constexpr int kEnvPoints = 64;
    static constexpr float kEnvLoHz = 150.0f, kEnvHiHz = 6000.0f;
    float getDisplayEnvelope (float* dest) const noexcept
    {
        for (int i = 0; i < kEnvPoints; ++i) dest[i] = dispEnv[(size_t) i].load (std::memory_order_relaxed);
        return dispLevel.load (std::memory_order_relaxed);
    }

    // ---- Procesado muestra a muestra (mono)
    float process (float x) noexcept
    {
        const float lp = lb0 * x + z1;
        z1 = lb1 * x - la1 * lp + z2;
        z2 = lb2 * x - la2 * lp;

        inFifo[(size_t) (N - H + pos)] = x;
        lpFifo[(size_t) (N - H + pos)] = lp;
        const float y = outQueue[(size_t) pos];

        if (++pos >= H)
        {
            pos = 0;
            processFrame();
        }
        return y;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kMinHz = 70.0, kMaxHz = 1000.0;
    static constexpr double kOnsetFlux = 0.30;

    struct Peak { int bin; float beta; };

    static float princarg (float a) noexcept
    {
        constexpr float twoPi = 6.28318530717958647692f;
        return a - twoPi * std::floor ((a + 3.14159265358979f) / twoPi);
    }

    static double midiToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    // -------------------------------------------------------------- trama
    void processFrame() noexcept
    {
        // Quantize/Robot necesitan el tono en cada trama; para la pantalla basta 1 de cada 4
        if (mode != Mode::Transpose || (hopCount++ & 3) == 0)
            detectPitch();
        const double ratio = std::clamp (currentRatio(), 0.25, 4.0);
        // Link se resuelve en la interfaz (los dos mandos se mueven juntos)
        const double formant = std::clamp (std::pow (2.0, (double) formantSemis / 12.0), 0.25, 4.0);

        // Análisis
        for (int i = 0; i < N; ++i)
            spec[(size_t) i] = { inFifo[(size_t) i] * window[(size_t) i], 0.0f };
        fft.perform (spec.data(), false);

        const float expect = (float) (2.0 * kPi * H / N);
        double flux = 0.0, total = 0.0;
        for (int k = 0; k <= half; ++k)
        {
            mag[(size_t) k] = std::abs (spec[(size_t) k]);
            flux  += std::max (0.0f, mag[(size_t) k] - prevMag[(size_t) k]);
            total += mag[(size_t) k];
            prevMag[(size_t) k] = mag[(size_t) k];
            phase[(size_t) k] = std::arg (spec[(size_t) k]);
            const float dphi = princarg (phase[(size_t) k] - prevPhase[(size_t) k] - expect * (float) k);
            omega[(size_t) k] = (float) (2.0 * kPi * k / N) + dphi / (float) H;   // rad/muestra
            prevPhase[(size_t) k] = phase[(size_t) k];
        }


        const bool identity = std::abs (ratio - 1.0) < 1.0e-4 && std::abs (formant - 1.0) < 1.0e-4;

        // Ataque (consonante, golpe): se reinicia la fase para que no se emborrone
        if (total > 1.0e-3 && flux / total > kOnsetFlux)
            prevPeaks.clear();

        if (identity)
        {
            std::copy (spec.begin(), spec.end(), outSpec.begin());
            prevPeaks.clear();
        }
        else
        {
            shiftPeaks ((float) ratio);
            if (std::abs (formant - ratio) > 1.0e-4)
                applyFormantCorrection ((float) ratio, (float) formant);

            // Espectro hermítico para obtener una señal real
            outSpec[0] = { outSpec[0].real(), 0.0f };
            outSpec[(size_t) half] = { outSpec[(size_t) half].real(), 0.0f };
            for (int k = 1; k < half; ++k)
                outSpec[(size_t) (N - k)] = std::conj (outSpec[(size_t) k]);
        }

        // Síntesis y solapamiento
        fft.perform (outSpec.data(), true);
        for (int i = 0; i < N; ++i)
            acc[(size_t) i] += outSpec[(size_t) i].real() * window[(size_t) i] * olaScale;

        std::copy (acc.begin(), acc.begin() + H, outQueue.begin());
        std::copy (acc.begin() + H, acc.end(), acc.begin());
        std::fill (acc.end() - H, acc.end(), 0.0f);
        std::copy (inFifo.begin() + H, inFifo.end(), inFifo.begin());
        std::copy (lpFifo.begin() + H, lpFifo.end(), lpFifo.begin());

        outputHz.store (curF0 > 0.0f ? (float) (curF0 * ratio) : 0.0f, std::memory_order_relaxed);
    }

    double currentRatio() const noexcept
    {
        const double semis = midiNote >= 0 ? (double) (midiNote - 60) : (double) pitchSemis;
        const double transpose = std::pow (2.0, semis / 12.0);

        switch (mode)
        {
            case Mode::Transpose:
                return transpose;

            case Mode::Quantize:
            {
                if (curF0 <= 0.0f) return transpose;
                const double f = curF0 * transpose;
                const double note = 69.0 + 12.0 * std::log2 (f / 440.0);
                int best = (int) std::lround (note);
                for (int d = 0; d <= 6; ++d)
                {
                    const int a = (int) std::lround (note) + d, b = (int) std::lround (note) - d;
                    const bool okA = (allowed >> (((a % 12) + 12) % 12)) & 1, okB = (allowed >> (((b % 12) + 12) % 12)) & 1;
                    if (okA && okB) { best = std::abs (a - note) <= std::abs (b - note) ? a : b; break; }
                    if (okA) { best = a; break; }
                    if (okB) { best = b; break; }
                }
                return midiToHz ((double) best) / curF0;
            }

            case Mode::Robot:
            default:
            {
                if (curF0 <= 0.0f) return 1.0;
                const double note = midiNote >= 0 ? (double) midiNote : 72.0 + std::round ((double) pitchSemis);
                return midiToHz (note) / curF0;
            }
        }
    }

    // Desplaza cada pico con su región y corrige la fase (Laroche & Dolson)
    void shiftPeaks (float ratio) noexcept
    {
        std::fill (outSpec.begin(), outSpec.end(), std::complex<float> {});

        float maxMag = 0.0f;
        for (int k = 0; k <= half; ++k) maxMag = std::max (maxMag, mag[(size_t) k]);
        const float floorMag = maxMag * 1.0e-5f;

        peaks.clear();
        for (int k = 2; k <= half - 2; ++k)
        {
            const float m = mag[(size_t) k];
            if (m > floorMag && m > mag[(size_t) (k - 1)] && m >= mag[(size_t) (k + 1)]
                && m > mag[(size_t) (k - 2)] && m >= mag[(size_t) (k + 2)])
                peaks.push_back (k);
        }

        curPeaks.clear();
        if (peaks.empty())
        {
            prevPeaks.clear();
            return;
        }

        size_t prevIdx = 0;
        const float binToRad = (float) (2.0 * kPi / N);

        for (size_t pi = 0; pi < peaks.size(); ++pi)
        {
            const int p = peaks[pi];

            // Región de influencia: hasta el mínimo entre picos vecinos
            int lo = 0, hi = half;
            if (pi > 0)
            {
                lo = peaks[pi - 1];
                for (int k = peaks[pi - 1]; k < p; ++k)
                    if (mag[(size_t) k] < mag[(size_t) lo]) lo = k;
                lo += 1;
            }
            if (pi + 1 < peaks.size())
            {
                hi = p;
                for (int k = p; k < peaks[pi + 1]; ++k)
                    if (mag[(size_t) k] < mag[(size_t) hi]) hi = k;
            }

            // Fase acumulada heredada del pico más cercano de la trama anterior
            float beta = 0.0f;
            while (prevIdx + 1 < prevPeaks.size()
                   && std::abs (prevPeaks[prevIdx + 1].bin - p) <= std::abs (prevPeaks[prevIdx].bin - p))
                ++prevIdx;
            if (! prevPeaks.empty() && std::abs (prevPeaks[prevIdx].bin - p) <= 3)
                beta = prevPeaks[prevIdx].beta;

            const float w = omega[(size_t) p];
            const float wOut = w * ratio;
            beta = princarg (beta + (wOut - w) * (float) H);
            curPeaks.push_back ({ p, beta });

            const int shift = (int) std::lround ((wOut - w) / binToRad);
            const auto rot = std::polar (1.0f, beta);

            for (int k = lo; k <= hi; ++k)
            {
                const int j = k + shift;
                if (j >= 0 && j <= half)
                    outSpec[(size_t) j] += spec[(size_t) k] * rot;
            }
        }

        std::swap (prevPeaks, curPeaks);
    }

    // Envolvente espectral por cepstrum (log-magnitud suavizada)
    void computeEnvelope() noexcept
    {
        for (int k = 0; k <= half; ++k)
            cep[(size_t) k] = { std::log (mag[(size_t) k] + 1.0e-9f), 0.0f };
        for (int k = 1; k < half; ++k)
            cep[(size_t) (N - k)] = cep[(size_t) k];

        fft.perform (cep.data(), true);
        for (int i = lifter; i <= N - lifter; ++i)
            cep[(size_t) i] = {};
        fft.perform (cep.data(), false);

        for (int k = 0; k <= half; ++k)
            logEnv[(size_t) k] = cep[(size_t) k].real();
    }

    void publishEnvelope() noexcept
    {
        computeEnvelope();
        constexpr float toDb = 8.685889638f;              // 20 / ln(10)
        const float ref = 20.0f * std::log10 ((float) N * 0.25f);   // seno a 0 dBFS con ventana Hann
        const float binPerHz = (float) N / (float) sr;
        float peak = -200.0f;
        for (int i = 0; i < kEnvPoints; ++i)
        {
            // Media en ±1/12 de octava (quita el rizado del cepstrum) y +4.5 dB/oct
            // de inclinación para que los formantes agudos se vean igual que los graves
            const float hz = kEnvLoHz * std::pow (kEnvHiHz / kEnvLoHz, (float) i / (float) (kEnvPoints - 1));
            float sum = 0.0f;
            for (int k = -2; k <= 2; ++k)
                sum += envAt (hz * std::pow (2.0f, (float) k / 24.0f) * binPerHz);
            const float db = toDb * sum / 5.0f - ref;
            dispEnv[(size_t) i].store (db + 4.5f * std::log2 (hz / 1000.0f), std::memory_order_relaxed);
            peak = std::max (peak, db);
        }
        dispLevel.store (peak, std::memory_order_relaxed);
    }

    float envAt (float bin) const noexcept
    {
        if (bin <= 0.0f) return logEnv[0];
        if (bin >= (float) half) return logEnv[(size_t) half];
        const int i = (int) bin;
        const float t = bin - (float) i;
        return logEnv[(size_t) i] + t * (logEnv[(size_t) (i + 1)] - logEnv[(size_t) i]);
    }

    // Lo que estaba en el bin j/ratio tenía la envolvente original allí:
    // se cambia por la envolvente deseada en j/formant.
    void applyFormantCorrection (float ratio, float formant) noexcept
    {
        computeEnvelope();
        constexpr float maxLog = 2.07f;   // ±18 dB
        for (int j = 0; j <= half; ++j)
        {
            const float d = std::clamp (envAt ((float) j / formant) - envAt ((float) j / ratio), -maxLog, maxLog);
            outSpec[(size_t) j] *= std::exp (d);
        }
    }

    // --------------------------------------------------------------- YIN
    void detectPitch() noexcept
    {
        double energy = 0.0;
        for (int i = 0; i < yinLen; ++i)
        {
            float s = 0.0f;
            for (int d = 0; d < decim; ++d) s += lpFifo[(size_t) (i * decim + d)];
            yinX[(size_t) i] = s / (float) decim;
            if (i < yinW) energy += (double) yinX[(size_t) i] * yinX[(size_t) i];
        }

        float periodD = -1.0f;
        if (std::sqrt (energy / yinW) > 1.0e-3)
        {
            double running = 0.0;
            yinD[0] = 1.0f;
            for (int tau = 1; tau <= tauMaxD; ++tau)
            {
                double dsum = 0.0;
                const float* a = yinX.data();
                const float* b = yinX.data() + tau;
                for (int j = 0; j < yinW; ++j)
                {
                    const float diff = a[j] - b[j];
                    dsum += (double) diff * diff;
                }
                running += dsum;
                yinD[(size_t) tau] = running > 0.0 ? (float) (dsum * tau / running) : 1.0f;
            }

            int best = -1;
            for (int tau = tauMinD; tau < tauMaxD; ++tau)
            {
                if (yinD[(size_t) tau] < 0.2f)
                {
                    while (tau + 1 < tauMaxD && yinD[(size_t) (tau + 1)] < yinD[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }

            if (best > 0)
            {
                const float s0 = yinD[(size_t) (best - 1)], s1 = yinD[(size_t) best], s2 = yinD[(size_t) (best + 1)];
                const float den = s0 - 2.0f * s1 + s2;
                const float sh = std::abs (den) > 1.0e-9f ? std::clamp (0.5f * (s0 - s2) / den, -0.5f, 0.5f) : 0.0f;
                periodD = (float) best + sh;
            }
        }

        if (periodD > 0.0f)
        {
            const float hz = (float) (dsr / periodD);
            // Saltos bruscos (errores de octava) se ignoran durante un par de tramas
            if (curF0 > 0.0f && std::abs (12.0f * std::log2 (hz / curF0)) > 9.0f && glitches < 2)
                ++glitches;
            else
            {
                curF0 = hz;
                glitches = 0;
            }
            unvoicedRun = 0;
        }
        else if (++unvoicedRun >= 2)
        {
            curF0 = 0.0f;
            glitches = 0;
        }
        detectedHz.store (curF0, std::memory_order_relaxed);
    }

    // ---------------------------------------------------------- estado
    double sr = 48000.0;
    int N = 2048, H = 512, half = 1024, pos = 0, lifter = 48;
    float olaScale = 1.0f;
    SimpleFFT fft;

    std::vector<float> window, inFifo, lpFifo, acc, outQueue;
    std::vector<std::complex<float>> spec, outSpec, cep;
    std::vector<float> mag, prevMag, phase, prevPhase, omega, logEnv;
    std::vector<int> peaks;
    std::vector<Peak> prevPeaks, curPeaks;

    float lb0 = 0, lb1 = 0, lb2 = 0, la1 = 0, la2 = 0, z1 = 0, z2 = 0;

    int decim = 2, tauMaxD = 0, tauMinD = 0, yinLen = 0, yinW = 0;
    double dsr = 24000.0;
    std::vector<float> yinX, yinD;
    int unvoicedRun = 0, glitches = 0, hopCount = 0;
    float curF0 = 0.0f;

    float pitchSemis = 0.0f, formantSemis = 0.0f;
    Mode mode = Mode::Transpose;
    int midiNote = -1;
    uint16_t allowed = 0xFFF;

    std::atomic<float> detectedHz { 0.0f }, outputHz { 0.0f };

    int envEvery = 6, envCount = 0;
    std::array<std::atomic<float>, kEnvPoints> dispEnv {};
    std::atomic<float> dispLevel { -200.0f };
};


// Motor completo: PSOLA para voz y phase vocoder para mezclas, con fundido según
// lo que entra. Los dos van alineados en el tiempo.
class PitchEngine
{
public:
    void prepare (double sampleRate)
    {
        psola.prepare (sampleRate);
        vocoder.prepare (sampleRate);
        latency = std::max (psola.getLatencySamples(), vocoder.getLatencySamples());
        vocDelay.assign ((size_t) (latency - vocoder.getLatencySamples()), 0.0f);
        psolaDelay.assign ((size_t) (latency - psola.getLatencySamples()), 0.0f);
        fadeStep = (float) (1.0 / (0.25 * sampleRate));
        reset();
    }

    void reset()
    {
        psola.reset();
        vocoder.reset();
        std::fill (vocDelay.begin(), vocDelay.end(), 0.0f);
        std::fill (psolaDelay.begin(), psolaDelay.end(), 0.0f);
        vp = pp = 0;
        weight = voiceSource ? 1.0f : 0.0f;
        vocoderRunning = false;
    }

    int getLatencySamples() const noexcept { return latency; }

    void setPitchSemitones (float s) noexcept   { psola.setPitchSemitones (s); vocoder.setPitchSemitones (s); }
    void setFormantSemitones (float s) noexcept { psola.setFormantSemitones (s); vocoder.setFormantSemitones (s); }
    void setMode (Mode m) noexcept              { psola.setMode (m); vocoder.setMode (m); }
    void setMidiNote (int n) noexcept           { psola.setMidiNote (n); vocoder.setMidiNote (n); }
    void setScale (int root, Scale s) noexcept  { const auto m = scaleMask (root, s); psola.setScale (m); vocoder.setScale (m); }

    float getDetectedHz() const noexcept { return psola.getDetectedHz(); }
    float getOutputHz() const noexcept   { return psola.getOutputHz(); }
    static constexpr int kEnvPoints = PsolaEngine::kEnvPoints;
    static constexpr float kEnvLoHz = PsolaEngine::kEnvLoHz, kEnvHiHz = PsolaEngine::kEnvHiHz;
    float getDisplayEnvelope (float* dest) const noexcept { return psola.getDisplayEnvelope (dest); }
    float getVoiceWeight() const noexcept { return weight; }

    // Fuente: voz (PSOLA) o mezcla / varios instrumentos (vocoder)
    void setVoiceSource (bool voice) noexcept { voiceSource = voice; }

    float process (float x) noexcept
    {
        float a = psola.process (x);   // siempre: detecta el tono y alimenta la pantalla

        const float target = voiceSource ? 1.0f : 0.0f;
        float b = 0.0f;
        if (target < 1.0f || weight < 1.0f)
        {
            if (! vocoderRunning) { vocoder.reset(); std::fill (vocDelay.begin(), vocDelay.end(), 0.0f); vocoderRunning = true; }
            b = vocoder.process (x);
            if (! vocDelay.empty()) { std::swap (vocDelay[vp], b); if (++vp >= vocDelay.size()) vp = 0; }
        }
        else vocoderRunning = false;

        if (! psolaDelay.empty()) { std::swap (psolaDelay[pp], a); if (++pp >= psolaDelay.size()) pp = 0; }

        if (weight < target) weight = std::min (target, weight + fadeStep);
        else if (weight > target) weight = std::max (target, weight - fadeStep);
        return b + weight * (a - b);
    }


private:
    PsolaEngine psola;
    VocoderEngine vocoder;
    int latency = 0;
    std::vector<float> vocDelay, psolaDelay;
    size_t vp = 0, pp = 0;
    float weight = 1.0f, fadeStep = 1.0e-4f;
    bool voiceSource = true, vocoderRunning = false;
};

} // namespace alterivo
