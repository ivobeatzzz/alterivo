// reverseIVO — EFECTOS de espacio (C++ puro, fuera del hilo de audio).
// Reverb: red de retardos realimentados (FDN) de 8 líneas con matriz de Hadamard, difusión de entrada,
// amortiguación de agudos por línea (decaimiento más corto en agudos, como una sala real) y modulación lenta
// para que la cola no suene metálica. Delay: sincronizado al tempo, con ping-pong y filtros en la realimentación.
#pragma once
#include "Dsp.h"
#include <array>

namespace reverseivo::dsp
{
class DelayLine
{
public:
    void prepare (int maxLen) { buf.assign ((size_t) maxLen + 4, 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) % (int) buf.size(); }
    // lectura fraccional (d en muestras, ≥1) con interpolación cúbica
    float read (double d) const
    {
        const int n = (int) buf.size();
        double rp = (double) w - d; while (rp < 0) rp += n;
        const int i1 = (int) rp; const double f = rp - i1;
        auto at = [&] (int i) { if (i < 0) i += n; else if (i >= n) i -= n; return buf[(size_t) i]; };
        const float y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
        const double c0 = y1, c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return (float) (((c3 * f + c2) * f + c1) * f + c0);
    }
private:
    std::vector<float> buf; int w = 0;
};

class Allpass
{
public:
    void prepare (int len, float g_) { d.prepare (len + 2); n = len; g = g_; }
    float tick (float x)
    {
        const float v = d.read (n);
        const float y = -g * x + v;
        d.push (x + g * y);
        return y;
    }
private:
    DelayLine d; int n = 1; float g = 0.6f;
};

class Reverb
{
public:
    // size 0..1, decay en segundos (RT60), predelay ms, damp 0..1
    void prepare (double sr_, float size, float decay, float predelayMs, float damp)
    {
        sr = sr_;
        static const int base[8] = { 1031, 1327, 1523, 1801, 2053, 2311, 2621, 2903 };   // primos, ms·~0.03
        const double scale = (0.35 + 1.3 * size) * sr / 48000.0;
        for (int i = 0; i < 8; ++i)
        {
            len[i] = std::max (8, (int) (base[i] * scale));
            lines[(size_t) i].prepare (len[i] + 64);
            // ganancia por vuelta para un RT60 dado (−60 dB tras decay segundos)
            gain[i] = std::pow (10.0, -3.0 * (double) len[i] / (sr * std::max (0.1f, decay)));
            lp[i] = OnePole();
        }
        dampCoef = OnePole::coefFor (18000.0 * std::pow (0.08, (double) damp), sr);
        static const int ap[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) diff[(size_t) i].prepare ((int) (ap[i] * sr / 48000.0 * (0.6 + 0.6 * size)), 0.62f);
        pre.prepare ((int) (sr * 0.25) + 8);
        preLen = std::max (1.0, predelayMs * 0.001 * sr);
        phase = 0;
    }
    // procesa una muestra estéreo (wet puro)
    void tick (float inL, float inR, float& outL, float& outR)
    {
        pre.push (0.5f * (inL + inR));
        float x = pre.read (preLen);
        for (auto& a : diff) x = a.tick (x);
        double o[8];
        phase += 2.0 * pi * 0.33 / sr; if (phase > 2.0 * pi) phase -= 2.0 * pi;
        for (int i = 0; i < 8; ++i)
        {
            double d = len[i];
            if (i == 2) d += 6.0 * std::sin (phase);          // modulación lenta: cola sin anillos metálicos
            if (i == 5) d += 5.0 * std::sin (phase * 1.37 + 1.0);
            const double y = lines[(size_t) i].read (d);
            o[i] = lp[i].lp (y, dampCoef) * gain[i];
        }
        // Hadamard 8x8 normalizada
        double h[8];
        for (int i = 0; i < 8; ++i) h[i] = o[i];
        for (int s = 1; s < 8; s <<= 1)
            for (int i = 0; i < 8; i += 2 * s)
                for (int j = i; j < i + s; ++j) { const double a = h[j], b = h[j + s]; h[j] = a + b; h[j + s] = a - b; }
        const double nrm = 1.0 / std::sqrt (8.0);
        for (int i = 0; i < 8; ++i) lines[(size_t) i].push ((float) (h[i] * nrm + (i % 2 == 0 ? x : -x) * 0.5));
        outL = (float) ((o[0] + o[2] + o[4] + o[6]) * 0.5);
        outR = (float) ((o[1] + o[3] + o[5] + o[7]) * 0.5);
    }
private:
    double sr = 48000;
    std::array<DelayLine, 8> lines;
    std::array<OnePole, 8> lp;
    int len[8] {};
    double gain[8] {};
    double dampCoef = 0.5;
    std::array<Allpass, 4> diff;
    DelayLine pre; double preLen = 1;
    double phase = 0;
};

class TempoDelay
{
public:
    void prepare (double sr_, double delaySeconds)
    {
        sr = sr_;
        d = std::max (1.0, delaySeconds * sr);
        l.prepare ((int) d + 8); r.prepare ((int) d + 8);
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
    }
    // fb 0..0.95; pingPong: la primera repetición sale por un lado y rebota
    void tick (float inL, float inR, float fb, bool pingPong, float& outL, float& outR)
    {
        const float yl = l.read (d), yr = r.read (d);
        // filtros en la realimentación: las repeticiones se oscurecen y no acumulan graves
        const float fl = (float) lpL.lowpass (hpL.highpass (yl, 180.0, 0.707, sr), 6500.0, 0.707, sr);
        const float fr = (float) lpR.lowpass (hpR.highpass (yr, 180.0, 0.707, sr), 6500.0, 0.707, sr);
        if (pingPong)
        {
            l.push (0.5f * (inL + inR) + fb * fr);
            r.push (fb * fl);
        }
        else
        {
            l.push (inL + fb * fl);
            r.push (inR + fb * fr);
        }
        outL = yl; outR = yr;
    }
private:
    double sr = 48000, d = 1;
    DelayLine l, r;
    Svf hpL, hpR, lpL, lpR;
};
} // namespace reverseivo::dsp
