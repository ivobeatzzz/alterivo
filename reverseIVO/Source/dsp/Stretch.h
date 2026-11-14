// reverseIVO — ESTIRAMIENTO TEMPORAL de alta calidad (vocoder de fase, fuera del hilo de audio).
// - Bloqueo de fase por picos (Laroche–Dolson): voces sin el sonido «metálico» del vocoder clásico.
// - Estéreo coherente: la rotación de fase se calcula sobre el canal medio y se aplica igual a L y R,
//   así la imagen estéreo no se emborrona.
// - Reinicio de fase en transitorios: los ataques siguen siendo ataques.
// - Modo «smear» para la capa de textura: fases aleatorias y espectro difuminado en el tiempo (pad vocal).
#pragma once
#include "Dsp.h"
#include <random>

namespace reverseivo::dsp
{
struct Stereo
{
    std::vector<float> l, r;
    int size() const { return (int) l.size(); }
    void resize (int n) { l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f); }
};

class Stretcher
{
public:
    // Devuelve la entrada estirada a outLen muestras (tono intacto)
    static Stereo stretch (const Stereo& in, int outLen, double sampleRate)
    {
        Stereo out; out.resize (outLen);
        const int inLen = in.size();
        if (inLen == 0 || outLen <= 0) return out;
        const double ratio = (double) outLen / (double) inLen;
        if (std::abs (ratio - 1.0) < 0.002)   // prácticamente igual: copia exacta, sin procesar
        {
            for (int i = 0; i < outLen; ++i) { const int j = std::min (i, inLen - 1); out.l[(size_t) i] = in.l[(size_t) j]; out.r[(size_t) i] = in.r[(size_t) j]; }
            return out;
        }
        const int N = sampleRate > 64000.0 ? 4096 : 2048, Hs = N / 4;
        process (in, out, N, Hs, ratio, false, 0.0f);
        return out;
    }

    // Textura: espectro difuminado (amount 0..1) con fases aleatorias, independiente en L y R (muy abierto)
    static Stereo smear (const Stereo& in, int outLen, double sampleRate, float amount, uint32_t seed = 7)
    {
        Stereo out; out.resize (outLen);
        if (in.size() == 0 || outLen <= 0) return out;
        const int N = sampleRate > 64000.0 ? 8192 : 4096, Hs = N / 4;
        process (in, out, N, Hs, (double) outLen / (double) in.size(), true, amount, seed);
        return out;
    }

private:
    static void process (const Stereo& in, Stereo& out, int N, int Hs, double ratio, bool smearMode, float smearAmt, uint32_t seed = 7)
    {
        const int inLen = in.size(), outLen = out.size(), half = N / 2;
        const double Ha = (double) Hs / ratio;
        FFT fft (N);
        std::vector<double> win ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * pi * i / N);
        std::vector<std::complex<double>> XL ((size_t) N), XR ((size_t) N), XM ((size_t) N);
        std::vector<double> prevPhaseA ((size_t) half + 1, 0.0), phaseS ((size_t) half + 1, 0.0), mag ((size_t) half + 1), prevMag ((size_t) half + 1, 0.0);
        std::vector<double> smL ((size_t) half + 1, 0.0), smR ((size_t) half + 1, 0.0);
        std::vector<double> accL ((size_t) outLen + N, 0.0), accR ((size_t) outLen + N, 0.0), norm ((size_t) outLen + N, 0.0);
        std::mt19937 rng (seed);
        std::uniform_real_distribution<double> uni (-pi, pi);
        auto sample = [&] (const std::vector<float>& v, long i) { return (i >= 0 && i < inLen) ? (double) v[(size_t) i] : 0.0; };

