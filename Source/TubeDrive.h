// alterIVO — saturación tipo válvula (se usa a 4x de sobremuestreo)
//
// Curva tanh asimétrica: el sesgo (bias) crece con el Drive, así aparecen
// armónicos pares (color "válvula") además de los impares. La ganancia de
// salida compensa el aumento de nivel para que el Drive no suba el volumen.
#pragma once

#include <algorithm>
#include <cmath>

namespace alterivo
{

class TubeDrive
{
public:
    // amount: 0..1
    void setAmount (float amount) noexcept
    {
        amount = std::clamp (amount, 0.0f, 1.0f);
        active = amount > 1.0e-4f;
        if (std::abs (amount - lastAmount) < 1.0e-5f)
            return;
        lastAmount = amount;

        preGain = std::pow (10.0f, (amount * 36.0f) / 20.0f);        // hasta +36 dB
        bias = 0.22f * amount;
        biasOffset = std::tanh (bias);
        mixIn = std::min (1.0f, amount * 8.0f);                       // entrada suave cerca de 0

        // Compensación de nivel: iguala el RMS de un seno a -18 dBFS RMS
        outGain = 1.0f;
        const int N = 64;
        const float A = 0.177f;
        double inSq = 0.0, outSq = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const float x = A * std::sin (6.2831853f * (float) i / (float) N);
            const float y = process (x);
            inSq += (double) x * x;
            outSq += (double) y * y;
        }
        outGain = outSq > 0.0 ? (float) std::sqrt (inSq / outSq) : 1.0f;
    }

    float process (float x) noexcept
    {
        if (! active)
            return x;
        const float y = (std::tanh (preGain * x + bias) - biasOffset) * outGain;
        return x + mixIn * (y - x);
    }

private:
    bool active = false;
    float lastAmount = -1.0f;
    float preGain = 1.0f, bias = 0.0f, biasOffset = 0.0f, outGain = 1.0f, mixIn = 0.0f;
};

// Filtro DC de 1 polo (la asimetría genera algo de continua)
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        r = (float) std::exp (-2.0 * 3.14159265358979323846 * 10.0 / sampleRate);
        reset();
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

} // namespace alterivo
