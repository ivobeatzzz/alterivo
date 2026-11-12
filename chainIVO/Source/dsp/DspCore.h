// chainIVO — infraestructura DSP común a todos los motores.
// Reglas: nada de memoria nueva ni bloqueos dentro de process(); todo se prepara en prepare().
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <atomic>
#include <complex>

namespace chainivo
{
constexpr int kMaxCh = 2;
constexpr float kPi = juce::MathConstants<float>::pi;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float powToDb (float p)   { return 10.0f * std::log10 (std::max (p, 1.0e-12f)); }
inline float msToCoeff (float ms, double sr) { return ms <= 0.0f ? 0.0f : std::exp (-1.0f / (0.001f * ms * (float) sr)); }

// Calidad de procesamiento: cambia de verdad el algoritmo o su coste (ver ARQUITECTURA.md)
enum class Quality { eco = 0, normal, high, ultra };

// Valor que se desliza hacia su objetivo (evita clics al automatizar)
struct Smooth
{
    float cur = 0.0f, target = 0.0f, coeff = 0.0f;
    void prepare (double sr, float ms) { coeff = msToCoeff (ms, sr); }
    void reset (float v) { cur = target = v; }
    void set (float v) { target = v; }
    float next() { cur = target + coeff * (cur - target); return cur; }
    bool settled() const { return std::abs (cur - target) < 1.0e-5f; }
};

// Filtro de estado variable de Simper (TPT): estable al modular, bien condicionado en float.
// Da a la vez paso bajo, banda y alto, y x == lp + k*bp + hp (reconstrucción exacta).
struct Svf
{
    float g = 0.0f, k = 1.4142f, a1 = 0, a2 = 0, a3 = 0;
    float ic1[kMaxCh] {}, ic2[kMaxCh] {};