        const int frames = (int) std::ceil ((double) outLen / Hs) + 2;
        long prevStart = 0; bool first = true;
        const double smearCoef = 1.0 - 0.92 * (double) smearAmt;   // cuánto se renueva el espectro en cada trama
        for (int f = 0; f < frames; ++f)
        {
            const long aStart = (long) std::llround (f * Ha) - half;   // trama de análisis centrada en f·Ha
            const long sStart = (long) f * Hs - half;                  // trama de síntesis centrada en f·Hs
            for (int i = 0; i < N; ++i)
            {
                const double w = win[(size_t) i];
                const double l = sample (in.l, aStart + i) * w, r = sample (in.r, aStart + i) * w;
                XL[(size_t) i] = { l, 0.0 }; XR[(size_t) i] = { r, 0.0 }; XM[(size_t) i] = { 0.5 * (l + r), 0.0 };
            }
            fft.forward (XL); fft.forward (XR); fft.forward (XM);
            const double hopA = first ? Ha : (double) (aStart - prevStart);

            if (smearMode)
            {
                for (int k = 0; k <= half; ++k)
                {
                    smL[(size_t) k] += smearCoef * (std::abs (XL[(size_t) k]) - smL[(size_t) k]);
                    smR[(size_t) k] += smearCoef * (std::abs (XR[(size_t) k]) - smR[(size_t) k]);
                    XL[(size_t) k] = std::polar (smL[(size_t) k], uni (rng));
                    XR[(size_t) k] = std::polar (smR[(size_t) k], uni (rng));
                }
            }
            else
            {
                // transitorio: flujo espectral positivo grande → se reinician las fases (ataque nítido)
                double flux = 0, energy = 0;
                for (int k = 0; k <= half; ++k)
                {
                    mag[(size_t) k] = std::abs (XM[(size_t) k]);
                    flux += std::max (0.0, mag[(size_t) k] - prevMag[(size_t) k]);
                    energy += mag[(size_t) k];
                }
                const bool transient = ! first && energy > 1e-6 && flux > 0.55 * energy;
                // picos del espectro medio
                std::vector<int> peakOf ((size_t) half + 1, -1);
                std::vector<int> peaks;
                for (int k = 2; k < half - 1; ++k)
                    if (mag[(size_t) k] > mag[(size_t) k - 1] && mag[(size_t) k] >= mag[(size_t) k + 1]
                        && mag[(size_t) k] > mag[(size_t) k - 2] && mag[(size_t) k] >= mag[(size_t) k + 2])
                        peaks.push_back (k);
                if (peaks.empty()) peaks.push_back (1);
                // cada bin pertenece al pico más cercano (frontera en el mínimo entre picos)
                size_t p = 0;
                for (int k = 0; k <= half; ++k)
                {
                    while (p + 1 < peaks.size() && std::abs (peaks[p + 1] - k) < std::abs (peaks[p] - k)) ++p;
                    peakOf[(size_t) k] = peaks[p];
                }
                std::vector<double> phaseA ((size_t) half + 1);
                for (int k = 0; k <= half; ++k) phaseA[(size_t) k] = std::arg (XM[(size_t) k]);
                std::vector<double> newPhaseS ((size_t) half + 1);
                for (int pk : peaks)
                {
                    const double omega = 2.0 * pi * pk / N;
                    double dphi = phaseA[(size_t) pk] - prevPhaseA[(size_t) pk] - omega * hopA;
                    dphi -= 2.0 * pi * std::round (dphi / (2.0 * pi));
                    const double instFreq = omega + dphi / std::max (1.0, hopA);
                    newPhaseS[(size_t) pk] = (first || transient) ? phaseA[(size_t) pk] : phaseS[(size_t) pk] + Hs * instFreq;
                }
                for (int k = 0; k <= half; ++k)
                {
                    const int pk = peakOf[(size_t) k];
                    if (k != pk) newPhaseS[(size_t) k] = newPhaseS[(size_t) pk] + (phaseA[(size_t) k] - phaseA[(size_t) pk]);
                }
                for (int k = 0; k <= half; ++k)
                {
                    const double rot = newPhaseS[(size_t) k] - phaseA[(size_t) k];
                    const auto c = std::polar (1.0, rot);
                    XL[(size_t) k] *= c; XR[(size_t) k] *= c;
                    phaseS[(size_t) k] = newPhaseS[(size_t) k];
                    prevPhaseA[(size_t) k] = phaseA[(size_t) k];
                    prevMag[(size_t) k] = mag[(size_t) k];
                }
            }
            for (int k = 1; k < half; ++k) { XL[(size_t) (N - k)] = std::conj (XL[(size_t) k]); XR[(size_t) (N - k)] = std::conj (XR[(size_t) k]); }
            XL[(size_t) half] = { XL[(size_t) half].real(), 0.0 }; XR[(size_t) half] = { XR[(size_t) half].real(), 0.0 };
            XL[0] = { XL[0].real(), 0.0 }; XR[0] = { XR[0].real(), 0.0 };
            fft.inverse (XL); fft.inverse (XR);
            for (int i = 0; i < N; ++i)
            {
                const long o = sStart + i + half;   // desplazado half para no tener índices negativos
                if (o < 0 || o >= (long) accL.size()) continue;
                const double w = win[(size_t) i];
                accL[(size_t) o] += XL[(size_t) i].real() * w;
                accR[(size_t) o] += XR[(size_t) i].real() * w;
                norm[(size_t) o] += w * w;
            }
            prevStart = aStart; first = false;
        }
        const double smearGain = smearMode ? 1.0 : 1.0;
        for (int i = 0; i < outLen; ++i)
        {
            const size_t o = (size_t) (i + half);
            const double nn = norm[o] > 1e-3 ? norm[o] : 1e-3;
            out.l[(size_t) i] = (float) (accL[o] / nn * smearGain);
            out.r[(size_t) i] = (float) (accR[o] / nn * smearGain);
        }
    }
};
} // namespace reverseivo::dsp
