// levelIVO — utilidades DSP sin dependencias de interfaz: filtros, medida de sonoridad, predicción lineal
// e interpolación autorregresiva (para reparar clics y clipping). Todo determinista y sin estado global.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <limits>

namespace levelivo::dsp
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }
inline float linToDb (float v)  { return 20.0f * std::log10 (std::max (v, 1.0e-9f)); }
inline float powToDb (double p) { return (float) (10.0 * std::log10 (std::max (p, 1.0e-12))); }

// Biquad (forma directa II transpuesta, doble precisión en el estado)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    // Diseños RBJ
    static Biquad lowpass (double fs, double f0, double q = 0.7071)
    {
        const double w = 2 * kPi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        Biquad b; b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
    static Biquad highpass (double fs, double f0, double q = 0.7071)
    {
        const double w = 2 * kPi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        Biquad b; b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
    // ITU-R BS.1770 para cualquier frecuencia de muestreo (prototipos analógicos de la norma)
    static Biquad kShelf (double fs)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad b; b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0;
        b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; return b;
    }
    static Biquad kHighpass (double fs)
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPi * f0 / fs), a0 = 1.0 + K / Q + K * K;
        Biquad b; b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0; b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; return b;
    }
};

// Cadena de biquads aplicada a un vector (en el sitio)
inline void runChain (std::vector<Biquad> chain, std::vector<float>& x)
{
    for (auto& b : chain) { b.reset(); for (auto& v : x) v = (float) b.process (v); }
}
// Filtro de fase cero (adelante + atrás) sobre un tramo
inline void zeroPhase (std::vector<Biquad> chain, float* x, int n)
{
    for (auto& b : chain)
    {
        b.reset(); for (int i = 0; i < n; ++i) x[i] = (float) b.process (x[i]);
        b.reset(); for (int i = n - 1; i >= 0; --i) x[i] = (float) b.process (x[i]);
    }
}

// Energía media en dB por tramos de 'hop' muestras con ventana de 'win' muestras centrada
inline std::vector<float> framePowerDb (const std::vector<float>& x, int hop, int win, int nFrames)
{
    std::vector<float> out ((size_t) nFrames, -120.0f);
    const int n = (int) x.size();
    // suma acumulada de cuadrados para ventanas arbitrarias
    std::vector<double> cs ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i) cs[(size_t) i + 1] = cs[(size_t) i] + (double) x[(size_t) i] * x[(size_t) i];
    for (int f = 0; f < nFrames; ++f)
    {
        const int c = f * hop + hop / 2;
        const int a = std::max (0, c - win / 2), b = std::min (n, c + win / 2);
        if (b > a) out[(size_t) f] = powToDb ((cs[(size_t) b] - cs[(size_t) a]) / (b - a));
    }
    return out;
}

template <typename T>
inline T percentile (std::vector<T> v, double p)
{
    if (v.empty()) return T();
    const size_t k = (size_t) std::clamp (p / 100.0 * (double) (v.size() - 1), 0.0, (double) (v.size() - 1));
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}
template <typename T> inline T median (std::vector<T> v) { return percentile (std::move (v), 50.0); }

// Media de energía (en dB) de un tramo de valores en dB
inline float energyMeanDb (const float* db, int n)
{
    if (n <= 0) return -120.0f;
    double s = 0; for (int i = 0; i < n; ++i) s += std::pow (10.0, db[i] / 10.0);
    return powToDb (s / n);
}

