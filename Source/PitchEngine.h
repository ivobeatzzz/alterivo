// alterIVO — motor de pitch y formantes (C++ puro, sin dependencias de JUCE)
//
//  · VOICE — PSOLA (pulso a pulso): se marca cada pulso de la voz, se recorta con
//    una ventana de dos periodos y se vuelve a colocar con la nueva separación.
//    Conserva el cuerpo, los ataques y los formantes.
//  · MIX — phase vocoder con bloqueo de fase por picos, para acordes y mezclas.
//  · Estéreo enlazado: el tono, los pulsos y la fase se calculan UNA vez sobre
//    los dos canales y se aplican igual a L y R. La imagen estéreo original se
//    conserva (no se pasa a mono).
//  · Formant: en PSOLA cada grano se estira o se encoge; en el vocoder se cambia
//    la envolvente espectral.
//  · Con Pitch = 0 y Formant = 0 la reconstrucción es exacta.
//  · Perfiles de latencia (LOW / NORMAL / HIGH QUALITY) y calidad offline.
//  · Sin reservas de memoria dentro del hilo de audio: todo se prepara antes.
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
enum class Scale { Chromatic = 0, Major = 1, Minor = 2 };
enum class Profile { Low = 0, Normal = 1, High = 2 };

constexpr int kMaxChannels = 2;

// Escalas: máscara de 12 bits (bit 0 = Do)
inline uint16_t scaleMask (int root, Scale s) noexcept
{
    uint16_t base = 0xFFF;
    if (s == Scale::Major) base = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
    if (s == Scale::Minor) base = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);
    root = ((root % 12) + 12) % 12;
    return (uint16_t) (((base << root) | (base >> (12 - root))) & 0xFFF);
}

inline bool maskAllows (uint16_t mask, int note) noexcept { return (mask >> (((note % 12) + 12) % 12)) & 1; }

inline int nearestAllowed (uint16_t mask, double note) noexcept
{
    const int base = (int) std::lround (note);
    for (int d = 0; d <= 6; ++d)
    {
        const int a = base + d, b = base - d;
        const bool okA = maskAllows (mask, a), okB = maskAllows (mask, b);
        if (okA && okB) return std::abs (a - note) <= std::abs (b - note) ? a : b;
        if (okA) return a;
        if (okB) return b;
    }
    return base;
}

inline double midiToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

// FFT compleja radix-2 (iterativa)
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

// Paso bajo de 2º orden (1 kHz) para detectar el tono
struct LowPass1k
{
    void prepare (double sr)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * 1000.0 / sr;
        const double alpha = std::sin (w0) / (2.0 * 0.7071);
        const double cw = std::cos (w0), a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cw) * 0.5) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
        z1 = z2 = 0.0f;
    }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

// Detector de tono YIN sobre una ventana diezmada de la señal filtrada
class YinDetector
{
public:
    void prepare (double sr, int windowSize, double minHz, double maxHz)
    {
        decim   = std::max (1, (int) std::round (sr / 24000.0));
        dsr     = sr / decim;
        tauMaxD = (int) std::ceil (dsr / minHz) + 2;
        tauMinD = std::max (2, (int) std::floor (dsr / maxHz));
        yinLen  = windowSize / decim;
        yinW    = std::max (16, yinLen - tauMaxD - 1);
        x.assign ((size_t) yinLen, 0.0f);
        d.assign ((size_t) tauMaxD + 2, 0.0f);
    }

