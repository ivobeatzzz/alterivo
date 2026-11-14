// reverseIVO — voz sintética para las pruebas (sin archivos externos)
#pragma once
#include "render/Renderer.h"
#include "dsp/Dsp.h"

namespace reverseivo
{
// Voz sintética: dos palabras con silencio, consonante inicial, vibrato y formantes
inline Source makeVoice (double sr)
{
    Source s; s.sampleRate = sr;
    const int n = (int) (sr * 2.4);
    s.l.assign ((size_t) n, 0.0f); s.r.assign ((size_t) n, 0.0f);
    dsp::Pink noise (99);
    dsp::Svf f1, f2, f3, nf;
    double ph = 0;
    struct Word { double t0, t1, f0; } words[] = { { 0.30, 0.78, 196.0 }, { 0.95, 1.85, 233.0 } };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        double amp = 0, f0 = 200, cons = 0;
        for (auto& w : words)
            if (t >= w.t0 && t < w.t1)
            {
                const double u = (t - w.t0) / (w.t1 - w.t0);
                amp = std::min (1.0, (t - w.t0) / 0.03) * std::min (1.0, (w.t1 - t) / 0.08) * (0.8 + 0.2 * std::sin (6.0 * u));
                f0 = w.f0 * (1.0 + 0.012 * std::sin (2 * dsp::pi * 5.5 * t)) * (1.0 - 0.05 * u);
                if (t - w.t0 < 0.05) cons = 1.0 - (t - w.t0) / 0.05;
            }
        ph += 2 * dsp::pi * f0 / sr;
        double glott = 0;
        for (int h = 1; h <= 30; ++h) glott += std::sin (h * ph) / h;
        double v = 0.6 * f1.lowpass (glott, 700.0, 4.0, sr) + 0.35 * f2.lowpass (glott, 1200.0, 5.0, sr) + 0.15 * f3.highpass (glott, 2600.0, 3.0, sr);
        v *= amp * 0.35;
        const double c = nf.highpass (noise.pink(), 3500.0, 0.8, sr) * cons * 0.5 + noise.white() * 0.0005;   // «s» inicial y ruido de fondo
        s.l[(size_t) i] = (float) (v + c);
        s.r[(size_t) i] = (float) (v * 0.93 + c * 1.05);   // un poco de diferencia L/R
    }
    return s;
}

} // namespace reverseivo
