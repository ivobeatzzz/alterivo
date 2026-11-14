// reverseIVO — SELECCIÓN AUTOMÁTICA de la región. Busca dónde empieza la voz (inicio de la frase) y dónde
// acaba la primera palabra: esa palabra, invertida, es la que mejor «entra» en la frase. El usuario la puede mover.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace reverseivo::analysis
{
struct Region { int start = 0, end = 0; bool found = false; };

inline std::vector<float> envelopeDb (const std::vector<float>& l, const std::vector<float>& r, double sr, int hop)
{
    const int n = (int) l.size(), frames = std::max (1, n / hop);
    std::vector<float> e ((size_t) frames, -120.0f);
    const int win = hop * 2;
    for (int f = 0; f < frames; ++f)
    {
        double s = 0; int c = 0;
        for (int i = f * hop; i < std::min (n, f * hop + win); ++i) { const double m = 0.5 * (l[(size_t) i] + r[(size_t) i]); s += m * m; ++c; }
        e[(size_t) f] = c > 0 && s > 1e-12 ? (float) (10.0 * std::log10 (s / c)) : -120.0f;
    }
    (void) sr;
    return e;
}

// Primera frase y primera palabra dentro del audio capturado
inline Region detectFirstWord (const std::vector<float>& l, const std::vector<float>& r, double sr)
{
    Region reg;
    const int n = (int) l.size();
    if (n < (int) (sr * 0.05)) { reg.start = 0; reg.end = n; return reg; }
    const int hop = std::max (1, (int) (sr * 0.005));   // 5 ms
    const auto e = envelopeDb (l, r, sr, hop);
    const float peak = *std::max_element (e.begin(), e.end());
    if (peak < -70.0f) { reg.start = 0; reg.end = n; return reg; }
    const float thr = std::max (peak - 32.0f, -60.0f);
    int on = 0; while (on < (int) e.size() && e[(size_t) on] < thr) ++on;
    // la palabra acaba en el primer valle claro (o silencio) tras 140 ms de voz, como mucho 1,4 s
    const int minF = on + (int) (0.14 * sr / hop), maxF = std::min ((int) e.size() - 1, on + (int) (1.4 * sr / hop));
    int endF = maxF; float runMax = -120.0f;
    for (int f = on; f <= maxF; ++f)
    {
        runMax = std::max (runMax, e[(size_t) f]);
        if (f >= minF && (e[(size_t) f] < runMax - 14.0f || e[(size_t) f] < thr)) { endF = f; break; }
    }
    // pequeño margen antes del ataque (que no se coma la consonante) y después del final
    reg.start = std::max (0, on * hop - (int) (0.012 * sr));
    reg.end = std::min (n, endF * hop + (int) (0.03 * sr));
    if (reg.end - reg.start < (int) (0.08 * sr)) reg.end = std::min (n, reg.start + (int) (0.3 * sr));
    reg.found = true;
    return reg;
}
} // namespace reverseivo::analysis
