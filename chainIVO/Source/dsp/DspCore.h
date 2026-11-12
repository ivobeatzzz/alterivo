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
    float tick (float db) { const float c = db > v ? a : r; v = db + c * (v - db); return v; }
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

// Información de tempo que reciben los motores sincronizados
struct TempoInfo
{
    double bpm = 120.0;
    double ppq = 0.0;       // posición en negras al inicio del bloque
    bool playing = false;
};
} // namespace chainivo