    void setup (double sr, float freq, float q)
    {
        freq = juce::jlimit (5.0f, (float) sr * 0.49f, freq);
        g = std::tan (kPi * freq / (float) sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void scaleG (float m) { g *= m; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    void reset() { std::fill (std::begin (ic1), std::end (ic1), 0.0f); std::fill (std::begin (ic2), std::end (ic2), 0.0f); }

    struct Out { float lp, bp, hp; };
    Out tick (int ch, float x)
    {
        const float v3 = x - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        return { v2, v1, x - k * v1 - v2 };
    }
};

// Bandas de EQ basadas en el SVF (formulación de Simper para campana y estantes)
struct EqBand
{
    enum Type { highPass, lowShelf, bell, highShelf };
    Type type = bell;
    Svf f;
    float m0 = 1, m1 = 0, m2 = 0;   // salida = m0*x + m1*bp + m2*lp (para cada tipo)
    bool hpMode = false;

    void setup (double sr, Type t, float freq, float q, float gainDb)
    {
        type = t;
        const float A = std::pow (10.0f, gainDb / 40.0f);
        switch (t)
        {
            case highPass:
                f.setup (sr, freq, q); hpMode = true; break;
            case bell:
            {
                // Q constante en la campana: k = 1/(Q*A)
                f.setup (sr, freq, q * A);
                m0 = 1.0f; m1 = f.k * (A * A - 1.0f); m2 = 0.0f; hpMode = false; break;
            }
            case lowShelf:
            {
                f.setup (sr, freq, q); f.scaleG (1.0f / std::sqrt (A));
                m0 = 1.0f; m1 = f.k * (A - 1.0f); m2 = A * A - 1.0f; hpMode = false; break;
            }
            case highShelf:
            {
                f.setup (sr, freq, q); f.scaleG (std::sqrt (A));
                m0 = A * A; m1 = f.k * (1.0f - A) * A; m2 = 1.0f - A * A; hpMode = false; break;
            }
        }
    }
    float tick (int ch, float x)
    {
        auto o = f.tick (ch, x);
        if (hpMode) return o.hp;
        return m0 * x + m1 * o.bp + m2 * o.lp;
    }
    // Respuesta en magnitud (para dibujar la curva real del EQ)
    float magnitudeDb (double sr, float freq) const
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const std::complex<double> z = std::polar (1.0, w);
        // Transformación bilineal del prototipo analógico del SVF: s = (z-1)/(z+1) / g
        const std::complex<double> s = (z - 1.0) / (z + 1.0) / (double) f.g;
        const std::complex<double> den = s * s + (double) f.k * s + 1.0;
        std::complex<double> h;
        if (hpMode) h = s * s / den;
        else h = (double) m0 + (double) m1 * (s / den) + (double) m2 * (1.0 / den);
        return (float) (20.0 * std::log10 (std::max (1.0e-9, std::abs (h))));
    }
    void reset() { f.reset(); }
};

// Bloqueo de continua (después de saturaciones asimétricas)
struct DcBlock
{
    float R = 0.9995f, x1[kMaxCh] {}, y1[kMaxCh] {};
    void prepare (double sr) { R = 1.0f - (2.0f * kPi * 8.0f / (float) sr); reset(); }
    void reset() { std::fill (std::begin (x1), std::end (x1), 0.0f); std::fill (std::begin (y1), std::end (y1), 0.0f); }
    float tick (int ch, float x) { const float y = x - x1[ch] + R * y1[ch]; x1[ch] = x; y1[ch] = y; return y; }
};

// Línea de retardo con lectura fraccionaria (lineal o Hermite de 4 puntos)
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float tap (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    void addAgo (int d, float v) { buf[(size_t) ((w - 1 - d) & mask)] += v; }   // suma en una muestra ya escrita (aún no leída)
    float readLinear (float d) const
    {
        const int i = (int) d; const float f = d - (float) i;
        return tap (i) + f * (tap (i + 1) - tap (i));
    }
    float readHermite (float d) const
    {
        const int i = (int) d; const float f = d - (float) i;
        const float y0 = tap (i - 1 < 0 ? 0 : i - 1), y1 = tap (i), y2 = tap (i + 1), y3 = tap (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

// Seguidor de envolvente en dB con ataque y relevo separados
struct EnvDb
{
    float a = 0, r = 0, v = -120.0f;
    void prepare (double sr, float attMs, float relMs) { a = msToCoeff (attMs, sr); r = msToCoeff (relMs, sr); v = -120.0f; }
    float tick (float db)
    {
        if (! (db > -200.0f)) db = -200.0f;   // también atrapa NaN: un valor roto nunca se queda dentro
        const float c = db > v ? a : r; v = db + c * (v - db); return v;
    }
    void reset() { v = -120.0f; }
};

// Medidor de pico para la interfaz (lo lee el hilo de la interfaz)
struct PeakMeter
{
    std::atomic<float> peak { 0.0f };
    void feed (const float* const* ch, int nch, int n)
    {
        float p = 0.0f;
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i) p = std::max (p, std::abs (ch[c][i]));
        float old = peak.load();
        if (p > old) peak.store (p);
    }
    float take() { return peak.exchange (0.0f); }
};

// Notas para los efectos sincronizados al tempo (en negras)
inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 bar" };
    return n;
}
inline double noteBeats (int idx)
{
    static const double b[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
    return b[juce::jlimit (0, 10, idx)];
}

// Limitador de picos con anticipación (salida de la cadena).
// Ganancia necesaria por muestra -> mínimo en una ventana de la anticipación -> media de la misma
// longitud (rampa suave) -> recuperación exponencial. Así la ganancia ya ha bajado cuando llega el pico:
// la salida no pasa del techo y no hay distorsión de recorte. Estéreo enlazado. Sin memoria nueva en process.
class PeakLimiter
{
public:
    void prepare (double sampleRate, float lookMs = 1.5f, float releaseMs = 80.0f)
    {
        la = std::max (1, (int) std::lround (sampleRate * lookMs * 0.001));
        cap = la + 2;
        for (auto& d : delay) d.assign ((size_t) la, 0.0f);
        dqIdx.assign ((size_t) cap, 0); dqVal.assign ((size_t) cap, 1.0f);
        box.assign ((size_t) la, 1.0f);
        rel = (float) (1.0 - std::exp (-1.0 / (releaseMs * 0.001 * sampleRate)));
        reset();
    }
    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        boxSum = (double) la; dp = 0; bp = 0; t = 0; head = tail = 0; gain = 1.0f;
    }
    int latency() const noexcept { return la; }

    // Devuelve la ganancia mínima aplicada en el bloque (1 = no ha tocado nada)
    float process (float* const* ch, int nch, int n, float ceiling, bool active) noexcept
    {
        float minGain = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            for (int c = 0; c < nch; ++c) pk = std::max (pk, std::abs (ch[c][i]));
            const float req = active && pk > ceiling ? ceiling / pk : 1.0f;
            // mínimo deslizante de las últimas la+1 ganancias necesarias (cola monótona)
            while (head != tail && dqVal[(size_t) ((tail + cap - 1) % cap)] >= req) tail = (tail + cap - 1) % cap;
            dqIdx[(size_t) tail] = t; dqVal[(size_t) tail] = req; tail = (tail + 1) % cap;
            while (dqIdx[(size_t) head] < t - la) head = (head + 1) % cap;
            const float s = dqVal[(size_t) head];
            // media de la anticipación: la ganancia baja en rampa y llega justo al pico
            boxSum += (double) s - (double) box[(size_t) bp]; box[(size_t) bp] = s; if (++bp >= la) bp = 0;
            const float b = (float) (boxSum / (double) la);
            gain = b < gain ? b : gain + (b - gain) * rel;
            minGain = std::min (minGain, gain);
            for (int c = 0; c < nch; ++c)
            {
                const float x = ch[c][i];
                ch[c][i] = delay[(size_t) c][(size_t) dp] * gain;
                delay[(size_t) c][(size_t) dp] = x;
            }
            if (++dp >= la) dp = 0;
            ++t;
        }
        return minGain;
    }

private:
    int la = 64, cap = 66, dp = 0, bp = 0, head = 0, tail = 0;
    juce::int64 t = 0;
    std::array<std::vector<float>, 2> delay;
    std::vector<juce::int64> dqIdx; std::vector<float> dqVal, box;
    double boxSum = 0.0;
    float rel = 0.001f, gain = 1.0f;
};

// Información de tempo que reciben los motores sincronizados
struct TempoInfo
{
    double bpm = 120.0;
    double ppq = 0.0;       // posición en negras al inicio del bloque
    int beatsPerBar = 4;    // numerador del compás
    bool playing = false;
};
} // namespace chainivo
