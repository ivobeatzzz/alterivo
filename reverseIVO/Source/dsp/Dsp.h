// reverseIVO — utilidades DSP (C++ puro). Todo se calcula fuera del hilo de audio, así que se prioriza la
// calidad: filtros TPT sin artefactos al automatizarlos, interpolación sinc con antialias, saturación sobremuestreada.
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace reverseivo::dsp
{
constexpr double pi = 3.14159265358979323846;
inline float dbToLin (float db) { return db <= -120.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }
inline float linToDb (float g) { return g <= 1e-6f ? -120.0f : 20.0f * std::log10 (g); }
template <typename T> T clamp01 (T v) { return v < (T) 0 ? (T) 0 : (v > (T) 1 ? (T) 1 : v); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

// Curva 0..1 → 0..1. shape 0.5 = lineal; hacia 1 se concentra al final, hacia 0 al principio.
inline float shapeCurve (float t, float shape)
{
    t = clamp01 (t);
    const float k = (shape - 0.5f) * 2.0f;   // -1..1
    if (std::abs (k) < 1e-3f) return t;
    const float e = std::pow (8.0f, k);       // exponente 1/8 .. 8
    return std::pow (t, e);
}
// Interpolación logarítmica de frecuencias
inline float freqLerp (float a, float b, float t) { return a * std::pow (b / a, clamp01 (t)); }

// --------------------------------------------------------------- FFT radix-2 (tamaño potencia de 2)
class FFT
{
public:
    explicit FFT (int size) : n (size), tw ((size_t) size / 2), rev ((size_t) size)
    {
        for (int k = 0; k < n / 2; ++k) tw[(size_t) k] = std::polar (1.0, -2.0 * pi * k / n);
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    void forward (std::vector<std::complex<double>>& x) const { run (x, false); }
    void inverse (std::vector<std::complex<double>>& x) const
    {
        run (x, true);
        for (auto& v : x) v /= (double) n;
    }
    int size() const { return n; }
private:
    void run (std::vector<std::complex<double>>& x, bool inv) const
    {
        for (int i = 0; i < n; ++i) if (rev[(size_t) i] > i) std::swap (x[(size_t) i], x[(size_t) rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < len / 2; ++j)
                {
                    auto w = tw[(size_t) (j * step)]; if (inv) w = std::conj (w);
                    const auto u = x[(size_t) (i + j)], v = x[(size_t) (i + j + len / 2)] * w;
                    x[(size_t) (i + j)] = u + v; x[(size_t) (i + j + len / 2)] = u - v;
                }
        }
    }
    int n;
    std::vector<std::complex<double>> tw;
    std::vector<int> rev;
};

// --------------------------------------------------------------- Filtro de estado variable TPT (Zavalishin)
// Estable con cambios de frecuencia muestra a muestra: ideal para barridos de filtro sin clics.
struct Svf
{
    double ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    // devuelve {lp, bp, hp}
    inline void tick (double x, double fc, double q, double sr, double& lp, double& bp, double& hp)
    {
        fc = std::min (fc, sr * 0.49);
        const double g = std::tan (pi * fc / sr), k = 1.0 / q;
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    double lowpass (double x, double fc, double q, double sr) { double l, b, h; tick (x, fc, q, sr, l, b, h); return l; }
    double highpass (double x, double fc, double q, double sr) { double l, b, h; tick (x, fc, q, sr, l, b, h); return h; }
    // estantería (shelf) a partir del SVF: ganancia lineal g en graves (low) o agudos (high)
    double lowShelf (double x, double fc, double gain, double sr) { double l, b, h; tick (x, fc, 0.707, sr, l, b, h); return x + (gain - 1.0) * l; }
    double highShelf (double x, double fc, double gain, double sr) { double l, b, h; tick (x, fc, 0.707, sr, l, b, h); return x + (gain - 1.0) * h; }
};

struct OnePole
{
    double z = 0;
    double lp (double x, double coef) { z += coef * (x - z); return z; }
    static double coefFor (double fc, double sr) { return 1.0 - std::exp (-2.0 * pi * fc / sr); }
};

// --------------------------------------------------------------- Interpolación sinc con ventana de Kaiser
// rate > 1 (lectura más rápida, tono más agudo) reduce el corte para no generar aliasing.
class SincReader
{
public:
    static constexpr int half = 16;          // 32 coeficientes
    static constexpr int table = 2048;       // resolución fraccional
    SincReader()
    {
        win.resize ((size_t) (half * table + 1));
        const double beta = 8.6, i0b = besselI0 (beta);
        for (int i = 0; i <= half * table; ++i)
        {
            const double x = (double) i / (double) (half * table);
            win[(size_t) i] = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - x * x))) / i0b;
        }
        // núcleo completo (sinc · ventana) para corte 1: el caso más habitual, sin calcular senos
        full.resize (win.size());
        for (int i = 0; i <= half * table; ++i)
        {
            const double t = (double) i / table;
            full[(size_t) i] = (t < 1e-9 ? 1.0 : std::sin (pi * t) / (pi * t)) * win[(size_t) i];
        }
    }
    // lee x en la posición pos (en muestras); cutoff 0..1 (1 = Nyquist)
    float read (const float* x, int n, double pos, double cutoff) const
    {
        const int i0 = (int) std::floor (pos);
        const double frac = pos - i0;
        double acc = 0, wsum = 0;
        if (cutoff >= 0.999)
        {
            for (int k = -half + 1; k <= half; ++k)
            {
                const double at = std::abs ((double) k - frac);
                if (at >= half) continue;
                const double c = full[(size_t) (at * table)];
                wsum += c;
                const int idx = i0 + k;
                if (idx >= 0 && idx < n) acc += c * x[idx];
            }
            return wsum > 1e-9 ? (float) (acc / wsum) : 0.0f;
        }
        for (int k = -half + 1; k <= half; ++k)
        {
            const int idx = i0 + k;
            const double t = (double) k - frac;
            const double at = std::abs (t);
            if (at >= half) continue;
            const double w = win[(size_t) (at / half * (half * table))];
            const double s = at < 1e-9 ? cutoff : std::sin (pi * cutoff * t) / (pi * t);
            const double c = s * w;
            wsum += c;
            if (idx >= 0 && idx < n) acc += c * x[idx];
        }
        return wsum > 1e-9 ? (float) (acc / wsum) : 0.0f;
    }
private:
    static double besselI0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
        return s;
    }
    std::vector<double> win, full;
};