    // get(i) devuelve la muestra i (0..windowSize-1) de la ventana. Devuelve el periodo en
    // muestras originales (-1 = sin tono); aperiodicity = valor YIN del mínimo (0 = periódico)
    template <typename Getter>
    float detect (Getter get, bool& energetic, float& aperiodicity) noexcept
    {
        double energy = 0.0;
        for (int i = 0; i < yinLen; ++i)
        {
            float s = 0.0f;
            for (int k = 0; k < decim; ++k) s += get (i * decim + k);
            x[(size_t) i] = s / (float) decim;
            if (i < yinW) energy += (double) x[(size_t) i] * x[(size_t) i];
        }
        aperiodicity = 1.0f;
        energetic = std::sqrt (energy / yinW) > 1.0e-3;
        if (! energetic) return -1.0f;

        double running = 0.0;
        d[0] = 1.0f;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            double dsum = 0.0;
            const float* a = x.data();
            const float* b = x.data() + tau;
            for (int j = 0; j < yinW; ++j)
            {
                const float diff = a[j] - b[j];
                dsum += (double) diff * diff;
            }
            running += dsum;
            d[(size_t) tau] = running > 0.0 ? (float) (dsum * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = tauMinD; tau < tauMaxD; ++tau)
        {
            if (d[(size_t) tau] < 0.2f)
            {
                while (tau + 1 < tauMaxD && d[(size_t) (tau + 1)] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best <= 0) return -1.0f;

        const float s0 = d[(size_t) (best - 1)], s1 = d[(size_t) best], s2 = d[(size_t) (best + 1)];
        const float den = s0 - 2.0f * s1 + s2;
        const float sh = std::abs (den) > 1.0e-9f ? std::clamp (0.5f * (s0 - s2) / den, -0.5f, 0.5f) : 0.0f;
        aperiodicity = s1;
        return ((float) best + sh) * (float) decim;
    }

private:
    int decim = 2, tauMaxD = 0, tauMinD = 0, yinLen = 0, yinW = 0;
    double dsr = 24000.0;
    std::vector<float> x, d;
};

// ============================================================================ PSOLA
class PsolaEngine
{
public:
    void prepare (double sampleRate, int channels, Profile p)
    {
        sr = sampleRate;
        numCh = std::clamp (channels, 1, kMaxChannels);
        profile = p;

        const double winSec = p == Profile::Low ? 0.02 : (p == Profile::High ? 0.08 : 0.04);
        minHz = p == Profile::Low ? 110.0 : (p == Profile::High ? 70.0 : 75.0);
        capStretch = p == Profile::Low;     // LOW: el grano no se alarga con Formant hacia abajo
        N = 1;
        while (N < (int) std::lround (sr * winSec)) N <<= 1;
        pitchHop = p == Profile::High ? N / 8 : N / 4;
        maxPeriod = (int) std::ceil (sr / minHz);
        // Retardo: el detector va N/2 por detrás y un grano de voz grave con Formant -12
        // ocupa hasta 4 periodos (3 si no se alarga). El DAW lo compensa automáticamente.
        latency = N / 2 + (capStretch ? 3 : 4) * maxPeriod + 128;
        unvoicedPeriod = std::max (32, (int) std::lround (sr * 0.004));

        int size = 1;
        while (size < latency * 4 + N * 2) size <<= 1;
        mask = size - 1;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            in[(size_t) c].assign ((size_t) size, 0.0f);
            acc[(size_t) c].assign ((size_t) size, 0.0f);
        }
        lp.assign ((size_t) size, 0.0f);
        lowPass.prepare (sr);
        yin.prepare (sr, N, minHz, kMaxHz);

        // Listas con capacidad de sobra: nunca piden memoria durante el audio
        marks.clear();
        marks.reserve (kListCapacity);
        history.clear();
        history.reserve (kListCapacity);

        // Pantalla de formantes y planitud espectral
        envN = 1;
        while (envN < (int) std::lround (sr * 0.04)) envN <<= 1;
        envHalf = envN / 2;
        fft.prepare (envN);
        window.resize ((size_t) envN);
        for (int i = 0; i < envN; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / envN));
        spec.assign ((size_t) envN, {});
        cep.assign ((size_t) envN, {});
        logEnv.assign ((size_t) envHalf + 1, 0.0f);
        lifter = std::max (8, (int) std::lround (sr / 1000.0));
        envEvery = std::max (pitchHop, (int) std::lround (sr / 30.0));
        scalePendingMax = (int) (0.5 * sr);

        reset();
    }