// ---------------------------------------------------------------- Predicción lineal
// Levinson-Durbin sobre autocorrelación r[0..p]; devuelve a[0..p] con a[0] = 1
inline std::vector<double> levinson (const std::vector<double>& r, int p)
{
    std::vector<double> a ((size_t) p + 1, 0.0), tmp ((size_t) p + 1);
    a[0] = 1.0;
    double err = r[0] * (1.0 + 1e-9) + 1e-12;
    for (int i = 1; i <= p; ++i)
    {
        double acc = r[(size_t) i];
        for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
        const double k = -acc / err;
        tmp = a;
        for (int j = 1; j < i; ++j) a[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
        a[(size_t) i] = k;
        err *= (1.0 - k * k);
        if (err <= 1e-15) break;
    }
    return a;
}
inline void autocorr (const float* x, int n, int p, std::vector<double>& r, bool window = true)
{
    r.assign ((size_t) p + 1, 0.0);
    std::vector<double> w ((size_t) n);
    for (int i = 0; i < n; ++i) w[(size_t) i] = x[i] * (window ? 0.5 - 0.5 * std::cos (2 * kPi * (i + 0.5) / n) : 1.0);
    for (int k = 0; k <= p; ++k) { double s = 0; for (int i = k; i < n; ++i) s += w[(size_t) i] * w[(size_t) (i - k)]; r[(size_t) k] += s; }
}

// Interpolación autorregresiva por mínimos cuadrados (LSAR, Janssen et al.): rellena x[gapA, gapB)
// usando el contexto a ambos lados. Si 'clipLevel' > 0 aplica la condición de coherencia del clipping:
// la muestra reconstruida no puede quedar por debajo del nivel de recorte ni cambiar de signo.
inline bool lsarInterpolate (float* x, int n, int gapA, int gapB, int order = 32, float clipLevel = 0.0f)
{
    const int g = gapB - gapA;
    if (g <= 0 || gapA < order || gapB + order > n) return false;
    const int ctx = std::min (1024, std::max (4 * order, 256));
    const int la = std::max (0, gapA - ctx), rb = std::min (n, gapB + ctx);
    // Coeficientes AR a partir de los dos contextos
    std::vector<double> r, r2;
    autocorr (x + la, gapA - la, order, r);
    if (rb - gapB > order) { autocorr (x + gapB, rb - gapB, order, r2); for (size_t k = 0; k < r.size(); ++k) r[k] += r2[k]; }
    if (r[0] <= 1e-12) { for (int i = gapA; i < gapB; ++i) x[i] = 0; return true; }
    r[0] *= 1.0001;
    const auto a = levinson (r, order);
    // Sistema: minimizar sum_t (sum_k a_k x[t-k])^2 en t de gapA .. gapB+order-1 respecto a x[gapA..gapB)
    // Normal: M s = -c,  M_ij = sum_t a(t-i) a(t-j),  c_i = sum_t a(t-i) * (parte conocida en t)
    const int p = order;
    std::vector<double> M ((size_t) g * g, 0.0), c ((size_t) g, 0.0);
    // autocorrelación de los coeficientes: M_ij = R_a(|i-j|)
    std::vector<double> ra ((size_t) p + 1, 0.0);
    for (int k = 0; k <= p; ++k) for (int j = 0; j + k <= p; ++j) ra[(size_t) k] += a[(size_t) j] * a[(size_t) (j + k)];
    for (int i = 0; i < g; ++i) for (int j = 0; j < g; ++j) { const int d = std::abs (i - j); if (d <= p) M[(size_t) i * g + j] = ra[(size_t) d]; }
    for (int t = gapA; t < gapB + p; ++t)
    {
        double known = 0;
        for (int k = 0; k <= p; ++k) { const int idx = t - k; if (idx < gapA || idx >= gapB) known += a[(size_t) k] * x[idx]; }
        for (int k = 0; k <= p; ++k) { const int idx = t - k; if (idx >= gapA && idx < gapB) c[(size_t) (idx - gapA)] += a[(size_t) k] * known; }
    }
    // Cholesky (M es simétrica definida positiva)
    for (int i = 0; i < g; ++i) M[(size_t) i * g + i] += 1e-9;
    for (int j = 0; j < g; ++j)
    {
        double d = M[(size_t) j * g + j];
        for (int k = 0; k < j; ++k) d -= M[(size_t) j * g + k] * M[(size_t) j * g + k];
        if (d <= 0) return false;
        d = std::sqrt (d); M[(size_t) j * g + j] = d;
        for (int i = j + 1; i < g; ++i)
        {
            double s = M[(size_t) i * g + j];
            for (int k = 0; k < j; ++k) s -= M[(size_t) i * g + k] * M[(size_t) j * g + k];
            M[(size_t) i * g + j] = s / d;
        }
    }
    std::vector<double> y ((size_t) g);
    for (int i = 0; i < g; ++i) { double s = -c[(size_t) i]; for (int k = 0; k < i; ++k) s -= M[(size_t) i * g + k] * y[(size_t) k]; y[(size_t) i] = s / M[(size_t) i * g + i]; }
    for (int i = g - 1; i >= 0; --i) { double s = y[(size_t) i]; for (int k = i + 1; k < g; ++k) s -= M[(size_t) k * g + i] * y[(size_t) k]; y[(size_t) i] = s / M[(size_t) i * g + i]; }
    for (int i = 0; i < g; ++i)
    {
        float v = (float) y[(size_t) i];
        if (! std::isfinite (v)) return false;
        if (clipLevel > 0.0f)
        {
            const float orig = x[gapA + i];
            const float sgn = orig >= 0 ? 1.0f : -1.0f;
            if (v * sgn < clipLevel) v = sgn * clipLevel;          // coherencia: al menos el nivel recortado
            v = sgn * std::min (std::abs (v), clipLevel * 4.0f);    // límite de seguridad (+12 dB sobre el recorte)
        }
        x[gapA + i] = v;
    }
    return true;
}
} // namespace levelivo::dsp