// --------------------------------------------------------------- Sobremuestreo 4x para lo no lineal
// FIR de fase lineal (sinc con ventana) para subir y bajar; el proceso no lineal corre a 4x y vuelve sin aliasing.
class Oversampler4
{
public:
    Oversampler4()
    {
        const int taps = 95;   // impar, centro en 47
        h.resize ((size_t) taps);
        const double fc = 0.5 / 4.0 * 0.92;   // corte justo por debajo del Nyquist original
        double sum = 0;
        for (int i = 0; i < taps; ++i)
        {
            const double m = i - (taps - 1) / 2.0;
            const double s = std::abs (m) < 1e-9 ? 2 * fc : std::sin (2 * pi * fc * m) / (pi * m);
            const double w = 0.42 - 0.5 * std::cos (2 * pi * i / (taps - 1)) + 0.08 * std::cos (4 * pi * i / (taps - 1));
            h[(size_t) i] = s * w; sum += h[(size_t) i];
        }
        for (auto& v : h) v /= sum;
    }
    // aplica f(sample, índice original) al canal entero con sobremuestreo 4x
    template <typename Fn> void process (std::vector<float>& x, Fn&& f) const
    {
        const int n = (int) x.size(), N = n * 4, T = (int) h.size(), d = (T - 1) / 2;
        if (n == 0) return;
        // subir: sólo se multiplican los coeficientes que caen sobre muestras reales (polifase)
        std::vector<double> y ((size_t) N, 0.0);
        for (int i = 0; i < N; ++i)
        {
            double acc = 0;
            for (int m = (int) std::ceil ((i - d) / 4.0); ; ++m)
            {
                const int k = m * 4 + d - i;
                if (k >= T) break;
                if (k >= 0 && m >= 0 && m < n) acc += h[(size_t) k] * 4.0 * x[(size_t) m];
            }
            y[(size_t) i] = f (acc, i / 4);
        }
        // bajar: filtro y diezmado (sólo se calculan las muestras que se quedan)
        for (int i = 0; i < n; ++i)
        {
            double acc = 0;
            for (int k = 0; k < T; ++k) { const int j = i * 4 + k - d; if (j >= 0 && j < N) acc += h[(size_t) k] * y[(size_t) j]; }
            x[(size_t) i] = (float) acc;
        }
    }
private:
    std::vector<double> h;
};

// Saturación tipo cinta/válvula, ligeramente asimétrica (aporta pares), con ganancia compensada
inline double saturate (double x, double drive)
{
    if (drive <= 1e-4) return x;
    const double k = 1.0 + drive * 9.0;
    const double y = std::tanh (k * (x + 0.06 * drive)) - std::tanh (k * 0.06 * drive);
    return y / std::tanh (k) * (1.0 + 0.35 * drive) / (1.0 + 0.35 * drive * k / 10.0);
}

// Ruido rosa (Paul Kellet) con semilla fija: el mismo ajuste siempre da el mismo resultado
class Pink
{
public:
    explicit Pink (uint32_t seed = 0x1234567u) : s (seed) {}
    float white() { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) & 0xffffff) / 8388608.0f - 1.0f; }
    float pink()
    {
        const float w = white();
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f; b6 = w * 0.115926f;
        return p * 0.11f;
    }
private:
    uint32_t s; float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
};

// Rampa de coseno elevado (fundidos sin clic)
inline float fadeCos (float t) { t = clamp01 (t); return 0.5f - 0.5f * (float) std::cos (pi * t); }
} // namespace reverseivo::dsp