    void reset()
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            std::fill (in[(size_t) c].begin(), in[(size_t) c].end(), 0.0f);
            std::fill (acc[(size_t) c].begin(), acc[(size_t) c].end(), 0.0f);
        }
        std::fill (lp.begin(), lp.end(), 0.0f);
        lowPass.z1 = lowPass.z2 = 0.0f;
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
        scalePendingAge = 0;
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
    // La escala nueva entra en la siguiente nota (o a los 0,5 s), no a mitad de una
    void setScale (uint16_t m) noexcept         { if (m != pendingAllowed) { pendingAllowed = m; scalePendingAge = 0; } }
    // Síntesis activa: si no hace falta (todo a 0 o modo MIX) sólo se analiza
    void setSynthesisEnabled (bool on) noexcept { if (on && ! synthOn) synth = -1.0; synthOn = on; }
    // Calidad máxima de interpolación (renderizado offline o perfil HIGH QUALITY)
    void setHighQuality (bool hq) noexcept      { hqOffline = hq; }

    float getDetectedHz() const noexcept { return detectedHz.load (std::memory_order_relaxed); }
    float getOutputHz() const noexcept   { return outputHz.load (std::memory_order_relaxed); }

    static constexpr int kEnvPoints = 64;
    static constexpr float kEnvLoHz = 150.0f, kEnvHiHz = 6000.0f;
    float getDisplayEnvelope (float* dest) const noexcept
    {
        for (int i = 0; i < kEnvPoints; ++i) dest[i] = dispEnv[(size_t) i].load (std::memory_order_relaxed);
        return dispLevel.load (std::memory_order_relaxed);
    }

    // ---- Procesado: una muestra por canal
    void process (const float* x, float* y) noexcept
    {
        float sum = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            in[(size_t) c][(size_t) (t & mask)] = x[c];
            sum += x[c];
        }
        lp[(size_t) (t & mask)] = lowPass.process (sum / (float) numCh);   // análisis común a L y R
        ++t;

        if (t % pitchHop == 0) detectPitch();
        if (++envCount >= envEvery) { envCount = 0; publishEnvelope(); }

        placeMarks();
        if (synthOn) synthesize();
        else forgetOldMarks (t - latency);

        const int64_t u = t - latency;
        for (int c = 0; c < numCh; ++c)
        {
            if (u < 0) { y[c] = 0.0f; continue; }
            float& slot = acc[(size_t) c][(size_t) (u & mask)];
            y[c] = slot;
            slot = 0.0f;
        }
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kMaxHz = 1000.0;
    static constexpr size_t kListCapacity = 4096;

    struct Mark { int64_t pos; bool voiced; };
    struct PitchPoint { int64_t centre; float hz; };

    // Interpolación: cúbica (tiempo real) o Lanczos de 8 puntos (máxima calidad)
    float sampleAt (int c, double pos) const noexcept
    {
        const auto& buf = in[(size_t) c];
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float f = (float) (pos - fl);
        if (f == 0.0f) return buf[(size_t) (i & mask)];

        if (hqOffline || profile == Profile::High)
        {
            float s = 0.0f, wsum = 0.0f;
            for (int k = -3; k <= 4; ++k)
            {
                const float xk = (float) k - f;
                const float px = (float) kPi * xk;
                const float w = std::sin (px) / px * std::sin (px * 0.25f) / (px * 0.25f);
                s += w * buf[(size_t) ((i + k) & mask)];
                wsum += w;
            }
            return wsum != 0.0f ? s / wsum : s;
        }

        const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)];
        const float x1 = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    // Tono en un instante, interpolado del historial de detecciones
    float f0At (int64_t pos) const noexcept
    {
        if (history.size() <= historyStart) return 0.0f;
        const size_t lo = historyStart;
        if (pos <= history[lo].centre) return history[lo].hz;
        for (size_t i = lo + 1; i < history.size(); ++i)
        {
            const auto& b = history[i];
            if (pos <= b.centre)
            {
                const auto& a = history[i - 1];
                if (a.hz <= 0.0f || b.hz <= 0.0f)
                    return (pos - a.centre < b.centre - pos) ? a.hz : b.hz;
                const float k = (float) (pos - a.centre) / (float) (b.centre - a.centre);
                return a.hz * std::pow (b.hz / a.hz, k);
            }
        }
        return history.back().hz;
    }

    void pushMark (Mark m) noexcept
    {
        if (marks.size() >= kListCapacity) compactMarks();
        if (marks.size() < kListCapacity) marks.push_back (m);
    }

    void compactMarks() noexcept
    {
        if (marksStart == 0) return;
        marks.erase (marks.begin(), marks.begin() + (long) marksStart);   // no reserva memoria
        lastSynthIndex = lastSynthIndex > marksStart ? lastSynthIndex - marksStart : 0;
        marksStart = 0;
    }

    // ------------------------------------------------------- marcas de pulso
    void placeMarks() noexcept
    {
        const int64_t knownUntil = t - N / 2;   // el detector va N/2 por detrás

        while (true)
        {
            if (marks.size() <= marksStart)
            {
                if (knownUntil < (int64_t) maxPeriod * 2) return;
                pushMark ({ std::max<int64_t> (0, knownUntil - maxPeriod * 2), false });
                continue;
            }

            const Mark last = marks.back();
            const float hz = f0At (last.pos + maxPeriod / 2);
            Mark next;
            if (hz > 0.0f)
            {
                const double T = sr / hz;
                const int64_t predicted = last.pos + (int64_t) std::lround (T);
                const int radius = std::max (2, (int) (T * 0.2));
                if (predicted + radius + (int64_t) std::ceil (T) > knownUntil) return;

                int64_t best = predicted;
                float bestV = -1.0e9f;
                for (int64_t p = predicted - radius; p <= predicted + radius; ++p)
                {
                    const float v = lp[(size_t) (p & mask)];
                    if (v > bestV) { bestV = v; best = p; }
                }
                next = { best, true };
            }
            else
            {
                next = { last.pos + unvoicedPeriod, false };
                if (next.pos + unvoicedPeriod > knownUntil) return;
            }
            pushMark (next);
        }
    }

    // Sin síntesis: se conservan sólo las marcas cercanas a la salida (para retomar sin hueco)
    void forgetOldMarks (int64_t u) noexcept
    {
        while (marksStart + 3 < marks.size() && marks[marksStart + 1].pos < u - 2 * maxPeriod) ++marksStart;
        if (marksStart > 256) compactMarks();
        const float hz = curF0;
        outputHz.store (hz, std::memory_order_relaxed);
    }

    // ------------------------------------------------------------ síntesis
    void synthesize() noexcept
    {
        const int64_t u = t - latency;
        const double f = std::clamp (std::pow (2.0, (double) formantSemis / 12.0), 0.5, 2.0);

        while (true)
        {
            if (marks.size() < marksStart + 3) return;

            if (synth < 0.0)
            {
                // (Re)inicio: la primera marca que todavía no ha salido
                size_t i = marksStart + 1;
                while (i + 1 < marks.size() && marks[i].pos < u) ++i;
                synth = (double) marks[i].pos;
                lastSynthIndex = i;
            }

            size_t i = std::max (lastSynthIndex, marksStart + 1);
            while (i + 1 < marks.size() && std::abs ((double) marks[i + 1].pos - synth) <= std::abs ((double) marks[i].pos - synth))
                ++i;
            if (i + 1 >= marks.size() || (double) marks[i + 1].pos < synth) return;

            const Mark& m = marks[i];
            const int Pl = (int) std::max<int64_t> (1, m.pos - marks[i - 1].pos);
            const int Pr = (int) std::max<int64_t> (1, marks[i + 1].pos - m.pos);

            double advance = Pr, ratio = 1.0;
            const float hz = m.voiced ? f0At (m.pos) : 0.0f;
            if (m.voiced && hz > 0.0f)
            {
                ratio = std::clamp (targetRatio (hz), 0.25, 4.0);
                advance = Pr / ratio;
            }
            outputHz.store (hz > 0.0f ? (float) (hz * ratio) : 0.0f, std::memory_order_relaxed);

            // Grano: ventana Hann asimétrica (un periodo a cada lado), estirada por Formant.
            // El mismo grano (mismas posiciones) para todos los canales: estéreo intacto.
            const int64_t centre = (int64_t) std::llround (synth);
            const float gain = (float) std::sqrt (f / ratio);
            const double Lw = capStretch && f < 1.0 ? Pl : Pl / f;   // medias anchuras en la salida
            const double Rw = capStretch && f < 1.0 ? Pr : Pr / f;
            const int left = (int) std::ceil (Lw), right = (int) std::ceil (Rw);
            for (int j = -left; j <= right; ++j)
            {
                const int64_t o = centre + j;
                if (o < u) continue;
                const double xw = j < 0 ? j / Lw : j / Rw;
                if (xw <= -1.0 || xw >= 1.0) continue;
                const float w = gain * (float) (0.5 + 0.5 * std::cos (kPi * xw));
                const double src = (double) m.pos + (double) j * f;
                for (int c = 0; c < numCh; ++c)
                    acc[(size_t) c][(size_t) (o & mask)] += w * sampleAt (c, src);
            }

            synth += std::max (1.0, advance);
            lastSynthIndex = i;

            while (marksStart + 2 < lastSynthIndex) ++marksStart;
            if (marksStart > 256) compactMarks();
        }
    }

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
                if (quantNote < 0 || std::abs (note - (double) quantNote) > 0.65 || ! maskAllows (allowed, quantNote))
                    quantNote = nearestAllowed (allowed, note);
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

    // --------------------------------------------------------------- YIN
    void detectPitch() noexcept
    {
        const int64_t start = t - N;
        bool energetic = false;
        float aper = 1.0f;
        float period = -1.0f;
        if (start >= 0)
            period = yin.detect ([&] (int i) { return lp[(size_t) ((start + i) & mask)]; }, energetic, aper);

        const int maxGlitches = profile == Profile::High ? 4 : 2;
        if (period > 0.0f)
        {
            const float hz = (float) (sr / period);
            // Saltos bruscos (errores de octava) se ignoran durante unas pocas detecciones
            if (curF0 > 0.0f && std::abs (12.0f * std::log2 (hz / curF0)) > 9.0f && glitches < maxGlitches)
                ++glitches;
            else { curF0 = hz; glitches = 0; }
            unvoicedRun = 0;
        }
        else if (++unvoicedRun >= 2)
        {
            curF0 = 0.0f;
            glitches = 0;
            quantNote = -1;
        }
        detectedHz.store (curF0, std::memory_order_relaxed);

        // Escala nueva: entre notas, o si lleva 0,5 s esperando
        if (pendingAllowed != allowed)
        {
            scalePendingAge += pitchHop;
            if (curF0 <= 0.0f || scalePendingAge >= scalePendingMax) { allowed = pendingAllowed; quantNote = -1; }
        }

        if (history.size() >= kListCapacity && historyStart > 0)
        {
            history.erase (history.begin(), history.begin() + (long) historyStart);
            historyStart = 0;
        }
        if (history.size() < kListCapacity)
            history.push_back ({ t - N / 2, curF0 });
        const int64_t keepFrom = (marks.size() > marksStart ? marks[marksStart].pos : t) - N;
        while (historyStart + 2 < history.size() && history[historyStart + 1].centre < keepFrom) ++historyStart;
        if (historyStart > 256) { history.erase (history.begin(), history.begin() + (long) historyStart); historyStart = 0; }
    }

    // ------------------------------------------------- pantalla de formantes
    void publishEnvelope() noexcept
    {
        const int64_t start = t - envN;
        if (start < 0) return;
        for (int i = 0; i < envN; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < numCh; ++c) s += in[(size_t) c][(size_t) ((start + i) & mask)];
            spec[(size_t) i] = { s / (float) numCh * window[(size_t) i], 0.0f };
        }
        fft.perform (spec.data(), false);
        float total = 0.0f;
        for (int k = 0; k <= envHalf; ++k)
        {
            const float m = std::abs (spec[(size_t) k]);
            total += m;
            cep[(size_t) k] = { std::log (m + 1.0e-9f), 0.0f };
        }
        for (int k = 1; k < envHalf; ++k)
            cep[(size_t) (envN - k)] = cep[(size_t) k];
        fft.perform (cep.data(), true);
        for (int i = lifter; i <= envN - lifter; ++i)
            cep[(size_t) i] = {};
        fft.perform (cep.data(), false);
        for (int k = 0; k <= envHalf; ++k)
            logEnv[(size_t) k] = cep[(size_t) k].real();

        constexpr float toDb = 8.685889638f;
        const float ref = 20.0f * std::log10 ((float) envN * 0.25f);
        const float binPerHz = (float) envN / (float) sr;
        float peak = -200.0f;
        for (int i = 0; i < kEnvPoints; ++i)
        {
            // Media en ±1/12 de octava (quita el rizado) y +4.5 dB/oct de inclinación
            const float hz = kEnvLoHz * std::pow (kEnvHiHz / kEnvLoHz, (float) i / (float) (kEnvPoints - 1));
            float s = 0.0f;
            for (int k = -2; k <= 2; ++k)
                s += envAt (hz * std::pow (2.0f, (float) k / 24.0f) * binPerHz);
            const float db = toDb * s / 5.0f - ref;
            dispEnv[(size_t) i].store (db + 4.5f * std::log2 (hz / 1000.0f), std::memory_order_relaxed);
            peak = std::max (peak, db);
        }
        dispLevel.store (total > 0.0f ? peak : -200.0f, std::memory_order_relaxed);
    }

    float envAt (float bin) const noexcept
    {
        if (bin <= 0.0f) return logEnv[0];
        if (bin >= (float) envHalf) return logEnv[(size_t) envHalf];
        const int i = (int) bin;
        const float k = bin - (float) i;
        return logEnv[(size_t) i] + k * (logEnv[(size_t) (i + 1)] - logEnv[(size_t) i]);
    }

    // ---------------------------------------------------------- estado
    double sr = 48000.0, minHz = 75.0;
    Profile profile = Profile::Normal;
    int numCh = 2;
    int N = 2048, latency = 3072, pitchHop = 512, maxPeriod = 640, unvoicedPeriod = 192;
    bool capStretch = false, synthOn = true, hqOffline = false;
    int64_t mask = 0, t = 0;
    std::array<std::vector<float>, kMaxChannels> in, acc;
    std::vector<float> lp;
    LowPass1k lowPass;
    YinDetector yin;

    std::vector<Mark> marks;
    size_t marksStart = 0, lastSynthIndex = 0;
    std::vector<PitchPoint> history;
    size_t historyStart = 0;
    double synth = -1.0;

    int unvoicedRun = 0, glitches = 0;
    float curF0 = 0.0f;
    int quantNote = -1;

    float pitchSemis = 0.0f, formantSemis = 0.0f;
    Mode mode = Mode::Transpose;
    int midiNote = -1;
    uint16_t allowed = 0xFFF, pendingAllowed = 0xFFF;
    int scalePendingAge = 0, scalePendingMax = 24000;

    SimpleFFT fft;
    int envN = 2048, envHalf = 1024, lifter = 48, envEvery = 1600, envCount = 0;
    std::vector<float> window, logEnv;
    std::vector<std::complex<float>> spec, cep;

    std::atomic<float> detectedHz { 0.0f }, outputHz { 0.0f };
    std::array<std::atomic<float>, kEnvPoints> dispEnv {};
    std::atomic<float> dispLevel { -200.0f };
};

// ===================================================================== phase vocoder
// Mezclas y material polifónico. Los picos y su fase se deciden sobre los dos canales
// juntos y la misma rotación se aplica a L y R, así la diferencia entre canales no cambia.
class VocoderEngine
{
public:
    void prepare (double sampleRate, int channels, Profile p)
    {
        sr = sampleRate;
        numCh = std::clamp (channels, 1, kMaxChannels);
        const double winSec = p == Profile::Low ? 0.02 : (p == Profile::High ? 0.08 : 0.04);
        N = 1;
        while (N < (int) std::lround (sr * winSec)) N <<= 1;
        H = N / 8;
        half = N / 2;
        fft.prepare (N);

        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));
        double wsum = 0.0;
        for (int m = 0; m < N / H; ++m)
            wsum += (double) window[(size_t) (m * H)] * window[(size_t) (m * H)];
        olaScale = (float) (1.0 / wsum);

        for (int c = 0; c < kMaxChannels; ++c)
        {
            auto& s = ch[(size_t) c];
            s.inFifo.assign ((size_t) N, 0.0f);
            s.acc.assign ((size_t) N, 0.0f);
            s.outQueue.assign ((size_t) H, 0.0f);
            s.spec.assign ((size_t) N, {});
            s.outSpec.assign ((size_t) N, {});
            s.prevPhase.assign ((size_t) half + 1, 0.0f);
            s.omega.assign ((size_t) half + 1, 0.0f);
            s.mag.assign ((size_t) half + 1, 0.0f);
        }
        lpFifo.assign ((size_t) N, 0.0f);
        cep.assign ((size_t) N, {});
        mag.assign ((size_t) half + 1, 0.0f);
        prevMag.assign ((size_t) half + 1, 0.0f);
        logEnv.assign ((size_t) half + 1, 0.0f);
        peaks.reserve ((size_t) half);
        prevPeaks.reserve ((size_t) half);
        curPeaks.reserve ((size_t) half);
        lifter = std::max (8, (int) std::lround (sr / 1000.0));
        lowPass.prepare (sr);
        yin.prepare (sr, N, 70.0, 1000.0);
        reset();
    }

    void reset()
    {
        for (auto& s : ch)
        {
            std::fill (s.inFifo.begin(), s.inFifo.end(), 0.0f);
            std::fill (s.acc.begin(), s.acc.end(), 0.0f);
            std::fill (s.outQueue.begin(), s.outQueue.end(), 0.0f);
            std::fill (s.prevPhase.begin(), s.prevPhase.end(), 0.0f);
        }
        std::fill (lpFifo.begin(), lpFifo.end(), 0.0f);
        std::fill (prevMag.begin(), prevMag.end(), 0.0f);
        prevPeaks.clear();
        hopCount = glitches = unvoicedRun = pos = 0;
        lowPass.z1 = lowPass.z2 = 0.0f;
        curF0 = 0.0f;
        quantNote = -1;
    }

    int getLatencySamples() const noexcept { return N; }

    void setPitchSemitones (float s) noexcept   { pitchSemis = s; }
    void setFormantSemitones (float s) noexcept { formantSemis = s; }
    void setMode (Mode m) noexcept              { mode = m; }
    void setMidiNote (int note) noexcept        { midiNote = note; }
    void setScale (uint16_t m) noexcept         { allowed = m; }

    void process (const float* x, float* y) noexcept
    {
        float sum = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            ch[(size_t) c].inFifo[(size_t) (N - H + pos)] = x[c];
            y[c] = ch[(size_t) c].outQueue[(size_t) pos];
            sum += x[c];
        }
        lpFifo[(size_t) (N - H + pos)] = lowPass.process (sum / (float) numCh);

        if (++pos >= H)
        {
            pos = 0;
            processFrame();
        }
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kOnsetFlux = 0.30;

    struct Peak { int bin; float beta; };
    struct Channel
    {
        std::vector<float> inFifo, acc, outQueue, prevPhase, omega, mag;
        std::vector<std::complex<float>> spec, outSpec;
    };

    static float princarg (float a) noexcept
    {
        constexpr float twoPi = 6.28318530717958647692f;
        return a - twoPi * std::floor ((a + 3.14159265358979f) / twoPi);
    }

    void processFrame() noexcept
    {
        if (mode != Mode::Transpose || (hopCount++ & 3) == 0)
            detectPitch();
        const double ratio = std::clamp (currentRatio(), 0.25, 4.0);
        const double formant = std::clamp (std::pow (2.0, (double) formantSemis / 12.0), 0.25, 4.0);

        const float expect = (float) (2.0 * kPi * H / N);
        for (int c = 0; c < numCh; ++c)
        {
            auto& s = ch[(size_t) c];
            for (int i = 0; i < N; ++i)
                s.spec[(size_t) i] = { s.inFifo[(size_t) i] * window[(size_t) i], 0.0f };
            fft.perform (s.spec.data(), false);
            for (int k = 0; k <= half; ++k)
            {
                s.mag[(size_t) k] = std::abs (s.spec[(size_t) k]);
                const float ph = std::arg (s.spec[(size_t) k]);
                const float dphi = princarg (ph - s.prevPhase[(size_t) k] - expect * (float) k);
                s.omega[(size_t) k] = (float) (2.0 * kPi * k / N) + dphi / (float) H;
                s.prevPhase[(size_t) k] = ph;
            }
        }

        // Magnitud conjunta: decide los picos para los dos canales
        double flux = 0.0, total = 0.0;
        for (int k = 0; k <= half; ++k)
        {
            float m = 0.0f;
            for (int c = 0; c < numCh; ++c) m += ch[(size_t) c].mag[(size_t) k];
            mag[(size_t) k] = m;
            flux  += std::max (0.0f, m - prevMag[(size_t) k]);
            total += m;
            prevMag[(size_t) k] = m;
        }

        const bool identity = std::abs (ratio - 1.0) < 1.0e-4 && std::abs (formant - 1.0) < 1.0e-4;
        if (total > 1.0e-3 && flux / total > kOnsetFlux)
            prevPeaks.clear();

        if (identity)
        {
            for (int c = 0; c < numCh; ++c)
                std::copy (ch[(size_t) c].spec.begin(), ch[(size_t) c].spec.end(), ch[(size_t) c].outSpec.begin());
            prevPeaks.clear();
        }
        else
        {
            shiftPeaks ((float) ratio);
            if (std::abs (formant - ratio) > 1.0e-4)
                applyFormantCorrection ((float) ratio, (float) formant);
            for (int c = 0; c < numCh; ++c)
            {
                auto& o = ch[(size_t) c].outSpec;
                o[0] = { o[0].real(), 0.0f };
                o[(size_t) half] = { o[(size_t) half].real(), 0.0f };
                for (int k = 1; k < half; ++k)
                    o[(size_t) (N - k)] = std::conj (o[(size_t) k]);
            }
        }

        for (int c = 0; c < numCh; ++c)
        {
            auto& s = ch[(size_t) c];
            fft.perform (s.outSpec.data(), true);
            for (int i = 0; i < N; ++i)
                s.acc[(size_t) i] += s.outSpec[(size_t) i].real() * window[(size_t) i] * olaScale;
            std::copy (s.acc.begin(), s.acc.begin() + H, s.outQueue.begin());
            std::copy (s.acc.begin() + H, s.acc.end(), s.acc.begin());
            std::fill (s.acc.end() - H, s.acc.end(), 0.0f);
            std::copy (s.inFifo.begin() + H, s.inFifo.end(), s.inFifo.begin());
        }
        std::copy (lpFifo.begin() + H, lpFifo.end(), lpFifo.begin());
    }

    double currentRatio() noexcept
    {
        const double semis = midiNote >= 0 ? (double) (midiNote - 60) : (double) pitchSemis;
        const double transpose = std::pow (2.0, semis / 12.0);
        switch (mode)
        {
            case Mode::Transpose: return transpose;
            case Mode::Quantize:
            {
                if (curF0 <= 0.0f) return transpose;
                const double note = 69.0 + 12.0 * std::log2 (curF0 * transpose / 440.0);
                if (quantNote < 0 || std::abs (note - (double) quantNote) > 0.65 || ! maskAllows (allowed, quantNote))
                    quantNote = nearestAllowed (allowed, note);
                return midiToHz ((double) quantNote) / curF0;
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

    // Laroche & Dolson con picos comunes: la fase acumulada de cada pico se aplica igual a L y R
    void shiftPeaks (float ratio) noexcept
    {
        for (int c = 0; c < numCh; ++c)
            std::fill (ch[(size_t) c].outSpec.begin(), ch[(size_t) c].outSpec.end(), std::complex<float> {});

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
        if (peaks.empty()) { prevPeaks.clear(); return; }

        size_t prevIdx = 0;
        const float binToRad = (float) (2.0 * kPi / N);
        for (size_t pi = 0; pi < peaks.size(); ++pi)
        {
            const int p = peaks[pi];
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

            float beta = 0.0f;
            while (prevIdx + 1 < prevPeaks.size()
                   && std::abs (prevPeaks[prevIdx + 1].bin - p) <= std::abs (prevPeaks[prevIdx].bin - p))
                ++prevIdx;
            if (! prevPeaks.empty() && std::abs (prevPeaks[prevIdx].bin - p) <= 3)
                beta = prevPeaks[prevIdx].beta;

            // Frecuencia del pico: la del canal más fuerte en ese punto
            int dom = 0;
            for (int c = 1; c < numCh; ++c)
                if (ch[(size_t) c].mag[(size_t) p] > ch[(size_t) dom].mag[(size_t) p]) dom = c;
            const float w = ch[(size_t) dom].omega[(size_t) p];
            const float wOut = w * ratio;
            beta = princarg (beta + (wOut - w) * (float) H);
            curPeaks.push_back ({ p, beta });

            const int shift = (int) std::lround ((wOut - w) / binToRad);
            const auto rot = std::polar (1.0f, beta);
            for (int c = 0; c < numCh; ++c)
            {
                auto& s = ch[(size_t) c];
                for (int k = lo; k <= hi; ++k)
                {
                    const int j = k + shift;
                    if (j >= 0 && j <= half)
                        s.outSpec[(size_t) j] += s.spec[(size_t) k] * rot;
                }
            }
        }
        std::swap (prevPeaks, curPeaks);
    }

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

    float envAt (float bin) const noexcept
    {
        if (bin <= 0.0f) return logEnv[0];
        if (bin >= (float) half) return logEnv[(size_t) half];
        const int i = (int) bin;
        const float k = bin - (float) i;
        return logEnv[(size_t) i] + k * (logEnv[(size_t) (i + 1)] - logEnv[(size_t) i]);
    }

    // La misma corrección de envolvente (calculada sobre los dos canales) para L y R
    void applyFormantCorrection (float ratio, float formant) noexcept
    {
        computeEnvelope();
        constexpr float maxLog = 2.07f;   // ±18 dB
        for (int j = 0; j <= half; ++j)
        {
            const float g = std::exp (std::clamp (envAt ((float) j / formant) - envAt ((float) j / ratio), -maxLog, maxLog));
            for (int c = 0; c < numCh; ++c) ch[(size_t) c].outSpec[(size_t) j] *= g;
        }
    }

    void detectPitch() noexcept
    {
        bool energetic = false;
        float aper = 1.0f;
        const float period = yin.detect ([&] (int i) { return lpFifo[(size_t) i]; }, energetic, aper);
        if (period > 0.0f)
        {
            const float hz = (float) (sr / period);
            if (curF0 > 0.0f && std::abs (12.0f * std::log2 (hz / curF0)) > 9.0f && glitches < 2) ++glitches;
            else { curF0 = hz; glitches = 0; }
            unvoicedRun = 0;
        }
        else if (++unvoicedRun >= 2) { curF0 = 0.0f; glitches = 0; quantNote = -1; }
    }

    double sr = 48000.0;
    int numCh = 2, N = 2048, H = 256, half = 1024, pos = 0, lifter = 48;
    float olaScale = 1.0f;
    SimpleFFT fft;
    std::vector<float> window, lpFifo, mag, prevMag, logEnv;
    std::vector<std::complex<float>> cep;
    std::array<Channel, kMaxChannels> ch;
    std::vector<int> peaks;
    std::vector<Peak> prevPeaks, curPeaks;
    LowPass1k lowPass;
    YinDetector yin;
    int hopCount = 0, glitches = 0, unvoicedRun = 0, quantNote = -1;
    float curF0 = 0.0f;
    float pitchSemis = 0.0f, formantSemis = 0.0f;
    Mode mode = Mode::Transpose;
    int midiNote = -1;
    uint16_t allowed = 0xFFF;
};

// ======================================================================= motor completo
// VOICE -> PSOLA, MIX -> vocoder, con fundido de 250 ms al cambiar. Los dos van
// alineados en el tiempo y procesan todos los canales juntos.
class PitchEngine
{
public:
    void prepare (double sampleRate, int channels, Profile p)
    {
        numCh = std::clamp (channels, 1, kMaxChannels);
        psola.prepare (sampleRate, numCh, p);
        vocoder.prepare (sampleRate, numCh, p);
        latency = std::max (psola.getLatencySamples(), vocoder.getLatencySamples());
        for (int c = 0; c < kMaxChannels; ++c)
        {
            vocDelay[(size_t) c].assign ((size_t) (latency - vocoder.getLatencySamples()), 0.0f);
            psolaDelay[(size_t) c].assign ((size_t) (latency - psola.getLatencySamples()), 0.0f);
        }
        fadeStep = (float) (1.0 / (0.25 * sampleRate));
        reset();
    }

    void reset()
    {
        psola.reset();
        vocoder.reset();
        for (int c = 0; c < kMaxChannels; ++c)
        {
            std::fill (vocDelay[(size_t) c].begin(), vocDelay[(size_t) c].end(), 0.0f);
            std::fill (psolaDelay[(size_t) c].begin(), psolaDelay[(size_t) c].end(), 0.0f);
        }
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
    void setVoiceSource (bool voice) noexcept   { voiceSource = voice; }
    void setActive (bool on) noexcept           { active = on; }
    void setHighQuality (bool hq) noexcept      { psola.setHighQuality (hq); }

    float getDetectedHz() const noexcept { return psola.getDetectedHz(); }
    float getOutputHz() const noexcept   { return psola.getOutputHz(); }
    static constexpr int kEnvPoints = PsolaEngine::kEnvPoints;
    static constexpr float kEnvLoHz = PsolaEngine::kEnvLoHz, kEnvHiHz = PsolaEngine::kEnvHiHz;
    float getDisplayEnvelope (float* dest) const noexcept { return psola.getDisplayEnvelope (dest); }
    float getVoiceWeight() const noexcept { return weight; }

    void process (const float* x, float* y) noexcept
    {
        const float target = voiceSource ? 1.0f : 0.0f;

        // PSOLA siempre analiza (tono y pantalla); sólo sintetiza cuando se oye
        psola.setSynthesisEnabled (active && (target > 0.0f || weight > 0.0f));
        float a[kMaxChannels] {}, b[kMaxChannels] {};
        psola.process (x, a);

        if (active && (target < 1.0f || weight < 1.0f))
        {
            if (! vocoderRunning)
            {
                vocoder.reset();
                for (auto& d : vocDelay) std::fill (d.begin(), d.end(), 0.0f);
                vocoderRunning = true;
            }
            vocoder.process (x, b);
        }
        else vocoderRunning = false;

        for (int c = 0; c < numCh; ++c)
        {
            if (! psolaDelay[(size_t) c].empty()) std::swap (psolaDelay[(size_t) c][pp], a[c]);
            if (! vocDelay[(size_t) c].empty())   std::swap (vocDelay[(size_t) c][vp], b[c]);
        }
        if (! psolaDelay[0].empty() && ++pp >= psolaDelay[0].size()) pp = 0;
        if (! vocDelay[0].empty() && ++vp >= vocDelay[0].size()) vp = 0;

        if (weight < target) weight = std::min (target, weight + fadeStep);
        else if (weight > target) weight = std::max (target, weight - fadeStep);
        for (int c = 0; c < numCh; ++c)
            y[c] = b[c] + weight * (a[c] - b[c]);
    }

private:
    PsolaEngine psola;
    VocoderEngine vocoder;
    int numCh = 2, latency = 0;
    std::array<std::vector<float>, kMaxChannels> vocDelay, psolaDelay;
    size_t vp = 0, pp = 0;
    float weight = 1.0f, fadeStep = 1.0e-4f;
    bool voiceSource = true, vocoderRunning = false, active = true;
};

} // namespace alterivo
